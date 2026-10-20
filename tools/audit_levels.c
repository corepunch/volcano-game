/* Read-only campaign audit linked against the production game modules. */
#include "game/game_internal.h"
#include "content/content_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define game (*game_current())


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
        int phases[256];bool active=false;
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
        char sprite[160];if (!game_entity_sprite(game_current(),e,sprite,sizeof sprite)) sprite[0]=0;
        fprintf(out,"{\"id\":%u,\"animation\":",e->id);json_string(out,sprite);
        fprintf(out,",\"x\":%.3f,\"y\":%.3f}",e->p.x,e->p.y);
    }
    fputs("]}",out);
}

int main(int argc,char **argv)
{
    if (argc!=3) { fprintf(stderr,"Usage: audit_levels ASSETS OUTPUT.json\n");return 2; }
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)) return 2;
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,GAME_WIDTH,GAME_HEIGHT,32,SDL_PIXELFORMAT_RGBA32);
    renderer=surface?SDL_CreateSoftwareRenderer(surface):NULL;
    GameConfig config={.content="content",.mounts={argv[1],"assets/particles"},.mount_count=2};
    if (!renderer || !game_startup(&config)) { fprintf(stderr,"Audit setup: %s\n",last_error);return 2; }
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    FILE *out=fopen(argv[2],"w");
    if (!out) return 2;
    fputs("{\"levels\":[",out);
    bool ok=true;
    for (int index=0;index<52;index++) {
        char name[32],relative[64],path[PATH_CAPACITY];RetailLevel raw;
        if (index<50) snprintf(name,sizeof name,"level%03d",index);
        else snprintf(name,sizeof name,"%s",index==50?"map":"menu");
        snprintf(relative,sizeof relative,"level/%s.lvl",name);
        if (index) fputc(',',out);
        fputs("{\"file\":",out);json_string(out,name);
        bool parsed=vfs_resolve(relative,path,sizeof path) && retail_level_read(path,&raw);
        fprintf(out,",\"parsed\":%s",parsed?"true":"false");
        if (!parsed) { fputs(",\"parse_error\":",out);json_string(out,last_error);fputc('}',out);ok=false;continue; }
        fputs(",\"background\":",out);json_string(out,raw.background);
        fprintf(out,",\"traffic_points\":%d,\"objects\":[",raw.traffic_count);
        for (int i=0;i<raw.object_count;i++) {
            const RetailObject *o=&raw.objects[i];if (i) fputc(',',out);
            fprintf(out,"{\"type\":%u,\"id\":%u,\"layer\":%u,\"link\":%u,\"name\":",o->type,o->id,o->layer,o->link_id);
            json_string(out,o->name);fputs(",\"params\":",out);json_string(out,o->params);
            fputs(",\"extra\":",out);json_string(out,o->extra);
            fprintf(out,",\"x\":%.4f,\"y\":%.4f}",o->p.x,o->p.y);
        }
        retail_level_free(&raw);
        fputc(']',out);
        if (index<50) {
            error_clear();bool loaded=game_init(index);
            fprintf(out,",\"initialized\":%s,\"init_error\":",loaded?"true":"false");json_string(out,last_error);
            if (loaded) {
                fprintf(out,",\"restored_mission\":%s,\"ruleset\":",game.rules->goals?"true":"false");json_string(out,game.rules->id);
                fprintf(out,",\"campaign\":%d,\"rescue_goal\":%d,\"animal_goal\":%d,\"basin_goal\":%d",
                    game.spec.campaign,game.goal,game.animals,game.basin_goal);
                game_update(1./60);game_draw();
                fprintf(out,",\"rendered\":%s,\"render_error\":",last_error[0]?"false":"true");json_string(out,last_error);
                /* Full restored-mission runs are covered by the retail suite.
                 * Keep this probe on the reference prototype map. */
                if (index==0) simulate(out);
            }
            fprintf(stderr,"Audited %s: %s\n",name,loaded?"initialized":"cannot initialize");
        }
        fputc('}',out);fflush(out);
    }
    fputs("]}\n",out);
    if (fclose(out)) ok=false;
    game_shutdown();SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);SDL_Quit();
    return ok?0:1;
}
