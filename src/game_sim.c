#include "game_internal.h"
#include <math.h>
#include <stdio.h>


static void game_update_production(Game *g, double dt)
{
    for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];
        if (e->kind!=BUILDING || !e->operational || e->assigned) continue;
        e->income_clock+=dt;e->bonus_clock+=dt;
        while (e->income_clock>=PRODUCTION_INTERVAL) {
            e->income_clock-=PRODUCTION_INTERVAL;game_credit(g, game_building_resource(e->building),PRODUCTION_INCOME);
        }
        if (e->output<0) continue;
        Entity *output=&g->objects[e->output];
        if (e->bonus_clock>=10 && output->hidden && !game_output_busy(g, e)) {
            e->bonus_clock=0;output->done=output->hidden=output->assigned=output->queued=false;
            snprintf(output->anim,sizeof output->anim,"%s",game_buildings[e->building].bonus);
            game_set_pickup(output,game_buildings[e->building].bonus,"");
        }
    }
}

static void game_update_rescues(Game *g, double dt)
{
    if (g->routes_dirty) {
        g->routes_dirty=false;
        for (int i=0;i<g->worker_count+g->evacuee_count;i++) {
            Worker *w=i<g->worker_count?&g->workers[i]:&g->evacuees[i-g->worker_count];
            float length;
            Entity *e=&g->objects[w->entity];
            if (w->phase==STRANDED && e->transport==ON_FOOT && !game_fogged(g, e) && game_route_to(g, w,w->home,-1,w->path,&w->count,&length)) {
                w->phase=RETURNING;w->step=1;
            }
        }
    }
    for (int i=0;i<g->evacuee_count;i++) {
        Worker *w=&g->evacuees[i];if (w->phase!=RETURNING) continue;
        Entity *e=&g->objects[w->entity];float budget=(float)dt*175;
        while (budget>0 && w->step<w->count) {
            Vec2 p=w->path[w->step];float d=game_distance(e->p,p);
            int direction=(int)floor(atan2(p.y-e->p.y,p.x-e->p.x)/(2*PI)*8+8.5)%8;
            snprintf(e->anim,sizeof e->anim,"man_run%d",direction);
            if (d<=budget) {e->p=p;w->step++;budget-=d;}
            else {e->p.x+=(p.x-e->p.x)*budget/d;e->p.y+=(p.y-e->p.y)*budget/d;budget=0;}
        }
        if (w->step==w->count) {e->done=e->hidden=true;w->phase=IDLE;g->rescued++;}
    }
}

static void game_update_vehicles(Game *g, double dt)
{
    for (int i=0;i<g->vehicle_count;i++) {
        Worker *v=&g->vehicles[i];Entity *e=&g->objects[v->entity],*base=&g->objects[v->home_base];
        if (v->phase==IDLE) {e->hidden=!base->operational || !game_gate_open(g, base);continue;}
        if (v->phase==WORKING) {
            v->work+=dt;
            if (v->work<1) continue;
            Entity *t=&g->objects[v->target];t->hidden=true;
            if (v->boat) {
                v->cargo=t->pickup_resource;v->value=t->pickup_value;
                t->done=true;
            }
            if (v->atv) {
                for (int j=0;j<v->count/2;j++) {Vec2 p=v->path[j];v->path[j]=v->path[v->count-1-j];v->path[v->count-1-j]=p;}
                v->phase=RETURNING;v->step=1;
            } else game_return_home(g, v);
        }
        if (v->phase!=OUTBOUND && v->phase!=RETURNING) continue;
        float budget=(float)dt*(v->atv?220:150);
        while (budget>0 && v->step<v->count) {
            Vec2 p=v->path[v->step];float d=game_distance(e->p,p);int directions=v->atv?16:24;
            v->direction=(int)floor(atan2(p.y-e->p.y,p.x-e->p.x)/(2*PI)*directions+directions+.5)%directions;
            snprintf(e->anim,sizeof e->anim,"%s%d",v->atv?"offroad":"boat",v->direction);
            if (d<=budget) {e->p=p;v->step++;budget-=d;}
            else {e->p.x+=(p.x-e->p.x)*budget/d;e->p.y+=(p.y-e->p.y)*budget/d;budget=0;}
        }
        if (v->step!=v->count) continue;
        if (v->phase==OUTBOUND) {
            v->phase=WORKING;v->work=0;
            if (v->boat) snprintf(e->anim,sizeof e->anim,"boat_fish_action0");
        } else {
            Entity *t=&g->objects[v->target];t->assigned=false;
            if (v->boat) {game_credit(g, v->cargo,v->value);g->fishing_trips++;t->bonus_clock=0;}
            else {t->transport=ON_FOOT;t->hidden=false;t->p=base->workpoint;g->routes_dirty=true;}
            v->phase=IDLE;v->target=-1;v->cargo=v->value=0;e->p=v->home;
            snprintf(e->anim,sizeof e->anim,"%s0",v->atv?"offroad":"boat");
        }
    }
    for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];
        if (e->kind==RESOURCE && e->transport==BY_BOAT && e->done && !e->assigned && game_gate_open(g, e)) {
            e->bonus_clock+=dt;
            if (e->bonus_clock>=10) {e->done=e->hidden=false;e->bonus_clock=0;}
        }
    }
}

static void game_update_animals(Game *g, double dt)
{
    for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];
        if (e->kind!=ANIMAL || e->done || e->assigned || e->hidden || game_fogged(g, e)) continue;
        if (!e->roaming) {
            if (e->checked_route==g->route_epoch) continue;
            e->checked_route=g->route_epoch;
            Vec2 path[MAX_PATH];int count;float length;bool reachable=false;
            for (int j=0;j<g->worker_count && !reachable;j++) {
                Worker *w=&g->workers[j];
                if (!w->recruiting && w->phase!=STRANDED && game_route(g, w,i,path,&count,&length)) reachable=true;
            }
            if (!reachable) continue;
            e->roaming=true;e->roam=g->level.traffic[game_closest(g, e->p)];
        }
        float d=game_distance(e->p,e->roam),budget=(float)dt*28;
        if (d<=budget) {
            e->p=e->roam;int candidates[MAX_TRAFFIC_POINTS],count=0;
            for (int n=0;n<g->level.traffic_count;n++) {
                Vec2 p=g->level.traffic[n];float gap=game_distance(e->p,p);
                Entity check=*e;check.p=p;
                if (gap>1 && gap<50 && game_distance(p,e->origin)<130 && !game_blocked(g, e->p,p,-1) && !game_fogged(g, &check)) candidates[count++]=n;
            }
            if (count) e->roam=g->level.traffic[candidates[(e->seed+(uint32_t)e->roam_step++*17u)%(uint32_t)count]];
        } else {e->p.x+=(e->roam.x-e->p.x)*budget/d;e->p.y+=(e->roam.y-e->p.y)*budget/d;}
        int direction=(int)floor(atan2(e->roam.y-e->p.y,e->roam.x-e->p.x)/(2*PI)*8+8.5)%8;
        snprintf(e->anim,sizeof e->anim,"%s%u_walk%d",g->level.objects[e->source].params,e->seed%3,direction);
    }
}

static void worker_travel(Game *g,Worker *w,double dt)
{
    Entity *e=&g->objects[w->entity];
    float budget=dt*(w->boat?150:175);
    while (budget>0 && w->step<w->count) {
        Vec2 p=w->path[w->step];float d=game_distance(e->p,p);int directions=w->boat?24:8;
        w->direction=(int)floor(atan2(p.y-e->p.y,p.x-e->p.x)/(2*PI)*directions+0.5+directions)%directions;
        const char *prefix=w->boat?"boat":w->phase==RETURNING && w->cargo?"worker_bag":"worker_run";
        snprintf(e->anim,sizeof(e->anim),"%s%d",prefix,w->direction);
        if (d<=budget) e->p=p,w->step++,budget-=d;
        else { e->p.x+=(p.x-e->p.x)*budget/d;e->p.y+=(p.y-e->p.y)*budget/d;budget=0; }
    }
    if (w->step==w->count) {
        if (w->phase==RETURNING) game_arrive_home(g, w);
        else {
            w->phase=WORKING;w->work=0;
            if (g->objects[w->target].kind==RESOURCE) { game_finish_work(g,(int)(w-g->workers));return; }
            if (g->objects[w->target].kind==BUILD_LOT) {
                Entity *t=&g->objects[w->target];snprintf(t->anim,sizeof t->anim,"%s_br",game_buildings[t->building].animation);t->frame=0;
            }
            if (!w->boat) snprintf(e->anim,sizeof(e->anim),"%s%d",game_work_animation(&g->objects[w->target]),w->direction);
        }
    }
}

static void worker_work(Game *g,Worker *w,double dt)
{
    w->work+=dt;
    if (w->work<g->objects[w->target].duration) return;
    game_finish_work(g,(int)(w-g->workers));
    /* Timed work starts its return movement in this same update. Pickups in
     * worker_travel still return on the next update, as before. */
    if (w->phase==RETURNING) worker_travel(g,w,dt);
}

typedef void (*WorkerThink)(Game *g,Worker *worker,double dt);
static const WorkerThink worker_states[]={
    [IDLE]=NULL,[OUTBOUND]=worker_travel,[WORKING]=worker_work,[RETURNING]=worker_travel,[STRANDED]=NULL
};

void game_tick(Game *g, double dt)
{
    if (!g || !g->ready || g->paused || g->won) return;
    dt=fmax(0,fmin(dt,0.1));g->time+=dt;g->message_time=fmax(0,g->message_time-dt);
    for (int i=0;i<g->worker_count;i++) {
        Worker *worker=&g->workers[i];
        WorkerThink think=worker_states[worker->phase];
        if (think) think(g,worker,dt);
    }
    int last=0;
    for (int n=0;n<g->object_count;n++) {
        int next=-1,order=2147483647;
        for (int i=0;i<g->object_count;i++) if (g->objects[i].queued && g->objects[i].queue_order>last && g->objects[i].queue_order<order)
            next=i,order=g->objects[i].queue_order;
        if (next<0) break;
        game_dispatch(g, next,true);last=order;
    }
    for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];
        if (e->follower>=0) {
            Worker *w=&g->workers[e->follower];Vec2 p=g->objects[w->entity].p;float angle=w->direction*2*PI/8;
            e->p=(Vec2){p.x-cosf(angle)*17,p.y-sinf(angle)*17};
            if (e->kind==ANIMAL) snprintf(e->anim,sizeof e->anim,"%s%u_run%d",g->level.objects[e->source].params,e->seed%3,w->direction);
            else snprintf(e->anim,sizeof(e->anim),"man_run%d",w->direction);
        }
    }
    if (g->early) {
        game_update_production(g, dt);game_update_vehicles(g, dt);game_update_rescues(g, dt);game_update_animals(g, dt);
        if (g->rescued>=g->goal && g->animals_rescued>=g->animals && g->purified>=g->basin_goal) {
            g->won=true;g->build_menu=-1;game_announce(g, "Mission complete!");
        }
    }
}
