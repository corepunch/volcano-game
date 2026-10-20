/* Mission map effects: foam, waterfalls, sparkles, insects, motes and
 * ground/overlay sprites. Stateless: every frame derives from time and seed,
 * so pause and restart stay deterministic. */
#include "render.h"
#include <math.h>
#include <stdint.h>

static void foam(const MapVisual *v,double time)
{
    double now=time+6;
    for (int pass=0;pass<2;pass++) {
        double rate=pass==0?v->rate:v->rate*.2;float max_life=pass==0?2.2f:3.4f;
        double phase=hash_random(v->seed+(uint32_t)pass*127u);
        int64_t latest=(int64_t)floor(now*rate-phase);
        for (int slot=(int)ceil(max_life*rate);slot>=0;slot--) {
            int64_t birth=latest-slot;float age=(float)(now-(birth+phase)/rate);
            uint32_t seed=(uint32_t)birth*747796405u+v->seed*2891336453u+(uint32_t)pass*277803737u;
            float r=hash_random(seed),s=hash_random(seed+1),life=max_life*(.7f+.3f*s);
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
    double clock=time*v->speed;float phase=hash_random(v->seed)*6.2831853f;
    if (v->kind==MAP_FOAM) foam(v,clock);
    else if (v->kind==MAP_SPRITE) {
        float wind=v->wind*(.72f*sinf((float)clock*.8f+v->p.x*.007f+v->p.y*.003f)+.28f*sinf((float)clock*1.7f+phase));
        draw_map_sprite(v->sprite,clock,v->p,vec2(v->scale_x,v->scale_y),wind,v->rotation,v->opacity);
    } else if (v->kind==MAP_INSECTS || v->kind==MAP_MOTES) {
        for (int i=0;i<v->count;i++) {
            uint32_t seed=v->seed+(uint32_t)i*127u;float p=hash_random(seed)*6.2831853f;
            float t=(float)clock*(.35f+.18f*hash_random(seed+1));
            Vec2 at={v->p.x+sinf(t+p)*v->extent.x*.5f+sinf(t*2.7f+p)*3,
                v->p.y+cosf(t*.83f+p)*v->extent.y*.5f+sinf(t*3.1f)*2};
            if (v->kind==MAP_INSECTS) draw_map_sprite(v->sprite,clock+p,at,vec2(.28f*v->scale_x,.28f*v->scale_x),0,9*sinf(t*2+p),v->opacity);
            else draw_glint(at.x,at.y,1.2f,((sinf(t*4+p)+1)*.22f+.12f)*v->opacity);
        }
    } else if (v->kind==MAP_SPARKLES) {
        double now=clock+3,phase_birth=hash_random(v->seed);
        int64_t latest=(int64_t)floor(now*v->rate-phase_birth);
        for (int slot=(int)ceil(v->rate*1.5);slot>=0;slot--) {
            int64_t birth=latest-slot;float age=(float)(now-(birth+phase_birth)/v->rate);
            uint32_t seed=(uint32_t)birth*747796405u+v->seed*2891336453u;
            float life=.6f+.8f*hash_random(seed);if (age<0 || age>=life) continue;
            float angle=hash_random(seed+1)*6.2831853f,radius=sqrtf(hash_random(seed+2));
            float x=v->p.x+cosf(angle)*radius*v->extent.x*.5f,y=v->p.y+sinf(angle)*radius*v->extent.y*.5f;
            float pulse=sinf(age/life*3.14159265f);pulse*=pulse;
            draw_glint(x,y,1.5f+2*hash_random(seed+3),pulse*v->opacity*.75f);
        }
    }
}
void map_scene_draw(const Game *g,MapVisualStage stage)
{
    for (int i=0;i<g->spec.visual_count;i++) if (g->spec.visuals[i].stage==stage) map_visual_draw(&g->spec.visuals[i],g->time);
}
