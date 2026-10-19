#include "game_internal.h"
#include <math.h>


int game_closest(Game *g, Vec2 p)
{
    int best=-1;float value=INFINITY;
    for (int i=0;i<g->level.traffic_count;i++) {
        float d=game_distance(p,g->level.traffic[i]);if (d<value) best=i,value=d;
    }
    return best;
}

static float game_segment_distance(Vec2 p,Vec2 a,Vec2 b)
{
    float vx=b.x-a.x,vy=b.y-a.y,length=vx*vx+vy*vy;
    float t=length>0?((p.x-a.x)*vx+(p.y-a.y)*vy)/length:0;
    t=fmaxf(0,fminf(1,t));return hypotf(p.x-a.x-t*vx,p.y-a.y-t*vy);
}

bool game_blocked(Game *g, Vec2 a,Vec2 b,int target)
{
    for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];
        if (i!=target && e->kind==BARRIER && !e->done && game_segment_distance(e->p,a,b)<20) return true;
    }
    return false;
}

bool game_route_to(Game *g, const Worker *w,Vec2 destination,int target,Vec2 *path,int *count,float *length)
{
    const Entity *e=&g->objects[w->entity];
    int start=game_closest(g, e->p),finish=game_closest(g, destination),n=g->level.traffic_count;
    if (start<0 || finish<0) return false;
    float costs[MAX_TRAFFIC_POINTS];int parents[MAX_TRAFFIC_POINTS],reverse[MAX_TRAFFIC_POINTS];
    bool visited[MAX_TRAFFIC_POINTS]={false};
    for (int i=0;i<n;i++) costs[i]=INFINITY,parents[i]=-1;
    costs[start]=0;
    while (true) {
        int current=-1;float cost=INFINITY;
        for (int i=0;i<n;i++) if (!visited[i] && costs[i]<cost) current=i,cost=costs[i];
        if (current<0) return false;
        if (current==finish) break;
        visited[current]=true;
        for (int i=0;i<n;i++) {
            float d=game_distance(g->level.traffic[current],g->level.traffic[i]);
            if (visited[i] || d>=50 || game_blocked(g, g->level.traffic[current],g->level.traffic[i],target)) continue;
            if (cost+d<costs[i]) costs[i]=cost+d,parents[i]=current;
        }
    }
    int k=0;for (int i=finish;i>=0;i=parents[i]) reverse[k++]=i;
    path[0]=e->p;*count=1;
    for (int i=k-1;i>=0;i--) path[(*count)++]=g->level.traffic[reverse[i]];
    Vec2 last=path[*count-1];float d=game_distance(last,destination);
    if (d>5) {
        float stop=target>=0 && (g->objects[target].kind==BARRIER || g->objects[target].kind==BUILD_LOT)?fminf(24,d*0.5f):0;
        path[(*count)++]=(Vec2){destination.x+(last.x-destination.x)*stop/d,destination.y+(last.y-destination.y)*stop/d};
    }
    for (int i=1;i<*count;i++) if (game_blocked(g, path[i-1],path[i],target)) return false;
    *length=costs[finish]+game_distance(e->p,g->level.traffic[start])+d;
    return true;
}

bool game_route(Game *g, const Worker *w,int target,Vec2 *path,int *count,float *length)
{
    Entity *e=&g->objects[target];
    return game_route_to(g, w,e->kind==SERVICE || e->kind==VALVE?e->workpoint:e->p,target,path,count,length);
}

bool game_vehicle_route(Game *g, const Worker *w,int target,Vec2 *path,int *count,float *length)
{
    Entity *t=&g->objects[target];
    if (w->atv) {
        if (t->transport!=BY_ATV) return false;
        for (int i=0;i<g->level.object_count;i++) {
            LevelObject *r=&g->level.objects[i];
            Vec2 center={r->x,r->y};
            if (r->type==152 && game_distance(t->p,center)<=(float)r->link_id) {
                /* The authored region is the offroad corridor. It must not
                 * grant the ATV free passage over unrelated road obstacles. */
                path[0]=g->objects[w->entity].p;path[1]=center;path[2]=t->p;*count=3;
                *length=game_distance(path[0],center)+game_distance(center,t->p);return true;
            }
        }
        return false;
    } else if (!w->boat || t->transport!=BY_BOAT) return false;
    return game_route(g, w,target,path,count,length);
}

bool game_reachable(Game *g, int target)
{
    Vec2 path[MAX_PATH];int count;float length;
    Entity *target_entity=&g->objects[target];
    if (g->early && (target_entity->effect==1 || game_fogged(g, target_entity) || !game_gate_open(g, target_entity))) return false;
    if (g->early && target_entity->transport!=ON_FOOT) {
        for (int i=0;i<g->vehicle_count;i++) {
            Worker *v=&g->vehicles[i];Entity *base=&g->objects[v->home_base];
            if (base->operational && game_gate_open(g, base) && game_vehicle_route(g, v,target,path,&count,&length)) return true;
        }
        return false;
    }
    for (int i=0;i<g->worker_count;i++) {
        Worker *w=&g->workers[i];Entity *e=&g->objects[target];
        if (w->phase==STRANDED || w->recruiting) continue;
        if ((w->boat==e->boat_worker) && game_route(g, w,target,path,&count,&length)) return true;
    }
    return false;
}
