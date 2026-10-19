# Verification, diagnosis and launch

## Build and package

Run from the selected repository root:

```sh
python3 tools/level_source.py check levels/SLUG.level.json
python3 tools/level_source.py compile levels/SLUG.level.json build/levels/SLUG.lvl
make all
./build/volcano --assets reference/rescue-team --level-file build/levels/SLUG.lvl
```

Treat `SLUG` as the new filename. `check` reports design warnings as text; it is
not a mission solver. Compilation copies background/sprite images beside the
binary and writes `SLUG.lvl.rules`. Confirm these files are all present.
Add a Make target using source, image(s), and compiler as prerequisites, plus
rules/image targets that recreate missing dependencies. Follow existing
`forest-mayday` or `emerald-falls` targets rather than editing the retail level set.

For a local macOS launcher:

```sh
python3 tools/package_custom_level.py "Mission Name" build/levels/SLUG.lvl reference/rescue-team
```

The tool creates `build/Mission Name.app` and points at the current executable,
compiled level and absolute original-asset directory. This is a **local launcher**,
not a self-contained redistributable game. Do not overwrite an existing launcher
with an unrelated mission. Verify its level path and dependencies.

When desktop tools are available, open/select the app through the available
native app tool and inspect its actual game window. A successful package command
alone does not prove the game is running. If a sandbox terminal launch cannot
connect to the display, use supported native app control rather than repeatedly
rerunning the same display failure. If user interaction invalidates native input,
refresh the current UI state; do not claim a rejected reload succeeded.

**R resets/reloads level data, not executable code.** Use it after JSON/art data
changes when appropriate. Code/UI changes need reopening the app. P/Space pause,
F1 displays graph links, and Enter replays a completed custom mission. Do not
reset an active user playthrough just to demonstrate a nonessential UI change.

## Visual acceptance

Inspect actual renders at the logical aspect ratio. Check:

- Measured clearing centers and actual buildings coincide; compare damaged and
  repaired states. Do not infer correctness solely from a JSON coordinate.
- Buildings and shadows do not cover paths, pickups or rescue characters.
- Every actionable sprite can be hovered and clicked at a natural point.
- HUD corners do not obscure jobs, rescues or essential scenery.
- No painted fake resources/buildings/obstacles conflict with actual gameplay.
- Workers follow painted trails, including final approaches and return trips.
- Foam is white at waterfall feet, glints stay in water, insects are sparse,
  and pause/restart preserve deterministic visual behavior.
- Tooltips have up to three centered rows: name, Required, Income, with original
  resource icons. Omit unused rows. Keep total cost visible while queued; no
  strategy hints. Compare income with actual completion/delivery/production.

The renderer uses trimmed frames: even correct pivot math benefits from visual
review. Recheck after changing art, actors, building anchors or approach points.

## Playthrough acceptance

A new filename is not automatically covered by the existing regression flags.
`check_custom_level --emerald` assumes Emerald Falls IDs/goals; `--mayday` assumes
Forest Mayday. The default checker assumes the starter. Do not run one against
an unrelated map and pretend it validates arbitrary levels.

Add a dedicated playthrough mode or adapt a suitable existing mode deliberately
in `tests/check_custom_level.c`. It includes the real engine implementation and
uses `test_entity`, `test_click_entity`, `game_update`, and `test_frame`. Use normal
click/queue semantics. Setup the mission by loading its compiled file; do not
inject resources, mark jobs complete or waive costs to make it pass.

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
both larger custom missions with three production repair priorities. Preserve
those checks when changing shared economy, route or queue code.

```sh
make test-levels
make test
```

Run the checks appropriate to the work. After engine changes, run the full suite
and relevant memory checks; `make sanitize` runs the project's sanitizer suite.
A focused sanitizer runner can use the matching new mission mode. Once checks
pass, repeat them only after another relevant change or unresolved concern.

Screenshots can be captured through the existing regression helper:

```sh
VOLCANO_TEST_SHOTS=1 SDL_VIDEODRIVER=dummy \
  ./build/check_custom_level reference/rescue-team build/levels/forest-mayday.lvl --mayday
```

This example is specifically for Forest Mayday. It writes BMPs to `build/`.
Convert screenshot encodings for viewing if necessary; do not substitute an
invented rendering or modify the art to conceal a layout problem.
Headless completion validates scheduling/rendering, not native desktop launch.

## Diagnose a reported stall

Read visible inventory and queued/assigned state, reproduce the user's actual
sequence, and inspect live code. Distinguish unaffordable work from busy workers,
blocked routes, disconnected traffic, fog/pollution gates, vehicle fuel, a hidden
pickup, overlap/hit selection, and return-trip failures. A queued checkmark means
queued/assigned, not already paid or finished. Messages should state facts rather
than give puzzle-solving hints; hover cards retain Required and Income.

For a route failure inspect nearest points, all pairwise links below 50 pixels,
20-pixel obstacle blocking, workpoint position, and whether returning workers
are stranded. For an economy failure track reachable stock and pending deliveries,
not the whole map's total. Prove the recovery with ordinary actions before
claiming the issue is fixed. If a player can continue their current run, explain
that without forcing an unnecessary reset.

## Deliverables

Retain editable JSON, selected image, exact prompt, optional referenced sprite
atlases, Make target and new mission check. Generated `.lvl`, `.lvl.rules`, copied
images, screenshots and launcher belong under `build/`. Share compiled files
with their sidecar and local-image dependencies. Keep original retail asset dumps
out of version control. Report concise launch instructions and the checks actually
performed.
