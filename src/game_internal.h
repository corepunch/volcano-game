#ifndef GAME_INTERNAL_H
#define GAME_INTERNAL_H
#include "volcano.h"

/* Private game-module contract. Runtime helpers always receive their world. */
#define MAX_WORKERS 64
#define MAX_PATH (MAX_TRAFFIC_POINTS+2)
#define PI 3.14159265358979323846
typedef enum { DECOR, WATER, BASE, BARRIER, RESOURCE, SURVIVOR, WORKER, BUILD_LOT, BUILDING, RECRUIT, ANIMAL, VALVE, SERVICE, VEHICLE, ENTITY_KIND_COUNT } EntityKind;
enum { ON_FOOT, BY_ATV, BY_BOAT };
enum { IDLE, OUTBOUND, WORKING, RETURNING, STRANDED };
enum { BUILD, REPAIR, DEMOLISH };
enum { NO_AMBIENT, BIRD, CLOUD };
typedef enum { RESOURCE_NONE, RESOURCE_FOOD, RESOURCE_FUEL, RESOURCE_MONEY, RESOURCE_WOOD } ResourceKind;
typedef struct { int food,wood,money; } Cost;
typedef struct {
    const char *key,*name,*animation,*bonus;
    int cost;
    ResourceKind resource;
} BuildingDef;
enum { BUILDING_COUNT=4, TREE_INCOME=20,PRODUCTION_INCOME=5,PRODUCTION_INTERVAL=5 };
typedef struct {
    const char *match;
    Cost cost;
    int wood_reward,clear_frame;
    bool burns,needs_dynamite;
} ObstacleDef;
typedef struct {
    Vec2 p,workpoint,origin,roam;
    char anim[128];
    EntityKind kind;
    int order,frame,effect,source,owner,output,action;
    double duration;
    double fire_stopped;
    bool done,hidden,assigned,queued,fire,floating,roof,operational;
    uint32_t seed;
    int ambient,support,visual;
    float travel_speed;
    int queue_order,building;
    int follower;
    int gate,transport,roam_step,checked_route;
    bool roaming;
    double income_clock,bonus_clock;
    /* Resolved at spawn: changing presentation never changes costs or cargo. */
    const ObstacleDef *obstacle;
    ResourceKind pickup_resource;
    int pickup_value;
    bool boat_worker;
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
    bool paused,debug,ready,early,won,routes_dirty,allow_cafe,local_background;
    float pointer_x,pointer_y;
    bool pointer_down;
    char message[256],background[PATH_CAPACITY],alpha[PATH_CAPACITY];
    char custom_path[PATH_CAPACITY];
    MapScene scene;
} Game;

typedef struct { Vec2 p;float angle; } VisualPose;
extern const BuildingDef game_buildings[BUILDING_COUNT];
enum { JOB_CLICK=1, JOB_REPAIR=2, JOB_ATV=4 };
typedef struct {
    double duration;
    const char *work_animation;
    bool keep_target;
    void (*complete)(Game *g,Entity *target,Worker *worker);
    Cost cost;
    unsigned flags;
} JobDef;
const JobDef *game_job_definition(EntityKind kind);
const char *game_work_animation(const Entity *target);
const ObstacleDef *game_obstacle_definition(const char *name);
void game_set_pickup(Entity *e,const char *name,const char *source_name);
void game_apply_mission_rules(Game *g,int campaign);
int game_building_type(const char *name);

Game *game_current(void);

/* game_util.c */
float game_distance(Vec2 a,Vec2 b);
SDL_Color game_color(int r,int g,int b,int a);
bool game_fail(const char *format,...);
void game_announce(Game *g, const char *text);
float game_particle_random(uint32_t seed);

/* game_rules.c */
const char *game_sprite_name(const char *name);
Cost game_job_cost(Game *g, const Entity *e);
bool game_affordable(Game *g, Cost c);
void game_spend(Game *g, Cost c);
void game_credit(Game *g, int resource,int value);
int game_building_resource(int type);
void game_building_art(Entity *e);
bool game_available(const Worker *w);
bool game_gate_open(Game *g, const Entity *e);
bool game_fogged(Game *g, const Entity *e);
bool game_allowed_building(Game *g, int type);
bool game_burning_log(const Entity *e);
bool game_job(Game *g, const Entity *e);

/* game_spawn.c */
bool game_load_level(Game *g,int index,const char *custom_path);

/* game_navigation.c */
int game_closest(Game *g, Vec2 p);
bool game_blocked(Game *g, Vec2 a,Vec2 b,int target);
bool game_route_to(Game *g, const Worker *w,Vec2 destination,int target,Vec2 *path,int *count,float *length);
bool game_route(Game *g, const Worker *w,int target,Vec2 *path,int *count,float *length);
bool game_vehicle_route(Game *g, const Worker *w,int target,Vec2 *path,int *count,float *length);
bool game_reachable(Game *g, int target);

/* game_jobs.c */
void game_waiting_detail(Game *g,int target,char *text,size_t size);
bool game_dispatch(Game *g, int target,bool quiet);
void game_return_home(Game *g, Worker *w);
void game_finish_work(Game *g, int worker);
void game_arrive_home(Game *g, Worker *w);
bool game_output_busy(Game *g, const Entity *building);

/* game_sim.c */
void game_tick(Game *g, double dt);

/* game_render.c */
bool game_on_roof(const Entity *e);
VisualPose game_visual_pose(Game *g, const Entity *e);
int game_depth_compare(Game *g, const void *a,const void *b);
void game_paint_log_particles(Game *g, const Entity *e);
bool game_inside(Game *g, float x,float y,float w,float h);
int game_hud_button(Game *g);
int game_hovered_object(Game *g);
Vec2 game_build_button_position(Game *g, int option);
int game_build_button_at(Game *g);
void game_render(Game *g);

/* game_input.c: lifecycle requests are applied by game.c after input returns. */
typedef enum { GAME_COMMAND_NONE, GAME_COMMAND_RESTART, GAME_COMMAND_NEXT } GameCommand;
void game_set_pointer(Game *g, float x,float y,bool down);
GameCommand game_handle_click(Game *g, float x,float y);
GameCommand game_handle_key(Game *g, SDL_Keycode key);

#endif
