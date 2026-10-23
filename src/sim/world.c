/* World creation from a format-neutral LevelSpec. */
#include "world.h"
#include "../assets/assets.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void game_announce(Game *g,const char *text) { snprintf(g->message,sizeof g->message,"%s",text);g->message_time=4; }

static EntityKind kind_of(ArchetypeClass c)
{
    static const EntityKind kinds[CLASS_COUNT]={
        [CLASS_DECOR]=DECOR,[CLASS_WATER]=WATER,[CLASS_BASE]=BASE,[CLASS_OBSTACLE]=BARRIER,[CLASS_PICKUP]=RESOURCE,
        [CLASS_CIVILIAN]=SURVIVOR,[CLASS_WORKER]=WORKER,[CLASS_LOT]=BUILD_LOT,[CLASS_PRODUCER]=BUILDING,
        [CLASS_ANIMAL]=ANIMAL,[CLASS_VALVE]=VALVE,[CLASS_SERVICE]=SERVICE,[CLASS_VEHICLE]=VEHICLE};
    return kinds[c];
}

static bool build_links(Game *g)
{
    int n=g->traffic_count,count=0;
    g->link_start=malloc((size_t)(n+1)*sizeof *g->link_start);
    if (!g->link_start) return error_set("Out of memory building the traffic graph");
    for (int pass=0;pass<2;pass++) {
        count=0;
        for (int i=0;i<n;i++) {
            if (pass) g->link_start[i]=count;
            for (int j=0;j<n;j++) if (j!=i && vec2_distance(g->traffic[i],g->traffic[j])<LINK_DISTANCE) {
                if (pass) g->links[count]=j;
                count++;
            }
        }
        if (!pass && !(g->links=malloc((size_t)(count?count:1)*sizeof *g->links))) return error_set("Out of memory building the traffic graph");
    }
    g->link_start[n]=count;return true;
}

static bool add_worker(Game *g,Worker *list,int *count,int entity)
{
    if (*count>=g->object_count) return error_set("Too many actors");
    Worker *w=&list[(*count)++];memset(w,0,sizeof *w);
    w->entity=entity;w->target=w->home_base=-1;w->cargo=RESOURCE_NONE;w->home=g->objects[entity].p;
    return (w->path=malloc((size_t)game_path_capacity(g)*sizeof *w->path))!=NULL || error_set("Out of memory");
}

static bool spawn(Game *g,int i)
{
    const Placement *p=&g->spec.placements[i];Entity *e=&g->objects[i];
    e->id=p->id;e->order=e->source=i;e->kind=kind_of(p->type->kind);e->type=e->placed=p->type;
    e->sprite=p->sprite;e->layer=p->layer;e->variant=p->variant;
    e->p=e->origin=e->roam=e->workpoint=p->p;
    if ((e->has_workpoint=p->has_workpoint)) e->workpoint=p->workpoint;
    e->frame=e->kind==BARRIER || e->kind==RESOURCE?0:-1;
    e->follower=e->owner=e->output=e->checked_route=-1;
    e->gate=p->gate;e->support=p->support;e->fire_stopped=-1;
    e->fire=p->burning;e->floating=p->floating;e->roof=p->roof;
    e->transport=p->transport;e->travel_speed=p->travel_speed;
    if ((e->has_corridor=p->has_corridor)) { e->corridor=p->corridor;e->corridor_radius=p->corridor_radius; }
    if (e->kind==SERVICE) e->operational=p->operational;
    if (e->kind==BUILDING) { e->operational=g->rules->economy && p->operational;e->frame=g->rules->economy?0:-1; }
    if (e->kind==ANIMAL) g->animals++;
    if (e->kind==BARRIER || e->kind==RESOURCE || e->kind==SURVIVOR) {
        g->target_count++;
        if (e->kind==BARRIER) g->barriers++;
        else if (e->kind==RESOURCE) g->resources++;
        else g->survivors++;
    }
    char sprite[160];const Sprite *art=NULL;
    if (p->type->effect==EFFECT_NONE && game_entity_sprite(g,e,sprite,sizeof sprite) && !(art=sprite_find(sprite)))
        return error_set("Missing sprite '%s' for object %u (%s)",sprite,p->id,p->type->id);
    /* A position given as a sprite pivot becomes the base centre, so the object
     * draws exactly where its source format put it while its ring and approach
     * move to the middle of its base. */
    if (g->spec.positions_are_pivots && art && art->frame_count && archetype_has_base(p->type)) {
        float shift=sprite_base_x(art,e->frame);
        e->p.x+=shift;e->origin.x+=shift;e->roam.x+=shift;
        if (!e->has_workpoint) e->workpoint.x+=shift;
    }
    return true;
}

static bool place_crew(Game *g)
{
    if (!g->worker_count || !g->traffic_count) return error_set("Level has no workers or traffic graph");
    int base=-1;
    for (int j=0;j<g->object_count && base<0;j++) if (g->objects[j].kind==BASE) base=j;
    for (int i=0;i<g->worker_count;i++) {
        Worker *w=&g->workers[i];Entity *e=&g->objects[w->entity];
        w->boat=e->type->boat_only;
        if (!w->boat) for (int j=0;j<g->object_count;j++)
            if (g->objects[j].kind==BASE && vec2_distance(w->home,g->objects[j].p)<CROWD_RADIUS) { w->home_base=j;break; }
        e->hidden=w->home_base>=0;
        if (!g->rules->auto_evacuation || w->boat) continue;
        /* Restored missions share one headquarters. Distant crew are stranded
         * recruits who join after reaching it. */
        if (base<0) return error_set("Mission has no worker base");
        w->home_base=base;w->home=g->objects[base].p;
        if (g->mission.crew_at_hq || vec2_distance(e->p,w->home)<CROWD_RADIUS) { e->p=w->home;e->hidden=true; }
        else { w->phase=STRANDED;w->recruiting=true;e->kind=RECRUIT; }
    }
    return true;
}

Game *game_world_create(LevelSpec *spec)
{
    Game *g=calloc(1,sizeof *g);
    if (!g) { error_set("Cannot allocate the world");level_spec_free(spec);return NULL; }
    g->spec=*spec;memset(spec,0,sizeof *spec);
    g->mission=g->spec.mission;g->rules=g->mission.rules;
    g->stock=g->mission.start;g->goal=g->mission.people_goal;g->basin_goal=g->mission.basin_goal;
    g->traffic=g->spec.traffic;g->traffic_count=g->spec.traffic_count;
    g->build_menu=-1;g->pointer_x=g->pointer_y=-100;g->routes_dirty=true;
    g->object_count=g->spec.placement_count;
    int n=g->object_count?g->object_count:1;
    g->objects=calloc((size_t)n,sizeof *g->objects);
    g->workers=calloc((size_t)n,sizeof *g->workers);
    g->evacuees=calloc((size_t)n,sizeof *g->evacuees);
    g->vehicles=calloc((size_t)n,sizeof *g->vehicles);
    bool ok=g->objects && g->workers && g->evacuees && g->vehicles;
    if (!ok) error_set("Cannot allocate %d objects",n);
    if (ok && !(g->lot=archetype_find("lot"))) ok=error_set("content/archetypes.xml lacks the \"lot\" archetype");
    if (ok) ok=build_links(g);
    for (int i=0;i<g->object_count && ok;i++) ok=spawn(g,i);
    for (int i=0;i<g->object_count && ok;i++) {
        Entity *e=&g->objects[i];const Placement *p=&g->spec.placements[i];
        if (e->kind==WORKER) ok=add_worker(g,g->workers,&g->worker_count,i);
        else if (e->kind==VEHICLE) {
            if (p->service<0 || !p->has_home) { ok=error_set("Vehicle %u has no home service",e->id);break; }
            if ((ok=add_worker(g,g->vehicles,&g->vehicle_count,i))) {
                Worker *v=&g->vehicles[g->vehicle_count-1];
                v->boat=e->type->boat_only;v->atv=!v->boat;v->home_base=p->service;v->home=p->home;
                e->p=v->home;e->hidden=!g->objects[v->home_base].operational;
            }
        }
    }
    if (ok) ok=place_crew(g);
    for (int i=0;i<g->object_count && ok;i++) {
        Entity *e=&g->objects[i];const Placement *p=&g->spec.placements[i];
        if (e->kind==SURVIVOR && g->rules->auto_evacuation) {
            if ((ok=add_worker(g,g->evacuees,&g->evacuee_count,i))) {
                Worker *v=&g->evacuees[g->evacuee_count-1];v->phase=STRANDED;v->home=g->workers[0].home;
            }
        }
        if (e->kind==RESOURCE && p->producer>=0 && g->rules->economy) {
            Entity *owner=&g->objects[p->producer];
            e->owner=p->producer;owner->output=i;e->hidden=!owner->operational;
        }
    }
    if (!ok) { char reason[sizeof last_error];memcpy(reason,last_error,sizeof reason);game_world_free(g);error_set("%s",reason);return NULL; }
    g->ready=true;
    game_announce(g,g->spec.briefing[0]?g->spec.briefing:g->rules->auto_evacuation?"Collect supplies and clear routes. People return to headquarters when the path is safe.":
        "Click debris, supplies or survivors to dispatch a worker.");
    return g;
}

void game_world_free(Game *g)
{
    if (!g) return;
    Worker *lists[]={g->workers,g->evacuees,g->vehicles};int counts[]={g->worker_count,g->evacuee_count,g->vehicle_count};
    for (int l=0;l<3;l++) if (lists[l]) for (int i=0;i<counts[l];i++) free(lists[l][i].path);
    free(g->workers);free(g->evacuees);free(g->vehicles);free(g->objects);free(g->links);free(g->link_start);
    level_spec_free(&g->spec);free(g);
}
