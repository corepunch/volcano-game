/* Custom art must remain a real clickable, paid, removable route blocker. */
#include "game_test.h"
#include <stdio.h>
#include <string.h>

char asset_root[PATH_CAPACITY],last_error[1024];
SDL_Renderer *renderer;

static bool require(bool condition,const char *message)
{
    return condition?true:game_fail("Coaster debris: %s",message);
}

static bool play(const char *path)
{
    if (!game_init_file(path)) return false;
    SDL_Rect variants[10];int jobs=0,unique=0;
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];if (e->kind!=BARRIER) continue;
        if (!require(e->visual>=0 && *e->presentation_title,"every blocker has custom art and its own title")) return false;
        const MapVisual *v=&game.scene.visuals[e->visual];
        if (!require(v->kind==MAP_SPRITE && v->stage==MAP_WORLD && v->p.x==e->p.x && v->p.y==e->p.y &&
            strstr(v->image,"twisted-tracks-debris.png") && v->region.w*v->scale_x>=70 && v->region.w*v->scale_x<=90,
            "atlas, position and authored scale resolve correctly")) return false;
        bool seen=false;
        for (int j=0;j<unique;j++) if (!memcmp(&variants[j],&v->region,sizeof v->region)) seen=true;
        if (!seen) variants[unique++]=v->region;
        game_pointer(e->p.x,GAME_HEIGHT-e->p.y,false);
        if (!require(game_hovered_object(game_current())==i,"natural obstacle center is clickable")) return false;
        for (int j=0;j<game.object_count;j++) if (j!=i && game.objects[j].visual==e->visual)
            return game_fail("Coaster debris: linked art has a duplicate passive entity");
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
        if (!require(!game.won && game.food>=0 && game.wood>=0 && game.money>=0,"last rescue remains gated; budget stays valid")) return false;
    }
    if (!require(all_other,"all nine other blockers clear through ordinary scheduling")) return false;
    game_pointer(-100,-100,false);
    if (!test_frame("twisted-tracks-cleared-interior")) return false;
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];if (e->kind!=BARRIER || i==car) continue;
        game_pointer(e->p.x,GAME_HEIGHT-e->p.y,false);
        if (!require(e->done && e->hidden && !e->queued && !e->assigned && game_hovered_object(game_current())!=i,
            "clearing removes art, job state and click target")) return false;
    }
    bool paid=false;
    for (int tick=0;tick<3000 && !paid;tick++) {
        bool idle=false;
        for (int i=0;i<game.worker_count;i++) if (game.workers[i].phase==IDLE) idle=true;
        if (idle && game.food>=40 && game.money>=100) {
            int food=game.food,money=game.money;
            test_click_entity(car);
            if (!require(game.objects[car].assigned && game.food==food-40 && game.money==money-100,
                "custom car reserves the ordinary heavy-job price exactly once")) return false;
            int reserved_food=game.food,reserved_money=game.money;
            test_click_entity(car);
            if (!require(game.food==reserved_food && game.money==reserved_money,"second click cannot pay twice")) return false;
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
        fprintf(stderr,"final: shot=%d hidden=%d won=%d people=%d animals=%d food=%d money=%d\n",cleared_shot,game.objects[car].hidden,game.won,game.rescued,game.animals_rescued,game.food,game.money);
    if (!require(cleared_shot && game.objects[car].hidden && game.won && game.rescued==10 && game.animals_rescued==2,
        "all ten debris sprites disappear and every rescue completes")) return false;
    puts("PASS: all eight atlas variants are clickable; ten paid jobs clear through normal play; no passive duplicates, stale art, double charge or blocked return.");
    return true;
}

int main(int argc,char **argv)
{
    if (argc!=3 || strlen(argv[1])>=sizeof asset_root) return 2;
    strcpy(asset_root,argv[1]);
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)) return 2;
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,GAME_WIDTH,GAME_HEIGHT,32,SDL_PIXELFORMAT_RGBA32);
    renderer=surface?SDL_CreateSoftwareRenderer(surface):NULL;
    bool ok=renderer && resources_init();
    if (ok) {SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);ok=play(argv[2]);}
    if (!ok) fprintf(stderr,"%s\n",last_error);
    game_destroy();resources_destroy();SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);SDL_Quit();
    return ok?0:1;
}
