/* Retail level importer: Rescue Team 5 records -> LevelSpec.
 *
 * This is the only module that knows retail type codes, parameter strings,
 * sprite naming conventions and the heuristics needed to recover relations
 * the binaries leave implicit. Everything it produces refers to archetypes
 * from content/archetypes.xml. */
#include "content_internal.h"
#include "../assets/assets.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    T_MARKER=3,T_EFFECT=5,T_AMBIENT=6,T_SCENERY=2,T_BUILDING=101,T_BARRIER=102,T_WORKER=104,T_ANIMAL=105,
    T_RESOURCE=108,T_CIVILIAN=109,T_BASE=110,T_HELIPAD=112,T_BOAT=113,T_PIER=114,T_FIREFIGHTER=125,
    T_FIRE_STATION=126,T_VALVE=128,T_DELIVERY=142,T_LOT=146,T_BOOST=147,T_POLLUTION=151,T_OFFROAD_REGION=152,
    T_ATV=153,T_GARAGE=154
};

typedef struct {
    LevelSpec *spec;
    const RetailLevel *level;
    const Ruleset *rules;
    int *source;            /* Retail object index per placement. */
} Import;

/* Retail sprite names whose atlas entries carry a different name. */
static const char *sprite_alias(const char *name)
{
    static const char *aliases[][2]={
        {"food1","food"},{"food2","food_big"},{"wood1","wood"},{"wood2","wood_big"},
        {"gas1","gas"},{"gas2","gas_big"},{"tree00","bar_tree00"},{"stone00","bar_stone00"},
        {"burger","burger0"},{"sawmill","sawmill0"},{"heliport","heliport_br"},
        {"house","house0"},{"derrick","gas0"},{"stone_big00","bar_stone_big00"},
        {"money1","money"},{"money2","money_big"},{"bonus_fast_offroad","bonus_fast_offroader"},
        {"offroad_wood","wood_big"},{"boat_food","food_big"}
    };
    for (size_t i=0;i<sizeof aliases/sizeof *aliases;i++) if (!strcmp(name,aliases[i][0])) return aliases[i][1];
    return name;
}

static const Archetype *require(const char *id)
{
    const Archetype *a=archetype_find(id);
    if (!a) error_set("content/archetypes.xml lacks archetype \"%s\" required by the retail importer",id);
    return a;
}

/* Obstacle rules by retail parameter text; first match wins, so specific
 * families precede their general form. */
static const char *obstacle_archetype(const char *params)
{
    static const char *table[][2]={
        {"fog_","fog"},{"tree00","tree"},{"tree","log"},{"stone_big","boulder"},{"stone_dynamite","blast-rock"},
        {"stone","stone"},{"bridge_auto","auto-bridge"},{"bridge","bridge"},{"road","road"}
    };
    if (!strcmp(params,"fire")) return "fire-hazard";
    for (size_t i=0;i<sizeof table/sizeof *table;i++) {
        if (i==0 ? !strncmp(params,table[i][0],strlen(table[i][0])) : strstr(params,table[i][0])!=NULL) return table[i][1];
    }
    return "debris";
}

static const char *pickup_archetype(const char *params,const char *name)
{
    if (!strcmp(params,"boat_food")) return !strcmp(name,"food2")?"fishing-spot-large":"fishing-spot";
    if (!strcmp(params,"boat_wood")) return "floating-wood";
    size_t n=strlen(params);bool large=n && params[n-1]=='2';
    if (strstr(params,"food")) return large?"food-large":"food-small";
    if (strstr(params,"gas")) return large?"fuel-large":"fuel-small";
    if (strstr(params,"money")) return large?"money-large":"money-small";
    return large?"wood-large":"wood-small";
}

static const char *producer_archetype(const char *params)
{
    if (!strcmp(params,"sawmill")) return "sawmill";
    if (!strcmp(params,"burger")) return "cafe";
    if (!strcmp(params,"house")) return "house";
    return "gas-station"; /* The prototype's fallback for later retail buildings. */
}

static void decor_name(const char *params,char *out,size_t size)
{
    if (retail_param(params,"tree",out,size) || retail_param(params,"anim",out,size) || retail_param(params,"simple",out,size)) return;
    snprintf(out,size,"%s",params);
    char *end=strchr(out,';');if (end) *end=0;
}

static bool deferred_decor(const char *name)
{
    static const char *names[]={"editor_land","editor_water","211_oil","014_waste",
        "106_rails0","106_rails1","106_rails2","107_tunnel","offset=effect_lava_add"};
    for (size_t i=0;i<sizeof names/sizeof *names;i++) if (!strcmp(name,names[i])) return true;
    return false;
}

static Placement *add(Import *im,int source,const char *archetype,const char *sprite)
{
    const RetailObject *o=&im->level->objects[source];
    const Archetype *type=require(archetype);
    if (!type) return NULL;
    if (sprite && type->effect==EFFECT_NONE && !sprite_find(sprite)) {
        error_set("Missing original animation '%s' for object %u",sprite,o->id);return NULL;
    }
    LevelSpec *spec=im->spec;
    Placement *p=&spec->placements[spec->placement_count];
    memset(p,0,sizeof *p);
    p->id=o->id;p->type=type;p->p=o->p;p->layer=o->layer;p->producer=p->gate=p->service=p->support=-1;
    if (sprite && (!type->sprite || strcmp(sprite,type->sprite))) p->sprite=arena_strdup(&spec->arena,sprite);
    p->burning=o->type==T_BARRIER && !strcmp(o->name,"fire");
    p->roof=type->kind==CLASS_CIVILIAN && !strcmp(o->extra,"roof");
    p->transport=!strcmp(o->extra,"offroad")?TRANSPORT_ATV:!strcmp(o->params,"boat_food")?TRANSPORT_BOAT:TRANSPORT_FOOT;
    const char *art=sprite?sprite:type->sprite?type->sprite:"";
    p->floating=(type->kind==CLASS_DECOR && (strstr(art,"_water") || strstr(art,"treewater"))) ||
        (type->kind==CLASS_PICKUP && (!strcmp(o->params,"boat_wood") || !strcmp(o->params,"boat_food"))) || o->type==T_BOAT;
    p->operational=!strcmp(o->extra,"0;0");
    if (o->type==T_AMBIENT) {
        p->p=o->auxiliary;p->ambient_start=o->auxiliary;
        const char *last=strrchr(o->params,';');float speed=last?strtof(last+1,NULL):0;
        p->travel_speed=type->ambient==AMBIENT_BIRD?(speed>0?speed:110):8+hash_random(o->id)*6;
    }
    im->source[spec->placement_count++]=source;return p;
}

static bool import_object(Import *im,int source)
{
    const RetailObject *o=&im->level->objects[source];const Ruleset *r=im->rules;
    const Mission *m=&im->spec->mission;char name[512];
    switch (o->type) {
    case T_SCENERY:
        decor_name(o->params,name,sizeof name);
        if (deferred_decor(name) || !*name) return true;
        return add(im,source,!strcmp(name,"water0")?"water":"decor",name)!=NULL;
    case T_AMBIENT: {
        snprintf(name,sizeof name,"%s",o->params);char *end=strchr(name,';');if (end) *end=0;
        if (!*name) return true;
        return add(im,source,!strncmp(name,"bird",4)?"bird":strstr(name,"cloud")?"cloud":"decor",name)!=NULL;
    }
    case T_BUILDING:
        return !*o->params || add(im,source,producer_archetype(o->params),r->economy?NULL:sprite_alias(o->params))!=NULL;
    case T_BARRIER: {
        if (!*o->params) return true;
        const char *archetype=obstacle_archetype(o->params);
        return add(im,source,archetype,sprite_alias(o->params))!=NULL;
    }
    case T_WORKER: return add(im,source,"worker",NULL)!=NULL;
    case T_ANIMAL: {
        if (!r->wildlife || !*o->params) return true;
        Placement *p=add(im,source,o->params,NULL);
        if (p) p->variant=(int)(o->id%3);
        return p!=NULL;
    }
    case T_RESOURCE: {
        if (!*o->params) return true;
        const char *sprite=r->economy && !strcmp(o->params,"boat_food")?"boat_fish_idle":sprite_alias(o->params);
        return add(im,source,pickup_archetype(o->params,o->name),sprite)!=NULL;
    }
    case T_CIVILIAN: return add(im,source,"civilian",NULL)!=NULL;
    case T_BASE: return add(im,source,"hq",NULL)!=NULL;
    case T_HELIPAD: case T_FIRE_STATION: case T_BOOST:
        return !*o->params || add(im,source,"decor",sprite_alias(o->params))!=NULL;
    case T_FIREFIGHTER: return add(im,source,"decor","firefighter_idle0")!=NULL;
    case T_DELIVERY: return add(im,source,"decor","delivery_place0")!=NULL;
    case T_BOAT: return add(im,source,r->vehicles?"fishing-boat":"boat-worker",NULL)!=NULL;
    case T_PIER:
        if (r->economy) return add(im,source,"pier",NULL)!=NULL;
        return !*o->params || add(im,source,"decor",sprite_alias(o->params))!=NULL;
    case T_VALVE: return !r->gates || add(im,source,"valve",NULL)!=NULL;
    case T_LOT: return add(im,source,"lot",NULL)!=NULL;
    case T_POLLUTION: return !r->gates || add(im,source,"polluted-water",NULL)!=NULL;
    case T_ATV: return !r->vehicles || !m->offroad || add(im,source,"atv",NULL)!=NULL;
    case T_GARAGE: return !r->vehicles || !m->offroad || add(im,source,"garage",NULL)!=NULL;
    default: return true; /* Unknown retail controllers remain in the binary, inactive. */
    }
}

static int placement_by_id(const Import *im,uint32_t id,ArchetypeClass a,ArchetypeClass b)
{
    for (int i=0;i<im->spec->placement_count;i++) {
        const Placement *p=&im->spec->placements[i];
        if (p->id==id && (p->type->kind==a || p->type->kind==b)) return i;
    }
    return -1;
}

/* Relations the binaries express through markers, names and proximity. */
static bool resolve_relations(Import *im)
{
    LevelSpec *spec=im->spec;const RetailLevel *level=im->level;const Ruleset *r=im->rules;
    for (int i=0;i<spec->placement_count;i++) {
        Placement *p=&spec->placements[i];const RetailObject *o=&level->objects[im->source[i]];
        ArchetypeClass kind=p->type->kind;
        if (r->gates || r->vehicles) {
            /* An unnamed marker linked to a service or valve is its work point. */
            if (kind==CLASS_SERVICE || kind==CLASS_VALVE) for (int j=0;j<level->object_count;j++) {
                const RetailObject *marker=&level->objects[j];
                if (marker->type==T_MARKER && marker->link_id==o->id && !*marker->name) { p->workpoint=marker->p;p->has_workpoint=true; }
            }
        }
        if (r->gates) {
            /* Fog and pollution belong to the valve group named like them; piers,
             * fishing spots and unnamed pollution join the nearest named tile's group. */
            const char *group=p->type->effect==EFFECT_FOG || o->type==T_POLLUTION?o->name:NULL;
            if ((o->type==T_POLLUTION && !*o->name) || o->type==T_PIER || p->transport==TRANSPORT_BOAT) {
                float nearest=INFINITY;
                for (int j=0;j<level->object_count;j++) {
                    const RetailObject *tile=&level->objects[j];float d=vec2_distance(p->p,tile->p);
                    if (tile->type==T_POLLUTION && *tile->name && d<nearest) { nearest=d;group=tile->name; }
                }
            }
            if (group && *group) for (int j=0;j<spec->placement_count;j++)
                if (spec->placements[j].type->kind==CLASS_VALVE && !strcmp(group,level->objects[im->source[j]].name)) p->gate=j;
        }
        if (kind==CLASS_VEHICLE) {
            /* A marker named like the vehicle is its home; its link names the service. */
            for (int j=0;j<level->object_count;j++) {
                const RetailObject *marker=&level->objects[j];
                if (marker->type!=T_MARKER || !*marker->name || strcmp(marker->name,o->name)) continue;
                int service=placement_by_id(im,marker->link_id,CLASS_SERVICE,CLASS_SERVICE);
                if (service>=0) { p->service=service;p->home=marker->p;p->has_home=true; }
            }
            if (p->service<0) return error_set("Vehicle %u has no authored service marker",p->id);
        }
        if (r->vehicles && p->transport==TRANSPORT_ATV) {
            /* The authored offroad region containing the recruit is the ATV corridor. */
            for (int j=0;j<level->object_count && !p->has_corridor;j++) {
                const RetailObject *region=&level->objects[j];
                if (region->type==T_OFFROAD_REGION && vec2_distance(p->p,region->p)<=(float)region->link_id) {
                    p->corridor=region->p;p->corridor_radius=(float)region->link_id;p->has_corridor=true;
                }
            }
        }
        if (r->economy && kind==CLASS_PICKUP && o->link_id)
            p->producer=placement_by_id(im,o->link_id,CLASS_PRODUCER,CLASS_LOT);
    }
    /* Roof occupants have extra="roof" and a higher layer than flooded decor,
     * but no link ID. Recover the supporting house from its sprite footprint. */
    for (int i=0;i<spec->placement_count;i++) {
        Placement *p=&spec->placements[i];if (!p->roof) continue;
        float closest=INFINITY;
        for (int j=0;j<spec->placement_count;j++) {
            const Placement *house=&spec->placements[j];
            if (house->type->kind!=CLASS_DECOR || !house->sprite ||
                (strcmp(house->sprite,"sawmill_water") && strcmp(house->sprite,"house_water"))) continue;
            const Sprite *art=sprite_find(house->sprite);
            if (!art || !art->frame_count) continue;
            SpriteFrame f=sprite_body_frame(art->frames[0]);
            float dx=p->p.x-house->p.x,dy=p->p.y-house->p.y,d=vec2_distance(p->p,house->p);
            if (dx>=-f.dx && dx<=f.w-f.dx && dy>=-f.dy && dy<=f.h-f.dy && d<closest) { p->support=j;closest=d; }
        }
    }
    return true;
}

bool retail_level_load(int index,LevelSpec *out)
{
    memset(out,0,sizeof *out);
    char relative[64],path[PATH_CAPACITY];RetailLevel level;
    snprintf(relative,sizeof relative,"level/level%03d.lvl",index);
    if (!campaign_load()) return false;
    if (!vfs_resolve(relative,path,sizeof path)) return error_set("Missing original level %s",relative);
    if (!retail_level_read(path,&level)) return false;
    text_copy(out->path,sizeof out->path,path);
    out->source_index=index;out->campaign=campaign_number(index);
    if (!campaign_mission(out->campaign,&out->mission)) out->mission=*campaign_defaults();
    snprintf(out->title,sizeof out->title,"Level %d",out->campaign);
    snprintf(out->background,sizeof out->background,"texture/back/%s_0.jpg",level.background);
    snprintf(out->background_alpha,sizeof out->background_alpha,"texture/back/%s_0.alpha",level.background);
    out->retail_background=true;
    Import im={out,&level,out->mission.rules,malloc((size_t)(level.object_count+1)*sizeof(int))};
    bool ok=im.source && level_spec_reserve(out,level.object_count,level.traffic_count,0);
    if (ok) { memcpy(out->traffic,level.traffic,(size_t)level.traffic_count*sizeof *level.traffic);out->traffic_count=level.traffic_count; }
    error_clear();
    for (int i=0;i<level.object_count && ok;i++) ok=import_object(&im,i);
    if (ok) ok=resolve_relations(&im);
    free(im.source);retail_level_free(&level);
    if (!ok) { char reason[sizeof last_error];memcpy(reason,last_error,sizeof reason);level_spec_free(out);error_set("%s",reason); }
    return ok;
}
