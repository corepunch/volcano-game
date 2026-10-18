#include "volcano.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

char asset_root[PATH_CAPACITY];
char last_error[1024];
SDL_Renderer *renderer;
static bool integer(const char *text,int *value) {
    char *end;errno=0;long number=strtol(text,&end,10);
    if(errno||*end||number<0||number>10000000)return false;*value=(int)number;return true;
}
static bool resource_folder(const char *path) {
    char probe[PATH_CAPACITY];struct stat info;
    int n=snprintf(probe,sizeof probe,"%s/texture/lazy",path);
    return n>0 && n<(int)sizeof probe && !stat(probe,&info) && S_ISDIR(info.st_mode);
}
static void default_assets(void) {
    snprintf(asset_root,sizeof asset_root,"resources");
    if(resource_folder(asset_root))return;
    char *base=SDL_GetBasePath();
    if(base){
        const char *locations[]={"../resources","../Resources"};
        for(size_t i=0;i<sizeof locations/sizeof *locations;i++){
            snprintf(asset_root,sizeof asset_root,"%s%s",base,locations[i]);
            if(resource_folder(asset_root)){SDL_free(base);return;}
        }
        SDL_free(base);
    }
    snprintf(asset_root,sizeof asset_root,"resources");
}
int main(int argc,char **argv) {
    const char *assets=getenv("RESCUE_TEAM_ASSETS"),*shot=NULL;
    int frames=0,level=0,result=1;bool hidden=false,software=false,selftest=false,inspect=false;
    SDL_Window *window=NULL;
    if(assets)snprintf(asset_root,sizeof(asset_root),"%s",assets);
    else default_assets();
    for(int i=1;i<argc;i++){
        const char *arg=argv[i];
        if(!strcmp(arg,"--assets")||!strcmp(arg,"--screenshot")||!strcmp(arg,"--frames")||!strcmp(arg,"--level")){
            if(i+1>=argc){fprintf(stderr,"Missing value for %s\n",arg);return 1;}const char *value=argv[++i];
            if(!strcmp(arg,"--assets")){if(strlen(value)>=sizeof(asset_root)){fprintf(stderr,"Resource path too long\n");return 1;}strcpy(asset_root,value);}
            else if(!strcmp(arg,"--screenshot"))shot=value;
            else if(!integer(value,!strcmp(arg,"--frames")?&frames:&level)){fprintf(stderr,"Invalid number for %s\n",arg);return 1;}
        }else if(!strcmp(arg,"--hidden"))hidden=true;
        else if(!strcmp(arg,"--software"))software=true;
        else if(!strcmp(arg,"--self-test"))selftest=true;
        else if(!strcmp(arg,"--inspect"))inspect=true;
        else if(!strcmp(arg,"--help")){
            puts("Volcano SDL2 - C-only Rescue Team 5 prototype\n"
                 "Usage: volcano [--assets ORIGINAL_FOLDER] [--level N]\n"
                 "               [--frames N] [--screenshot FILE.bmp] [--hidden]\n"
                 "               [--software] [--self-test] [--inspect]\n"
                 "Resources default to $RESCUE_TEAM_ASSETS or local resources/.\n"
                 "Click obstacles, supplies or survivors; Space pause, R restart,\n"
                 "F1 paths, Escape pause/resume. Close the window to quit.");return 0;
        }else {fprintf(stderr,"Unknown option: %s\n",arg);return 1;}
    }
    if(shot&&!frames)frames=1;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)){snprintf(last_error,sizeof(last_error),"%s",SDL_GetError());goto done;}
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
    window=SDL_CreateWindow("Volcano - Rescue Team 5 - SDL2 / C",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,GAME_WIDTH,GAME_HEIGHT,SDL_WINDOW_RESIZABLE|(hidden?SDL_WINDOW_HIDDEN:0));
    if(!window){snprintf(last_error,sizeof(last_error),"%s",SDL_GetError());goto done;}
    renderer=SDL_CreateRenderer(window,-1,software?SDL_RENDERER_SOFTWARE:SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!renderer){snprintf(last_error,sizeof(last_error),"%s",SDL_GetError());goto done;}
    SDL_RenderSetLogicalSize(renderer,GAME_WIDTH,GAME_HEIGHT);SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    if(!resources_init())goto done;
    if(inspect&&!resources_selftest())goto done;
    if(selftest){
        if(!resources_selftest()||!level_selftest(asset_root)||!game_selftest())goto done;
        frames=1;
    }
    if(!game_init(level))goto done;
    Uint64 previous=SDL_GetPerformanceCounter();bool running=true;int frame=0;
    while(running){
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            if(event.type==SDL_QUIT)running=false;
            if(event.type==SDL_MOUSEMOTION)game_pointer((float)event.motion.x,(float)event.motion.y,(event.motion.state&SDL_BUTTON_LMASK)!=0);
            if(event.type==SDL_MOUSEBUTTONDOWN&&event.button.button==SDL_BUTTON_LEFT)game_pointer((float)event.button.x,(float)event.button.y,true);
            if(event.type==SDL_MOUSEBUTTONUP&&event.button.button==SDL_BUTTON_LEFT)game_click((float)event.button.x,(float)event.button.y);
            if(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_LEAVE)game_pointer(-1,-1,false);
            if(event.type==SDL_KEYDOWN&&!event.key.repeat)game_key(event.key.keysym.sym);
        }
        Uint64 now=SDL_GetPerformanceCounter();double dt=(double)(now-previous)/SDL_GetPerformanceFrequency();previous=now;
        game_update(frames?1./60:fmin(dt,0.05));
        SDL_SetRenderDrawColor(renderer,14,30,43,255);SDL_RenderClear(renderer);game_draw();
        if(last_error[0])goto done;
        if(frames&&frame+1>=frames&&shot&&!renderer_screenshot(shot)){snprintf(last_error,sizeof(last_error),"Cannot save screenshot: %s",SDL_GetError());goto done;}
        SDL_RenderPresent(renderer);if(frames&&++frame>=frames)running=false;
        if(software&&!frames)SDL_Delay(8);
    }
    result=0;
done:
    if(result)fprintf(stderr,"Volcano: %s\n",last_error[0]?last_error:"Operation failed");
    resources_destroy();if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();return result;
}
