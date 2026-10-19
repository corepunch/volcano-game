#include "game_test.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

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
static bool particle_test_pixels(const Entity *e,uint32_t *hash,bool *visible)
{
    SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);game_paint_log_particles(game_current(), e);
    int width,height;SDL_GetRendererOutputSize(renderer,&width,&height);
    SDL_Surface *pixels=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);
    if (!pixels) return game_fail("Cannot allocate particle test frame");
    bool ok=SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_RGBA32,pixels->pixels,pixels->pitch)==0;
    *hash=2166136261u;*visible=false;
    if (ok) for (int y=0;y<height;y++) for (int x=0;x<width;x++) {
        const Uint8 *p=(const Uint8 *)pixels->pixels+y*pixels->pitch+x*4;
        for (int c=0;c<3;c++) { *hash=(*hash^p[c])*16777619u;if (p[c]) *visible=true; }
    }
    SDL_FreeSurface(pixels);
    return ok && !last_error[0];
}
static bool particles_selftest(void)
{
    if (!game_init(0)) return false;
    int log=-1,burning=0,unlit=0;
    uint32_t initial,paused,animated,restarted;bool visible;
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if (game_burning_log(e)) {
            burning++;if (log<0) log=i;
            if (!particle_test_pixels(e,&initial,&visible) || !visible)
                return game_fail("Fire-marked tree did not emit particles");
        } else if (e->kind==BARRIER) {
            if (!strcmp(e->anim,"tree00")) unlit++;
            if (!particle_test_pixels(e,&initial,&visible) || visible)
                return game_fail("Unmarked barrier emitted fire");
        }
    }
    if (log<0 || burning!=2 || unlit!=2) return game_fail("Expected two burning and two unlit trees in level000");
    Entity *e=&game.objects[log];
    if (!particle_test_pixels(e,&initial,&visible) || !visible || !test_frame("burning-logs"))
        return game_fail("Logs were not burning on level entry");
    game_key(SDLK_SPACE);game_update(.1);
    if (!particle_test_pixels(e,&paused,&visible) || paused!=initial) return game_fail("Paused fire changed");
    game_key(SDLK_SPACE);game_update(.1);
    if (!particle_test_pixels(e,&animated,&visible) || animated==initial) return game_fail("Fire did not animate");
    /* Open the route to the marked tree for the clearing lifecycle check. */
    for (int i=0;i<game.object_count;i++) if (i!=log && game.objects[i].kind==BARRIER) {
        Entity *other=&game.objects[i];other->done=other->hidden=true;
        if (game_burning_log(other)) other->fire_stopped=game.time;
    }
    if (!game_dispatch(game_current(), log,true)) return game_fail("Cannot dispatch to the burning tree");
    for (int tick=0;tick<1000 && !e->done;tick++) game_update(.1);
    if (!e->done || !particle_test_pixels(e,&animated,&visible) || !visible || !test_frame("fire-cleared"))
        return game_fail("Clearing logs did not retain fading particles");
    for (int tick=0;tick<71;tick++) game_update(.1);
    if (!particle_test_pixels(e,&animated,&visible) || visible || !test_frame("smoke-cleared"))
        return game_fail("Cleared logs continued emitting fire/smoke");
    game_key(SDLK_r);
    if (!particle_test_pixels(&game.objects[log],&restarted,&visible) || !visible || restarted!=initial)
        return game_fail("Restart did not restore the initial fire");
    puts("PASS: only the two fire-marked trees burn; unmarked trees/other barriers stay unlit; fire animates, pauses, fades after clearing and resets on restart.");
    return true;
}
static bool scenery_selftest(void)
{
    if (!game_init(0)) return false;
    int bird=-1,cloud=-1,wood=-1,house=-1,land=-1,birds=0,clouds=0,roofs=0,floating=0;
    for (int i=0;i<game.object_count;i++) {
        Entity *e=&game.objects[i];
        if (e->ambient==BIRD) { bird=i;birds++; }
        if (e->ambient==CLOUD) { cloud=i;clouds++; }
        if (!strcmp(e->anim,"boat_wood")) wood=i;
        if (!strcmp(e->anim,"sawmill_water")) house=i;
        if (e->kind==BASE) land=i;
        if (e->floating) floating++;
        if (e->roof) {
            roofs++;
            if (!game_on_roof(e)) return game_fail("Roof survivor has no flooded house support");
            Entity *support=&game.objects[e->support];
            const Entity *pair[]={support,e};
            if (game_depth_compare(game_current(), &pair[0],&pair[1])>=0) return game_fail("Roof occupant sorts behind the house");
        }
    }
    if (birds!=3 || clouds!=2 || roofs!=4 || floating!=17 || wood<0 || house<0 || land<0)
        return game_fail("Unexpected scenery fixtures: birds=%d clouds=%d roofs=%d floating=%d",birds,clouds,roofs,floating);
    VisualPose b0=game_visual_pose(game_current(), &game.objects[bird]),c0=game_visual_pose(game_current(), &game.objects[cloud]);
    VisualPose h0=game_visual_pose(game_current(), &game.objects[house]),w0=game_visual_pose(game_current(), &game.objects[wood]);
    if (!test_frame("scenery-start")) return false;
    for (int tick=0;tick<10;tick++) game_update(.1);
    VisualPose b1=game_visual_pose(game_current(), &game.objects[bird]),c1=game_visual_pose(game_current(), &game.objects[cloud]);
    VisualPose h1=game_visual_pose(game_current(), &game.objects[house]),w1=game_visual_pose(game_current(), &game.objects[wood]);
    float bird_span=GAME_WIDTH+192,cloud_span=GAME_WIDTH+360;
    float bx=fmodf(b1.p.x-b0.p.x+bird_span,bird_span),cx=fmodf(c1.p.x-c0.p.x+cloud_span,cloud_span);
    if (fabsf(bx-game.objects[bird].travel_speed)>.01f || fabsf(cx-game.objects[cloud].travel_speed)>.01f)
        return game_fail("Birds/clouds failed to cross left to right");
    if (fabsf(h1.p.y-h0.p.y)<.01f || fabsf(h1.angle-h0.angle)<.01f ||
        fabsf(w1.p.y-w0.p.y)<.01f || fabsf(w1.angle-w0.angle)<.01f)
        return game_fail("Floating houses/wood did not bob and rock");
    /* A roof occupant must retain its original house-local coordinates under
     * translation and rotation, including at opposite wave extremes. */
    for (int sample=0;sample<16;sample++) {
        game.time=sample*.5;
        for (int i=0;i<game.object_count;i++) {
            Entity *e=&game.objects[i];if (!game_on_roof(e)) continue;
            Entity *support=&game.objects[e->support];
            VisualPose p=game_visual_pose(game_current(), e),parent=game_visual_pose(game_current(), support);
            float a=parent.angle*(float)(PI/180),dx=p.p.x-parent.p.x,dy=p.p.y-parent.p.y;
            if (fabsf(cosf(a)*dx-sinf(a)*dy-(e->p.x-support->p.x))>.001f ||
                fabsf(sinf(a)*dx+cosf(a)*dy-(e->p.y-support->p.y))>.001f || p.angle!=parent.angle)
                return game_fail("Roof survivor slid relative to the house");
            game_pointer(p.p.x,GAME_HEIGHT-p.p.y,false);
            if (game_hovered_object(game_current())!=i) return game_fail("Moving roof survivor lost its click target");
        }
        VisualPose p=game_visual_pose(game_current(), &game.objects[land]);
        if (game_distance(p.p,game.objects[land].p)>.001f || p.angle) return game_fail("Land object bobbed");
    }
    game_pointer(-1,-1,false);
    if (!test_frame("scenery-moving")) return false;
    game_key(SDLK_SPACE);double before=game.time;game_update(.1);
    VisualPose frozen=game_visual_pose(game_current(), &game.objects[house]);
    if (before!=game.time) return game_fail("Paused scenery clock advanced");
    game_key(SDLK_SPACE);VisualPose resumed=game_visual_pose(game_current(), &game.objects[house]);
    if (game_distance(frozen.p,resumed.p)>.001f || frozen.angle!=resumed.angle) return game_fail("Paused float changed");
    /* Check both sides of the off-screen wrap and a new height each pass. */
    Entity *e=&game.objects[bird];
    double crossing=(bird_span-game_particle_random(e->seed+3)*bird_span)/e->travel_speed;
    game.time=crossing-.01;VisualPose exit=game_visual_pose(game_current(), e);
    game.time=crossing+.01;VisualPose entry=game_visual_pose(game_current(), e);
    if (exit.p.x<GAME_WIDTH+50 || entry.p.x>-50 || fabsf(entry.p.y-exit.p.y)<1)
        return game_fail("Bird wrap was visible or failed to choose a new height");
    e=&game.objects[cloud];
    crossing=(cloud_span-game_particle_random(e->seed+3)*cloud_span)/e->travel_speed;
    game.time=crossing-.01;exit=game_visual_pose(game_current(), e);game.time=crossing+.01;entry=game_visual_pose(game_current(), e);
    if (exit.p.x<GAME_WIDTH+128 || entry.p.x>-128) return game_fail("Cloud wrapped while visible");
    /* Rescue releases the roof attachment before following a worker. */
    for (int i=0;i<game.object_count;i++) if (game.objects[i].roof) {
        e=&game.objects[i];e->done=true;e->follower=0;
        VisualPose p=game_visual_pose(game_current(), e);
        if (game_on_roof(e) || game_distance(p.p,e->p)>.001f || p.angle) return game_fail("Rescued survivor retained house transform");
        break;
    }
    game_key(SDLK_r);
    VisualPose restart=game_visual_pose(game_current(), &game.objects[bird]);
    if (game_distance(restart.p,b0.p)>.001f) return game_fail("Restart did not reset ambient scenery");
    puts("PASS: left-to-right birds at changing heights, drifting clouds, off-screen wraps, water bob/rotation, roof support/draw order/hit targets, rescue detach, pause and restart.");
    return true;
}
static bool interaction_selftest(void)
{
    if (!game_init(0)) return false;
    int helmets=0,outside=0,lot=-1;
    for (int i=0;i<game.worker_count;i++) {
        Worker *w=&game.workers[i];
        if (w->home_base>=0) { if (!game.objects[w->entity].hidden) return game_fail("In-house worker visible as a character");helmets++; }
        else if (!w->boat && !game.objects[w->entity].hidden) outside++;
    }
    if (helmets!=2 || outside!=1) return game_fail("Incorrect in-house or outside worker representation");
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==BUILD_LOT) { lot=i;break; }
    if (lot<0) return game_fail("No build lot restored");
    Entity *t=&game.objects[lot];Vec2 p=t->p;
    game_pointer(p.x,GAME_HEIGHT-p.y,false);
    if (game_hovered_object(game_current())!=lot || !test_frame("hover-lot")) return game_fail("Build lot hover failed");
    game_click(p.x,GAME_HEIGHT-p.y);
    if (game.build_menu!=lot) return game_fail("Build lot click did not open menu");
    Vec2 button=game_build_button_position(game_current(), 0);game_click(button.x,GAME_HEIGHT-button.y);
    if (game.build_menu!=lot || t->assigned || game.wood) return game_fail("Unaffordable construction started");
    if (!test_frame("build-disabled")) return false;
    game_key(SDLK_ESCAPE);
    if (game.build_menu>=0 || game.paused) return game_fail("Escape failed to dismiss construction menu");
    game_click(p.x,GAME_HEIGHT-p.y);game_click(1000,600);
    if (game.build_menu>=0) return game_fail("Outside click failed to dismiss construction menu");
    /* Open the road to construction independently of the full-level test. */
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==BARRIER) game.objects[i].done=true;
    game.wood=100;game_click(p.x,GAME_HEIGHT-p.y);
    button=game_build_button_position(game_current(), 2);game_pointer(button.x,GAME_HEIGHT-button.y,true);
    if (game_build_button_at(game_current())!=2 || !test_frame("build-menu")) return game_fail("Building button hover failed");
    game_click(button.x,GAME_HEIGHT-button.y);
    if (!t->assigned || game.wood!=40 || game.build_menu>=0) return game_fail("Construction dispatch or wood cost failed");
    int builder=-1;
    for (int i=0;i<game.worker_count;i++) if (game.workers[i].target==lot) builder=i;
    if (builder<0 || game.objects[game.workers[builder].entity].hidden) return game_fail("Builder failed to leave HQ");
    if (!test_frame("job-checkmark")) return false;
    for (int tick=0;tick<500 && game.workers[builder].phase==OUTBOUND;tick++) game_update(.1);
    Worker *w=&game.workers[builder];
    if (w->phase!=WORKING || t->kind!=BUILD_LOT) return game_fail("Construction did not require work");
    for (int tick=0;tick<20;tick++) game_update(.1);
    if (!test_frame("construction-progress")) return false;
    double before=w->work;game_key(SDLK_SPACE);game_update(.1);game_key(SDLK_SPACE);
    if (w->work!=before) return game_fail("Paused construction advanced");
    for (int tick=0;tick<100 && w->phase==WORKING;tick++) game_update(.1);
    if (t->kind!=BUILDING || strcmp(t->anim,"house0") || w->phase!=RETURNING) return game_fail("Construction did not return worker immediately");
    Vec2 position=game.objects[w->entity].p;
    game_update(.1);
    if (game_distance(position,game.objects[w->entity].p)<.001) return game_fail("Builder paused before returning to HQ");
    for (int tick=0;tick<500 && w->phase!=IDLE;tick++) game_update(.1);
    if (w->phase!=IDLE || game_distance(w->home,game.objects[w->entity].p)>.001) return game_fail("Builder failed to return to its real home");
    /* Every menu option must spend its own cost and finish with its own art. */
    for (int option=0;option<4;option++) {
        if (!game_init(0)) return false;
        lot=-1;
        for (int i=0;i<game.object_count;i++) {
            if (game.objects[i].kind==BARRIER) game.objects[i].done=true;
            if (game.objects[i].kind==BUILD_LOT) lot=i;
        }
        if (lot<0) return game_fail("Missing lot in construction variant test");
        t=&game.objects[lot];game.wood=100;p=t->p;
        game_click(p.x,GAME_HEIGHT-p.y);button=game_build_button_position(game_current(), option);
        game_click(button.x,GAME_HEIGHT-button.y);
        if (!t->assigned || game.wood!=100-game_buildings[option].cost) return game_fail("Building variant cost failed: %d",option);
        for (int tick=0;tick<600 && t->kind==BUILD_LOT;tick++) game_update(.1);
        if (t->kind!=BUILDING || strcmp(t->anim,game_buildings[option].animation)) return game_fail("Building variant completion failed: %d",option);
    }
    /* Isolate one actual land worker to exercise immediate returns and queue ownership. */
    if (!game_init(0)) return false;
    game.worker_count=1;w=&game.workers[0];int barrier=-1;
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==BARRIER && game_dispatch(game_current(), i,true)) { barrier=i;break; }
    if (barrier<0) return game_fail("No barrier for return regression");
    for (int tick=0;tick<500 && w->phase!=RETURNING;tick++) game_update(.1);
    if (w->phase!=RETURNING || w->target!=-1 || game.completed!=1) return game_fail("Completed clear did not return worker immediately");
    position=game.objects[w->entity].p;game_update(.1);
    if (game_distance(position,game.objects[w->entity].p)<.001) return game_fail("Worker paused after clearing");
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==RESOURCE && game_dispatch(game_current(), i,true))
        return game_fail("Returning worker accepted a job before reaching HQ");
    for (int tick=0;tick<500 && w->phase!=IDLE;tick++) game_update(.1);
    if (w->phase!=IDLE || game.completed!=1) return game_fail("Clear return stalled or counted completion twice");
    int supply=-1;
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==RESOURCE && game_dispatch(game_current(), i,true)) { supply=i;break; }
    if (supply<0 || w->phase!=OUTBOUND || game_distance(w->home,w->path[0])>.001) return game_fail("Worker could not take an order from HQ");
    for (int tick=0;tick<500 && w->phase==OUTBOUND;tick++) game_update(.1);
    if (w->phase!=RETURNING || !game.objects[supply].hidden || !w->cargo || game.food || game.wood || game.gas)
        return game_fail("Pickup was not immediate or inventory credited before delivery");
    int queue=-1;
    for (int i=0;i<game.object_count;i++) if (game_job(game_current(), &game.objects[i]) && !game.objects[i].done && game_reachable(game_current(), i)) { queue=i;break; }
    if (queue<0) return game_fail("No queue regression target");
    p=game.objects[queue].p;game_click(p.x,GAME_HEIGHT-p.y);
    if (!game.objects[queue].queued) return game_fail("Busy-worker click did not queue job");
    game_click(p.x,GAME_HEIGHT-p.y);
    if (game.objects[queue].queued) return game_fail("Queued job could not be cancelled");
    game_click(p.x,GAME_HEIGHT-p.y);
    for (int tick=0;tick<500 && !game.objects[queue].assigned;tick++) game_update(.1);
    if (!game.objects[queue].assigned || game.objects[queue].queued || game.completed!=2 || !(game.food || game.wood || game.gas))
        return game_fail("Queued job did not dispatch after cargo delivery");
    if (game_distance(w->home,w->path[0])>.001) return game_fail("Chained worker returned to previous worksite instead of HQ");
    if (!game_init(0)) return false;
    puts("PASS: construction menu/costs/work, authored feedback, HQ helmets, instant pickups, cargo delivery, immediate HQ returns and cancellable queued jobs.");
    return true;
}
/* Campaign regressions use the same scheduler and UI as the application. */
int test_entity(uint32_t seed)
{
    for (int i=0;i<game.object_count;i++) if (game.objects[i].seed==seed) return i;
    return -1;
}
void test_click_entity(int index)
{
    Vec2 p=game.objects[index].p;game_click(p.x,GAME_HEIGHT-p.y);
}
bool test_choose_building(int lot,int type)
{
    test_click_entity(lot);
    if (game.build_menu!=lot) return false;
    Vec2 p=game_build_button_position(game_current(), type);game_click(p.x,GAME_HEIGHT-p.y);
    return game.objects[lot].assigned;
}
static bool campaign_selftest(void)
{
    if (campaign_index(1)!=15 || campaign_index(2)!=18 || campaign_index(3)!=19 || campaign_number(0)!=38)
        return game_fail("Original campaign order regression");
    for (int i=0;i<50;i++) {
        if (!game_init(i)) return false;
        game_update(1./60);game_draw();
        if (last_error[0]) return false;
    }
    puts("PASS: all 50 original missions initialize and render; campaign order comes from map prerequisites.");
    if (!game_init(campaign_index(2))) return false;
    int cafe=test_entity(646),output=test_entity(657);Entity *building=&game.objects[cafe];
    if (building->operational || !game.objects[output].hidden || game_dispatch(game_current(), output,true)) return game_fail("Ruined cafe produced a bonus");
    int starting_wood=game.wood;
    if (!game_dispatch(game_current(), cafe,true) || game.wood!=starting_wood-20 || game_dispatch(game_current(), cafe,true) || game.wood!=starting_wood-20)
        return game_fail("Repair did not reserve its cost exactly once");
    for (int i=0;i<500 && !building->operational;i++) game_update(.1);
    if (!building->operational) return game_fail("Cafe repair never completed");
    int start_food=game.food;
    for (int i=0;i<50;i++) game_update(.1);
    if (game.food!=start_food+5) return game_fail("Cafe failed to pay recurring automatic income");
    for (int i=0;i<51;i++) game_update(.1);
    if (game.objects[output].hidden || game.objects[output].done) return game_fail("Cafe failed to expose its authored bonus pickup");
    if (!game_dispatch(game_current(), output,true)) return game_fail("Cafe bonus cannot be collected");
    for (int i=0;i<1000 && game_output_busy(game_current(), building);i++) game_update(.1);
    if (game_output_busy(game_current(), building)) return game_fail("Cafe bonus delivery stalled");
    /* Income continues while the finite bonus is carried. A new bonus must not
     * reset the returning worker's target or create a second copy of its cargo. */
    for (int i=0;i<110;i++) game_update(.1);
    if (game.objects[output].hidden || game.objects[output].done) return game_fail("Delivered cafe bonus did not respawn");
    building->action=DEMOLISH;
    if (!game_dispatch(game_current(), cafe,true) || game_dispatch(game_current(), output,true)) return game_fail("Demolition allowed a conflicting bonus reservation");
    for (int i=0;i<500 && building->kind==BUILDING;i++) game_update(.1);
    if (building->kind!=BUILD_LOT || !game.objects[output].hidden) return game_fail("Demolition did not release its lot and output");
    start_food=game.food;
    for (int i=0;i<100;i++) game_update(.1);
    if (game.food!=start_food) return game_fail("Demolished cafe kept producing");
    puts("PASS: damaged buildings, atomic repair costs, automatic income, recurring bonus delivery and demolition ownership.");
    for (int mission=1;mission<=3;mission++) {
        if (!game_init(campaign_index(mission))) return false;
        const int goals[]={3,3,5},food[]={20,50,10},wood[]={0,30,100},money[]={0,20,30};
        if (!game.early || game.goal!=goals[mission-1] || game.food!=food[mission-1] ||
            game.wood!=wood[mission-1] || game.money!=money[mission-1]) return game_fail("Mission %d initial rules",mission);
        int active=0;
        for (int i=0;i<game.worker_count;i++) if (game_available(&game.workers[i])) active++;
        if (active!=2) return game_fail("Mission %d starts with %d active workers rather than two",mission,active);
        if (mission>1 && game.worker_count-active!=mission-1) return game_fail("Stranded workers are already hired");
        game_key(SDLK_p);game_update(.1);game_key(SDLK_p);
        if (game.time || game.rescued || game.food!=food[mission-1] || game.money!=money[mission-1]) return game_fail("Pause changed mission economy");
        int conversion=0;bool repaired=false,produced=false,spent_money=false;
        for (int tick=0;tick<18000 && !game.won;tick++) {
            if (tick%10==0) {
                /* Follow level 3's house / money / boulder / replacement cafe sequence. */
                if (mission==3) {
                    int lot=test_entity(607),rock=test_entity(615);
                    Entity *site=&game.objects[lot];
                    if (site->kind==BUILD_LOT && !site->assigned)
                        test_choose_building(lot,conversion?1:2);
                    if (!conversion && site->kind==BUILDING && site->operational && !site->assigned &&
                        game.objects[rock].done && !game_output_busy(game_current(), site)) {
                        test_click_entity(lot);
                        if (game.build_menu==lot) {
                            Vec2 p=game_build_button_position(game_current(), 2);game_click(p.x,GAME_HEIGHT-p.y);
                            if (site->assigned && site->action==DEMOLISH) conversion=1;
                        }
                    }
                    if (conversion && site->kind==BUILDING && site->building==1 && site->operational) conversion=2;
                    if (game.build_menu>=0) game_key(SDLK_ESCAPE);
                }
                for (int pass=0;pass<3;pass++) for (int i=0;i<game.object_count;i++) {
                    Entity *e=&game.objects[i];
                    bool candidate=pass==0?e->kind==RESOURCE:pass==1?e->kind==BUILDING && !e->operational:e->kind==BARRIER;
                    if (!candidate || e->hidden || e->assigned || (e->done && e->kind!=BUILDING)) continue;
                    if (mission==3 && pass==2 && game.objects[test_entity(615)].done && conversion!=2) continue;
                    int before_money=game.money;
                    test_click_entity(i);
                    if (e->assigned && e->kind==BUILDING) repaired=true;
                    if (game.money<before_money) spent_money=true;
                }
            }
            int before_food=game.food,before_money=game.money;
            game_update(.1);
            if (game.food>before_food || game.money>before_money) produced=true;
            if (game.money<before_money) spent_money=true;
            if (mission==2) for (int i=0;i<game.object_count;i++)
                if (game.objects[i].kind==BUILDING && game.objects[i].operational) repaired=true;
            if (game.food<0 || game.wood<0 || game.money<0) return game_fail("Mission economy became negative");
        }
        if (!game.won) {
            fprintf(stderr,"Mission %d stalled: rescued=%d/%d food=%d wood=%d money=%d conversion=%d\n",
                    mission,game.rescued,game.goal,game.food,game.wood,game.money,conversion);
            for (int i=0;i<game.object_count;i++) {
                Entity *e=&game.objects[i];
                if ((e->kind==BARRIER || e->kind==SURVIVOR || e->kind==RECRUIT) && !e->done)
                    fprintf(stderr," pending %u %s (%d) at %.0f,%.0f\n",e->seed,e->anim,e->kind,e->p.x,e->p.y);
            }
            return game_fail("Campaign mission %d did not complete under its restored rules",mission);
        }
        if (game.rescued!=game.goal) return game_fail("Wrong mission rescue total");
        if (mission==2 && (!repaired || !produced || !spent_money)) return game_fail("Level 2 skipped its economy: repair=%d income=%d paid=%d",repaired,produced,spent_money);
        if (mission==3 && (!spent_money || conversion!=2)) return game_fail("Level 3 skipped house-to-cafe conversion");
        active=0;
        for (int i=0;i<game.worker_count;i++) if (!game.workers[i].recruiting) active++;
        if (active!=mission+1) return game_fail("Rescued worker did not join crew");
        double finished=game.time;game_update(.1);
        if (game.time!=finished) return game_fail("Completed mission kept running");
        char shot[40];snprintf(shot,sizeof shot,"campaign-%d-complete",mission);
        if (!test_frame(shot)) return false;
        if (mission==1 && game.objects[test_entity(991)].done) return game_fail("Tutorial ignored its dynamite gate");
        game_click(683,GAME_HEIGHT-470);
        if (game.campaign!=mission+1 || game.won || game.time) return game_fail("Next level does not follow campaign order");
        if (!game_init(campaign_index(mission))) return false;
        game.food=0;game_key(SDLK_r);
        if (game.rescued || game.won || game.time || game.food!=food[mission-1] || game.wood!=wood[mission-1]) return game_fail("Mission restart leaked progress");
        printf("PASS: campaign level %d completes with %d rescues; costs, production, recruits, victory, next level and restart.\n",mission,goals[mission-1]);
    }
    return true;
}

/* Real click/queue playthroughs. No inventory grants or direct completion flags. */
static bool missions_4_6_selftest(void)
{
    if (!game_init(campaign_index(4))) return false;
    int recruit=test_entity(891);
    for (int i=0;i<20;i++) game_update(.1);
    if (game.objects[recruit].transport!=BY_ATV || game.workers[1].phase!=STRANDED) return game_fail("Offroad worker escaped without ATV");
    test_click_entity(recruit);
    if (game.transport_trips!=1 || game.gas!=60 || !game.objects[recruit].assigned) return game_fail("ATV did not reserve one fuel payment");
    test_click_entity(recruit);
    if (game.transport_trips!=1 || game.gas!=60) return game_fail("ATV rescue reserved twice");
    Vec2 vehicle=game.objects[game.vehicles[0].entity].p;double before=game.time;
    game_key(SDLK_p);for (int i=0;i<20;i++) game_update(.1);game_key(SDLK_p);
    if (game.time!=before || game_distance(vehicle,game.objects[game.vehicles[0].entity].p)>.001 || game.gas!=60) return game_fail("Paused ATV kept moving or spending fuel");
    if (!test_frame("campaign-4-atv-outbound")) return false;
    game_key(SDLK_r);
    if (game.transport_trips || game.gas!=70 || game.vehicles[0].target!=-1 || game.objects[test_entity(891)].assigned) return game_fail("Restart retained an active ATV rescue");
    if (!game_init(campaign_index(6))) return false;
    int fish=test_entity(462),pier=test_entity(858),plot=test_entity(1330);
    if (game_gate_open(game_current(), &game.objects[fish]) || game_gate_open(game_current(), &game.objects[pier])) return game_fail("Polluted basin is open on load");
    test_click_entity(fish);test_click_entity(pier);
    if (game.transport_trips || game.gas!=220 || game.wood!=90 || game.objects[pier].assigned) return game_fail("Pollution gate spent resources or allowed fishing");
    if (test_choose_building(plot,1) || game.wood!=90 || game.objects[plot].assigned) return game_fail("Level 6 allowed a forbidden cafe");
    game_key(SDLK_ESCAPE);
    if (!test_frame("campaign-6-polluted")) return false;
    if (!game_init(campaign_index(5))) return false;
    int fog=test_entity(847);
    test_click_entity(fog);
    if (game.objects[fog].assigned || game.objects[fog].queued || game.objects[fog].done || game.money!=40) return game_fail("Fog could be cleared without its valve");
    puts("PASS: ATV-only rescues, exact-once fuel, pause/restart, pollution-gated piers/fishing, linked fog valves and forbidden cafes.");
    const int goals[]={13,6,2},animal_goals[]={0,16,6},foods[]={60,70,60},woods[]={80,80,90},cash[]={70,40,40},fuel[]={70,0,220};
    for (int mission=4;mission<=6;mission++) {
        fprintf(stderr,"Testing campaign level %d...\n",mission);
        if (!game_init(campaign_index(mission))) return false;
        int n=mission-4;
        if (game.goal!=goals[n] || game.animals!=animal_goals[n] || game.food!=foods[n] || game.wood!=woods[n] ||
            game.money!=cash[n] || game.gas!=fuel[n]) return game_fail("Level %d initial mission data",mission);
        int starting_crew=0;
        for (int i=0;i<game.worker_count;i++) if (!game.workers[i].recruiting) starting_crew++;
        if (starting_crew!=(mission==5?2:1)) return game_fail("Wrong starting crew in level %d",mission);
        bool saw_roaming=false,saw_partial=false;
        for (int tick=0;tick<6000 && !game.won;tick++) {
            if (tick%10==0) {
                for (int pass=0;pass<5;pass++) for (int i=0;i<game.object_count;i++) {
                    Entity *e=&game.objects[i];
                    if (e->hidden || e->assigned || e->done || e->queued) continue;
                    if (pass==0 && ((e->kind==BUILDING || e->kind==SERVICE) && !e->operational)) test_click_entity(i);
                    else if (pass==1 && e->kind==BUILD_LOT) {
                        int houses=0,cafes=0;
                        for (int j=0;j<game.object_count;j++) if (game.objects[j].kind==BUILDING) {
                            if (game.objects[j].building==2) houses++;
                            if (game.objects[j].building==1 && game.objects[j].operational) cafes++;
                        }
                        int type=mission==4?1:mission==5?(!cafes?1:houses<1?2:1):(houses<1?2:3);
                        test_choose_building(i,type);
                        if (game.build_menu>=0) game_key(SDLK_ESCAPE);
                    } else if (pass==2 && (e->kind==RESOURCE || e->kind==VALVE || e->kind==RECRUIT)) test_click_entity(i);
                    else if (pass==3 && e->kind==BARRIER && !e->effect) {
                        /* The level-5 walkthrough needs one large rock, not all of them. */
                        if (mission==5 && strstr(e->anim,"stone_big") && e->seed!=1046) continue;
                        test_click_entity(i);
                    } else if (pass==4 && e->kind==ANIMAL) {
                        /* Allow the authored animals to wander before reserving capture. */
                        if (game_distance(e->p,e->origin)>1) saw_roaming=true;
                        if (tick>200) test_click_entity(i);
                    }
                }
            }
            game_update(.1);
            if ((game.rescued==game.goal && game.animals_rescued<game.animals) ||
                (game.animals_rescued==game.animals && game.rescued<game.goal)) saw_partial=true;
            if (game.food<0 || game.wood<0 || game.money<0 || game.gas<0) return game_fail("Level %d overspent inventory",mission);
        }
        if (!game.won) {
            fprintf(stderr,"Level %d stalled: people %d/%d animals %d/%d basins %d/%d food=%d wood=%d money=%d fuel=%d trips=%d fish=%d\n",
                mission,game.rescued,game.goal,game.animals_rescued,game.animals,game.purified,game.basin_goal,game.food,game.wood,game.money,game.gas,game.transport_trips,game.fishing_trips);
            for (int i=0;i<game.object_count;i++) {
                Entity *e=&game.objects[i];
                if (!e->hidden && !e->done && (e->kind==RECRUIT || e->kind==ANIMAL || e->kind==VALVE || e->kind==SERVICE))
                    fprintf(stderr," pending %u %s kind=%d at %.0f,%.0f gate=%d transport=%d queued=%d assigned=%d\n",e->seed,e->anim,e->kind,e->p.x,e->p.y,e->gate,e->transport,e->queued,e->assigned);
            }
            return game_fail("Campaign level %d did not complete",mission);
        }
        if (game.rescued!=game.goal || game.animals_rescued!=game.animals || game.purified!=game.basin_goal)
            return game_fail("Mission finished before every objective");
        if (mission==4 && game.transport_trips!=5) return game_fail("Level 4 bypassed an ATV rescue");
        if (mission>=5 && !saw_roaming) return game_fail("Animal rescue bypassed wandering");
        if (mission>=5 && !saw_partial) return game_fail("Multiple mission objectives were not exercised independently");
        if (mission==6 && (!game.fishing_trips || game.purified!=2)) return game_fail("Level 6 bypassed fishing or purification");
        char shot[48];snprintf(shot,sizeof shot,"campaign-%d-complete",mission);
        if (!test_frame(shot)) return false;
        printf("PASS: campaign level %d completes: %d people, %d animals, %d basins; %d vehicle trips, %d fishing deliveries.\n",
            mission,game.rescued,game.animals_rescued,game.purified,game.transport_trips,game.fishing_trips);
        game_click(683,GAME_HEIGHT-470);
        if (game.campaign!=mission+1 || game.won) return game_fail("Level %d progression failed",mission);
        if (!game_init(campaign_index(mission))) return false;
        game_key(SDLK_r);
        if (game.rescued || game.animals_rescued || game.purified || game.transport_trips || game.food!=foods[n] || game.gas!=fuel[n])
            return game_fail("Level %d restart leaked mission state",mission);
    }
    return true;
}

bool game_selftest(void)
{
    if (!game_init(0) || !game_architecture_selftest()) return false;
    if (game.level.traffic_count!=110 || game.worker_count!=5 || game.barriers!=10 || game.resources!=15 || game.survivors!=6)
        return game_fail("Unexpected original level000 counts: traffic=%d workers=%d barriers=%d resources=%d survivors=%d",game.level.traffic_count,game.worker_count,game.barriers,game.resources,game.survivors);
    double before=game.time;game_key(SDLK_SPACE);game_update(0.1);
    if (game.time!=before) return game_fail("Pause advanced simulation");
    game_key(SDLK_SPACE);
    bool found_block=false;
    for (int i=0;i<game.level.traffic_count;i++) for (int j=i+1;j<game.level.traffic_count;j++)
        if (game_distance(game.level.traffic[i],game.level.traffic[j])<50 && game_blocked(game_current(), game.level.traffic[i],game.level.traffic[j],-1)) found_block=true;
    if (!found_block) return game_fail("Uncleared barriers do not block traffic edges");
    int sample=-1;
    for (int i=0;i<game.object_count;i++) if (game.objects[i].kind==BARRIER && game_dispatch(game_current(), i,true)) { sample=i;break; }
    if (sample<0) return game_fail("No initial barrier is reachable");
    if (game_dispatch(game_current(), sample,true)) return game_fail("A job was assigned twice");
    Vec2 positions[MAX_WORKERS];
    for (int i=0;i<game.worker_count;i++) positions[i]=game.objects[game.workers[i].entity].p;
    game_update(0.1);
    bool moved=false;
    for (int i=0;i<game.worker_count;i++) if (game_distance(positions[i],game.objects[game.workers[i].entity].p)>0.01) moved=true;
    if (!moved || game.objects[sample].done) return game_fail("Movement did not interpolate before timed work");
    before=game.time;game_key(SDLK_SPACE);game_update(0.1);game_key(SDLK_SPACE);
    if (game.time!=before) return game_fail("Pause failed during an active job");
    bool boat_used=false;
    for (int tick=0;tick<30000 && game.completed<game.target_count;tick++) {
        bool active=false;
        for (int i=0;i<game.object_count;i++) {
            Entity *e=&game.objects[i];
            if ((e->kind==BARRIER || e->kind==RESOURCE || e->kind==SURVIVOR) && game_dispatch(game_current(), i,true)) active=true;
        }
        for (int i=0;i<game.worker_count;i++) if (game.workers[i].phase!=IDLE) {
            active=true;if (game.workers[i].boat) boat_used=true;
        }
        if (!active) break;
        game_update(0.1);
    }
    if (game.completed!=game.target_count) {
        for (int i=0;i<game.object_count;i++) {
            Entity *e=&game.objects[i];
            if ((e->kind==BARRIER || e->kind==RESOURCE || e->kind==SURVIVOR) && !e->done)
                return game_fail("Only %d/%d jobs completed; first unreachable '%s' at %.1f,%.1f",game.completed,game.target_count,e->anim,e->p.x,e->p.y);
        }
        return game_fail("Workers failed to return: %d/%d jobs",game.completed,game.target_count);
    }
    if (game.rescued!=game.survivors || game.cleared!=game.barriers || game.collected!=game.resources) return game_fail("Gameplay progress counters disagree");
    if (!boat_used || game.food<=0 || game.wood<=0 || game.gas<=0) return game_fail("Boat dispatch or resource inventory failed");
    for (int i=0;i<game.level.traffic_count;i++) for (int j=i+1;j<game.level.traffic_count;j++)
        if (game_distance(game.level.traffic[i],game.level.traffic[j])<50 && game_blocked(game_current(), game.level.traffic[i],game.level.traffic[j],-1)) return game_fail("Cleared barriers still block the graph");
    for (int i=0;i<game.worker_count;i++) {
        Worker *w=&game.workers[i];if (w->phase!=IDLE || game_distance(game.objects[w->entity].p,w->home)>0.01) return game_fail("Worker failed to return home");
    }
    printf("PASS: original level000, %d graph points, %d completed jobs (%d rescues); movement, work, pause, resource collection and return home.\n",game.level.traffic_count,game.completed,game.rescued);
    if (!game_init(0)) return false;
    if (game.completed || game.food || game.wood || game.gas || game.rescued) return game_fail("Restart did not reset progress");
    /* Exercise retail regions in the same logical coordinates as SDL input. */
    game_pointer(42,46,false);game_draw(); /* hover */
    game_pointer(42,46,true);game_draw();  /* pressed */
    game_click(42,46);
    before=game.time;game_update(.1);
    if (!game.paused || game.time!=before) return game_fail("Retail pause button failed");
    game_pointer(683,298,false);game_draw();
    game_pointer(683,298,true);game_draw();
    game_click(683,298);
    if (game.paused) return game_fail("Retail Continue button failed");
    game_update(.1);
    game_click(42,46);game_pointer(683,411,true);game_draw();game_click(683,411);
    if (game.paused || game.time || game.completed) return game_fail("Retail Play again button failed");
    game_key(SDLK_ESCAPE);
    if (!game.paused) return game_fail("Escape did not open the retail pause menu");
    game_key(SDLK_ESCAPE);
    if(last_error[0])return false;
    puts("PASS: retail HUD fonts/atlas states, pause, Continue, Play again and Escape controls.");
    return particles_selftest() && scenery_selftest() && interaction_selftest() && campaign_selftest() && missions_4_6_selftest();
}
