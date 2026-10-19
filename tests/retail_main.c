/* Retail regression runner. Requires the original Rescue Team resources:
 *   build/test_retail reference/rescue-team
 * Kept out of the shipping executable; mission and core tests run without it. */
#include "game_test.h"
#include <stdio.h>
#include <string.h>

char asset_root[PATH_CAPACITY],last_error[1024];
SDL_Renderer *renderer;

int main(int argc,char **argv)
{
    if (argc!=2 || strlen(argv[1])>=sizeof asset_root) {
        fprintf(stderr,"Usage: %s RETAIL_ASSET_FOLDER\n",argv[0]);return 2;
    }
    strcpy(asset_root,argv[1]);
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)) return 2;
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,GAME_WIDTH,GAME_HEIGHT,32,SDL_PIXELFORMAT_RGBA32);
    renderer=surface?SDL_CreateSoftwareRenderer(surface):NULL;
    bool ok=renderer && resources_init();
    if (ok) {
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
        ok=resources_selftest() && level_selftest(asset_root) && game_selftest();
    }
    if (!ok) fprintf(stderr,"%s\n",last_error[0]?last_error:"Retail regression failed");
    game_destroy();resources_destroy();
    if (renderer) SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);SDL_Quit();
    return ok?0:1;
}
