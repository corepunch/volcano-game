/* Helpers shared by the mission and retail regression runners. */
#include "game_test.h"
#include <stdio.h>
#include <stdlib.h>

static SDL_Surface *surface;

bool test_startup(const char *const *mounts,int count)
{
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)) return error_set("%s",SDL_GetError());
    surface=SDL_CreateRGBSurfaceWithFormat(0,GAME_WIDTH,GAME_HEIGHT,32,SDL_PIXELFORMAT_RGBA32);
    renderer=surface?SDL_CreateSoftwareRenderer(surface):NULL;
    if (!renderer) return error_set("Cannot create a software renderer: %s",SDL_GetError());
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    GameConfig config={.content="content"};
    for (int i=0;i<count;i++) config.mounts[config.mount_count++]=mounts[i];
    return game_startup(&config);
}

void test_shutdown(void)
{
    game_shutdown();
    if (renderer) SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);renderer=NULL;surface=NULL;SDL_Quit();
}

bool test_frame(const char *name)
{
    SDL_SetRenderDrawColor(renderer,14,30,43,255);SDL_RenderClear(renderer);game_draw();
    if (last_error[0]) return false;
    if (getenv("VOLCANO_TEST_SHOTS")) {
        char path[256];snprintf(path,sizeof path,"build/%s.bmp",name);
        if (!renderer_screenshot(path)) return game_fail("Cannot save test frame %s",name);
    }
    return true;
}

int test_entity(uint32_t id)
{
    for (int i=0;i<game.object_count;i++) if (game.objects[i].id==id) return i;
    return -1;
}

void test_click_entity(int index)
{
    Vec2 p=game.objects[index].p;game_click(p.x,GAME_HEIGHT-p.y);
}

bool test_choose_building(int lot,int slot)
{
    test_click_entity(lot);
    if (game.build_menu!=lot) return false;
    Vec2 p=game_build_button_position(game_current(),slot);game_click(p.x,GAME_HEIGHT-p.y);
    return game.objects[lot].assigned;
}

const char *test_sprite(int index)
{
    static char name[160];
    if (!game_entity_sprite(game_current(),&game.objects[index],name,sizeof name)) name[0]=0;
    return name;
}
