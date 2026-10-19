/* Routes over the authored traffic graph. Points strictly closer than
 * LINK_DISTANCE connect; live obstacles block segments within BARRIER_RADIUS
 * of their centre, except the job's own target. */
#include "world.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

int game_path_capacity(const Game *g) { return g->traffic_count+3; }

int game_closest(const Game *g,Vec2 p)
{
    int best=-1;float value=INFINITY;
    for (int i=0;i<g->traffic_count;i++) {
        float d=vec2_distance(p,g->traffic[i]);
        if (d<value) best=i,value=d;
    }
    return best;
}

static float segment_distance(Vec2 p,Vec2 a,Vec2 b)
{
    float vx=b.x-a.x,vy=b.y-a.y,length=vx*vx+vy*vy;
    float t=length>0?((p.x-a.x)*vx+(p.y-a.y)*vy)/length:0;
    t=fmaxf(0,fminf(1,t));return hypotf(p.x-a.x-t*vx,p.y-a.y-t*vy);
}

bool game_blocked(const Game *g,Vec2 a,Vec2 b,int target)
{
    for (int i=0;i<g->object_count;i++) {
        const Entity *e=&g->objects[i];
        if (i!=target && e->kind==BARRIER && !e->done && segment_distance(e->p,a,b)<BARRIER_RADIUS) return true;
    }
    return false;
}

static bool inside(Vec2 p,Vec2 centre,Vec2 axes)
{
    float x=(p.x-centre.x)/axes.x,y=(p.y-centre.y)/axes.y;return x*x+y*y<=1;
}

/* Cuts the path where it first enters the ellipse around CENTRE, so the
 * worker stops at the edge of the footprint instead of walking into it. */
static void stop_at_footprint(Vec2 *path,int *count,Vec2 centre,Vec2 axes)
{
    if (axes.x<=0 || axes.y<=0) return;
    for (int i=0;i+1<*count;i++) {
        Vec2 a=path[i],b=path[i+1];
        if (inside(a,centre,axes)) { *count=i+1;return; }
        if (!inside(b,centre,axes)) continue;
        /* Solve |(a + t(b-a) - c) / axes| = 1 for the entering t in [0,1]. */
        float dx=(b.x-a.x)/axes.x,dy=(b.y-a.y)/axes.y,ox=(a.x-centre.x)/axes.x,oy=(a.y-centre.y)/axes.y;
        float qa=dx*dx+dy*dy,qb=2*(ox*dx+oy*dy),qc=ox*ox+oy*oy-1,disc=qb*qb-4*qa*qc;
        float t=qa>0 && disc>=0?(-qb-sqrtf(disc))/(2*qa):1;
        t=fmaxf(0,fminf(1,t));
        path[i+1]=vec2(a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t);*count=i+2;return;
    }
}

bool game_route_to(const Game *g,const Worker *w,Vec2 destination,int target,Vec2 *path,int *count,float *length)
{
    const Entity *e=&g->objects[w->entity];
    int start=game_closest(g,e->p),finish=game_closest(g,destination),n=g->traffic_count;
    if (start<0 || finish<0) return false;
    float *costs=malloc((size_t)n*sizeof *costs);int *parents=malloc((size_t)n*sizeof *parents);
    bool *visited=calloc((size_t)n,sizeof *visited);bool found=false;
    if (costs && parents && visited) {
        for (int i=0;i<n;i++) costs[i]=INFINITY,parents[i]=-1;
        costs[start]=0;
        for (;;) {
            int current=-1;float cost=INFINITY;
            for (int i=0;i<n;i++) if (!visited[i] && costs[i]<cost) current=i,cost=costs[i];
            if (current<0) break;
            if (current==finish) { found=true;break; }
            visited[current]=true;
            for (int k=g->link_start[current];k<g->link_start[current+1];k++) {
                int i=g->links[k];
                if (visited[i]) continue;
                float d=vec2_distance(g->traffic[current],g->traffic[i]);
                if (cost+d<costs[i] && !game_blocked(g,g->traffic[current],g->traffic[i],target)) costs[i]=cost+d,parents[i]=current;
            }
        }
    }
    if (found) {
        int nodes=0;
        for (int i=finish;i>=0;i=parents[i]) nodes++;
        path[0]=e->p;*count=1+nodes;
        for (int i=finish,k=nodes;i>=0;i=parents[i],k--) path[k]=g->traffic[i];
        if (vec2_distance(path[*count-1],destination)>5) path[(*count)++]=destination;
        if (target>=0) {
            const Entity *t=&g->objects[target];
            if (t->type->approach==APPROACH_FOOTPRINT && !t->has_workpoint)
                stop_at_footprint(path,count,destination,game_footprint(g,t));
        }
        *length=0;
        for (int i=1;i<*count && found;i++) {
            if (game_blocked(g,path[i-1],path[i],target)) found=false;
            *length+=vec2_distance(path[i-1],path[i]);
        }
    }
    free(costs);free(parents);free(visited);return found;
}

bool game_route(const Game *g,const Worker *w,int target,Vec2 *path,int *count,float *length)
{
    const Entity *e=&g->objects[target];
    return game_route_to(g,w,e->has_workpoint?e->workpoint:e->p,target,path,count,length);
}

bool game_vehicle_route(const Game *g,const Worker *w,int target,Vec2 *path,int *count,float *length)
{
    const Entity *t=&g->objects[target];
    if (w->atv) {
        /* The authored region is the offroad corridor. It must not grant the
         * ATV free passage over unrelated road obstacles. */
        if (t->transport!=TRANSPORT_ATV || !t->has_corridor) return false;
        path[0]=g->objects[w->entity].p;path[1]=t->corridor;path[2]=t->p;*count=3;
        *length=vec2_distance(path[0],t->corridor)+vec2_distance(t->corridor,t->p);return true;
    }
    if (!w->boat || t->transport!=TRANSPORT_BOAT) return false;
    return game_route(g,w,target,path,count,length);
}

bool game_reachable(const Game *g,int target)
{
    const Entity *t=&g->objects[target];int count;float length;bool reachable=false;
    if (g->rules->gates && (t->type->effect==EFFECT_FOG || game_fogged(g,t) || !game_gate_open(g,t))) return false;
    Vec2 *path=malloc((size_t)game_path_capacity(g)*sizeof *path);
    if (!path) return false;
    if (g->rules->vehicles && t->transport!=TRANSPORT_FOOT) {
        for (int i=0;i<g->vehicle_count && !reachable;i++) {
            const Worker *v=&g->vehicles[i];const Entity *base=&g->objects[v->home_base];
            reachable=base->operational && game_gate_open(g,base) && game_vehicle_route(g,v,target,path,&count,&length);
        }
    } else for (int i=0;i<g->worker_count && !reachable;i++) {
        const Worker *w=&g->workers[i];
        if (w->phase==STRANDED || w->recruiting || w->boat!=t->type->boat_only) continue;
        reachable=game_route(g,w,target,path,&count,&length);
    }
    free(path);return reachable;
}
