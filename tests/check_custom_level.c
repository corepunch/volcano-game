/* Exercise compiled source through the production decoder, UI and scheduler. */
#include "game_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char asset_root[PATH_CAPACITY],last_error[1024];
SDL_Renderer *renderer;

static bool check(bool condition,const char *message)
{
    return condition?true:game_fail("Custom level regression: %s",message);
}
static bool play(const char *path)
{
    if (!game_init_file(path)) return false;
    if (!check(game.early && game.campaign==0 && game.goal==2 && game.food==20 && game.wood==0,
               "compiled mission settings")) return false;
    int tree=test_entity(4),bridge=test_entity(5);
    float length;Vec2 route_points[MAX_PATH];int count;
    if (!check(!game_route_to(game_current(), &game.workers[0],game.objects[test_entity(6)].p,-1,route_points,&count,&length) &&
               !game_route_to(game_current(), &game.workers[0],game.objects[test_entity(7)].p,-1,route_points,&count,&length),
               "both rescues start behind barriers")) return false;
    test_click_entity(bridge);
    if (!check(!game.objects[bridge].assigned && game.wood==0,"bridge needs earned wood")) return false;
    test_click_entity(tree);
    if (!check(game.objects[tree].assigned && game.food==0,"tree reserves the initial food")) return false;
    bool chopping=false;
    for (int tick=0;tick<1000 && !game.objects[tree].done;tick++) {
        game_update(.1);
        for (int i=0;i<game.worker_count;i++) {
            Worker *w=&game.workers[i];
            if (w->phase==WORKING && w->target==tree)
                chopping|=!strncmp(game.objects[w->entity].anim,"worker_wood",11);
        }
    }
    if (!check(chopping,"tree clearing plays the axe animation")) return false;
    if (!check(game.objects[tree].done && game.wood==20,"tree yields bridge budget")) return false;
    for (int tick=0;tick<3000 && !game.won;tick++) {
        if (tick%10==0 && !game.objects[bridge].assigned && !game.objects[bridge].done) test_click_entity(bridge);
        game_update(.1);
    }
    if (!check(game.won && game.rescued==2 && game.wood==0 && game.food==0,"mission completes through normal play")) return false;
    game_pointer(683,GAME_HEIGHT-350,false);
    if (!check(game_hud_button(game_current())==-1,"custom victory has one replay button")) return false;
    if (!test_frame("custom-level-complete")) return false;
    game_key(SDLK_RETURN);
    if (!check(!game.won && !game.time && *game.custom_path && game.food==20,"completion restarts custom file")) return false;
    game_key(SDLK_p);game_update(.1);
    if (!check(game.time==0,"custom pause")) return false;
    game_key(SDLK_r);
    if (!check(!game.paused && !game.time && game.food==20,"custom restart")) return false;
    Game *previous=game_current();
    if (!check(!game_init_file("build/no-such-custom-level.lvl") && previous==game_current(),"failed load preserves previous game")) return false;
    last_error[0]=0;
    return true;
}
static bool foam_pixels(uint32_t *hash,int *lit)
{
    SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
    for (int i=0;i<game.scene.visual_count;i++) if (game.scene.visuals[i].kind==MAP_FOAM) map_visual_draw(&game.scene.visuals[i],game.time);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,GAME_WIDTH,GAME_HEIGHT,32,SDL_PIXELFORMAT_RGBA32);
    if (!surface) return false;
    bool ok=SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_RGBA32,surface->pixels,surface->pitch)==0;
    *hash=2166136261u;*lit=0;int brightest=0;
    if (ok) for (int y=0;y<GAME_HEIGHT;y++) for (int x=0;x<GAME_WIDTH;x++) {
        const Uint8 *p=(const Uint8 *)surface->pixels+y*surface->pitch+x*4;
        if (p[0] || p[1] || p[2]) {
            (*lit)++;if (p[0]>brightest) brightest=p[0];
            if (p[0]!=p[1] || p[1]!=p[2]) ok=game_fail("Foam contains a colored or gray-texture tint");
            bool in_pool=false;
            for (int i=0;i<game.scene.visual_count;i++) {
                const MapVisual *v=&game.scene.visuals[i];if (v->kind!=MAP_FOAM) continue;
                float world_y=GAME_HEIGHT-y;
                if (fabsf(x-v->p.x)<v->extent.x*.6f+20 && world_y<=v->p.y+14 && world_y>=v->p.y-14) in_pool=true;
            }
            if (!in_pool) ok=game_fail("Waterfall particles escaped the bottom foam pool");
        }
        for (int c=0;c<3;c++) *hash=(*hash^p[c])*16777619u;
    }
    SDL_FreeSurface(surface);return ok && check(brightest>=220,"foam has bright white highlights") && !last_error[0];
}
static bool scene_check(const char *path)
{
    if (!game_init_file(path)) return false;
    int foam=0;
    for (int i=0;i<game.scene.visual_count;i++) if (game.scene.visuals[i].kind==MAP_FOAM) foam++;
    if (!check(foam==6,"six authored foam emitters")) return false;
    uint32_t initial,paused,moving,restarted;int lit;
    if (!foam_pixels(&initial,&lit) || !check(lit>1000,"white foam is visible at the waterfall feet")) return false;
    game_key(SDLK_p);game_update(.1);
    if (!foam_pixels(&paused,&lit) || !check(initial==paused,"paused waterfalls are stationary")) return false;
    game_key(SDLK_p);game_update(.1);
    if (!foam_pixels(&moving,&lit) || !check(initial!=moving,"waterfall particles animate")) return false;
    game_key(SDLK_r);
    if (!foam_pixels(&restarted,&lit) || !check(initial==restarted,"waterfall particles restart deterministically")) return false;
    puts("PASS: white foam stays at the six authored impact points; no falling-water particles; animation, pause and restart verified from pixels.");
    return true;
}
static bool visual_hash(MapVisualKind kind,uint32_t *hash,int *lit)
{
    SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
    for (int i=0;i<game.scene.visual_count;i++) if (game.scene.visuals[i].kind==kind)
        map_visual_draw(&game.scene.visuals[i],game.time);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,GAME_WIDTH,GAME_HEIGHT,32,SDL_PIXELFORMAT_RGBA32);
    if (!surface) return false;
    bool ok=SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_RGBA32,surface->pixels,surface->pitch)==0;
    *hash=2166136261u;*lit=0;
    if (ok) for (int y=0;y<GAME_HEIGHT;y++) for (int x=0;x<GAME_WIDTH;x++) {
        const Uint8 *p=(const Uint8 *)surface->pixels+y*surface->pitch+x*4;
        if (p[0] || p[1] || p[2]) (*lit)++;
        for (int c=0;c<3;c++) *hash=(*hash^p[c])*16777619u;
    }
    SDL_FreeSurface(surface);return ok && !last_error[0];
}
static bool animated_scene_check(const char *path)
{
    if (!game_init_file(path)) return false;
    int counts[5]={0},world=0,proxies=0;
    uint32_t initial[5],paused,animated,restarted;int lit;
    for (int i=0;i<game.scene.visual_count;i++) {
        MapVisual *v=&game.scene.visuals[i];counts[v->kind]++;if (v->stage==MAP_WORLD) world++;
    }
    for (int i=0;i<game.object_count;i++) if (game.objects[i].visual>=0) proxies++;
    if (!check(counts[MAP_SPRITE]==0 && counts[MAP_SPARKLES]>=6 && counts[MAP_INSECTS]>=5 &&
        counts[MAP_MOTES]>0 && world==proxies,"baked foliage with independent ambient effects")) return false;
    for (int kind=0;kind<5;kind++) if (counts[kind] && (!visual_hash((MapVisualKind)kind,&initial[kind],&lit) ||
        !check(lit>0,"every placed visual kind renders"))) return false;
    game_key(SDLK_p);game_update(.1);
    for (int kind=0;kind<5;kind++) if (counts[kind] && (!visual_hash((MapVisualKind)kind,&paused,&lit) ||
        !check(paused==initial[kind],"paused scene is stable"))) return false;
    game_key(SDLK_p);for (int i=0;i<10;i++) game_update(.1);
    for (int kind=0;kind<5;kind++) if (counts[kind] && (!visual_hash((MapVisualKind)kind,&animated,&lit) ||
        !check(animated!=initial[kind],"each visual kind animates"))) return false;
    game_key(SDLK_r);
    for (int kind=0;kind<5;kind++) if (counts[kind] && (!visual_hash((MapVisualKind)kind,&restarted,&lit) ||
        !check(restarted==initial[kind],"scene restart is deterministic"))) return false;
    puts("PASS: foliage remains baked; foam, sparkles, original butterflies and motes render, animate, pause and restart.");
    return true;
}
static bool mayday_food_recovery(const char *path)
{
    if (!game_init_file(path)) return false;
    int north=test_entity(20),south=test_entity(22),tree=test_entity(21),rock=test_entity(23),cafe=test_entity(30);
    const uint32_t hover_ids[]={30,40,42};
    const char *hover_shots[]={"forest-mayday-repair-income","forest-mayday-wood-income","forest-mayday-money-income"};
    for (int i=0;i<3;i++) {
        Entity *e=&game.objects[test_entity(hover_ids[i])];game_pointer(e->p.x,GAME_HEIGHT-e->p.y,false);
        if (!test_frame(hover_shots[i])) return false;
    }
    test_click_entity(north);test_click_entity(south);
    for (int tick=0;tick<1000;tick++) game_update(.1);
    if (!check(game.objects[north].done && game.objects[south].done && game.food==0,
               "human opening spends starting food on the two first obstacles")) return false;
    test_click_entity(test_entity(40));test_click_entity(test_entity(41));
    for (int tick=0;tick<1000;tick++) game_update(.1);
    if (!check(game.wood==60 && game.food==0 && game.rescued==2,
               "reproduced screenshot state: zero food, sixty timber, two people rescued")) return false;
    game_pointer(game.objects[tree].p.x,GAME_HEIGHT-game.objects[tree].p.y,false);
    if (!test_frame("forest-mayday-tree-cost")) return false;
    test_click_entity(rock);test_click_entity(tree);
    if (!check(game.objects[tree].queued && !game.objects[tree].assigned &&
               !strcmp(game.message,"Required: 20 food"),
               "resource-blocked queue shows the full cost without strategy hints")) return false;
    if (!test_frame("forest-mayday-waiting-for-food")) return false;
    game_pointer(game.objects[rock].p.x,GAME_HEIGHT-game.objects[rock].p.y,false);
    if (!test_frame("forest-mayday-rock-cost")) return false;
    test_click_entity(cafe);
    if (!check(game.objects[cafe].assigned && game.wood==40,
               "an affordable repair dispatches past earlier resource-blocked jobs")) return false;
    bool production_shot=false,food_shot=false;
    for (int tick=0;tick<1500 && !game.objects[tree].done;tick++) {
        game_update(.1);
        Entity *producer=&game.objects[cafe],*food=&game.objects[test_entity(31)];
        if (producer->operational && !production_shot) {
            game_pointer(producer->p.x,GAME_HEIGHT-producer->p.y,false);
            if (!test_frame("forest-mayday-production-income")) return false;
            production_shot=true;
        }
        if (!food->hidden && !food_shot) {
            game_pointer(food->p.x,GAME_HEIGHT-food->p.y,false);
            if (!test_frame("forest-mayday-food-income")) return false;
            food_shot=true;
        }
    }
    if (!check(game.objects[cafe].operational && game.objects[tree].done && !game.objects[tree].queued,
               "cafe production automatically resumes the queued tree without another click")) return false;
    puts("PASS: reproduced the zero-food opening; cost-only feedback, cafe repair and automatic queued-job recovery verified through normal clicks.");
    return true;
}
static bool play_production_mission(const char *path,bool mayday)
{
    if (mayday && !mayday_food_recovery(path)) return false;
    if (!mayday && (!scene_check(path) || !animated_scene_check(path))) return false;
    const char *name=mayday?"Forest Mayday":"Emerald Falls";
    int goal=mayday?10:7;
    const uint32_t orders[3][3]={{30,32,34},{34,30,32},{32,34,30}};
    for (int order=0;order<3;order++) {
        if (!game_init_file(path)) return false;
        if (!check(game.local_background && game.goal==goal && game.animals==2 && game.food==40 && game.wood==0,
                   "production mission source settings")) return false;
        for (int seed=50;seed<50+goal;seed++) {
            int entity=test_entity((uint32_t)seed);float length;int count;Vec2 points[MAX_PATH];
            if (!check(entity>=0 && !game_route_to(game_current(), &game.workers[0],game.objects[entity].p,-1,points,&count,&length),
                       "every rescue is gated at the start")) return false;
        }
        if (!order && !test_frame(mayday?"forest-mayday-start":"emerald-falls-start")) return false;
        bool paid_east=false,repaired_shot=false;
        for (int tick=0;tick<6000 && !game.won;tick++) {
            if (tick%10==0) {
                for (int i=0;i<game.object_count;i++) {
                    Entity *e=&game.objects[i];
                    if (e->kind==RESOURCE && !e->hidden && !e->done && !e->assigned && !e->queued) test_click_entity(i);
                }
                for (int j=0;j<3;j++) {
                    int i=test_entity(orders[order][j]);Entity *e=&game.objects[i];
                    if (!e->operational && !e->assigned && !e->queued) test_click_entity(i);
                }
                for (int i=0;i<game.object_count;i++) {
                    Entity *e=&game.objects[i];
                    if ((e->kind==BARRIER || e->kind==ANIMAL) && !e->done && !e->hidden && !e->assigned && !e->queued) {
                        int money=game.money;test_click_entity(i);
                        if (e->seed==23 && game.money==money-100) paid_east=true;
                    }
                }
            }
            game_update(.1);
            if (mayday && !order && !repaired_shot && game.objects[test_entity(30)].operational &&
                game.objects[test_entity(32)].operational && game.objects[test_entity(34)].operational) {
                if (!test_frame("forest-mayday-buildings-repaired")) return false;
                repaired_shot=true;
            }
            if (game.objects[test_entity(23)].assigned) paid_east=true;
            if (!check(game.food>=0 && game.wood>=0 && game.money>=0,"mission budget never negative")) return false;
        }
        if (!check(game.won && game.rescued==goal && game.animals_rescued==2 && game.worker_count==3 && paid_east,
                   "production rescue puzzle completes without injected resources")) {
            fprintf(stderr,"order=%d rescued=%d animals=%d food=%d wood=%d money=%d time=%.1f\n",order,game.rescued,game.animals_rescued,game.food,game.wood,game.money,game.time);
            return false;
        }
        if (!order && !test_frame(mayday?"forest-mayday-complete":"emerald-falls-complete")) return false;
        printf("PASS: %s production order %d completes %d people and 2 animals in %.1f simulated seconds.\n",name,order+1,goal,game.time);
    }
    game_key(SDLK_r);
    return check(!game.won && !game.time && game.food==40 && game.wood==0,"mission restart resets puzzle");
}
/* These IDs are authored explicitly by Ancient Echoes. Exercise the actual
 * UI, route graph and economy; no inventory grants or completion shortcuts. */
static bool play_ancient_echoes(const char *path)
{
    if (!game_init_file(path)) return false;
    test_click_entity(test_entity(20));test_click_entity(test_entity(22));
    for (int tick=0;tick<1200;tick++) game_update(.1);
    if (!check(game.objects[test_entity(20)].done && game.objects[test_entity(22)].done && game.food==0,
               "two plausible opening jobs exhaust food")) return false;
    const uint32_t caches[]={40,41,43};
    for (int j=0;j<3;j++) test_click_entity(test_entity(caches[j]));
    for (int tick=0;tick<1200;tick++) game_update(.1);
    if (!check(game.wood==80 && game.food==0,"delivered timber finances food recovery")) return false;
    test_click_entity(test_entity(21));test_click_entity(test_entity(23));
    if (!check(game.objects[test_entity(21)].queued && !game.objects[test_entity(21)].assigned,
               "food-starved clearing job waits in queue")) return false;
    test_click_entity(test_entity(30));
    if (!check(game.objects[test_entity(30)].assigned && game.wood==60,
               "affordable cafe repair dispatches past blocked jobs")) return false;
    for (int tick=0;tick<1600 && !game.objects[test_entity(21)].done;tick++) game_update(.1);
    if (!check(game.objects[test_entity(30)].operational && game.objects[test_entity(21)].done,
               "renewable food resumes queued tourist rescue without another click")) return false;
    const uint32_t orders[3][3]={{30,32,34},{34,30,32},{32,34,30}};
    for (int order=0;order<3;order++) {
        if (!game_init_file(path)) return false;
        if (!check(game.local_background && game.goal==9 && game.animals==2 && game.worker_count==3 &&
                   game.food==40 && game.wood==0 && game.money==0,"Ancient Echoes initial mission")) return false;
        for (uint32_t seed=50;seed<=61;seed++) {
            if (seed==59) continue;
            int i=test_entity(seed);float length;int count;Vec2 points[MAX_PATH];
            if (!check(i>=0 && !game_route_to(game_current(),&game.workers[0],game.objects[i].p,-1,points,&count,&length),
                       "every tourist, recruit and animal starts gated")) return false;
        }
        if (!order && !test_frame("ancient-echoes-start")) return false;
        bool repaired=false,paid=false;
        for (int tick=0;tick<7500 && !game.won;tick++) {
            if (tick%10==0) {
                for (int i=0;i<game.object_count;i++) {
                    Entity *e=&game.objects[i];
                    if (e->kind==RESOURCE && !e->hidden && !e->done && !e->assigned && !e->queued) test_click_entity(i);
                }
                for (int j=0;j<3;j++) {
                    int i=test_entity(orders[order][j]);Entity *e=&game.objects[i];
                    if (!e->operational && !e->assigned && !e->queued) test_click_entity(i);
                }
                for (int i=0;i<game.object_count;i++) {
                    Entity *e=&game.objects[i];
                    if ((e->kind==BARRIER || e->kind==ANIMAL) && !e->done && !e->hidden && !e->assigned && !e->queued)
                        test_click_entity(i);
                }
            }
            game_update(.1);
            if (game.objects[test_entity(23)].assigned) paid=true;
            if (!check(game.food>=0 && game.wood>=0 && game.money>=0,"nonnegative rescue budget")) return false;
            if (!order && !repaired && game.objects[test_entity(30)].operational &&
                game.objects[test_entity(32)].operational && game.objects[test_entity(34)].operational) {
                if (!test_frame("ancient-echoes-repaired")) return false;
                repaired=true;
            }
        }
        if (!check(game.won && game.rescued==9 && game.animals_rescued==2 && game.worker_count==3 && paid,
                   "Ancient Echoes completes all objectives through ordinary clicks")) {
            fprintf(stderr,"order=%d rescued=%d animals=%d food=%d wood=%d money=%d time=%.1f\n",
                    order,game.rescued,game.animals_rescued,game.food,game.wood,game.money,game.time);
            return false;
        }
        if (!order && !test_frame("ancient-echoes-complete")) return false;
        printf("PASS: Ancient Echoes repair order %d rescues nine people and two animals in %.1fs.\n",order+1,game.time);
    }
    game_key(SDLK_r);game_key(SDLK_p);game_update(1);
    if (!check(game.time==0 && game.paused,"Ancient Echoes pause")) return false;
    game_key(SDLK_r);
    return check(!game.paused && !game.won && game.time==0 && game.food==40 && game.wood==0 && game.rescued==0,
                 "Ancient Echoes restart restores all initial settings");
}
int main(int argc,char **argv)
{
    if (argc>2 && !strcmp(argv[1],"--decode")) {
        Level *level=malloc(sizeof *level);if (!level) return 2;
        for (int i=2;i<argc;i++) if (!level_load(argv[i],level,last_error,sizeof last_error)) {
            fprintf(stderr,"%s: %s\n",argv[i],last_error);free(level);return 1;
        }
        free(level);printf("Production C reader accepted %d compiled binaries\n",argc-2);return 0;
    }
    if ((argc!=3 && argc!=4) || (argc==4 && strcmp(argv[3],"--emerald") && strcmp(argv[3],"--mayday") && strcmp(argv[3],"--ancient")) || strlen(argv[1])>=sizeof asset_root) return 2;
    strcpy(asset_root,argv[1]);
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)) return 2;
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,GAME_WIDTH,GAME_HEIGHT,32,SDL_PIXELFORMAT_RGBA32);
    renderer=surface?SDL_CreateSoftwareRenderer(surface):NULL;
    bool ok=renderer && resources_init();
    if (ok) {
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
        ok=argc==4?(!strcmp(argv[3],"--ancient")?play_ancient_echoes(argv[2]):
            play_production_mission(argv[2],!strcmp(argv[3],"--mayday"))):play(argv[2]);
    }
    if (!ok) fprintf(stderr,"%s\n",last_error);
    else puts("PASS: custom source mission, dependency gates, exact budget, automatic rescue, victory, pause and restart.");
    game_destroy();resources_destroy();SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);SDL_Quit();
    return ok?0:1;
}
