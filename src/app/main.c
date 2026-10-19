/* Volcano: window, main loop and command line. */
#include "../game/game.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

extern SDL_Renderer *renderer;
bool renderer_screenshot(const char *path);

static bool integer(const char *text,int *value)
{
    char *end;errno=0;long number=strtol(text,&end,10);
    if (errno || *end || number<0 || number>10000000) return false;
    *value=(int)number;return true;
}

static bool folder(const char *path)
{
    struct stat info;return !stat(path,&info) && S_ISDIR(info.st_mode);
}

/* First existing folder among PATH relative to the working directory, then
 * relative to the executable (development tree and macOS bundle layouts). */
static bool locate(const char *const *candidates,int count,char *out,size_t size)
{
    for (int i=0;i<count;i++) if (folder(candidates[i]) && text_copy(out,size,candidates[i])) return true;
    char *base=SDL_GetBasePath();bool found=false;
    for (int i=0;base && i<count && !found;i++) {
        const char *prefixes[]={"../","../Resources/"};
        for (int p=0;p<2 && !found;p++) found=snprintf(out,size,"%s%s%s",base,prefixes[p],candidates[i])<(int)size && folder(out);
    }
    SDL_free(base);return found;
}

static void usage(void)
{
    puts("Volcano - Rescue Team style disaster rescue (C11 / SDL2)\n"
         "Usage: volcano [--mission FILE.xml | --campaign 1..50 | --level SOURCE_INDEX]\n"
         "               [--assets RETAIL_FOLDER] [--pack FOLDER]... [--content FOLDER] [--hd]\n"
         "               [--frames N] [--screenshot FILE.bmp] [--hidden] [--software]\n"
         "Assets default to $RESCUE_TEAM_ASSETS or reference/rescue-team/; each --pack\n"
         "mounts another folder above it (later packs win). Missions may add their own packs.\n"
         "Click obstacles, supplies or buildings; Space pause, R restart, F1 paths,\n"
         "Escape pause/resume. Close the window to quit.");
}

int main(int argc,char **argv)
{
    const char *shot=NULL,*mission=NULL,*assets=getenv("RESCUE_TEAM_ASSETS"),*content=NULL;
    const char *packs[VFS_MAX_MOUNTS];int pack_count=0;
    int frames=0,level=-1,campaign=1,result=1;bool hidden=false,software=false,hd=false;
    SDL_Window *window=NULL;
    for (int i=1;i<argc;i++) {
        const char *arg=argv[i];
        static const char *valued[]={"--assets","--pack","--content","--screenshot","--frames","--level","--campaign","--mission"};
        bool takes=false;
        for (size_t k=0;k<sizeof valued/sizeof *valued;k++) takes|=!strcmp(arg,valued[k]);
        if (takes) {
            if (i+1>=argc) { fprintf(stderr,"Missing value for %s\n",arg);return 1; }
            const char *value=argv[++i];
            if (!strcmp(arg,"--assets")) assets=value;
            else if (!strcmp(arg,"--pack")) {
                if (pack_count==VFS_MAX_MOUNTS-2) { fprintf(stderr,"Too many packs\n");return 1; }
                packs[pack_count++]=value;
            }
            else if (!strcmp(arg,"--content")) content=value;
            else if (!strcmp(arg,"--screenshot")) shot=value;
            else if (!strcmp(arg,"--mission")) mission=value;
            else if (!integer(value,!strcmp(arg,"--frames")?&frames:!strcmp(arg,"--campaign")?&campaign:&level)) {
                fprintf(stderr,"Invalid number for %s\n",arg);return 1;
            }
        } else if (!strcmp(arg,"--hidden")) hidden=true;
        else if (!strcmp(arg,"--software")) software=true;
        else if (!strcmp(arg,"--hd")) hd=true;
        else if (!strcmp(arg,"--help")) { usage();return 0; }
        else { fprintf(stderr,"Unknown option: %s\n",arg);return 1; }
    }
    if (shot && !frames) frames=1;
    static char retail[PATH_CAPACITY],particles[PATH_CAPACITY],content_folder[PATH_CAPACITY];
    GameConfig config={.hd=hd};
    static const char *retail_candidates[]={"reference/rescue-team","resources"};
    static const char *particle_candidates[]={"assets/particles"},*content_candidates[]={"content"};
    if (assets) config.mounts[config.mount_count++]=assets;
    else if (locate(retail_candidates,2,retail,sizeof retail)) config.mounts[config.mount_count++]=retail;
    if (locate(particle_candidates,1,particles,sizeof particles)) config.mounts[config.mount_count++]=particles;
    for (int i=0;i<pack_count;i++) config.mounts[config.mount_count++]=packs[i];
    config.content=content?content:locate(content_candidates,1,content_folder,sizeof content_folder)?content_folder:"content";

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)) { error_set("%s",SDL_GetError());goto done; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
    window=SDL_CreateWindow("Volcano",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,GAME_WIDTH,GAME_HEIGHT,
        SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|(hidden?SDL_WINDOW_HIDDEN:0));
    if (!window) { error_set("%s",SDL_GetError());goto done; }
    renderer=SDL_CreateRenderer(window,-1,software?SDL_RENDERER_SOFTWARE:SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if (!renderer) { error_set("%s",SDL_GetError());goto done; }
    SDL_RenderSetLogicalSize(renderer,GAME_WIDTH,GAME_HEIGHT);SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    if (!game_startup(&config)) goto done;
    if (!(mission?game_init_file(mission):level>=0?game_init(level):game_init_campaign(campaign))) goto done;

    Uint64 previous=SDL_GetPerformanceCounter();bool running=true;int frame=0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type==SDL_QUIT) running=false;
            if (event.type==SDL_MOUSEMOTION) game_pointer((float)event.motion.x,(float)event.motion.y,(event.motion.state&SDL_BUTTON_LMASK)!=0);
            if (event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_LEFT) game_pointer((float)event.button.x,(float)event.button.y,true);
            if (event.type==SDL_MOUSEBUTTONUP && event.button.button==SDL_BUTTON_LEFT) game_click((float)event.button.x,(float)event.button.y);
            if (event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_LEAVE) game_pointer(-1,-1,false);
            if (event.type==SDL_KEYDOWN && !event.key.repeat) game_key(event.key.keysym.sym);
        }
        Uint64 now=SDL_GetPerformanceCounter();double dt=(double)(now-previous)/SDL_GetPerformanceFrequency();previous=now;
        game_update(frames?1./60:dt);
        SDL_SetRenderDrawColor(renderer,14,30,43,255);SDL_RenderClear(renderer);game_draw();
        if (last_error[0]) goto done;
        if (frames && frame+1>=frames && shot && !renderer_screenshot(shot)) { error_set("Cannot save screenshot: %s",SDL_GetError());goto done; }
        SDL_RenderPresent(renderer);
        if (frames && ++frame>=frames) running=false;
        if (software && !frames) SDL_Delay(8);
    }
    result=0;
done:
    if (result) fprintf(stderr,"Volcano: %s\n",last_error[0]?last_error:"Operation failed");
    game_shutdown();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();return result;
}
