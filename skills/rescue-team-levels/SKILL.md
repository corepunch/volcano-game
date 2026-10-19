---
name: rescue-team-levels
description: Create, edit, balance, compile, visually verify, and launch custom Rescue Team 5 levels in the volcano-sdl2 restoration. Use for level requests, source authoring, scenery/effect placement, and custom mission debugging; not for unrelated games or general image creation.
---

# Rescue Team levels

Deliver editable level JSON, its background and prompt, compiled binary and
dependencies, verified progression, and a way to launch it. For a narrow fix,
preserve the rest of the mission and verify the affected behavior.

## Locate the project and evidence

Default checkout: `/Users/igor/Developer/volcano-sdl2`. Prefer the user's current
or explicitly selected checkout. Work from its root. Original assets normally
live in `reference/rescue-team`; Make accepts `ASSETS=...`.

Read repository instructions, `docs/creating-levels.md`, and
`docs/level-source.schema.json`. Check current code when a behavior matters:

| Question | Repository authority |
| --- | --- |
| Source/binary contract, dependencies | `tools/level_source.py`, `src/level.c` |
| Prices, routing, queueing, production, rescues, tooltips | `src/game.c` |
| Sprite frames/pivots/drawing | `src/resources.c`, original `texture/lazy/*.lua` |
| Placed effects and limits | `src/scene.c`, `src/volcano.h` |
| Playthroughs and render checks | `tests/check_custom_level.c`, `src/*checks.h`, `Makefile` |

References describe this restoration, not guaranteed retail parity. Reconcile
them with live code if the engine changes.

## Step by step

1. **Define the mission and its budget.** Use the user's theme and scope. Choose
   goals, crew, starting supplies, route choices and gate dependencies. Read
   [source and gameplay](references/source-and-gameplay.md) for the format,
   supported mechanics and link rules. Start with `assets/starter.level.json`
   for a small verified pattern, or a suitable project level. Emerald Falls and
   Forest Mayday are examples, not mandatory layouts. Budget reachable supplies
   before each mandatory job; check reasonable alternative spending orders.

2. **Plan playable geography.** Sketch HQ, paths, gates, building pads, pickups
   and rescue clearings on a 1366×768 canvas. Art briefs use top-left screen
   coordinates; source records use bottom-left world coordinates. Keep important
   content clear of the HUD. Keep unrelated routes at least 50 world pixels apart.
   For wreck/disaster maps, read [interior roads and themed obstacles](references/level-design.md).
   Arrange the wreck around playable interior branches; do not leave most of the
   center inaccessible while confining jobs and rescues to perimeter trails.

3. **Create/select the background.** Read [art and effects](references/art-and-effects.md).
   For new painted artwork use the available imagegen skill and built-in image tool.
   Save the final image and exact prompt as `levels/SLUG.png` and
   `levels/SLUG.prompt.txt`; reference `@SLUG.png`. Preserve existing images when
   creating a new level. Keep this project's forest, plants and overlapping
   shadows baked with terrain and static set pieces. Do not repeat the rejected
   conversion of the entire forest into sprite trees. Reuse original UI icons;
   if new icons/states are explicitly needed, generate the entire set together
   in one atlas, following repository instructions.

4. **Measure the actual painting.** Inspect generated output rather than trusting
   requested coordinates. Measure trail centerlines, clearing centers, bridge
   gaps and waterfall feet, then convert those measurements into game coordinates.
   If the painting differs from the brief, adapt the source or refine the art.

5. **Place gameplay using real pivots.** Write `levels/SLUG.level.json` with a
   complete mission, unique IDs, one HQ, starting workers, routes and objectives.
   Retail building pivots are not visual centers. Read frame dimensions/offsets
   and use `scripts/layout.py anchor` to calculate an anchor from a measured
   clearing center. Compare damaged and repaired frames. Do not change the
   renderer or atlas pivots to compensate for poor level placement. Keep linked
   production pickups and approaches outside both building footprints.

6. **Build routes and add effects.** Use `scripts/layout.py route` or sampled
   points along the painted trails. Every pair closer than 50 pixels connects;
   barriers block segments closer than 20 pixels to their centers. Inspect
   attachment segments, gate bypasses and return trips. Add generic `effect`
   records for foam, water glints, insects and motes. Keep the existing `MapScene`
   abstraction, not waterfall-specific storage. Foam is pure white and belongs
   at measured waterfall feet; do not add falling-water particles. Keep glints
   in water and use the original butterfly animation where appropriate.

7. **Check and compile.** Run the commands below. Investigate every warning;
   intentionally separate boat networks can be valid. Compilation packages local
   images and writes the mission sidecar. Validation alone proves neither
   solvability nor visual alignment.

8. **Verify pictures and play.** Read [verification and launch](references/verification-and-launch.md).
   Inspect actual start and repaired-state renders. Check clearings, pivots,
   click targets, pickups, HUD overlap and effect placement. Hover cards use up
   to three rows: **Name → Required → Income**, omitting inapplicable rows.
   Amounts use original resource icons, retain full costs while queued, and
   include no strategy hints. Income must agree with actual rewards/production.
   Complete the new mission through ordinary input/scheduling without resource
   grants or direct completion flags. Exercise alternative openings, depleted
   food, queues, production recovery, pause, restart and every objective. Existing
   tests do not automatically test a newly named mission.

9. **Make launch reproducible.** Add appropriate Make targets and new-level
   checks. Keep source/art/prompt in `levels/` and generated binaries in `build/`.
   Launch when requested, and verify the intended level is visibly running before
   claiming success. Preserve a user's active run when relaunch is unnecessary.
   **R reloads data and resets the mission; rebuilt code/UI requires reopening
   the app.** Explain that distinction when relevant.

10. **Hand over concrete results.** Report the mission, editable source/art paths,
    launch command/app, and checks performed. Disclose unverified behavior.
    For generated artwork include the saved prompt and generation method.
    Restoration tests do not justify claiming compatibility with the retail game.

```sh
python3 tools/level_source.py check levels/SLUG.level.json
python3 tools/level_source.py compile levels/SLUG.level.json build/levels/SLUG.lvl
make all
```

## Layout helpers

Run these helpers by their absolute path if the skill is installed outside the
checkout. They print JSON; they do not mutate levels, images or original assets.

```sh
python3 SKILL_DIR/scripts/layout.py route --screen \
  --points '[[165,450],[450,450],[660,450]]' \
  --points '[[450,450],[455,395],[455,350]]'
python3 SKILL_DIR/scripts/layout.py anchor \
  --assets reference/rescue-team --sprite burger0_br \
  --center 330.25 327.25 --screen --compare-sprite burger0
```

Replace `SKILL_DIR` with this skill's actual directory. Merge generated traffic
deliberately; helper sampling does not rule out accidental geometric shortcuts.
