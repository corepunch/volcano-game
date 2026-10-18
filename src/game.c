#include "volcano.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define MAX_WORKERS 64
#define MAX_PATH (MAX_TRAFFIC_POINTS+2)
#define PI 3.14159265358979323846
enum { DECOR, WATER, BASE, BARRIER, RESOURCE, SURVIVOR, WORKER, BUILD_LOT, BUILDING, RECRUIT, ANIMAL, VALVE, SERVICE, VEHICLE };
enum { ON_FOOT, BY_ATV, BY_BOAT };
enum { IDLE, OUTBOUND, WORKING, RETURNING, STRANDED };
enum { BUILD, REPAIR, DEMOLISH };
enum { NO_AMBIENT, BIRD, CLOUD };
typedef struct {
    Vec2 p,workpoint,origin,roam;
    char anim[128];
    int kind,order,frame,effect,source,owner,output,action;
    double duration;
    double fire_stopped;
    bool done,hidden,assigned,queued,fire,floating,roof,operational;
    uint32_t seed;
    int ambient,support;
    float travel_speed;
    int queue_order,building;
    int follower;
    int gate,transport,roam_step,checked_route;
    bool roaming;
    double income_clock,bonus_clock;
} Entity;
typedef struct {
    int entity,phase,target,step,count,direction,home_base,cargo,value;
    bool boat,recruiting,atv;
    Vec2 home,path[MAX_PATH];
    double work;
} Worker;
typedef struct {
    Level level;
    Entity objects[MAX_LEVEL_OBJECTS];
    int object_count,worker_count,target_count,barriers,resources,survivors;
    int cleared,collected,rescued,completed,food,wood,money,gas,index,build_menu,next_order,campaign,goal;
    Worker workers[MAX_WORKERS],evacuees[MAX_WORKERS],vehicles[16];
    int evacuee_count,vehicle_count,animals,animals_rescued,purified,basin_goal;
    int transport_trips,fishing_trips,route_epoch;
    double time,message_time;
    bool paused,debug,ready,early,won,routes_dirty;
    float pointer_x,pointer_y;
    bool pointer_down;
    char message[256],background[PATH_CAPACITY],alpha[PATH_CAPACITY];
} Game;
static Game *current_game;
#define game (*current_game)

static float distance(Vec2 a,Vec2 b) { return hypotf(a.x-b.x,a.y-b.y); }
static SDL_Color color(int r,int g,int b,int a) { return (SDL_Color){r,g,b,a}; }
static bool fail(const char *format,...)
{
    va_list args;va_start(args,format);vsnprintf(last_error,1024,format,args);va_end(args);return false;
}
static void announce(const char *text) { snprintf(game.message,sizeof(game.message),"%s",text);game.message_time=4; }
static float particle_random(uint32_t seed);
static const char *sprite_name(const char *name)
{
    static const char *aliases[][2]={
        {"food1","food"},{"food2","food_big"},{"wood1","wood"},{"wood2","wood_big"},
        {"gas1","gas"},{"gas2","gas_big"},{"tree00","bar_tree00"},{"stone00","bar_stone00"},
        {"burger","burger0"},{"sawmill","sawmill0"},{"heliport","heliport_br"},
        {"house","house0"},{"derrick","gas0"},{"stone_big00","bar_stone_big00"},
        {"money1","money"},{"money2","money_big"},{"bonus_fast_offroad","bonus_fast_offroader"},
        {"offroad_wood","wood_big"},{"boat_food","food_big"}
    };
    for (size_t i=0;i<sizeof(aliases)/sizeof(*aliases);i++) if (!strcmp(name,aliases[i][0])) return aliases[i][1];
    return name;
}
static int add_entity(int kind,const LevelObject *raw,const char *anim)
{
    if (game.object_count>=MAX_LEVEL_OBJECTS) return -1;
    int index=game.object_count++;
    Entity *e=&game.objects[index];
    e->kind=kind;e->p=e->workpoint=e->origin=e->roam=(Vec2){raw->x,raw->y};e->frame=-1;e->follower=-1;e->order=index;e->fire_stopped=-1;
    /* Retail marks burning barriers in the object's name, not its sprite. */
    e->fire=raw->type==102 && !strcmp(raw->name,"fire");
    e->seed=raw->id;e->support=-1;e->owner=e->output=e->gate=e->checked_route=-1;e->roof=kind==SURVIVOR && !strcmp(raw->extra,"roof");
    e->transport=!strcmp(raw->extra,"offroad")?BY_ATV:!strcmp(raw->params,"boat_food")?BY_BOAT:ON_FOOT;
    e->floating=(kind==DECOR && (strstr(anim,"_water") || strstr(anim,"treewater"))) ||
        (kind==RESOURCE && (!strcmp(anim,"boat_wood") || !strcmp(raw->params,"boat_food"))) || raw->type==113;
    if (raw->type==6) {
        e->ambient=!strncmp(anim,"bird",4)?BIRD:strstr(anim,"cloud")?CLOUD:NO_AMBIENT;
        const char *last=strrchr(raw->params,';');float speed=last?strtof(last+1,NULL):0;
        e->travel_speed=e->ambient==BIRD?(speed>0?speed:110):8+particle_random(e->seed)*6;
    }
    snprintf(e->anim,sizeof(e->anim),"%s",anim);
    return index;
}
static void decor_name(const char *params,char *output,size_t capacity)
{
    if (level_param(params,"tree",output,capacity) || level_param(params,"anim",output,capacity) ||
        level_param(params,"simple",output,capacity)) return;
    snprintf(output,capacity,"%s",params);
    char *end=strchr(output,';');if (end) *end=0;
}
/* These are editor tiles and scene controllers, not atlas declarations. Keep
 * their original records in Level; their scenario behavior is still deferred. */
static bool deferred_decor(const char *name)
{
    static const char *names[]={"editor_land","editor_water","211_oil","014_waste",
        "106_rails0","106_rails1","106_rails2","107_tunnel","offset=effect_lava_add"};
    for (size_t i=0;i<sizeof names/sizeof *names;i++) if (!strcmp(name,names[i])) return true;
    return false;
}
static int closest(Vec2 p)
{
    int best=-1;float value=INFINITY;
    for (int i=0;i<game.level.traffic_count;i++) {
        float d=distance(p,game.level.traffic[i]);if (d<value) best=i,value=d;
    }
    return best;
}
static float segment_distance(Vec2 p,Vec2 a,Vec2 b)
{
    float vx=b.x-a.x,vy=b.y-a.y,length=vx*vx+vy*vy;
    float t=length>0?((p.x-a.x)*vx+(p.y-a.y)*vy)/length:0;
    t=fmaxf(0,fminf(1,t));return hypotf(p.x-a.x-t*vx,p.y-a.y-t*vy);
}
static bool blocked(Vec2 a,Vec2 b,int target)
{
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if (i!=target && e->kind==BARRIER && !e->done && segment_distance(e->p,a,b)<20) return true;
    }
    return false;
}
static bool route_to(const Worker *w,Vec2 destination,int target,Vec2 *path,int *count,float *length)
{
    const Entity *e=&game.objects[w->entity];
    int start=closest(e->p),finish=closest(destination),n=game.level.traffic_count;
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
            float d=distance(game.level.traffic[current],game.level.traffic[i]);
            if (visited[i] || d>=50 || blocked(game.level.traffic[current],game.level.traffic[i],target)) continue;
            if (cost+d<costs[i]) costs[i]=cost+d,parents[i]=current;
        }
    }
    int k=0;for (int i=finish;i>=0;i=parents[i]) reverse[k++]=i;
    path[0]=e->p;*count=1;
    for (int i=k-1;i>=0;i--) path[(*count)++]=game.level.traffic[reverse[i]];
    Vec2 last=path[*count-1];float d=distance(last,destination);
    if (d>5) {
        float stop=target>=0 && (game.objects[target].kind==BARRIER || game.objects[target].kind==BUILD_LOT)?fminf(24,d*0.5f):0;
        path[(*count)++]=(Vec2){destination.x+(last.x-destination.x)*stop/d,destination.y+(last.y-destination.y)*stop/d};
    }
    for (int i=1;i<*count;i++) if (blocked(path[i-1],path[i],target)) return false;
    *length=costs[finish]+distance(e->p,game.level.traffic[start])+d;
    return true;
}
static bool route(const Worker *w,int target,Vec2 *path,int *count,float *length)
{
    Entity *e=&game.objects[target];
    return route_to(w,e->kind==SERVICE || e->kind==VALVE?e->workpoint:e->p,target,path,count,length);
}
static const char *build_anims[]={"sawmill0","burger0","house0","gas0"};
static const char *build_names[]={"Sawmill","Burger Cafe","House","Gas Station"};
static const int build_costs[]={40,40,60,80};
typedef struct { int food,wood,money; } Cost;
/* Starting inventories are visible in the original level 1–6 screenshots.
 * Costs and production intervals are explicit restoration balance: the retail
 * rules live outside the supplied .lvl files, so these are not claimed exact. */
static Cost job_cost(const Entity *e)
{
    Cost c={0};if (!game.early) return c;
    if (e->kind==BARRIER) {
        if (strstr(e->anim,"tree")) c.food=20;
        else if (strstr(e->anim,"stone_big")) c.food=40,c.money=100;
        else if (strstr(e->anim,"stone")) c.food=20;
        else if (strstr(e->anim,"bridge_auto")) c.food=20,c.wood=40;
        else if (strstr(e->anim,"bridge") || strstr(e->anim,"road")) c.wood=20;
    } else if (e->kind==BUILDING && !e->operational) c.wood=build_costs[e->building]/2;
    else if (e->kind==ANIMAL) c.food=10;
    else if (e->kind==VALVE) c.money=20;
    else if (e->kind==SERVICE && !e->operational) c.wood=40;
    return c;
}
static bool affordable(Cost c) { return game.food>=c.food && game.wood>=c.wood && game.money>=c.money; }
static void spend(Cost c) { game.food-=c.food;game.wood-=c.wood;game.money-=c.money; }
static void credit(int resource,int value)
{
    if (resource==1) game.food+=value;
    else if (resource==2) game.gas+=value;
    else if (resource==3) game.money+=value;
    else if (resource==4) game.wood+=value;
}
static int building_resource(int type) { static const int kinds[]={4,1,3,2};return kinds[type]; }
static void building_art(Entity *e)
{
    snprintf(e->anim,sizeof e->anim,"%s%s",build_anims[e->building],e->operational?"":"_br");e->frame=0;
}
static bool available(const Worker *w) { return w->phase==IDLE; }
static bool gate_open(const Entity *e) { return e->gate<0 || game.objects[e->gate].done; }
static bool fogged(const Entity *e)
{
    for (int i=0;i<game.object_count;i++) {
        Entity *fog=&game.objects[i];
        if (fog->effect==1 && !fog->done && distance(e->p,fog->p)<105) return true;
    }
    return false;
}
static bool vehicle_route(const Worker *w,int target,Vec2 *path,int *count,float *length)
{
    Entity *t=&game.objects[target];
    if (w->atv) {
        if (t->transport!=BY_ATV) return false;
        for (int i=0;i<game.level.object_count;i++) {
            LevelObject *r=&game.level.objects[i];
            Vec2 center={r->x,r->y};
            if (r->type==152 && distance(t->p,center)<=(float)r->link_id) {
                /* The authored region is the offroad corridor. It must not
                 * grant the ATV free passage over unrelated road obstacles. */
                path[0]=game.objects[w->entity].p;path[1]=center;path[2]=t->p;*count=3;
                *length=distance(path[0],center)+distance(center,t->p);return true;
            }
        }
        return false;
    } else if (!w->boat || t->transport!=BY_BOAT) return false;
    return route(w,target,path,count,length);
}
static bool dispatch_vehicle(int target,bool quiet)
{
    Entity *t=&game.objects[target];
    if (!gate_open(t) || fogged(t)) return false;
    int chosen=-1,count=0,best_count=0;float length,shortest=INFINITY;
    Vec2 path[MAX_PATH],best[MAX_PATH];
    for (int i=0;i<game.vehicle_count;i++) {
        Worker *v=&game.vehicles[i];Entity *base=&game.objects[v->home_base];
        if (v->phase!=IDLE || !base->operational || !gate_open(base)) continue;
        if (vehicle_route(v,target,path,&count,&length) && length<shortest) {
            chosen=i;shortest=length;best_count=count;memcpy(best,path,(size_t)count*sizeof *path);
        }
    }
    if (chosen<0 || game.gas<10) {
        if (!quiet) announce(game.gas<10?"Not enough fuel: 10 required.":"Repair the garage or pier and clear its route first.");
        return false;
    }
    Worker *v=&game.vehicles[chosen];game.gas-=10;v->target=target;v->phase=OUTBOUND;v->step=1;v->count=best_count;
    memcpy(v->path,best,(size_t)best_count*sizeof *best);game.objects[v->entity].hidden=false;
    t->assigned=true;t->queued=false;game.transport_trips++;
    if (!quiet) announce(v->atv?"ATV dispatched to rescue a worker.":"Boat dispatched to fish.");
    return true;
}
static bool allowed_building(int type) { return !(game.early && game.campaign==6 && type==1); }
static bool dispatch(int target,bool quiet)
{
    Entity *t=&game.objects[target];
    if (game.paused || game.won || t->hidden || (t->done && t->kind!=BUILDING && t->kind!=BUILD_LOT) || t->assigned) return false;
    if (game.early && (t->transport==BY_ATV || t->transport==BY_BOAT)) return dispatch_vehicle(target,quiet);
    if (game.early && (t->effect==1 || !gate_open(t) || fogged(t))) {
        if (!quiet) announce("Turn the linked valve to clear the fog or purify the water first.");
        return false;
    }
    if (game.early && (t->kind==SURVIVOR || t->kind==RECRUIT)) return false;
    if (game.early && t->kind==RESOURCE && t->owner>=0 && game.objects[t->owner].assigned) return false;
    Cost cost=job_cost(t);
    if (game.early && !strcmp(t->anim,"stone_dynamite")) {
        if (!quiet) announce("Dynamite is required for this obstruction.");return false;
    }
    if (!affordable(cost)) { if (!quiet) announce("Not enough resources. Collect supplies or restore production.");return false; }
    int chosen=-1,best_count=0;float shortest=INFINITY;
    Vec2 path[MAX_PATH],best[MAX_PATH];
    for (int i=0;i<game.worker_count;i++) {
        Worker *w=&game.workers[i];
        bool compatible=!strcmp(t->anim,"boat_wood")?w->boat:!w->boat;
        int count;float length;
        if (compatible && available(w) && route(w,target,path,&count,&length) && length<shortest) {
            shortest=length;chosen=i;best_count=count;memcpy(best,path,count*sizeof(Vec2));
        }
    }
    if (chosen<0) { if (!quiet) announce("No free worker can reach this target. Clear the route first.");return false; }
    spend(cost);
    Worker *w=&game.workers[chosen];
    w->phase=OUTBOUND;w->target=target;w->step=1;w->count=best_count;
    memcpy(w->path,best,best_count*sizeof(Vec2));
    t->assigned=true;t->queued=false;
    game.objects[w->entity].hidden=false;
    if (!quiet) announce(t->kind==BARRIER?"Worker dispatched to clear the route.":
        t->kind==SURVIVOR?"Worker dispatched to escort a survivor.":t->kind==BUILD_LOT?"Worker dispatched to build.":"Collecting supplies.");
    return true;
}

bool game_init(int index)
{
    if (!campaign_load(asset_root)) return false;
    char path[PATH_CAPACITY];snprintf(path,sizeof(path),"%s/level/level%03d.lvl",asset_root,index);
    /* Helpers build against the candidate; commit only after full validation. */
    Game *candidate=calloc(1,sizeof(*candidate)),*previous=current_game;
    if (!candidate) return fail("Cannot allocate gameplay state");
    if (!level_load(path,&candidate->level,last_error,1024)) { free(candidate);return false; }
    current_game=candidate;
    game.index=index;game.build_menu=-1;game.pointer_x=game.pointer_y=-100;
    game.campaign=campaign_number(index);game.early=game.campaign>=1 && game.campaign<=6;game.routes_dirty=true;
    if (game.early) {
        const Cost starts[]={{20,0,0},{50,30,20},{10,100,30},{60,80,70},{70,80,40},{60,90,40}};
        Cost c=starts[game.campaign-1];game.food=c.food;game.wood=c.wood;game.money=c.money;
        const int goals[]={3,3,5,13,6,2},fuel[]={0,0,0,70,0,220};
        game.goal=goals[game.campaign-1];game.gas=fuel[game.campaign-1];game.basin_goal=game.campaign==6?2:0;
    }
    snprintf(game.background,sizeof(game.background),"texture/back/%s_0.jpg",game.level.background);
    snprintf(game.alpha,sizeof(game.alpha),"texture/back/%s_0.alpha",game.level.background);
    for (int i=0;i<game.level.object_count;i++) {
        LevelObject *raw=&game.level.objects[i];int kind=DECOR;char name[128]={0};
        switch (raw->type) {
        case 2: decor_name(raw->params,name,sizeof(name));if (!strcmp(name,"water0")) kind=WATER;break;
        case 102: kind=BARRIER;snprintf(name,sizeof(name),"%s",raw->params);break;
        case 108:
            kind=RESOURCE;snprintf(name,sizeof(name),"%s",game.early && !strcmp(raw->params,"boat_food")?"boat_fish_idle":raw->params);break;
        case 110: kind=BASE;snprintf(name,sizeof(name),"base0");break;
        case 104: case 113:
            kind=game.early && raw->type==113?VEHICLE:WORKER;
            snprintf(name,sizeof(name),"%s",raw->type==113?"boat0":"worker_wait0");break;
        case 109: kind=SURVIVOR;snprintf(name,sizeof(name),"man_idle0");break;
        case 105:
            if (!game.early) continue;
            kind=ANIMAL;snprintf(name,sizeof name,"%s%u_idle0",raw->params,raw->id%3);break;
        case 128:
            if (!game.early) continue;
            kind=VALVE;snprintf(name,sizeof name,"tap");break;
        case 151:
            if (!game.early) continue;
            kind=WATER;snprintf(name,sizeof name,"water0");break;
        case 153: case 154:
            if (!game.early || game.campaign<4) continue;
            kind=raw->type==153?VEHICLE:SERVICE;
            snprintf(name,sizeof name,"%s",raw->type==153?"offroad0":"garage0");break;
        case 125: snprintf(name,sizeof(name),"firefighter_idle0");break;
        case 6:
            snprintf(name,sizeof(name),"%s",raw->params);
            { char *end=strchr(name,';');if (end) *end=0; }
            break;
        case 142: snprintf(name,sizeof(name),"delivery_place0");break;
        case 146: kind=BUILD_LOT;snprintf(name,sizeof(name),"build_place");break;
        case 101: kind=BUILDING;snprintf(name,sizeof(name),"%s",raw->params);break;
        case 114:
            if (game.early) {kind=SERVICE;snprintf(name,sizeof name,"seaport_br");break;}
            snprintf(name,sizeof name,"%s",raw->params);break;
        case 112: case 126: case 147:
            snprintf(name,sizeof(name),"%s",raw->params);break;
        default: continue;
        }
        if (!name[0] || (raw->type==2 && deferred_decor(name))) continue;
        int effect=raw->type==102?(!strncmp(name,"fog_",4)?1:!strcmp(name,"fire")?2:0):0;
        if (!effect && !animation_find(sprite_name(name))) {
            fail("Missing original animation '%s' for object %u",sprite_name(name),raw->id);goto restore;
        }
        int id=add_entity(kind,raw,name);if (id<0) { fail("Too many game objects");goto restore; }
        Entity *e=&game.objects[id];
        e->effect=effect;e->source=i;
        if (kind==ANIMAL) {game.animals++;e->duration=1.2;}
        if (kind==VALVE || kind==SERVICE) {e->duration=5;e->operational=kind==SERVICE && !strcmp(raw->extra,"0;0");}
        if (raw->type==151) e->effect=3;
        if (game.early && kind==BUILDING) {
            e->building=!strcmp(raw->params,"sawmill")?0:!strcmp(raw->params,"burger")?1:!strcmp(raw->params,"house")?2:3;
            e->operational=!strcmp(raw->extra,"0;0");e->duration=5;building_art(e);
        }
        if (raw->type==6) e->p=raw->auxiliary;
        if (kind==WORKER) {
            if (game.worker_count>=MAX_WORKERS) { fail("Too many workers");goto restore; }
            Worker *w=&game.workers[game.worker_count++];w->entity=id;w->home=e->p;w->boat=raw->type==113;w->target=-1;w->home_base=-1;
        } else if (kind==BARRIER || kind==RESOURCE || kind==SURVIVOR) {
            game.target_count++;e->duration=kind==BARRIER?5:kind==RESOURCE?0:0.8;
            if (kind==BARRIER) { game.barriers++;e->frame=0; }
            else if (kind==RESOURCE) { game.resources++;e->frame=0; }
            else game.survivors++;
        }
    }
    if (game.early) for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];const LevelObject *raw=&game.level.objects[e->source];
        if (e->kind==SERVICE || e->kind==VALVE) for (int j=0;j<game.level.object_count;j++) {
            LevelObject *marker=&game.level.objects[j];
            if (marker->type==3 && marker->link_id==e->seed && !marker->name[0]) e->workpoint=(Vec2){marker->x,marker->y};
        }
        const char *group=(e->effect==1 || raw->type==151)?raw->name:NULL;
        if ((raw->type==151 && !*raw->name) || raw->type==114 || e->transport==BY_BOAT) {
            float nearest=INFINITY;
            for (int j=0;j<game.level.object_count;j++) {
                LevelObject *tile=&game.level.objects[j];float d=distance(e->p,(Vec2){tile->x,tile->y});
                if (tile->type==151 && tile->name[0] && d<nearest) {nearest=d;group=tile->name;}
            }
        }
        if (group && *group) for (int j=0;j<game.object_count;j++)
            if (game.objects[j].kind==VALVE && !strcmp(group,game.level.objects[game.objects[j].source].name)) e->gate=j;
        if (e->kind==VEHICLE) {
            if (game.vehicle_count==16) {fail("Too many rescue vehicles");goto restore;}
            Worker *v=&game.vehicles[game.vehicle_count++];v->entity=i;v->target=-1;v->home_base=-1;v->atv=raw->type==153;v->boat=!v->atv;
            for (int j=0;j<game.level.object_count;j++) {
                LevelObject *marker=&game.level.objects[j];
                if (marker->type!=3 || !*marker->name || strcmp(marker->name,raw->name)) continue;
                for (int k=0;k<game.object_count;k++) if (game.objects[k].kind==SERVICE && game.objects[k].seed==marker->link_id) {
                    v->home_base=k;v->home=(Vec2){marker->x,marker->y};
                }
            }
            if (v->home_base<0) {fail("Vehicle %u has no authored service marker",e->seed);goto restore;}
            e->p=v->home;e->hidden=!game.objects[v->home_base].operational;
        }
    }
    if (!game.worker_count || !game.level.traffic_count) {
        fail("Original level %d has no prototype workers or traffic graph",index);goto restore;
    }
    for (int i=0;i<game.worker_count;i++) {
        Worker *w=&game.workers[i];
        if (!w->boat) for (int j=0;j<game.object_count;j++)
            if (game.objects[j].kind==BASE && distance(w->home,game.objects[j].p)<80) { w->home_base=j;break; }
        game.objects[w->entity].hidden=w->home_base>=0;
        if (game.early && !w->boat) {
            int base=-1;
            for (int j=0;j<game.object_count;j++) if (game.objects[j].kind==BASE) { base=j;break; }
            if (base<0) { fail("Mission has no worker base");goto restore; }
            w->home_base=base;w->home=game.objects[base].p;
            if (game.campaign==1 || distance(game.objects[w->entity].p,w->home)<80) {
                game.objects[w->entity].p=w->home;game.objects[w->entity].hidden=true;
            } else {
                w->phase=STRANDED;w->recruiting=true;game.objects[w->entity].kind=RECRUIT;
            }
        }
    }
    if (game.early) for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if (e->kind==SURVIVOR) {
            if (game.evacuee_count==MAX_WORKERS) { fail("Too many evacuees");goto restore; }
            Worker *v=&game.evacuees[game.evacuee_count++];v->entity=i;v->phase=STRANDED;
            v->home=game.workers[0].home;v->target=-1;
        }
        const LevelObject *raw=&game.level.objects[e->source];
        if (e->kind==RESOURCE && raw->link_id) for (int j=0;j<game.object_count;j++) {
            Entity *owner=&game.objects[j];
            if (owner->seed==raw->link_id && (owner->kind==BUILDING || owner->kind==BUILD_LOT)) {
                e->owner=j;owner->output=i;e->hidden=!owner->operational;break;
            }
        }
    }
    /* Retail roof occupants have extra="roof" and a higher source layer than
     * flooded decor, but no link ID. Recover support from the authored footprint. */
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];if (!e->roof) continue;
        float closest_house=INFINITY;
        for (int j=0;j<game.object_count;j++) {
            Entity *house=&game.objects[j];
            if (house->kind!=DECOR || (strcmp(house->anim,"sawmill_water") && strcmp(house->anim,"house_water"))) continue;
            SpriteFrame f=animation_find(house->anim)->frames[0];
            float dx=e->p.x-house->p.x,dy=e->p.y-house->p.y,d=distance(e->p,house->p);
            if (dx>=-f.dx && dx<=f.w-f.dx && dy>=-f.dy && dy<=f.h-f.dy && d<closest_house)
                e->support=j,closest_house=d;
        }
    }
    game.ready=true;announce(game.early?"Collect supplies and clear routes. People return to headquarters when the path is safe.":"Click debris, supplies or survivors to dispatch a worker.");
    free(previous);last_error[0]=0;return true;
restore:
    current_game=previous;free(candidate);return false;
}
static void return_home(Worker *w)
{
    float length;
    if (!route_to(w,w->home,-1,w->path,&w->count,&length)) return;
    w->phase=RETURNING;w->step=1;
}
static bool burning_log(const Entity *e)
{
    return e->fire && e->kind==BARRIER && (!strcmp(e->anim,"tree00") || !strcmp(e->anim,"bar_tree00"));
}
static void finish_work(int worker)
{
    Worker *w=&game.workers[worker];Entity *t=&game.objects[w->target];t->done=true;t->assigned=false;
    if (t->kind==BARRIER) {
        if (burning_log(t)) t->fire_stopped=game.time;
        game.cleared++;game.completed++;
        if (strstr(t->anim,"bridge")) t->frame=3;else t->hidden=true;
        if (game.early && strstr(t->anim,"tree")) game.wood+=20;
        game.routes_dirty=true;
        game.route_epoch++;
    } else if (t->kind==BUILD_LOT) {
        t->kind=BUILDING;t->frame=0;
        snprintf(t->anim,sizeof t->anim,"%s",build_anims[t->building]);
        t->operational=true;t->income_clock=t->bonus_clock=0;
    } else if (t->kind==BUILDING) {
        t->income_clock=t->bonus_clock=0;
        if (t->action==DEMOLISH) {
            t->kind=BUILD_LOT;t->operational=false;t->done=false;
            snprintf(t->anim,sizeof t->anim,"build_place");
            if (t->output>=0) { Entity *output=&game.objects[t->output];output->hidden=true;output->queued=false; }
        } else { t->operational=true;building_art(t); }
    } else if (t->kind==SERVICE) {
        t->operational=true;snprintf(t->anim,sizeof t->anim,"%s",game.level.objects[t->source].type==114?"seaport":"garage0");
    } else if (t->kind==VALVE) {
        t->frame=1;game.routes_dirty=true;game.route_epoch++;if (game.basin_goal) game.purified++;
        for (int i=0;i<game.object_count;i++) if (game.objects[i].gate==w->target &&
            (game.objects[i].effect==1 || game.objects[i].effect==3)) {
            game.objects[i].hidden=game.objects[i].done=true;
        }
    } else if (t->kind==RESOURCE) {
        w->value=t->anim[strlen(t->anim)-1]=='2'?20:10;
        w->cargo=strstr(t->anim,"food")?1:strstr(t->anim,"gas")?2:strstr(t->anim,"money")?3:4;
        t->hidden=true;game.collected++;
    } else t->follower=worker;
    if (t->kind==BARRIER || t->kind==BUILDING || t->kind==BUILD_LOT || t->kind==VALVE || t->kind==SERVICE) w->target=-1;
    return_home(w);
}
static void arrive_home(Worker *w)
{
    Entity *e=&game.objects[w->entity];
    if (w->recruiting) { w->recruiting=false;e->kind=WORKER;game.rescued++; }
    if (w->target>=0) {
        Entity *t=&game.objects[w->target];
        if (t->kind==SURVIVOR || t->kind==ANIMAL) {
            t->hidden=true;t->follower=-1;
            if (t->kind==ANIMAL) game.animals_rescued++;else game.rescued++;
        }
        credit(w->cargo,w->value);
        game.completed++;
    }
    e->p=w->home;e->hidden=w->home_base>=0;w->phase=IDLE;w->target=-1;w->cargo=w->value=0;
    snprintf(e->anim,sizeof(e->anim),"%s",w->boat?"boat0":"worker_wait0");
    if (!game.early && game.completed==game.target_count) announce("Prototype level complete. All routes cleared and survivors escorted!");
}
static bool output_busy(const Entity *building)
{
    if (building->output<0) return false;
    for (int i=0;i<game.worker_count;i++)
        if (game.workers[i].target==building->output && game.workers[i].phase!=IDLE) return true;
    return false;
}
static void update_production(double dt)
{
    static const char *resources[]={"wood1","food1","money1","gas1"};
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if (e->kind!=BUILDING || !e->operational || e->assigned) continue;
        e->income_clock+=dt;e->bonus_clock+=dt;
        while (e->income_clock>=5) { e->income_clock-=5;credit(building_resource(e->building),5); }
        if (e->output<0) continue;
        Entity *output=&game.objects[e->output];
        if (e->bonus_clock>=10 && output->hidden && !output_busy(e)) {
            e->bonus_clock=0;output->done=output->hidden=output->assigned=output->queued=false;
            snprintf(output->anim,sizeof output->anim,"%s",resources[e->building]);
        }
    }
}
static void update_rescues(double dt)
{
    if (game.routes_dirty) {
        game.routes_dirty=false;
        for (int i=0;i<game.worker_count+game.evacuee_count;i++) {
            Worker *w=i<game.worker_count?&game.workers[i]:&game.evacuees[i-game.worker_count];
            float length;
            Entity *e=&game.objects[w->entity];
            if (w->phase==STRANDED && e->transport==ON_FOOT && !fogged(e) && route_to(w,w->home,-1,w->path,&w->count,&length)) {
                w->phase=RETURNING;w->step=1;
            }
        }
    }
    for (int i=0;i<game.evacuee_count;i++) {
        Worker *w=&game.evacuees[i];if (w->phase!=RETURNING) continue;
        Entity *e=&game.objects[w->entity];float budget=(float)dt*175;
        while (budget>0 && w->step<w->count) {
            Vec2 p=w->path[w->step];float d=distance(e->p,p);
            int direction=(int)floor(atan2(p.y-e->p.y,p.x-e->p.x)/(2*PI)*8+8.5)%8;
            snprintf(e->anim,sizeof e->anim,"man_run%d",direction);
            if (d<=budget) {e->p=p;w->step++;budget-=d;}
            else {e->p.x+=(p.x-e->p.x)*budget/d;e->p.y+=(p.y-e->p.y)*budget/d;budget=0;}
        }
        if (w->step==w->count) {e->done=e->hidden=true;w->phase=IDLE;game.rescued++;}
    }
}
static void update_vehicles(double dt)
{
    for (int i=0;i<game.vehicle_count;i++) {
        Worker *v=&game.vehicles[i];Entity *e=&game.objects[v->entity],*base=&game.objects[v->home_base];
        if (v->phase==IDLE) {e->hidden=!base->operational || !gate_open(base);continue;}
        if (v->phase==WORKING) {
            v->work+=dt;
            if (v->work<1) continue;
            Entity *t=&game.objects[v->target];t->hidden=true;
            if (v->boat) {
                v->cargo=1;v->value=!strcmp(game.level.objects[t->source].name,"food2")?20:10;
                t->done=true;
            }
            if (v->atv) {
                for (int j=0;j<v->count/2;j++) {Vec2 p=v->path[j];v->path[j]=v->path[v->count-1-j];v->path[v->count-1-j]=p;}
                v->phase=RETURNING;v->step=1;
            } else return_home(v);
        }
        if (v->phase!=OUTBOUND && v->phase!=RETURNING) continue;
        float budget=(float)dt*(v->atv?220:150);
        while (budget>0 && v->step<v->count) {
            Vec2 p=v->path[v->step];float d=distance(e->p,p);int directions=v->atv?16:24;
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
            Entity *t=&game.objects[v->target];t->assigned=false;
            if (v->boat) {credit(v->cargo,v->value);game.fishing_trips++;t->bonus_clock=0;}
            else {t->transport=ON_FOOT;t->hidden=false;t->p=base->workpoint;game.routes_dirty=true;}
            v->phase=IDLE;v->target=-1;v->cargo=v->value=0;e->p=v->home;
            snprintf(e->anim,sizeof e->anim,"%s0",v->atv?"offroad":"boat");
        }
    }
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if (e->kind==RESOURCE && e->transport==BY_BOAT && e->done && !e->assigned && gate_open(e)) {
            e->bonus_clock+=dt;
            if (e->bonus_clock>=10) {e->done=e->hidden=false;e->bonus_clock=0;}
        }
    }
}
static void update_animals(double dt)
{
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if (e->kind!=ANIMAL || e->done || e->assigned || e->hidden || fogged(e)) continue;
        if (!e->roaming) {
            if (e->checked_route==game.route_epoch) continue;
            e->checked_route=game.route_epoch;
            Vec2 path[MAX_PATH];int count;float length;bool reachable=false;
            for (int j=0;j<game.worker_count && !reachable;j++) {
                Worker *w=&game.workers[j];
                if (!w->recruiting && w->phase!=STRANDED && route(w,i,path,&count,&length)) reachable=true;
            }
            if (!reachable) continue;
            e->roaming=true;e->roam=game.level.traffic[closest(e->p)];
        }
        float d=distance(e->p,e->roam),budget=(float)dt*28;
        if (d<=budget) {
            e->p=e->roam;int candidates[MAX_TRAFFIC_POINTS],count=0;
            for (int n=0;n<game.level.traffic_count;n++) {
                Vec2 p=game.level.traffic[n];float gap=distance(e->p,p);
                Entity check=*e;check.p=p;
                if (gap>1 && gap<50 && distance(p,e->origin)<130 && !blocked(e->p,p,-1) && !fogged(&check)) candidates[count++]=n;
            }
            if (count) e->roam=game.level.traffic[candidates[(e->seed+(uint32_t)e->roam_step++*17u)%(uint32_t)count]];
        } else {e->p.x+=(e->roam.x-e->p.x)*budget/d;e->p.y+=(e->roam.y-e->p.y)*budget/d;}
        int direction=(int)floor(atan2(e->roam.y-e->p.y,e->roam.x-e->p.x)/(2*PI)*8+8.5)%8;
        snprintf(e->anim,sizeof e->anim,"%s%u_walk%d",game.level.objects[e->source].params,e->seed%3,direction);
    }
}
void game_update(double dt)
{
    if (!current_game || !game.ready || game.paused || game.won) return;
    dt=fmax(0,fmin(dt,0.1));game.time+=dt;game.message_time=fmax(0,game.message_time-dt);
    for (int i=0;i<game.worker_count;i++) {
        Worker *w=&game.workers[i];Entity *e=&game.objects[w->entity];
        if (w->phase==WORKING) { w->work+=dt;if (w->work>=game.objects[w->target].duration) finish_work(i); }
        if (w->phase==OUTBOUND || w->phase==RETURNING) {
            float budget=dt*(w->boat?150:175);
            while (budget>0 && w->step<w->count) {
                Vec2 p=w->path[w->step];float d=distance(e->p,p);int directions=w->boat?24:8;
                w->direction=(int)floor(atan2(p.y-e->p.y,p.x-e->p.x)/(2*PI)*directions+0.5+directions)%directions;
                const char *prefix=w->boat?"boat":w->phase==RETURNING && w->cargo?"worker_bag":"worker_run";
                snprintf(e->anim,sizeof(e->anim),"%s%d",prefix,w->direction);
                if (d<=budget) e->p=p,w->step++,budget-=d;
                else { e->p.x+=(p.x-e->p.x)*budget/d;e->p.y+=(p.y-e->p.y)*budget/d;budget=0; }
            }
            if (w->step==w->count) {
                if (w->phase==RETURNING) arrive_home(w);
                else {
                    w->phase=WORKING;w->work=0;
                    if (game.objects[w->target].kind==RESOURCE) { finish_work(i);continue; }
                    if (game.objects[w->target].kind==BUILD_LOT) {
                        Entity *t=&game.objects[w->target];snprintf(t->anim,sizeof t->anim,"%s_br",build_anims[t->building]);t->frame=0;
                    }
                    if (!w->boat) snprintf(e->anim,sizeof(e->anim),"%s%d",(game.objects[w->target].kind==BARRIER || game.objects[w->target].kind==BUILD_LOT || game.objects[w->target].kind==BUILDING)?"worker_srepair":"worker_gather",w->direction);
                }
            }
        }
    }
    int last=0;
    for (int n=0;n<game.object_count;n++) {
        int next=-1,order=2147483647;
        for (int i=0;i<game.object_count;i++) if (game.objects[i].queued && game.objects[i].queue_order>last && game.objects[i].queue_order<order)
            next=i,order=game.objects[i].queue_order;
        if (next<0) break;
        dispatch(next,true);last=order;
    }
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if (e->follower>=0) {
            Worker *w=&game.workers[e->follower];Vec2 p=game.objects[w->entity].p;float angle=w->direction*2*PI/8;
            e->p=(Vec2){p.x-cosf(angle)*17,p.y-sinf(angle)*17};
            if (e->kind==ANIMAL) snprintf(e->anim,sizeof e->anim,"%s%u_run%d",game.level.objects[e->source].params,e->seed%3,w->direction);
            else snprintf(e->anim,sizeof(e->anim),"man_run%d",w->direction);
        }
    }
    if (game.early) {
        update_production(dt);update_vehicles(dt);update_rescues(dt);update_animals(dt);
        if (game.rescued>=game.goal && game.animals_rescued>=game.animals && game.purified>=game.basin_goal) {
            game.won=true;game.build_menu=-1;announce("Mission complete!");
        }
    }
}
typedef struct { Vec2 p;float angle; } VisualPose;
static bool on_roof(const Entity *e) { return e->roof && e->support>=0 && !e->done && e->follower<0; }
static VisualPose visual_pose(const Entity *e)
{
    VisualPose pose={e->p,0};
    if (e->ambient) {
        float margin=e->ambient==BIRD?96:180,span=GAME_WIDTH+2*margin;
        double travel=game.time*e->travel_speed+particle_random(e->seed+3)*span;
        uint32_t pass=(uint32_t)floor(travel/span);
        pose.p.x=(float)fmod(travel,span)-margin;
        pose.p.y=70+particle_random(e->seed+pass*747796405u+9)*(GAME_HEIGHT-200);
        if (e->ambient==BIRD) pose.p.y+=5*sinf((float)game.time*1.2f+e->seed);
    } else if (on_roof(e)) {
        const Entity *house=&game.objects[e->support];VisualPose parent=visual_pose(house);
        float angle=parent.angle*(float)(PI/180),c=cosf(angle),s=sinf(angle);
        float dx=e->p.x-house->p.x,dy=e->p.y-house->p.y;
        pose.p=(Vec2){parent.p.x+c*dx+s*dy,parent.p.y-s*dx+c*dy};pose.angle=parent.angle;
    } else if (e->floating) {
        float phase=particle_random(e->seed)*2*(float)PI;
        float wave=(float)game.time*(1.2f+particle_random(e->seed+1)*.5f)+phase;
        pose.p.y+=2.3f*sinf(wave);pose.angle=1.8f*sinf(wave+.7f);
    }
    return pose;
}
static float entity_depth(const Entity *e)
{
    /* Roof occupants sort just after their support, using the house waterline
     * rather than their elevated feet. Bobbing never changes world depth. */
    return on_roof(e)?game.objects[e->support].p.y-.01f:e->p.y;
}
static int depth_compare(const void *a,const void *b)
{
    const Entity *aa=*(const Entity *const *)a,*bb=*(const Entity *const *)b;
    float ay=entity_depth(aa),by=entity_depth(bb);
    return ay==by?aa->order-bb->order:ay>by?-1:1;
}
static void paint(const Entity *e)
{
    if (!e->hidden) {
        VisualPose pose=visual_pose(e);
        if (e->effect==1) {
            char path[128];snprintf(path,sizeof path,"texture/static/part_%s.png",e->anim);
            draw_image(path,pose.p.x-64,pose.p.y-32,128,96,NULL);return;
        }
        if (e->effect==2) {
            draw_particle(false,(int)(game.time*15)%32,pose.p.x,pose.p.y+18,64,0,color(255,255,255,255));return;
        }
        if (e->effect==3) {
            draw_sprite_tinted("water0",pose.p.x,pose.p.y,game.time,0,color(95,175,65,215));return;
        }
        double time=game.time+(e->ambient==BIRD?particle_random(e->seed)*2.4:0);
        draw_sprite_rotated(sprite_name(e->anim),pose.p.x,pose.p.y,time,e->frame,e->ambient==CLOUD?.65f:1,pose.angle);
    }
}
static float particle_random(uint32_t seed)
{
    seed^=seed>>16;seed*=0x7feb352du;seed^=seed>>15;seed*=0x846ca68bu;seed^=seed>>16;
    return (seed>>8)*(1.f/16777216.f);
}
static void paint_log_particles(const Entity *e)
{
    if (!burning_log(e) || (e->hidden && !e->done) || (e->done && e->fire_stopped<0)) return;
    /* Sample a bounded birth history instead of allocating particles. The
     * seven-second warmup makes existing fires fully alight on level entry.
     * Seed by owner and birth, so redraws/pause never advance the effect. */
    double now=game.time+7,stop=e->fire_stopped+7;
    for (int smoke=1;smoke>=0;smoke--) {
        /* conf/particles.lua: fire 5/s, 2–2.5s; smoke 1.5/s, 5–7s. */
        double rate=smoke?1.5:5,max_life=smoke?7:2.5;
        float phase=particle_random((uint32_t)e->order+91u);
        int64_t latest=(int64_t)floor(now*rate-phase);
        for (int slot=(int)ceil(max_life*rate);slot>=0;slot--) {
            int64_t birth=latest-slot;double born=(birth+phase)/rate,age=now-born;
            if (e->done && born>stop) continue;
            uint32_t seed=(uint32_t)birth*747796405u+(uint32_t)e->order*2891336453u+(uint32_t)smoke*277803737u;
            float life=(smoke?5:2)+particle_random(seed)*(smoke?2:.5f);
            if (age<0 || age>=life) continue;
            float t=(float)(age/life),r=particle_random(seed+1),s=particle_random(seed+2);
            float x=e->p.x+(r-.5f)*16+(smoke?(s-.5f)*2:(s-.5f)*20)*(float)age;
            float y=e->p.y+8+(smoke?15+5*r:25+5*r)*(float)age;
            float radius=(smoke?10+5*s:10+6*s)+(smoke?2*(float)age:0);
            float opacity=smoke?fminf(1,t*5)*(1-t):fminf(1,(1-t)*5);
            int shade=smoke?(int)(196*(1-t)):255;
            float angle=smoke?(r-.5f)*(float)age*57.29578f:0;
            draw_particle(smoke,(int)(t*32),x,y,radius*2,angle,color(shade,shade,shade,(int)(opacity*255)));
        }
    }
}
static bool inside(float x,float y,float w,float h)
{
    return game.pointer_x>=x && game.pointer_x<x+w && game.pointer_y>=y && game.pointer_y<y+h;
}
static int hud_button(void)
{
    if (game.paused || game.won) {
        if (inside(508,429,350,83)) return 1;
        if (inside(508,329,350,55)) return 2;
    }
    return inside(1,681,82,82)?0:-1;
}
static int button_state(int button)
{
    return hud_button()==button?(game.pointer_down?2:1):0;
}
static void gui(float x,float y,float w,float h,float u,float v,float uw,float vh)
{
    draw_atlas("texture/gui/gui00.png",(SDL_FRect){x,y,w,h},(SDL_FRect){u,v,uw,vh});
}
static void label(const char *font,const char *text,float x,float y,float h,int size,bool centered)
{
    if (centered) x-=text_width(font,text,size)*0.5f;
    draw_font(font,text,x,GAME_HEIGHT-y-h+(h-size)*0.5f,size,color(255,255,255,255));
}
/* ButtonSysCtrl's original fixed end caps, stretched middle, and three
   authored atlas states. Nothing is recolored to manufacture a state. */
static void menu_button(int id,const char *text,float y,float h,float u,float v,float uw,float vh,float cap)
{
    float uv_cap=cap/1024;v-=button_state(id)*vh;
    gui(508,y,cap,h,u,v,uv_cap,vh);
    gui(508+cap,y,350-2*cap,h,u+uv_cap,v,uw-2*uv_cap,vh);
    gui(858-cap,y,cap,h,u+uw-uv_cap,v,uv_cap,vh);
    label("CambriaBold30B",text,683,y,h,30,true);
}
static void draw_hud(void)
{
    /* Rectangles/UVs recovered from retail gui/level.lua (1366 x 768). */
    gui(-47,668,488,115,.0020f,.5332f,.4766f,.1123f);
    gui(1,681,82,82,.7441f,.9189f-button_state(0)*.0801f,.0801f,.0801f);
    draw_atlas("texture/gui/gui01.png",(SDL_FRect){24,704,36,37},(SDL_FRect){.6201f,.7031f,.0352f,.2891f});
    gui(1010,671,454,153,.4805f,.5176f,.4434f,.1494f);
    gui(87,707,359,28,.0020f,.6504f,.3506f,.0273f);
    /* The restored prototype is untimed: keep the retail time meter full.
       The supplied .lvl data has no retail time-limit configuration. */
    gui(89,715,352,12,.3662f,.6719f,.3438f,.0117f);
    gui(200,708,10,26,.3545f,.6514f,.0098f,.0254f);
    const int inventory[]={game.food,game.wood,game.money,game.gas};
    const float positions[]={123,211,303,383};
    char text[128];
    for (int i=0;i<4;i++) {
        snprintf(text,sizeof text,"%d",inventory[i]);
        label("CalibriRegular22N",text,positions[i],739,20,22,false);
    }
    gui(87,732,37,36,.9590f,.6221f,.0361f,.0352f);
    gui(173,733,41,35,.9590f,.5869f,.0400f,.0342f);
    gui(262,733,44,35,.9551f,.5508f,.0430f,.0342f);
    gui(358,733,23,35,.9336f,.6221f,.0225f,.0342f);
    const int remaining[]={game.early?game.goal-game.rescued:game.survivors-game.rescued,
        game.early?game.animals-game.animals_rescued:game.barriers-game.cleared,
        game.early?game.basin_goal-game.purified:game.resources-game.collected};
    const char *tasks[]={"Rescue people",game.early?"Rescue animals":"Clear obstructions",game.early?"Purify the basins":"Collect supplies"};
    for (int i=0;i<(game.early?1+(game.animals>0)+(game.basin_goal>0):3);i++) {
        float y=741-21*i;bool done=remaining[i]==0;
        if (done) gui(1036,y,21,22,.8535f,.7373f,.0205f,.0215f);
        else gui(1035,y,20,21,.8330f,.7373f,.0195f,.0205f);
        if (done) snprintf(text,sizeof text,"%s: Done!",tasks[i]);
        else snprintf(text,sizeof text,"%s: %d",tasks[i],remaining[i]);
        label(done?"CalibriBold20G":"CalibriBold20W",text,1059,738-21*i,25,20,false);
    }
    if (game.early) {
        snprintf(text,sizeof text,"Level %d   Time %d:%02d",game.campaign,(int)game.time/60,(int)game.time%60);
        label("CalibriBold20W",text,683,718,25,20,true);
    }
    if (game.paused || game.won) {
        draw_rect(0,0,GAME_WIDTH,GAME_HEIGHT,color(0,0,0,130));
        /* Retail pause panel and header from gui/escape.lua. */
        gui(367,154,632,461,0,0,.6172f,.4502f);
        gui(378,542,610,64,.0029f,.4502f,.5957f,.0625f);
        const char *title=game.won?"Level complete":"Pause";
        draw_font("CambriaBold30W",title,683-text_width("CambriaBold30W",title,30)*.5f,
            GAME_HEIGHT-554-45+7.5f,30,color(110,83,58,255));
        menu_button(1,game.won?"Next level":"Continue",429,83,0,.9189f,.2676f,.0811f,45);
        menu_button(2,"Play again",329,55,.5059f,.9463f,.2363f,.0537f,32);
    }
    /* Context feedback is transient; it does not cover the scene with a bar. */
    if (!game.paused && game.message_time>0) {
        float width=text_width("CalibriBold20W",game.message,20);
        draw_rect((GAME_WIDTH-width)*.5f-14,GAME_HEIGHT-42,width+28,30,color(10,28,40,205));
        draw_font("CalibriBold20W",game.message,(GAME_WIDTH-width)*.5f,GAME_HEIGHT-40,20,color(255,255,255,255));
    }
}
static bool job(const Entity *e)
{
    return e->kind==BARRIER || e->kind==RESOURCE || e->kind==SURVIVOR ||
        (game.early && ((e->kind==BUILDING || e->kind==SERVICE) && !e->operational)) ||
        e->kind==ANIMAL || e->kind==VALVE || (e->kind==RECRUIT && e->transport==BY_ATV);
}
static bool over_hud(void) { return inside(0,668,446,100) || inside(1010,671,356,97); }
static int hovered_object(void)
{
    if (game.paused || game.won || over_hud()) return -1;
    Vec2 p={game.pointer_x,game.pointer_y};int best=-1;float shortest=INFINITY;
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if (e->hidden || !(job(e) || e->kind==BUILD_LOT || e->kind==BUILDING || e->kind==BASE || e->kind==RECRUIT || e->kind==SERVICE || e->kind==VEHICLE) || (job(e) && e->done && e->kind!=BUILDING)) continue;
        Animation *a=animation_find(sprite_name(e->anim));if ((!a || !a->frame_count) && !e->effect) continue;
        SpriteFrame f=e->effect?(SpriteFrame){.dx=40,.dy=20,.w=80,.h=64}:a->frames[0];VisualPose pose=visual_pose(e);
        float angle=pose.angle*(float)(PI/180),c=cosf(angle),s=sinf(angle);
        float dx=p.x-pose.p.x,dy=p.y-pose.p.y,lx=c*dx-s*dy,ly=s*dx+c*dy;
        if (lx>=-f.dx-6 && lx<=f.w-f.dx+6 && ly>=-f.dy-6 && ly<=f.h-f.dy+6) {
            float d=distance(p,pose.p);if (d<shortest) best=i,shortest=d;
        }
    }
    return best;
}
static bool reachable(int target)
{
    Vec2 path[MAX_PATH];int count;float length;
    Entity *target_entity=&game.objects[target];
    if (game.early && (target_entity->effect==1 || fogged(target_entity) || !gate_open(target_entity))) return false;
    if (game.early && target_entity->transport!=ON_FOOT) {
        for (int i=0;i<game.vehicle_count;i++) {
            Worker *v=&game.vehicles[i];Entity *base=&game.objects[v->home_base];
            if (base->operational && gate_open(base) && vehicle_route(v,target,path,&count,&length)) return true;
        }
        return false;
    }
    for (int i=0;i<game.worker_count;i++) {
        Worker *w=&game.workers[i];Entity *e=&game.objects[target];
        if (w->phase==STRANDED || w->recruiting) continue;
        if ((w->boat==(!strcmp(e->anim,"boat_wood"))) && route(w,target,path,&count,&length)) return true;
    }
    return false;
}
static Vec2 build_button_position(int option)
{
    Entity *e=&game.objects[game.build_menu];
    float x=fmaxf(176,fminf(GAME_WIDTH-176,e->p.x));
    float y=fmaxf(120,fminf(570,e->p.y-90));
    const Vec2 offsets[]={{-105,34},{-35,-24},{35,-24},{105,34}};
    return (Vec2){x+offsets[option].x,y+offsets[option].y};
}
static int build_button_at(void)
{
    if (game.build_menu<0 || game.paused) return -1;
    if (game.early && game.objects[game.build_menu].kind==BUILDING)
        return distance((Vec2){game.pointer_x,game.pointer_y},build_button_position(2))<39?4:-1;
    for (int i=0;i<4;i++) if (distance((Vec2){game.pointer_x,game.pointer_y},build_button_position(i))<39) return i;
    return -1;
}
static void tooltip(const char *title,const char *detail)
{
    float w=fmaxf(text_width("CalibriBold20W",title,20),text_width("CalibriBold20W",detail,20))+28;
    float h=*detail?64:39,x=fminf(GAME_WIDTH-w-8,fmaxf(8,game.pointer_x+22));
    float y=fmaxf(8,game.pointer_y-h-20);
    /* Hint window's original nine-slice artwork, with ten-pixel end caps. */
    const float us[]={.6748f,.6846f,.9873f},vs[]={.2314f,.2412f,.3466f};
    const float uw[]={.0098f,.3027f,.0098f},vh[]={.0098f,.1054f,.0098f};
    float xs[]={x,x+10,x+w-10},ys[]={y,y+10,y+h-10};
    float ws[]={10,w-20,10},hs[]={10,h-20,10};
    for (int j=0;j<3;j++) for (int i=0;i<3;i++) gui(xs[i],ys[j],ws[i],hs[j],us[i],vs[j],uw[i],vh[j]);
    label("CalibriBold20W",title,x+w*.5f,y+h-30,25,20,true);
    if (*detail) label("CalibriBold20W",detail,x+w*.5f,y+7,25,20,true);
}
static const char *entity_title(const Entity *e)
{
    if (e->kind==BUILD_LOT) return e->assigned?build_names[e->building]:"Building slot";
    if (e->kind==BASE) return "Headquarters";
    if (e->kind==ANIMAL) return !strcmp(game.level.objects[e->source].params,"cat")?"Rescue a cat":"Rescue a dog";
    if (e->kind==VALVE) return "Valve";
    if (e->kind==SERVICE) return game.level.objects[e->source].type==114?"Pier":"Garage";
    if (e->kind==VEHICLE) return game.level.objects[e->source].type==113?"Fishing boat":"ATV";
    if (e->kind==RECRUIT) return "Trapped worker";
    if (e->kind==SURVIVOR) return "Survivor";
    if (strstr(e->anim,"stone")) return "Stone";
    if (strstr(e->anim,"tree")) return "Fallen tree";
    if (strstr(e->anim,"bridge")) return "Broken bridge";
    if (strstr(e->anim,"road")) return "Damaged road";
    if (strstr(e->anim,"house")) return "House";
    if (strstr(e->anim,"sawmill")) return "Sawmill";
    if (strstr(e->anim,"burger")) return "Burger Cafe";
    if (strstr(e->anim,"food")) return "Food";
    if (strstr(e->anim,"money")) return "Money";
    if (strstr(e->anim,"gas")) return "Fuel";
    if (strstr(e->anim,"wood")) return "Wood";
    return "Building";
}
static void draw_context(void)
{
    if (game.paused) return;
    if (game.build_menu>=0) {
        if (game.early && game.objects[game.build_menu].kind==BUILDING) {
            Vec2 p=build_button_position(2);int state=build_button_at()==4?(game.pointer_down?2:1):0;
            gui(p.x-41,p.y-41,82,82,.7441f,.9189f-state*.0801f,.0801f,.0801f);
            draw_atlas("texture/gui/gui01.png",(SDL_FRect){p.x-26,p.y-32,52,65},(SDL_FRect){.9004f,.0078f,.0508f,.5078f});
            if (build_button_at()==4) tooltip("Demolish building","Clear the lot to build something else");
            return;
        }
        static const SDL_FRect images[]={{.7891f,.0078f,.0615f,.4063f},{.6777f,.0078f,.0566f,.3906f},
            {.7354f,.0078f,.0518f,.4141f},{.6230f,.0156f,.0547f,.3750f}};
        static const SDL_FRect locks[]={{.8242f,.4375f,.0605f,.4063f},{.7129f,.4375f,.0557f,.3906f},
            {.7695f,.4375f,.0518f,.4141f},{.6572f,.4453f,.0547f,.3750f}};
        int hover=build_button_at();char text[80];
        for (int i=0;i<4;i++) {
            Vec2 p=build_button_position(i);bool afford=game.wood>=build_costs[i] && allowed_building(i);
            int state=hover==i?(game.pointer_down?2:1):0;
            gui(p.x-41,p.y-41,82,82,.7441f,.9189f-state*.0801f,.0801f,.0801f);
            draw_atlas("texture/gui/gui01.png",(SDL_FRect){p.x-30,p.y-13,60,50},afford?images[i]:locks[i]);
            snprintf(text,sizeof text,"%d",build_costs[i]);
            draw_font("CalibriBold20W",text,p.x-8-text_width("CalibriBold20W",text,20)*.5f,
                GAME_HEIGHT-p.y+13.5f,20,afford?color(255,255,255,255):color(255,85,75,255));
            gui(p.x+9,p.y-34,24,21,.9590f,.5869f,.0400f,.0342f);
        }
        if (hover>=0) {
            if (!allowed_building(hover)) snprintf(text,sizeof text,"Not available on this level");
            else snprintf(text,sizeof text,"Required: %d wood",build_costs[hover]);
            tooltip(build_names[hover],text);
        }
        return;
    }
    int hover=hovered_object();
    if (hover>=0) {
        Entity *e=&game.objects[hover];char detail[100]="";
        if (e->queued) snprintf(detail,sizeof detail,"Queued - click to cancel");
        else if (e->assigned) snprintf(detail,sizeof detail,"Worker assigned");
        else if (e->effect==1) snprintf(detail,sizeof detail,"Turn the linked valve to clear the fog");
        else if (!gate_open(e)) snprintf(detail,sizeof detail,"Stop the waste pipeline first");
        else if (e->transport==BY_ATV) snprintf(detail,sizeof detail,"Send the ATV: 10 fuel");
        else if (e->transport==BY_BOAT) snprintf(detail,sizeof detail,"Send a fishing boat: 10 fuel");
        else if (e->kind==RESOURCE) snprintf(detail,sizeof detail,"Income: %d",e->anim[strlen(e->anim)-1]=='2'?20:10);
        else if (e->kind==ANIMAL) snprintf(detail,sizeof detail,"Capture and return to headquarters: 10 food");
        else if (e->kind==VALVE) snprintf(detail,sizeof detail,"%s: 20 money",game.basin_goal?"Purify the basin":"Clear the fog");
        else if (e->kind==SERVICE) snprintf(detail,sizeof detail,"%s",e->operational?"Ready for vehicle missions":"Repair: 40 wood");
        else if (e->kind==VEHICLE) snprintf(detail,sizeof detail,"Select a rescue or fishing target to dispatch");
        else if (e->kind==BUILD_LOT) snprintf(detail,sizeof detail,"Click to choose a building");
        else if (game.early && e->kind==BUILDING && e->operational) snprintf(detail,sizeof detail,"Income: 5 every 5s; click to demolish");
        else if (game.early && (e->kind==SURVIVOR || e->kind==RECRUIT)) snprintf(detail,sizeof detail,"Clear the route to headquarters");
        else if (game.early && (e->kind==BARRIER || e->kind==BUILDING)) {
            Cost c=job_cost(e);snprintf(detail,sizeof detail,"%s: %d food, %d wood, %d money",e->kind==BUILDING?"Repair":"Required",c.food,c.wood,c.money);
        }
        else if (job(e)) snprintf(detail,sizeof detail,"%s",reachable(hover)?(e->kind==SURVIVOR?"Click to rescue":"Click to clear") : "Clear the route first");
        else if (e->kind==BASE) snprintf(detail,sizeof detail,"Helmets show available workers");
        tooltip(entity_title(e),detail);
    }
}
static void draw_progress(const Entity *target,double fraction)
{
    VisualPose pose=visual_pose(target);float x=pose.p.x,y=pose.p.y-37;
    draw_sprite("progress",x,y,0,0,1);
    SDL_Rect previous;bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&previous);
    SDL_Rect clip={(int)(x-33),(int)(GAME_HEIGHT-y-6),(int)(66*fmax(0,fmin(1,fraction))),12};
    SDL_RenderSetClipRect(renderer,&clip);draw_sprite("progress",x,y,0,1,1);
    SDL_RenderSetClipRect(renderer,clipped?&previous:NULL);
}
void game_draw(void)
{
    if (!current_game || !game.ready) return;
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==WATER && game.objects[i].effect!=3) paint(&game.objects[i]);
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==WATER && game.objects[i].effect==3) paint(&game.objects[i]);
    draw_image(game.background,0,0,GAME_WIDTH,GAME_HEIGHT,game.alpha);
    if (game.debug) {
        for (int i=0;i<game.level.traffic_count;i++) for (int j=i+1;j<game.level.traffic_count;j++) {
            Vec2 a=game.level.traffic[i],b=game.level.traffic[j];
            if (distance(a,b)<50) draw_line(a.x,GAME_HEIGHT-a.y,b.x,GAME_HEIGHT-b.y,blocked(a,b,-1)?color(255,70,100,180):color(70,220,100,180));
        }
    }
    int hover=hovered_object();
    if (!game.paused && (hover>=0 || game.build_menu>=0)) {
        int selected=game.build_menu>=0?game.build_menu:hover;Entity *e=&game.objects[selected];
        Animation *a=animation_find(sprite_name(e->anim));float w=a?fmaxf(65,a->frames[0].w+24):100;
        w=fminf(w,170);float h=w*.62f;
        int state=(e->kind==BASE || reachable(selected)) && affordable(job_cost(e)) &&
            (!game.early || e->transport==ON_FOOT || game.gas>=10)?1:0;
        VisualPose pose=visual_pose(e);
        draw_atlas("texture/gui/highlight.png",(SDL_FRect){pose.p.x-w*.5f,pose.p.y-h*.5f,w,h},
            (SDL_FRect){.0078125f,state?.140625f:.5703125f,.6171875f,.421875f});
    }
    Entity *sorted[MAX_LEVEL_OBJECTS];int count=0;
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind!=WATER && !game.objects[i].ambient) sorted[count++]=&game.objects[i];
    qsort(sorted,count,sizeof(*sorted),depth_compare);
    for (int i=0;i<count;i++) {
        Entity *e=sorted[i];if (e->hidden) { paint_log_particles(e);continue; }
        if ((e->kind==WORKER || e->kind==RECRUIT) && strncmp(e->anim,"boat",4)) draw_sprite("worker_shadow",e->p.x,e->p.y,game.time,-1,1);
        if (e->kind==SURVIVOR && !on_roof(e)) draw_sprite("man_shadow",e->p.x,e->p.y,game.time,-1,1);
        paint(e);
        paint_log_particles(e);
    }
    for (int i=0;i<game.worker_count;i++) {
        Worker *w=&game.workers[i];Entity *e=&game.objects[w->entity];
        if (w->phase==WORKING) {
            Entity *t=&game.objects[w->target];draw_progress(t,w->work/t->duration);
        }
        if (game.debug && (w->phase==OUTBOUND || w->phase==RETURNING)) for (int j=w->step;j<w->count;j++) {
            Vec2 a=j==w->step?e->p:w->path[j-1],b=w->path[j];draw_line(a.x,GAME_HEIGHT-a.y,b.x,GAME_HEIGHT-b.y,color(255,224,90,255));
        }
    }
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if (!e->hidden && (e->queued || e->assigned)) {
            bool working=false;
            for (int j=0;j<game.worker_count;j++) if (game.workers[j].target==i && game.workers[j].phase==WORKING) working=true;
            VisualPose pose=visual_pose(e);
            if (!working) draw_sprite("target",pose.p.x+22,pose.p.y-12,0,0,1);
        }
        if (e->kind==BASE) {
            int count=0,total=0;
            for (int j=0;j<game.worker_count;j++) if (game.workers[j].home_base==i && game.workers[j].phase==IDLE) total++;
            for (int j=0;j<game.worker_count;j++) if (game.workers[j].home_base==i && game.workers[j].phase==IDLE)
                draw_sprite("icon_head",e->p.x+(count++-(total-1)*.5f)*23,e->p.y+65,0,0,1);
        }
    }
    /* Type-6 scenery lives above the world, as in its original layer 9. */
    for (int i=0;i<game.object_count;i++) if (game.objects[i].ambient) paint(&game.objects[i]);
    draw_context();draw_hud();
}
void game_pointer(float x,float y,bool down)
{
    if (!current_game) return;
    game.pointer_x=x;game.pointer_y=GAME_HEIGHT-y;game.pointer_down=down;
}
void game_click(float x,float y)
{
    if (!current_game || !game.ready) return;
    game_pointer(x,y,false);
    int button=hud_button();
    if (game.won) {
        if (button==1) game_init(campaign_index(game.campaign+1));
        else if (button==2) game_key(SDLK_r);
        return;
    }
    if (button==0 || button==1) { game.paused=!game.paused;game.build_menu=-1;return; }
    if (button==2) { game_key(SDLK_r);return; }
    if (game.paused) return;
    if (inside(0,668,446,100) || inside(1010,671,356,97)) return;
    if (game.build_menu>=0) {
        int option=build_button_at();
        if (option==4) {
            Entity *building=&game.objects[game.build_menu];
            if (output_busy(building)) { announce("Wait for the supply delivery before demolishing.");return; }
            building->action=DEMOLISH;building->duration=5;
            if (dispatch(game.build_menu,false)) game.build_menu=-1;
            return;
        }
        if (option>=0) {
            Entity *lot=&game.objects[game.build_menu];
            if (!allowed_building(option)) {announce("Burger Cafes cannot be built on this level. Repair the piers and fish for food.");return;}
            if (game.wood<build_costs[option]) { announce("Not enough wood for this building.");return; }
            lot->building=option;lot->duration=5;
            if (dispatch(game.build_menu,false)) { game.wood-=build_costs[option];game.build_menu=-1; }
            return;
        }
        game.build_menu=-1;return;
    }
    int best=hovered_object();
    if (best<0) return;
    Entity *e=&game.objects[best];
    if (game.early && e->kind==BUILDING && e->operational && !e->assigned) {game.build_menu=best;return;}
    if (e->kind==BUILD_LOT && !e->assigned) { game.build_menu=best;return; }
    if (!job(e) || (e->done && e->kind!=BUILDING) || e->assigned) return;
    if (e->queued) { e->queued=false;return; }
    if (!dispatch(best,false) && reachable(best)) {
        e->queued=true;e->queue_order=++game.next_order;announce("Job queued for the next available worker.");
    }
}
void game_key(SDL_Keycode key)
{
    if (!current_game) return;
    if (game.won && (key==SDLK_RETURN || key==SDLK_n)) {game_init(campaign_index(game.campaign+1));return;}
    if (game.won && key!=SDLK_r) return;
    if (key==SDLK_ESCAPE && game.build_menu>=0) { game.build_menu=-1;return; }
    if (key==SDLK_SPACE || key==SDLK_p || key==SDLK_ESCAPE) { game.paused=!game.paused;game.build_menu=-1; }
    else if (key==SDLK_r) {
        if (!game_init(game.index)) { announce(last_error);last_error[0]=0; }
    }
    else if (key==SDLK_F1) game.debug=!game.debug;
}
int game_level_index(void) { return current_game?game.index:0; }
static bool test_frame(const char *name)
{
    SDL_SetRenderDrawColor(renderer,14,30,43,255);SDL_RenderClear(renderer);game_draw();
    if (last_error[0]) return false;
    if (getenv("VOLCANO_TEST_SHOTS")) {
        char path[256];snprintf(path,sizeof path,"build/%s.bmp",name);
        if (!renderer_screenshot(path)) return fail("Cannot save test frame %s",name);
    }
    return true;
}
static bool particle_test_pixels(const Entity *e,uint32_t *hash,bool *visible)
{
    SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);paint_log_particles(e);
    int width,height;SDL_GetRendererOutputSize(renderer,&width,&height);
    SDL_Surface *pixels=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);
    if (!pixels) return fail("Cannot allocate particle test frame");
    bool ok=SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_RGBA32,pixels->pixels,pixels->pitch)==0;
    *hash=2166136261u;*visible=false;
    if (ok) for (int y=0;y<height;y++) for (int x=0;x<width;x++) {
        const Uint8 *p=(const Uint8 *)pixels->pixels+y*pixels->pitch+x*4;
        for (int c=0;c<3;c++) { *hash=(*hash^p[c])*16777619u;if (p[c]) *visible=true; }
    }
    SDL_FreeSurface(pixels);
    return ok && !last_error[0];
}
static bool particles_selftest(void)
{
    if (!game_init(0)) return false;
    int log=-1,burning=0,unlit=0;
    uint32_t initial,paused,animated,restarted;bool visible;
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if (burning_log(e)) {
            burning++;if (log<0) log=i;
            if (!particle_test_pixels(e,&initial,&visible) || !visible)
                return fail("Fire-marked tree did not emit particles");
        } else if (e->kind==BARRIER) {
            if (!strcmp(e->anim,"tree00")) unlit++;
            if (!particle_test_pixels(e,&initial,&visible) || visible)
                return fail("Unmarked barrier emitted fire");
        }
    }
    if (log<0 || burning!=2 || unlit!=2) return fail("Expected two burning and two unlit trees in level000");
    Entity *e=&game.objects[log];
    if (!particle_test_pixels(e,&initial,&visible) || !visible || !test_frame("burning-logs"))
        return fail("Logs were not burning on level entry");
    game_key(SDLK_SPACE);game_update(.1);
    if (!particle_test_pixels(e,&paused,&visible) || paused!=initial) return fail("Paused fire changed");
    game_key(SDLK_SPACE);game_update(.1);
    if (!particle_test_pixels(e,&animated,&visible) || animated==initial) return fail("Fire did not animate");
    /* Open the route to the marked tree for the clearing lifecycle check. */
    for (int i=0;i<game.object_count;i++) if (i!=log && game.objects[i].kind==BARRIER) {
        Entity *other=&game.objects[i];other->done=other->hidden=true;
        if (burning_log(other)) other->fire_stopped=game.time;
    }
    if (!dispatch(log,true)) return fail("Cannot dispatch to the burning tree");
    for (int tick=0;tick<1000 && !e->done;tick++) game_update(.1);
    if (!e->done || !particle_test_pixels(e,&animated,&visible) || !visible || !test_frame("fire-cleared"))
        return fail("Clearing logs did not retain fading particles");
    for (int tick=0;tick<71;tick++) game_update(.1);
    if (!particle_test_pixels(e,&animated,&visible) || visible || !test_frame("smoke-cleared"))
        return fail("Cleared logs continued emitting fire/smoke");
    game_key(SDLK_r);
    if (!particle_test_pixels(&game.objects[log],&restarted,&visible) || !visible || restarted!=initial)
        return fail("Restart did not restore the initial fire");
    puts("PASS: only the two fire-marked trees burn; unmarked trees/other barriers stay unlit; fire animates, pauses, fades after clearing and resets on restart.");
    return true;
}
static bool scenery_selftest(void)
{
    if (!game_init(0)) return false;
    int bird=-1,cloud=-1,wood=-1,house=-1,land=-1,birds=0,clouds=0,roofs=0,floating=0;
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if (e->ambient==BIRD) { bird=i;birds++; }
        if (e->ambient==CLOUD) { cloud=i;clouds++; }
        if (!strcmp(e->anim,"boat_wood")) wood=i;
        if (!strcmp(e->anim,"sawmill_water")) house=i;
        if (e->kind==BASE) land=i;
        if (e->floating) floating++;
        if (e->roof) {
            roofs++;
            if (!on_roof(e)) return fail("Roof survivor has no flooded house support");
            Entity *support=&game.objects[e->support];
            const Entity *pair[]={support,e};
            if (depth_compare(&pair[0],&pair[1])>=0) return fail("Roof occupant sorts behind the house");
        }
    }
    if (birds!=3 || clouds!=2 || roofs!=4 || floating!=17 || wood<0 || house<0 || land<0)
        return fail("Unexpected scenery fixtures: birds=%d clouds=%d roofs=%d floating=%d",birds,clouds,roofs,floating);
    VisualPose b0=visual_pose(&game.objects[bird]),c0=visual_pose(&game.objects[cloud]);
    VisualPose h0=visual_pose(&game.objects[house]),w0=visual_pose(&game.objects[wood]);
    if (!test_frame("scenery-start")) return false;
    for (int tick=0;tick<10;tick++) game_update(.1);
    VisualPose b1=visual_pose(&game.objects[bird]),c1=visual_pose(&game.objects[cloud]);
    VisualPose h1=visual_pose(&game.objects[house]),w1=visual_pose(&game.objects[wood]);
    float bird_span=GAME_WIDTH+192,cloud_span=GAME_WIDTH+360;
    float bx=fmodf(b1.p.x-b0.p.x+bird_span,bird_span),cx=fmodf(c1.p.x-c0.p.x+cloud_span,cloud_span);
    if (fabsf(bx-game.objects[bird].travel_speed)>.01f || fabsf(cx-game.objects[cloud].travel_speed)>.01f)
        return fail("Birds/clouds failed to cross left to right");
    if (fabsf(h1.p.y-h0.p.y)<.01f || fabsf(h1.angle-h0.angle)<.01f ||
        fabsf(w1.p.y-w0.p.y)<.01f || fabsf(w1.angle-w0.angle)<.01f)
        return fail("Floating houses/wood did not bob and rock");
    /* A roof occupant must retain its original house-local coordinates under
     * translation and rotation, including at opposite wave extremes. */
    for (int sample=0;sample<16;sample++) {
        game.time=sample*.5;
        for (int i=0;i<game.object_count;i++) {
            Entity *e=&game.objects[i];if (!on_roof(e)) continue;
            Entity *support=&game.objects[e->support];
            VisualPose p=visual_pose(e),parent=visual_pose(support);
            float a=parent.angle*(float)(PI/180),dx=p.p.x-parent.p.x,dy=p.p.y-parent.p.y;
            if (fabsf(cosf(a)*dx-sinf(a)*dy-(e->p.x-support->p.x))>.001f ||
                fabsf(sinf(a)*dx+cosf(a)*dy-(e->p.y-support->p.y))>.001f || p.angle!=parent.angle)
                return fail("Roof survivor slid relative to the house");
            game_pointer(p.p.x,GAME_HEIGHT-p.p.y,false);
            if (hovered_object()!=i) return fail("Moving roof survivor lost its click target");
        }
        VisualPose p=visual_pose(&game.objects[land]);
        if (distance(p.p,game.objects[land].p)>.001f || p.angle) return fail("Land object bobbed");
    }
    game_pointer(-1,-1,false);
    if (!test_frame("scenery-moving")) return false;
    game_key(SDLK_SPACE);double before=game.time;game_update(.1);
    VisualPose frozen=visual_pose(&game.objects[house]);
    if (before!=game.time) return fail("Paused scenery clock advanced");
    game_key(SDLK_SPACE);VisualPose resumed=visual_pose(&game.objects[house]);
    if (distance(frozen.p,resumed.p)>.001f || frozen.angle!=resumed.angle) return fail("Paused float changed");
    /* Check both sides of the off-screen wrap and a new height each pass. */
    Entity *e=&game.objects[bird];
    double crossing=(bird_span-particle_random(e->seed+3)*bird_span)/e->travel_speed;
    game.time=crossing-.01;VisualPose exit=visual_pose(e);
    game.time=crossing+.01;VisualPose entry=visual_pose(e);
    if (exit.p.x<GAME_WIDTH+50 || entry.p.x>-50 || fabsf(entry.p.y-exit.p.y)<1)
        return fail("Bird wrap was visible or failed to choose a new height");
    e=&game.objects[cloud];
    crossing=(cloud_span-particle_random(e->seed+3)*cloud_span)/e->travel_speed;
    game.time=crossing-.01;exit=visual_pose(e);game.time=crossing+.01;entry=visual_pose(e);
    if (exit.p.x<GAME_WIDTH+128 || entry.p.x>-128) return fail("Cloud wrapped while visible");
    /* Rescue releases the roof attachment before following a worker. */
    for (int i=0;i<game.object_count;i++) if (game.objects[i].roof) {
        e=&game.objects[i];e->done=true;e->follower=0;
        VisualPose p=visual_pose(e);
        if (on_roof(e) || distance(p.p,e->p)>.001f || p.angle) return fail("Rescued survivor retained house transform");
        break;
    }
    game_key(SDLK_r);
    VisualPose restart=visual_pose(&game.objects[bird]);
    if (distance(restart.p,b0.p)>.001f) return fail("Restart did not reset ambient scenery");
    puts("PASS: left-to-right birds at changing heights, drifting clouds, off-screen wraps, water bob/rotation, roof support/draw order/hit targets, rescue detach, pause and restart.");
    return true;
}
static bool interaction_selftest(void)
{
    if (!game_init(0)) return false;
    int helmets=0,outside=0,lot=-1;
    for (int i=0;i<game.worker_count;i++) {
        Worker *w=&game.workers[i];
        if (w->home_base>=0) { if (!game.objects[w->entity].hidden) return fail("In-house worker visible as a character");helmets++; }
        else if (!w->boat && !game.objects[w->entity].hidden) outside++;
    }
    if (helmets!=2 || outside!=1) return fail("Incorrect in-house or outside worker representation");
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==BUILD_LOT) { lot=i;break; }
    if (lot<0) return fail("No build lot restored");
    Entity *t=&game.objects[lot];Vec2 p=t->p;
    game_pointer(p.x,GAME_HEIGHT-p.y,false);
    if (hovered_object()!=lot || !test_frame("hover-lot")) return fail("Build lot hover failed");
    game_click(p.x,GAME_HEIGHT-p.y);
    if (game.build_menu!=lot) return fail("Build lot click did not open menu");
    Vec2 button=build_button_position(0);game_click(button.x,GAME_HEIGHT-button.y);
    if (game.build_menu!=lot || t->assigned || game.wood) return fail("Unaffordable construction started");
    if (!test_frame("build-disabled")) return false;
    game_key(SDLK_ESCAPE);
    if (game.build_menu>=0 || game.paused) return fail("Escape failed to dismiss construction menu");
    game_click(p.x,GAME_HEIGHT-p.y);game_click(1000,600);
    if (game.build_menu>=0) return fail("Outside click failed to dismiss construction menu");
    /* Open the road to construction independently of the full-level test. */
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==BARRIER) game.objects[i].done=true;
    game.wood=100;game_click(p.x,GAME_HEIGHT-p.y);
    button=build_button_position(2);game_pointer(button.x,GAME_HEIGHT-button.y,true);
    if (build_button_at()!=2 || !test_frame("build-menu")) return fail("Building button hover failed");
    game_click(button.x,GAME_HEIGHT-button.y);
    if (!t->assigned || game.wood!=40 || game.build_menu>=0) return fail("Construction dispatch or wood cost failed");
    int builder=-1;
    for (int i=0;i<game.worker_count;i++) if (game.workers[i].target==lot) builder=i;
    if (builder<0 || game.objects[game.workers[builder].entity].hidden) return fail("Builder failed to leave HQ");
    if (!test_frame("job-checkmark")) return false;
    for (int tick=0;tick<500 && game.workers[builder].phase==OUTBOUND;tick++) game_update(.1);
    Worker *w=&game.workers[builder];
    if (w->phase!=WORKING || t->kind!=BUILD_LOT) return fail("Construction did not require work");
    for (int tick=0;tick<20;tick++) game_update(.1);
    if (!test_frame("construction-progress")) return false;
    double before=w->work;game_key(SDLK_SPACE);game_update(.1);game_key(SDLK_SPACE);
    if (w->work!=before) return fail("Paused construction advanced");
    for (int tick=0;tick<100 && w->phase==WORKING;tick++) game_update(.1);
    if (t->kind!=BUILDING || strcmp(t->anim,"house0") || w->phase!=RETURNING) return fail("Construction did not return worker immediately");
    Vec2 position=game.objects[w->entity].p;
    game_update(.1);
    if (distance(position,game.objects[w->entity].p)<.001) return fail("Builder paused before returning to HQ");
    for (int tick=0;tick<500 && w->phase!=IDLE;tick++) game_update(.1);
    if (w->phase!=IDLE || distance(w->home,game.objects[w->entity].p)>.001) return fail("Builder failed to return to its real home");
    /* Every menu option must spend its own cost and finish with its own art. */
    for (int option=0;option<4;option++) {
        if (!game_init(0)) return false;
        lot=-1;
        for (int i=0;i<game.object_count;i++) {
            if (game.objects[i].kind==BARRIER) game.objects[i].done=true;
            if (game.objects[i].kind==BUILD_LOT) lot=i;
        }
        if (lot<0) return fail("Missing lot in construction variant test");
        t=&game.objects[lot];game.wood=100;p=t->p;
        game_click(p.x,GAME_HEIGHT-p.y);button=build_button_position(option);
        game_click(button.x,GAME_HEIGHT-button.y);
        if (!t->assigned || game.wood!=100-build_costs[option]) return fail("Building variant cost failed: %d",option);
        for (int tick=0;tick<600 && t->kind==BUILD_LOT;tick++) game_update(.1);
        if (t->kind!=BUILDING || strcmp(t->anim,build_anims[option])) return fail("Building variant completion failed: %d",option);
    }
    /* Isolate one actual land worker to exercise immediate returns and queue ownership. */
    if (!game_init(0)) return false;
    game.worker_count=1;w=&game.workers[0];int barrier=-1;
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==BARRIER && dispatch(i,true)) { barrier=i;break; }
    if (barrier<0) return fail("No barrier for return regression");
    for (int tick=0;tick<500 && w->phase!=RETURNING;tick++) game_update(.1);
    if (w->phase!=RETURNING || w->target!=-1 || game.completed!=1) return fail("Completed clear did not return worker immediately");
    position=game.objects[w->entity].p;game_update(.1);
    if (distance(position,game.objects[w->entity].p)<.001) return fail("Worker paused after clearing");
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==RESOURCE && dispatch(i,true))
        return fail("Returning worker accepted a job before reaching HQ");
    for (int tick=0;tick<500 && w->phase!=IDLE;tick++) game_update(.1);
    if (w->phase!=IDLE || game.completed!=1) return fail("Clear return stalled or counted completion twice");
    int supply=-1;
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==RESOURCE && dispatch(i,true)) { supply=i;break; }
    if (supply<0 || w->phase!=OUTBOUND || distance(w->home,w->path[0])>.001) return fail("Worker could not take an order from HQ");
    for (int tick=0;tick<500 && w->phase==OUTBOUND;tick++) game_update(.1);
    if (w->phase!=RETURNING || !game.objects[supply].hidden || !w->cargo || game.food || game.wood || game.gas)
        return fail("Pickup was not immediate or inventory credited before delivery");
    int queue=-1;
    for (int i=0;i<game.object_count;i++) if (job(&game.objects[i]) && !game.objects[i].done && reachable(i)) { queue=i;break; }
    if (queue<0) return fail("No queue regression target");
    p=game.objects[queue].p;game_click(p.x,GAME_HEIGHT-p.y);
    if (!game.objects[queue].queued) return fail("Busy-worker click did not queue job");
    game_click(p.x,GAME_HEIGHT-p.y);
    if (game.objects[queue].queued) return fail("Queued job could not be cancelled");
    game_click(p.x,GAME_HEIGHT-p.y);
    for (int tick=0;tick<500 && !game.objects[queue].assigned;tick++) game_update(.1);
    if (!game.objects[queue].assigned || game.objects[queue].queued || game.completed!=2 || !(game.food || game.wood || game.gas))
        return fail("Queued job did not dispatch after cargo delivery");
    if (distance(w->home,w->path[0])>.001) return fail("Chained worker returned to previous worksite instead of HQ");
    if (!game_init(0)) return false;
    puts("PASS: construction menu/costs/work, authored feedback, HQ helmets, instant pickups, cargo delivery, immediate HQ returns and cancellable queued jobs.");
    return true;
}
#include "campaign_checks.h"
#include "missions_4_6_checks.h"
bool game_selftest(void)
{
    if (!game_init(0)) return false;
    if (game.level.traffic_count!=110 || game.worker_count!=5 || game.barriers!=10 || game.resources!=15 || game.survivors!=6)
        return fail("Unexpected original level000 counts: traffic=%d workers=%d barriers=%d resources=%d survivors=%d",game.level.traffic_count,game.worker_count,game.barriers,game.resources,game.survivors);
    double before=game.time;game_key(SDLK_SPACE);game_update(0.1);
    if (game.time!=before) return fail("Pause advanced simulation");
    game_key(SDLK_SPACE);
    bool found_block=false;
    for (int i=0;i<game.level.traffic_count;i++) for (int j=i+1;j<game.level.traffic_count;j++)
        if (distance(game.level.traffic[i],game.level.traffic[j])<50 && blocked(game.level.traffic[i],game.level.traffic[j],-1)) found_block=true;
    if (!found_block) return fail("Uncleared barriers do not block traffic edges");
    int sample=-1;
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==BARRIER && dispatch(i,true)) { sample=i;break; }
    if (sample<0) return fail("No initial barrier is reachable");
    if (dispatch(sample,true)) return fail("A job was assigned twice");
    Vec2 positions[MAX_WORKERS];
    for (int i=0;i<game.worker_count;i++) positions[i]=game.objects[game.workers[i].entity].p;
    game_update(0.1);
    bool moved=false;
    for (int i=0;i<game.worker_count;i++) if (distance(positions[i],game.objects[game.workers[i].entity].p)>0.01) moved=true;
    if (!moved || game.objects[sample].done) return fail("Movement did not interpolate before timed work");
    before=game.time;game_key(SDLK_SPACE);game_update(0.1);game_key(SDLK_SPACE);
    if (game.time!=before) return fail("Pause failed during an active job");
    bool boat_used=false;
    for (int tick=0;tick<30000 && game.completed<game.target_count;tick++) {
        bool active=false;
        for (int i=0;i<game.object_count;i++) {
            Entity *e=&game.objects[i];
            if ((e->kind==BARRIER || e->kind==RESOURCE || e->kind==SURVIVOR) && dispatch(i,true)) active=true;
        }
        for (int i=0;i<game.worker_count;i++) if (game.workers[i].phase!=IDLE) {
            active=true;if (game.workers[i].boat) boat_used=true;
        }
        if (!active) break;
        game_update(0.1);
    }
    if (game.completed!=game.target_count) {
        for (int i=0;i<game.object_count;i++) {
            Entity *e=&game.objects[i];
            if ((e->kind==BARRIER || e->kind==RESOURCE || e->kind==SURVIVOR) && !e->done)
                return fail("Only %d/%d jobs completed; first unreachable '%s' at %.1f,%.1f",game.completed,game.target_count,e->anim,e->p.x,e->p.y);
        }
        return fail("Workers failed to return: %d/%d jobs",game.completed,game.target_count);
    }
    if (game.rescued!=game.survivors || game.cleared!=game.barriers || game.collected!=game.resources) return fail("Gameplay progress counters disagree");
    if (!boat_used || game.food<=0 || game.wood<=0 || game.gas<=0) return fail("Boat dispatch or resource inventory failed");
    for (int i=0;i<game.level.traffic_count;i++) for (int j=i+1;j<game.level.traffic_count;j++)
        if (distance(game.level.traffic[i],game.level.traffic[j])<50 && blocked(game.level.traffic[i],game.level.traffic[j],-1)) return fail("Cleared barriers still block the graph");
    for (int i=0;i<game.worker_count;i++) {
        Worker *w=&game.workers[i];if (w->phase!=IDLE || distance(game.objects[w->entity].p,w->home)>0.01) return fail("Worker failed to return home");
    }
    printf("PASS: original level000, %d graph points, %d completed jobs (%d rescues); movement, work, pause, resource collection and return home.\n",game.level.traffic_count,game.completed,game.rescued);
    if (!game_init(0)) return false;
    if (game.completed || game.food || game.wood || game.gas || game.rescued) return fail("Restart did not reset progress");
    /* Exercise retail regions in the same logical coordinates as SDL input. */
    game_pointer(42,46,false);game_draw(); /* hover */
    game_pointer(42,46,true);game_draw();  /* pressed */
    game_click(42,46);
    before=game.time;game_update(.1);
    if (!game.paused || game.time!=before) return fail("Retail pause button failed");
    game_pointer(683,298,false);game_draw();
    game_pointer(683,298,true);game_draw();
    game_click(683,298);
    if (game.paused) return fail("Retail Continue button failed");
    game_update(.1);
    game_click(42,46);game_pointer(683,411,true);game_draw();game_click(683,411);
    if (game.paused || game.time || game.completed) return fail("Retail Play again button failed");
    game_key(SDLK_ESCAPE);
    if (!game.paused) return fail("Escape did not open the retail pause menu");
    game_key(SDLK_ESCAPE);
    if(last_error[0])return false;
    puts("PASS: retail HUD fonts/atlas states, pause, Continue, Play again and Escape controls.");
    return particles_selftest() && scenery_selftest() && interaction_selftest() && campaign_selftest() && missions_4_6_selftest();
}
