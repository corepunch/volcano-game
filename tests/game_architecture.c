#include "game_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int inventory(const Game *g,ResourceKind kind)
{
    switch (kind) {
    case RESOURCE_FOOD: return g->food;
    case RESOURCE_FUEL: return g->gas;
    case RESOURCE_MONEY: return g->money;
    case RESOURCE_WOOD: return g->wood;
    default: return 0;
    }
}

static bool isolated_world_check(int level,bool barrier)
{
    Game *active=game_current(),*world=calloc(1,sizeof *world);
    if (!world) return game_fail("Cannot allocate isolated test world");
    bool ok=false;double active_time=active->time;int active_completed=active->completed;
    if (!game_load_level(world,level,NULL)) goto done;
    if (game_current()!=active) {game_fail("Candidate load installed an active world");goto done;}
    int target=-1,worker=-1;
    for (int i=0;i<world->object_count;i++) {
        Entity *e=&world->objects[i];
        bool candidate=barrier?e->kind==BARRIER && e->obstacle->wood_reward>0:e->kind==RESOURCE;
        if (candidate && game_dispatch(world,i,true)) {target=i;break;}
    }
    if (target<0) {game_fail("Isolated test found no reachable job");goto done;}
    for (int i=0;i<world->worker_count;i++) if (world->workers[i].target==target) worker=i;
    if (worker<0) {game_fail("Isolated dispatch lost its worker");goto done;}
    Entity *e=&world->objects[target];
    Cost cost=game_job_cost(world,e);
    ResourceKind resource=barrier?RESOURCE_WOOD:e->pickup_resource;
    int before=inventory(world,resource),reward=barrier?e->obstacle->wood_reward:e->pickup_value;
    /* An art replacement must not alter routing compatibility, fees or delivery. */
    snprintf(e->anim,sizeof e->anim,"cosmetic_replacement");
    Cost renamed=game_job_cost(world,e);
    if (cost.food!=renamed.food || cost.wood!=renamed.wood || cost.money!=renamed.money) {
        game_fail("Artwork changed a resolved gameplay cost");goto done;
    }
    for (int tick=0;tick<1500 && world->workers[worker].phase!=IDLE;tick++) game_tick(world,.1);
    if (world->workers[worker].phase!=IDLE || !e->done || inventory(world,resource)!=before+reward) {
        game_fail("Isolated callback lost its reward or return journey");goto done;
    }
    if (game_handle_key(world,SDLK_r)!=GAME_COMMAND_RESTART || game_current()!=active ||
        active->time!=active_time || active->completed!=active_completed) {
        game_fail("Explicit world update mutated the active world");goto done;
    }
    ok=true;
done:
    free(world);return ok;
}

static bool failed_load_check(void)
{
    char path[]="/tmp/volcano-architecture-XXXXXX",rules[PATH_CAPACITY],source[PATH_CAPACITY];
    int fd=mkstemp(path);if (fd<0) return game_fail("Cannot create rollback test fixture");
    FILE *output=fdopen(fd,"wb"),*input=NULL;bool ok=false;
    if (!output) {close(fd);unlink(path);return game_fail("Cannot write rollback fixture");}
    snprintf(rules,sizeof rules,"%s.rules",path);
    snprintf(source,sizeof source,"%s/level/level000.lvl",asset_root);
    input=fopen(source,"rb");
    if (!input) goto done;
    char buffer[4096];size_t n;
    while ((n=fread(buffer,1,sizeof buffer,input))>0)
        if (fwrite(buffer,1,n,output)!=n) goto done;
    if (ferror(input)) goto done;
    fclose(input);input=NULL;
    if (fclose(output)) {output=NULL;goto done;}output=NULL;
    output=fopen(rules,"wb");if (!output) goto done;
    if (fputs("version=2\n",output)<0) goto done;
    if (fclose(output)) {output=NULL;goto done;}output=NULL;
    Game *active=game_current();double time=active->time;int completed=active->completed;
    bool loaded=game_init_file(path);
    /* This fails after decoding a valid binary, while building mission state. */
    ok=!loaded && game_current()==active && active->time==time && active->completed==completed &&
        strstr(last_error,"mission rules")!=NULL;
    last_error[0]=0;
done:
    if (input) fclose(input);
    if (output) fclose(output);
    unlink(path);unlink(rules);
    return ok?true:game_fail("Failed candidate load did not preserve the active world");
}

bool game_architecture_selftest(void)
{
    if (!isolated_world_check(campaign_index(1),true) || !isolated_world_check(0,false) || !failed_load_check()) return false;
    puts("PASS: isolated worlds, callback work/returns, artwork-independent costs/cargo, and rollback after mission-rule failure.");
    return true;
}
