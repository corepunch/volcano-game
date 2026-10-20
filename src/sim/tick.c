/* Fixed-step simulation. Update order: workers, queued jobs, followers, then
 * (per ruleset) production, vehicles, evacuations, wildlife and victory. */
#include "world.h"

/* Timers compare against sums of fixed ticks, which can land a hair under a
 * whole-second boundary; this tolerance keeps authored intervals exact. */
#define TIMER_EPSILON 1e-6
#include <math.h>
#include <stdlib.h>

int game_direction(Vec2 from,Vec2 to,int directions)
{
    return (int)floor(atan2(to.y-from.y,to.x-from.x)/(2*PI)*directions+directions+.5)%directions;
}

/* Moves E along PATH by BUDGET units, updating step and facing. */
static float follow_path(Entity *e,const Vec2 *path,int *step,int count,float budget,int directions,int *direction)
{
    while (budget>0 && *step<count) {
        Vec2 p=path[*step];float d=vec2_distance(e->p,p);
        if (d>0) *direction=game_direction(e->p,p,directions);
        if (d<=budget) { e->p=p;(*step)++;budget-=d; }
        else { e->p.x+=(p.x-e->p.x)*budget/d;e->p.y+=(p.y-e->p.y)*budget/d;budget=0; }
    }
    return budget;
}

static void update_production(Game *g,double dt)
{
    for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];const Archetype *t=e->type;
        if (e->kind!=BUILDING || !e->operational || e->assigned) continue;
        e->income_clock+=dt;e->bonus_clock+=dt;
        while (t->income_interval>0 && e->income_clock>=t->income_interval-TIMER_EPSILON) {
            e->income_clock-=t->income_interval;game_credit(g,t->produces,t->income);
        }
        if (e->output<0 || !t->bonus) continue;
        Entity *output=&g->objects[e->output];
        if (e->bonus_clock>=t->bonus_interval-TIMER_EPSILON && output->hidden && !game_output_busy(g,e)) {
            /* The output always offers the current producer's bonus. */
            e->bonus_clock=0;output->done=output->hidden=output->assigned=output->queued=false;
            output->type=t->bonus;output->sprite=NULL;
        }
    }
}

static void update_rescues(Game *g,double dt)
{
    if (g->routes_dirty) {
        g->routes_dirty=false;
        for (int i=0;i<g->worker_count+g->evacuee_count;i++) {
            Worker *w=i<g->worker_count?&g->workers[i]:&g->evacuees[i-g->worker_count];
            Entity *e=&g->objects[w->entity];float length;
            if (w->phase==STRANDED && e->transport==TRANSPORT_FOOT && !game_fogged(g,e) &&
                game_route_to(g,w,w->home,-1,w->path,&w->count,&length)) { w->phase=RETURNING;w->step=1; }
        }
    }
    for (int i=0;i<g->evacuee_count;i++) {
        Worker *w=&g->evacuees[i];if (w->phase!=RETURNING) continue;
        Entity *e=&g->objects[w->entity];
        e->pose=POSE_MOVE;
        follow_path(e,w->path,&w->step,w->count,(float)dt*e->type->speed,e->type->directions,&e->direction);
        if (w->step==w->count) { e->done=e->hidden=true;w->phase=IDLE;g->rescued++; }
    }
}

static void update_vehicles(Game *g,double dt)
{
    for (int i=0;i<g->vehicle_count;i++) {
        Worker *v=&g->vehicles[i];Entity *e=&g->objects[v->entity],*base=&g->objects[v->home_base];
        if (v->phase==IDLE) { e->hidden=!base->operational || !game_gate_open(g,base);continue; }
        if (v->phase==WORKING) {
            v->work+=dt;
            if (v->work<1-TIMER_EPSILON) continue;
            Entity *t=&g->objects[v->target];t->hidden=true;
            if (v->boat) { v->cargo=t->type->resource;v->value=t->type->amount;t->done=true; }
            if (v->atv) {
                for (int j=0;j<v->count/2;j++) { Vec2 p=v->path[j];v->path[j]=v->path[v->count-1-j];v->path[v->count-1-j]=p; }
                v->phase=RETURNING;v->step=1;
            } else game_return_home(g,v);
        }
        if (v->phase!=OUTBOUND && v->phase!=RETURNING) continue;
        e->pose=POSE_MOVE;
        follow_path(e,v->path,&v->step,v->count,(float)dt*e->type->speed,e->type->directions,&v->direction);
        e->direction=v->direction;
        if (v->step!=v->count) continue;
        if (v->phase==OUTBOUND) {
            v->phase=WORKING;v->work=0;
            if (v->boat) e->pose=POSE_ACT;
        } else {
            Entity *t=&g->objects[v->target];t->assigned=false;
            if (v->boat) { game_credit(g,v->cargo,v->value);g->fishing_trips++;t->bonus_clock=0; }
            else { t->transport=TRANSPORT_FOOT;t->hidden=false;t->p=base->workpoint;g->routes_dirty=true; }
            v->phase=IDLE;v->target=-1;v->cargo=RESOURCE_NONE;v->value=0;e->p=v->home;
            e->pose=POSE_IDLE;
        }
    }
    for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];
        if (e->kind==RESOURCE && e->transport==TRANSPORT_BOAT && e->done && !e->assigned && game_gate_open(g,e)) {
            e->bonus_clock+=dt;
            if (e->bonus_clock>=10-TIMER_EPSILON) { e->done=e->hidden=false;e->bonus_clock=0; }
        }
    }
}

static void update_animals(Game *g,double dt)
{
    for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];
        if (e->kind!=ANIMAL || e->done || e->assigned || e->hidden || game_fogged(g,e)) continue;
        if (!e->roaming) {
            /* Animals start roaming once any free crew member could reach them. */
            if (e->checked_route==g->route_epoch) continue;
            e->checked_route=g->route_epoch;
            int count;float length;bool reachable=false;
            Vec2 *path=malloc((size_t)game_path_capacity(g)*sizeof *path);
            for (int j=0;path && j<g->worker_count && !reachable;j++) {
                const Worker *w=&g->workers[j];
                if (!w->recruiting && w->phase!=STRANDED) reachable=game_route(g,w,i,path,&count,&length);
            }
            free(path);
            if (!reachable) continue;
            e->roaming=true;e->roam=g->traffic[game_closest(g,e->p)];
        }
        float d=vec2_distance(e->p,e->roam),budget=(float)dt*e->type->speed;
        if (d<=budget) {
            /* Wander to a neighbouring point near home, never through obstacles or fog. */
            e->p=e->roam;int node=game_closest(g,e->p),count=0;
            for (int pass=0;pass<2;pass++) {
                int pick=pass?(int)((e->id+(uint32_t)e->roam_step*17u)%(uint32_t)count):-1,seen=0;
                for (int k=g->link_start[node];k<g->link_start[node+1];k++) {
                    Vec2 p=g->traffic[g->links[k]];Entity check=*e;check.p=p;
                    if (vec2_distance(e->p,p)<=1 || vec2_distance(p,e->origin)>=130 || game_blocked(g,e->p,p,-1) || game_fogged(g,&check)) continue;
                    if (pass && seen==pick) { e->roam=p;e->roam_step++;break; }
                    seen++;
                }
                if (!pass) count=seen;
                if (!count) break;
            }
        } else { e->p.x+=(e->roam.x-e->p.x)*budget/d;e->p.y+=(e->roam.y-e->p.y)*budget/d; }
        e->pose=POSE_MOVE;e->direction=game_direction(e->p,e->roam,8);
    }
}

static void worker_travel(Game *g,Worker *w,double dt)
{
    Entity *e=&g->objects[w->entity];
    e->pose=w->phase==RETURNING && w->cargo!=RESOURCE_NONE?POSE_CARRY:POSE_MOVE;
    follow_path(e,w->path,&w->step,w->count,(float)(dt*e->type->speed),e->type->directions,&w->direction);
    e->direction=w->direction;
    if (w->step!=w->count) return;
    if (w->phase==RETURNING) { game_arrive_home(g,w);return; }
    Entity *t=&g->objects[w->target];
    w->phase=WORKING;w->work=0;
    if (t->kind==RESOURCE) { game_finish_work(g,(int)(w-g->workers));return; }
    if (t->kind==BUILD_LOT) t->constructing=true;
    if (!w->boat) {
        /* Face the target from the edge of its footprint. */
        if (vec2_distance(e->p,t->p)>1) w->direction=e->direction=game_direction(e->p,t->p,e->type->directions);
        e->pose=POSE_WORK;e->pose_action=t->type->action;
    }
}

static void worker_work(Game *g,Worker *w,double dt)
{
    w->work+=dt;
    if (w->work<g->objects[w->target].type->duration-TIMER_EPSILON) return;
    game_finish_work(g,(int)(w-g->workers));
    /* Timed work starts its return movement in this same update. Pickups in
     * worker_travel still return on the next update. */
    if (w->phase==RETURNING) worker_travel(g,w,dt);
}

void game_tick(Game *g,double dt)
{
    if (!g || !g->ready || g->paused || g->won) return;
    dt=fmax(0,fmin(dt,0.1));g->time+=dt;g->ticks++;g->message_time=fmax(0,g->message_time-dt);
    for (int i=0;i<g->worker_count;i++) {
        Worker *w=&g->workers[i];
        if (w->phase==OUTBOUND || w->phase==RETURNING) worker_travel(g,w,dt);
        else if (w->phase==WORKING) worker_work(g,w,dt);
    }
    int last=0;
    for (int n=0;n<g->object_count;n++) {
        int next=-1,order=2147483647;
        for (int i=0;i<g->object_count;i++)
            if (g->objects[i].queued && g->objects[i].queue_order>last && g->objects[i].queue_order<order) next=i,order=g->objects[i].queue_order;
        if (next<0) break;
        game_dispatch(g,next,true);last=order;
    }
    for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];
        if (e->follower<0) continue;
        Worker *w=&g->workers[e->follower];Vec2 p=g->objects[w->entity].p;float angle=w->direction*2*(float)PI/8;
        e->p=vec2(p.x-cosf(angle)*17,p.y-sinf(angle)*17);
        e->pose=POSE_RUN;e->direction=w->direction;
    }
    if (g->rules->economy) update_production(g,dt);
    if (g->rules->vehicles) update_vehicles(g,dt);
    if (g->rules->auto_evacuation) update_rescues(g,dt);
    if (g->rules->wildlife) update_animals(g,dt);
    if (g->rules->goals && g->rescued>=g->goal && g->animals_rescued>=g->animals && g->purified>=g->basin_goal) {
        g->won=true;g->build_menu=-1;game_announce(g,"Mission complete!");
    }
}

void game_advance(Game *g,double seconds)
{
    if (!g) return;
    g->accumulator+=fmax(0,fmin(seconds,0.1));
    while (g->accumulator>=TICK_SECONDS-1e-9) { g->accumulator-=TICK_SECONDS;game_tick(g,TICK_SECONDS); }
}
