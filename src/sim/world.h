#ifndef VOLCANO_WORLD_H
#define VOLCANO_WORLD_H
/* Simulation contract. No SDL: the world advances in fixed ticks, refers to
 * content by archetype and never stores sprite names. Presentation derives
 * art from archetype + state (appearance.c), which is also how footprints
 * are measured. */
#include "../content/content.h"

#define TICK_SECONDS (1.0/60.0)
#define CROWD_RADIUS 80     /* Workers closer than this to HQ start inside it. */
#define LINK_DISTANCE 50    /* Traffic points strictly closer than this connect. */
#define BARRIER_RADIUS 20   /* Live obstacles block segments passing closer than this. */

typedef enum { DECOR,WATER,BASE,BARRIER,RESOURCE,SURVIVOR,WORKER,BUILD_LOT,BUILDING,RECRUIT,ANIMAL,VALVE,SERVICE,VEHICLE,ENTITY_KIND_COUNT } EntityKind;
enum { IDLE,OUTBOUND,WORKING,RETURNING,STRANDED };
enum { REPAIR,DEMOLISH };
typedef enum { POSE_IDLE,POSE_MOVE,POSE_CARRY,POSE_WORK,POSE_RUN,POSE_ACT } PoseAction;

typedef struct {
    /* Identity. */
    uint32_t id;
    int order,source;               /* Draw tie-break and placement index (equal at spawn). */
    EntityKind kind;
    const Archetype *type;          /* Current archetype: lots and producers swap at runtime. */
    const Archetype *placed;        /* Archetype the level placed, restored when rebuilt alike. */
    const char *sprite;             /* Per-placement art override (scenery, retail scenes). */
    uint32_t layer;
    /* Position. */
    Vec2 p,origin,workpoint,roam;
    bool has_workpoint;
    /* Appearance state. */
    PoseAction pose;
    const char *pose_action;        /* Work action key while in POSE_WORK. */
    int direction,variant,frame;    /* frame -1 animates. */
    bool hidden,floating,roof,constructing;
    /* Jobs. */
    bool done,assigned,queued,operational;
    int queue_order,action;         /* action: REPAIR or DEMOLISH for producers. */
    const Archetype *building;      /* Lot: producer being built or chosen. */
    /* Relations (entity indices, -1 none). */
    int owner,output,gate,support,follower;
    /* Behaviour. */
    Transport transport;
    bool fire;
    double fire_stopped;
    double income_clock,bonus_clock;
    bool roaming;
    int roam_step,checked_route;
    float travel_speed;
    bool has_corridor;
    Vec2 corridor;
    float corridor_radius;
} Entity;

/* A moving agent: crew member, evacuee or vehicle. */
typedef struct {
    int entity,phase,target,step,count,direction,home_base,value;
    Resource cargo;
    bool boat,recruiting,atv;
    Vec2 home;
    Vec2 *path;                     /* Capacity: traffic points + 3. */
    double work;
} Worker;

typedef struct {
    LevelSpec spec;                 /* Owned: placements, traffic, effects, local archetypes. */
    const Ruleset *rules;
    Mission mission;
    const Archetype *lot;           /* Archetype a demolished producer becomes. */
    Entity *objects;
    int object_count;
    const Vec2 *traffic;
    int traffic_count;
    int *links,*link_start;         /* Traffic adjacency (CSR): neighbours of i are links[link_start[i]..link_start[i+1]). */
    Worker *workers,*evacuees,*vehicles;
    int worker_count,evacuee_count,vehicle_count;
    Stock stock;
    int target_count,barriers,resources,survivors,cleared,collected,rescued,completed;
    int goal,animals,animals_rescued,purified,basin_goal;
    int next_order,transport_trips,fishing_trips,route_epoch;
    double time,accumulator;
    uint64_t ticks;
    bool paused,won,routes_dirty,ready;
    /* Interface state owned by the session (game/input.c, render/). */
    int build_menu;
    bool debug,pointer_down;
    float pointer_x,pointer_y;
    char message[256];
    double message_time;
} Game;

typedef struct { Vec2 p;float angle; } VisualPose;

/* world.c: creation from a LevelSpec (ownership moves into the world). */
Game *game_world_create(LevelSpec *spec);
void game_world_free(Game *g);
void game_announce(Game *g,const char *text);

/* appearance.c: art and footprints, shared by simulation and rendering. */
bool game_entity_sprite(const Game *g,const Entity *e,char *out,size_t size);
/* Semi-axes of the selection ellipse; workers doing a job stand on its edge. */
Vec2 game_footprint(const Game *g,const Entity *e);
const char *game_entity_title(const Game *g,const Entity *e);

/* economy.c */
Stock game_job_cost(const Game *g,const Entity *e);
bool game_affordable(const Game *g,Stock c);
void game_spend(Game *g,Stock c);
void game_credit(Game *g,Resource resource,int value);
bool game_available(const Worker *w);
bool game_gate_open(const Game *g,const Entity *e);
bool game_fogged(const Game *g,const Entity *e);
bool game_burning_log(const Entity *e);
bool game_job(const Game *g,const Entity *e);

/* navigation.c */
int game_closest(const Game *g,Vec2 p);
bool game_blocked(const Game *g,Vec2 a,Vec2 b,int target);
bool game_route_to(const Game *g,const Worker *w,Vec2 destination,int target,Vec2 *path,int *count,float *length);
bool game_route(const Game *g,const Worker *w,int target,Vec2 *path,int *count,float *length);
bool game_vehicle_route(const Game *g,const Worker *w,int target,Vec2 *path,int *count,float *length);
bool game_reachable(const Game *g,int target);
int game_path_capacity(const Game *g);

/* jobs.c */
enum { JOB_CLICK=1,JOB_REPAIR=2,JOB_ATV=4 };
typedef struct {
    void (*complete)(Game *g,Entity *target,Worker *worker);
    bool keep_target;
    unsigned flags;
} JobDef;
const JobDef *game_job_definition(EntityKind kind);
void game_waiting_detail(Game *g,int target,char *text,size_t size);
bool game_dispatch(Game *g,int target,bool quiet);
void game_return_home(Game *g,Worker *w);
void game_finish_work(Game *g,int worker);
void game_arrive_home(Game *g,Worker *w);
bool game_output_busy(const Game *g,const Entity *building);
bool game_demolish(Game *g,int building);
bool game_construct(Game *g,int lot,const Archetype *producer);

/* tick.c: TICK_SECONDS per step; game_advance accumulates real time. */
void game_tick(Game *g,double dt);
void game_advance(Game *g,double seconds);
int game_direction(Vec2 from,Vec2 to,int directions);
#endif
