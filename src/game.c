#include "volcano.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define MAX_WORKERS 64
#define MAX_PATH (MAX_TRAFFIC_POINTS+2)
#define PI 3.14159265358979323846
enum { DECOR, WATER, BASE, BARRIER, RESOURCE, SURVIVOR, WORKER, BUILD_LOT, BUILDING };
enum { IDLE, OUTBOUND, WORKING, WAITING, RETURNING };
typedef struct {
    Vec2 p;
    char anim[128];
    int kind,order,frame;
    double duration;
    bool done,hidden,assigned,queued;
    int queue_order,building;
    int follower;
} Entity;
typedef struct {
    int entity,phase,target,step,count,direction,home_base,cargo,value;
    bool boat;
    Vec2 home,path[MAX_PATH];
    double work,wait;
} Worker;
typedef struct {
    Level level;
    Entity objects[MAX_LEVEL_OBJECTS];
    int object_count,worker_count,target_count,barriers,resources,survivors;
    int cleared,collected,rescued,completed,food,wood,money,gas,index,build_menu,next_order;
    Worker workers[MAX_WORKERS];
    double time,message_time;
    bool paused,debug,ready;
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
static const char *sprite_name(const char *name)
{
    static const char *aliases[][2]={
        {"food1","food"},{"food2","food_big"},{"wood1","wood"},{"wood2","wood_big"},
        {"gas1","gas"},{"gas2","gas_big"},{"tree00","bar_tree00"},{"stone00","bar_stone00"},
        {"burger","burger0"},{"sawmill","sawmill0"},{"heliport","heliport_br"}
    };
    for (size_t i=0;i<sizeof(aliases)/sizeof(*aliases);i++) if (!strcmp(name,aliases[i][0])) return aliases[i][1];
    return name;
}
static int add_entity(int kind,const LevelObject *raw,const char *anim)
{
    if (game.object_count>=MAX_LEVEL_OBJECTS) return -1;
    int index=game.object_count++;
    Entity *e=&game.objects[index];
    e->kind=kind;e->p=(Vec2){raw->x,raw->y};e->frame=-1;e->follower=-1;e->order=index;
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
    return route_to(w,game.objects[target].p,target,path,count,length);
}
static const char *build_anims[]={"sawmill0","burger0","house0","gas0"};
static const char *build_names[]={"Sawmill","Burger Cafe","House","Gas Station"};
static const int build_costs[]={40,40,60,80};
static bool available(const Worker *w) { return w->phase==IDLE || w->phase==WAITING; }
static bool dispatch(int target,bool quiet)
{
    Entity *t=&game.objects[target];
    if (game.paused || t->done || t->assigned) return false;
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
    Worker *w=&game.workers[chosen];
    w->phase=OUTBOUND;w->target=target;w->step=1;w->count=best_count;
    memcpy(w->path,best,best_count*sizeof(Vec2));
    t->assigned=true;t->queued=false;w->wait=0;
    game.objects[w->entity].hidden=false;
    if (!quiet) announce(t->kind==BARRIER?"Worker dispatched to clear the route.":
        t->kind==SURVIVOR?"Worker dispatched to escort a survivor.":t->kind==BUILD_LOT?"Worker dispatched to build.":"Collecting supplies.");
    return true;
}

bool game_init(int index)
{
    char path[PATH_CAPACITY];snprintf(path,sizeof(path),"%s/level/level%03d.lvl",asset_root,index);
    /* Helpers build against the candidate; commit only after full validation. */
    Game *candidate=calloc(1,sizeof(*candidate)),*previous=current_game;
    if (!candidate) return fail("Cannot allocate gameplay state");
    if (!level_load(path,&candidate->level,last_error,1024)) { free(candidate);return false; }
    current_game=candidate;
    game.index=index;game.build_menu=-1;game.pointer_x=game.pointer_y=-100;
    snprintf(game.background,sizeof(game.background),"texture/back/%s_0.jpg",game.level.background);
    snprintf(game.alpha,sizeof(game.alpha),"texture/back/%s_0.alpha",game.level.background);
    for (int i=0;i<game.level.object_count;i++) {
        LevelObject *raw=&game.level.objects[i];int kind=DECOR;char name[128]={0};
        switch (raw->type) {
        case 2: decor_name(raw->params,name,sizeof(name));if (!strcmp(name,"water0")) kind=WATER;break;
        case 102: kind=BARRIER;snprintf(name,sizeof(name),"%s",raw->params);break;
        case 108: kind=RESOURCE;snprintf(name,sizeof(name),"%s",raw->params);break;
        case 110: kind=BASE;snprintf(name,sizeof(name),"base0");break;
        case 104: case 113: kind=WORKER;snprintf(name,sizeof(name),"%s",raw->type==113?"boat0":"worker_wait0");break;
        case 109: kind=SURVIVOR;snprintf(name,sizeof(name),"man_idle0");break;
        case 125: snprintf(name,sizeof(name),"firefighter_idle0");break;
        case 6:
            snprintf(name,sizeof(name),"%s",raw->params);
            { char *end=strchr(name,';');if (end) *end=0; }
            break;
        case 142: snprintf(name,sizeof(name),"delivery_place0");break;
        case 146: kind=BUILD_LOT;snprintf(name,sizeof(name),"build_place");break;
        case 101: kind=BUILDING;snprintf(name,sizeof(name),"%s",raw->params);break;
        case 112: case 114: case 126: case 147:
            snprintf(name,sizeof(name),"%s",raw->params);break;
        default: continue;
        }
        if (!name[0]) continue;
        if (!animation_find(sprite_name(name))) {
            fail("Missing original animation '%s' for object %u",sprite_name(name),raw->id);goto restore;
        }
        int id=add_entity(kind,raw,name);if (id<0) { fail("Too many game objects");goto restore; }
        Entity *e=&game.objects[id];
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
    if (!game.worker_count || !game.level.traffic_count) {
        fail("Original level %d has no prototype workers or traffic graph",index);goto restore;
    }
    for (int i=0;i<game.worker_count;i++) {
        Worker *w=&game.workers[i];
        if (!w->boat) for (int j=0;j<game.object_count;j++)
            if (game.objects[j].kind==BASE && distance(w->home,game.objects[j].p)<80) { w->home_base=j;break; }
        game.objects[w->entity].hidden=w->home_base>=0;
    }
    game.ready=true;announce("Click debris, supplies or survivors to dispatch a worker.");
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
static void finish_work(int worker)
{
    Worker *w=&game.workers[worker];Entity *t=&game.objects[w->target];t->done=true;t->assigned=false;
    if (t->kind==BARRIER) {
        game.cleared++;game.completed++;
        if (strstr(t->anim,"bridge")) t->frame=3;else t->hidden=true;
    } else if (t->kind==BUILD_LOT) {
        t->kind=BUILDING;t->frame=0;
        snprintf(t->anim,sizeof t->anim,"%s",build_anims[t->building]);
    } else if (t->kind==RESOURCE) {
        w->value=t->anim[strlen(t->anim)-1]=='2'?20:10;
        w->cargo=strstr(t->anim,"food")?1:strstr(t->anim,"gas")?2:strstr(t->anim,"money")?3:4;
        t->hidden=true;game.collected++;
    } else t->follower=worker;
    if (t->kind==BARRIER || t->kind==BUILDING) {
        w->phase=WAITING;w->wait=1.5;w->target=-1;
        snprintf(game.objects[w->entity].anim,128,"%s",w->boat?"boat0":"worker_wait0");
    } else return_home(w);
}
static void arrive_home(Worker *w)
{
    Entity *e=&game.objects[w->entity];
    if (w->target>=0) {
        Entity *t=&game.objects[w->target];
        if (t->kind==SURVIVOR) { t->hidden=true;t->follower=-1;game.rescued++; }
        if (w->cargo==1) game.food+=w->value;
        else if (w->cargo==2) game.gas+=w->value;
        else if (w->cargo==3) game.money+=w->value;
        else if (w->cargo==4) game.wood+=w->value;
        game.completed++;
    }
    e->p=w->home;e->hidden=w->home_base>=0;w->phase=IDLE;w->target=-1;w->cargo=w->value=0;
    snprintf(e->anim,sizeof(e->anim),"%s",w->boat?"boat0":"worker_wait0");
    if (game.completed==game.target_count) announce("Prototype level complete. All routes cleared and survivors escorted!");
}
void game_update(double dt)
{
    if (!current_game || !game.ready || game.paused) return;
    dt=fmax(0,fmin(dt,0.1));game.time+=dt;game.message_time=fmax(0,game.message_time-dt);
    for (int i=0;i<game.worker_count;i++) {
        Worker *w=&game.workers[i];Entity *e=&game.objects[w->entity];
        if (w->phase==WAITING) {
            w->wait=fmax(0,w->wait-dt);
            if (w->wait<=0) return_home(w);
        }
        else if (w->phase==WORKING) { w->work+=dt;if (w->work>=game.objects[w->target].duration) finish_work(i); }
        else if (w->phase==OUTBOUND || w->phase==RETURNING) {
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
                    if (!w->boat) snprintf(e->anim,sizeof(e->anim),"%s%d",(game.objects[w->target].kind==BARRIER || game.objects[w->target].kind==BUILD_LOT)?"worker_srepair":"worker_gather",w->direction);
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
            e->p=(Vec2){p.x-cosf(angle)*17,p.y-sinf(angle)*17};snprintf(e->anim,sizeof(e->anim),"man_run%d",w->direction);
        }
    }
}
static int depth_compare(const void *a,const void *b)
{
    const Entity *aa=*(const Entity *const *)a,*bb=*(const Entity *const *)b;
    return aa->p.y==bb->p.y?aa->order-bb->order:aa->p.y>bb->p.y?-1:1;
}
static void paint(const Entity *e)
{
    if (!e->hidden) draw_sprite(sprite_name(e->anim),e->p.x,e->p.y,game.time,e->frame,1);
}
static bool inside(float x,float y,float w,float h)
{
    return game.pointer_x>=x && game.pointer_x<x+w && game.pointer_y>=y && game.pointer_y<y+h;
}
static int hud_button(void)
{
    if (game.paused) {
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
    const int remaining[]={game.survivors-game.rescued,game.barriers-game.cleared,game.resources-game.collected};
    const char *tasks[]={"Rescue people","Clear obstructions","Collect supplies"};
    for (int i=0;i<3;i++) {
        float y=741-21*i;bool done=remaining[i]==0;
        if (done) gui(1036,y,21,22,.8535f,.7373f,.0205f,.0215f);
        else gui(1035,y,20,21,.8330f,.7373f,.0195f,.0205f);
        if (done) snprintf(text,sizeof text,"%s: Done!",tasks[i]);
        else snprintf(text,sizeof text,"%s: %d",tasks[i],remaining[i]);
        label(done?"CalibriBold20G":"CalibriBold20W",text,1059,738-21*i,25,20,false);
    }
    if (game.paused) {
        draw_rect(0,0,GAME_WIDTH,GAME_HEIGHT,color(0,0,0,130));
        /* Retail pause panel and header from gui/escape.lua. */
        gui(367,154,632,461,0,0,.6172f,.4502f);
        gui(378,542,610,64,.0029f,.4502f,.5957f,.0625f);
        draw_font("CambriaBold30W","Pause",683-text_width("CambriaBold30W","Pause",30)*.5f,
            GAME_HEIGHT-554-45+7.5f,30,color(110,83,58,255));
        menu_button(1,"Continue",429,83,0,.9189f,.2676f,.0811f,45);
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
    return e->kind==BARRIER || e->kind==RESOURCE || e->kind==SURVIVOR;
}
static bool over_hud(void) { return inside(0,668,446,100) || inside(1010,671,356,97); }
static int hovered_object(void)
{
    if (game.paused || over_hud()) return -1;
    Vec2 p={game.pointer_x,game.pointer_y};int best=-1;float shortest=INFINITY;
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if (e->hidden || !(job(e) || e->kind==BUILD_LOT || e->kind==BUILDING || e->kind==BASE) || (job(e) && e->done)) continue;
        Animation *a=animation_find(sprite_name(e->anim));if (!a || !a->frame_count) continue;
        SpriteFrame f=a->frames[0];
        if (p.x>=e->p.x-f.dx-6 && p.x<=e->p.x-f.dx+f.w+6 && p.y>=e->p.y-f.dy-6 && p.y<=e->p.y-f.dy+f.h+6) {
            float d=distance(p,e->p);if (d<shortest) best=i,shortest=d;
        }
    }
    return best;
}
static bool reachable(int target)
{
    Vec2 path[MAX_PATH];int count;float length;
    for (int i=0;i<game.worker_count;i++) {
        Worker *w=&game.workers[i];Entity *e=&game.objects[target];
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
        static const SDL_FRect images[]={{.7891f,.0078f,.0615f,.4063f},{.6777f,.0078f,.0566f,.3906f},
            {.7354f,.0078f,.0518f,.4141f},{.6230f,.0156f,.0547f,.3750f}};
        static const SDL_FRect locks[]={{.8242f,.4375f,.0605f,.4063f},{.7129f,.4375f,.0557f,.3906f},
            {.7695f,.4375f,.0518f,.4141f},{.6572f,.4453f,.0547f,.3750f}};
        int hover=build_button_at();char text[80];
        for (int i=0;i<4;i++) {
            Vec2 p=build_button_position(i);bool afford=game.wood>=build_costs[i];
            int state=hover==i?(game.pointer_down?2:1):0;
            gui(p.x-41,p.y-41,82,82,.7441f,.9189f-state*.0801f,.0801f,.0801f);
            draw_atlas("texture/gui/gui01.png",(SDL_FRect){p.x-30,p.y-13,60,50},afford?images[i]:locks[i]);
            snprintf(text,sizeof text,"%d",build_costs[i]);
            draw_font("CalibriBold20W",text,p.x-8-text_width("CalibriBold20W",text,20)*.5f,
                GAME_HEIGHT-p.y+13.5f,20,afford?color(255,255,255,255):color(255,85,75,255));
            gui(p.x+9,p.y-34,24,21,.9590f,.5869f,.0400f,.0342f);
        }
        if (hover>=0) {
            snprintf(text,sizeof text,"Required: %d wood",build_costs[hover]);tooltip(build_names[hover],text);
        }
        return;
    }
    int hover=hovered_object();
    if (hover>=0) {
        Entity *e=&game.objects[hover];char detail[100]="";
        if (e->queued) snprintf(detail,sizeof detail,"Queued - click to cancel");
        else if (e->assigned) snprintf(detail,sizeof detail,"Worker assigned");
        else if (e->kind==RESOURCE) snprintf(detail,sizeof detail,"Income: %d",e->anim[strlen(e->anim)-1]=='2'?20:10);
        else if (e->kind==BUILD_LOT) snprintf(detail,sizeof detail,"Click to choose a building");
        else if (job(e)) snprintf(detail,sizeof detail,"%s",reachable(hover)?(e->kind==SURVIVOR?"Click to rescue":"Click to clear") : "Clear the route first");
        else if (e->kind==BASE) snprintf(detail,sizeof detail,"Helmets show available workers");
        tooltip(entity_title(e),detail);
    }
}
static void draw_progress(const Entity *target,double fraction)
{
    float x=target->p.x,y=target->p.y-37;
    draw_sprite("progress",x,y,0,0,1);
    SDL_Rect previous;bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&previous);
    SDL_Rect clip={(int)(x-33),(int)(GAME_HEIGHT-y-6),(int)(66*fmax(0,fmin(1,fraction))),12};
    SDL_RenderSetClipRect(renderer,&clip);draw_sprite("progress",x,y,0,1,1);
    SDL_RenderSetClipRect(renderer,clipped?&previous:NULL);
}
void game_draw(void)
{
    if (!current_game || !game.ready) return;
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==WATER) paint(&game.objects[i]);
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
        int state=(e->kind==BASE || e->kind==BUILDING || reachable(selected))?1:0;
        draw_atlas("texture/gui/highlight.png",(SDL_FRect){e->p.x-w*.5f,e->p.y-h*.5f,w,h},
            (SDL_FRect){.0078125f,state?.140625f:.5703125f,.6171875f,.421875f});
    }
    Entity *sorted[MAX_LEVEL_OBJECTS];int count=0;
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind!=WATER) sorted[count++]=&game.objects[i];
    qsort(sorted,count,sizeof(*sorted),depth_compare);
    for (int i=0;i<count;i++) {
        Entity *e=sorted[i];if (e->hidden) continue;
        if (e->kind==WORKER && strncmp(e->anim,"boat",4)) draw_sprite("worker_shadow",e->p.x,e->p.y,game.time,-1,1);
        if (e->kind==SURVIVOR) draw_sprite("man_shadow",e->p.x,e->p.y,game.time,-1,1);
        paint(e);
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
            if (!working) draw_sprite("target",e->p.x+22,e->p.y-12,0,0,1);
        }
        if (e->kind==BASE) {
            int count=0,total=0;
            for (int j=0;j<game.worker_count;j++) if (game.workers[j].home_base==i && game.workers[j].phase==IDLE) total++;
            for (int j=0;j<game.worker_count;j++) if (game.workers[j].home_base==i && game.workers[j].phase==IDLE)
                draw_sprite("icon_head",e->p.x+(count++-(total-1)*.5f)*23,e->p.y+65,0,0,1);
        }
    }
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
    if (button==0 || button==1) { game.paused=!game.paused;game.build_menu=-1;return; }
    if (button==2) { game_key(SDLK_r);return; }
    if (game.paused) return;
    if (inside(0,668,446,100) || inside(1010,671,356,97)) return;
    if (game.build_menu>=0) {
        int option=build_button_at();
        if (option>=0) {
            Entity *lot=&game.objects[game.build_menu];
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
    if (e->kind==BUILD_LOT && !e->assigned) { game.build_menu=best;return; }
    if (!job(e) || e->done || e->assigned) return;
    if (e->queued) { e->queued=false;return; }
    if (!dispatch(best,false) && reachable(best)) {
        e->queued=true;e->queue_order=++game.next_order;announce("Job queued for the next available worker.");
    }
}
void game_key(SDL_Keycode key)
{
    if (!current_game) return;
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
    if (t->kind!=BUILDING || strcmp(t->anim,"house0") || w->phase!=WAITING) return fail("Construction completion failed");
    Vec2 position=game.objects[w->entity].p;
    for (int tick=0;tick<14;tick++) game_update(.1);
    if (w->phase!=WAITING || distance(position,game.objects[w->entity].p)>.001) return fail("Worker did not wait for 1.5 seconds");
    game_update(.1);game_update(.1);
    if (w->phase!=RETURNING) return fail("Worker did not return after waiting");
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
    /* Isolate one actual land worker to exercise chaining and queue ownership. */
    if (!game_init(0)) return false;
    game.worker_count=1;w=&game.workers[0];int barrier=-1;
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==BARRIER && dispatch(i,true)) { barrier=i;break; }
    if (barrier<0) return fail("No barrier for waiting regression");
    for (int tick=0;tick<500 && w->phase!=WAITING;tick++) game_update(.1);
    if (w->phase!=WAITING || game.completed!=1) return fail("Completed clear did not release worker");
    position=game.objects[w->entity].p;int supply=-1;
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==RESOURCE && dispatch(i,true)) { supply=i;break; }
    if (supply<0 || w->phase!=OUTBOUND || distance(position,w->path[0])>.001) return fail("Worker could not take an order while waiting");
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
    puts("PASS: construction menu/costs/work, authored feedback, HQ helmets, instant pickups, cargo delivery, 1.5-second wait, chaining and cancellable queued jobs.");
    return true;
}
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
    return interaction_selftest();
}
