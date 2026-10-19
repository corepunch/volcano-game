/* Architecture guarantees, on the project's own First Rescue mission:
 * worlds are isolated values, art never changes rules, and a failed load
 * leaves the running world (and its art) untouched. */
#include "game_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool isolated_world_check(const char *mission,bool barrier)
{
    Game *active=game_current();double active_time=active->time;int active_completed=active->completed;
    LevelSpec spec;Game *world=NULL;bool ok=false;
    if (!mission_load(mission,&spec) || !(world=game_world_create(&spec))) return false;
    if (game_current()!=active) { game_fail("Candidate world became active");goto done; }
    int target=-1,worker=-1;
    for (int i=0;i<world->object_count && target<0;i++) {
        Entity *e=&world->objects[i];
        bool candidate=barrier?e->kind==BARRIER && e->type->reward.wood>0:e->kind==RESOURCE;
        if (candidate && game_dispatch(world,i,true)) target=i;
    }
    if (target<0) { game_fail("Isolated test found no reachable job");goto done; }
    for (int i=0;i<world->worker_count;i++) if (world->workers[i].target==target) worker=i;
    if (worker<0) { game_fail("Isolated dispatch lost its worker");goto done; }
    Entity *e=&world->objects[target];
    Stock cost=game_job_cost(world,e);
    Resource resource=barrier?RESOURCE_WOOD:e->type->resource;
    int before=world->stock.amount[resource],reward=barrier?e->type->reward.wood:e->type->amount;
    /* Replacing art must not alter routing compatibility, fees or delivery. */
    e->sprite="cosmetic_replacement";
    Stock renamed=game_job_cost(world,e);
    if (memcmp(&cost,&renamed,sizeof cost)) { game_fail("Artwork changed a resolved gameplay cost");goto done; }
    for (int tick=0;tick<9000 && world->workers[worker].phase!=IDLE;tick++) game_tick(world,TICK_SECONDS);
    if (world->workers[worker].phase!=IDLE || !e->done || world->stock.amount[resource]!=before+reward) {
        game_fail("Isolated callback lost its reward or return journey");goto done;
    }
    if (game_handle_key(world,SDLK_r)!=GAME_COMMAND_RESTART || game_current()!=active ||
        active->time!=active_time || active->completed!=active_completed) {
        game_fail("Updating a candidate world mutated the active world");goto done;
    }
    ok=true;
done:
    game_world_free(world);return ok;
}

/* Writes a mission that parses but fails while building the world. */
static bool failed_load_check(void)
{
    char path[]="/tmp/volcano-architecture-XXXXXX";
    int fd=mkstemp(path);
    if (fd<0) return game_fail("Cannot create rollback fixture");
    FILE *output=fdopen(fd,"w");
    bool written=output && fputs(
        "<mission format=\"volcano-mission-1\" retail-background=\"103\">\n"
        "  <settings rules=\"rescue\" people=\"1\"/>\n"
        "  <traffic>10,10 20,10</traffic>\n"
        "  <object id=\"1\" type=\"hq\" x=\"10\" y=\"10\"/>\n"
        "  <object id=\"2\" type=\"worker\" x=\"10\" y=\"10\"/>\n"
        "  <object id=\"3\" type=\"decor\" sprite=\"no_such_sprite\" x=\"20\" y=\"10\"/>\n"
        "</mission>\n",output)>=0;
    if (output) fclose(output);else close(fd);
    Game *active=game_current();double time=active->time;int completed=active->completed;
    bool loaded=written && game_init_file(path);
    bool ok=written && !loaded && game_current()==active && active->time==time && active->completed==completed &&
        strstr(last_error,"no_such_sprite");
    unlink(path);error_clear();
    /* The running level must still draw: its art layer was restored. */
    ok=ok && test_frame("architecture-after-failed-load");
    return ok?true:game_fail("Failed candidate load did not preserve the active world");
}

bool game_architecture_selftest(const char *mission)
{
    if (!game_init_file(mission) || !isolated_world_check(mission,true) || !isolated_world_check(mission,false) ||
        !failed_load_check()) return false;
    puts("PASS: isolated worlds, callback work/returns, artwork-independent costs/cargo, and rollback after a failed load.");
    return true;
}
