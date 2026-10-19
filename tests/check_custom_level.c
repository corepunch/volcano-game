/* Exercise compiled source through the production decoder, UI and scheduler. */
#include "../src/game.c"

char asset_root[PATH_CAPACITY],last_error[1024];
SDL_Renderer *renderer;

static bool check(bool condition,const char *message)
{
    return condition?true:fail("Custom level regression: %s",message);
}
static bool play(const char *path)
{
    if (!game_init_file(path)) return false;
    if (!check(game.early && game.campaign==0 && game.goal==2 && game.food==20 && game.wood==0,
               "compiled mission settings")) return false;
    int tree=test_entity(4),bridge=test_entity(5);
    float length;Vec2 route_points[MAX_PATH];int count;
    if (!check(!route_to(&game.workers[0],game.objects[test_entity(6)].p,-1,route_points,&count,&length) &&
               !route_to(&game.workers[0],game.objects[test_entity(7)].p,-1,route_points,&count,&length),
               "both rescues start behind barriers")) return false;
    test_click_entity(bridge);
    if (!check(!game.objects[bridge].assigned && game.wood==0,"bridge needs earned wood")) return false;
    test_click_entity(tree);
    if (!check(game.objects[tree].assigned && game.food==0,"tree reserves the initial food")) return false;
    for (int tick=0;tick<1000 && !game.objects[tree].done;tick++) game_update(.1);
    if (!check(game.objects[tree].done && game.wood==20,"tree yields bridge budget")) return false;
    for (int tick=0;tick<3000 && !game.won;tick++) {
        if (tick%10==0 && !game.objects[bridge].assigned && !game.objects[bridge].done) test_click_entity(bridge);
        game_update(.1);
    }
    if (!check(game.won && game.rescued==2 && game.wood==0 && game.food==0,"mission completes through normal play")) return false;
    game_pointer(683,GAME_HEIGHT-350,false);
    if (!check(hud_button()==-1,"custom victory has one replay button")) return false;
    if (!test_frame("custom-level-complete")) return false;
    game_key(SDLK_RETURN);
    if (!check(!game.won && !game.time && *game.custom_path && game.food==20,"completion restarts custom file")) return false;
    game_key(SDLK_p);game_update(.1);
    if (!check(game.time==0,"custom pause")) return false;
    game_key(SDLK_r);
    if (!check(!game.paused && !game.time && game.food==20,"custom restart")) return false;
    Game *previous=current_game;
    if (!check(!game_init_file("build/no-such-custom-level.lvl") && previous==current_game,"failed load preserves previous game")) return false;
    last_error[0]=0;
    return true;
}
static bool waterfall_pixels(uint32_t *hash,int *lit)
{
    SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);paint_waterfalls();
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,GAME_WIDTH,GAME_HEIGHT,32,SDL_PIXELFORMAT_RGBA32);
    if (!surface) return false;
    bool ok=SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_RGBA32,surface->pixels,surface->pitch)==0;
    *hash=2166136261u;*lit=0;int brightest=0;
    if (ok) for (int y=0;y<GAME_HEIGHT;y++) for (int x=0;x<GAME_WIDTH;x++) {
        const Uint8 *p=(const Uint8 *)surface->pixels+y*surface->pitch+x*4;
        if (p[0] || p[1] || p[2]) {
            (*lit)++;if (p[0]>brightest) brightest=p[0];
            if (p[0]!=p[1] || p[1]!=p[2]) ok=fail("Foam contains a colored or gray-texture tint");
            bool in_pool=false;
            for (int i=0;i<game.waterfall_count;i++) {
                const Waterfall *w=&game.waterfalls[i];float base=w->p.y-w->height,world_y=GAME_HEIGHT-y;
                if (fabsf(x-w->p.x-w->drift)<w->width*.6f+20 && world_y<=base+(w->centered?14:2) &&
                    world_y>=base-(w->centered?14:45)) in_pool=true;
            }
            if (!in_pool) ok=fail("Waterfall particles escaped the bottom foam pool");
        }
        for (int c=0;c<3;c++) *hash=(*hash^p[c])*16777619u;
    }
    SDL_FreeSurface(surface);return ok && check(brightest>=220,"foam has bright white highlights") && !last_error[0];
}
static bool waterfall_check(const char *path)
{
    if (!game_init_file(path)) return false;
    if (!check(game.waterfall_count==6,"six authored foam emitters")) return false;
    uint32_t initial,paused,moving,restarted;int lit;
    if (!waterfall_pixels(&initial,&lit) || !check(lit>1000,"white foam is visible at the waterfall feet")) return false;
    game_key(SDLK_p);game_update(.1);
    if (!waterfall_pixels(&paused,&lit) || !check(initial==paused,"paused waterfalls are stationary")) return false;
    game_key(SDLK_p);game_update(.1);
    if (!waterfall_pixels(&moving,&lit) || !check(initial!=moving,"waterfall particles animate")) return false;
    game_key(SDLK_r);
    if (!waterfall_pixels(&restarted,&lit) || !check(initial==restarted,"waterfall particles restart deterministically")) return false;
    puts("PASS: white foam stays at the six authored impact points; no falling-water particles; animation, pause and restart verified from pixels.");
    return true;
}
static bool play_emerald(const char *path)
{
    if (!waterfall_check(path)) return false;
    const uint32_t orders[3][3]={{30,32,34},{34,30,32},{32,34,30}};
    for (int order=0;order<3;order++) {
        if (!game_init_file(path)) return false;
        if (!check(game.local_background && game.goal==7 && game.animals==2 && game.food==40 && game.wood==0,
                   "Emerald Falls source settings")) return false;
        for (int seed=50;seed<=56;seed++) {
            int entity=test_entity((uint32_t)seed);float length;int count;Vec2 points[MAX_PATH];
            if (!check(entity>=0 && !route_to(&game.workers[0],game.objects[entity].p,-1,points,&count,&length),
                       "every rescue is gated at the start")) return false;
        }
        if (!order && !test_frame("emerald-falls-start")) return false;
        bool paid_east=false;
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
            if (game.objects[test_entity(23)].assigned) paid_east=true;
            if (!check(game.food>=0 && game.wood>=0 && game.money>=0,"Emerald budget never negative")) return false;
        }
        if (!check(game.won && game.rescued==7 && game.animals_rescued==2 && game.worker_count==3 && paid_east,
                   "Emerald rescue puzzle completes without injected resources")) {
            fprintf(stderr,"order=%d rescued=%d animals=%d food=%d wood=%d money=%d time=%.1f\n",order,game.rescued,game.animals_rescued,game.food,game.wood,game.money,game.time);
            return false;
        }
        if (!order && !test_frame("emerald-falls-complete")) return false;
        printf("PASS: Emerald Falls production order %d completes 7 people and 2 animals in %.1f simulated seconds.\n",order+1,game.time);
    }
    game_key(SDLK_r);
    return check(!game.won && !game.time && game.food==40 && game.wood==0,"Emerald restart resets puzzle");
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
    if ((argc!=3 && argc!=4) || (argc==4 && strcmp(argv[3],"--emerald")) || strlen(argv[1])>=sizeof asset_root) return 2;
    strcpy(asset_root,argv[1]);
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)) return 2;
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,GAME_WIDTH,GAME_HEIGHT,32,SDL_PIXELFORMAT_RGBA32);
    renderer=surface?SDL_CreateSoftwareRenderer(surface):NULL;
    bool ok=renderer && resources_init();
    if (ok) {SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);ok=argc==4?play_emerald(argv[2]):play(argv[2]);}
    if (!ok) fprintf(stderr,"%s\n",last_error);
    else puts("PASS: custom source mission, dependency gates, exact budget, automatic rescue, victory, pause and restart.");
    free(current_game);resources_destroy();SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);SDL_Quit();
    return ok?0:1;
}
