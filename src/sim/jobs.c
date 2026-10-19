/* Job scheduling. The scheduler owns reservation, exact-once spending,
 * movement and return; completion callbacks own each job's effect on the
 * world. Durations, work actions and prices come from the target archetype. */
#include "world.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void required_detail(const Game *g,const Entity *e,char *text,size_t size)
{
    Stock c=game_job_cost(g,e);char parts[RESOURCE_COUNT][32];size_t n=0;
    for (int r=0;r<RESOURCE_COUNT;r++) {
        parts[r][0]=0;
        if (c.amount[r] && r!=RESOURCE_FUEL) snprintf(parts[r],sizeof parts[r],"%s%d %s",n++?", ":"",c.amount[r],resource_names[r]);
    }
    snprintf(text,size,"Required: %s%s%s",n?parts[0]:"none",parts[1],parts[2]);
}

void game_waiting_detail(Game *g,int target,char *text,size_t size)
{
    if (!game_affordable(g,game_job_cost(g,&g->objects[target]))) required_detail(g,&g->objects[target],text,size);
    else snprintf(text,size,"%s",game_reachable(g,target)?"Waiting for a free worker":"Waiting for an open route");
}

static bool dispatch_vehicle(Game *g,int target,bool quiet)
{
    Entity *t=&g->objects[target];
    if (!game_gate_open(g,t) || game_fogged(g,t)) return false;
    int chosen=-1,count=0,best_count=0;float length,shortest=INFINITY;
    Vec2 *path=malloc((size_t)game_path_capacity(g)*sizeof *path);
    if (!path) return false;
    for (int i=0;i<g->vehicle_count;i++) {
        Worker *v=&g->vehicles[i];Entity *base=&g->objects[v->home_base];
        if (v->phase!=IDLE || !base->operational || !game_gate_open(g,base)) continue;
        if (game_vehicle_route(g,v,target,path,&count,&length) && length<shortest) {
            chosen=i;shortest=length;best_count=count;memcpy(v->path,path,(size_t)count*sizeof *path);
        }
    }
    free(path);
    if (chosen<0 || g->stock.fuel<10) {
        if (!quiet) game_announce(g,g->stock.fuel<10?"Not enough fuel: 10 required.":"Repair the garage or pier and clear its route first.");
        return false;
    }
    Worker *v=&g->vehicles[chosen];g->stock.fuel-=10;v->target=target;v->phase=OUTBOUND;v->step=1;v->count=best_count;
    g->objects[v->entity].hidden=false;t->assigned=true;t->queued=false;g->transport_trips++;
    if (!quiet) game_announce(g,v->atv?"ATV dispatched to rescue a worker.":"Boat dispatched to fish.");
    return true;
}

static Stock dispatch_cost(const Game *g,const Entity *t)
{
    if (t->kind==BUILD_LOT && t->building) return t->building->build_cost;
    return game_job_cost(g,t);
}

bool game_dispatch(Game *g,int target,bool quiet)
{
    if (target<0 || target>=g->object_count) return false;
    Entity *t=&g->objects[target];const Ruleset *rules=g->rules;
    if (g->paused || g->won || t->hidden || (t->done && t->kind!=BUILDING && t->kind!=BUILD_LOT) || t->assigned) return false;
    if (rules->vehicles && t->transport!=TRANSPORT_FOOT) return dispatch_vehicle(g,target,quiet);
    if (rules->gates && (t->type->effect==EFFECT_FOG || !game_gate_open(g,t) || game_fogged(g,t))) {
        if (!quiet) game_announce(g,"Turn the linked valve to clear the fog or purify the water first.");
        return false;
    }
    if (rules->auto_evacuation && (t->kind==SURVIVOR || t->kind==RECRUIT)) return false;
    if (rules->economy && t->kind==RESOURCE && t->owner>=0 && g->objects[t->owner].assigned) return false;
    if (!game_job_definition(t->kind)->complete) return false;
    if (t->kind==BUILD_LOT && !t->building) return false;
    if (rules->economy && t->type->needs_dynamite) {
        if (!quiet) game_announce(g,"Dynamite is required for this obstruction.");
        return false;
    }
    Stock cost=dispatch_cost(g,t);
    if (!game_affordable(g,cost)) {
        if (!quiet) { char text[192];game_waiting_detail(g,target,text,sizeof text);game_announce(g,text); }
        return false;
    }
    int chosen=-1,best_count=0;float shortest=INFINITY;
    Vec2 *path=malloc((size_t)game_path_capacity(g)*sizeof *path);
    if (!path) return false;
    for (int i=0;i<g->worker_count;i++) {
        Worker *w=&g->workers[i];
        bool compatible=t->type->boat_only?w->boat:!w->boat;
        int count;float length;
        if (compatible && game_available(w) && game_route(g,w,target,path,&count,&length) && length<shortest) {
            shortest=length;chosen=i;best_count=count;memcpy(w->path,path,(size_t)count*sizeof *path);
        }
    }
    free(path);
    if (chosen<0) { if (!quiet) game_announce(g,"No free worker can reach this target. Clear the route first.");return false; }
    game_spend(g,cost);
    Worker *w=&g->workers[chosen];
    w->phase=OUTBOUND;w->target=target;w->step=1;w->count=best_count;
    t->assigned=true;t->queued=false;
    g->objects[w->entity].hidden=false;
    if (!quiet) game_announce(g,t->kind==BARRIER?"Worker dispatched to clear the route.":
        t->kind==SURVIVOR?"Worker dispatched to escort a survivor.":t->kind==BUILD_LOT?"Worker dispatched to build.":"Collecting supplies.");
    return true;
}

bool game_construct(Game *g,int lot,const Archetype *producer)
{
    Entity *e=&g->objects[lot];
    if (e->kind!=BUILD_LOT || e->assigned) return false;
    const Archetype *previous=e->building;e->building=producer;
    if (game_dispatch(g,lot,false)) return true;
    e->building=previous;return false;
}

bool game_demolish(Game *g,int building)
{
    Entity *e=&g->objects[building];
    if (game_output_busy(g,e)) { game_announce(g,"Wait for the supply delivery before demolishing.");return false; }
    e->action=DEMOLISH;
    if (game_dispatch(g,building,false)) return true;
    e->action=REPAIR;return false;
}

void game_return_home(Game *g,Worker *w)
{
    float length;
    if (!game_route_to(g,w,w->home,-1,w->path,&w->count,&length)) return;
    w->phase=RETURNING;w->step=1;
}

/* Completion callbacks */

static void clear_barrier(Game *g,Entity *t,Worker *w)
{
    (void)w;
    if (game_burning_log(t)) t->fire_stopped=g->time;
    g->cleared++;g->completed++;
    if (t->type->cleared_frame>=0) t->frame=t->type->cleared_frame;else t->hidden=true;
    if (g->rules->economy) for (int r=0;r<RESOURCE_COUNT;r++) g->stock.amount[r]+=t->type->reward.amount[r];
    g->routes_dirty=true;g->route_epoch++;
}

static void construct_building(Game *g,Entity *t,Worker *w)
{
    (void)g;(void)w;
    /* Rebuilding what the level placed restores its mission-specific variant. */
    t->type=archetype_is(t->placed,t->building)?t->placed:t->building;
    t->kind=BUILDING;t->operational=true;t->constructing=false;t->income_clock=t->bonus_clock=0;t->frame=0;
}

static void repair_or_demolish(Game *g,Entity *t,Worker *w)
{
    (void)w;
    t->income_clock=t->bonus_clock=0;
    if (t->action==DEMOLISH) {
        t->kind=BUILD_LOT;t->type=g->lot;t->operational=false;t->done=false;t->building=NULL;t->action=REPAIR;
        if (t->output>=0) { Entity *output=&g->objects[t->output];output->hidden=true;output->queued=false; }
    } else { t->operational=true;t->frame=0; }
}

static void repair_service(Game *g,Entity *t,Worker *w) { (void)g;(void)w;t->operational=true; }

static void open_valve(Game *g,Entity *t,Worker *w)
{
    t->frame=1;g->routes_dirty=true;g->route_epoch++;
    if (g->basin_goal) g->purified++;
    for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];
        if (e->gate==w->target && (e->type->effect==EFFECT_FOG || e->type->effect==EFFECT_POLLUTION)) e->hidden=e->done=true;
    }
}

static void collect_resource(Game *g,Entity *t,Worker *w)
{
    w->value=t->type->amount;w->cargo=t->type->resource;t->hidden=true;g->collected++;
}

static void escort_follower(Game *g,Entity *t,Worker *w) { t->follower=(int)(w-g->workers); }

const JobDef *game_job_definition(EntityKind kind)
{
    static const JobDef definitions[ENTITY_KIND_COUNT]={
        [BARRIER]={clear_barrier,false,JOB_CLICK},
        [RESOURCE]={collect_resource,true,JOB_CLICK},
        [SURVIVOR]={escort_follower,true,JOB_CLICK},
        [ANIMAL]={escort_follower,true,JOB_CLICK},
        [BUILD_LOT]={construct_building,false,0},
        [BUILDING]={repair_or_demolish,false,JOB_REPAIR},
        [SERVICE]={repair_service,false,JOB_REPAIR},
        [VALVE]={open_valve,false,JOB_CLICK},
        [RECRUIT]={NULL,false,JOB_ATV}
    };
    static const JobDef none={0};
    return kind>=0 && kind<ENTITY_KIND_COUNT?&definitions[kind]:&none;
}

void game_finish_work(Game *g,int worker)
{
    Worker *w=&g->workers[worker];Entity *t=&g->objects[w->target];
    const JobDef *definition=game_job_definition(t->kind);
    t->done=true;t->assigned=false;
    definition->complete(g,t,w);
    if (!definition->keep_target) w->target=-1;
    game_return_home(g,w);
}

void game_arrive_home(Game *g,Worker *w)
{
    Entity *e=&g->objects[w->entity];
    if (w->recruiting) { w->recruiting=false;e->kind=WORKER;g->rescued++; }
    if (w->target>=0) {
        Entity *t=&g->objects[w->target];
        if (t->kind==SURVIVOR || t->kind==ANIMAL) {
            t->hidden=true;t->follower=-1;
            if (t->kind==ANIMAL) g->animals_rescued++;else g->rescued++;
        }
        game_credit(g,w->cargo,w->value);
        g->completed++;
    }
    e->p=w->home;e->hidden=w->home_base>=0;w->phase=IDLE;w->target=-1;w->cargo=RESOURCE_NONE;w->value=0;
    e->pose=POSE_IDLE;
    if (!g->rules->goals && g->completed==g->target_count) game_announce(g,"Prototype level complete. All routes cleared and survivors escorted!");
}

bool game_output_busy(const Game *g,const Entity *building)
{
    if (building->output<0) return false;
    for (int i=0;i<g->worker_count;i++)
        if (g->workers[i].target==building->output && g->workers[i].phase!=IDLE) return true;
    return false;
}
