#include "game_internal.h"
#include <stdio.h>
#include <string.h>


/* Restoration balance, not recovered retail constants. One definition is shared
 * by construction, repair, production, tooltips and authored sprite selection. */
const BuildingDef game_buildings[BUILDING_COUNT]={
    {"sawmill","Sawmill","sawmill0","wood1",40,RESOURCE_WOOD},
    {"burger","Burger Cafe","burger0","food1",40,RESOURCE_FOOD},
    {"house","House","house0","money1",60,RESOURCE_MONEY},
    {"derrick","Gas Station","gas0","gas1",80,RESOURCE_FUEL}
};

int game_building_type(const char *name)
{
    for (int i=0;i<BUILDING_COUNT;i++) if (!strcmp(name,game_buildings[i].key)) return i;
    return 3; /* Preserve the prototype's fallback for later retail buildings. */
}

const ObstacleDef *game_obstacle_definition(const char *name)
{
    /* First match wins: specific families precede their general form. */
    static const ObstacleDef definitions[]={
        {"tree00",{20,0,0},20,-1,true,false},
        {"tree",{20,0,0},20,-1,false,false},
        {"stone_big",{40,0,100},0,-1,false,false},
        {"stone_dynamite",{20,0,0},0,-1,false,true},
        {"stone",{20,0,0},0,-1,false,false},
        {"bridge_auto",{20,40,0},0,3,false,false},
        {"bridge",{0,20,0},0,3,false,false},
        {"road",{0,20,0},0,-1,false,false},
        {"",{0,0,0},0,-1,false,false}
    };
    for (size_t i=0;i<sizeof definitions/sizeof *definitions;i++)
        if (strstr(name,definitions[i].match)) return &definitions[i];
    return NULL;
}

void game_set_pickup(Entity *e,const char *name,const char *source_name)
{
    static const struct { const char *match;ResourceKind resource; } kinds[]={
        {"food",RESOURCE_FOOD},{"gas",RESOURCE_FUEL},{"money",RESOURCE_MONEY},{"",RESOURCE_WOOD}
    };
    for (size_t i=0;i<sizeof kinds/sizeof *kinds;i++) if (strstr(name,kinds[i].match)) {
        e->pickup_resource=kinds[i].resource;break;
    }
    size_t n=strlen(name);e->pickup_value=n && name[n-1]=='2'?20:10;
    e->boat_worker=!strcmp(name,"boat_wood");
    if (e->transport==BY_BOAT) {
        e->pickup_resource=RESOURCE_FOOD;e->pickup_value=!strcmp(source_name,"food2")?20:10;
    }
}

void game_apply_mission_rules(Game *g,int campaign)
{
    static const struct { Cost start;int fuel,people,basins;bool cafe; } missions[]={
        {{20,0,0},0,3,0,true},{{50,30,20},0,3,0,true},{{10,100,30},0,5,0,true},
        {{60,80,70},70,13,0,true},{{70,80,40},0,6,0,true},{{60,90,40},220,2,2,false}
    };
    g->early=campaign>=1 && campaign<=(int)(sizeof missions/sizeof *missions);g->allow_cafe=true;
    if (!g->early) return;
    int i=campaign-1;g->food=missions[i].start.food;g->wood=missions[i].start.wood;g->money=missions[i].start.money;
    g->gas=missions[i].fuel;g->goal=missions[i].people;g->basin_goal=missions[i].basins;g->allow_cafe=missions[i].cafe;
}

const char *game_sprite_name(const char *name)
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

Cost game_job_cost(Game *g, const Entity *e)
{
    Cost c={0};if (!g->early) return c;
    const JobDef *definition=game_job_definition(e->kind);
    if (e->kind==BARRIER && e->obstacle) c=e->obstacle->cost;
    else if (e->kind==BUILDING && !e->operational) c.wood=game_buildings[e->building].cost/2;
    else if (!(definition->flags&JOB_REPAIR) || !e->operational) c=definition->cost;
    return c;
}

bool game_affordable(Game *g, Cost c) { return g->food>=c.food && g->wood>=c.wood && g->money>=c.money; }

void game_spend(Game *g, Cost c) { g->food-=c.food;g->wood-=c.wood;g->money-=c.money; }

void game_credit(Game *g, int resource,int value)
{
    if (resource==1) g->food+=value;
    else if (resource==2) g->gas+=value;
    else if (resource==3) g->money+=value;
    else if (resource==4) g->wood+=value;
}

int game_building_resource(int type) { return game_buildings[type].resource; }

void game_building_art(Entity *e)
{
    snprintf(e->anim,sizeof e->anim,"%s%s",game_buildings[e->building].animation,e->operational?"":"_br");e->frame=0;
}

bool game_available(const Worker *w) { return w->phase==IDLE; }

bool game_gate_open(Game *g, const Entity *e) { return e->gate<0 || g->objects[e->gate].done; }

bool game_fogged(Game *g, const Entity *e)
{
    for (int i=0;i<g->object_count;i++) {
        Entity *fog=&g->objects[i];
        if (fog->effect==1 && !fog->done && game_distance(e->p,fog->p)<105) return true;
    }
    return false;
}

bool game_allowed_building(Game *g, int type) { return g->allow_cafe || type!=1; }

bool game_burning_log(const Entity *e)
{
    return e->fire && e->kind==BARRIER && e->obstacle && e->obstacle->burns;
}

bool game_job(Game *g, const Entity *e)
{
    unsigned flags=game_job_definition(e->kind)->flags;
    return (flags&JOB_CLICK) || ((flags&JOB_REPAIR) && g->early && !e->operational) ||
        ((flags&JOB_ATV) && e->transport==BY_ATV);
}
