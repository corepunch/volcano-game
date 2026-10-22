# Creating and revising playable backgrounds

This is the production workflow used for Ancient Echoes and Twisted Tracks:
paint the environment with the built-in ImageGen tool, measure the actual
painting, author gameplay separately, and verify the resulting map in the real
SDL renderer and scheduler. Run the commands below from the repository root.

## Files and tools

| Tool or file | Use in these missions |
| --- | --- |
| Built-in `image_gen.imagegen` | Generate the original backgrounds and edit their composition/access paths |
| Image viewing tool | Inspect each original/edit and the real game screenshots before accepting it |
| [Mission skill](../skills/rescue-team-levels/SKILL.md) | Authoring workflow, supported mechanics and verification requirements |
| [Art reference](../skills/rescue-team-levels/references/art-and-effects.md) | Canvas coordinates, sprite pivots, foliage, water and other ambient effects |
| [Layout helper](../skills/rescue-team-levels/scripts/layout.py) | Print an `<object>` standing on a measured clearing centre; sample traffic polylines into `<traffic>` |
| Python 3 | Sample measured routes, run the mission checker and record provenance |
| [Mission checker](../tools/mission.py) | Validate `mission.xml` and report route, crew and goal warnings |
| [Mission guide](creating-missions.md) | The `mission.xml` format, archetypes, routes and economy |
| C11 compiler, SDL2, `make`, `pkg-config` | Build the actual game and regression runner |
| [Mission regressions](../tests/missions_main.c) | Normal click/queue playthroughs and real rendered frames |
| Python Pillow | Convert the runner's BMP screenshots to PNG for visual inspection |
| [App packager](../tools/package_app.py) | Build a self-contained macOS app for one mission (`make app APP_MISSION=NAME`) |
| Git | Retain each verified step, original art and prompts, and publish changes |

The image-generation skill available in the authoring environment supplied the
built-in tool workflow. These backgrounds used that tool directly; no CLI/API
fallback, custom image SDK, Blender scene or procedural background painter was
used. The existing [Blender character workflow](rendering-characters.md) is a
separate process. The runtime reads original game assets from the ignored
`reference/rescue-team/` directory; installation is covered by the
[project README](../README.md#quick-start). Pillow is only needed for the
screenshot conversion commands below, not to load a mission.

The [preserved history](../art-source/background-history/README.md) contains all five
generation/edit stages, including the intermediate coaster image. Its
[manifest](../art-source/background-history/manifest.json) records hashes, dimensions
and input relationships. Each mission folder, `missions/SLUG/`, holds the
deliverables the game loads: `mission.xml`, `background.png` and its exact
`background.prompt.txt`. Concepts, earlier stages and rejected images live under
`art-source/`; screenshots and app bundles belong under ignored `build/`.

## Plan composition and rescue access

Use a **1368 × 1026 logical canvas**, with a high fixed camera and no horizon.
All six production backgrounds are 1368 × 1026, with 2736 × 2052 density exports.
Their native generated masters are 1448 × 1086; higher density does not add detail. Keep actionable objects clear of the upper-left HUD region
around x=0–446, y=0–100 and upper-right x=1010–1366, y=0–100. Check the bottom
message area in actual renders too.

The revised compositions make the disaster itself occupy the centre and east,
rather than a small landmark above a broad network of gardens. The temple edit
requested roughly 60% of visible land for the ruined complex; the coaster edit
requested roughly 65% of the image for the ride. These are art-direction targets,
not measured coverage guarantees. The retained images show the accepted result.

Keep a short formal public approach near headquarters. Deeper rescue access
should look improvised: narrow irregular dirt, crushed grass, gravel, ruts and
swept gaps among debris. Use the shared [road and bridge dimensions](road-and-bridge-sizes.md):
24 logical pixels of clear road width (22–26 acceptance range), measured
perpendicular to travel. Request this same width in every level, with ragged
uncurbed edges, broken-fence openings and bare rescue pockets.
Retain occasional cracked paving where it belongs to the original tourist site.
Specify the desired surface separately for each branch; a generic request for
"paths" tended to produce wide, tidy promenades everywhere.

Place the large wreckage between and beside these corridors. A fallen column,
rail or support drawn across a supposed route looks blocked even though it
creates no runtime collision. Give the artist explicit bare corridors and
building clearings, then adapt gameplay to the paths that were actually painted.

Keep trees, plants, terrain, overlapping shadows, ancient masonry and the coaster
baked into one background. Workers, survivors, animals, supplies, repairable
buildings and paid barriers are separate runtime objects. Exclude them from the
painting, along with UI, text, labels, icons, grid and watermark. Use the existing
UI assets. If new icons are ever required, `AGENTS.md` requires the whole set
and all authored states in one ImageGen atlas call.

## Generate and edit the image

For an initial generation, provide the theme, camera/style, dominant footprint,
trail surfaces, top-left coordinate polylines, bare pads and exclusions. The exact
initial prompts are archived for [Ancient Echoes](../art-source/background-history/ancient-echoes-v1.prompt.txt)
and [Twisted Tracks](../art-source/background-history/twisted-tracks-v1.prompt.txt).

The built-in calls used these arguments:

```text
Initial generation:
  prompt = exact initial prompt text
  transparent_background = false
  omit referenced_image_paths and num_last_images_to_include

Edit:
  prompt = exact edit prompt text
  referenced_image_paths = [absolute path of the input PNG]
  transparent_background = false
  omit num_last_images_to_include
```

Inspect a local input image before submitting an edit. Explicitly identify it
as the edit target and state what should change and what should remain. The
built-in tool returns a saved generation path; copy the chosen PNG into the
mission folder as `missions/SLUG/background.png`. Do not leave a mission
depending on an image only in the authoring environment's generated-images folder.

The temple used one expansion edit. The coaster used an expansion edit followed
by a targeted access edit: the larger wreck initially covered some eastern
approaches, so the second edit cleared a narrow dirt route through the fence
and two rescue pockets while retaining the large structure. See the
[stage table](../art-source/background-history/README.md) for the precise input and
prompt for each call. Do not submit the combined coaster prompt file as though
it had been one historical call.

Inspect every result. Requested coordinates are guidance; they do not establish
where the generated artist actually put a path or clearing. Make another focused
edit when a crucial access gap is missing. For an existing map, preserve its
previous image and prompt in `art-source/background-history/` before accepting a replacement. Record the
full edit sequence, input images and output hashes. Image generation is
nondeterministic; exact pixels are reproduced by retaining the selected PNG,
not by assuming a prompt rerun will match it.

## Measure the painting and author routes

Convert actual image pixels `(ix, iy)` in an image of size `(W, H)` to world
coordinates as follows:

```text
world_x = ix * 1366 / W
world_y = 768 - iy * 768 / H
```

For a 1366 × 768 game render, x already matches world x, and
`world_y = 768 - screen_y`. Exclude window chrome and letterboxing if measuring
a desktop screenshot. Use the painting's trail centreline and the centre of a
bare clearing, rather than the original prompt's coordinates.

Sample routes at about 28–30 logical pixels. The helper prints a `<traffic>`
element ready to paste into `mission.xml`:

```sh
mkdir -p build
python3 skills/rescue-team-levels/scripts/layout.py route --screen --spacing 28 \
  --points '[[155,470],[200,455],[300,450],[450,450],[650,450],[800,450]]' \
  --points '[[450,450],[450,350],[450,280],[480,230],[530,190],[550,160]]' \
  > build/route-example.xml
```

This example contains only two Ancient Echoes polylines. Merge every intended
branch deliberately when authoring a complete map; do not replace the current
network with this partial example. A mission may hold several `<traffic>`
elements, and all their points form one graph. The final `mission.xml` contains
the measured, sampled routes used by the missions. One-off Python authoring
scripts applied the same interpolation during production; no such script is
needed at runtime.

The movement rules come from [navigation.c](../src/sim/navigation.c):

- Every pair of traffic points at distance **less than 50** connects, regardless
  of point order. Keep unrelated routes at least 50 pixels apart.
- An uncleared barrier blocks segments passing **less than 20** pixels from its
  centre. Put a gate on a narrow straight section; nearby diagonal edges at a
  bend can bypass it.
- Objects attach to their nearest traffic point, with their approach and return
  segments checked too. Moving a survivor can change its effective branch.
- Workers stop where their route enters an obstacle's, producer's, lot's,
  service's or valve's selection ellipse (width = sprite body width + 24,
  clamped to 65–170; height 0.62 × width) and face it while working, unless the
  object has a `workpoint`. Keep the approach side of each obstacle on a road
  that is reachable before it is cleared. Pickups, survivors and animals are
  still approached at their centre.
- Painted rubble, supports and fences have no collision. Add real obstacle
  objects for paid work; keep cosmetic debris out of the visible walking strip.

Press F1 for graph links during manual inspection. Validation warnings about
remote approaches or disconnected components require investigation, even when
the image itself looks continuous.

## Place buildings, supplies and effects

An object's x/y is the centre of its base. The engine centres the shown
frame's body on it horizontally and the sprite's pivot sets the ground line,
so a building is placed by measuring its painted clearing's centre and nothing
else; never compensate for a sprite's pivot in the coordinates. The helper
converts a measurement taken in top-left logical pixels:

```sh
python3 skills/rescue-team-levels/scripts/layout.py place \
  --center 310 350 --screen --id 30 --type cafe
```

For this Ancient Echoes cafe the result is
`<object id="30" type="cafe" x="310" y="418"/>`. If art sits too high or low
on its clearing, adjust the sprite's pivot height, not the mission. Inspect
both states and keep pickups outside their combined footprints. The measured
building centres, in top-left logical pixels, are:

| Building | Ancient Echoes | Twisted Tracks |
| --- | --- | --- |
| Headquarters | — | (147,629) |
| Cafe | (310,350) | (499,668) |
| Sawmill | (530,610) | (914,683) |
| House / park office | (835,610) | (1127,666.5) |

These are the 2026-10-19 accepted layouts. The current `mission.xml` is
authoritative if later revisions move anything. Buildings default to layer 5
and characters to layer 6. Production pickups name their producer with
`producer="ID"`; place them where a player can naturally click and a worker can
reach them, outside the building's selection ellipse.

Ambient accents are independent `<effect>` elements: water `sparkles`, the
original animated `butterfly` via `insects`, sparse `motes`, and the coaster
pond's `foam`. Use the [effect reference](../skills/rescue-team-levels/references/art-and-effects.md)
or the [mission guide](creating-missions.md#map-effects) for attributes and bounds. Put glints inside the measured water surface and pure
white foam at the waterfall's impact, not along the falling sheet. Recheck all
effect positions when an edit moves the pond or terrain. Large baked structures
do not need separate sprites to appear in these missions.

## Preserve the mission economy during art changes

The two missions share an intentional test ID convention: barriers 20–25,
cafe/output 30/31, sawmill/output 32/33, house/output 34/35, timber caches
40/41/43, money cache 42, people 50–58 or 50–59, and animals 60/61. Person 53
is a stranded recruit. There are two starting workers, plus that recruit.
Ancient Echoes has nine people to rescue; Twisted Tracks has ten; each has two
animals. Starting stock is 40 food and no wood, money or fuel.

Clearing the first log (20) and small stone (22) spends the 40 starting food.
The log yields 20 wood and the three caches deliver 60, giving 80 wood. Repairing
the cafe costs 20 wood and restores food; the other producers renew timber and
money. The large stone (23) costs 40 food and 100 money. Art changes must preserve
access to these sources before their mandatory expenses. The full supported
costs and mechanics are in the [mission guide](creating-missions.md).

`play_disaster_mission()` in [`tests/missions_main.c`](../tests/missions_main.c)
uses this exact ID/budget contract for `--ancient`, `--coaster` and
`--library-rescue`. A new unrelated mission needs a deliberate
test mode or adaptation. Moving an initial gate, cache or building can invalidate
the recovery puzzle even when total supplies remain unchanged.

## Build, inspect and complete both missions

Each `mission.xml` names its painting with `background="background.png"`,
relative to the mission folder. The game loads it directly; there is no compile
or copy step.

```sh
python3 tools/mission.py check missions/ancient-echoes/mission.xml missions/twisted-tracks/mission.xml
make
VOLCANO_TEST_SHOTS=1 make test-missions
```

`make test-missions` plays every project mission on the generated placeholder
pack; add `MISSION_ASSETS=reference/rescue-team` to render with the retail art.
The runner captures `build/SLUG-start.bmp`, `build/SLUG-repaired.bmp` and
`build/SLUG-complete.bmp` for each of these slugs. Convert them for viewing:

```sh
python3 - <<'PY'
from pathlib import Path
from PIL import Image
for slug in ('ancient-echoes', 'twisted-tracks'):
    for state in ('start', 'repaired', 'complete'):
        source = Path('build') / f'{slug}-{state}.bmp'
        Image.open(source).save(source.with_suffix('.png'))
PY
```

Pillow conversion was used successfully for these SDL BMPs; macOS `sips` failed
to extract them. This conversion changes screenshot encoding only. The painted
backgrounds were copied directly from ImageGen, with no scripted repainting.

Inspect start and repaired renders: disaster scale, actual trail alignment,
clickable jobs, building shadows and trims, supplies, survivors, HUD clearance,
water effects and return routes. A headless playthrough cannot decide whether a
worker appears to walk through a painted column or coaster car; visual inspection
is a separate requirement.

The dedicated checks verify every person and animal is initially gated, correct
crew/recruit distinction, a zero-food opening, affordable cafe repair past
blocked queued jobs, automatic queue recovery, nonnegative balances, completion
in three repair orders, pause and restart. They use ordinary clicks, production
and simulation updates, without stock grants or direct completion flags. Finish
with `make test` for the project suite after relevant mission/test changes, and
`make test-retail ASSETS=reference/rescue-team` when the retail art is available.

## Play, package and hand over

| Purpose | Ancient Echoes | Twisted Tracks |
| --- | --- | --- |
| Build and play | `make run MISSION=ancient-echoes` | `make run MISSION=twisted-tracks` |
| Play with the HD statics pack | `make run-hd MISSION=ancient-echoes` | `make run-hd MISSION=twisted-tracks` |
| Build app bundle | `make app APP_MISSION=ancient-echoes` | `make app APP_MISSION=twisted-tracks` |
| Dedicated test | `--ancient` in `make test-missions` | `--coaster` and `tests/coaster_main.c` in `make test-missions` |

The app target creates `build/Ancient Echoes.app` or `build/Twisted Tracks.app`.
The bundle is self-contained: it copies the binary, the retail asset folder,
`content/`, `missions/` and the project's packs, and refers only to paths inside
itself. Retail assets are not redistributable; share such a bundle only where
licensed. Native desktop launch is a separate check from software-renderer tests.

With `--hd` (as `make run-hd` passes), the game uses `background@2x.png` beside
a mission's `background.png` when it exists. All six maps have matching 4:3
exports, recorded in
[`art-source/retina-backgrounds.json`](../art-source/retina-backgrounds.json).
These exports use resampling, not additional authored Retina detail.

There is no compile step. After changing `mission.xml`, `sprites.xml` or art,
**press R: it reloads the mission and resets it.** A rebuilt executable requires
restarting the game or rebuilding the app. Do not restart an active playthrough
just to demonstrate a revision without considering its state.

Retain the selected image, exact per-call prompts, inputs/history under
`art-source/`, `mission.xml` and the regression mode. Review the diff, commit each completed verified step
before starting the next as required by `AGENTS.md`, and push when requested.
The checks establish behavior in this restoration, not in the retail game.
