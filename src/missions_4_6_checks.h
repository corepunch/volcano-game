/* Real click/queue playthroughs. No inventory grants or direct completion flags. */
static bool missions_4_6_selftest(void)
{
    if (!game_init(campaign_index(4))) return false;
    int recruit=test_entity(891);
    for (int i=0;i<20;i++) game_update(.1);
    if (game.objects[recruit].transport!=BY_ATV || game.workers[1].phase!=STRANDED) return fail("Offroad worker escaped without ATV");
    test_click_entity(recruit);
    if (game.transport_trips!=1 || game.gas!=60 || !game.objects[recruit].assigned) return fail("ATV did not reserve one fuel payment");
    test_click_entity(recruit);
    if (game.transport_trips!=1 || game.gas!=60) return fail("ATV rescue reserved twice");
    Vec2 vehicle=game.objects[game.vehicles[0].entity].p;double before=game.time;
    game_key(SDLK_p);for (int i=0;i<20;i++) game_update(.1);game_key(SDLK_p);
    if (game.time!=before || distance(vehicle,game.objects[game.vehicles[0].entity].p)>.001 || game.gas!=60) return fail("Paused ATV kept moving or spending fuel");
    if (!test_frame("campaign-4-atv-outbound")) return false;
    game_key(SDLK_r);
    if (game.transport_trips || game.gas!=70 || game.vehicles[0].target!=-1 || game.objects[test_entity(891)].assigned) return fail("Restart retained an active ATV rescue");
    if (!game_init(campaign_index(6))) return false;
    int fish=test_entity(462),pier=test_entity(858),plot=test_entity(1330);
    if (gate_open(&game.objects[fish]) || gate_open(&game.objects[pier])) return fail("Polluted basin is open on load");
    test_click_entity(fish);test_click_entity(pier);
    if (game.transport_trips || game.gas!=220 || game.wood!=90 || game.objects[pier].assigned) return fail("Pollution gate spent resources or allowed fishing");
    if (test_choose_building(plot,1) || game.wood!=90 || game.objects[plot].assigned) return fail("Level 6 allowed a forbidden cafe");
    game_key(SDLK_ESCAPE);
    if (!test_frame("campaign-6-polluted")) return false;
    if (!game_init(campaign_index(5))) return false;
    int fog=test_entity(847);
    test_click_entity(fog);
    if (game.objects[fog].assigned || game.objects[fog].queued || game.objects[fog].done || game.money!=40) return fail("Fog could be cleared without its valve");
    puts("PASS: ATV-only rescues, exact-once fuel, pause/restart, pollution-gated piers/fishing, linked fog valves and forbidden cafes.");
    const int goals[]={13,6,2},animal_goals[]={0,16,6},foods[]={60,70,60},woods[]={80,80,90},cash[]={70,40,40},fuel[]={70,0,220};
    for (int mission=4;mission<=6;mission++) {
        fprintf(stderr,"Testing campaign level %d...\n",mission);
        if (!game_init(campaign_index(mission))) return false;
        int n=mission-4;
        if (game.goal!=goals[n] || game.animals!=animal_goals[n] || game.food!=foods[n] || game.wood!=woods[n] ||
            game.money!=cash[n] || game.gas!=fuel[n]) return fail("Level %d initial mission data",mission);
        int starting_crew=0;
        for (int i=0;i<game.worker_count;i++) if (!game.workers[i].recruiting) starting_crew++;
        if (starting_crew!=(mission==5?2:1)) return fail("Wrong starting crew in level %d",mission);
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
                        if (distance(e->p,e->origin)>1) saw_roaming=true;
                        if (tick>200) test_click_entity(i);
                    }
                }
            }
            game_update(.1);
            if ((game.rescued==game.goal && game.animals_rescued<game.animals) ||
                (game.animals_rescued==game.animals && game.rescued<game.goal)) saw_partial=true;
            if (game.food<0 || game.wood<0 || game.money<0 || game.gas<0) return fail("Level %d overspent inventory",mission);
        }
        if (!game.won) {
            fprintf(stderr,"Level %d stalled: people %d/%d animals %d/%d basins %d/%d food=%d wood=%d money=%d fuel=%d trips=%d fish=%d\n",
                mission,game.rescued,game.goal,game.animals_rescued,game.animals,game.purified,game.basin_goal,game.food,game.wood,game.money,game.gas,game.transport_trips,game.fishing_trips);
            for (int i=0;i<game.object_count;i++) {
                Entity *e=&game.objects[i];
                if (!e->hidden && !e->done && (e->kind==RECRUIT || e->kind==ANIMAL || e->kind==VALVE || e->kind==SERVICE))
                    fprintf(stderr," pending %u %s kind=%d at %.0f,%.0f gate=%d transport=%d queued=%d assigned=%d\n",e->seed,e->anim,e->kind,e->p.x,e->p.y,e->gate,e->transport,e->queued,e->assigned);
            }
            return fail("Campaign level %d did not complete",mission);
        }
        if (game.rescued!=game.goal || game.animals_rescued!=game.animals || game.purified!=game.basin_goal)
            return fail("Mission finished before every objective");
        if (mission==4 && game.transport_trips!=5) return fail("Level 4 bypassed an ATV rescue");
        if (mission>=5 && !saw_roaming) return fail("Animal rescue bypassed wandering");
        if (mission>=5 && !saw_partial) return fail("Multiple mission objectives were not exercised independently");
        if (mission==6 && (!game.fishing_trips || game.purified!=2)) return fail("Level 6 bypassed fishing or purification");
        char shot[48];snprintf(shot,sizeof shot,"campaign-%d-complete",mission);
        if (!test_frame(shot)) return false;
        printf("PASS: campaign level %d completes: %d people, %d animals, %d basins; %d vehicle trips, %d fishing deliveries.\n",
            mission,game.rescued,game.animals_rescued,game.purified,game.transport_trips,game.fishing_trips);
        game_click(683,GAME_HEIGHT-470);
        if (game.campaign!=mission+1 || game.won) return fail("Level %d progression failed",mission);
        if (!game_init(campaign_index(mission))) return false;
        game_key(SDLK_r);
        if (game.rescued || game.animals_rescued || game.purified || game.transport_trips || game.food!=foods[n] || game.gas!=fuel[n])
            return fail("Level %d restart leaked mission state",mission);
    }
    return true;
}
