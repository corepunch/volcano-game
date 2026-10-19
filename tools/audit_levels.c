/* Read-only campaign audit linked against the production game modules. */
#include "game_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define game (*game_current())

char asset_root[PATH_CAPACITY];
char last_error[1024];
SDL_Renderer *renderer;

static void json_string(FILE *out,const char *s)
{
    fputc('"',out);
    for (const unsigned char *p=(const unsigned char *)s;*p;p++) {
        if (*p=='"' || *p=='\\') fprintf(out,"\\%c",*p);
        else if (*p<32) fprintf(out,"\\u%04x",*p);
        else fputc(*p,out);
    }
    fputc('"',out);
}

/* This probes only the prototype's jobs. Finishing these is deliberately not
 * reported as completing the original mission or validating retail rules. */
static void simulate(FILE *out)
{
    int ticks=0;bool sweep=true;
    for (;ticks<30000 && game.completed<game.target_count;ticks++) {
        if (sweep) for (int i=0;i<game.object_count;i++) {
            Entity *e=&game.objects[i];
            if (e->kind==BARRIER || e->kind==RESOURCE || e->kind==SURVIVOR) game_dispatch(game_current(),i,true);
        }
        int phases[MAX_WORKERS];bool active=false;
        for (int i=0;i<game.worker_count;i++) {
            phases[i]=game.workers[i].phase;
            if (phases[i]!=IDLE) active=true;
        }
        if (!active) break;
        game_update(.1);sweep=false;
        for (int i=0;i<game.worker_count;i++)
            if (phases[i]!=game.workers[i].phase) sweep=true;
    }
    fprintf(out,",\"probe\":{\"completed\":%d,\"targets\":%d,\"ticks\":%d,\"rescued\":%d,\"survivors\":%d,\"pending\":[",
            game.completed,game.target_count,ticks,game.rescued,game.survivors);
    bool comma=false;
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if ((e->kind!=BARRIER && e->kind!=RESOURCE && e->kind!=SURVIVOR) || (e->done && e->follower<0)) continue;
        if (comma) fputc(',',out);
        comma=true;
        fprintf(out,"{\"id\":%u,\"animation\":",e->seed);json_string(out,e->anim);
        fprintf(out,",\"x\":%.3f,\"y\":%.3f}",e->p.x,e->p.y);
    }
    fputs("]}",out);
}

int main(int argc,char **argv)
{
    if (argc!=3 || strlen(argv[1])>=sizeof asset_root) {
        fprintf(stderr,"Usage: audit_levels ASSETS OUTPUT.json\n");return 2;
    }
    strcpy(asset_root,argv[1]);
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)) return 2;
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,GAME_WIDTH,GAME_HEIGHT,32,SDL_PIXELFORMAT_RGBA32);
    renderer=surface?SDL_CreateSoftwareRenderer(surface):NULL;
    if (!renderer || !resources_init()) { fprintf(stderr,"Audit setup: %s\n",last_error);return 2; }
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    FILE *out=fopen(argv[2],"w");
    Level *raw=malloc(sizeof *raw);
    if (!out || !raw) return 2;
    fputs("{\"levels\":[",out);
    bool ok=true;
    for (int index=0;index<52;index++) {
        char name[32],path[PATH_CAPACITY],error[1024];
        if (index<50) snprintf(name,sizeof name,"level%03d",index);
        else snprintf(name,sizeof name,"%s",index==50?"map":"menu");
        if (snprintf(path,sizeof path,"%s/level/%s.lvl",asset_root,name)>=(int)sizeof path) {ok=false;break;}
        if (index) fputc(',',out);
        fputs("{\"file\":",out);json_string(out,name);
        bool parsed=level_load(path,raw,error,sizeof error);
        fprintf(out,",\"parsed\":%s",parsed?"true":"false");
        if (!parsed) {fputs(",\"parse_error\":",out);json_string(out,error);fputc('}',out);ok=false;continue;}
        fputs(",\"background\":",out);json_string(out,raw->background);
        fprintf(out,",\"traffic_points\":%d,\"objects\":[",raw->traffic_count);
        for (int i=0;i<raw->object_count;i++) {
            LevelObject *o=&raw->objects[i];if (i) fputc(',',out);
            fprintf(out,"{\"type\":%u,\"id\":%u,\"layer\":%u,\"link\":%u,\"name\":",o->type,o->id,o->layer,o->link_id);
            json_string(out,o->name);fputs(",\"params\":",out);json_string(out,o->params);
            fputs(",\"extra\":",out);json_string(out,o->extra);
            fprintf(out,",\"x\":%.4f,\"y\":%.4f}",o->x,o->y);
        }
        fputc(']',out);
        if (index<50) {
            last_error[0]=0;bool loaded=game_init(index);
            fprintf(out,",\"initialized\":%s,\"init_error\":",loaded?"true":"false");json_string(out,last_error);
            if (loaded) {
                fprintf(out,",\"restored_mission\":%s,\"campaign\":%d,\"rescue_goal\":%d,\"animal_goal\":%d,\"basin_goal\":%d",
                    game.early?"true":"false",game.campaign,game.goal,game.animals,game.basin_goal);
                game_update(1./60);game_draw();
                fprintf(out,",\"rendered\":%s,\"render_error\":",last_error[0]?"false":"true");json_string(out,last_error);
                /* Full early-mission runs are covered by make test, including
                 * building choices. Keep the legacy probe on its reference map. */
                if (index==0) simulate(out);
            }
            fprintf(stderr,"Audited %s: %s\n",name,loaded?"initialized":"cannot initialize");
        }
        fputc('}',out);fflush(out);
    }
    fputs("]}\n",out);if (fclose(out)) ok=false;
    free(raw);game_destroy();
    resources_destroy();SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);SDL_Quit();
    return ok?0:1;
}
