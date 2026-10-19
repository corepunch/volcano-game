#include "game_internal.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int game_add_entity(Game *g, int kind,const LevelObject *raw,const char *anim)
{
    if (g->object_count>=MAX_LEVEL_OBJECTS) return -1;
    int index=g->object_count++;
    Entity *e=&g->objects[index];
    e->kind=kind;e->p=e->workpoint=e->origin=e->roam=(Vec2){raw->x,raw->y};e->frame=-1;e->follower=-1;e->order=index;e->fire_stopped=-1;
    /* Retail marks burning barriers in the object's name, not its sprite. */
    e->fire=raw->type==102 && !strcmp(raw->name,"fire");
    e->seed=raw->id;e->support=-1;e->visual=-1;e->owner=e->output=e->gate=e->checked_route=-1;e->roof=kind==SURVIVOR && !strcmp(raw->extra,"roof");
    e->transport=!strcmp(raw->extra,"offroad")?BY_ATV:!strcmp(raw->params,"boat_food")?BY_BOAT:ON_FOOT;
    e->floating=(kind==DECOR && (strstr(anim,"_water") || strstr(anim,"treewater"))) ||
        (kind==RESOURCE && (!strcmp(anim,"boat_wood") || !strcmp(raw->params,"boat_food"))) || raw->type==113;
    if (raw->type==6) {
        e->ambient=!strncmp(anim,"bird",4)?BIRD:strstr(anim,"cloud")?CLOUD:NO_AMBIENT;
        const char *last=strrchr(raw->params,';');float speed=last?strtof(last+1,NULL):0;
        e->travel_speed=e->ambient==BIRD?(speed>0?speed:110):8+game_particle_random(e->seed)*6;
    }
    snprintf(e->anim,sizeof(e->anim),"%s",anim);
    return index;
}

static void game_decor_name(const char *params,char *output,size_t capacity)
{
    if (level_param(params,"tree",output,capacity) || level_param(params,"anim",output,capacity) ||
        level_param(params,"simple",output,capacity)) return;
    snprintf(output,capacity,"%s",params);
    char *end=strchr(output,';');if (end) *end=0;
}

static bool game_deferred_decor(const char *name)
{
    static const char *names[]={"editor_land","editor_water","211_oil","014_waste",
        "106_rails0","106_rails1","106_rails2","107_tunnel","offset=effect_lava_add"};
    for (size_t i=0;i<sizeof names/sizeof *names;i++) if (!strcmp(name,names[i])) return true;
    return false;
}

static bool game_custom_rules(Game *g, const char *path)
{
    FILE *file=fopen(path,"rb");
    if (!file) return errno==ENOENT?true:game_fail("Cannot open custom mission rules: %s",path);
    char buffer[1024];size_t n=fread(buffer,1,sizeof buffer-1,file);
    bool ok=!ferror(file) && feof(file);fclose(file);buffer[n]=0;
    if (!ok || memchr(buffer,0,n)) return game_fail("Invalid or oversized custom mission rules: %s",path);
    const char *keys[]={"version","food","wood","money","fuel","rescue_goal","basin_goal","allow_cafe"};
    int values[8]={0};unsigned seen=0;char *save=NULL;
    for (char *line=strtok_r(buffer,"\r\n",&save);line;line=strtok_r(NULL,"\r\n",&save)) {
        char *equals=strchr(line,'=');if (!equals) return game_fail("Invalid mission rule: %s",line);
        *equals=0;int key=-1;
        for (int i=0;i<8;i++) if (!strcmp(line,keys[i])) key=i;
        if (key<0 || (seen&(1u<<key))) return game_fail("Unknown or duplicate mission rule: %s",line);
        char *end;errno=0;long value=strtol(equals+1,&end,10);
        if (errno || end==equals+1 || *end || value<0 || value>1000000) return game_fail("Invalid mission value: %s",line);
        values[key]=(int)value;seen|=1u<<key;
    }
    if (seen!=255 || values[0]!=1 || values[7]>1) return game_fail("Incomplete or unsupported custom mission rules");
    g->early=true;g->food=values[1];g->wood=values[2];g->money=values[3];g->gas=values[4];
    g->goal=values[5];g->basin_goal=values[6];g->allow_cafe=values[7]!=0;
    return true;
}

/* Raw retail type IDs enter through this registry. New types register a spawn
 * callback here; common runtime rules use EntityKind and resolved data. */
typedef struct SpawnDef SpawnDef;
typedef bool (*SpawnCallback)(Game *g,int source,const SpawnDef *definition);
struct SpawnDef {
    uint32_t type;
    EntityKind kind;
    const char *animation;
    SpawnCallback spawn;
    bool mission_only,vehicle_mission_only;
};

static bool spawn_entity(Game *g,int i,EntityKind kind,const char *name)
{
    const LevelObject *raw=&g->level.objects[i];
    if (!name[0]) return true;
    int effect=raw->type==102?(!strncmp(name,"fog_",4)?1:!strcmp(name,"fire")?2:0):0;
    if (!effect && !animation_find(game_sprite_name(name))) {
        game_fail("Missing original animation '%s' for object %u",game_sprite_name(name),raw->id);return false;
    }
    int id=game_add_entity(g, kind,raw,name);if (id<0) { game_fail("Too many game objects");return false; }
    Entity *e=&g->objects[id];
    e->effect=effect;e->source=i;e->duration=game_job_definition(kind)->duration;
    if (kind==BARRIER) e->obstacle=game_obstacle_definition(raw->params);
    if (kind==RESOURCE) game_set_pickup(e,raw->params,raw->name);
    if (kind==ANIMAL) {g->animals++;}
    if (kind==VALVE || kind==SERVICE) {e->operational=kind==SERVICE && !strcmp(raw->extra,"0;0");}
    if (raw->type==151) e->effect=3;
    if (g->early && kind==BUILDING) {
        e->building=game_building_type(raw->params);
        e->operational=!strcmp(raw->extra,"0;0");game_building_art(e);
    }
    if (raw->type==6) e->p=raw->auxiliary;
    if (kind==WORKER) {
        if (g->worker_count>=MAX_WORKERS) { game_fail("Too many workers");return false; }
        Worker *w=&g->workers[g->worker_count++];w->entity=id;w->home=e->p;w->boat=raw->type==113;w->target=-1;w->home_base=-1;
    } else if (kind==BARRIER || kind==RESOURCE || kind==SURVIVOR) {
        g->target_count++;
        if (kind==BARRIER) { g->barriers++;e->frame=0; }
        else if (kind==RESOURCE) { g->resources++;e->frame=0; }
        else g->survivors++;
    }
    return true;
}

static bool spawn_simple(Game *g,int source,const SpawnDef *definition)
{
    const LevelObject *raw=&g->level.objects[source];
    return spawn_entity(g,source,definition->kind,definition->animation?definition->animation:raw->params);
}

static bool spawn_decor(Game *g,int source,const SpawnDef *definition)
{
    (void)definition;
    char name[128];game_decor_name(g->level.objects[source].params,name,sizeof name);
    if (game_deferred_decor(name)) return true;
    return spawn_entity(g,source,!strcmp(name,"water0")?WATER:DECOR,name);
}

static bool spawn_resource(Game *g,int source,const SpawnDef *definition)
{
    (void)definition;
    const char *name=g->level.objects[source].params;
    return spawn_entity(g,source,RESOURCE,g->early && !strcmp(name,"boat_food")?"boat_fish_idle":name);
}

static bool spawn_boat(Game *g,int source,const SpawnDef *definition)
{
    (void)definition;
    return spawn_entity(g,source,g->early?VEHICLE:WORKER,"boat0");
}

static bool spawn_animal(Game *g,int source,const SpawnDef *definition)
{
    (void)definition;
    const LevelObject *raw=&g->level.objects[source];char name[128];
    snprintf(name,sizeof name,"%s%u_idle0",raw->params,raw->id%3);
    return spawn_entity(g,source,ANIMAL,name);
}

static bool spawn_ambient(Game *g,int source,const SpawnDef *definition)
{
    (void)definition;
    char name[128];snprintf(name,sizeof name,"%s",g->level.objects[source].params);
    char *end=strchr(name,';');if (end) *end=0;
    return spawn_entity(g,source,DECOR,name);
}

static bool spawn_pier(Game *g,int source,const SpawnDef *definition)
{
    (void)definition;
    return spawn_entity(g,source,g->early?SERVICE:DECOR,g->early?"seaport_br":g->level.objects[source].params);
}

static bool spawn_visual(Game *g,int source,const SpawnDef *definition)
{
    (void)definition;
    if (!*g->custom_path) return true;
    const LevelObject *raw=&g->level.objects[source];int visual;
    if (!map_visual_parse(&g->scene,raw,g->custom_path,&visual)) return false;
    if (visual>=0 && g->scene.visuals[visual].stage==MAP_WORLD) {
        int entity=game_add_entity(g,DECOR,raw,"");
        if (entity<0) return game_fail("Too many world visuals");
        g->objects[entity].visual=visual;g->objects[entity].source=source;
    }
    return true;
}

static bool spawn_record(Game *g,int source)
{
    static const SpawnDef definitions[]={
        {2,DECOR,NULL,spawn_decor,false,false},
        {5,DECOR,NULL,spawn_visual,false,false},
        {6,DECOR,NULL,spawn_ambient,false,false},
        {101,BUILDING,NULL,spawn_simple,false,false},
        {102,BARRIER,NULL,spawn_simple,false,false},
        {104,WORKER,"worker_wait0",spawn_simple,false,false},
        {105,ANIMAL,NULL,spawn_animal,true,false},
        {108,RESOURCE,NULL,spawn_resource,false,false},
        {109,SURVIVOR,"man_idle0",spawn_simple,false,false},
        {110,BASE,"base0",spawn_simple,false,false},
        {112,DECOR,NULL,spawn_simple,false,false},
        {113,WORKER,NULL,spawn_boat,false,false},
        {114,SERVICE,NULL,spawn_pier,false,false},
        {125,DECOR,"firefighter_idle0",spawn_simple,false,false},
        {126,DECOR,NULL,spawn_simple,false,false},
        {128,VALVE,"tap",spawn_simple,true,false},
        {142,DECOR,"delivery_place0",spawn_simple,false,false},
        {146,BUILD_LOT,"build_place",spawn_simple,false,false},
        {147,DECOR,NULL,spawn_simple,false,false},
        {151,WATER,"water0",spawn_simple,true,false},
        {153,VEHICLE,"offroad0",spawn_simple,true,true},
        {154,SERVICE,"garage0",spawn_simple,true,true}
    };
    for (size_t i=0;i<sizeof definitions/sizeof *definitions;i++) {
        const SpawnDef *definition=&definitions[i];
        if (definition->type!=g->level.objects[source].type) continue;
        if ((definition->mission_only && !g->early) ||
            (definition->vehicle_mission_only && !*g->custom_path && g->campaign<4)) return true;
        return definition->spawn(g,source,definition);
    }
    return true; /* Unknown retail controllers remain in the lossless Level data. */
}

bool game_load_level(Game *g,int index,const char *custom_path)
{
    if (!custom_path && !campaign_load(asset_root)) return false;
    char path[PATH_CAPACITY];
    if (custom_path) {
        if (strlen(custom_path)>=sizeof path-6) return game_fail("Custom level path too long");
        snprintf(path,sizeof path,"%s",custom_path);
    } else snprintf(path,sizeof(path),"%s/level/level%03d.lvl",asset_root,index);
    if (!level_load(path,&g->level,last_error,1024)) return false;
    g->index=index;g->build_menu=-1;g->pointer_x=g->pointer_y=-100;
    g->campaign=custom_path?0:campaign_number(index);g->routes_dirty=true;
    game_apply_mission_rules(g,g->campaign);
    if (custom_path) {
        snprintf(g->custom_path,sizeof g->custom_path,"%s",path);
        char rules[PATH_CAPACITY];snprintf(rules,sizeof rules,"%s.rules",path);
        if (!game_custom_rules(g, rules)) return false;
    }
    snprintf(g->background,sizeof(g->background),"texture/back/%s_0.jpg",g->level.background);
    snprintf(g->alpha,sizeof(g->alpha),"texture/back/%s_0.alpha",g->level.background);
    if (custom_path && g->level.background[0]=='@') {
        const char *image=g->level.background+1,*slash=strrchr(path,'/');
        if (!*image || strchr(image,'/') || strchr(image,'\\') || !strcmp(image,".") || !strcmp(image,"..")) {
            game_fail("Custom background must be @FILENAME beside the level");return false;
        }
        int n=snprintf(g->background,sizeof g->background,"%.*s%s",slash?(int)(slash-path+1):0,path,image);
        if (n<0 || n>=(int)sizeof g->background) {game_fail("Custom background path too long");return false;}
        g->alpha[0]=0;g->local_background=true;
    }
    for (int i=0;i<g->level.object_count;i++) if (!spawn_record(g,i)) return false;
    if (g->early) for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];const LevelObject *raw=&g->level.objects[e->source];
        if (e->kind==SERVICE || e->kind==VALVE) for (int j=0;j<g->level.object_count;j++) {
            LevelObject *marker=&g->level.objects[j];
            if (marker->type==3 && marker->link_id==e->seed && !marker->name[0]) e->workpoint=(Vec2){marker->x,marker->y};
        }
        const char *group=(e->effect==1 || raw->type==151)?raw->name:NULL;
        if ((raw->type==151 && !*raw->name) || raw->type==114 || e->transport==BY_BOAT) {
            float nearest=INFINITY;
            for (int j=0;j<g->level.object_count;j++) {
                LevelObject *tile=&g->level.objects[j];float d=game_distance(e->p,(Vec2){tile->x,tile->y});
                if (tile->type==151 && tile->name[0] && d<nearest) {nearest=d;group=tile->name;}
            }
        }
        if (group && *group) for (int j=0;j<g->object_count;j++)
            if (g->objects[j].kind==VALVE && !strcmp(group,g->level.objects[g->objects[j].source].name)) e->gate=j;
        if (e->kind==VEHICLE) {
            if (g->vehicle_count==16) {game_fail("Too many rescue vehicles");return false;}
            Worker *v=&g->vehicles[g->vehicle_count++];v->entity=i;v->target=-1;v->home_base=-1;v->atv=raw->type==153;v->boat=!v->atv;
            for (int j=0;j<g->level.object_count;j++) {
                LevelObject *marker=&g->level.objects[j];
                if (marker->type!=3 || !*marker->name || strcmp(marker->name,raw->name)) continue;
                for (int k=0;k<g->object_count;k++) if (g->objects[k].kind==SERVICE && g->objects[k].seed==marker->link_id) {
                    v->home_base=k;v->home=(Vec2){marker->x,marker->y};
                }
            }
            if (v->home_base<0) {game_fail("Vehicle %u has no authored service marker",e->seed);return false;}
            e->p=v->home;e->hidden=!g->objects[v->home_base].operational;
        }
    }
    if (!g->worker_count || !g->level.traffic_count) {
        game_fail("Original level %d has no prototype workers or traffic graph",index);return false;
    }
    for (int i=0;i<g->worker_count;i++) {
        Worker *w=&g->workers[i];
        if (!w->boat) for (int j=0;j<g->object_count;j++)
            if (g->objects[j].kind==BASE && game_distance(w->home,g->objects[j].p)<80) { w->home_base=j;break; }
        g->objects[w->entity].hidden=w->home_base>=0;
        if (g->early && !w->boat) {
            int base=-1;
            for (int j=0;j<g->object_count;j++) if (g->objects[j].kind==BASE) { base=j;break; }
            if (base<0) { game_fail("Mission has no worker base");return false; }
            w->home_base=base;w->home=g->objects[base].p;
            if (g->campaign==1 || game_distance(g->objects[w->entity].p,w->home)<80) {
                g->objects[w->entity].p=w->home;g->objects[w->entity].hidden=true;
            } else {
                w->phase=STRANDED;w->recruiting=true;g->objects[w->entity].kind=RECRUIT;
            }
        }
    }
    if (g->early) for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];
        if (e->kind==SURVIVOR) {
            if (g->evacuee_count==MAX_WORKERS) { game_fail("Too many evacuees");return false; }
            Worker *v=&g->evacuees[g->evacuee_count++];v->entity=i;v->phase=STRANDED;
            v->home=g->workers[0].home;v->target=-1;
        }
        const LevelObject *raw=&g->level.objects[e->source];
        if (e->kind==RESOURCE && raw->link_id) for (int j=0;j<g->object_count;j++) {
            Entity *owner=&g->objects[j];
            if (owner->seed==raw->link_id && (owner->kind==BUILDING || owner->kind==BUILD_LOT)) {
                e->owner=j;owner->output=i;e->hidden=!owner->operational;break;
            }
        }
    }
    /* Retail roof occupants have extra="roof" and a higher source layer than
     * flooded decor, but no link ID. Recover support from the authored footprint. */
    for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];if (!e->roof) continue;
        float closest_house=INFINITY;
        for (int j=0;j<g->object_count;j++) {
            Entity *house=&g->objects[j];
            if (house->kind!=DECOR || (strcmp(house->anim,"sawmill_water") && strcmp(house->anim,"house_water"))) continue;
            SpriteFrame f=sprite_body_frame(animation_find(house->anim)->frames[0]);
            float dx=e->p.x-house->p.x,dy=e->p.y-house->p.y,d=game_distance(e->p,house->p);
            if (dx>=-f.dx && dx<=f.w-f.dx && dy>=-f.dy && dy<=f.h-f.dy && d<closest_house)
                e->support=j,closest_house=d;
        }
    }
    g->ready=true;game_announce(g, g->early?"Collect supplies and clear routes. People return to headquarters when the path is safe.":"Click debris, supplies or survivors to dispatch a worker.");
    last_error[0]=0;return true;
}
