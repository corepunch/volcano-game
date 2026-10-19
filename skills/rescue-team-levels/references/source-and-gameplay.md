# Mission format and gameplay

Contents: mission folder and XML; settings; archetypes and objects; routes;
economy and queues; relationships and advanced mechanics; retail scenes;
supported boundaries.

## Mission folder and XML

A mission is `missions/SLUG/mission.xml` plus its art. The game reads the XML
directly; there is no compile step, sidecar or binary output. Validate with:

```sh
python3 tools/mission.py check missions/SLUG/mission.xml
make run MISSION=SLUG
```

| File | Contract |
| --- | --- |
| `mission.xml` | Required; grammar below |
| `background.png` | Painted 16:9 plate stretched over 1366×768 |
| `background@2x.png` | Optional; used instead with `--hd` |
| `*.png` + `sprites.xml` | Optional mission art and its sprite sheet |
| `*.prompt.txt`, `design.txt` | Provenance: exact prompts, measured layout, references |

Concepts, rejected images and earlier stages go to `art-source/`, not the
mission folder.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- One-line summary of the mission. -->
<mission format="volcano-mission-1" title="Starter Rescue" retail-background="103">
  <settings rules="rescue" food="20" people="2"/>
  <traffic>326,371 314,342 310,304 …</traffic>
  <object id="2" type="hq" x="345" y="371"/>
  <object id="3" type="worker" x="345" y="371"/>
  <object id="4" type="tree" x="545" y="358"/>
  <object id="5" type="bridge" x="426" y="196" sprite="bridge_wood01"/>
  <object id="6" type="civilian" x="525" y="395"/>
  <object id="8" type="food-large" x="315" y="268"/>
</mission>
```

| Element | Contract |
| --- | --- |
| `<mission>` | `format="volcano-mission-1"`, `title`, exactly one of `background="FILE"` or `retail-background="103"` |
| `<settings>` | Exactly one; see below |
| `<pack path>` | Asset folder mounted for this mission (e.g. `../../assets/kenney-characters`); up to 8 |
| `<sprites path>` | Mission sprite sheet, usually `sprites.xml`; up to 8 |
| `<archetype>` | Mission-specific object kind: `id`, `extends`, overrides |
| `<traffic>` | Space-separated `x,y` points; may repeat; all points form one graph |
| `<object>` | Placed archetype |
| `<effect>` | Decorative animated effect; see art/effects reference |

The loader (`src/content/mission.c`) is strict: unknown elements/attributes,
duplicate ids, unknown types, out-of-range values and relationships to the
wrong kind of object are errors naming the line. Paths are relative to the
mission file. Coordinates are world units, **bottom-left origin**, on the
1366×768 playfield. Limits: 4096 objects, 4096 traffic points, 1024 effects.
`retail-background` requires the original asset directory.

## Mission settings

```xml
<settings rules="rescue" food="40" wood="0" money="0" fuel="0" people="7" basins="0"/>
```

| Attribute | Default | Meaning |
| --- | --- | --- |
| `rules` | required | Ruleset; missions use `rescue` |
| `food`, `wood`, `money`, `fuel` | 0 | Starting stock, 0–1000000 |
| `people` | 0 | Civilians **plus recruits who return home**, excluding starting crew |
| `basins` | 0 | Completed valve activations required |
| `crew-at-hq` | false | Every on-foot worker starts in HQ (no recruits) |
| `offroad` | true | Affects only retail imports of ATV/garage records |
| `buildable` | all four | Producers lots may build, e.g. `"sawmill house gas-station"` |

Every authored animal adds a separate animal objective automatically. There are
no configurable deadlines, star thresholds or upgrades. The original campaign's
settings use the same attributes in `content/campaign.xml`.

Use one HQ. Starting workers must be **strictly less than 80 world units** from
the first `hq`; more distant on-foot workers start stranded and join after
returning home. Include those recruits in `people`. Every mission uses the first
HQ as the common destination.

## Archetypes and objects

Archetypes are object kinds in `content/archetypes.xml`; `docs/content.md` lists
every attribute. Prices are data there, not in C.

| Type | Supported authoring behavior |
| --- | --- |
| `hq` | Headquarters |
| `worker` | Starting crew/recruit; `transport="atv"` + `corridor` for ATV recruitment |
| `boat-worker` | Crew member restricted to the water network |
| `civilian` | Automatic on-foot evacuation once the route opens; `roof` + `support` for rooftop art |
| `cat`, `dog` | Click to capture (10 food) and lead home; `variant` 0–2 |
| `tree` / `log` | 20 food, 20 wood reward; only `tree` can be `burning` |
| `stone` / `boulder` | 20 food / 40 food + 100 money |
| `bridge` / `auto-bridge` | 20 wood / 20 food + 40 wood; `sprite="bridge_wood01"` etc. for variants |
| `road` | 20 wood |
| `debris` | Art-less, cost-free obstruction; base for themed archetypes |
| `fog` | Cleared by its `gate` valve; blocks work within 105 units |
| `blast-rock`, `fire-hazard` | Dynamite/road-fire rules not restored; avoid as mandatory gates |
| `food-small/large`, `wood-…`, `money-…`, `fuel-small/large` | 10/20-unit supplies |
| `floating-wood` | 10 wood for a boat worker |
| `fishing-spot`, `fishing-spot-large` | 10/20 food for the fishing boat; `transport="boat"` |
| `sawmill`, `cafe`, `house`, `gas-station` | Producers; damaged unless `operational="true"` |
| `lot` | Construction site for a `buildable` producer |
| `pier`, `garage` | Vehicle services; 40 wood repair; `operational="true"` starts working |
| `valve` | 20 money; clears its gated group; counts toward `basins` |
| `atv`, `fishing-boat` | Vehicles; need `service` and `home` |
| `decor` | Scenery with per-placement `sprite`, depth-sorted with actors |
| `water`, `polluted-water`, `bird`, `cloud` | Animated water and ambient scenery |

A mission archetype inherits everything from `extends` (cost, reward, duration,
work animation, production, collision, removal) and overrides only the named
attributes, usually `title`, `sprite` and `damaged`. `<cost>`, `<reward>`,
`<build>` or `<repair food= wood= money= fuel=/>` children replace the inherited
amounts:

```xml
<archetype id="derailed-coaster-car" extends="boulder" title="Derailed coaster car"
           sprite="twisted-tracks/derailed-coaster-car"/>
<archetype id="field-kitchen" extends="cafe" title="Field kitchen"
           sprite="library-rescue/field-kitchen" damaged="library-rescue/field-kitchen-damaged"/>
```

Object attributes:

| Attribute | Default | Meaning |
| --- | --- | --- |
| `id` | required | Positive, unique integer; relationships refer to it |
| `type` | required | Shared or mission archetype id |
| `x`, `y` | 0 | Ground point, bottom-left world units |
| `layer` | 6 for workers/civilians/animals/vehicles, else 5 | Draw order, then descending y |
| `sprite` | archetype's | Per-placement art (decor, alternate bridge) |
| `variant` | 0 | Animal art: 0–2 selects `cat0`/`cat1`/`cat2` |
| `operational` | false | Working producer/service |
| `burning` | false | Burnable obstacle on fire |
| `floating` | false | Bobs on water |
| `roof` | false | Rooftop survivor (with `support`) |
| `transport` | `foot` | `atv` recruit or `boat` fishing spot |
| `producer`, `gate`, `service`, `support` | none | Relationships (below) |
| `workpoint="x,y"`, `home="x,y"`, `corridor="x,y,radius"` | none | Authored points (below) |
| `speed` | archetype's | Ambient bird/cloud travel speed |

## Movement graph

- Every pair of traffic points at distance **strictly less than 50** connects;
  exactly 50 does not. Neither point order nor painted roads defines edges.
- Sample roads about 28–42 units apart; the helper defaults to 30. Keep
  unrelated routes at least 50 apart, including workpoint/pickup approaches
  near gates.
- A live obstacle blocks any segment passing **strictly less than 20** units
  from its center. Gate a narrow throat so nearby cross-links cannot bypass it.
  Avoid placing gates exactly at a sharp corner: diagonal links can cut around
  their blocking radius even when every painted path looks gated.
- The target obstacle is excluded from its own collision check; other obstacles
  still apply. Avoid gates that mutually block access to each other.
- Every actor/destination attaches to its nearest graph point. Both attachment
  segments are collision checked. Give off-path buildings clear short approaches.
- **Footprint approach.** Workers stop where their route enters the selection
  ellipse of an obstacle, producer, lot, service or valve, and face it while
  working, unless it has a `workpoint`. The ellipse is centered on the object's
  ground point; width = sprite body width + 24, clamped to 65–170; height =
  0.62 × width. Pickups, civilians and animals are approached at their center.
  Keep the approach side of each obstacle on a road reachable before clearing.
- Check outbound and return paths. Terrain and sprite silhouettes do not make
  collision. Use F1 to inspect all potential geometric links.

`tools/mission.py check` warns about disconnected traffic networks, objects 50
or more units from traffic, all workers stranded, a `people` goal that differs
from civilians plus stranded recruits, and `basins` above the valve count. It
does not prove solvability, animation existence or the absence of gate shortcuts.
Separate water networks may be intentional.

## Restoration economy

Confirm against `content/archetypes.xml` and `src/sim/jobs.c`/`economy.c`.
These prices are explicit restoration rules, not recovered retail constants; do
not change them merely to match a screenshot from the retail game. A mission
changes them only through its own archetypes.

| Action | Reserved cost | Result |
| --- | --- | --- |
| Clear tree/log | 20 food | 20 wood on completion |
| Clear stone | 20 food | Opens route |
| Clear boulder | 40 food + 100 money | Opens route |
| Repair bridge/road | 20 wood | Opens route |
| Repair auto-bridge | 20 food + 40 wood | Opens route |
| Construct sawmill/cafe/house/gas station | 40/40/60/80 wood | Production |
| Repair those buildings | 20/20/30/40 wood | Restored production |
| Repair pier/garage | 40 wood | Enables service |
| Capture animal | 10 food | Rescue counted after return home |
| Operate valve | 20 money | Clears gated group; optional basin progress |
| ATV/fishing trip | 10 fuel | Recruitment/food delivery |
| Small/large pickup | No worker resource cost | 10/20 units after delivery |

Buildings pay 5 units every 5 seconds automatically. An authored linked bonus
pickup replenishes to 10 units every 10 seconds when eligible; a worker delivers
it to HQ before credit. A building without an authored output does not invent
one. Costs reserve atomically on dispatch. Queues do not reserve supplies, retry
jobs in order, and allow an affordable later job past an unaffordable earlier
one. Clicking a queued job cancels it.

Budget by reachable stage, including pending deliveries. Never finance a gate
with supplies behind that gate. Check alternative spending and partial-stock
states, not only a global total or an ideal repair sequence.

Forest Mayday's human-opening regression clears the first northern log and
southern stone, then collects two timber caches: **0 food, 60 wood, 0 money,
two people rescued**. The second tree needs 20 food. Repairing the reachable cafe
for 20 wood produces food and automatically resumes the queued tree. This is
recoverable without inventory grants. Keep that recovery possible after moves.

## Relationships and advanced supported mechanics

**Production outputs:** a pickup's `producer="ID"` names its producer or lot.
Output stays hidden until operation. Keep it clickable outside damaged/repaired
footprints. Demolition retains the relationship while releasing the lot. The
runtime updates its supply type to match the current producer on replenishment.

**Burning trees:** `<object type="tree" burning="true"/>` adds restored
fire/smoke; an unmarked tree stays unlit and `log` cannot burn. `fire-hazard` is
a different, incompletely restored specialist hazard. Avoid mandatory
road-fire/dynamite puzzles.

**Valves:** fog, polluted water, piers and fishing spots take `gate="VALVE_ID"`.
Operating the valve clears gated fog/pollution and opens the gated services.
Fog suppresses dispatch within 105 units, so give the valve a `workpoint`
outside active fog.

**ATV:** `<object type="atv" service="GARAGE_ID" home="x,y"/>`. The recruit is a
`worker` with `transport="atv"` and `corridor="x,y,radius"`; the ATV drives home →
corridor center → recruit and back, paying 10 fuel per trip, and drops the
recruit at the garage's workpoint. Start with an operational garage or provide a
reachable repair approach and 40 wood. This supports offroad recruits, not
arbitrary civilian rescues.

**Boat:** `<object type="fishing-boat" service="PIER_ID" home="x,y"/>` with
`home` on the water network and the pier's `workpoint` on land. Fishing spots
use `transport="boat"`; gate piers and fishing spots with their basin's valve.
Separate basins carefully (keep different water networks over 50 apart) and
provide land access, repairs and fuel first.

**Rooftop survivors:** `roof="true"` with `support="DECOR_ID"` draws the
survivor on that building. An open foot route still evacuates the person.

## Retail scenes

The game imports original scenes at runtime (`--campaign N`, `--level INDEX`,
`src/content/retail_import.c`); there is no decompiler to editable missions. To
adapt one, dump it and author a new `mission.xml`:

```sh
python3 tools/retail_levels.py dump reference/rescue-team/level/level018.lvl build/level018.xml
make analyze-levels      # build/level-analysis.json for all 52 binaries
```

Mapping: barrier `tree00`→`tree` (`name="fire"`→`burning="true"`), other
`tree*`→`log`, `stone_big*`→`boulder`, `stone_dynamite*`→`blast-rock`,
`stone*`→`stone`, `bridge_auto*`→`auto-bridge`, `bridge*`→`bridge`
(`bridge_wood01` is `type="bridge" sprite="bridge_wood01"`), `road*`/`broken_road0`→`road`;
resources `food1/2`, `wood1/2`, `money1/2`, `gas1/2`→`food-small/large`,
`wood-…`, `money-…`, `fuel-small/large`; buildings `sawmill`/`burger`/`house`/`derrick`→
`sawmill`/`cafe`/`house`/`gas-station`; `extra="0;0"`→`operational="true"`;
resource `link_id`→`producer`; worker `extra="offroad"`→`transport="atv"`;
civilian `extra="roof"`→`roof="true"`. Markers become `workpoint`/`home`, type-152
regions become `corridor`, named valve groups become `gate`.

Original examples: `level011` (boats/piers), `level012` (ATV/garage), `level013`
(valve/fog), `level018` (production outputs). Exact relationship IDs are in
`docs/creating-missions.md`; do not fabricate retail metadata. Keep dumps under
`build/`.

## Campaign patterns to study

File numbering is not campaign order. Type-135 map nodes use `name` for the
mission background and `extra` for its prerequisite; follow the empty prerequisite
to recover all 50 missions. `make analyze-levels` inventories all 52 scene/map
binaries, graph components, type parameters, sample IDs and first appearances.

| Campaign mission | Scene | Useful pattern |
| --- | --- | --- |
| 1 | `level015` | Supplies → tree → bridge; three civilians |
| 2 | `level018` | Production repairs, linked bonuses and a recruit |
| 3 | `level019` | Construction lot, house income for a boulder, then cafe replacement |
| 4 | `level012` | Garage/ATV, radius region, transported recruits |
| 5 | `level013` | Shared valve/fog group and animal rescue |
| 6 | `level011` | Land plus two deliberately separate polluted boat networks |

The repeated design is a potentially connected network partitioned by paid jobs,
with rewards/producers opening the next partition. Introduce one dependency,
then combine it with familiar route/resource constraints and add branches.
This is an interpretation of scene data and restored play, not recovered retail AI.
Campaign mission 1 starts its crew at HQ (`crew-at-hq="true"` in
`content/campaign.xml`) despite distant binary positions; missions use the
under-80-unit rule by default. Move starting crew near HQ when adapting that
scene. Presence of service records alone proves no behavior.

## Unsupported progression

Helicopter transport, doctors/healing, police/thieves, railways, medicine deadlines,
generators, bonfires, runway lights, boosts, air deliveries and dynamite rules
are not restored mission systems and have no archetypes. Some retail records
render cosmetically on import. Roof visual attachment exists but an open foot
route can evacuate the person. Use a crashed aircraft as scenery unless
implementing its mechanics is explicitly part of the task. Engine feature work
needs its own verification.
