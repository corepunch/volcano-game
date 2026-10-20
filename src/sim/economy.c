/* Prices, credit, gates and job eligibility. Numbers come from archetypes. */
#include "world.h"

Stock game_job_cost(const Game *g,const Entity *e)
{
    Stock none={{0}};
    if (!g->rules->economy) return none;
    if (e->kind==BUILDING || e->kind==SERVICE) return e->operational?none:e->type->repair_cost;
    return e->type->cost;
}

bool game_affordable(const Game *g,Stock c)
{
    for (int r=0;r<RESOURCE_COUNT;r++) if (g->stock.amount[r]<c.amount[r]) return false;
    return true;
}

void game_spend(Game *g,Stock c) { for (int r=0;r<RESOURCE_COUNT;r++) g->stock.amount[r]-=c.amount[r]; }

void game_credit(Game *g,Resource resource,int value)
{
    if (resource>=0 && resource<RESOURCE_COUNT) g->stock.amount[resource]+=value;
}

bool game_available(const Worker *w) { return w->phase==IDLE; }

bool game_gate_open(const Game *g,const Entity *e) { return e->gate<0 || g->objects[e->gate].done; }

/* Active fog suppresses work and roaming nearby. */
bool game_fogged(const Game *g,const Entity *e)
{
    for (int i=0;i<g->object_count;i++) {
        const Entity *fog=&g->objects[i];
        if (fog->type->effect==EFFECT_FOG && fog->kind==BARRIER && !fog->done && vec2_distance(e->p,fog->p)<105) return true;
    }
    return false;
}

bool game_burning_log(const Entity *e) { return e->fire && e->kind==BARRIER && e->type->burnable; }

bool game_job(const Game *g,const Entity *e)
{
    unsigned flags=game_job_definition(e->kind)->flags;
    return (flags&JOB_CLICK) || ((flags&JOB_REPAIR) && g->rules->economy && !e->operational) ||
        ((flags&JOB_ATV) && e->transport==TRANSPORT_ATV);
}
