#include "game_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>


static void game_required_detail(Game *g, const Entity *e,char *text,size_t size)
{
    Cost c=game_job_cost(g, e);char food[24]="",wood[24]="",money[24]="";
    if (c.food) snprintf(food,sizeof food,"%d food",c.food);
    if (c.wood) snprintf(wood,sizeof wood,"%s%d wood",c.food?", ":"",c.wood);
    if (c.money) snprintf(money,sizeof money,"%s%d money",c.food || c.wood?", ":"",c.money);
    snprintf(text,size,"Required: %s%s%s",c.food || c.wood || c.money?food:"none",wood,money);
}

void game_waiting_detail(Game *g, int target,char *text,size_t size)
{
    if (!game_affordable(g, game_job_cost(g, &g->objects[target]))) game_required_detail(g, &g->objects[target],text,size);
    else snprintf(text,size,"%s",game_reachable(g, target)?"Waiting for a free worker":"Waiting for an open route");
}

static bool game_dispatch_vehicle(Game *g, int target,bool quiet)
{
    Entity *t=&g->objects[target];
    if (!game_gate_open(g, t) || game_fogged(g, t)) return false;
    int chosen=-1,count=0,best_count=0;float length,shortest=INFINITY;
    Vec2 path[MAX_PATH],best[MAX_PATH];
    for (int i=0;i<g->vehicle_count;i++) {
        Worker *v=&g->vehicles[i];Entity *base=&g->objects[v->home_base];
        if (v->phase!=IDLE || !base->operational || !game_gate_open(g, base)) continue;
        if (game_vehicle_route(g, v,target,path,&count,&length) && length<shortest) {
            chosen=i;shortest=length;best_count=count;memcpy(best,path,(size_t)count*sizeof *path);
        }
    }
    if (chosen<0 || g->gas<10) {
        if (!quiet) game_announce(g, g->gas<10?"Not enough fuel: 10 required.":"Repair the garage or pier and clear its route first.");
        return false;
    }
    Worker *v=&g->vehicles[chosen];g->gas-=10;v->target=target;v->phase=OUTBOUND;v->step=1;v->count=best_count;
    memcpy(v->path,best,(size_t)best_count*sizeof *best);g->objects[v->entity].hidden=false;
    t->assigned=true;t->queued=false;g->transport_trips++;
    if (!quiet) game_announce(g, v->atv?"ATV dispatched to rescue a worker.":"Boat dispatched to fish.");
    return true;
}

bool game_dispatch(Game *g, int target,bool quiet)
{
    if (target<0 || target>=g->object_count) return false;
    Entity *t=&g->objects[target];
    if (g->paused || g->won || t->hidden || (t->done && t->kind!=BUILDING && t->kind!=BUILD_LOT) || t->assigned) return false;
    if (g->early && (t->transport==BY_ATV || t->transport==BY_BOAT)) return game_dispatch_vehicle(g, target,quiet);
    if (g->early && (t->effect==1 || !game_gate_open(g, t) || game_fogged(g, t))) {
        if (!quiet) game_announce(g, "Turn the linked valve to clear the fog or purify the water first.");
        return false;
    }
    if (g->early && (t->kind==SURVIVOR || t->kind==RECRUIT)) return false;
    if (g->early && t->kind==RESOURCE && t->owner>=0 && g->objects[t->owner].assigned) return false;
    if (!game_job_definition(t->kind)->complete) return false;
    Cost cost=t->kind==BUILD_LOT?(Cost){.wood=game_buildings[t->building].cost}:game_job_cost(g,t);
    if (g->early && t->obstacle && t->obstacle->needs_dynamite) {
        if (!quiet) game_announce(g, "Dynamite is required for this obstruction.");return false;
    }
    if (!game_affordable(g, cost)) {
        if (!quiet) { char text[192];game_waiting_detail(g, target,text,sizeof text);game_announce(g, text); }
        return false;
    }
    int chosen=-1,best_count=0;float shortest=INFINITY;
    Vec2 path[MAX_PATH],best[MAX_PATH];
    for (int i=0;i<g->worker_count;i++) {
        Worker *w=&g->workers[i];
        bool compatible=t->boat_worker?w->boat:!w->boat;
        int count;float length;
        if (compatible && game_available(w) && game_route(g, w,target,path,&count,&length) && length<shortest) {
            shortest=length;chosen=i;best_count=count;memcpy(best,path,count*sizeof(Vec2));
        }
    }
    if (chosen<0) { if (!quiet) game_announce(g, "No free worker can reach this target. Clear the route first.");return false; }
    game_spend(g, cost);
    Worker *w=&g->workers[chosen];
    w->phase=OUTBOUND;w->target=target;w->step=1;w->count=best_count;
    memcpy(w->path,best,best_count*sizeof(Vec2));
    t->assigned=true;t->queued=false;
    g->objects[w->entity].hidden=false;
    if (!quiet) game_announce(g, t->kind==BARRIER?"Worker dispatched to clear the route.":
        t->kind==SURVIVOR?"Worker dispatched to escort a survivor.":t->kind==BUILD_LOT?"Worker dispatched to build.":"Collecting supplies.");
    return true;
}

void game_return_home(Game *g, Worker *w)
{
    float length;
    if (!game_route_to(g, w,w->home,-1,w->path,&w->count,&length)) return;
    w->phase=RETURNING;w->step=1;
}

static void clear_barrier(Game *g,Entity *t,Worker *w)
{
    (void)w;
    if (game_burning_log(t)) t->fire_stopped=g->time;
    g->cleared++;g->completed++;
    if (t->obstacle->clear_frame>=0) t->frame=t->obstacle->clear_frame;else t->hidden=true;
    if (g->early) g->wood+=t->obstacle->wood_reward;
    g->routes_dirty=true;g->route_epoch++;
}

static void construct_building(Game *g,Entity *t,Worker *w)
{
    (void)g;(void)w;
    t->kind=BUILDING;t->operational=true;t->income_clock=t->bonus_clock=0;game_building_art(t);
}

static void repair_or_demolish(Game *g,Entity *t,Worker *w)
{
    (void)w;
    t->income_clock=t->bonus_clock=0;
    if (t->action==DEMOLISH) {
        t->kind=BUILD_LOT;t->operational=false;t->done=false;
        snprintf(t->anim,sizeof t->anim,"build_place");
        if (t->output>=0) { Entity *output=&g->objects[t->output];output->hidden=true;output->queued=false; }
    } else { t->operational=true;game_building_art(t); }
}

static void repair_service(Game *g,Entity *t,Worker *w)
{
    (void)w;
    t->operational=true;snprintf(t->anim,sizeof t->anim,"%s",g->level.objects[t->source].type==114?"seaport":"garage0");
}

static void open_valve(Game *g,Entity *t,Worker *w)
{
    t->frame=1;g->routes_dirty=true;g->route_epoch++;if (g->basin_goal) g->purified++;
    for (int i=0;i<g->object_count;i++) if (g->objects[i].gate==w->target &&
        (g->objects[i].effect==1 || g->objects[i].effect==3)) {
        g->objects[i].hidden=g->objects[i].done=true;
    }
}

static void collect_resource(Game *g,Entity *t,Worker *w)
{
    w->value=t->pickup_value;w->cargo=t->pickup_resource;t->hidden=true;g->collected++;
}

static void escort_follower(Game *g,Entity *t,Worker *w)
{
    t->follower=(int)(w-g->workers);
}

/* The scheduler owns reservation, movement and return. Callbacks own the effect
 * of a completed job. Keep a target only while cargo or a follower needs it. */
const JobDef *game_job_definition(EntityKind kind)
{
    static const JobDef definitions[ENTITY_KIND_COUNT]={
        [BARRIER]={5,"worker_srepair",false,clear_barrier,{0},JOB_CLICK},
        [RESOURCE]={0,"worker_gather",true,collect_resource,{0},JOB_CLICK},
        [SURVIVOR]={.8,"worker_gather",true,escort_follower,{0},JOB_CLICK},
        [ANIMAL]={1.2,"worker_gather",true,escort_follower,{.food=10},JOB_CLICK},
        [BUILD_LOT]={5,"worker_srepair",false,construct_building,{0},0},
        [BUILDING]={5,"worker_srepair",false,repair_or_demolish,{0},JOB_REPAIR},
        [SERVICE]={5,"worker_gather",false,repair_service,{.wood=40},JOB_REPAIR},
        [VALVE]={5,"worker_gather",false,open_valve,{.money=20},JOB_CLICK},
        [RECRUIT]={.flags=JOB_ATV}
    };
    static const JobDef none={0};
    return kind>=0 && kind<ENTITY_KIND_COUNT?&definitions[kind]:&none;
}

const char *game_work_animation(const Entity *target)
{
    /* Choose presentation from resolved job data without changing its duration,
     * resource reservation or completion callback. Both art sets have these. */
    if (target->kind==BARRIER && target->obstacle) {
        if (strstr(target->obstacle->match,"tree")) return "worker_wood";
        if (strstr(target->obstacle->match,"stone")) return "worker_pick";
    }
    if (target->kind==VALVE) return "worker_tap";
    if (target->kind==ANIMAL) return "worker_pet";
    if (target->kind==SURVIVOR) return "worker_call";
    if (target->kind==BUILDING || target->kind==SERVICE) return "worker_repair";
    return game_job_definition(target->kind)->work_animation;
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

void game_arrive_home(Game *g, Worker *w)
{
    Entity *e=&g->objects[w->entity];
    if (w->recruiting) { w->recruiting=false;e->kind=WORKER;g->rescued++; }
    if (w->target>=0) {
        Entity *t=&g->objects[w->target];
        if (t->kind==SURVIVOR || t->kind==ANIMAL) {
            t->hidden=true;t->follower=-1;
            if (t->kind==ANIMAL) g->animals_rescued++;else g->rescued++;
        }
        game_credit(g, w->cargo,w->value);
        g->completed++;
    }
    e->p=w->home;e->hidden=w->home_base>=0;w->phase=IDLE;w->target=-1;w->cargo=w->value=0;
    snprintf(e->anim,sizeof(e->anim),"%s",w->boat?"boat0":"worker_wait0");
    if (!g->early && g->completed==g->target_count) game_announce(g, "Prototype level complete. All routes cleared and survivors escorted!");
}

bool game_output_busy(Game *g, const Entity *building)
{
    if (building->output<0) return false;
    for (int i=0;i<g->worker_count;i++)
        if (g->workers[i].target==building->output && g->workers[i].phase!=IDLE) return true;
    return false;
}
