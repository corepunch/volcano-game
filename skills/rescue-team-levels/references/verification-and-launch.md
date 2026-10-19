# Verification, diagnosis and launch

## Check, run and package

Run from the selected repository root:

```sh
python3 tools/mission.py check missions/SLUG/mission.xml
make
make run MISSION=SLUG
# equivalent: ./build/volcano --assets reference/rescue-team --mission missions/SLUG/mission.xml
```

Treat `SLUG` as the new mission folder. `check` reports load errors and design
warnings as text; it is not a mission solver. There is no compile step or
sidecar: the game reads `mission.xml`, its background, sprite sheets and packs
directly from the folder. `make run` and `make app` accept any folder under
`missions/`, so a new mission needs no new Make targets. Useful variants:

| Command | Purpose |
| --- | --- |
| `make run MISSION=SLUG ASSETS=PATH` | Use another retail asset folder |
| `make hd-pack` then `make run-hd MISSION=SLUG` | HD statics overlay pack (`build/packs/hd`) plus `background@2x.png` when present |
| `./build/volcano --pack FOLDER --mission …` | Mount extra asset packs (repeatable; later wins) |
| `make run-campaign CAMPAIGN=N` | Original campaign mission for comparison |

For a macOS app:

```sh
make app APP_MISSION=SLUG
```

`tools/package_app.py` creates `build/Mission Name.app` (title from the folder
name). The bundle is **self-contained**: it copies the binary, the retail asset
folder, `content/`, `missions/` and the project's packs, and its launcher refers
only to paths inside the bundle. Retail assets are not redistributable; share
such a bundle only where licensed. Without `APP_MISSION` it opens the campaign.
Do not overwrite an existing bundle for an unrelated mission without reason.

When desktop tools are available, open/select the app through the available
native app tool and inspect its actual game window. A successful package command
alone does not prove the game is running. If a sandbox terminal launch cannot
connect to the display, use supported native app control rather than repeatedly
rerunning the same display failure. If user interaction invalidates native input,
refresh the current UI state; do not claim a rejected reload succeeded.

**R reloads the mission file and resets the mission; it does not reload
executable code.** Use it after `mission.xml`/sprite/art changes when
appropriate. Code/UI changes need restarting the game or rebuilding the app.
P/Space pause, F1 displays graph links and worker paths, and Enter replays a
completed mission. Do not reset an active user playthrough just to demonstrate
a nonessential UI change.

## Visual acceptance

Inspect actual renders at the logical aspect ratio. Check:

- Measured clearing centers and actual buildings coincide; compare damaged and
  repaired states. Do not infer correctness solely from an XML coordinate.
- Buildings and shadows do not cover paths, pickups or rescue characters.
- Every actionable sprite can be hovered and clicked at a natural point; the
  hover ring is also where workers stop, so it should sit on the approach road.
- HUD corners do not obscure jobs, rescues or essential scenery.
- No painted fake resources/buildings/obstacles conflict with actual gameplay.
- Workers follow painted trails, including final approaches and return trips,
  and stand at the edge of each job facing it.
- Foam is white at waterfall feet, glints stay in water, insects are sparse,
  and pause/restart preserve deterministic visual behavior.
- Tooltips have up to three centered rows: name, Required, Income, with original
  resource icons. Omit unused rows. Keep total cost visible while queued; no
  strategy hints. Compare income with actual completion/delivery/production.

The renderer uses trimmed frames: even correct pivot math benefits from visual
review. Recheck after changing art, actors, building anchors or approach points.

## Playthrough acceptance

A new mission is not automatically covered by the existing regression flags.
`tests/missions_main.c` runs `missions/first-rescue` in its default mode;
`--emerald`, `--mayday`, `--ancient`, `--coaster` and `--library-rescue` assume
those missions' ids, goals and budgets. `tests/coaster_main.c` covers Twisted
Tracks' debris archetypes. Do not run one against an unrelated map and pretend
it validates arbitrary missions.

Add a dedicated playthrough mode or adapt a suitable existing mode deliberately
in `tests/missions_main.c`, and add its line to the `test-missions` target in
the `Makefile`. Tests load the mission with `game_init_file` and use
`test_entity`, `test_click_entity`, `game_update` and `test_frame`. Use normal
click/queue semantics; do not inject resources, mark jobs complete or waive
costs to make it pass. Tests run on the generated placeholder pack
(`build/packs/test`), so check logic there and visuals with retail art.

Verify what the mission actually requires:

1. Correct starting resources, goals, animal count and crew/recruit distinction.
2. Each intended gated survivor initially has no open route home.
3. Gates charge their actual costs exactly once; balances never go negative.
4. Mandatory production and pickup delivery remain reachable and functional.
5. A plausible human opening, not only an automated ideal repair priority.
6. Resource-depleted states, pending deliveries and queued jobs recover when
   a legitimately affordable producer/route becomes available. An unaffordable
   queued job must not prevent a later affordable repair.
7. Alternative orders where the design offers a choice. Do not assume every
   possible spending choice must win unless that is part of the requested design.
8. Every person/animal/basin goal completes with correct home/transport behavior.
9. Pause stops progress; restart reloads data and restores the full initial state.
10. Economy tooltips agree with actual spending and income.

Useful existing tests reproduce Forest Mayday's 0-food/60-wood opening and finish
the larger disaster missions with three production repair priorities. Preserve
those checks when changing shared economy, route or queue code.

```sh
make test                                     # core, all missions (placeholder art), tool tests
make test-retail ASSETS=reference/rescue-team # retail campaign suites and missions on retail art
make sanitize                                 # the test suite under ASan/UBSan
```

Run the checks appropriate to the work. After engine changes, run the full suite
and `make sanitize`. Once checks pass, repeat them only after another relevant
change or unresolved concern.

Screenshots come from the regression helpers:

```sh
VOLCANO_TEST_SHOTS=1 make test-missions
VOLCANO_TEST_SHOTS=1 make test-missions MISSION_ASSETS=reference/rescue-team   # retail art
```

Each mode writes its named frames (for example `build/forest-mayday-complete.bmp`
or `build/twisted-tracks-start.bmp`) to `build/`. A single run is also possible:
`SDL_VIDEODRIVER=dummy ./build/test_missions reference/rescue-team missions/forest-mayday/mission.xml --mayday`.
Convert screenshot encodings for viewing if necessary; do not substitute an
invented rendering or modify the art to conceal a layout problem. Headless
completion validates scheduling/rendering, not native desktop launch.

## Diagnose a reported stall

Read visible inventory and queued/assigned state, reproduce the user's actual
sequence, and inspect live code. Distinguish unaffordable work from busy workers,
blocked routes, disconnected traffic, fog/pollution gates, vehicle fuel, a hidden
pickup, overlap/hit selection, and return-trip failures. A queued checkmark means
queued/assigned, not already paid or finished. Messages should state facts rather
than give puzzle-solving hints; hover cards retain Required and Income.

For a route failure inspect nearest points, all pairwise links below 50 units,
20-unit obstacle blocking, where the route enters the target's footprint ellipse
(or its `workpoint`), and whether returning workers are stranded. For an economy
failure track reachable stock and pending deliveries, not the whole map's total.
Prove the recovery with ordinary actions before claiming the issue is fixed. If
a player can continue their current run, explain that without forcing an
unnecessary reset.

## Deliverables

Retain `missions/SLUG/` with `mission.xml`, the selected background, its exact
prompt, any sprite sheet and atlas with its prompt, and design notes; the new
regression mode; and concepts or superseded art under `art-source/`. Screenshots
and app bundles belong under `build/`. Keep original retail asset dumps out of
version control. Report concise launch instructions and the checks actually
performed.
