#include "volcano.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool bad(const LevelObject *raw,const char *field)
{
    snprintf(last_error,sizeof last_error,"Map visual %u: invalid %s",raw->id,field);return false;
}
static bool scalar(const LevelObject *raw,const char *key,float fallback,float low,float high,float *out)
{
    char value[4096];*out=fallback;
    if (!level_param(raw->params,key,value,sizeof value)) return true;
    char *end;errno=0;float result=strtof(value,&end);
    if (errno || end==value || *end || !isfinite(result) || result<low || result>high) return bad(raw,key);
    *out=result;return true;
}
static bool vector(const LevelObject *raw,const char *key,float *values,int count)
{
    char value[4096];if (!level_param(raw->params,key,value,sizeof value)) return true;
    char *at=value;
    for (int i=0;i<count;i++) {
        char *end;errno=0;float f=strtof(at,&end);
        if (errno || end==at || !isfinite(f) || (i<count-1?*end!=',':*end!=0)) return bad(raw,key);
        values[i]=f;at=end+1;
    }
    return true;
}
static bool local_image(const LevelObject *raw,const char *level_path,char *out,size_t size)
{
    char name[4096];
    if (!level_param(raw->params,"image",name,sizeof name) || name[0]!='@' || !name[1] ||
        strchr(name+1,'/') || strchr(name+1,'\\') || !strcmp(name+1,".") || !strcmp(name+1,"..")) return bad(raw,"image (expected @FILENAME)");
    const char *slash=strrchr(level_path,'/');
    int n=snprintf(out,size,"%.*s%s",slash?(int)(slash-level_path+1):0,level_path,name+1);
    return n>=0 && (size_t)n<size?true:bad(raw,"image path length");
}
bool map_visual_parse(MapScene *scene,const LevelObject *raw,const char *level_path,int *index)
{
    *index=-1;char kind[64];size_t n=strcspn(raw->params,";");
    if (n>=sizeof kind) return true;
    memcpy(kind,raw->params,n);kind[n]=0;
    MapVisualKind type;
    if (!strcmp(kind,"foam") || !strcmp(kind,"waterfall")) type=MAP_FOAM;
    else if (!strcmp(kind,"sprite")) type=MAP_SPRITE;
    else if (!strcmp(kind,"sparkles")) type=MAP_SPARKLES;
    else if (!strcmp(kind,"insects")) type=MAP_INSECTS;
    else if (!strcmp(kind,"motes")) type=MAP_MOTES;
    else return true;
    if (scene->visual_count==MAX_MAP_VISUALS) return bad(raw,"visual count (maximum 1024)");
    MapVisual v={.kind=type,.stage=type==MAP_SPRITE?MAP_WORLD:type==MAP_INSECTS || type==MAP_MOTES?MAP_OVERLAY:MAP_GROUND,
        .seed=raw->id,.p={raw->x,raw->y},.pivot={.5f,.94f},.scale_x=raw->scale_x,.scale_y=raw->scale_y,.rotation=raw->rotation};
    float count;
    if (!scalar(raw,"width",32,1,600,&v.extent.x) || !scalar(raw,"height",type==MAP_FOAM?28:40,1,600,&v.extent.y) ||
        !scalar(raw,"rate",type==MAP_FOAM?26:6,.1f,180,&v.rate) || !scalar(raw,"speed",1,.05f,6,&v.speed) ||
        !scalar(raw,"wind",0,0,16,&v.wind) || !scalar(raw,"opacity",1,0,1,&v.opacity) ||
        !scalar(raw,"count",type==MAP_INSECTS?3:8,1,32,&count)) return false;
    if (count!=floorf(count) || v.scale_x<.02f || v.scale_x>5 || v.scale_y<.02f || v.scale_y>5) return bad(raw,"scale or count");
    v.count=(int)count;
    if (!strcmp(kind,"waterfall")) {
        float height,drift;
        if (!scalar(raw,"height",72,16,220,&height) || !scalar(raw,"drift",0,-50,50,&drift)) return false;
        v.p.x+=drift;v.p.y-=height;v.extent.y=28;
    }
    char stage[4096];
    if (level_param(raw->params,"stage",stage,sizeof stage)) {
        if (!strcmp(stage,"ground")) v.stage=MAP_GROUND;
        else if (!strcmp(stage,"world")) v.stage=MAP_WORLD;
        else if (!strcmp(stage,"overlay")) v.stage=MAP_OVERLAY;
        else return bad(raw,"stage");
    }
    if (type==MAP_SPRITE) {
        if (!local_image(raw,level_path,v.image,sizeof v.image)) return false;
        int width,height;if (!image_file_size(v.image,&width,&height)) return false;
        float rect[4]={0,0,(float)width,(float)height},pivot[2]={v.pivot.x,v.pivot.y};
        if (!vector(raw,"rect",rect,4) || !vector(raw,"pivot",pivot,2)) return false;
        for (int i=0;i<4;i++) if (rect[i]<0 || rect[i]>8192 || rect[i]!=floorf(rect[i])) return bad(raw,"rect");
        v.region=(SDL_Rect){(int)rect[0],(int)rect[1],(int)rect[2],(int)rect[3]};
        if (!v.region.w || !v.region.h || v.region.x+v.region.w>width || v.region.y+v.region.h>height ||
            pivot[0]<0 || pivot[0]>1 || pivot[1]<0 || pivot[1]>1) return bad(raw,"atlas bounds or pivot");
        v.pivot=(Vec2){pivot[0],pivot[1]};
    } else if (type==MAP_INSECTS) {
        char animation[4096];
        bool named=level_param(raw->params,"animation",animation,sizeof animation);
        if (named && strlen(animation)>=sizeof v.animation) return bad(raw,"animation length");
        snprintf(v.animation,sizeof v.animation,"%s",named?animation:"butterfly");
        if (!animation_find(v.animation)) return bad(raw,"animation");
    }
    *index=scene->visual_count;scene->visuals[scene->visual_count++]=v;return true;
}
static float random_value(uint32_t seed)
{
    seed^=seed>>16;seed*=0x7feb352du;seed^=seed>>15;seed*=0x846ca68bu;seed^=seed>>16;
    return (seed>>8)*(1.f/16777216.f);
}
static void foam(const MapVisual *v,double time)
{
    double now=time+6;
    for (int pass=0;pass<2;pass++) {
        double rate=pass==0?v->rate:v->rate*.2;float max_life=pass==0?2.2f:3.4f;
        double phase=random_value(v->seed+(uint32_t)pass*127u);
        int64_t latest=(int64_t)floor(now*rate-phase);
        for (int slot=(int)ceil(max_life*rate);slot>=0;slot--) {
            int64_t birth=latest-slot;float age=(float)(now-(birth+phase)/rate);
            uint32_t seed=(uint32_t)birth*747796405u+v->seed*2891336453u+(uint32_t)pass*277803737u;
            float r=random_value(seed),s=random_value(seed+1),life=max_life*(.7f+.3f*s);
            if (age<0 || age>=life) continue;
            float t=age/life,fade=fminf(1,t*9)*fminf(1,(1-t)*4);
            float x=v->p.x+(r-.5f)*v->extent.x*1.1f+(s-.5f)*5*age;
            float y=v->p.y+(r-.5f)*6+(.5f-t)*12;
            float width=pass==0?3+4*s+age:6+age*3,height=width*(pass==0?.55f:.42f);
            float alpha=fade*(pass==0?205+50*r:165+50*s)*v->opacity;
            draw_foam_particle(x,y,width,height,r*360+age*(s-.5f)*15,(Uint8)alpha,pass==1);
        }
    }
}
void map_visual_draw(const MapVisual *v,double time)
{
    double clock=time*v->speed;float phase=random_value(v->seed)*6.2831853f;
    if (v->kind==MAP_FOAM) foam(v,clock);
    else if (v->kind==MAP_SPRITE) {
        float wind=v->wind*(.72f*sinf((float)clock*.8f+v->p.x*.007f+v->p.y*.003f)+.28f*sinf((float)clock*1.7f+phase));
        draw_map_sprite(v->image,v->region,v->pivot,v->p,(Vec2){v->scale_x,v->scale_y},wind,v->rotation,v->opacity);
    } else if (v->kind==MAP_INSECTS || v->kind==MAP_MOTES) {
        for (int i=0;i<v->count;i++) {
            uint32_t seed=v->seed+(uint32_t)i*127u;float p=random_value(seed)*6.2831853f;
            float t=(float)clock*(.35f+.18f*random_value(seed+1));
            Vec2 at={v->p.x+sinf(t+p)*v->extent.x*.5f+sinf(t*2.7f+p)*3,
                v->p.y+cosf(t*.83f+p)*v->extent.y*.5f+sinf(t*3.1f)*2};
            if (v->kind==MAP_INSECTS) draw_map_animation(v->animation,at,clock+p,.28f*v->scale_x,9*sinf(t*2+p),v->opacity);
            else draw_glint(at.x,at.y,1.2f,((sinf(t*4+p)+1)*.22f+.12f)*v->opacity);
        }
    } else if (v->kind==MAP_SPARKLES) {
        double now=clock+3,phase_birth=random_value(v->seed);
        int64_t latest=(int64_t)floor(now*v->rate-phase_birth);
        for (int slot=(int)ceil(v->rate*1.5);slot>=0;slot--) {
            int64_t birth=latest-slot;float age=(float)(now-(birth+phase_birth)/v->rate);
            uint32_t seed=(uint32_t)birth*747796405u+v->seed*2891336453u;
            float life=.6f+.8f*random_value(seed);if (age<0 || age>=life) continue;
            float angle=random_value(seed+1)*6.2831853f,radius=sqrtf(random_value(seed+2));
            float x=v->p.x+cosf(angle)*radius*v->extent.x*.5f,y=v->p.y+sinf(angle)*radius*v->extent.y*.5f;
            float pulse=sinf(age/life*3.14159265f);pulse*=pulse;
            draw_glint(x,y,1.5f+2*random_value(seed+3),pulse*v->opacity*.75f);
        }
    }
}
void map_scene_draw(const MapScene *scene,MapVisualStage stage,double time)
{
    for (int i=0;i<scene->visual_count;i++) if (scene->visuals[i].stage==stage) map_visual_draw(&scene->visuals[i],time);
}
