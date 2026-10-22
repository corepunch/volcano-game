/* SDL drawing primitives and the texture cache. Sprites and fonts come from
 * the catalog (assets/); images resolve through the asset mounts. */
#include "render.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Texture { char path[PATH_CAPACITY],alpha[PATH_CAPACITY];SDL_Texture *handle;int w,h;struct Texture *next; } Texture;
SDL_Renderer *renderer;
static int canvas_width=GAME_WIDTH,canvas_height=GAME_HEIGHT;
void render_set_canvas(int width,int height) { canvas_width=width;canvas_height=height; }
int render_width(void) { return canvas_width; }
int render_height(void) { return canvas_height; }
static Texture *textures;
static Texture *log_particles[2];

static bool failure(const char *message,const char *path) { return error_set("%s: %s",message,path); }

SDL_Color game_color(int r,int g,int b,int a) { return (SDL_Color){(Uint8)r,(Uint8)g,(Uint8)b,(Uint8)a}; }

static Texture *texture_load(const char *path,const char *alpha) {
    if(!alpha)alpha="";
    for(Texture *t=textures;t;t=t->next)if(!strcmp(path,t->path)&&!strcmp(alpha,t->alpha)){return t;}
    int w,h;unsigned char *pixels=image_decode(path,&w,&h);if(!pixels)return NULL;
    if(*alpha){int aw,ah;unsigned char *mask=image_decode_gray(alpha,&aw,&ah);
        if(!mask||aw!=w||ah!=h){if(mask)image_release(mask);image_release(pixels);failure("Invalid JPEG alpha mask",alpha);return NULL;}
        for(size_t i=0;i<(size_t)w*h;i++){pixels[i*4+3]=mask[i];}
        image_release(mask);}
    SDL_Texture *handle=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,w,h);
    if(!handle){image_release(pixels);failure("Cannot create texture",SDL_GetError());return NULL;}
    int result=SDL_UpdateTexture(handle,NULL,pixels,w*4);image_release(pixels);
    if(result){SDL_DestroyTexture(handle);failure("Cannot upload image",SDL_GetError());return NULL;}
    Texture *t=calloc(1,sizeof(*t));if(!t){SDL_DestroyTexture(handle);return NULL;}
    snprintf(t->path,sizeof(t->path),"%s",path);snprintf(t->alpha,sizeof(t->alpha),"%s",alpha);t->handle=handle;t->w=w;t->h=h;t->next=textures;textures=t;SDL_SetTextureBlendMode(handle,SDL_BLENDMODE_BLEND);return t;
}
void render_shutdown(void) {
    log_particles[0]=log_particles[1]=NULL;
    while(textures){Texture *next=textures->next;SDL_DestroyTexture(textures->handle);free(textures);textures=next;}
}
static SDL_Rect frame_source(const SpriteFrame *f,const Texture *t) {
    return (SDL_Rect){(int)lroundf(f->u*t->w),(int)lroundf((1-f->v-f->cv)*t->h),(int)lroundf(f->cu*t->w),(int)lroundf(f->cv*t->h)};
}
static const SpriteFrame *sprite_frame(const Sprite *a,double time,int frame) {
    int index=frame>=0?frame:a->fps>0?(int)(time*a->fps)%a->frame_count:0;if(index>=a->frame_count)index=a->frame_count-1;
    return &a->frames[index];
}
static bool sprite_color(const char *name,float x,float y,double time,int frame,float alpha,float angle,SDL_Color tint) {
    const Sprite *a=sprite_find(name);if(!a || !a->frame_count)return false;
    const SpriteFrame *f=sprite_frame(a,time,frame);Texture *t=texture_load(a->texture,NULL);if(!t)return false;
    SDL_Rect src=frame_source(f,t);
    SDL_FRect dest={x-f->dx,render_height()-y-f->h+f->dy,f->w,f->h};SDL_SetTextureAlphaMod(t->handle,(Uint8)(fminf(1,fmaxf(0,alpha))*255));
    /* Rotate about the authored anchor, so attached roof occupants share the
     * house transform even when their trimmed animation frames change size. */
    SDL_FPoint pivot={f->dx,f->h-f->dy};
    SDL_SetTextureColorMod(t->handle,tint.r,tint.g,tint.b);
    int result=angle?SDL_RenderCopyExF(renderer,t->handle,&src,&dest,angle,&pivot,SDL_FLIP_NONE):SDL_RenderCopyF(renderer,t->handle,&src,&dest);
    SDL_SetTextureColorMod(t->handle,255,255,255);
    if(result){failure("Sprite render failed",SDL_GetError());return false;}return true;
}
bool draw_sprite(const char *name,float x,float y,double time,int frame,float alpha) {
    return sprite_color(name,x,y,time,frame,alpha,0,(SDL_Color){255,255,255,255});
}
bool draw_sprite_rotated(const char *name,float x,float y,double time,int frame,float alpha,float angle) {
    return sprite_color(name,x,y,time,frame,alpha,angle,(SDL_Color){255,255,255,255});
}
bool draw_sprite_tinted(const char *name,float x,float y,double time,int frame,SDL_Color tint) {
    return sprite_color(name,x,y,time,frame,tint.a/255.f,0,tint);
}
bool draw_asset_image(const char *relative,float x,float y,float w,float h,const char *alpha) {
    char path[PATH_CAPACITY],mask[PATH_CAPACITY]="";
    if(!vfs_resolve(relative,path,sizeof path))return failure("Missing image",relative);
    if(alpha&&*alpha&&!vfs_resolve(alpha,mask,sizeof mask))return failure("Missing alpha mask",alpha);
    Texture *t=texture_load(path,mask);if(!t)return false;
    SDL_FRect dest={x,render_height()-y-h,w,h};SDL_SetTextureAlphaMod(t->handle,255);return SDL_RenderCopyF(renderer,t->handle,NULL,&dest)==0;
}
bool draw_image_file(const char *path,float x,float y,float w,float h) {
    Texture *t=texture_load(path,NULL);if(!t)return false;
    SDL_FRect dest={x,render_height()-y-h,w,h};SDL_SetTextureAlphaMod(t->handle,255);return SDL_RenderCopyF(renderer,t->handle,NULL,&dest)==0;
}
static bool draw_geometry(const char *path,SDL_Rect region,Vec2 pivot,Vec2 position,Vec2 scale,float bend,float angle,float opacity) {
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
                .position={position.x+c*local_x-s*local_y,render_height()-position.y-s*local_x-c*local_y},
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
bool draw_map_sprite(const char *name,double time,Vec2 position,Vec2 scale,float bend,float angle,float opacity) {
    const Sprite *a=sprite_find(name);if(!a || !a->frame_count)return failure("Missing map sprite",name);
    const SpriteFrame *f=sprite_frame(a,time,-1);Texture *t=texture_load(a->texture,NULL);if(!t)return false;
    SDL_Rect region=frame_source(f,t);
    Vec2 pivot={f->dx/f->w,(f->h-f->dy)/f->h};
    Vec2 ratio={scale.x*f->w/region.w,scale.y*f->h/region.h};
    return draw_geometry(a->texture,region,pivot,position,ratio,bend,angle,opacity);
}
void draw_glint(float x,float y,float radius,float opacity) {
    SDL_Vertex vertices[9];int indices[24];
    vertices[0]=(SDL_Vertex){.position={x,render_height()-y},.color={255,255,255,(Uint8)(fminf(1,fmaxf(0,opacity))*255)}};
    for(int i=0;i<8;i++) {
        float angle=i*.7853981634f,r=i%2?radius*.17f:radius;
        vertices[i+1]=(SDL_Vertex){.position={x+cosf(angle)*r,render_height()-y+sinf(angle)*r},.color={255,255,255,0}};
        indices[i*3]=0;indices[i*3+1]=i+1;indices[i*3+2]=(i+1)%8+1;
    }
    if(SDL_RenderGeometry(renderer,NULL,vertices,9,indices,24))failure("Glint render failed",SDL_GetError());
}
bool draw_atlas(const char *relative,SDL_FRect rect,SDL_FRect uv) {
    char path[PATH_CAPACITY];if(!vfs_resolve(relative,path,sizeof path))return failure("Missing GUI atlas",relative);
    Texture *t=texture_load(path,NULL);if(!t)return false;
    SDL_Rect src={(int)lroundf(uv.x*t->w),(int)lroundf((1-uv.y-uv.h)*t->h),
        (int)lroundf(uv.w*t->w),(int)lroundf(uv.h*t->h)};
    if(src.w<=0||src.h<=0||rect.w<=0||rect.h<=0)return true;
    if(src.x<0||src.y<0||src.x+src.w>t->w||src.y+src.h>t->h)return failure("GUI region outside original atlas",relative);
    rect.y=render_height()-rect.y-rect.h;
    SDL_SetTextureAlphaMod(t->handle,255);SDL_SetTextureColorMod(t->handle,255,255,255);
    return SDL_RenderCopyF(renderer,t->handle,&src,&rect)==0;
}
bool draw_particle(bool smoke,int frame,float x,float y,float diameter,float angle,SDL_Color tint) {
    return draw_particle_stretched(smoke,frame,x,y,diameter,diameter,angle,tint);
}
bool draw_particle_stretched(bool smoke,int frame,float x,float y,float w,float h,float angle,SDL_Color tint) {
    char path[PATH_CAPACITY];
    const char *relative=smoke?"texture/static/part_smoke.png":"texture/static/part_fire.png";
    if(!vfs_resolve(relative,path,sizeof path))return failure("Missing particle texture",relative);
    Texture *t=texture_load(path,NULL);if(!t)return false;
    frame=frame<0?0:frame>31?31:frame;
    SDL_Rect src=smoke?(SDL_Rect){0,0,t->w,t->h}:
        (SDL_Rect){(frame%8)*(t->w/8),(frame/8)*(t->h/4),t->w/8,t->h/4};
    SDL_FRect dest={x-w*.5f,render_height()-y-h*.5f,w,h};
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
                .position={x+cosf(angle)*w*.5f*radius[ring],render_height()-y+sinf(angle)*h*.5f*radius[ring]},
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
    const char *relative=smoke?"texture/particles/log-smoke.png":"texture/particles/log-fire.png";
    char path[PATH_CAPACITY];
    if(!vfs_resolve(relative,path,sizeof path)){failure("Missing log particle atlas",relative);return NULL;}
    Texture *t=texture_load(path,NULL);if(!t)return NULL;
    if((smoke && (t->w!=528 || t->h!=132)) || (!smoke && (t->w!=680 || t->h!=1270))) {
        failure("Invalid log particle atlas dimensions",path);return NULL;
    }
    return log_particles[smoke]=t;
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
            .position={x+(side-.5f)*w+bend*lift*lift,render_height()-y+(f-.5f)*h},
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
                .position={x+c*dx-s*dy,render_height()-y-s*dx-c*dy},
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
float text_width(const char *name,const char *text,int size) {
    const Font *f=font_find(name);if(!f||f->size<=0)return 0;
    float width=0;
    while(*text){uint32_t code=utf8_next(&text);for(int i=0;i<f->count;i++)if(f->glyphs[i].code==code){width+=f->glyphs[i].advance*(float)size/f->size;break;}}
    return width;
}
void draw_font(const char *name,const char *text,float x,float y,int size,SDL_Color color) {
    const Font *f=font_find(name);if(!f||f->size<=0)return;
    Texture *t=texture_load(f->texture,NULL);if(!t)return;float scale=(float)size/f->size,left=x,baseline=y+size;
    SDL_SetTextureColorMod(t->handle,color.r,color.g,color.b);SDL_SetTextureAlphaMod(t->handle,color.a);
    while(*text){uint32_t code=utf8_next(&text);if(code=='\n'){x=left;baseline+=size;continue;}const Glyph *g=NULL;
        for(int i=0;i<f->count;i++)if(f->glyphs[i].code==code){g=&f->glyphs[i];break;}
        if(!g)for(int i=0;i<f->count;i++)if(f->glyphs[i].code=='?'){g=&f->glyphs[i];break;}
        if(!g)continue;
        if(g->w&&g->h){SDL_Rect src={g->x,g->y,g->w,g->h};SDL_FRect dst={x+g->bx*scale,baseline-g->by*scale,g->w*scale,g->h*scale};SDL_RenderCopyF(renderer,t->handle,&src,&dst);}x+=g->advance*scale;
    }
    SDL_SetTextureColorMod(t->handle,255,255,255);SDL_SetTextureAlphaMod(t->handle,255);
}
void draw_rect(float x,float y,float w,float h,SDL_Color c){SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);SDL_FRect r={x,y,w,h};SDL_RenderFillRectF(renderer,&r);}
void draw_line(float x,float y,float xx,float yy,SDL_Color c){SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);SDL_RenderDrawLineF(renderer,x,y,xx,yy);}
bool renderer_screenshot(const char *path) {
    int w,h;SDL_GetRendererOutputSize(renderer,&w,&h);SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);if(!surface)return false;
    bool ok=SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_RGBA32,surface->pixels,surface->pitch)==0&&SDL_SaveBMP(surface,path)==0;SDL_FreeSurface(surface);return ok;
}
