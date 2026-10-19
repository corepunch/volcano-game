# Creating Volcano levels

Author levels as `.level.json` files, compile them with `tools/level_source.py`,
and open the resulting `.lvl` with `--level-file`. The source format preserves
every field of Rescue Team 5's version 301 binaries, including unresolved object
types. Python 3 is required for the authoring tools; the game remains C and SDL2.

The optional `mission` section supplies goals and starting resources for this
restoration. It compiles to a separate `.lvl.rules` file. The `.lvl` itself stays
in the original binary layout: no extra header, private object type, or trailer.
Binary compatibility has been verified against our production reader and all
52 supplied binaries, not by running newly authored levels in the retail game.

## Start with the example

From the repository root:

```sh
make level-example
```

This builds and plays `levels/first-rescue.level.json`. It reuses the original
background `103` but has its own eight-object scene and two-person mission.
One worker starts at headquarters with 20 food and no wood. Clearing the tree
costs that food and yields 20 wood; repairing the bridge consumes the wood.
Both actions open a rescue route. A reachable food pickup provides optional
headroom. The regression test finishes without collecting it or adding supplies.

To create another level:

```sh
cp levels/first-rescue.level.json levels/my-level.level.json
# Edit the copy, then validate and compile:
python3 tools/level_source.py check levels/my-level.level.json
python3 tools/level_source.py compile levels/my-level.level.json build/levels/my-level.lvl
./build/volcano --level-file build/levels/my-level.lvl
```

Keep the `.lvl` and its `.lvl.rules` together when sharing a compiled mission.
Use **F1** to inspect traffic links, **R** to reload the file and its rules, and
**P** to pause. Enter or Play again restarts a completed custom mission.
Recompile after editing the source, then press R in the game.

An existing binary is also an editable starting point:

```sh
python3 tools/level_source.py decompile reference/rescue-team/level/level018.lvl build/level018.level.json
python3 tools/level_source.py compile build/level018.level.json build/level018-copy.lvl
cmp reference/rescue-team/level/level018.lvl build/level018-copy.lvl
```

Decompiling retail data does **not** invent a `mission` section. Without a
companion rules file, custom loading uses the prototype's manual escort and
obstacle/pickup interactions, with no mission economy or victory panel.
Add mission settings deliberately when adapting a retail scene. For example,
campaign mission 2 (`level018`) uses 50 food, 30 wood, 20 money, no fuel, and
a rescue goal of 3 in this restoration. Its goal includes one stranded worker.
Keep imported retail data under ignored `build/`; original assets and full
retail scene dumps are not intended for version control.

## Forest Mayday example

`make forest-mayday` plays `levels/forest-mayday.level.json`, a forest airliner
crash rescue with ten people (nine passengers and one stranded rescue worker)
and two animals. Its large aircraft and forest shadows remain part of the baked
background; only creek foam, glints, butterflies and motes are animated overlays.
`make forest-mayday-app` creates a local macOS launcher that loads the compiled
level and the existing asset directory.

The starting 40 food supports two initial jobs. The northern log yields 20 wood,
which repairs the cafe and renews the food supply. A second log protects a
passenger glade and an additional worker. The southern stone protects 40 cached
wood and 60 money; the sawmill and ranger house provide renewable wood and money.
The eastern rockfall needs 40 food plus 100 money, and the final evacuation trail
needs another 20 wood. Every person is initially gated. The regression runner
completes the mission through ordinary clicks with three production repair orders,
without adding resources. Tune costs by choosing supported barrier/building types,
then verify the full mission again after changing routes or supplies.

## Source fields

JSON uses double quotes and disallows comments and trailing commas. `_comment`
is an optional annotation on the root, mission, and individual object records.
It is ignored by compilation. `$schema` can point to
`../docs/level-source.schema.json` for editor completion. The compiler performs
additional byte-length, ID, and total-point checks beyond the JSON Schema.

| Root field | Meaning |
| --- | --- |
| `format` | Required string `volcano-level-1` |
| `version` | Binary version; omitted means `301`; other versions are rejected |
| `background` | Required resource stem, e.g. `103` loads `texture/back/103_0.jpg` and its `.alpha` |
| `max_id` | Preserved original editor counter; omitted means the highest object ID |
| `objects` | Ordered array, maximum 4096 records |
| `mission` | Optional restoration mission configuration, described below |

For original artwork beside the source, set `background` to `@FILENAME.png`
(or `.jpg`/`.jpeg`). The compiler copies the image unchanged beside the output
binary, and custom loading displays it across the 1366×768 playfield using the
image's own opacity. Use a 16:9 composition and keep important gameplay out of
the HUD corners. A filename must be local to the source directory, with no
directory components. Decompilation also copies the image beside the new source.
This convention is specific to the restoration; ordinary retail background
stems keep their existing behavior.

Each object needs a unique unsigned 32-bit `id` and a `type`. Use descriptive
type aliases from the table below or the original numeric type. Unknown numeric
types remain legal for preservation, but their behavior may be ignored. Start
new IDs at 1: zero means “no link” in reference fields. Preserve object order
when adapting a scene; it breaks some rendering ties and affects base selection.

Ordinary objects use these fields:

| Field | Default | Meaning |
| --- | --- | --- |
| `layer` | `0` | Original unsigned layer number |
| `name` | `""` | Object name, also used for fire and named groups |
| `params` | `""` | Sprite name or type-specific parameter text |
| `extra` | `""` | State/transport metadata; preserve exact strings |
| `link_id` | `0` | Type-specific reference or value, not always an object ID |
| `x`, `y` | `0` | World position in pixels, bottom-left origin |
| `rotation` | `0` | Original transform, preserved as float32 |
| `scale_x`, `scale_y` | `1` | Original scale values, preserved as float32 |
| `auxiliary` | `[x, y]` | Only type 6: initial ambient position `[x, y]` |

For type `traffic`/18, use only `id`, `type`, `layer`, and `points`, an array of
`[x, y]` pairs. There may be multiple traffic records; their point arrays are
concatenated by the runtime, with a total limit of 4096. Traffic records do not
have the ordinary name, parameter, link, or transform payload.

Coordinates and transforms must fit finite float32 values. The runtime supports
127 background bytes, 511 name bytes, and 4095 bytes each for `params` and `extra`;
strings cannot contain NUL. New text is UTF-8. Decompilation uses escaped
surrogates for any non-UTF-8 retail bytes, preserving them on recompilation.
The runtime positions gameplay using `x,y`; most original rotation, scale, and
layer values are retained as data rather than applied as arbitrary transforms.
Do not use scale to resize obstacle collision or rotate a road network.
Sprite positions use the original atlas pivots, which differ from the visible
image centers. When centering a building in a painted clearing, check the
rendered damaged and repaired artwork, then keep its approach and bonus pickup
clear of the building footprint.

For a frame with width/height `w,h` and authored pivot `dx,dy`, its visible
rectangle center in world coordinates is `(x + w/2 - dx, y + h/2 - dy)`.
To place that center at a clearing's measured `cx,cy`, author
`x = cx - w/2 + dx` and `y = cy - h/2 + dy`. Read the frame from the original
atlas declaration rather than assuming a center pivot. For example, damaged
`burger0_br` is 94×85 with pivot (45,31), so its visible center is 2 pixels right
and 11.5 pixels above the level position. Repaired art can have different trim
bounds while retaining the same ground attachment; inspect both states before
calling the placement finished.

## Mission settings

Supply every field if `mission` is present:

```json
"mission": {
  "food": 20,
  "wood": 0,
  "money": 0,
  "fuel": 0,
  "rescue_goal": 2,
  "basin_goal": 0,
  "allow_cafe": true
}
```

Quantities are integers from 0 to 1000000. `allow_cafe` is a boolean. The rescue
goal counts civilians **plus recruited workers delivered home**, excluding the
starting crew. Every authored animal is an additional objective automatically.
Set `basin_goal` to the number of required valve activations; the runtime counts
each completed valve when this goal is nonzero, rather than deriving unique
basins from the water tiles. There are no custom deadlines, star thresholds,
upgrades, or campaign progression in this format.

Place at least one type-104 worker within **strictly less than 80 pixels** of
the first type-110 base. Other on-foot workers start stranded and join the crew
after their route home opens; include them in `rescue_goal`. Custom missions
use the first base as everyone's destination. Keep to one headquarters until
multiple-base mission behavior is implemented. Runtime limits are 64 worker
records, 64 civilian evacuees, and 16 vehicles.

The rules file is UTF-8 `key=value` text with `version=1` and the seven mission
fields, one per line; `allow_cafe` becomes 0 or 1. Compile it from JSON instead
of maintaining it separately. Decompilation imports an adjacent rules file.
The compiler refuses to compile a source without `mission` over an output that
still has a rules file; remove the obsolete sidecar explicitly first.

## Objects suitable for new missions

| Type / alias | Parameters and behavior |
| --- | --- |
| 2 `scenery` | `params="anim=water0"`, `tree=decor2_tree1;…`, or an existing scenery animation; cosmetic |
| 3 `marker` | Approach/docking marker; see relationships below |
| 4 `background` | Retail background object; the restoration loads the root `background` field instead |
| 5 `effect` | Placed map visual: sprite, foam, sparkles, insects, or motes; other retail weather types remain deferred |
| 6 `ambient` | E.g. `bird00_idle;bird00_action;bird00_sh;128`; `auxiliary` sets initial position |
| 18 `traffic` | Implicit movement graph |
| 101 `building` | `sawmill`, `burger`, `house`, `derrick`; `extra="0;0"` means working, other values mean damaged |
| 102 `barrier` | `tree00`, `stone00`, `stone_big00`, `bridge_wood00`, `bridge_wood01`, `bridge_auto00`, `bridge_auto01`, `broken_road0`; also linked `fog_high` |
| 104 `worker` | Starting crew near HQ, otherwise recruit; `extra="offroad"` requires ATV recruitment |
| 105 `animal` | `params="cat"` or `"dog"`; on-foot capture and return home |
| 108 `resource` | `food1/2`, `wood1/2`, `money1/2`, `gas1/2`; `boat_food` for fishing |
| 109 `civilian` | Automatically evacuates over an open on-foot route |
| 110 `base` | Worker headquarters; no parameters required |
| 113 `boat` | Named vehicle with a matching pier marker |
| 114 `pier` | `params="seaport"`; working only when `extra="0;0"` |
| 128 `valve` | Named gate controller, optional linked workpoint marker |
| 146 `lot` | `params="build_place"`; click to construct a production building |
| 151 `pollution` | Named basin water tiles, linked by valve group |
| 152 `offroad_region` | Center `x,y`; `link_id` is its radius in pixels |
| 153 `atv` | Named vehicle with a matching garage marker |
| 154 `garage` | `params="garage"`; `extra="0;0"` for operational service |

Helicopters, doctors, police, thieves, railway operations, medicine deliveries,
generators, bonfires, runway lights, and timed boosts appear in the original
scenes but their mission behavior is not restored. Several draw cosmetically;
others are ignored. `heli`, `boat`, `roof`, and `sick` rescue tags do not add
their retail transport/healing requirements to custom missions. Rooftop visual
attachment exists, but an on-foot route can still evacuate that survivor.
Do not make progression depend on these features. Preserve their records when
round-tripping retail scenes. The generated catalog lists all observed types
and parameter strings; type names for unresolved controllers are interpretations.

## Routes and obstacle placement

The current runtime connects **every pair of traffic points at distance less
than 50 pixels**, with no explicit edge list. It finds a shortest route by
distance. Use spacing around 30–42 pixels along a road. A gap of exactly 50
does not connect. Keep parallel roads and water lanes at least 50 pixels apart
unless a crossing is intended: close points create shortcuts regardless of
the background artwork. Point order does not define edges.

An uncleared barrier blocks any route segment passing **less than 20 pixels**
from its center. Put it across a narrow throat in the graph so there is no
alternate nearby edge around it. The worker may approach the barrier being
cleared because that target is excluded from its own collision check. There
must still be a clear approach past every other barrier. Avoid stacking
obstacles so closely that each blocks access to the other.

Actors and destinations attach to their nearest traffic point. The first and
last approach segments are collision checked too. Place pickups, civilians,
headquarters, and workpoints near the intended road, especially at forks.
Sprites and terrain do not create collision; movement does not read a terrain
mask. Separate water networks intentionally. F1 shows potential graph links,
including those currently obstructed, so inspect both artwork and actual play.

`check` reports disconnected networks and distant approaches as warnings,
because separate boating networks and long authored approaches can be valid.
It also warns about missing crew/base, mismatched goals, unsupported mission
types, and dangling known references. It does not prove solvability, validate
animation existence, simulate the economy, or detect every blocking shortcut.
Load and complete the level to verify those properties.

## Layered map visuals

Keep painted scenery baked when its lighting and overlapping shadows matter,
as with Emerald Falls' forest. Place moving accents as independent type-5 map
records. Optional sprites suit isolated props that benefit from animation.
The runtime stores all of them in one `MapScene`, with a shared visual list
and game clock. Visuals never block routes or count as gameplay jobs.

| Parameter prefix | Role and useful parameters |
| --- | --- |
| `sprite` | Local PNG/JPEG sprite or atlas region; `image`, `rect`, `pivot`, `wind`, `speed` |
| `foam` | White foam around the exact authored `x,y`; `width`, `rate` |
| `sparkles` | Brief surface glints inside an ellipse; `width`, `height`, `rate`, `speed`, `opacity` |
| `insects` | Local looping flight with animated wing frames; `animation=butterfly`, `width`, `height`, `count`, `speed` |
| `motes` | Small drifting light accents; `width`, `height`, `count`, `speed`, `opacity` |

For example, a tree sprite record can use:

```json
{
  "type": "effect",
  "id": 100,
  "name": "spruce",
  "params": "sprite;image=@emerald-foliage.png;rect=0,0,382,430;pivot=.51,.968;wind=2.5;speed=.8;stage=world",
  "x": 300,
  "y": 520,
  "scale_x": 0.25,
  "scale_y": 0.25
}
```

`rect` is the source rectangle in **top-left image pixels**, as `x,y,width,height`.
`pivot` is the root/attachment point normalized to that rectangle, as `x,y`
between zero and one. Level `x,y` use the usual bottom-left world coordinates.
Object scale and rotation transform the sprite around its pivot. `wind` is
canopy bend in world pixels; the lower trunk stays fixed. A sprite with zero
wind can also represent a static prop in the same system. The compiler copies
all referenced local atlas images beside the binary, and decompilation copies
them beside the new source. Regions are read directly from the authored atlas.

`stage=ground` draws over terrain and under actors. `stage=world` shares the
actor depth sort using the visual's ground position. `stage=overlay` draws over
the world and under the HUD. Sprites default to world, foam and sparkles to
ground, and insects/motes to overlay. Keep large trees clear of clickable jobs
and trail centers; use small plants at meadow edges and riverbank glints only
inside water. Prefer several narrow sparkle patches following the river to a
large patch crossing its banks.

All kinds share `speed` (0.05–6) and `opacity` (0–1). Region width/height may be
1–600 pixels, emission rate 0.1–180, insect/mote count 1–32, wind 0–16 pixels,
and sprite scales 0.02–5. Atlas regions must be integral pixels wholly inside
the image. The scene supports 1024 visuals within the level's 4096-record limit.
Every kind pauses and restarts with the game. Insects use the original Rescue
Team `butterfly` animation unless another existing animation is specified.

Emerald Falls uses `emerald-falls.png` as its baked background, including trees,
plants and their painted shadows. Ten water-glint patches, nine butterfly groups,
four mote regions and six foam patches remain independent editable map objects.

## Object relationships

**Waterfall particles.** In a custom level, a type-5 `effect` object with
`params="foam;width=32;rate=26"` centers white foam directly at `x,y`. Use this
for precise placement on painted impact points; particles stay close to the
specified center as they move. Emerald Falls uses six of these centered emitters.
The older
`params="waterfall;width=32;height=72;rate=26;drift=0"` locates a waterfall at `x,y`.
Height extends downward to its receiving pool, width spreads the foam, and drift
offsets the foot horizontally, all in pixels. Rate controls white foam flecks per
second. Flecks and open foam rings drift gently downstream at the foot; there
are no falling-water particles or aerial mist. Legacy height is 16–220 pixels
and drift is −50–50; other limits follow the shared map visual parameters above.
Default width/height/rate/drift values are 32/72/26/0. Effects draw over the background and behind
actors and bridges. Foam is drawn as white geometry with feathered alpha edges,
follows game time, pauses with the level, and restarts deterministically.

**Production outputs.** A type-108 bonus pickup's `link_id` points to a type-101
building or type-146 lot. The pickup stays hidden until its owner operates.
Use one output per owner, positioned beside the road. The original campaign-2
scene (`level018`) has food pickup 657 linked to cafe 646. The runtime turns an
existing output into that building's correct supply type when it replenishes.
It does not invent an output if none was authored. Demolition retains the
relationship while releasing the construction lot.

**Burning logs.** For a tree barrier use `params="tree00"` and `name="fire"`
to add flame/smoke. An unmarked tree stays unlit. This is distinct from a barrier
whose `params` is `fire`, which uses road-fire artwork and has deferred specialist
rules. Avoid making road-fire or dynamite barriers the main puzzle in a new mission.

**Valve groups.** A valve's `name`, such as `tap0`, matches the `name` of its
fog barriers or pollution tiles. These use names rather than link IDs to group
the effect. For example, valve 819 and fog 847 in `level013` both use `tap0`.
Completing the valve clears its grouped effect; fog prevents dispatch within
105 pixels while active. Give the valve a clear on-foot workpoint outside its
own fog. An unnamed type-3 marker with `link_id=819` supplies that workpoint;
the original example is marker 1045 at approximately `(1102,404)`.

**ATV and garage.** A type-153 vehicle's `name` matches a **named** type-3 marker,
and that marker's `link_id` references its garage. The marker is the vehicle's
home position. In `level012`, ATV 798 has name `1`; marker 928 also has name `1`
and points to garage 662. A stranded worker with `extra="offroad"` must fall
within a type-152 region's radius (`link_id`, not an object reference). The ATV
travels via that region's center. Start with a working garage, or provide its
unnamed repair approach marker and 40 wood. This transport currently supports
stranded workers, not arbitrary civilian offroad rescues.

**Boats and piers.** Use the same named-marker relationship for type-113 boats
and type-114 piers. In `level011`, boat 859 has name `1`; marker 861 has name `1`
and references pier 858. An additional **unnamed** marker 860 references 858
and provides the worker's land approach for repairs. Put the named marker on
its water network and the unnamed marker on land. Fishing resources use
`params="boat_food"`; their `name` can be `food2` for a 20-food catch (otherwise
10). Boats return their catch before crediting food.

Piers and fishing spots take their pollution gate from the nearest **named**
type-151 water tile. Match that tile's name to a valve. Use well-separated
basins and inspect which tile is nearest. Some pollution `link_id` values are
tile metadata, not references; preserve them in imported levels. Boat routes
use the same implicit graph, so keep different basins disconnected even after
their valves are open.

## Balance and progression

These are the restoration's explicit balance rules from `src/game_rules.c` and
`src/game_jobs.c`, not
recovered retail constants. The binaries do not carry these prices or intervals.

Hover cards contain up to three centered rows: the object's name, `Required`,
and `Income`. Amounts use the original resource icons. Omit rows that do not
apply, retain the full cost while queued, and do not add strategy hints.
Income is the actual pickup/job reward; production buildings also show their
income interval on the same row.

| Action | Cost | Result |
| --- | --- | --- |
| Clear tree | 20 food | 20 wood after work completes |
| Clear small stone | 20 food | Opens route |
| Clear large stone | 40 food and 100 money | Opens route |
| Repair wooden bridge or broken road | 20 wood | Opens route |
| Repair large auto bridge | 20 food and 40 wood | Opens route |
| Construct sawmill / cafe / house / gas station | 40 / 40 / 60 / 80 wood | Wood / food / money / fuel production |
| Repair those buildings | Half their construction cost | Restores production |
| Repair pier or garage | 40 wood | Enables service |
| Rescue animal | 10 food | Counts when returned home |
| Operate valve | 20 money | Clears named effect and increments basin progress when required |
| ATV or fishing trip | 10 fuel | Recruitment or food delivery |

Each working production building pays 5 units every 5 seconds and exposes its
authored 10-unit bonus pickup every 10 seconds when no earlier bonus is
outstanding. A worker must bring pickup cargo home before it enters inventory.
Small and large ordinary supply piles deliver 10 and 20 units. Costs are reserved
when a job is dispatched. A stranded on-foot civilian returns automatically
once the full route opens; there is no rescue fee. Animals require a click.

Build the puzzle as dependencies, then place its artwork:

1. Choose a rescue goal and a clear home route for every objective. Put a
   starting worker near HQ and verify a useful first action is possible.
2. Put one barrier between HQ and a rescue branch. Supply the exact cost on
   the reachable side, or make its cost obtainable through an earlier action.
3. Add a second branch whose reward funds the next step: food unlocks a tree,
   its wood restores a sawmill or bridge, money from a house funds a valve or
   large boulder, fuel enables a vehicle trip.
4. Check the reachable inventory **before each mandatory action**, not just
   the total resources on the map. Never lock the only food source behind an
   unaffordable food barrier, or the only fuel source behind the ATV that needs it.
5. Add choices with visible consequences: a short expensive route versus a
   longer cheap one, an extra worker behind a branch, or a production repair
   competing with a bridge for wood. Keep the main route recoverable after an
   optional purchase, or make that purchase's income cover the lost budget.
6. Add scenery last. Keep actionable objects distinct and out of the top-left
   and top-right HUD regions. Use original sprite names from the catalog.
7. Finish from a fresh restart with no injected resources. Recheck after moving
   points, because an accidental connection can bypass a barrier or a basin gate.

## Patterns in the supplied campaign

The map's type-135 nodes use `name` for the mission's background code and
`extra` for its prerequisite's background code. Following the empty prerequisite
recovers all 50 missions. File numbering is not play order:

| Campaign position | Binary | Scene objects / traffic points | Pattern to study |
| --- | --- | --- | --- |
| 1 | `level015` | 128 / 61 | Supply-to-tree-to-bridge sequence, three civilians; special tutorial crew placement |
| 2 | `level018` | 150 / 64 | Repairable production, linked output pickups, two civilians plus one recruit |
| 3 | `level019` | 140 / 96 | Construction lot, income needed for large stone, house-to-cafe replacement |
| 4 | `level012` | 123 / 141 | Garage, ATV marker, radius region, five transported recruits and eight civilians |
| 5 | `level013` | 187 / 143 | Seven fog patches sharing one valve, six people and sixteen animals |
| 6 | `level011` | 227 / 112 | Two pollution groups and piers; graph components of 85, 14, and 13 points |

Campaign levels 1–5 each have one potential traffic component before obstacles.
Level 6 separates land from the two boat networks. The repeated design pattern
is an initially connected potential network partitioned by paid work, with
resource rewards or production opening further partitions. This is a design
interpretation of the scene data and working restoration, not recovered retail AI.
The first mission's runtime deliberately relocates both workers to HQ, even
though its binary places them far away. Custom missions use the distance-based
recruit rule, so relocate starting workers when adapting that particular scene.

The rest of the campaign introduces additional service records gradually:

| First campaign position containing records | New types |
| --- | --- |
| 7 | Generator |
| 8 | Doctor, hospital, boost |
| 10 | Air delivery actor and delivery location |
| 11 | Thief, road entry, police officer and police station |
| 12 | Helicopter and helipad |
| 13 | Firefighter and fire station |
| 15 | Railway station and train |
| 17 | Medicine delivery |
| 23 | Bonfire |
| 26 | Runway light |

Presence is not proof that a mechanic is enabled: mission 1 already contains
ATV/garage records, although its restored tutorial does not activate them.
These later services suggest a useful progression for future designs: introduce
one dependency, combine it with familiar resource and route constraints, then
increase the number of branches. Reusing records alone will not implement those
services in this engine. The analysis reports first appearances across the whole
campaign so they can be studied without confusing source-file numbering with
mission order.

Generate the full local inventory and check binary preservation with:

```sh
make analyze-levels
# Report: build/level-analysis.json
make test-levels
make test
```

The analysis records every observed type, all distinct parameter strings and
their frequencies, sample records with original IDs, campaign order, point
counts, graph components, and first type appearances. All 52 files must round-trip byte for byte.
The tests also pass recompiled files through the C decoder, reject malformed
sources/truncated binaries, preserve type-6/type-18 payloads, and complete the
starter mission through the actual click interface with its exact budget.

## Binary layout reference

All numeric fields are little-endian. `u32` means four unsigned bytes, `f32`
means IEEE-754 binary32, and `string` means a two-byte unsigned byte length
followed by exactly that many bytes, without a terminator.

```text
u32 version (=301)
string background
u32 max_id
u32 object_count
repeat object_count:
    u32 type
    u32 id
    if type == 18:
        u32 point_count
        repeat point_count: f32 x, f32 y
        u32 layer
    else:
        u32 layer
        string name
        string params
        string extra
        u32 link_id
        f32 x, y, rotation, scale_x, scale_y
        if type == 6: f32 auxiliary_x, auxiliary_y
EOF
```

No padding or trailing data is allowed. The source compiler preserves object
and point order, string contents, unsigned values, and finite float32 values,
including signed zero. The runtime ignores the editor counter for gameplay.
The production decoder is `src/level.c`. `src/game_rules.c` defines balance;
`src/game_spawn.c` resolves retail types and links; `src/game_jobs.c` handles
reservation and completion; `src/game_navigation.c` and `src/game_sim.c` handle
routes and movement. Campaign prerequisites are read by `src/campaign.c`.
These files are the primary references for the rules above.
