# Creating Volcano missions

A mission is a folder under `missions/` with a `mission.xml` file. The game
reads that XML directly: there is no compile step, sidecar or binary output.
Edit the file, press **R** in the running game, and the mission reloads.
Python 3 is only needed for the checking and layout tools; the game itself
remains C and SDL2.

This page explains the mission format and how to design with it. The complete
attribute reference for archetypes, rulesets, campaign settings and sprite
sheets is [content.md](content.md). The loader in
[`src/content/mission.c`](../src/content/mission.c) is the authority; its
header comment summarises the grammar.

For the complete artwork process, see [Creating and revising backgrounds](creating-backgrounds.md).
It covers the tools used for Ancient Echoes and Twisted Tracks, their original
and intermediate images, exact prompt sequence, measured layout and
verification commands.

## Start with the example

From the repository root:

```sh
make run MISSION=first-rescue
```

This builds the game and plays `missions/first-rescue/mission.xml`. It reuses
the original background `103` but has its own seven-object scene, one road
network and a two-person mission. One worker starts at headquarters with 20 food
and no wood. Clearing the tree costs that food and yields 20 wood; repairing the
bridge consumes the wood. Both actions open a rescue route. A reachable food
pickup provides optional headroom. The regression test finishes without
collecting it or adding supplies.

The same mission, retitled, is the skill's starter at
[`skills/rescue-team-levels/assets/starter.mission.xml`](../skills/rescue-team-levels/assets/starter.mission.xml).
To create another mission, copy either one into a new folder:

```sh
mkdir -p missions/my-mission
cp missions/first-rescue/mission.xml missions/my-mission/mission.xml
# Edit the copy, then validate and play it:
python3 tools/mission.py check missions/my-mission/mission.xml
make run MISSION=my-mission
```

`make run` accepts any folder name under `missions/`. Without Make, run
`./build/volcano --assets reference/rescue-team --mission missions/my-mission/mission.xml`.
Add `--pack FOLDER` (repeatable) to mount extra asset packs and `--hd` to use
`background@2x.png` where a mission provides one.

Use **F1** to inspect traffic links and worker paths, **R** to reload the
mission file and restart, and **P** or Space to pause. Enter or Play again
restarts a completed mission.

`tools/mission.py check` reports errors the game would refuse to load and
design warnings; see [Routes and obstacle placement](#routes-and-obstacle-placement).
It accepts several files, e.g. `python3 tools/mission.py check missions/*/mission.xml`.

### Mission folder

| File | Purpose |
| --- | --- |
| `mission.xml` | Required: settings, objects, roads, effects |
| `background.png` | Painted playfield, stretched over 1366×768; use a 16:9 image |
| `background@2x.png` | Optional double-resolution background, used only with `--hd` |
| `*.png` + `sprites.xml` | Optional mission artwork and its sprite sheet |
| `background.prompt.txt`, `*.prompt.txt` | Exact image-generation prompts (provenance) |
| `design.txt` | Optional design notes: measured layout, references, decisions |

Concept art, rejected images and earlier background stages do not belong in the
mission folder. Keep them under [`art-source/`](../art-source) (for example
`art-source/background-history/` and `art-source/concepts/`). Screenshots,
app bundles and other generated files belong under ignored `build/`.

### Adapting an original scene

The game imports original Rescue Team scenes at runtime
(`--campaign N` or `--level INDEX`); there is no decompile-to-editable-source
step. To base a new mission on a retail scene, dump it for study and author a
new `mission.xml` from what you learn:

```sh
python3 tools/retail_levels.py dump reference/rescue-team/level/level018.lvl build/level018.xml
```

The dump lists every original record (type, name, params, extra, link and
transform) as XML. Translate the parts you need using the
[retail mapping](#retail-binary-reference) below. For example, campaign mission
2 (`level018`) uses 50 food, 30 wood, 20 money, no fuel and a rescue goal of 3
in this restoration (see `content/campaign.xml`). Its goal includes one stranded
worker. Keep retail dumps under ignored `build/`; original assets and full retail
scene dumps are not intended for version control.

## The project's missions

| Folder | Theme | Regression |
| --- | --- | --- |
| `first-rescue` | Two-route starter on retail background 103 | default mode |
| `emerald-falls` | Forest and waterfalls; three-branch economy | `--emerald` |
| `forest-mayday` | Airliner crash in a forest | `--mayday` |
| `ancient-echoes` | Collapsed temple tourist site | `--ancient` |
| `twisted-tracks` | Wrecked roller coaster with themed debris | `--coaster`, plus `tests/coaster_main.c` |
| `library-rescue` | Collapsed civic library with field stations | `--library-rescue` |

`make test` plays all six through the real UI on a generated placeholder pack
(`tests/missions_main.c`, `tests/coaster_main.c`). `make test-retail
ASSETS=reference/rescue-team` repeats them on the retail art. `make app
APP_MISSION=NAME` builds a self-contained macOS bundle that opens one mission.

### Forest Mayday

`make run MISSION=forest-mayday` plays a forest airliner crash rescue with ten
people (nine passengers and one stranded rescue worker) and two animals. Its
large aircraft and forest shadows remain part of the baked background; only
creek foam, glints, butterflies and motes are animated effects. `make hd-pack`
followed by `make run-hd MISSION=forest-mayday` plays it with the HD statics
pack and its `background@2x.png`.

The starting 40 food supports two initial jobs. The northern log yields 20 wood,
which repairs the cafe and renews the food supply. A second log protects a
passenger glade and an additional worker. The southern stone protects 40 cached
wood and 60 money; the sawmill and ranger house provide renewable wood and money.
The eastern rockfall (a `boulder`) needs 40 food plus 100 money, and the final
evacuation trail (a `road`) needs another 20 wood. Every person is initially
gated. The `--mayday` regression completes the mission through ordinary clicks
with three production repair orders, without adding resources. Tune costs by
choosing archetypes (or extending them), then verify the full mission again
after changing routes or supplies.

### Ancient Echoes

`make run MISSION=ancient-echoes` plays the collapsed ancient temple
tourist-site mission. Rescue eight tourists and a stranded rescue worker, plus a
cat and dog. The olive-terrace trail, service court and two temple approaches
have separate gates. Clearing the first log and service-court stone spends all
40 starting food; the log and three timber caches fund the cafe, sawmill and
visitor-centre house. The cafe restores food and the house earns the 100 money
needed at the main rockfall. The final broken paving needs 20 wood. All scenery
and ancient masonry remain in the painted background; only authored jobs block
movement. The enlarged ruined complex fills the centre and eastern side of the
screen. The formal visitor approach gives way to rough rescue tracks through
collapsed colonnades, rubble-lined service access and fragments of mosaic
courtyard.

The `--ancient` regression checks every initial rescue gate, a zero-food
opening and automatic queue recovery, three production repair orders, all nine
people and both animals, pause and restart. The background and its exact
generation prompt are `missions/ancient-echoes/background.png` and
`background.prompt.txt`; the first version is in `art-source/background-history/`.

### Twisted Tracks

`make run MISSION=twisted-tracks` plays the collapsed amusement-park roller
coaster mission. Ground-level maintenance roads extend through the centre of the
reorganized wreck, branching to visitors in an upper interior pocket, inside the
broken loop and beside the blue train. Nine visitors and a stranded technician
count toward ten people; a cat and dog have their own rescue objective.

Ten removable jobs are mission archetypes that extend ordinary obstacles and use
eight authored debris sprites: red track, yellow supports, bent fencing, cables,
a detached car, concrete footings, wheels and timber maintenance panels. The
sprites are cut from `debris.png` by `missions/twisted-tracks/sprites.xml`. The
panels extend `tree` and yield 20 timber; the derailed car extends `boulder`;
the other eight extend `stone`. Two of those reuse a sprite under a different
title. These are ordinary clearing tasks, not new traversal mechanics. Three
repair caches and renewable cafe, sawmill and park-office production support
recovery after spending all 40 starting food. The expensive car needs 40 food
and 100 money and lies on the common train-pocket spur, so the service-road
loop cannot bypass it. Large static rails, towers and the main train remain
painted scenery; cleared jobs reveal clean road underneath.

The `--coaster` regression checks every initial rescue gate, zero-food recovery,
three producer repair orders, all ten people and both animals, and
pause/restart. `tests/coaster_main.c` checks that all ten blockers are mission
archetypes with their own titles and eight distinct atlas sprites, that each is
clickable at its centre, that every job clears completely, and that the car
reserves 40 food and 100 money exactly once. Set `VOLCANO_TEST_SHOTS=1` to keep
start, repaired and cleared renders in `build/`.

The production background is `missions/twisted-tracks/background.png`; the
transparent debris atlas is `debris.png`. Both have exact built-in ImageGen
prompts saved alongside, and `design.txt` records the layout. The original
background, concepts and the atlas's earlier region JSON are preserved under
`art-source/`.

### Library Rescue

`make run MISSION=library-rescue` plays an original fictional collapsed civic
library with teal roofing, ochre walls and tiled interior corridors. The
background uses a fixed parallel three-quarter projection and human-scale
doors/wall fragments. Real collapse photographs informed roof/slab failure,
without copying their buildings, footprints or camera views. The earlier
real-site recreation was rejected and is retained only as authoring history
under `art-source/background-history/ground-zero-rejected.*`.

The mission mounts the project's custom blue-uniform workers and male/female
civilian atlases with `<pack path="../../assets/kenney-characters"/>`. That pack
replaces the retail worker and civilian sprites by name for this mission only;
nothing is copied or installed.

The original `base0` blue headquarters is used unchanged. Nine civilians, one
stranded worker and two animals occupy the three interior search routes and an
eastern courtyard. The resource puzzle and ordinary worker/queue rules are those
of the other disaster missions: two starting workers, 40 food, repairable
food/timber/money producers and six paid route jobs. The stations are mission
archetypes: `field-kitchen` extends `cafe`, `salvage-supply-station` extends
`sawmill` and `logistics-station` extends `house`, each with its own working
and damaged sprite. The route jobs extend `tree`, `stone`, `boulder` and `road`.

The `--library-rescue` regression checks every initial rescue gate, the
depleted-food opening, automatic queue recovery, three repair orders, the
original HQ artwork, station click targets, custom worker/civilian atlas
selection, complete debris removal, pause and restart.

The folder holds the background and its prompt, the prop atlas `props.png` with
its original prompt, `sprites.xml`, and `design.txt`, which records reference
photos, scale constraints and the measured layout. The atlas's first HQ cell is
deliberately unused; the other authored props and station states are used
directly. The background was generated from the two real photographs only, with
no generated-image input.

## The mission file

```xml
<?xml version="1.0" encoding="UTF-8"?>
<mission format="volcano-mission-1" title="Library Rescue" background="background.png">
  <settings rules="rescue" food="40" people="10"/>
  <pack path="../../assets/kenney-characters"/>
  <sprites path="sprites.xml"/>
  <archetype id="field-kitchen" extends="cafe" title="Field kitchen"
             sprite="library-rescue/field-kitchen" damaged="library-rescue/field-kitchen-damaged"/>
  <traffic>210,248 237.5,245.5 265,243 …</traffic>
  <object id="2" type="hq" x="190" y="128"/>
  <object id="3" type="worker" x="190" y="128"/>
  <object id="30" type="field-kitchen" x="245" y="378"/>
  <object id="31" type="food-small" x="320" y="323" producer="30"/>
  <object id="50" type="civilian" x="300" y="608"/>
  <effect kind="motes" x="690" y="438" width="130" height="70" speed=".25" opacity=".16" count="5"/>
</mission>
```

The loader is strict: unknown elements or attributes, duplicate ids, unknown
types and out-of-range values are errors naming the file and line. Comments are
allowed anywhere; the project's missions keep a one-line summary at the top.

| Element | Count | Meaning |
| --- | --- | --- |
| `<mission>` | root | `format="volcano-mission-1"`, `title`, and exactly one of `background` or `retail-background` |
| `<settings>` | exactly one | Ruleset, starting stock and goals |
| `<pack path>` | 0–8 | Asset folder mounted while this mission runs |
| `<sprites path>` | 0–8 | Mission sprite sheet |
| `<archetype>` | any | Mission-specific object kind |
| `<traffic>` | one or more | Road points; all of them form one graph |
| `<object>` | any | A placed archetype |
| `<effect>` | 0–1024 | Decorative animated map effect |

`background` is an image path relative to the mission file. The game stretches
it over the 1366×768 playfield using the image's own opacity. Use a 16:9
composition and keep important gameplay out of the HUD corners. With `--hd`, a
`NAME@2x.EXT` file beside it is used instead when present.
`retail-background="103"` instead loads `texture/back/103_0.jpg` and its alpha
from the retail assets.

All paths (`background`, `pack`, `sprites`) are relative to the mission file
and must not be absolute. Coordinates are world units with a **bottom-left**
origin on the 1366×768 playfield. A mission may hold up to 4096 objects and
4096 traffic points.

## Mission settings

```xml
<settings rules="rescue" food="40" wood="0" money="0" fuel="0" people="7" basins="0"/>
```

| Attribute | Default | Meaning |
| --- | --- | --- |
| `rules` | required | Ruleset from `content/archetypes.xml`; missions use `rescue` |
| `food`, `wood`, `money`, `fuel` | 0 | Starting stock, 0–1000000 |
| `people` | 0 | Civilians **plus recruited workers delivered home** |
| `basins` | 0 | Completed valve activations required |
| `crew-at-hq` | `false` | Start every on-foot worker inside headquarters |
| `offroad` | `true` | Only affects retail imports (campaign 1–3 leave ATV/garage records inactive) |
| `buildable` | all four | Space-separated producers offered by lots, e.g. `"sawmill house gas-station"` |

The people goal excludes the starting crew. Every authored animal is an
additional objective automatically. The basin goal counts each completed valve,
rather than deriving unique basins from the water tiles. There are no custom
deadlines, star thresholds or upgrades. Campaign positions 1–50 take the same
attributes from `content/campaign.xml`.

Place at least one worker within **strictly less than 80 units** of the first
`hq`. Other on-foot workers start stranded and join the crew after their route
home opens; include them in `people`. With `crew-at-hq="true"` every on-foot
worker starts inside headquarters instead and none count as recruits. Every
mission uses the first headquarters as everyone's destination; keep to one
until multiple-base mission behaviour is implemented.

## Objects

```xml
<object id="20" type="tree" x="455" y="373"/>
<object id="5" type="bridge" x="426" y="196" sprite="bridge_wood01"/>
<object id="61" type="dog" x="720" y="558" variant="1"/>
```

Each object needs a positive, unique integer `id` and a `type`, which names an
archetype from `content/archetypes.xml` or one declared by the mission.
Relationship attributes refer to other objects by id. The project's missions
use stable id ranges (see [Preserve the mission economy](creating-backgrounds.md#preserve-the-mission-economy-during-art-changes))
so regressions can find their objects.

| Attribute | Default | Meaning |
| --- | --- | --- |
| `x`, `y` | 0 | Ground point in world units, bottom-left origin |
| `layer` | 6 for workers, civilians, animals and vehicles; otherwise 5 | Draw order |
| `sprite` | archetype's | Replace the sprite for this placement (decor, alternate bridge art) |
| `variant` | 0 | Art variant for animals: 0–2 selects `cat0`/`cat1`/`cat2` (or `dog…`) |
| `operational` | `false` | Producer or service starts working |
| `burning` | `false` | Burnable obstacle starts on fire |
| `floating` | `false` | Bobs gently, for objects on water |
| `roof` | `false` | Civilian waiting on a roof (pair with `support`) |
| `transport` | `foot` | `atv`: recruit reached by ATV; `boat`: fishing spot reached by boat |
| `producer`, `gate`, `service`, `support` | none | Relationships, described [below](#object-relationships) |
| `workpoint`, `home`, `corridor` | none | Authored points, described below |
| `speed` | archetype's | Travel speed of ambient birds and clouds |

World objects draw by ascending `layer`, then descending world `y` within each
layer. Use the defaults for ordinary gameplay and higher layers for foreground
scenery.

### Archetypes to place

Costs and rewards below are the shared defaults; [content.md](content.md) lists
every attribute, and `content/archetypes.xml` holds the numbers.

| Type | Behaviour |
| --- | --- |
| `hq` | Worker headquarters, the destination for every rescue and delivery |
| `worker` | Starting crew near HQ, otherwise a stranded recruit; `transport="atv"` needs an ATV trip |
| `boat-worker` | Crew member that only travels the water network |
| `civilian` | Walks home automatically once an on-foot route opens |
| `cat`, `dog` | Click to capture (10 food) and lead home; `variant` 0–2 |
| `tree` | Fallen tree: 20 food, yields 20 wood; can be `burning` |
| `log` | Same as `tree` but cannot burn |
| `stone` | Small rock: 20 food |
| `boulder` | Large rock: 40 food and 100 money |
| `blast-rock` | Needs dynamite, which is not restored; do not make progress depend on it |
| `bridge`, `auto-bridge` | Broken bridge: 20 wood, or 20 food and 40 wood; stays visible once repaired |
| `road` | Damaged road: 20 wood |
| `debris` | Generic obstruction with no art or cost; base for mission archetypes |
| `fog` | Toxic fog cleared by its valve (`gate`) |
| `fire-hazard` | Road fire with deferred specialist rules; avoid as a main puzzle |
| `food-small`/`-large`, `wood-…`, `money-…`, `fuel-…` | Supplies of 10/20 units, credited when carried home |
| `floating-wood` | 10 wood collected by a boat worker |
| `fishing-spot`, `fishing-spot-large` | 10/20 food caught by the fishing boat (`transport="boat"`) |
| `sawmill`, `cafe`, `house`, `gas-station` | Producers of wood/food/money/fuel; damaged unless `operational` |
| `lot` | Construction site; opens the build menu |
| `pier`, `garage` | Services for the fishing boat and ATV; repair 40 wood |
| `valve` | 20 money; clears its gated group and counts toward `basins` |
| `atv`, `fishing-boat` | Vehicles; need `service` and `home` |
| `decor` | Scenery with a per-placement `sprite`; drawn in world depth |
| `water`, `polluted-water` | Animated water tile; polluted water clears with its valve |
| `bird`, `cloud` | Ambient drifting scenery |

Helicopters, doctors, police, thieves, railway operations, medicine deliveries,
generators, bonfires, runway lights and timed boosts appear in the original
scenes, but their mission behaviour is not restored and they have no
archetypes. Rooftop visual attachment exists, but an on-foot route can still
evacuate that survivor. Do not make progression depend on these features.

## Routes and obstacle placement

```xml
<traffic>326,371 314,342 310,304 353,294 391,288</traffic>
```

A `<traffic>` element holds space-separated `x,y` points. A mission can have
several; all their points form one graph. The runtime connects **every pair of
points at distance less than 50 units**, with no explicit edge list, and finds a
shortest route by distance. Use spacing around 28–42 units along a road. A gap
of exactly 50 does not connect. Keep parallel roads and water lanes at least 50
units apart unless a crossing is intended: close points create shortcuts
regardless of the background artwork. Point order does not define edges.

A live obstacle blocks any route segment passing **less than 20 units** from
its centre. Put it across a narrow throat in the graph so there is no alternate
nearby edge around it. The obstacle being cleared is excluded from its own
collision check. There must still be a clear approach past every other
obstacle. Avoid stacking obstacles so closely that each blocks access to the
other.

Actors and destinations attach to their nearest traffic point. The first and
last approach segments are collision checked too. Place pickups, civilians,
headquarters and workpoints near the intended road, especially at forks.
Sprites and terrain do not create collision; movement does not read a terrain
mask. Separate water networks intentionally. F1 shows potential graph links,
including those currently obstructed, so inspect both artwork and actual play.

**Workers stop at the footprint.** Workers do not walk to the centre of
obstacles, producers, lots, services and valves. Unless the object has an
authored `workpoint`, a worker stops where its route enters the object's
selection ellipse (the ring drawn on hover) and faces the object while working.
The ellipse is centred on the object's ground point; its width is the sprite
body width plus 24, clamped to 65–170 units, and its height is 0.62 times the
width. Pickups, civilians and animals are still approached at their centre.
Because of this, the side from which a worker approaches matters: keep the
approach side of every obstacle on a road that is reachable before it is
cleared, and keep a building's approach road clear of its own footprint.

`tools/mission.py check` reports these as warnings:

- separate traffic networks before obstacles (water or offroad networks can be
  intentional);
- objects whose nearest traffic point is 50 or more units away;
- every worker stranded (none within 80 units of HQ);
- a `people` goal that differs from civilians plus stranded recruits;
- a `basins` goal larger than the number of valves.

It also reports missing files, unknown types, dangling relationships and a
missing HQ or crew as errors. It does not prove solvability, simulate the
economy or detect every blocking shortcut. Load and complete the mission to
verify those properties.

## Mission-specific artwork

New art for stationary objects comes from a mission sprite sheet and a mission
archetype. The sheet names regions of a PNG in the mission folder; the archetype
gives an existing object kind those sprites and its own title.

```xml
<!-- missions/library-rescue/sprites.xml -->
<sprites>
  <sprite name="library-rescue/field-kitchen" image="props.png" rect="745,110,345,280" pivot="0.5,0.68" scale="0.34"/>
  <sprite name="library-rescue/field-kitchen-damaged" image="props.png" rect="380,110,345,280" pivot="0.5,0.7" scale="0.34"/>
</sprites>
```

```xml
<!-- missions/library-rescue/mission.xml -->
<sprites path="sprites.xml"/>
<archetype id="field-kitchen" extends="cafe" title="Field kitchen"
           sprite="library-rescue/field-kitchen" damaged="library-rescue/field-kitchen-damaged"/>
<object id="30" type="field-kitchen" x="245" y="378"/>
```

`rect` is `x,y,width,height` in image pixels from the **top-left**. `pivot` is
the ground point, normalized within that rectangle (0,0 is its top-left). The
world size is the rectangle times `scale`. `image` and `scale` can also be set
once on the `<sprites>` root as defaults, and image paths are relative to the
sheet. Animated sprites use `<frame>` children instead of a single `rect`; see
[content.md](content.md#sprite-sheets). Prefix mission sprite names with the
mission slug so they cannot collide with shared names.

`extends` inherits everything from the parent archetype: costs, rewards, work
duration and animation, production, collision and removal. The child overrides
only the attributes it names, typically `title`, `sprite` and (for producers and
services) `damaged`. A `<cost>`, `<reward>`, `<build>` or `<repair>` child
replaces the inherited amounts for that stock:

```xml
<archetype id="steel-beam" extends="stone" title="Steel beam" sprite="my-mission/beam">
  <cost food="20" wood="20"/>
</archetype>
```

Choose a pivot at the projected ground footprint's centre, not the image's
bottom edge. It is the object's ground point, so it also centres the selection
ellipse that sets click bounds and where workers stop. Measure each state's
footprint; a producer's working (`sprite`) and damaged (`damaged`) art can have
different trims. A cleared obstacle disappears completely; demolishing a
producer restores ordinary lot artwork.

Generate all required props and states in one atlas; never synthesize repaired
or UI states by tinting or otherwise post-processing a normal sprite. Save the
exact prompt beside the atlas (for example `props.prompt.txt`).

Retail sprites keep their original atlas pivots, which differ from the visible
image centres. When centring a retail building in a painted clearing, read its
frame from the original declaration rather than assuming a centre pivot. For a
frame with width/height `w,h` and pivot `dx,dy`, its visible rectangle centre in
world coordinates is `(x + w/2 - dx, y + h/2 - dy)`. To place that centre at a
clearing's measured `cx,cy`, author `x = cx - w/2 + dx` and
`y = cy - h/2 + dy`. For example, damaged `burger0_br` is 94×85 with pivot
(45,31), so its visible centre is 2 units right and 11.5 units above the
object position. The skill's `layout.py anchor` command prints this calculation
as an `<object>` element. Repaired art can have different trim bounds while
retaining the same ground attachment; inspect both states before calling the
placement finished.

Decorative world-depth art, such as an isolated prop, is an object:
`<object id="90" type="decor" x="300" y="520" sprite="my-mission/crate"/>`. It
is depth-sorted with actors but never blocks routes or counts as a job.

## Map effects

Keep painted scenery baked when its lighting and overlapping shadows matter,
as with Emerald Falls' forest. Place moving accents as `<effect>` elements.
Effects never block routes or count as gameplay jobs. All of them share the game
clock, pause with the game and restart deterministically.

```xml
<effect kind="foam" x="1113.5" y="635.1" width="30" rate="22"/>
<effect kind="sparkles" x="1099" y="722" width="38" height="55" rate="5" speed=".8" opacity=".7"/>
<effect kind="insects" x="130" y="560" width="54" height="32" speed="1" opacity=".9" count="2"/>
<effect kind="motes" x="280" y="590" width="110" height="65" speed=".7" opacity=".45" count="8"/>
<effect kind="sprite" x="300" y="520" sprite="my-mission/fern" wind="2.5" speed=".8"/>
```

| Kind | Role and useful attributes | Default stage |
| --- | --- | --- |
| `foam` | White foam around the exact `x,y`; `width`, `rate` | ground |
| `waterfall` | Foam at the foot of a fall whose lip is `x,y`; `fall`, `drift`, `width`, `rate` | ground |
| `sparkles` | Brief surface glints inside an ellipse; `width`, `height`, `rate`, `speed`, `opacity` | ground |
| `insects` | Looping flight with animated wings; `sprite` (default `butterfly`), `width`, `height`, `count`, `speed` | overlay |
| `motes` | Small drifting light accents; `width`, `height`, `count`, `speed`, `opacity` | overlay |
| `sprite` | A named sprite; `sprite` (required), `wind`, `speed`, `scale-x`, `scale-y`, `rotation`, `opacity` | ground |

| Attribute | Range | Default |
| --- | --- | --- |
| `width`, `height` | 1–600 | 32; height 28 for foam, 40 otherwise |
| `rate` | 0.1–180 per second | 26 for foam and waterfall, 6 otherwise |
| `speed` | 0.05–6 | 1 |
| `opacity` | 0–1 | 1 |
| `count` | integer 1–32 | 3 for insects, 8 otherwise |
| `wind` | 0–16 units of canopy bend | 0 |
| `scale-x`, `scale-y` | 0.02–5 | 1 |
| `rotation` | −360–360 degrees | 0 |
| `stage` | `ground` or `overlay` | per kind |
| `fall`, `drift` | 16–220 and −50–50 (waterfall only) | 72 and 0 |

`stage="ground"` draws over terrain and under actors; `stage="overlay"` draws
over the world and under the HUD. There is no world-depth stage for effects:
place depth-sorted art as a `decor` object instead. A `sprite` effect's `wind`
bends its top while the lower trunk stays fixed; with zero wind it is a static
ground decal.

Keep large art clear of clickable jobs and trail centres; use small plants at
meadow edges and riverbank glints only inside water. Prefer several narrow
sparkle patches following the river to a large patch crossing its banks.

**Waterfall foam.** `kind="foam"` centres white foam directly at `x,y`. Use it
for precise placement on painted impact points; particles stay close to the
specified centre as they move. Emerald Falls uses six of these. `kind="waterfall"`
locates a waterfall by its lip at `x,y`: `fall` extends downward to the
receiving pool and `drift` offsets the foot horizontally, so foam appears at
`(x + drift, y - fall)`. Rate controls white foam flecks per second. Flecks and
open foam rings drift gently downstream at the foot; there are no falling-water
particles or aerial mist. Do not apply the lip offset to a directly measured
impact centre. Foam is drawn as white geometry with feathered alpha edges.

Emerald Falls uses its baked background for trees, plants and their painted
shadows. Ten water-glint patches, nine butterfly groups, four mote regions and
six foam patches remain independent editable effects.

## Object relationships

Relationships are attributes on the dependent object that name another object
by id. The loader rejects a relationship to an object of the wrong kind.

| Attribute | On | Names | Effect |
| --- | --- | --- | --- |
| `producer="ID"` | pickup | producer or lot | Output pickup offered while the owner works |
| `gate="ID"` | fog, polluted water, pier, fishing spot, … | valve | Closed until the valve has been operated |
| `service="ID"` | `atv`, `fishing-boat` | garage or pier | The vehicle's base; it works only when that service does |
| `support="ID"` | roof civilian | decor house | The building the survivor rides on |
| `workpoint="x,y"` | any job target | — | Where the worker stands, instead of the footprint edge |
| `home="x,y"` | vehicle | — | Parking spot on the vehicle's network |
| `corridor="x,y,radius"` | `transport="atv"` worker | — | Offroad region the ATV drives through |

**Production outputs.** A pickup with `producer="ID"` names a producer or lot.
The pickup stays hidden until its owner operates. Use one output per owner,
positioned beside the road and outside the owner's footprint in both states.
The original campaign-2 scene (`level018`) has food pickup 657 linked to cafe
646. The runtime turns an existing output into the producer's supply type when
it replenishes. It does not invent an output if none was authored. Demolition
retains the relationship while releasing the lot.

**Burning trees.** `<object type="tree" burning="true"/>` adds flame and smoke.
An unmarked tree stays unlit, and `log` cannot burn. This is distinct from
`fire-hazard`, which uses road-fire artwork and has deferred specialist rules.
Avoid making road fires or `blast-rock` the main puzzle in a new mission.

**Valve groups.** Fog, polluted water, piers and fishing spots take `gate="ID"`
naming their valve. Operating the valve clears every fog and polluted-water
object gated by it; fog prevents dispatch within 105 units while active. Give
the valve a clear on-foot `workpoint` outside its own fog. In the retail scene
`level013`, valve 819 and its fog share the name `tap0`, and marker 1045 at
approximately `(1102,404)` is the valve's workpoint.

**ATV and garage.** An `atv` needs `service` naming its garage and `home` for
its parking spot. A stranded worker with `transport="atv"` needs
`corridor="x,y,radius"`: the ATV drives from its home through the corridor
centre to the worker and back, and pays 10 fuel per trip. The recruit is
dropped at the garage's workpoint. Start with an `operational` garage, or give
it a reachable repair approach and 40 wood. In `level012`, ATV 798 has its home
at marker 928, which points to garage 662. This transport supports stranded
workers, not arbitrary civilian offroad rescues.

**Boats and piers.** A `fishing-boat` uses the same `service` and `home`
pattern with a pier; put `home` on the water network and the pier's
`workpoint` on land for repairs. Fishing spots use `transport="boat"`;
`fishing-spot` delivers 10 food and `fishing-spot-large` 20. Boats return
their catch before crediting food. In `level011`, boat 859's home is marker 861
for pier 858, and unnamed marker 860 is the pier's land approach. Gate piers and
fishing spots with their basin's valve. Boat routes use the same implicit graph,
so keep different basins disconnected even after their valves are open.

## Balance and progression

These are the restoration's explicit balance rules from
`content/archetypes.xml`, not recovered retail constants. Missions change them
only by declaring archetypes with their own `<cost>`, `<reward>`, `<build>` or
`<repair>`.

Hover cards contain up to three centred rows: the object's name, `Required`,
and `Income`. Amounts use the original resource icons. Omit rows that do not
apply, retain the full cost while queued, and do not add strategy hints.
Income is the actual pickup/job reward; production buildings also show their
income interval on the same row.

| Action | Cost | Result |
| --- | --- | --- |
| Clear `tree`/`log` | 20 food | 20 wood after work completes |
| Clear `stone` | 20 food | Opens route |
| Clear `boulder` | 40 food and 100 money | Opens route |
| Repair `bridge` or `road` | 20 wood | Opens route |
| Repair `auto-bridge` | 20 food and 40 wood | Opens route |
| Construct sawmill / cafe / house / gas station | 40 / 40 / 60 / 80 wood | Wood / food / money / fuel production |
| Repair those buildings | Half their construction cost | Restores production |
| Repair pier or garage | 40 wood | Enables service |
| Rescue animal | 10 food | Counts when returned home |
| Operate valve | 20 money | Clears its gated group and increments basin progress |
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
2. Put one obstacle between HQ and a rescue branch. Supply the exact cost on
   the reachable side, or make its cost obtainable through an earlier action.
   Remember that the worker stops on the obstacle's footprint edge: the road
   leading to that edge must be reachable.
3. Add a second branch whose reward funds the next step: food unlocks a tree,
   its wood restores a sawmill or bridge, money from a house funds a valve or
   boulder, fuel enables a vehicle trip.
4. Check the reachable inventory **before each mandatory action**, not just
   the total resources on the map. Never lock the only food source behind an
   unaffordable food obstacle, or the only fuel source behind the ATV that needs it.
5. Add choices with visible consequences: a short expensive route versus a
   longer cheap one, an extra worker behind a branch, or a production repair
   competing with a bridge for wood. Keep the main route recoverable after an
   optional purchase, or make that purchase's income cover the lost budget.
6. Add scenery last. Keep actionable objects distinct and out of the top-left
   and top-right HUD regions. Theme jobs with mission archetypes rather than
   new mechanics.
7. Finish from a fresh restart with no injected resources. Recheck after moving
   points, because an accidental connection can bypass an obstacle or a basin gate.

A new mission is not covered by the existing regression flags. Add a
playthrough mode to `tests/missions_main.c` (and a line to `test-missions` in
the `Makefile`) that completes it through ordinary clicks. `VOLCANO_TEST_SHOTS=1
make test` saves the frames each mode captures to `build/`.

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
The first mission deliberately starts both workers at HQ (`crew-at-hq="true"`
in `content/campaign.xml`), even though its binary places them far away. Missions
use the distance-based recruit rule by default, so relocate starting workers when
adapting that particular scene.

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
ATV/garage records, although its restored tutorial does not activate them
(`offroad="false"`). These later services suggest a useful progression for
future designs: introduce one dependency, combine it with familiar resource and
route constraints, then increase the number of branches. Reusing records alone
will not implement those services in this engine. The analysis reports first
appearances across the whole campaign so they can be studied without confusing
source-file numbering with mission order.

Generate the full local inventory with:

```sh
make analyze-levels
# Report: build/level-analysis.json
make test-retail ASSETS=reference/rescue-team
```

The analysis records every observed type, all distinct parameter strings and
their frequencies, sample records with original IDs, campaign order, point
counts, graph components, and first type appearances. The retail suite loads the
campaign scenes through the runtime importer.

## Retail binary reference

Original scenes are Rescue Team 5 version 301 binaries in
`reference/rescue-team/level/`. The game imports them at runtime in
[`src/content/retail_import.c`](../src/content/retail_import.c), which maps
records to the same archetypes missions use. `tools/retail_levels.py dump`
writes one as XML for study; `analyze` (behind `make analyze-levels`) inventories
them all. Neither tool writes binaries.

The layout, for reference: all numbers are little-endian; strings are a two-byte
length followed by that many bytes. A file holds `u32 version (301)`, the
background string, `u32 max_id`, `u32 object_count`, then records. Each record
starts with `u32 type, u32 id`. Traffic (type 18) continues with a point count,
float32 `x,y` pairs and a layer. Every other record has `u32 layer`, the `name`,
`params` and `extra` strings, `u32 link_id`, and float32 `x, y, rotation,
scale_x, scale_y`; ambient records (type 6) add an auxiliary position.

When translating a dump into a mission:

| Retail record | Mission object |
| --- | --- |
| barrier `tree00` | `tree` (burnable); `name="fire"` → `burning="true"` |
| barrier, other `tree*` | `log` |
| barrier `stone_big*` / `stone_dynamite*` / other `stone*` | `boulder` / `blast-rock` / `stone` |
| barrier `bridge_auto*` / other `bridge*` | `auto-bridge` / `bridge` (`bridge_wood01` is `type="bridge" sprite="bridge_wood01"`) |
| barrier `road*`, `broken_road0` | `road` |
| barrier `fire` | `fire-hazard` |
| barrier `fog_high` | `fog` with `gate` naming the valve of the same `name` |
| resource `food1/2`, `wood1/2`, `money1/2`, `gas1/2` | `food-small/large`, `wood-…`, `money-…`, `fuel-small/large` |
| resource `boat_food` | `fishing-spot` (`name="food2"` → `fishing-spot-large`) with `transport="boat"` |
| resource `boat_wood` | `floating-wood` |
| building `sawmill`, `burger`, `house`, `derrick` | `sawmill`, `cafe`, `house`, `gas-station`; `extra="0;0"` → `operational="true"` |
| resource `link_id` to a building/lot | `producer="ID"` |
| worker `extra="offroad"` | `transport="atv"` with `corridor` from the enclosing type-152 region (`link_id` is its radius) |
| civilian `extra="roof"` | `roof="true"` with `support` naming the flooded house decor |
| base, lot `build_place`, valve, pier `seaport`, garage | `hq`, `lot`, `valve`, `pier`, `garage`; `extra="0;0"` → `operational="true"` |
| animal `cat`/`dog` | `cat`/`dog` |
| atv / boat with named marker | `atv` / `fishing-boat` with `service` and `home` from the marker |
| unnamed marker linked to a service or valve | that object's `workpoint` |
| scenery (type 2) / ambient (type 6) | `decor` with its `sprite`, or `water`, `bird`, `cloud` |

Barrier variants whose art differs from the archetype default keep it with
`sprite`, using the atlas name. A few retail names differ from their atlas
entries (`tree00` is drawn as `bar_tree00`); `src/content/retail_import.c` lists
them. Coordinates carry over
unchanged; both use the bottom-left world origin.
