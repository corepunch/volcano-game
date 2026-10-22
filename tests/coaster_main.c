/* Mission-defined debris archetypes must be real clickable, paid, removable route blockers. */
#include "game_test.h"
#include <stdio.h>
#include <string.h>


static bool require(bool condition,const char *message)
{
    return condition?true:game_fail("Coaster debris: %s",message);
}

static bool play(const char *path)
{
    if (!game_init_file(path)) return false;
    const char *variants[10];int jobs=0,unique=0;
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];if (e->kind!=BARRIER) continue;
        const char *art=e->type->sprite;const Sprite *sprite=art?sprite_find(art):NULL;
        if (!require(e->type->base && e->type->title && strcmp(e->type->title,e->type->base->title),
            "every blocker is a mission archetype with its own title")) return false;
        if (!require(sprite && !strncmp(art,"twisted-tracks/",15) && strstr(sprite->texture,"debris.png") &&
            sprite->frames[0].w>=70 && sprite->frames[0].w<=90,"atlas sprite and authored scale resolve correctly")) return false;
        bool seen=false;
        for (int j=0;j<unique;j++) if (!strcmp(variants[j],art)) seen=true;
        if (!seen) variants[unique++]=art;
        game_pointer(e->p.x,render_height()-e->p.y,false);
        if (!require(game_hovered_object(game_current())==i,"natural obstacle center is clickable")) return false;
        jobs++;
    }
    if (!require(jobs==10 && unique==8,"ten jobs use all eight distinct authored variants")) return false;
    int car=test_entity(23);
    /* Leave the common train gate until last, so ordinary play can verify all
     * optional branches before automatic civilian evacuation triggers victory. */
    bool all_other=false;
    for (int tick=0;tick<6500 && !all_other;tick++) {
        if (tick%10==0) for (int i=0;i<game.object_count;i++) {
            Entity *e=&game.objects[i];
            bool job=e->kind==RESOURCE || (e->kind==BUILDING && !e->operational) ||
                (e->kind==BARRIER && i!=car) || e->kind==ANIMAL;
            if (job && !e->hidden && !e->done && !e->assigned && !e->queued) test_click_entity(i);
        }
        game_update(.1);all_other=true;
        for (int i=0;i<game.object_count;i++) {
            Entity *e=&game.objects[i];if (e->kind==BARRIER && i!=car && !e->done) all_other=false;
        }
        if (!require(!game.won && game.stock.food>=0 && game.stock.wood>=0 && game.stock.money>=0,"last rescue remains gated; budget stays valid")) return false;
    }
    if (!require(all_other,"all nine other blockers clear through ordinary scheduling")) return false;
    game_pointer(-100,-100,false);
    if (!test_frame("twisted-tracks-cleared-interior")) return false;
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];if (e->kind!=BARRIER || i==car) continue;
        game_pointer(e->p.x,render_height()-e->p.y,false);
        if (!require(e->done && e->hidden && !e->queued && !e->assigned && game_hovered_object(game_current())!=i,
            "clearing removes art, job state and click target")) return false;
    }
    bool paid=false;
    for (int tick=0;tick<3000 && !paid;tick++) {
        bool idle=false;
        for (int i=0;i<game.worker_count;i++) if (game.workers[i].phase==IDLE) idle=true;
        if (idle && game.stock.food>=40 && game.stock.money>=100) {
            int food=game.stock.food,money=game.stock.money;
            test_click_entity(car);
            if (!require(game.objects[car].assigned && game.stock.food==food-40 && game.stock.money==money-100,
                "coaster car reserves its heavy-job price exactly once")) return false;
            int reserved_food=game.stock.food,reserved_money=game.stock.money;
            test_click_entity(car);
            if (!require(game.stock.food==reserved_food && game.stock.money==reserved_money,"second click cannot pay twice")) return false;
            paid=true;
        }
        game_update(.1);
    }
    if (!require(paid,"renewable supplies finance the final car job")) return false;
    bool cleared_shot=false;
    for (int tick=0;tick<3000 && !game.won;tick++) {
        if (tick%10==0) for (int i=0;i<game.object_count;i++) {
            Entity *e=&game.objects[i];
            if (e->kind==ANIMAL && !e->done && !e->assigned && !e->queued) test_click_entity(i);
        }
        game_update(.1);
        if (game.objects[car].done && !cleared_shot) {
            game_pointer(-100,-100,false);
            if (!test_frame("twisted-tracks-all-debris-removed")) return false;
            cleared_shot=true;
        }
    }
    if (!(cleared_shot && game.objects[car].hidden && game.won && game.rescued==10 && game.animals_rescued==2))
        fprintf(stderr,"final: shot=%d hidden=%d won=%d people=%d animals=%d food=%d money=%d\n",cleared_shot,game.objects[car].hidden,game.won,game.rescued,game.animals_rescued,game.stock.food,game.stock.money);
    if (!require(cleared_shot && game.objects[car].hidden && game.won && game.rescued==10 && game.animals_rescued==2,
        "all ten debris sprites disappear and every rescue completes")) return false;
    puts("PASS: all eight atlas variants are clickable; ten paid jobs clear through normal play; no stale art, double charge or blocked return.");
    return true;
}

int main(int argc,char **argv)
{
    if (argc!=3) { fprintf(stderr,"Usage: %s ASSET_FOLDER missions/twisted-tracks/mission.xml\n",argv[0]);return 2; }
    const char *mounts[]={argv[1]};
    bool ok=test_startup(mounts,1) && play(argv[2]);
    if (!ok) fprintf(stderr,"%s\n",last_error);
    test_shutdown();
    return ok?0:1;
}
