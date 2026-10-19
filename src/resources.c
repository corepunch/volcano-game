#include "volcano.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A data lexer, deliberately not a Lua interpreter. Only the original atlas
 * declaration grammar is accepted: scalar assignments, records, known calls. */
enum { TOKEN_END=256,TOKEN_WORD,TOKEN_STRING,TOKEN_NUMBER };
typedef struct {
    const char *source,*at,*end,*path;
    int token,line;
    char text[PATH_CAPACITY];
    double number;
    bool failed;
} Lexer;
typedef struct { uint32_t code; int x,y,w,h,bx,by,advance; } Glyph;
typedef struct { char name[128],texture[PATH_CAPACITY]; int size; Glyph *glyphs; int count; } Font;
typedef struct Texture { char path[PATH_CAPACITY],alpha[PATH_CAPACITY]; SDL_Texture *handle; int w,h; struct Texture *next; } Texture;
static Animation *animations;
static int animations_count;
static Font fonts[16];
static int font_count;
static Texture *textures;
static Texture *log_particles[2];

static bool failure(const char *message,const char *path) {
    snprintf(last_error,sizeof(last_error),"%s: %s",message,path);return false;
}
static void *grow(void *pointer,size_t bytes) {
    void *result=realloc(pointer,bytes);
    if(!result)snprintf(last_error,sizeof(last_error),"Out of memory allocating %zu bytes",bytes);
    return result;
}
static bool join_path(char *out,size_t size,const char *a,const char *b) {
    int n=snprintf(out,size,"%s/%s",a,b);
    if(n<0 || (size_t)n>=size)return failure("Resource path too long",b);
    return true;
}
static size_t utf8_write(char *out,uint32_t c) {
    if(c<0x80){out[0]=(char)c;return 1;}
    if(c<0x800){out[0]=(char)(0xC0|(c>>6));out[1]=(char)(0x80|(c&63));return 2;}
    if(c<0x10000){out[0]=(char)(0xE0|(c>>12));out[1]=(char)(0x80|((c>>6)&63));out[2]=(char)(0x80|(c&63));return 3;}
    out[0]=(char)(0xF0|(c>>18));out[1]=(char)(0x80|((c>>12)&63));out[2]=(char)(0x80|((c>>6)&63));out[3]=(char)(0x80|(c&63));return 4;
}
static uint32_t utf8_read(const char **text) {
    const unsigned char *p=(const unsigned char*)*text; uint32_t c=*p++;
    int extra=0;
    if(c>=0xF0){c&=7;extra=3;}else if(c>=0xE0){c&=15;extra=2;}else if(c>=0xC0){c&=31;extra=1;}
    while(extra-- && *p){if((*p&0xC0)!=0x80)break;c=(c<<6)|(*p++&63);}
    *text=(const char*)p;return c;
}
char *read_resource_text(const char *path,size_t *length) {
    FILE *f=fopen(path,"rb");if(!f){failure("Cannot open data file",path);return NULL;}
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long size=ftell(f);rewind(f);
    if(size<0 || size>32*1024*1024){fclose(f);failure("Invalid text resource size",path);return NULL;}
    unsigned char *bytes=malloc((size_t)size+1);if(!bytes){fclose(f);return NULL;}
    size_t got=fread(bytes,1,(size_t)size,f);fclose(f);bytes[got]=0;
    if(got!=(size_t)size){free(bytes);failure("Truncated text resource",path);return NULL;}
    if(got>=2 && ((bytes[0]==255 && bytes[1]==254)||(bytes[0]==254 && bytes[1]==255))){
        bool little=bytes[0]==255;
        if(got%2){free(bytes);failure("Odd UTF-16 resource length",path);return NULL;}
        char *decoded=malloc(got*2+1);if(!decoded){free(bytes);return NULL;}size_t count=0;
        for(size_t i=2;i<got;i+=2){
            uint32_t c=little?(uint32_t)(bytes[i]|bytes[i+1]<<8):(uint32_t)(bytes[i]<<8|bytes[i+1]);
            if(c>=0xD800 && c<=0xDBFF){
                if(i+3>=got){free(decoded);free(bytes);failure("Truncated UTF-16 surrogate",path);return NULL;}
                uint32_t low=little?(uint32_t)(bytes[i+2]|bytes[i+3]<<8):(uint32_t)(bytes[i+2]<<8|bytes[i+3]);
                if(low<0xDC00 || low>0xDFFF){free(decoded);free(bytes);failure("Invalid UTF-16 surrogate",path);return NULL;}
                c=0x10000+((c-0xD800)<<10)+low-0xDC00;i+=2;
            }else if(c>=0xDC00 && c<=0xDFFF){free(decoded);free(bytes);failure("Invalid UTF-16 surrogate",path);return NULL;}
            count+=utf8_write(decoded+count,c);
        }
        decoded[count]=0;free(bytes);if(length)*length=count;return decoded;
    }
    if(got>=3 && bytes[0]==0xEF && bytes[1]==0xBB && bytes[2]==0xBF){memmove(bytes,bytes+3,got-3);got-=3;bytes[got]=0;}
    if(length)*length=got;return (char*)bytes;
}
static void lexer_error(Lexer *l,const char *message) {
    if(!l->failed)snprintf(last_error,sizeof(last_error),"%s:%d: %s near '%s'",l->path,l->line,message,l->text);
    l->failed=true;l->token=TOKEN_END;l->at=l->end;
}
static void next(Lexer *l) {
    if(l->failed)return;
    for(;;){
        while(l->at<l->end && isspace((unsigned char)*l->at)){if(*l->at=='\n')l->line++;l->at++;}
        if(l->end-l->at>=2 && l->at[0]=='-' && l->at[1]=='-'){
            l->at+=2;
            if(l->end-l->at>=2 && l->at[0]=='[' && l->at[1]=='['){
                l->at+=2;while(l->end-l->at>=2 && !(l->at[0]==']' && l->at[1]==']')){if(*l->at=='\n')l->line++;l->at++;}
                if(l->end-l->at<2){lexer_error(l,"Unterminated comment");return;}l->at+=2;
            }else while(l->at<l->end && *l->at!='\n')l->at++;
        }else break;
    }
    l->text[0]=0;
    if(l->at>=l->end){l->token=TOKEN_END;return;}
    unsigned char c=(unsigned char)*l->at++;
    if(isalpha(c)||c=='_'){
        const char *start=l->at-1;while(l->at<l->end && (isalnum((unsigned char)*l->at)||*l->at=='_'))l->at++;
        size_t n=(size_t)(l->at-start);if(n>=sizeof(l->text)){lexer_error(l,"Identifier too long");return;}
        memcpy(l->text,start,n);l->text[n]=0;l->token=TOKEN_WORD;return;
    }
    if(c=='\''||c=='"'){
        size_t n=0;
        while(l->at<l->end && (unsigned char)*l->at!=c){
            unsigned char v=(unsigned char)*l->at++;
            if(v=='\\'){
                if(l->at==l->end){lexer_error(l,"Unterminated escape");return;}v=(unsigned char)*l->at++;
                if(v=='n')v='\n';else if(v=='r')v='\r';else if(v=='t')v='\t';
                else if(isdigit(v)){int number=v-'0';for(int i=0;i<2 && l->at<l->end && isdigit((unsigned char)*l->at);i++)number=number*10+(*l->at++-'0');v=(unsigned char)number;}
            }
            if(n+1>=sizeof(l->text)){lexer_error(l,"String too long");return;}l->text[n++]=(char)v;
        }
        if(l->at>=l->end){lexer_error(l,"Unterminated string");return;}l->at++;l->text[n]=0;l->token=TOKEN_STRING;return;
    }
    if(isdigit(c)||c=='.'||(c=='-' && l->at<l->end && (isdigit((unsigned char)*l->at)||*l->at=='.'))){
        char *after;errno=0;l->number=strtod(l->at-1,&after);
        if(after==l->at-1 || errno || !isfinite(l->number) || fabs(l->number)>FLT_MAX){lexer_error(l,"Invalid numeric data");return;}
        l->at=after;l->token=TOKEN_NUMBER;return;
    }
    l->token=c;l->text[0]=(char)c;l->text[1]=0;
}
static bool take(Lexer *l,int token) {
    if(l->token!=token){lexer_error(l,"Unexpected data syntax");return false;}next(l);return !l->failed;
}
static bool word(Lexer *l,const char *expected) {
    if(l->token!=TOKEN_WORD || strcmp(l->text,expected)){lexer_error(l,"Unsupported executable syntax");return false;}next(l);return !l->failed;
}
static bool string_value(Lexer *l,char *out,size_t size) {
    if(l->token!=TOKEN_STRING || strlen(l->text)>=size){lexer_error(l,"Expected bounded string");return false;}
    strcpy(out,l->text);next(l);return !l->failed;
}
static bool number_value(Lexer *l,float *value) {
    if(l->token!=TOKEN_NUMBER){lexer_error(l,"Expected numeric constant");return false;}
    *value=(float)l->number;next(l);return !l->failed;
}
/* Retail Calibri metrics contain offsets such as 256 + 24 and 512-24.
   Accept only bounded sums/differences of literals, never script evaluation. */
static bool metric_value(Lexer *l,float *value) {
    if(!number_value(l,value))return false;
    while(l->token=='+' || l->token=='-' || (l->token==TOKEN_NUMBER && l->number<0)) {
        int sign=1;
        if(l->token=='+' || l->token=='-'){sign=l->token=='-'?-1:1;next(l);}
        float term;if(!number_value(l,&term))return false;
        double result=(double)*value+sign*(double)term;
        if(!isfinite(result)||result<INT_MIN||result>INT_MAX){lexer_error(l,"Glyph metric exceeds integer range");return false;}
        *value=(float)result;
    }
    return true;
}
static bool skip_data(Lexer *l,int depth) {
    if(depth>32){lexer_error(l,"Data nesting exceeds limit");return false;}
    if(l->token=='{'){
        next(l);
        while(l->token!='}' && !l->failed){
            if(l->token==TOKEN_END){lexer_error(l,"Truncated data table");return false;}
            if(l->token==','||l->token==';'||l->token=='='||l->token=='['||l->token==']'){next(l);continue;}
            if(!skip_data(l,depth+1))return false;
        }
        return take(l,'}');
    }
    if(l->token==TOKEN_NUMBER||l->token==TOKEN_STRING||l->token==TOKEN_WORD){next(l);return !l->failed;}
    lexer_error(l,"Expected constant data");return false;
}
static Animation *new_animation(const char *name) {
    for(int i=0;i<animations_count;i++)if(!strcmp(animations[i].name,name)){failure("Duplicate sprite name",name);return NULL;}
    Animation *grown=grow(animations,(size_t)(animations_count+1)*sizeof(*animations));if(!grown)return NULL;
    animations=grown;Animation *a=&animations[animations_count++];memset(a,0,sizeof(*a));snprintf(a->name,sizeof(a->name),"%s",name);return a;
}
static bool frame_table(Lexer *l,Animation *a) {
    if(!a){lexer_error(l,"Frames declared without an animation");return false;}
    if(!take(l,'{'))return false;
    while(l->token!='}' && !l->failed){
        if(l->token==','||l->token==';'){next(l);continue;}
        if(!take(l,'{'))return false;
        SpriteFrame f={0};unsigned seen=0;
        while(l->token!='}' && !l->failed){
            if(l->token!=TOKEN_WORD){lexer_error(l,"Expected frame field");return false;}
            const char *names[]={"dx","dy","u","v","cu","cv","w","h"};float *values[]={&f.dx,&f.dy,&f.u,&f.v,&f.cu,&f.cv,&f.w,&f.h};
            int index=-1;for(int i=0;i<8;i++)if(!strcmp(l->text,names[i]))index=i;
            if(index<0){lexer_error(l,"Unknown frame field");return false;}next(l);
            if(!take(l,'=') || !number_value(l,values[index]))return false;seen|=1u<<index;
            if(l->token==','||l->token==';')next(l);
        }
        if(!take(l,'}'))return false;
        if(seen!=255||f.w<=0||f.h<=0||f.cu<=0||f.cv<=0||f.u<0||f.v<0||f.cu>1||f.cv>1||f.u+f.cu>1.00001f||f.v+f.cv>1.00001f){lexer_error(l,"Incomplete or invalid sprite frame");return false;}
        SpriteFrame *frames=grow(a->frames,(size_t)(a->frame_count+1)*sizeof(f));if(!frames)return false;a->frames=frames;a->frames[a->frame_count++]=f;
    }
    return take(l,'}');
}
static bool glyph_table(Lexer *l,Font *font) {
    if(!font){lexer_error(l,"Glyphs declared without font");return false;}
    if(!take(l,'{'))return false;
    while(l->token!='}'&&!l->failed){
        if(l->token==','||l->token==';'){next(l);continue;}if(!take(l,'{'))return false;
        Glyph g={0};bool has_code=false;
        while(l->token!='}'&&!l->failed){
            char field[32];if(l->token!=TOKEN_WORD||strlen(l->text)>=sizeof(field)){lexer_error(l,"Invalid font field");return false;}strcpy(field,l->text);next(l);if(!take(l,'='))return false;
            if(!strcmp(field,"ch")){char ch[32];if(!string_value(l,ch,sizeof(ch)))return false;const char *p=ch;g.code=utf8_read(&p);has_code=true;}
            else {float value;if(!metric_value(l,&value))return false;
                if((double)value<INT_MIN || (double)value>INT_MAX){lexer_error(l,"Glyph metric exceeds integer range");return false;}
                if(!strcmp(field,"x"))g.x=(int)value;else if(!strcmp(field,"y"))g.y=(int)value;else if(!strcmp(field,"w"))g.w=(int)value;else if(!strcmp(field,"h"))g.h=(int)value;
                else if(!strcmp(field,"bX"))g.bx=(int)value;else if(!strcmp(field,"bY"))g.by=(int)value;else if(!strcmp(field,"aX"))g.advance=(int)value;else if(strcmp(field,"aY")){lexer_error(l,"Unknown glyph metric");return false;}}
            if(l->token==','||l->token==';')next(l);
        }
        if(!take(l,'}')||!has_code)return false;
        Glyph *glyphs=grow(font->glyphs,(size_t)(font->count+1)*sizeof(g));if(!glyphs)return false;font->glyphs=glyphs;font->glyphs[font->count++]=g;
    }
    return take(l,'}');
}
static bool parse_source(const char *source,size_t size,const char *path,bool font_file) {
    Lexer l={.source=source,.at=source,.end=source+size,.path=path,.line=1};next(&l);
    Animation *current=NULL;Font *font=NULL;bool ok=true;
    while(l.token!=TOKEN_END&&!l.failed&&ok){
        if(l.token==';'){next(&l);continue;}
        if(l.token!=TOKEN_WORD){lexer_error(&l,"Expected data declaration");break;}
        char symbol[128];if(strlen(l.text)>=sizeof(symbol)){lexer_error(&l,"Name too long");break;}strcpy(symbol,l.text);next(&l);
        if(l.token=='='){
            next(&l);
            if(l.token==TOKEN_WORD&&!strcmp(l.text,"nil")){next(&l);if(!strcmp(symbol,"texinfo"))current=NULL;else if(!strcmp(symbol,"font"))font=NULL;continue;}
            if(!strcmp(symbol,"texinfo")&&!font_file){
                char name[128];ok=word(&l,"createTextureInfoEx")&&take(&l,'(')&&string_value(&l,name,sizeof(name))&&take(&l,')');
                if(ok){current=new_animation(name);ok=current!=NULL;}
            }else if(!strcmp(symbol,"frames")&&!font_file)ok=frame_table(&l,current);
            else if(!strcmp(symbol,"font")&&font_file){
                char name[128];ok=word(&l,"createFont")&&take(&l,'(')&&string_value(&l,name,sizeof(name))&&take(&l,')');
                if(ok){if(font_count>=16){lexer_error(&l,"Too many fonts");break;}font=&fonts[font_count++];memset(font,0,sizeof(*font));strcpy(font->name,name);}
            }else if(!strcmp(symbol,"chars")&&font_file)ok=glyph_table(&l,font);
            else if(!strcmp(symbol,"bounds")||!strcmp(symbol,"points"))ok=skip_data(&l,0);
            else {lexer_error(&l,"Unsupported assignment; scripts are never executed");break;}
        }else if(!strcmp(symbol,"addTextureInfoExNodes")&&!font_file){
            char texture[512];float fps;
            ok=current&&take(&l,'(')&&word(&l,"texinfo")&&take(&l,',')&&string_value(&l,texture,sizeof(texture))&&take(&l,',')&&word(&l,"frames")&&take(&l,',')&&number_value(&l,&fps)&&take(&l,')');
            if(ok && (fps<0 || fps>1000)){lexer_error(&l,"Invalid animation frame rate");ok=false;}
            if(ok){char rel[640];snprintf(rel,sizeof(rel),"texture/lazy/%s.png",texture);ok=join_path(current->texture,sizeof(current->texture),asset_root,rel);current->fps=fps;}
        }else if((!strcmp(symbol,"addBoundInfoExNodes")||!strcmp(symbol,"addPointsInfoExNodes"))&&!font_file){
            ok=take(&l,'(')&&word(&l,"texinfo")&&take(&l,',')&&word(&l,!strcmp(symbol,"addBoundInfoExNodes")?"bounds":"points")&&take(&l,')');
        }else if(!strcmp(symbol,"initFont")&&font_file){
            char family[128],style[128];float value;
            ok=font&&take(&l,'(')&&word(&l,"font")&&take(&l,',')&&string_value(&l,family,sizeof(family))&&take(&l,',')&&string_value(&l,style,sizeof(style))&&take(&l,',')&&number_value(&l,&value)&&take(&l,')');if(ok)font->size=(int)value;
        }else if(!strcmp(symbol,"setFontTexture")&&font_file){
            char texture[512];float width,height;
            ok=font&&take(&l,'(')&&word(&l,"font")&&take(&l,',')&&string_value(&l,texture,sizeof(texture))&&take(&l,',')&&number_value(&l,&width)&&take(&l,',')&&number_value(&l,&height)&&take(&l,')');
            if(ok){char rel[640];snprintf(rel,sizeof(rel),"font/en/%s.png",texture);ok=join_path(font->texture,sizeof(font->texture),asset_root,rel);}
        }else if(!strcmp(symbol,"setFontTransform")&&font_file){
            float x,y;ok=font&&take(&l,'(')&&word(&l,"font")&&take(&l,',')&&number_value(&l,&x)&&take(&l,',')&&number_value(&l,&y)&&take(&l,')');
        }else if(!strcmp(symbol,"fillFont")&&font_file)ok=font&&take(&l,'(')&&word(&l,"font")&&take(&l,',')&&word(&l,"chars")&&take(&l,')');
        else {lexer_error(&l,"Unsupported function; scripts are never executed");break;}
    }
    if(!ok&&!l.failed && !last_error[0])lexer_error(&l,"Incomplete resource declaration");
    return ok&&!l.failed;
}
static bool parse_declarations(const char *path,bool font_file) {
    size_t size;char *source=read_resource_text(path,&size);if(!source)return false;
    bool result=parse_source(source,size,path,font_file);free(source);return result;
}
static int compare_names(const void *a,const void *b){return strcmp(*(char*const*)a,*(char*const*)b);}
bool resources_init(void) {
    char dir_path[PATH_CAPACITY];if(!join_path(dir_path,sizeof(dir_path),asset_root,"texture/lazy"))return false;
    DIR *directory=opendir(dir_path);if(!directory)return failure("Missing original sprite folder",dir_path);
    char **names=NULL;int count=0;struct dirent *entry;
    while((entry=readdir(directory))){size_t n=strlen(entry->d_name);if(n<4||strcmp(entry->d_name+n-4,".lua"))continue;
        char **grown=grow(names,(size_t)(count+1)*sizeof(char*));if(!grown){closedir(directory);return false;}names=grown;names[count++]=strdup(entry->d_name);}
    closedir(directory);qsort(names,(size_t)count,sizeof(char*),compare_names);bool ok=true;
    for(int i=0;i<count;i++){char path[PATH_CAPACITY];if(ok)ok=join_path(path,sizeof(path),dir_path,names[i])&&parse_declarations(path,false);free(names[i]);}free(names);
    if(!ok)return false;
    const char *font_files[]={"font/en/CambriaBold30W.lua","font/en/CambriaBold30B.lua"};
    for(size_t i=0;i<sizeof font_files/sizeof *font_files;i++) {
        char font_path[PATH_CAPACITY];
        if(!join_path(font_path,sizeof font_path,asset_root,font_files[i])||!parse_declarations(font_path,true))return false;
    }
    printf("Parsed %d original animations and %d bitmap fonts as data (no scripting runtime).\n",animations_count,font_count);
    return animation_find("worker_run0") && animation_find("water0");
}
Animation *animation_find(const char *name){for(int i=0;i<animations_count;i++)if(!strcmp(animations[i].name,name))return &animations[i];return NULL;}
int animation_count(void){return animations_count;}
static Texture *texture_load(const char *path,const char *alpha) {
    if(!alpha)alpha="";
    for(Texture *t=textures;t;t=t->next)if(!strcmp(path,t->path)&&!strcmp(alpha,t->alpha))return t;
    int w,h,n;unsigned char *pixels=stbi_load(path,&w,&h,&n,4);if(!pixels){failure("Cannot decode original image",path);return NULL;}
    if(*alpha){int aw,ah;unsigned char *mask=stbi_load(alpha,&aw,&ah,&n,1);
        if(!mask||aw!=w||ah!=h){stbi_image_free(mask);stbi_image_free(pixels);failure("Invalid original JPEG alpha",alpha);return NULL;}
        for(size_t i=0;i<(size_t)w*h;i++)pixels[i*4+3]=mask[i];stbi_image_free(mask);}
    SDL_Texture *handle=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,w,h);
    if(!handle){stbi_image_free(pixels);failure("Cannot create texture",SDL_GetError());return NULL;}
    int result=SDL_UpdateTexture(handle,NULL,pixels,w*4);stbi_image_free(pixels);
    if(result){SDL_DestroyTexture(handle);failure("Cannot upload image",SDL_GetError());return NULL;}
    Texture *t=calloc(1,sizeof(*t));if(!t){SDL_DestroyTexture(handle);return NULL;}
    snprintf(t->path,sizeof(t->path),"%s",path);snprintf(t->alpha,sizeof(t->alpha),"%s",alpha);t->handle=handle;t->w=w;t->h=h;t->next=textures;textures=t;SDL_SetTextureBlendMode(handle,SDL_BLENDMODE_BLEND);return t;
}
bool draw_sprite(const char *name,float x,float y,double time,int frame,float alpha) {
    return draw_sprite_rotated(name,x,y,time,frame,alpha,0);
}
static bool sprite_color(const char *name,float x,float y,double time,int frame,float alpha,float angle,SDL_Color tint) {
    Animation *a=animation_find(name);if(!a || !a->frame_count)return false;
    int index=frame>=0?frame:(int)(time*a->fps)%a->frame_count;if(index>=a->frame_count)index=a->frame_count-1;
    SpriteFrame *f=&a->frames[index];Texture *t=texture_load(a->texture,NULL);if(!t)return false;
    SDL_Rect src={(int)lroundf(f->u*t->w),(int)lroundf((1-f->v-f->cv)*t->h),(int)lroundf(f->cu*t->w),(int)lroundf(f->cv*t->h)};
    SDL_FRect dest={x-f->dx,GAME_HEIGHT-y-f->h+f->dy,f->w,f->h};SDL_SetTextureAlphaMod(t->handle,(Uint8)(fminf(1,fmaxf(0,alpha))*255));
    /* Rotate about the authored anchor, so attached roof occupants share the
     * house transform even when their trimmed animation frames change size. */
    SDL_FPoint pivot={f->dx,f->h-f->dy};
    SDL_SetTextureColorMod(t->handle,tint.r,tint.g,tint.b);
    int result=angle?SDL_RenderCopyExF(renderer,t->handle,&src,&dest,angle,&pivot,SDL_FLIP_NONE):SDL_RenderCopyF(renderer,t->handle,&src,&dest);
    SDL_SetTextureColorMod(t->handle,255,255,255);
    if(result){failure("Sprite render failed",SDL_GetError());return false;}return true;
}
bool draw_sprite_rotated(const char *name,float x,float y,double time,int frame,float alpha,float angle) {
    return sprite_color(name,x,y,time,frame,alpha,angle,(SDL_Color){255,255,255,255});
}
bool draw_sprite_tinted(const char *name,float x,float y,double time,int frame,SDL_Color tint) {
    return sprite_color(name,x,y,time,frame,tint.a/255.f,0,tint);
}
bool draw_image(const char *relative,float x,float y,float w,float h,const char *alpha) {
    char path[PATH_CAPACITY],mask[PATH_CAPACITY]="";if(!join_path(path,sizeof(path),asset_root,relative))return false;
    if(alpha&&*alpha&&!join_path(mask,sizeof(mask),asset_root,alpha))return false;
    Texture *t=texture_load(path,mask);if(!t)return false;
    SDL_FRect dest={x,GAME_HEIGHT-y-h,w,h};SDL_SetTextureAlphaMod(t->handle,255);return SDL_RenderCopyF(renderer,t->handle,NULL,&dest)==0;
}
bool draw_image_file(const char *path,float x,float y,float w,float h) {
    Texture *t=texture_load(path,NULL);if(!t)return false;
    SDL_FRect dest={x,GAME_HEIGHT-y-h,w,h};SDL_SetTextureAlphaMod(t->handle,255);return SDL_RenderCopyF(renderer,t->handle,NULL,&dest)==0;
}
bool image_file_size(const char *path,int *w,int *h) {
    Texture *t=texture_load(path,NULL);if(!t)return false;*w=t->w;*h=t->h;return true;
}
bool draw_map_sprite(const char *path,SDL_Rect region,Vec2 pivot,Vec2 position,Vec2 scale,float bend,float angle,float opacity) {
    Texture *t=texture_load(path,NULL);if(!t)return false;
    if(region.x<0 || region.y<0 || region.w<1 || region.h<1 || region.x+region.w>t->w || region.y+region.h>t->h)
        return failure("Map sprite region outside image",path);
    enum { STRIPS=10 };SDL_Vertex vertices[(STRIPS+1)*2];int indices[STRIPS*6];
    float radians=angle*.01745329252f,c=cosf(radians),s=sinf(radians);
    for(int row=0;row<=STRIPS;row++) {
        float fraction=(float)row/STRIPS;
        float local_y=(pivot.y-fraction)*region.h*scale.y;
        /* Keep the lower trunk and roots still; bend the upper canopy. */
        float weight=fmaxf(0,(pivot.y-fraction-.2f)/.8f);weight*=weight;
        for(int side=0;side<2;side++) {
            float local_x=(side-pivot.x)*region.w*scale.x+bend*weight;
            vertices[row*2+side]=(SDL_Vertex){
                .position={position.x+c*local_x-s*local_y,GAME_HEIGHT-position.y-s*local_x-c*local_y},
                .color={255,255,255,(Uint8)(fminf(1,fmaxf(0,opacity))*255)},
                .tex_coord={(region.x+side*region.w)/(float)t->w,(region.y+fraction*region.h)/(float)t->h}};
        }
        if(row<STRIPS) {
            int a=row*2,b=a+2,k=row*6;
            indices[k]=a;indices[k+1]=b;indices[k+2]=a+1;
            indices[k+3]=a+1;indices[k+4]=b;indices[k+5]=b+1;
        }
    }
    SDL_SetTextureColorMod(t->handle,255,255,255);SDL_SetTextureAlphaMod(t->handle,255);
    return SDL_RenderGeometry(renderer,t->handle,vertices,(STRIPS+1)*2,indices,STRIPS*6)==0?
        true:failure("Map sprite render failed",SDL_GetError());
}
bool draw_map_animation(const char *name,Vec2 position,double time,float scale,float angle,float opacity) {
    Animation *a=animation_find(name);if(!a || !a->frame_count)return false;
    SpriteFrame *f=&a->frames[(int)(time*a->fps)%a->frame_count];
    Texture *t=texture_load(a->texture,NULL);if(!t)return false;
    SDL_Rect region={(int)lroundf(f->u*t->w),(int)lroundf((1-f->v-f->cv)*t->h),(int)lroundf(f->cu*t->w),(int)lroundf(f->cv*t->h)};
    Vec2 pivot={f->dx/f->w,(f->h-f->dy)/f->h};
    Vec2 ratio={scale*f->w/region.w,scale*f->h/region.h};
    return draw_map_sprite(a->texture,region,pivot,position,ratio,0,angle,opacity);
}
void draw_glint(float x,float y,float radius,float opacity) {
    SDL_Vertex vertices[9];int indices[24];
    vertices[0]=(SDL_Vertex){.position={x,GAME_HEIGHT-y},.color={255,255,255,(Uint8)(fminf(1,fmaxf(0,opacity))*255)}};
    for(int i=0;i<8;i++) {
        float angle=i*.7853981634f,r=i%2?radius*.17f:radius;
        vertices[i+1]=(SDL_Vertex){.position={x+cosf(angle)*r,GAME_HEIGHT-y+sinf(angle)*r},.color={255,255,255,0}};
        indices[i*3]=0;indices[i*3+1]=i+1;indices[i*3+2]=(i+1)%8+1;
    }
    if(SDL_RenderGeometry(renderer,NULL,vertices,9,indices,24))failure("Glint render failed",SDL_GetError());
}
bool draw_atlas(const char *relative,SDL_FRect rect,SDL_FRect uv) {
    char path[PATH_CAPACITY];if(!join_path(path,sizeof path,asset_root,relative))return false;
    Texture *t=texture_load(path,NULL);if(!t)return false;
    SDL_Rect src={(int)lroundf(uv.x*t->w),(int)lroundf((1-uv.y-uv.h)*t->h),
        (int)lroundf(uv.w*t->w),(int)lroundf(uv.h*t->h)};
    if(src.w<=0||src.h<=0||rect.w<=0||rect.h<=0)return true;
    if(src.x<0||src.y<0||src.x+src.w>t->w||src.y+src.h>t->h)return failure("GUI region outside original atlas",relative);
    rect.y=GAME_HEIGHT-rect.y-rect.h;
    SDL_SetTextureAlphaMod(t->handle,255);SDL_SetTextureColorMod(t->handle,255,255,255);
    return SDL_RenderCopyF(renderer,t->handle,&src,&rect)==0;
}
bool draw_particle(bool smoke,int frame,float x,float y,float diameter,float angle,SDL_Color tint) {
    return draw_particle_stretched(smoke,frame,x,y,diameter,diameter,angle,tint);
}
bool draw_particle_stretched(bool smoke,int frame,float x,float y,float w,float h,float angle,SDL_Color tint) {
    char path[PATH_CAPACITY];
    if(!join_path(path,sizeof path,asset_root,smoke?"texture/static/part_smoke.png":"texture/static/part_fire.png"))return false;
    Texture *t=texture_load(path,NULL);if(!t)return false;
    frame=frame<0?0:frame>31?31:frame;
    SDL_Rect src=smoke?(SDL_Rect){0,0,t->w,t->h}:
        (SDL_Rect){(frame%8)*(t->w/8),(frame/8)*(t->h/4),t->w/8,t->h/4};
    SDL_FRect dest={x-w*.5f,GAME_HEIGHT-y-h*.5f,w,h};
    SDL_SetTextureColorMod(t->handle,tint.r,tint.g,tint.b);SDL_SetTextureAlphaMod(t->handle,tint.a);
    int result=SDL_RenderCopyExF(renderer,t->handle,&src,&dest,angle,NULL,SDL_FLIP_NONE);
    SDL_SetTextureColorMod(t->handle,255,255,255);SDL_SetTextureAlphaMod(t->handle,255);
    return result?failure("Particle render failed",SDL_GetError()):true;
}
bool draw_particle_glow(float x,float y,float w,float h,SDL_Color tint) {
    enum { SEGMENTS=24, RINGS=4 };
    const float radius[RINGS]={0,.25f,.6f,1},alpha[RINGS]={1,.6f,.16f,0};
    SDL_Vertex vertices[(SEGMENTS+1)*RINGS];int indices[SEGMENTS*(RINGS-1)*6],count=0;
    for(int i=0;i<=SEGMENTS;i++) {
        float angle=i*6.28318530718f/SEGMENTS;
        for(int ring=0;ring<RINGS;ring++) {
            SDL_Color color=tint;color.a=(Uint8)(tint.a*alpha[ring]);
            vertices[i*RINGS+ring]=(SDL_Vertex){
                .position={x+cosf(angle)*w*.5f*radius[ring],GAME_HEIGHT-y+sinf(angle)*h*.5f*radius[ring]},
                .color=color,.tex_coord={0,0}};
        }
    }
    for(int i=0;i<SEGMENTS;i++) for(int ring=0;ring<RINGS-1;ring++) {
        int a=i*RINGS+ring,b=(i+1)*RINGS+ring;
        indices[count++]=a;indices[count++]=b;indices[count++]=b+1;
        indices[count++]=a;indices[count++]=b+1;indices[count++]=a+1;
    }
    SDL_BlendMode previous;SDL_GetRenderDrawBlendMode(renderer,&previous);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_ADD);
    int result=SDL_RenderGeometry(renderer,NULL,vertices,(SEGMENTS+1)*RINGS,indices,count);
    SDL_SetRenderDrawBlendMode(renderer,previous);
    return result?failure("Particle glow render failed",SDL_GetError()):true;
}
static Texture *log_particle_texture(bool smoke) {
    if(log_particles[smoke])return log_particles[smoke];
    const char *name=smoke?"log-smoke.png":"log-fire.png";
    char path[PATH_CAPACITY];
    char *base=SDL_GetBasePath();
    const char *roots[]={asset_root,"assets/particles",base?base:"",base?base:""};
    const char *folders[]={"/texture/particles/","/","../assets/particles/","../Resources/texture/particles/"};
    for(int i=0;i<4;i++) {
        if(i>=2 && !base)continue;
        int n=snprintf(path,sizeof path,"%s%s%s",roots[i],folders[i],name);
        if(n<0 || (size_t)n>=sizeof path)continue;
        FILE *file=fopen(path,"rb");if(!file)continue;fclose(file);
        SDL_free(base);
        Texture *t=texture_load(path,NULL);if(!t)return NULL;
        if((smoke && (t->w!=528 || t->h!=132)) || (!smoke && (t->w!=680 || t->h!=1270))) {
            failure("Invalid log particle atlas dimensions",path);return NULL;
        }
        return log_particles[smoke]=t;
    }
    SDL_free(base);failure("Missing log particle atlas",name);return NULL;
}
bool draw_log_particle(bool smoke,int frame,float x,float y,float w,float h,float bend,SDL_Color tint) {
    Texture *t=log_particle_texture(smoke);if(!t)return false;
    frame=frame<0?0:frame%(smoke?4:80);
    SDL_Rect src=smoke?(SDL_Rect){frame*132+2,2,128,128}:
        (SDL_Rect){(frame%8)*85+2,(frame/8)*127+2,81,123};
    enum { STRIPS=10 };
    SDL_Vertex vertices[(STRIPS+1)*2];int indices[STRIPS*6];
    for(int row=0;row<=STRIPS;row++) {
        float f=(float)row/STRIPS,lift=1-f;
        for(int side=0;side<2;side++) vertices[row*2+side]=(SDL_Vertex){
            .position={x+(side-.5f)*w+bend*lift*lift,GAME_HEIGHT-y+(f-.5f)*h},
            .color=tint,.tex_coord={(src.x+side*src.w)/(float)t->w,(src.y+f*src.h)/(float)t->h}};
        if(row<STRIPS) {
            int a=row*2,b=a+2,k=row*6;
            indices[k]=a;indices[k+1]=b;indices[k+2]=a+1;
            indices[k+3]=a+1;indices[k+4]=b;indices[k+5]=b+1;
        }
    }
    SDL_SetTextureColorMod(t->handle,255,255,255);SDL_SetTextureAlphaMod(t->handle,255);
    return SDL_RenderGeometry(renderer,t->handle,vertices,(STRIPS+1)*2,indices,STRIPS*6)==0?
        true:failure("Log particle render failed",SDL_GetError());
}
bool draw_foam_particle(float x,float y,float w,float h,float angle,Uint8 opacity,bool ring) {
    /* White vertices with transparent edges have no smoke texture or gray
     * pigment. Small irregular flecks and open rings read as surface foam. */
    enum { SEGMENTS=20, BANDS=3 };
    SDL_Vertex vertices[(SEGMENTS+1)*BANDS];int indices[SEGMENTS*12],count=0;
    float radians=angle*.01745329252f,c=cosf(radians),s=sinf(radians);
    const float radii[2][BANDS]={{0,.48f,1},{.63f,.81f,1}};
    for(int i=0;i<=SEGMENTS;i++) {
        float theta=i*6.28318530718f/SEGMENTS;
        float rough=1+.10f*sinf(theta*3+radians)+.06f*cosf(theta*5-radians);
        for(int band=0;band<BANDS;band++) {
            float radius=radii[ring?1:0][band]*rough;
            float dx=cosf(theta)*w*.5f*radius,dy=sinf(theta)*h*.5f*radius;
            Uint8 alpha=band==2 || (ring && band==0)?0:opacity;
            vertices[i*BANDS+band]=(SDL_Vertex){
                .position={x+c*dx-s*dy,GAME_HEIGHT-y-s*dx-c*dy},
                .color={255,255,255,alpha},.tex_coord={0,0}};
        }
    }
    for(int i=0;i<SEGMENTS;i++) {
        if(ring && (i%10==0 || i%10>=8))continue;
        for(int band=0;band<BANDS-1;band++) {
            int a=i*BANDS+band,b=(i+1)*BANDS+band;
            indices[count++]=a;indices[count++]=b;indices[count++]=b+1;
            indices[count++]=a;indices[count++]=b+1;indices[count++]=a+1;
        }
    }
    return SDL_RenderGeometry(renderer,NULL,vertices,(SEGMENTS+1)*BANDS,indices,count)==0?
        true:failure("Foam particle render failed",SDL_GetError());
}
static Font *font_find(const char *name) {
    for(int i=0;i<font_count;i++)if(!strcmp(fonts[i].name,name))return &fonts[i];return NULL;
}
float text_width(const char *name,const char *text,int size) {
    Font *f=font_find(name);if(!f||f->size<=0)return 0;
    float width=0;
    while(*text){uint32_t code=utf8_read(&text);for(int i=0;i<f->count;i++)if(f->glyphs[i].code==code){width+=f->glyphs[i].advance*(float)size/f->size;break;}}
    return width;
}
void draw_font(const char *name,const char *text,float x,float y,int size,SDL_Color color) {
    Font *f=font_find(name);if(!f||f->size<=0)return;
    Texture *t=texture_load(f->texture,NULL);if(!t)return;float scale=(float)size/f->size,left=x,baseline=y+size;
    SDL_SetTextureColorMod(t->handle,color.r,color.g,color.b);SDL_SetTextureAlphaMod(t->handle,color.a);
    while(*text){uint32_t code=utf8_read(&text);if(code=='\n'){x=left;baseline+=size;continue;}Glyph *g=NULL;
        for(int i=0;i<f->count;i++)if(f->glyphs[i].code==code){g=&f->glyphs[i];break;}
        if(!g)for(int i=0;i<f->count;i++)if(f->glyphs[i].code=='?'){g=&f->glyphs[i];break;}
        if(!g)continue;
        if(g->w&&g->h){SDL_Rect src={g->x,g->y,g->w,g->h};SDL_FRect dst={x+g->bx*scale,baseline-g->by*scale,g->w*scale,g->h*scale};SDL_RenderCopyF(renderer,t->handle,&src,&dst);}x+=g->advance*scale;
    }
    SDL_SetTextureColorMod(t->handle,255,255,255);SDL_SetTextureAlphaMod(t->handle,255);
}
void draw_text(const char *text,float x,float y,int size,SDL_Color color) {
    draw_font("CambriaBold30W",text,x,y,size,color);
}
void draw_rect(float x,float y,float w,float h,SDL_Color c){SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);SDL_FRect r={x,y,w,h};SDL_RenderFillRectF(renderer,&r);}
void draw_line(float x,float y,float xx,float yy,SDL_Color c){SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);SDL_RenderDrawLineF(renderer,x,y,xx,yy);}
bool renderer_screenshot(const char *path) {
    int w,h;SDL_GetRendererOutputSize(renderer,&w,&h);SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);if(!surface)return false;
    bool ok=SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_RGBA32,surface->pixels,surface->pitch)==0&&SDL_SaveBMP(surface,path)==0;SDL_FreeSurface(surface);return ok;
}
bool resources_selftest(void) {
    int frames=0;
    const char *required_fonts[]={"CambriaBold30W","CambriaBold30B","CalibriRegular22N","CalibriBold20W","CalibriBold20G"};
    for(size_t i=0;i<sizeof required_fonts/sizeof *required_fonts;i++)
        if(!font_find(required_fonts[i]))return failure("Missing retail HUD font",required_fonts[i]);
    for(int i=0;i<font_count;i++) {
        Font *f=&fonts[i];int w,h,n;
        if(!stbi_info(f->texture,&w,&h,&n))return failure("Invalid bitmap font texture",f->texture);
        for(int j=0;j<f->count;j++) {
            Glyph *g=&f->glyphs[j];
            if(g->x<0||g->y<0||g->w<0||g->h<0||g->x+g->w>w||g->y+g->h>h)
                return failure("Glyph outside original font atlas",f->name);
        }
    }
    for(int i=0;i<animations_count;i++){
        Animation *a=&animations[i];int w,h,n;
        if(!a->frame_count||!stbi_info(a->texture,&w,&h,&n))return failure("Invalid original animation texture",a->texture);
        for(int j=0;j<a->frame_count;j++){SpriteFrame *f=&a->frames[j];int x=(int)lroundf(f->u*w),y=(int)lroundf((1-f->v-f->cv)*h),fw=(int)lroundf(f->cu*w),fh=(int)lroundf(f->cv*h);
            if(x<0||y<0||fw<=0||fh<=0||x+fw>w||y+fh>h)return failure("Frame outside original atlas",a->name);frames++;}
    }
    /* Exercise the same complete parser used for originals, without creating
       files or invoking any scripting runtime. Rejected inputs do not allocate
       animations, so the live resource registry remains intact. */
    const char *rejected[]={
        "os.execute('must not run')",
        "require('anything')",
        "texinfo = dofile('anything')",
        "bounds = { nested = function() end }",
        "bounds = { 1e100 }",
        "bounds = { 1e999 }",
        "bounds = { 0, 1 ",
        "--[[",
        "--[[missing close",
        "texinfo = createTextureInfoEx('unterminated)",
    };
    char saved_error[sizeof last_error];memcpy(saved_error,last_error,sizeof last_error);
    for(size_t i=0;i<sizeof rejected/sizeof rejected[0];i++) {
        last_error[0]=0;
        if(parse_source(rejected[i],strlen(rejected[i]),"parser rejection self-test",false))
            return failure("Parser accepted unsupported or malformed resource data",rejected[i]);
    }
    const char *valid="--[[constant bounds]]\nbounds = { { x=1.25, y=-2 }, {3,4} }";
    if(!parse_source(valid,strlen(valid),"parser acceptance self-test",false))return false;
    const char *invalid_frame="{{dx=0,dy=0,u=1e20,v=0,cu=.5,cv=.5,w=32,h=32}}";
    Lexer frame_lexer={.source=invalid_frame,.at=invalid_frame,.end=invalid_frame+strlen(invalid_frame),.path="invalid UV self-test",.line=1};
    Animation temporary={0};next(&frame_lexer);
    bool frame_accepted=frame_table(&frame_lexer,&temporary);free(temporary.frames);
    if(frame_accepted)return failure("Parser accepted invalid atlas UV bounds","self-test");
    memcpy(last_error,saved_error,sizeof last_error);
    printf("Resource checks: %d animations, %d frames inside original texture bounds; malformed/executable declarations rejected; UTF-16 font metrics loaded.\n",animations_count,frames);return true;
}
void resources_destroy(void) {
    log_particles[0]=log_particles[1]=NULL;
    for(int i=0;i<animations_count;i++)free(animations[i].frames);free(animations);animations=NULL;animations_count=0;
    for(int i=0;i<font_count;i++)free(fonts[i].glyphs);font_count=0;
    while(textures){Texture *next=textures->next;SDL_DestroyTexture(textures->handle);free(textures);textures=next;}
}
