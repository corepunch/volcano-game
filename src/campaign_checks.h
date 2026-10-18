/* Included by game.c so regression runs exercise the actual scheduler and UI. */
static int test_entity(uint32_t seed)
{
    for (int i=0;i<game.object_count;i++) if (game.objects[i].seed==seed) return i;
    return -1;
}
static void test_click_entity(int index)
{
    Vec2 p=game.objects[index].p;game_click(p.x,GAME_HEIGHT-p.y);
}
static bool test_choose_building(int lot,int type)
{
    test_click_entity(lot);
    if (game.build_menu!=lot) return false;
    Vec2 p=build_button_position(type);game_click(p.x,GAME_HEIGHT-p.y);
    return game.objects[lot].assigned;
}
static bool campaign_selftest(void)
{
    if (campaign_index(1)!=15 || campaign_index(2)!=18 || campaign_index(3)!=19 || campaign_number(0)!=38)
        return fail("Original campaign order regression");
    for (int i=0;i<50;i++) {
        if (!game_init(i)) return false;
        game_update(1./60);game_draw();
        if (last_error[0]) return false;
    }
    puts("PASS: all 50 original missions initialize and render; campaign order comes from map prerequisites.");
    if (!game_init(campaign_index(2))) return false;
    int cafe=test_entity(646),output=test_entity(657);Entity *building=&game.objects[cafe];
    if (building->operational || !game.objects[output].hidden || dispatch(output,true)) return fail("Ruined cafe produced a bonus");
    int starting_wood=game.wood;
    if (!dispatch(cafe,true) || game.wood!=starting_wood-20 || dispatch(cafe,true) || game.wood!=starting_wood-20)
        return fail("Repair did not reserve its cost exactly once");
    for (int i=0;i<500 && !building->operational;i++) game_update(.1);
    if (!building->operational) return fail("Cafe repair never completed");
    int start_food=game.food;
    for (int i=0;i<50;i++) game_update(.1);
    if (game.food!=start_food+5) return fail("Cafe failed to pay recurring automatic income");
    for (int i=0;i<51;i++) game_update(.1);
    if (game.objects[output].hidden || game.objects[output].done) return fail("Cafe failed to expose its authored bonus pickup");
    if (!dispatch(output,true)) return fail("Cafe bonus cannot be collected");
    for (int i=0;i<1000 && output_busy(building);i++) game_update(.1);
    if (output_busy(building)) return fail("Cafe bonus delivery stalled");
    /* Income continues while the finite bonus is carried. A new bonus must not
     * reset the returning worker's target or create a second copy of its cargo. */
    for (int i=0;i<110;i++) game_update(.1);
    if (game.objects[output].hidden || game.objects[output].done) return fail("Delivered cafe bonus did not respawn");
    building->action=DEMOLISH;
    if (!dispatch(cafe,true) || dispatch(output,true)) return fail("Demolition allowed a conflicting bonus reservation");
    for (int i=0;i<500 && building->kind==BUILDING;i++) game_update(.1);
    if (building->kind!=BUILD_LOT || !game.objects[output].hidden) return fail("Demolition did not release its lot and output");
    start_food=game.food;
    for (int i=0;i<100;i++) game_update(.1);
    if (game.food!=start_food) return fail("Demolished cafe kept producing");
    puts("PASS: damaged buildings, atomic repair costs, automatic income, recurring bonus delivery and demolition ownership.");
    for (int mission=1;mission<=3;mission++) {
        if (!game_init(campaign_index(mission))) return false;
        const int goals[]={3,3,5},food[]={20,50,10},wood[]={0,30,100},money[]={0,20,30};
        if (!game.early || game.goal!=goals[mission-1] || game.food!=food[mission-1] ||
            game.wood!=wood[mission-1] || game.money!=money[mission-1]) return fail("Mission %d initial rules",mission);
        int active=0;
        for (int i=0;i<game.worker_count;i++) if (available(&game.workers[i])) active++;
        if (active!=2) return fail("Mission %d starts with %d active workers rather than two",mission,active);
        if (mission>1 && game.worker_count-active!=mission-1) return fail("Stranded workers are already hired");
        game_key(SDLK_p);game_update(.1);game_key(SDLK_p);
        if (game.time || game.rescued || game.food!=food[mission-1] || game.money!=money[mission-1]) return fail("Pause changed mission economy");
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
                        game.objects[rock].done && !output_busy(site)) {
                        test_click_entity(lot);
                        if (game.build_menu==lot) {
                            Vec2 p=build_button_position(2);game_click(p.x,GAME_HEIGHT-p.y);
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
            if (game.food<0 || game.wood<0 || game.money<0) return fail("Mission economy became negative");
        }
        if (!game.won) {
            fprintf(stderr,"Mission %d stalled: rescued=%d/%d food=%d wood=%d money=%d conversion=%d\n",
                    mission,game.rescued,game.goal,game.food,game.wood,game.money,conversion);
            for (int i=0;i<game.object_count;i++) {
                Entity *e=&game.objects[i];
                if ((e->kind==BARRIER || e->kind==SURVIVOR || e->kind==RECRUIT) && !e->done)
                    fprintf(stderr," pending %u %s (%d) at %.0f,%.0f\n",e->seed,e->anim,e->kind,e->p.x,e->p.y);
            }
            return fail("Campaign mission %d did not complete under its restored rules",mission);
        }
        if (game.rescued!=game.goal) return fail("Wrong mission rescue total");
        if (mission==2 && (!repaired || !produced || !spent_money)) return fail("Level 2 skipped its economy: repair=%d income=%d paid=%d",repaired,produced,spent_money);
        if (mission==3 && (!spent_money || conversion!=2)) return fail("Level 3 skipped house-to-cafe conversion");
        active=0;
        for (int i=0;i<game.worker_count;i++) if (!game.workers[i].recruiting) active++;
        if (active!=mission+1) return fail("Rescued worker did not join crew");
        double finished=game.time;game_update(.1);
        if (game.time!=finished) return fail("Completed mission kept running");
        char shot[40];snprintf(shot,sizeof shot,"campaign-%d-complete",mission);
        if (!test_frame(shot)) return false;
        if (mission==1 && game.objects[test_entity(991)].done) return fail("Tutorial ignored its dynamite gate");
        game_click(683,GAME_HEIGHT-470);
        if (game.campaign!=mission+1 || game.won || game.time) return fail("Next level does not follow campaign order");
        if (!game_init(campaign_index(mission))) return false;
        game.food=0;game_key(SDLK_r);
        if (game.rescued || game.won || game.time || game.food!=food[mission-1] || game.wood!=wood[mission-1]) return fail("Mission restart leaked progress");
        printf("PASS: campaign level %d completes with %d rescues; costs, production, recruits, victory, next level and restart.\n",mission,goals[mission-1]);
    }
    return true;
}
