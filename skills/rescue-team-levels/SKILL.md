---
name: rescue-team-levels
description: Create, edit, balance, visually verify, and launch custom Rescue Team 5 style missions (missions/NAME/mission.xml) in the volcano-sdl2 restoration. Use for mission requests, mission XML authoring, scenery/effect placement, and custom mission debugging; not for unrelated games or general image creation.
---

# Rescue Team levels

Deliver a mission folder (`missions/SLUG/`) with its `mission.xml`, background
and prompt, any mission sprite sheet, verified progression, and a way to launch
it. For a narrow fix, preserve the rest of the mission and verify the affected
behavior.

## Locate the project and evidence

Default checkout: `/Users/igor/Developer/volcano-sdl2`. Prefer the user's current
or explicitly selected checkout. Work from its root. Original assets normally
live in `reference/rescue-team`; Make accepts `ASSETS=...`.

Read repository instructions, `docs/creating-missions.md` and
`docs/content.md`. Check current code when a behavior matters:

| Question | Repository authority |
| --- | --- |
| Mission XML grammar, relationships, effect attributes | `src/content/mission.c` (header comment and loader) |
| Archetypes: prices, rewards, durations, sprites | `content/archetypes.xml`, `src/content/content.c` |
| Routing, footprint approach, queueing, production, rescues | `src/sim/navigation.c`, `src/sim/jobs.c`, `src/sim/economy.c`, `src/sim/tick.c` |
| Sprite sheets, frames/pivots | `src/assets/assets.c` (header comment), original `texture/lazy/*.lua` |
| Effects and drawing, tooltips | `src/render/effects.c`, `src/render/world_view.c`, `src/render/hud.c` |
| Validation and design warnings | `tools/mission.py` |
| Playthroughs and render checks | `tests/missions_main.c`, `tests/coaster_main.c`, `Makefile` |

References describe this restoration, not guaranteed retail parity. Reconcile
them with live code if the engine changes.

## Step by step

1. **Define the mission and its budget.** Use the user's theme and scope. Choose
   goals, crew, starting supplies, route choices and gate dependencies. Read
   [source and gameplay](references/source-and-gameplay.md) for the format,
   supported mechanics and relationships. Start with `assets/starter.mission.xml`
   for a small verified pattern, or a suitable project mission. Emerald Falls and
   Forest Mayday are examples, not mandatory layouts. Budget reachable supplies
   before each mandatory job; check reasonable alternative spending orders.

2. **Plan playable geography.** Sketch HQ, paths, gates, building pads, pickups
   and rescue clearings on a 1368×1026 canvas. Art briefs use top-left screen
   coordinates; mission XML uses bottom-left world coordinates. Keep important
   content clear of the HUD. Keep unrelated routes at least 50 world units apart.
   For wreck/disaster maps, read [interior roads and themed obstacles](references/level-design.md).
   Arrange the wreck around playable interior branches; do not leave most of the
   center inaccessible while confining jobs and rescues to perimeter trails.

3. **Create/select the background.** Read [art and effects](references/art-and-effects.md).
   For new painted artwork use the available imagegen skill and built-in image tool.
   Save the final image and exact prompt as `missions/SLUG/background.png` and
   `missions/SLUG/background.prompt.txt`; reference it with
   `background="background.png"`. Keep concepts and rejected stages under
   `art-source/`. Preserve existing images when creating a new mission. Keep this
   project's forest, plants and overlapping shadows baked with terrain and static
   set pieces. Do not repeat the rejected conversion of the entire forest into
   sprite trees. Reuse original UI icons; if new icons/states are explicitly
   needed, generate the entire set together in one atlas, following repository
   instructions.

4. **Measure the actual painting.** Inspect generated output rather than trusting
   requested coordinates. Measure trail centerlines, clearing centers, bridge
   gaps and waterfall feet, then convert those measurements into game coordinates.
   If the painting differs from the brief, adapt the mission or refine the art.

5. **Place gameplay at base centres.** Write `missions/SLUG/mission.xml` with
   complete `<settings>`, unique object ids, one `hq`, starting workers, routes and
   objectives. An object's x,y is the centre of its base: the engine centres the
   shown frame's body there and the pivot sets only the ground line, so place a
   building at its measured clearing centre (`scripts/layout.py place`) and never
   offset coordinates for a sprite. Workers stop on a target's
   selection ellipse, not its center: keep each obstacle's approach side on a
   reachable road, and keep production pickups and approaches outside both
   building footprints. Themed jobs and stations are mission `<archetype>`s that
   `extends` an existing one, with art from a mission `sprites.xml`.

6. **Build routes and add effects.** Use `scripts/layout.py route` or sampled
   points along the painted trails; it prints a `<traffic>` element. Every pair
   closer than 50 units connects; obstacles block segments closer than 20 units
   to their centers. Inspect attachment segments, gate bypasses and return trips.
   Add `<effect>` elements for foam, water glints, insects and motes. Foam is pure
   white and belongs at measured waterfall feet; do not add falling-water
   particles. Keep glints in water and use the original butterfly animation
   where appropriate. Place world-depth props as `decor` objects, not effects.

7. **Check.** Run the commands below. Investigate every warning;
   intentionally separate boat networks can be valid. There is no compile step:
   the game loads `mission.xml`, its images and sprite sheets directly.
   Validation alone proves neither solvability nor visual alignment.

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

9. **Make launch reproducible.** `make run MISSION=SLUG` works for any mission
   folder; `make app APP_MISSION=SLUG` builds a self-contained bundle. Add a
   playthrough mode for the new mission to `tests/missions_main.c` and the
   `test-missions` target. Keep mission files in `missions/SLUG/` and generated
   screenshots and bundles in `build/`. Launch when requested, and verify the
   intended mission is visibly running before claiming success. Preserve a
   user's active run when relaunch is unnecessary. **R reloads the mission file
   and resets the mission; rebuilt code/UI requires restarting the game or
   rebuilding the app.** Explain that distinction when relevant.

10. **Hand over concrete results.** Report the mission, its folder and art paths,
    launch command/app, and checks performed. Disclose unverified behavior.
    For generated artwork include the saved prompt and generation method.
    Restoration tests do not justify claiming compatibility with the retail game.

```sh
python3 tools/mission.py check missions/SLUG/mission.xml
make
make run MISSION=SLUG
```

## Layout helpers

Run these helpers by their absolute path if the skill is installed outside the
checkout. They print XML snippets to paste into `mission.xml`; they do not mutate missions, images or original assets.

```sh
python3 SKILL_DIR/scripts/layout.py route --screen \
  --points '[[165,450],[450,450],[660,450]]' \
  --points '[[450,450],[455,395],[455,350]]'
python3 SKILL_DIR/scripts/layout.py place \
  --center 330.25 327.25 --screen --id 30 --type cafe
```

Replace `SKILL_DIR` with this skill's actual directory. Merge generated traffic
deliberately; helper sampling does not rule out accidental geometric shortcuts.
