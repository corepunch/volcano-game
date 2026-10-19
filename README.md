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

## Build and run

Requires a C11 compiler, `make`, `pkg-config`, and SDL2. These are already
available on this Mac.

```sh
cd ~/Developer/volcano-sdl2
make run
```

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

Log obstructions whose level object name is `fire` burn with the original
32-frame fire atlas and rising, rotating smoke particles. In level000 these are
objects 1249 and 1204, the two trees on the right; the two unmarked trees stay
unlit. Each fire has its own timing; effects pause with the game and
restart with the level. Clearing logs stops emission while existing flames and
smoke finish fading. Particle rates, lifetimes, sizes and speeds follow the
retail `conf/particles.lua` fire/smoke settings.

Birds fly left to right at a new random height on each pass, and translucent
clouds drift across the scene. Both wrap fully off screen and use the original
type-6 ambient sprites. Boats, floating wood, flooded houses, and water trees
bob and rock gently without moving their gameplay coordinates. Rooftop survivors
move with their supporting house and draw above its roof; picking them uses
their displayed position. All scenery pauses and resets with the level.

The binary format contains length-prefixed strings as well as numeric fields.
Our reader calls one of these strings `extra`; rooftop survivors store `roof`
there. They have source layer 6, flooded decor has layer 3, and ambient objects
have layer 9. The available prototype scripts discard this metadata and provide
no retail motion implementation. This restoration associates each `roof`
survivor with the flooded house underneath it (these records have no link ID),
shares the house's visual transform, and sorts its occupants after the house.

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
Custom mission supplies and goals compile to a `.lvl.rules` companion;
the `.lvl` retains the original version 301 binary layout. Custom levels load
directly without modifying the retail resources. Press R after recompiling to
reload a custom level. `make analyze-levels` inventories the original campaign;
`make test-levels` checks conversion and completes the starter through normal play.

Play **Emerald Falls**, our original forest-and-waterfall level, with
`make emerald-falls`. Its three branches contain seven people, two animals,
and a resource puzzle linking timber, food production, money production, and
the final river crossing. Six foam emitters add white surface foam at
the bottom pools, which pauses and resets with the mission. The editable source is
[`levels/emerald-falls.level.json`](levels/emerald-falls.level.json); the original
ImageGen background and its prompt are saved beside it. Regressions complete
the mission with three different production repair priorities.

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
