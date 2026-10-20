# Creating and revising playable backgrounds

This is the production workflow used for Ancient Echoes and Twisted Tracks:
paint the environment with the built-in ImageGen tool, measure the actual
painting, author gameplay separately, and verify the resulting map in the real
SDL renderer and scheduler. Run the commands below from the repository root.

## Files and tools

| Tool or file | Use in these levels |
| --- | --- |
| Built-in `image_gen.imagegen` | Generate the original backgrounds and edit their composition/access paths |
| Image viewing tool | Inspect each original/edit and the real game screenshots before accepting it |
| [Level skill](../skills/rescue-team-levels/SKILL.md) | Authoring workflow, supported mechanics and verification requirements |
| [Art reference](../skills/rescue-team-levels/references/art-and-effects.md) | Canvas coordinates, sprite pivots, foliage, water and other ambient effects |
| [Layout helper](../skills/rescue-team-levels/scripts/layout.py) | Read original sprite frames and calculate building anchors; sample traffic polylines |
| Python 3 | Edit JSON, sample measured routes, copy files and record provenance |
| [Level compiler](../tools/level_source.py) and [schema](level-source.schema.json) | Validate and compile mission JSON, rules and image dependencies |
| C11 compiler, SDL2, `make`, `pkg-config` | Build the actual game and regression runner |
| [Custom-level runner](../tests/check_custom_level.c) | Normal click/queue playthroughs and real rendered frames |
| Python Pillow | Convert the runner's BMP screenshots to PNG for visual inspection |
| [Launcher packager](../tools/package_custom_level.py) | Create local macOS launchers for the compiled missions |
| Git | Retain each verified step, original art and prompts, and publish changes |

The image-generation skill available in the authoring environment supplied the
built-in tool workflow. These backgrounds used that tool directly; no CLI/API
fallback, custom image SDK, Blender scene or procedural background painter was
used. The existing [Blender character workflow](rendering-characters.md) is a
separate process. The runtime reads original game assets from the ignored
`reference/rescue-team/` directory; installation is covered by the
[project README](../README.md#build-and-run). Pillow is only needed for the
screenshot conversion commands below, not to load a level.

The [preserved history](../levels/background-history/README.md) contains all five
generation/edit stages, including the intermediate coaster image. Its
[manifest](../levels/background-history/manifest.json) records hashes, dimensions
and input relationships. The current `.level.json` files, PNGs and prompts in
`levels/` are the editable deliverables; generated binaries, screenshots and
launchers belong under ignored `build/`.

## Plan composition and rescue access

Use a **1366 × 768 logical canvas**, with a high fixed camera and no horizon.
The saved images for these missions are 1672 × 941 and are mapped across that
logical canvas. Keep actionable objects clear of the upper-left HUD region
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
initial prompts are archived for [Ancient Echoes](../levels/background-history/ancient-echoes-v1.prompt.txt)
and [Twisted Tracks](../levels/background-history/twisted-tracks-v1.prompt.txt).

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
project. Do not leave a level depending on an image only in the authoring
environment's generated-images folder.

The temple used one expansion edit. The coaster used an expansion edit followed
by a targeted access edit: the larger wreck initially covered some eastern
approaches, so the second edit cleared a narrow dirt route through the fence
and two rescue pockets while retaining the large structure. See the
[stage table](../levels/background-history/README.md) for the precise input and
prompt for each call. Do not submit the combined coaster prompt file as though
it had been one historical call.

Inspect every result. Requested coordinates are guidance; they do not establish
where the generated artist actually put a path or clearing. Make another focused
edit when a crucial access gap is missing. For an existing map, preserve its
previous image and prompt in history before accepting a replacement. Record the
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

Sample routes at about 28–30 logical pixels. The helper prints a traffic record:

```sh
mkdir -p build
python3 skills/rescue-team-levels/scripts/layout.py route --screen --spacing 28 \
  --points '[[155,470],[200,455],[300,450],[450,450],[650,450],[800,450]]' \
  --points '[[450,450],[450,350],[450,280],[480,230],[530,190],[550,160]]' \
  > build/route-example.json
```

This example contains only two Ancient Echoes polylines. Merge every intended
branch deliberately when authoring a complete map; do not replace the current
network with this partial example. The final JSON contains the measured,
sampled routes used by the missions. One-off Python authoring scripts applied
the same interpolation during production; no such script is needed at runtime.

The movement rules come from [game_navigation.c](../src/game_navigation.c):

- Every pair of traffic points at distance **less than 50** connects, regardless
  of point order. Keep unrelated routes at least 50 pixels apart.
- An uncleared barrier blocks segments passing **less than 20** pixels from its
  centre. Put a gate on a narrow straight section; nearby diagonal edges at a
  bend can bypass it.
- Objects attach to their nearest traffic point, with their approach and return
  segments checked too. Moving a survivor can change its effective branch.
- Painted rubble, supports and fences have no collision. Add real barrier
  objects for paid work; keep cosmetic debris out of the visible walking strip.

Press F1 for graph links during manual inspection. Validation warnings about
remote approaches or disconnected components require investigation, even when
the image itself looks continuous.

## Place buildings, supplies and effects

Building x/y is an original atlas attachment, not its visible centre. For a
frame of width/height `w,h` and pivot `dx,dy`, use:

```text
anchor_x = clearing_world_x - w/2 + dx
anchor_y = clearing_world_y - h/2 + dy
```

Read actual frames and compare damaged/repaired states with the tracked helper:

```sh
python3 skills/rescue-team-levels/scripts/layout.py anchor \
  --assets reference/rescue-team --sprite burger0_br \
  --center 310 350 --screen --compare-sprite burger0
```

For this Ancient Echoes cafe, the result is world anchor `(308,406.5)`, or
screen attachment `(308,361.5)`. Do not change renderer pivots to compensate
for a map placement. Inspect both states and keep pickups outside their combined
footprints. The final measured building centres, in top-left logical pixels, are:

| Building | Ancient Echoes | Twisted Tracks |
| --- | --- | --- |
| Cafe | (310,350) | (325,430) |
| Sawmill | (530,610) | (665,675) |
| House / visitor office | (835,610) | (1040,675) |

These are the 2026-10-19 accepted layouts. The current mission JSON is
authoritative if later revisions move anything. Buildings use layer 5 and
characters layer 6. Production pickups link to their producer with `link_id`;
place them where a player can naturally click and a worker can reach them.

Ambient accents are independent type-5 `effect` records: water `sparkles`, the
original animated `butterfly` via `insects`, sparse `motes`, and the coaster
pond's `foam`. Use the [effect reference](../skills/rescue-team-levels/references/art-and-effects.md)
for syntax and bounds. Put glints inside the measured water surface and pure
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
costs and mechanics are in the [level guide](creating-levels.md).

`play_disaster_mission()` in the custom-level runner uses this exact ID/budget
contract for `--ancient` and `--coaster`. A new unrelated mission needs a deliberate
test mode or adaptation. Moving an initial gate, cache or building can invalidate
the recovery puzzle even when total supplies remain unchanged.

## Build, inspect and complete both missions

The background reference is `@ancient-echoes.png` or `@twisted-tracks.png` in
the corresponding level JSON. The compiler copies the image unchanged beside
the binary and writes the `.lvl.rules` sidecar. Keep all three together.

```sh
python3 tools/level_source.py check levels/ancient-echoes.level.json
python3 tools/level_source.py check levels/twisted-tracks.level.json
make all
VOLCANO_TEST_SHOTS=1 make test-ancient-echoes test-twisted-tracks
```

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
with `make test` for the project suite after relevant mission/test changes.

## Play, package and hand over

| Purpose | Ancient Echoes | Twisted Tracks |
| --- | --- | --- |
| Build and play | `make ancient-echoes` | `make twisted-tracks` |
| Matching run alias | `make run-ancient-echoes` | `make run-twisted-tracks` |
| Build local launcher | `make ancient-echoes-app` | `make twisted-tracks-app` |
| Dedicated test | `make test-ancient-echoes` | `make test-twisted-tracks` |

The launcher targets create `build/Ancient Echoes.app` and
`build/Twisted Tracks.app`. These launch the current binary, compiled map and
original-asset directory on this machine; they are local launchers rather than
self-contained distributable games. Native desktop launch is a separate check
from software-renderer tests. The existing `hd-levels`/`hd-app` workflow currently
packages Emerald Falls and Forest Mayday; it does not automatically add these
two maps or produce Retina versions of their backgrounds.

Recompile after changing JSON or art. **R reloads level data and resets the
mission.** A rebuilt executable requires reopening the app. Do not restart an
active playthrough just to demonstrate a revision without considering its state.

Retain the selected image, exact per-call prompts, inputs/history, JSON, test
mode and Make targets. Review the diff, commit each completed verified step
before starting the next as required by `AGENTS.md`, and push when requested.
The checks establish behavior in this restoration, not in the retail game.
