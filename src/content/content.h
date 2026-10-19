#ifndef VOLCANO_CONTENT_H
#define VOLCANO_CONTENT_H
/* Game content model. Everything the simulation needs to know about a kind
 * of object lives in an Archetype loaded from content/NAME.xml; missions may
 * add their own archetypes, usually by extending a shared one. No SDL. */
#include "../core/core.h"
#include "../assets/assets.h"

typedef enum { RESOURCE_FOOD,RESOURCE_WOOD,RESOURCE_MONEY,RESOURCE_FUEL,RESOURCE_COUNT,RESOURCE_NONE=-1 } Resource;
typedef union { struct { int food,wood,money,fuel; };int amount[RESOURCE_COUNT]; } Stock;
extern const char *const resource_names[RESOURCE_COUNT];

typedef enum {
    CLASS_DECOR,CLASS_WATER,CLASS_BASE,CLASS_OBSTACLE,CLASS_PICKUP,CLASS_CIVILIAN,CLASS_WORKER,
    CLASS_LOT,CLASS_PRODUCER,CLASS_ANIMAL,CLASS_VALVE,CLASS_SERVICE,CLASS_VEHICLE,CLASS_COUNT
} ArchetypeClass;
/* Where a worker stands to do a job: at the edge of the target's selection
 * footprint, at its centre, or at an authored work point. */
typedef enum { APPROACH_FOOTPRINT,APPROACH_CENTER,APPROACH_WORKPOINT } Approach;
typedef enum { EFFECT_NONE,EFFECT_FOG,EFFECT_FIRE,EFFECT_POLLUTION } VisualEffect;
typedef enum { AMBIENT_NONE,AMBIENT_BIRD,AMBIENT_CLOUD } Ambient;

typedef struct Archetype Archetype;
struct Archetype {
    const char *id,*title;
    ArchetypeClass kind;
    const Archetype *base;              /* "extends" parent, or NULL. */
    /* Appearance. Templates expand {d} direction, {v} variant, {action} work. */
    const char *sprite,*damaged;        /* Static art; producers/services also have damaged art. */
    const char *idle,*move,*carry,*run,*work,*shadow,*act;
    int directions,variants;
    float speed,opacity;
    Ambient ambient;
    VisualEffect effect;
    /* Jobs. */
    Approach approach;
    const char *action;                 /* Worker animation key while working on this target. */
    float duration;
    Stock cost,reward;
    int cleared_frame;                  /* Frame shown once cleared; -1 hides the obstacle. */
    bool burnable,needs_dynamite,boat_only;
    /* Pickups. */
    Resource resource;
    int amount;
    /* Producers and services. */
    Stock build_cost,repair_cost;
    Resource produces;
    int income;
    float income_interval,bonus_interval;
    const Archetype *bonus;             /* Pickup offered at the producer's linked output. */
    int menu_slot;                      /* Construction menu position, -1 when not buildable. */
    const char *menu_lock;              /* Message when the mission does not allow building it. */
};

/* Named policies of a ruleset. The restored rescue missions enable all of
 * them; the retail prototype scenes (campaign 7-50) enable none. */
typedef struct {
    const char *id;
    bool economy;           /* Costs, rewards, production, repairs, demolition. */
    bool auto_evacuation;   /* Civilians walk home when a route opens; distant crew are recruits. */
    bool vehicles;          /* ATV and boat trips. */
    bool gates;             /* Valve-linked fog and pollution. */
    bool wildlife;          /* Animals roam and count as objectives. */
    bool goals;             /* Objectives are people/animals/basins rather than clear-everything. */
} Ruleset;

#define MAX_BUILDABLE 8
typedef struct {
    const Ruleset *rules;
    Stock start;
    int people_goal,basin_goal;
    const Archetype *buildable[MAX_BUILDABLE];
    int buildable_count;
    bool crew_at_hq;        /* Start every on-foot worker at headquarters. */
    bool offroad;           /* ATV and garage records are active (retail missions 1-3 predate them). */
} Mission;

bool content_load(const char *directory);
void content_destroy(void);
const Archetype *archetype_find(const char *id);
const Ruleset *ruleset_find(const char *id);
bool archetype_is(const Archetype *a,const Archetype *ancestor);
/* Placement rule. Objects with a base (buildings, lots, services, valves,
 * headquarters, obstacles, supplies) are positioned by their base centre:
 * the selection ring is centred there, and their sprite's body is centred
 * on it horizontally while the sprite's pivot sets the ground line. Actors
 * and scenery are positioned by their sprite's pivot. */
bool archetype_has_base(const Archetype *a);
/* Where SPRITE's pivot goes when an object of type A positioned at P shows
 * FRAME (-1 while animating). */
Vec2 archetype_sprite_origin(const Archetype *a,const Sprite *sprite,int frame,Vec2 p);
/* Construction menu: producers with a menu slot, ordered by slot. */
int content_menu(const Archetype **out,int capacity);
bool mission_allows(const Mission *m,const Archetype *a);
/* Mission settings for an original campaign position (1-50). */
bool campaign_mission(int number,Mission *out);
const char *content_directory(void);

/* ---- Level specification: what a level file describes, in one format-neutral shape. */

typedef enum { TRANSPORT_FOOT,TRANSPORT_ATV,TRANSPORT_BOAT } Transport;
typedef struct {
    uint32_t id;
    const Archetype *type;
    Vec2 p;
    uint32_t layer;
    const char *sprite;                 /* Optional art override. */
    int variant;
    bool operational,burning,floating,roof;
    Transport transport;
    /* Relations are indices into the spec's placements, -1 for none. */
    int producer;                       /* Pickup: the producer or lot that offers it. */
    int gate;                           /* Valve that must be opened first. */
    int service;                        /* Vehicle: its home pier or garage. */
    int support;                        /* Roof occupant: the house it rides on. */
    bool has_workpoint,has_home,has_corridor;
    Vec2 workpoint,home,ambient_start;
    Vec2 corridor;float corridor_radius;/* Offroad corridor an ATV uses to reach this recruit. */
    float travel_speed;
} Placement;

typedef enum { MAP_FOAM,MAP_SPRITE,MAP_SPARKLES,MAP_INSECTS,MAP_MOTES } MapVisualKind;
typedef enum { MAP_GROUND,MAP_WORLD,MAP_OVERLAY } MapVisualStage;
typedef struct {
    MapVisualKind kind;
    MapVisualStage stage;
    uint32_t seed,layer;
    Vec2 p,extent;
    const char *sprite;
    float rate,speed,wind,scale_x,scale_y,rotation,opacity;
    int count;
} MapVisual;

typedef struct {
    Arena arena;
    char path[PATH_CAPACITY],title[128];
    char background[PATH_CAPACITY],background_alpha[PATH_CAPACITY];
    bool retail_background;             /* Background is a retail texture path, resolved through the VFS. */
    bool positions_are_pivots;          /* Placements give sprite pivots, not base centres (retail levels). */
    Mission mission;
    int campaign;                       /* Original campaign position, 0 for missions. */
    int source_index;                   /* Original level file number, -1 for missions. */
    Placement *placements;int placement_count;
    Vec2 *traffic;int traffic_count;
    MapVisual *visuals;int visual_count;
    const char *packs[8];int pack_count;           /* Asset folders the mission requires. */
    const char *sprite_files[8];int sprite_file_count;
    Archetype **local;int local_count;             /* Mission-defined archetypes (owned by the arena). */
} LevelSpec;

/* Native mission file (XML). Mission-local archetypes stay registered until
 * the next level load or level_spec_free(). */
bool mission_load(const char *path,LevelSpec *out);
/* Original retail level by file index, with campaign settings applied. */
bool retail_level_load(int index,LevelSpec *out);
void level_spec_free(LevelSpec *spec);
const Placement *level_placement(const LevelSpec *spec,uint32_t id);

/* Retail campaign order recovered from level/map.lvl's prerequisite chain. */
bool campaign_load(void);
int campaign_index(int number);
int campaign_number(int index);
/* Reads every retail level binary and rejects malformed variants (retail data required). */
bool retail_level_selftest(void);
#endif
