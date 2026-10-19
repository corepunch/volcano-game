/* The playfield: background, water, entities in depth order, selection
 * rings, work progress, burning-log particles and debug overlays. */
#include "render.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const Entity *entity;uint32_t layer;float depth; } DrawEntry;

/* Keep bobbing and roof attachment out of the ground-position sort key. */
static DrawEntry draw_entry(const Entity *e) { return (DrawEntry){e,e->layer,e->p.y}; }

static int draw_entry_compare(const void *a,const void *b)
{
    const DrawEntry *aa=a,*bb=b;
    if (aa->layer!=bb->layer) return aa->layer<bb->layer?-1:1;
    return aa->depth==bb->depth?aa->entity->order-bb->entity->order:aa->depth>bb->depth?-1:1;
}

bool game_on_roof(const Entity *e) { return e->roof && e->support>=0 && !e->done && e->follower<0; }

VisualPose game_visual_pose(const Game *g,const Entity *e)
{
    VisualPose pose={e->p,0};
    if (e->type->ambient) {
        float margin=e->type->ambient==AMBIENT_BIRD?96:180,span=GAME_WIDTH+2*margin;
        double travel=g->time*e->travel_speed+hash_random(e->id+3)*span;
        uint32_t pass=(uint32_t)floor(travel/span);
        pose.p.x=(float)fmod(travel,span)-margin;
        pose.p.y=70+hash_random(e->id+pass*747796405u+9)*(GAME_HEIGHT-200);
        if (e->type->ambient==AMBIENT_BIRD) pose.p.y+=5*sinf((float)g->time*1.2f+e->id);
    } else if (game_on_roof(e)) {
        const Entity *house=&g->objects[e->support];VisualPose parent=game_visual_pose(g,house);
        float angle=parent.angle*(float)(PI/180),c=cosf(angle),s=sinf(angle);
        float dx=e->p.x-house->p.x,dy=e->p.y-house->p.y;
        pose.p=vec2(parent.p.x+c*dx+s*dy,parent.p.y-s*dx+c*dy);pose.angle=parent.angle;
    } else if (e->floating) {
        float phase=hash_random(e->id)*2*(float)PI;
        float wave=(float)g->time*(1.2f+hash_random(e->id+1)*.5f)+phase;
        pose.p.y+=2.3f*sinf(wave);pose.angle=1.8f*sinf(wave+.7f);
    }
    return pose;
}

int game_depth_compare(const Game *g,const void *a,const void *b)
{
    (void)g;
    const Entity *aa=*(const Entity *const *)a,*bb=*(const Entity *const *)b;
    DrawEntry first=draw_entry(aa),second=draw_entry(bb);
    return draw_entry_compare(&first,&second);
}

static void paint(const Game *g,const Entity *e)
{
    if (e->hidden) return;
    VisualPose pose=game_visual_pose(g,e);char name[160];
    switch (e->type->effect) {
    case EFFECT_FOG: {
        char path[192];snprintf(path,sizeof path,"texture/static/part_%s.png",e->sprite?e->sprite:"fog_high");
        draw_asset_image(path,pose.p.x-64,pose.p.y-32,128,96,NULL);return;
    }
    case EFFECT_FIRE:
        draw_particle(false,(int)(g->time*15)%32,pose.p.x,pose.p.y+18,64,0,game_color(255,255,255,255));return;
    case EFFECT_POLLUTION:
        if (game_entity_sprite(g,e,name,sizeof name)) draw_sprite_tinted(name,pose.p.x,pose.p.y,g->time,0,game_color(95,175,65,215));
        return;
    default: break;
    }
    if (!game_entity_sprite(g,e,name,sizeof name)) return;
    double time=g->time+(e->type->ambient==AMBIENT_BIRD?hash_random(e->id)*2.4:0);
    Vec2 origin=archetype_sprite_origin(e->type,sprite_find(name),e->frame,pose.p);
    draw_sprite_rotated(name,origin.x,origin.y,time,e->frame,e->type->opacity,pose.angle);
}

void game_paint_log_particles(const Game *g,const Entity *e)
{
    if (!game_burning_log(e) || (e->hidden && !e->done) || (e->done && e->fire_stopped<0)) return;
    /* Seven seconds of birth history starts established fires fully alight.
     * No render-time state: pause, redraws and restart remain deterministic. */
    double now=g->time+7,stop=e->fire_stopped+7;
    if (e->done && now-stop>=7) return;
    float phase=hash_random((uint32_t)e->order+91u);
    float flicker=.88f+.08f*(float)sin(now*9+phase*19)+.04f*(float)sin(now*21+phase*37);
    float heat=e->done?fmaxf(0,1-(float)(now-stop)/.8f):1;
    if (heat>0) {
        draw_particle_glow(e->p.x,e->p.y+4,64*flicker,25,game_color(255,83,9,(int)(55*heat)));
        draw_particle_glow(e->p.x,e->p.y+9,31,20,game_color(255,153,32,(int)(70*heat)));
    }
    for (int layer=0;layer<3;layer++) {
        if (layer==1) {
            /* Two independently phased patches, each with the authored back
             * and front layers. Their roots stay on the wood while the tips
             * bend in the updraft. Cooling lifts and dissolves the last flame. */
            if (heat<=0) continue;
            for (int patch=0;patch<2;patch++) {
                float r=hash_random((uint32_t)e->order*17u+patch+13u);
                float wave=(float)sin(now*(5+r)+r*31);
                float width=(38+4*r)*(.8f+.2f*heat);
                float height=(80+5*r)*(1+.055f*wave);
                float x=e->p.x+(patch?8:-8)+wave*1.3f;
                float y=e->p.y+3+height*.5f+(1-heat)*8;
                int frame=(int)(now*(25+2*r)+r*40+patch*17)%40;
                draw_log_particle(false,frame,x,y,width,height,wave*3,game_color(255,182,116,(int)(heat*240)));
                draw_log_particle(false,frame+40,x,y,width,height,wave*3,game_color(255,236,185,(int)(heat*245)));
            }
            continue;
        }
        bool smoke=layer==0;
        double rate=smoke?2.8:2.5,max_life=smoke?7:2;
        int64_t latest=(int64_t)floor(now*rate-phase);
        for (int slot=(int)ceil(max_life*rate);slot>=0;slot--) {
            int64_t birth=latest-slot;double born=(birth+phase)/rate,age=now-born;
            if (e->done && born>stop) continue;
            uint32_t seed=(uint32_t)birth*747796405u+(uint32_t)e->order*2891336453u+(uint32_t)layer*277803737u;
            float life=(smoke?5:1.1f)+hash_random(seed)*(smoke?2:.9f);
            if (age<0 || age>=life) continue;
            float t=(float)(age/life),a=(float)age,r=hash_random(seed+1),s=hash_random(seed+2);
            float curl=sinf(a*(smoke?1.7f:5)+s*6.2831853f)-sinf(s*6.2831853f);
            if (smoke) {
                float x=e->p.x+(r-.5f)*23+(2+3*s)*a+curl*(2+3*t);
                float y=e->p.y+22+(13+4*r)*a+1.2f*a*a;
                float size=17+7*s+6*a;
                float opacity=fminf(1,a*1.8f)*(1-t)*(1-t)*.28f;
                int shade=(int)(103+65*t);
                draw_log_particle(true,(int)(s*4),x,y,size,size*.92f,curl*4,game_color(shade+5,shade+2,shade,(int)(opacity*255)));
            } else {
                float x=e->p.x+(r-.5f)*29+(s-.35f)*12*a+curl*3*t;
                float y=e->p.y+12+(27+17*s)*a+4*a*a;
                float opacity=fminf(1,a*12)*(1-t)*(1-t);
                draw_particle_glow(x,y,2.5f*(1-t)+.6f,5*(1-t)+1,game_color(255,102,9,(int)(opacity*200)));
                draw_particle_glow(x,y,.9f,1.5f,game_color(255,216,106,(int)(opacity*245)));
            }
        }
    }
}

static void draw_progress(const Game *g,const Entity *target,double fraction)
{
    VisualPose pose=game_visual_pose(g,target);float x=pose.p.x,y=pose.p.y-37;
    draw_sprite("progress",x,y,0,0,1);
    SDL_Rect previous;bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&previous);
    SDL_Rect clip={(int)(x-33),(int)(GAME_HEIGHT-y-6),(int)(66*fmax(0,fmin(1,fraction))),12};
    SDL_RenderSetClipRect(renderer,&clip);draw_sprite("progress",x,y,0,1,1);
    SDL_RenderSetClipRect(renderer,clipped?&previous:NULL);
}

/* The selection ring is the footprint: workers stop on this ellipse. */
static void draw_selection(const Game *g,int selected)
{
    const Entity *e=&g->objects[selected];Vec2 axes=game_footprint(g,e);
    bool ready=(e->kind==BASE || game_reachable(g,selected)) && game_affordable(g,game_job_cost(g,e)) &&
        (!g->rules->vehicles || e->transport==TRANSPORT_FOOT || g->stock.fuel>=10);
    VisualPose pose=game_visual_pose(g,e);
    draw_atlas("texture/gui/highlight.png",(SDL_FRect){pose.p.x-axes.x,pose.p.y-axes.y,axes.x*2,axes.y*2},
        (SDL_FRect){.0078125f,ready?.140625f:.5703125f,.6171875f,.421875f});
}

static void draw_background(const Game *g)
{
    if (g->spec.retail_background) draw_asset_image(g->spec.background,0,0,GAME_WIDTH,GAME_HEIGHT,g->spec.background_alpha);
    else draw_image_file(g->spec.background,0,0,GAME_WIDTH,GAME_HEIGHT);
}

void game_render(Game *g)
{
    if (!g || !g->ready) return;
    /* Water draws beneath the background, which is transparent over it. */
    for (int pass=0;pass<2;pass++) for (int i=0;i<g->object_count;i++) {
        const Entity *e=&g->objects[i];
        if (e->kind==WATER && (e->type->effect==EFFECT_POLLUTION)==(pass==1)) paint(g,e);
    }
    draw_background(g);
    map_scene_draw(g,MAP_GROUND);
    if (g->debug) for (int i=0;i<g->traffic_count;i++) for (int k=g->link_start[i];k<g->link_start[i+1];k++) {
        int j=g->links[k];if (j<i) continue;
        Vec2 a=g->traffic[i],b=g->traffic[j];
        draw_line(a.x,GAME_HEIGHT-a.y,b.x,GAME_HEIGHT-b.y,game_blocked(g,a,b,-1)?game_color(255,70,100,180):game_color(70,220,100,180));
    }
    int hover=game_hovered_object(g);
    if (!g->paused && (hover>=0 || g->build_menu>=0)) draw_selection(g,g->build_menu>=0?g->build_menu:hover);
    DrawEntry *sorted=malloc((size_t)(g->object_count?g->object_count:1)*sizeof *sorted);int count=0;
    if (!sorted) return;
    for (int i=0;i<g->object_count;i++) if (g->objects[i].kind!=WATER) sorted[count++]=draw_entry(&g->objects[i]);
    qsort(sorted,(size_t)count,sizeof *sorted,draw_entry_compare);
    for (int i=0;i<count;i++) {
        const Entity *e=sorted[i].entity;
        if (e->hidden) { game_paint_log_particles(g,e);continue; }
        if (e->type->shadow && !game_on_roof(e)) draw_sprite(e->type->shadow,e->p.x,e->p.y,g->time,-1,1);
        paint(g,e);
        game_paint_log_particles(g,e);
    }
    free(sorted);
    for (int i=0;i<g->worker_count;i++) {
        const Worker *w=&g->workers[i];const Entity *e=&g->objects[w->entity];
        if (w->phase==WORKING) { const Entity *t=&g->objects[w->target];draw_progress(g,t,w->work/t->type->duration); }
        if (g->debug && (w->phase==OUTBOUND || w->phase==RETURNING)) for (int j=w->step;j<w->count;j++) {
            Vec2 a=j==w->step?e->p:w->path[j-1],b=w->path[j];
            draw_line(a.x,GAME_HEIGHT-a.y,b.x,GAME_HEIGHT-b.y,game_color(255,224,90,255));
        }
    }
    for (int i=0;i<g->object_count;i++) {
        const Entity *e=&g->objects[i];
        if (!e->hidden && (e->queued || e->assigned)) {
            bool working=false;
            for (int j=0;j<g->worker_count;j++) if (g->workers[j].target==i && g->workers[j].phase==WORKING) working=true;
            VisualPose pose=game_visual_pose(g,e);
            if (!working) draw_sprite("target",pose.p.x+22,pose.p.y-12,0,0,1);
        }
        if (e->kind==BASE) {
            int drawn=0,total=0;
            for (int j=0;j<g->worker_count;j++) if (g->workers[j].home_base==i && g->workers[j].phase==IDLE) total++;
            for (int j=0;j<g->worker_count;j++) if (g->workers[j].home_base==i && g->workers[j].phase==IDLE)
                draw_sprite("icon_head",e->p.x+(drawn++-(total-1)*.5f)*23,e->p.y+65,0,0,1);
        }
    }
    map_scene_draw(g,MAP_OVERLAY);
    game_draw_context(g);game_draw_hud(g);
}
