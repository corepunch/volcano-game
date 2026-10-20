/* Art selection from archetype + state, and the footprint derived from it.
 * Shared by the renderer (drawing, selection ring, hit tests) and the
 * simulation (where workers stand), so both always agree. */
#include "world.h"
#include "../assets/assets.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool expand(const char *template,const Entity *e,char *out,size_t size)
{
    size_t n=0;
    for (const char *s=template;*s;) {
        char value[64];const char *insert=NULL;size_t skip=1;
        if (!strncmp(s,"{d}",3)) { snprintf(value,sizeof value,"%d",e->direction);insert=value;skip=3; }
        else if (!strncmp(s,"{v}",3)) { snprintf(value,sizeof value,"%d",e->variant);insert=value;skip=3; }
        else if (!strncmp(s,"{action}",8)) { insert=e->pose_action?e->pose_action:"";skip=8; }
        if (insert) {
            size_t length=strlen(insert);
            if (n+length>=size) return false;
            memcpy(out+n,insert,length);n+=length;
        } else {
            if (n+1>=size) return false;
            out[n++]=*s;
        }
        s+=skip;
    }
    out[n]=0;return n>0;
}

static const char *actor_template(const Archetype *t,PoseAction pose)
{
    switch (pose) {
    case POSE_MOVE: return t->move;
    case POSE_CARRY: return t->carry?t->carry:t->move;
    case POSE_WORK: return t->work?t->work:t->idle;
    case POSE_RUN: return t->run?t->run:t->move;
    case POSE_ACT: return t->act?t->act:t->idle;
    default: return t->idle;
    }
}

bool game_entity_sprite(const Game *g,const Entity *e,char *out,size_t size)
{
    (void)g;const Archetype *t=e->type;const char *template;
    switch (e->kind) {
    case WORKER: case RECRUIT: case SURVIVOR: case ANIMAL: case VEHICLE:
        template=actor_template(t,e->pose);break;
    case BUILDING:
        template=e->sprite?e->sprite:e->operational?t->sprite:t->damaged;break;
    case BUILD_LOT:
        template=e->constructing && e->building?e->building->damaged:t->sprite;break;
    case SERVICE:
        template=e->operational || !t->damaged?t->sprite:t->damaged;break;
    default:
        template=e->sprite?e->sprite:t->sprite;break;
    }
    return template && expand(template,e,out,size);
}

/* Centred on the object's position; for objects with a base that is the
 * horizontal centre of the same body (see archetype_sprite_origin). */
Vec2 game_footprint(const Game *g,const Entity *e)
{
    char name[160];const Sprite *s;float w=100;
    if (game_entity_sprite(g,e,name,sizeof name) && (s=sprite_find(name)) && s->frame_count) {
        float left,right;sprite_body_span(s,e->frame,&left,&right);
        w=fmaxf(65,right-left+24);
    }
    w=fminf(w,170);
    return vec2(w*.5f,w*.62f*.5f);
}

const char *game_entity_title(const Game *g,const Entity *e)
{
    (void)g;
    if (e->kind==BUILD_LOT && e->assigned && e->building) return e->building->title;
    return e->type->title?e->type->title:"Building";
}
