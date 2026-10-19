# Volcano SDL2

A standalone **C11 + SDL2** restoration of ORCA's Volcano game prototype,
using the exact **Rescue Team 5** resources copied into `reference/rescue-team/`.

The source files in `~/Documents/Rescue Team` stay in place. The `reference/`
folder is ignored by Git, with each game in its own subfolder:
`rescue-team/` supplies this game's assets, while `my-kingdom-for-the-princess/`
holds the separate editor reference. The program reads binary `.lvl` files,
PNG/JPEG atlases, JPEG `.alpha` masks, and bitmap font metrics directly at
runtime. Its small data parser accepts the original Lua-shaped atlas/font
syntax as declarations. **There is no Lua library, script execution, asset
conversion, generated XML, or dependency on ORCA.**

## Game architecture

The game uses fixed entity/actor arrays, immutable definition tables, and C
callbacks. The SDL-facing API in `volcano.h` owns a single active world;
implementation modules receive an explicit `Game *`. `game_internal.h` is the
private contract between those modules, not an application API.

| Module | Responsibility |
| --- | --- |
| `src/game.c` | Active-world ownership, transactional initialization, public API, shutdown |
| `src/game_spawn.c` | Retail type → spawn callback registry; validation and entity links |
| `src/game_rules.c` | Building, obstacle, pickup and mission definitions; economy queries |
| `src/game_jobs.c` | Reservation, exact-once spending, completion callbacks and home delivery |
| `src/game_navigation.c` | Route finding, barriers, vehicle access and reachability |
| `src/game_sim.c` | Worker state callbacks, production, vehicles, rescues, animals and victory |
| `src/game_render.c` | Visual poses, depth sorting, effects, HUD, hit testing and tooltips |
| `src/game_input.c` | Pointer/key translation, menu commands and lifecycle requests |
| `src/game_util.c` | Shared geometry, deterministic randomness and diagnostics |
| `tests/game_selftest.c` | Campaign, interaction and rendered regressions linked to production code |
| `tests/game_architecture.c` | World isolation, callback behavior, art independence and failed-load rollback |

The review found four sources of coupling in the former `game.c`: a hidden
current-world macro, duplicated building arrays, gameplay inferred repeatedly
from animation strings, and tests that included the implementation file. These
are replaced by explicit context, shared records, spawn-time rule resolution,
and tests linked against the same modules as the executable.

To add a retail object type, register its `SpawnDef` and callback in
`game_spawn.c`. To add an interaction, define its `JobDef` in `game_jobs.c`:
duration, work animation, completion callback, delivery policy, cost and click
eligibility. The scheduler chooses and reserves the worker, spends once after
finding a route, and starts the return journey after the callback. Construction
uses this same spending path. Completion callbacks change the target and world;
they do not dispatch another job. Building records supply menu labels, prices,
sprites, output resource and bonus pickup together. Obstacle and pickup rules
resolve once from the original level data, so replacing their displayed sprites
does not change fees, wood rewards, cargo amounts or worker compatibility.

Loading builds and validates a fresh zeroed candidate. A successful public init
replaces and frees the old world; failure frees the candidate and preserves the
active world. Entity indices stay stable for a world's lifetime. A successful
init/restart invalidates all old world/entity pointers. Input returns lifecycle
commands; the application applies them after the input handler returns. `game_destroy()` is
idempotent and releases the active world before SDL resources are destroyed.

Update order remains workers → queued jobs → followers → production → vehicles
→ automatic rescues → animals → victory. Timed jobs begin returning in the same
update; immediate pickups begin moving home on the next update. Rendering
computes source-layer and ground-Y keys before sorting, so its comparator needs
no hidden world. Lower layers draw first; within a layer, higher world Y draws
first, with original entity order breaking ties.

Further architectural boundaries remain explicit: the resource cache, renderer
and diagnostic buffer are still application-wide; vehicles and evacuees keep
their specialized movement loops; route queries still scan the traffic graph
and barriers. Cached graph adjacency and a fixed simulation tick are useful
future changes, but need separate behavior/performance validation. Callback
addresses are runtime code references; future saves must serialize stable IDs
and resolve definitions again, rather than persisting pointers.

## Build and run

Requires a C11 compiler, `make`, `pkg-config`, and SDL2. These are already
available on this Mac.

```sh
cd ~/Developer/volcano-sdl2
make run
```

Run either custom level (builds the game and compiles the level as needed):

```sh
make run-emerald-falls
make run-forest-mayday
```

To build our new static sprites at twice the original texture resolution:

```sh
make hd-app
make run-emerald-falls-hd
make run-forest-mayday-hd
```

`make hd-app` creates the self-contained `build/Volcano HD.app`, opening Emerald
Falls, plus `build/Emerald Falls HD.app` and `build/Forest Mayday HD.app` launchers.
Keep those small launchers beside `Volcano HD.app`. The main bundle includes both
missions and their backgrounds. The original run targets continue to use retail
sprites. HD artwork covers every interactive building, debris and resource sprite
used by the two custom missions, plus the fuel station and construction lot:
23 sprites and 46 authored damage/repair states. HD workers and civilians use
our [Kenney character adaptation](assets/kenney-characters/README.md), rendered
from an editable Blender rig in eight directions. Animals, scenery animations
and HUD artwork retain their original textures.

Both HD missions also use AI-upscaled **3344×1882** backgrounds, exactly twice
the original **1672×941** image dimensions. The logical playfield remains
1366×768; paths, objects and click targets keep their original coordinates.
The originals remain in `levels/SLUG.png`; Retina artwork is saved separately as
`levels/SLUG@2x.png`. `make hd-levels` compiles the same mission data into
`build/hd-levels`, with the Retina backgrounds alongside it. All HD run targets
and the HD app use that directory. Upscale settings and source/output hashes
are recorded in [`levels/retina-backgrounds.json`](levels/retina-backgrounds.json).

The ImageGen source sheets, exact prompts and extraction manifest live in
[`assets/hd-static`](assets/hd-static). `tools/build_hd_assets.py` requires Pillow
and packs the authored frames into `build/hd-assets/texture/lazy/volcano-static.png`.
Texture regions have two pixels per world unit, including padding for shadows;
body placement, interaction bounds, frame order and frame rates stay unchanged.
Body artwork is only downsampled when packing, with no enlargement or
script-generated work states.
Original resources under `reference/rescue-team` stay untouched. macOS windows
support Retina backing pixels. `make test-hd` runs the conversion, mission,
resource and campaign checks with the HD asset directory.

HD buildings, debris, bridges, road damage and supplies also have short, soft
ground shadows in the retail style. All 46 shadow states were authored together
in `assets/hd-static/shadows.generated.png`; `shadows.prompt.txt` records the
ImageGen prompt and `shadows.json` records source regions and footprint placement.
The packer combines each authored shadow with its object into one transparent
sprite, as retail buildings do. Shadows project northwest (upper-left) at 45°,
with a grounded near edge and a cast distance proportional to object height;
road damage retains only contact shading. The 55% shadow strength is baked into
the alpha. Runtime draws each object once, with no separate static shadow layer.
Padded frames retain the original body bounds for clicks and highlights.
Retail character shadows remain in use.

On macOS, `make app` also creates the double-clickable `build/Volcano.app`
with its own bundled resource copy. The app does not need the Documents folder.
The tracked bundle metadata lives in `packaging/Info.plist`.

The default is campaign level 1. Select a campaign position with
`./build/volcano --campaign 3`; `--level 19` selects an original source file.
Campaign order is recovered from the original map's prerequisite chain:
the first three missions are `level015`, `level018`, and `level019`.
Campaign levels 4–6 are `level012`, `level013`, and `level011`.
The original prototype, `level000`, is campaign level 38.

To populate the ignored assets on a fresh checkout:

```sh
mkdir -p reference/rescue-team
rsync -a --exclude='.DS_Store' "$HOME/Documents/Rescue Team/" reference/rescue-team/
```

To use another original resource folder:

```sh
./build/volcano --assets "/path/to/Rescue Team"
```

`RESCUE_TEAM_ASSETS` also sets the resource folder; `--assets` takes precedence.
The default is `reference/rescue-team/`, resolved from the working directory
or executable location, with the older `resources/` folder still supported as
a fallback. `make app` bundles assets from `reference/rescue-team/` into
`Contents/Resources`; override the packaging source with `make app ASSETS=/path/to/assets`.

Click debris to send an available worker and collect supplies. In campaign
levels 1–6, people return to headquarters once their route is clear; stranded
workers join the crew after returning and count toward the rescue goal.
Later inspection levels retain the prototype's click-to-escort behavior.
Workers follow the original traffic points, clear blocked paths,
perform timed work, and return home. Pickups happen immediately on arrival;
workers carry supplies back before crediting the inventory. After clearing or
building, workers immediately return to HQ before taking another order.
Fishing boats collect food in level 6; prototype boats collect floating wood
in later inspection levels. Small and large supply piles yield 10 and 20 units. The top bar
uses the retail atlas, original Calibri bitmap fonts, resource icons, timer
meter, three objective rows, and the authored hover/pressed button states.
The pause button opens the retail panel with Continue and Play again controls.
Hover over targets for tooltips and green/red selection circles. Accepted and
queued jobs show green checkmarks; click a queued job again to cancel it.
Working targets use the original progress-bar artwork. Available workers inside
HQ appear as helmet icons above its roof and emerge when dispatched.

Log obstructions whose level object name is `fire` burn with two layers of
40-frame flame animation, restrained warm light, rising embers and expanding,
drifting smoke. The downloaded CC0 textures and their credits live in
[`assets/particles`](assets/particles/README.md). In level000 these are
objects 1249 and 1204, the two trees on the right; the two unmarked trees stay
unlit. Each fire has its own timing; effects pause with the game and
restart with the level. Clearing logs stops emission while existing flames and
smoke finish fading. Independently phased flame patches stay rooted across the
wood and bend in the updraft; smoke dissipates within seven seconds of clearing.

Birds fly left to right at a new random height on each pass, and translucent
clouds drift across the scene. Both wrap fully off screen and use the original
type-6 ambient sprites. Boats, floating wood, flooded houses, and water trees
bob and rock gently without moving their gameplay coordinates. Rooftop survivors
move with their supporting house and draw above its roof; picking them uses
their displayed position. All scenery pauses and resets with the level.

The binary format contains length-prefixed strings as well as numeric fields.
Our reader calls one of these strings `extra`; rooftop survivors store `roof`
there. They have source layer 6, flooded decor has layers 2–3, and ambient objects
normally have layer 9. Rendering respects these source layers, so rooftop
survivors draw above their houses without a special depth override. The available
prototype scripts discard this metadata and provide no retail motion
implementation. This restoration associates each `roof` survivor with the flooded
house underneath it (these records have no link ID) and shares the house's visual
transform so they bob and rock together.

Click a building lot to open the four-button construction menu: Sawmill and
Burger Cafe cost 40 wood, House costs 60, and Gas Station costs 80. Choosing an
affordable building dispatches a worker for five seconds of construction once
the route is clear. Prices turn red and authored disabled artwork appears when
wood is insufficient. Escape or clicking outside dismisses the menu.

Campaign levels 1–6 have repairable production buildings, automatic income,
recurring worker-collected bonus supplies, resource costs and wood rewards
for debris. Linked supplies stay hidden until their building operates.
Click a functioning building and its authored demolition button to release
the lot for a different building. Existing deliveries finish before demolition
can start; pickups cannot be reserved while their owner is being demolished.
Level 3 supports the tutorial's house-to-cafe replacement sequence.

The initial food/wood/money inventories (20/0/0, 50/30/20, 10/100/30) and
rescue goals (3, 3, 5) match the original screenshots and walkthrough:
[GameHouse Rescue Team 5 walkthrough](https://www.gamehouse.com/blog/2015/10/rescue-team-5-walkthrough/).
Repair costs are half construction costs. Trees cost 20 food and yield 20 wood;
small stones cost 20 food, large stones 40 food and 100 money, wooden bridges
20 wood, and large bridges 20 food and 40 wood. Working buildings pay 5 units
every 5 seconds and expose a 10-unit bonus every 10 seconds when no previous
bonus is outstanding. **These costs and intervals are explicit restoration
balance, not recovered retail constants.** The supplied resources do not
contain the original rules or time limits. The first six missions currently
play without a deadline, display elapsed time and finish on their actual
rescue goals, with Next level and Play again controls. Timed stars, upgrades,
save profiles and later campaign mechanics remain unimplemented.

Levels 4–6 add the following restored rules, using original worker, vehicle,
animal and building artwork:

| Campaign level | Objectives | Starting food / wood / money / fuel |
| --- | --- | --- |
| 4 | Rescue 13 people, including 5 workers requiring the ATV | 60 / 80 / 70 / 70 |
| 5 | Rescue 6 people and 16 animals | 70 / 80 / 40 / 0 |
| 6 | Purify 2 basins and rescue 2 people and 6 animals | 60 / 90 / 40 / 220 |

Click an offroad worker to dispatch the ATV from its garage. Each trip reserves
10 fuel once; the worker joins after being delivered and returning to HQ.
Animals wander along opened routes. Clicking one reserves a worker and 10 food;
the animal stops wandering during capture and follows the worker home before
counting as rescued. Fog is cleared through its linked valve.

In level 6, valves purify their own basins and unlock the associated piers.
Repairing a pier costs 40 wood, then its boat can fish only within that basin's
traffic network. A fishing trip costs 10 fuel; food is credited after the boat
returns to its pier. Fishing spots replenish after 10 seconds. Valve work costs
20 money. These fees and intervals use the same provisional restoration balance
as levels 1–3. Cafe construction is disabled in level 6 using the authored locked
menu artwork. Victory requires every people, animal and basin objective.

- **Space / P:** pause or resume
- **R:** restart the current level
- **Enter / N:** advance after completing a restored mission
- **F1:** show traffic connections and worker paths
- **Escape:** dismiss the construction menu, or open/close the pause menu
- Close the window to quit

## Create your own levels

Levels now have an editable JSON source format, a lossless compiler/decompiler,
and a playable example:

```sh
make level-example
python3 tools/level_source.py check levels/first-rescue.level.json
python3 tools/level_source.py compile levels/first-rescue.level.json build/levels/first-rescue.lvl
./build/volcano --level-file build/levels/first-rescue.lvl
```

The [level authoring guide](docs/creating-levels.md) explains source fields,
object types, traffic spacing, obstacle gates, resource budgets, building
outputs, vehicle markers, and patterns recovered from the original campaign.
The [starter source](levels/first-rescue.level.json) has editor support through
the [JSON Schema](docs/level-source.schema.json).
The reusable [Rescue Team level skill](skills/rescue-team-levels/SKILL.md) provides
the full authoring workflow, a tested starter, and helpers for traffic sampling
and placement using original sprite pivots. Invoke it as `$rescue-team-levels`
after installing the skill in your personal Codex skills folder.
Custom mission supplies and goals compile to a `.lvl.rules` companion;
the `.lvl` retains the original version 301 binary layout. Custom levels load
directly without modifying the retail resources. Press R after recompiling to
reload a custom level. `make analyze-levels` inventories the original campaign;
`make test-levels` checks conversion and completes the starter through normal play.

Play **Emerald Falls**, our original forest-and-waterfall level, with
`make emerald-falls`. Its three branches contain seven people, two animals,
and a resource puzzle linking timber, food production, money production, and
the final river crossing. The original painted background preserves the forest's
integrated lighting and shadows. Original Rescue Team butterflies, water glints,
light motes and six white foam patches animate independently over it.
These effects share a layered scene system, pause and reset with the mission,
and remain editable as placed objects.
The editable source is
[`levels/emerald-falls.level.json`](levels/emerald-falls.level.json); its baked
background is `levels/emerald-falls.png`. Regressions complete
the mission with three different production repair priorities.

Play **Forest Mayday**, a forest crash-site rescue around an A320-sized
passenger jet, with `make forest-mayday`. Ten people and two animals wait behind
fallen trees, a rockfall and a damaged evacuation trail. Restore the cafe,
forestry shed and ranger house to fund the rescue. The aircraft, forest and
shadows are baked into the painting; creek foam, water glints, butterflies and
light motes animate independently. Its editable source is
[`levels/forest-mayday.level.json`](levels/forest-mayday.level.json), and the
background and built-in ImageGen prompt are saved as
[`levels/forest-mayday.png`](levels/forest-mayday.png) and
[`levels/forest-mayday.prompt.txt`](levels/forest-mayday.prompt.txt).
`make forest-mayday-app` creates a local macOS launcher in `build/Forest Mayday.app`.
Regressions complete all rescues with three different production repair priorities.

## Verification

```sh
make test
make sanitize
```

Tests read all 52 original levels to their exact end, validate atlas frames,
reject malformed data, and simulate the recovered level through its 31 jobs:
10 obstacles, 15 supply pickups, and 6 escorted survivors. They check movement,
blocked paths, work timing, pause, return journeys, inventories, boats, and
restart, HUD hit regions, retail font bounds, and button artwork in normal,
hover, and pressed states. Additional regressions cover all four construction
choices and costs, immediate pickups and cargo delivery, immediate HQ returns,
queued-job cancellation, and in-house helmets. Rendered particle checks
cover fire animation, pause, restart, and smoke fading after clearing, including
that unmarked trees and other barriers do not burn. Tests use SDL's software renderer without opening
a window. Set `VOLCANO_TEST_SHOTS=1` with `make test`
to save deterministic interaction frames under `build/`.
Scenery checks cover travel and off-screen wrapping, bobbing and rotation,
stationary land, roof attachment/draw order and clicking, release on rescue,
and pause/restart.

Campaign regressions initialize and render all 50 missions, verify the map
order, and complete the first six missions through the normal click and
queue interfaces without injecting resources or clearing obstacles directly.
They cover damaged buildings, exact-once spending, automatic income, recurring
bonus delivery, demolition ownership, recruitment, the house-to-cafe sequence,
mission completion, advancement and restart.
Additional checks cover ATV-only access and exact-once fuel reservations,
vehicle pause/restart, wandering animals and food costs, linked fog and
pollution gates, blocked pier repair/fishing, the cafe restriction, boat cargo
delivery, and independent completion of multiple objectives. Level 5's test
clears only its necessary large boulder; all three new runs finish without
injecting resources or marking tasks complete directly.

A reproducible frame can be saved without a window:

```sh
SDL_VIDEODRIVER=dummy ./build/volcano --software --hidden \
  --frames 60 --screenshot build/preview.bmp
```

## Scope and provenance

The restored gameplay follows the former `samples/Volcano` prototype, first
introduced in ORCA commit `09a68b1d` (2026-02-25). Its later movement and work
callbacks were commented out. This project restores those interactions in C
and reads the original binary level instead of the archived Lua level table.

The installed resources identify themselves as Rescue Team 5 in
`conf/locale_en.lua`. The worker atlas matches the former Volcano atlas byte
for byte. Original artwork and data are kept in the ignored `reference/rescue-team/` directory
and are not intended to be committed.

All 50 missions load and render; the binary reader also accepts the original
map and menu. Artwork aliases resolve money, large stones and building names;
fog and road fire use their authored effect textures. Known editor tiles and
unrestored scene controllers stay in the parsed data without being mistaken
for missing animations. Unknown animation references still fail explicitly.
Only campaign levels 1–6 have the expanded mission rules described above.
Later levels remain available for inspection and prototype interactions.

The image decoder is the existing `stb_image.h`, copied with its license
intact from ORCA. All authored application sources are `.c` and `.h` files;
SDL2 is the only external runtime library.
