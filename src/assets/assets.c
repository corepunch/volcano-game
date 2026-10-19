/* Sprite/font catalog and the native sprite sheet format.
 *
 * <sprites image="props.png" scale="0.34">            sheet defaults
 *   <sprite name="props/kitchen" rect="380,110,345,280" pivot="0.5,0.7"/>
 *   <sprite name="worker_run0" image="run.png" scale="0.5" fps="15">
 *     <frame rect="2,2,40,90" pivot="0.5,0.93" repeat="2"/>
 *   </sprite>
 * </sprites>
 *
 * Rectangles are image pixels from the top-left. Everything inside a
 * rectangle is normalized to it (0,0 top-left, 1,1 bottom-right), so it does
 * not change with the image's resolution: pivot is the ground point (frames of
 * trimmed animations may hold it slightly outside), body the clickable bounds
 * without baked shadow. World size is the rectangle times
 * scale. Image paths are relative to the sheet file. */
#include "assets_internal.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"
#include "../core/xml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LEVEL_SOURCE=100 };
typedef struct {
    Sprite *items;
    int count,capacity;
    int *slots;          /* Open-addressed name index into items; -1 empty. */
    int slot_capacity;
    Arena strings;
} SpriteTable;

static SpriteTable base_table,level_table,*target=&base_table;
static Font *fonts;
static int fonts_count;

static uint32_t name_hash(const char *name)
{
    uint32_t h=2166136261u;
    while (*name) { h^=(unsigned char)*name++;h*=16777619u; }
    return h;
}

static int table_lookup(const SpriteTable *t,const char *name)
{
    if (!t->slot_capacity) return -1;
    for (uint32_t i=name_hash(name)&(uint32_t)(t->slot_capacity-1);;i=(i+1)&(uint32_t)(t->slot_capacity-1)) {
        int item=t->slots[i];
        if (item<0) return -1;
        if (!strcmp(t->items[item].name,name)) return item;
    }
}

static bool table_index(SpriteTable *t)
{
    if (t->count*2<t->slot_capacity) return true;
    int capacity=t->slot_capacity?t->slot_capacity*2:1024;
    int *slots=malloc((size_t)capacity*sizeof *slots);
    if (!slots) return error_set("Out of memory indexing sprites");
    for (int i=0;i<capacity;i++) slots[i]=-1;
    for (int item=0;item<t->count;item++) {
        uint32_t i=name_hash(t->items[item].name)&(uint32_t)(capacity-1);
        while (slots[i]>=0) i=(i+1)&(uint32_t)(capacity-1);
        slots[i]=item;
    }
    free(t->slots);t->slots=slots;t->slot_capacity=capacity;return true;
}

static void table_free(SpriteTable *t)
{
    for (int i=0;i<t->count;i++) free(t->items[i].frames);
    free(t->items);free(t->slots);arena_free(&t->strings);
    memset(t,0,sizeof *t);
}

Sprite *catalog_new_sprite(const char *name,int source)
{
    if (strlen(name)>=sizeof ((Sprite *)0)->name) { error_set("Sprite name too long: %.64s...",name);return NULL; }
    int existing=table_lookup(target,name);
    if (existing>=0) {
        Sprite *s=&target->items[existing];
        if (s->source==source) { error_set("Duplicate sprite name '%s'",name);return NULL; }
        if (s->source>source) {
            /* A higher layer already defines NAME: parse into a discarded record. */
            static Sprite discarded;
            free(discarded.frames);memset(&discarded,0,sizeof discarded);strcpy(discarded.name,name);
            return &discarded;
        }
        free(s->frames);
        memset(s,0,sizeof *s);strcpy(s->name,name);s->source=source;return s;
    }
    if (target->count==target->capacity) {
        int capacity=target->capacity?target->capacity*2:1024;
        Sprite *grown=realloc(target->items,(size_t)capacity*sizeof *grown);
        if (!grown) { error_set("Out of memory adding sprites");return NULL; }
        target->items=grown;target->capacity=capacity;
    }
    Sprite *s=&target->items[target->count++];
    memset(s,0,sizeof *s);strcpy(s->name,name);s->source=source;
    if (!table_index(target)) return NULL;
    /* table_index only rebuilds on growth; insert the new name otherwise. */
    if (table_lookup(target,name)<0) {
        uint32_t i=name_hash(name)&(uint32_t)(target->slot_capacity-1);
        while (target->slots[i]>=0) i=(i+1)&(uint32_t)(target->slot_capacity-1);
        target->slots[i]=target->count-1;
    }
    return s;
}

bool catalog_add_frame(Sprite *sprite,SpriteFrame frame)
{
    SpriteFrame *frames=realloc(sprite->frames,(size_t)(sprite->frame_count+1)*sizeof frame);
    if (!frames) return error_set("Out of memory adding frames to %s",sprite->name);
    SpriteFrame body=sprite_body_frame(frame);
    float left=-body.dx,right=body.w-body.dx;
    if (!sprite->frame_count || left<sprite->body_left) sprite->body_left=left;
    if (!sprite->frame_count || right>sprite->body_right) sprite->body_right=right;
    sprite->frames=frames;sprite->frames[sprite->frame_count++]=frame;return true;
}

bool catalog_set_texture(Sprite *sprite,const char *path)
{
    return (sprite->texture=arena_strdup(&target->strings,path))!=NULL;
}

Font *catalog_new_font(const char *name)
{
    if (strlen(name)>=sizeof fonts->name) { error_set("Font name too long");return NULL; }
    for (int i=0;i<fonts_count;i++) if (!strcmp(fonts[i].name,name)) {
        free(fonts[i].glyphs);memset(&fonts[i],0,sizeof *fonts);strcpy(fonts[i].name,name);return &fonts[i];
    }
    Font *grown=realloc(fonts,(size_t)(fonts_count+1)*sizeof *fonts);
    if (!grown) { error_set("Out of memory adding fonts");return NULL; }
    fonts=grown;Font *f=&fonts[fonts_count++];memset(f,0,sizeof *f);strcpy(f->name,name);return f;
}

bool asset_resolve(const DeclarationScope *scope,const char *relative,char *out,size_t size)
{
    if (scope && scope->pack && path_join(out,size,scope->pack,relative) && file_exists(out)) return true;
    return vfs_resolve(relative,out,size);
}

/* Images (stb_image; no renderer needed) */

bool image_size(const char *path,int *w,int *h)
{
    int channels;
    if (!stbi_info(path,w,h,&channels)) return error_set("Cannot read image %s",path);
    return true;
}

unsigned char *image_decode(const char *path,int *w,int *h)
{
    int channels;unsigned char *pixels=stbi_load(path,w,h,&channels,4);
    if (!pixels) error_set("Cannot decode image %s",path);
    return pixels;
}

unsigned char *image_decode_gray(const char *path,int *w,int *h)
{
    int channels;unsigned char *pixels=stbi_load(path,w,h,&channels,1);
    if (!pixels) error_set("Cannot decode image %s",path);
    return pixels;
}

void image_release(void *pixels) { stbi_image_free(pixels); }

/* Native sprite sheets */

static bool sheet_frame(const XmlDoc *doc,const XmlNode *node,int image_w,int image_h,float scale,SpriteFrame *out)
{
    float rect[4],pivot[2]={.5f,1},body[4];
    if (!xml_floats(doc,node,"rect",rect,4,true)) return false;
    if (rect[0]<0 || rect[1]<0 || rect[2]<1 || rect[3]<1 || rect[0]+rect[2]>image_w || rect[1]+rect[3]>image_h)
        return xml_fail(doc,node,"rect lies outside the %dx%d image",image_w,image_h);
    if (!xml_floats(doc,node,"pivot",pivot,2,false)) return false;
    /* Trimmed animation frames may keep the ground point just outside the art. */
    if (pivot[0]<-1 || pivot[0]>2 || pivot[1]<-1 || pivot[1]>2) return xml_fail(doc,node,"pivot must lie within -1..2 (0..1 spans the rect)");
    float w=rect[2]*scale,h=rect[3]*scale;
    SpriteFrame f={.dx=w*pivot[0],.dy=h*(1-pivot[1]),.w=w,.h=h,
        .u=rect[0]/image_w,.v=1-(rect[1]+rect[3])/image_h,.cu=rect[2]/image_w,.cv=rect[3]/image_h};
    if (xml_attr(node,"body")) {
        if (!xml_floats(doc,node,"body",body,4,true)) return false;
        if (body[0]<0 || body[1]<0 || body[2]<=0 || body[3]<=0 || body[0]+body[2]>1 || body[1]+body[3]>1)
            return xml_fail(doc,node,"body must be a non-empty x,y,w,h within 0..1");
        /* Stored like the anchor: offsets from the body's bottom-left to the ground point. */
        f.body_w=body[2]*w;f.body_h=body[3]*h;
        f.body_dx=f.dx-body[0]*w;f.body_dy=f.dy-(1-body[1]-body[3])*h;
    }
    *out=f;return true;
}

static bool load_sheet(const char *path,int source)
{
    XmlDoc doc;
    if (!xml_load(&doc,path)) return false;
    static const char *sheet_attributes[]={"image","scale","fps",NULL},*sheet_children[]={"sprite",NULL};
    static const char *sprite_attributes[]={"name","image","scale","fps","rect","pivot","body",NULL};
    static const char *sprite_children[]={"frame",NULL};
    static const char *frame_attributes[]={"rect","pivot","body","repeat",NULL};
    const XmlNode *root=doc.root;bool ok=true;
    char folder[PATH_CAPACITY];path_directory(folder,sizeof folder,path);
    if (strcmp(root->name,"sprites")) ok=xml_fail(&doc,root,"expected <sprites> root");
    if (ok) ok=xml_only(&doc,root,sheet_attributes,sheet_children);
    const char *sheet_image;float sheet_scale,sheet_fps;
    if (ok) ok=xml_string(&doc,root,"image",NULL,&sheet_image) &&
        xml_float(&doc,root,"scale",1,.001f,1000,&sheet_scale) && xml_float(&doc,root,"fps",0,0,1000,&sheet_fps);
    XML_EACH(node,root,"sprite") {
        if (!ok) break;
        const char *name,*image;float scale,fps;char image_path[PATH_CAPACITY];int w,h;
        ok=xml_only(&doc,node,sprite_attributes,sprite_children) && xml_require(&doc,node,"name",&name) &&
            xml_string(&doc,node,"image",sheet_image,&image) && xml_float(&doc,node,"scale",sheet_scale,.001f,1000,&scale) &&
            xml_float(&doc,node,"fps",sheet_fps,0,1000,&fps);
        if (ok && !image) ok=xml_fail(&doc,node,"needs an image (on the sprite or the sheet)");
        if (ok && !path_join(image_path,sizeof image_path,folder,image)) ok=xml_fail(&doc,node,"image path too long");
        if (ok && !image_size(image_path,&w,&h)) ok=xml_fail(&doc,node,"cannot read image %s",image_path);
        Sprite *sprite=ok?catalog_new_sprite(name,source):NULL;
        if (ok && !sprite) ok=xml_fail(&doc,node,"%s",last_error);
        if (ok) { ok=catalog_set_texture(sprite,image_path);sprite->fps=fps; }
        if (ok && node->child_count) {
            if (xml_attr(node,"rect")) ok=xml_fail(&doc,node,"use rect or <frame> children, not both");
            XML_EACH(frame,node,"frame") {
                SpriteFrame f;int repeat;
                if (!ok) break;
                ok=xml_only(&doc,frame,frame_attributes,NULL) && xml_int(&doc,frame,"repeat",1,1,1000,&repeat) &&
                    sheet_frame(&doc,frame,w,h,scale,&f);
                for (int i=0;ok && i<repeat;i++) ok=catalog_add_frame(sprite,f);
            }
        } else if (ok) {
            SpriteFrame f;ok=sheet_frame(&doc,node,w,h,scale,&f) && catalog_add_frame(sprite,f);
        }
    }
    xml_free(&doc);return ok;
}

/* Loading */

static bool load_declarations(const char *path,bool font_file,const DeclarationScope *scope)
{
    size_t length;char *text=declaration_text(path,&length);
    if (!text) return false;
    bool ok=retail_declarations_parse(text,length,path,font_file,scope);
    free(text);return ok;
}

static bool visit_declarations(const char *path,const char *name,int mount,void *user)
{
    (void)name;(void)user;DeclarationScope scope={NULL,mount};
    return load_declarations(path,false,&scope);
}

static bool visit_sheet(const char *path,const char *name,int mount,void *user)
{
    (void)name;(void)user;return load_sheet(path,mount);
}

bool assets_load_base(void)
{
    assets_destroy();target=&base_table;
    if (!vfs_mount_count()) return error_set("No asset folders are mounted");
    if (!vfs_each("texture/lazy",".lua",visit_declarations,NULL) || !vfs_each("sprites",".sprites.xml",visit_sheet,NULL)) return false;
    static const char *font_files[]={"font/en/CambriaBold30W.lua","font/en/CambriaBold30B.lua"};
    for (size_t i=0;i<sizeof font_files/sizeof *font_files;i++) {
        char path[PATH_CAPACITY];
        if (!vfs_resolve(font_files[i],path,sizeof path)) return error_set("Missing bitmap font declarations %s",font_files[i]);
        if (!load_declarations(path,true,&(DeclarationScope){NULL,0})) return false;
    }
    return true;
}

void assets_clear_level(void) { table_free(&level_table); }

bool assets_load_level(const char *const *packs,int pack_count,const char *const *files,int file_count)
{
    assets_clear_level();target=&level_table;bool ok=true;
    for (int p=0;p<pack_count && ok;p++) {
        static const char *folders[]={"texture/lazy","sprites"},*suffixes[]={".lua",".sprites.xml"};
        for (int kind=0;kind<2 && ok;kind++) {
            char folder[PATH_CAPACITY];char **names;int count;
            if (!path_join(folder,sizeof folder,packs[p],folders[kind])) { ok=error_set("Pack path too long");break; }
            if (!(ok=dir_list(folder,suffixes[kind],&names,&count))) break;
            for (int i=0;i<count && ok;i++) {
                char path[PATH_CAPACITY];DeclarationScope scope={packs[p],LEVEL_SOURCE+p};
                ok=path_join(path,sizeof path,folder,names[i]) &&
                    (kind?load_sheet(path,LEVEL_SOURCE+p):load_declarations(path,false,&scope));
            }
            dir_list_free(names,count);
        }
    }
    for (int i=0;i<file_count && ok;i++) ok=load_sheet(files[i],LEVEL_SOURCE+pack_count+i);
    target=&base_table;
    if (!ok) assets_clear_level();
    return ok;
}

void assets_destroy(void)
{
    table_free(&base_table);table_free(&level_table);target=&base_table;
    for (int i=0;i<fonts_count;i++) free(fonts[i].glyphs);
    free(fonts);fonts=NULL;fonts_count=0;
}

const Sprite *sprite_find(const char *name)
{
    int i=table_lookup(&level_table,name);
    if (i>=0) return &level_table.items[i];
    i=table_lookup(&base_table,name);
    return i>=0?&base_table.items[i]:NULL;
}

int sprite_count(void) { return base_table.count+level_table.count; }

const Sprite *sprite_at(int index)
{
    if (index<0) return NULL;
    if (index<base_table.count) return &base_table.items[index];
    index-=base_table.count;
    return index<level_table.count?&level_table.items[index]:NULL;
}

const Font *font_find(const char *name)
{
    for (int i=0;i<fonts_count;i++) if (!strcmp(fonts[i].name,name)) return &fonts[i];
    return NULL;
}

int font_count(void) { return fonts_count; }
const Font *font_at(int index) { return index>=0 && index<fonts_count?&fonts[index]:NULL; }

bool assets_selftest(void)
{
    if (!retail_declarations_selftest()) return false;
    for (int i=0;i<fonts_count;i++) {
        const Font *f=&fonts[i];int w,h;
        if (!image_size(f->texture,&w,&h)) return error_set("Invalid bitmap font texture %s",f->texture);
        for (int j=0;j<f->count;j++) {
            const Glyph *g=&f->glyphs[j];
            if (g->x<0 || g->y<0 || g->w<0 || g->h<0 || g->x+g->w>w || g->y+g->h>h)
                return error_set("Glyph outside font atlas %s",f->name);
        }
    }
    for (int i=0;i<sprite_count();i++) {
        const Sprite *s=sprite_at(i);int w,h;
        if (!s->frame_count || !s->texture || !image_size(s->texture,&w,&h))
            return error_set("Sprite %s has no frames or an unreadable texture",s->name);
    }
    return true;
}
