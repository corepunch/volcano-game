# Source format and gameplay

Contents: source/binary contract; mission settings; object vocabulary; routes;
economy and queues; linked mechanics; supported boundaries.

## Source and binary contract

Use the repository compiler, not a new binary writer:

```sh
python3 tools/level_source.py check levels/SLUG.level.json
python3 tools/level_source.py compile levels/SLUG.level.json build/levels/SLUG.lvl
python3 tools/level_source.py decompile build/levels/SLUG.lvl build/editable/SLUG.level.json
python3 tools/level_source.py analyze reference/rescue-team/level build/level-analysis.json
```

| Root field | Contract |
| --- | --- |
| `format` | Required `volcano-level-1` |
| `background` | Required retail stem, e.g. `103`, or local `@SLUG.png` |
| `objects` | Required ordered array, maximum 4096 records |
| `mission` | Seven fields below; omit only for an intentional prototype scene |
| `version` | Optional, defaults to 301; other values rejected |
| `max_id` | Optional uint32, derived when omitted; omit for a new scene |
| `$schema` | Usually `../docs/level-source.schema.json` from `levels/` |
| `_comment` | Author note; also accepted on mission and object records |

JSON has no comments/trailing commas; unknown fields are rejected. Coordinates
must fit finite float32. ID, layer and link values are uint32. Object IDs must
be unique; start new IDs at 1 since reference zero means no link. Preserve object
order and unknown numeric types when adapting retail scenes.

The v301 binary is little endian: uint32 version; uint16-byte-length background;
uint32 max ID and object count; typed records. Ordinary records store type, ID,
layer, three length-prefixed strings, link ID and five float32 transforms; type 6
adds two auxiliary floats. Traffic instead stores type, ID, point count,
float32 coordinate pairs and layer. Do not append custom mission data/trailers.

Mission settings compile into `SLUG.lvl.rules`: UTF-8 `version=1` plus all seven
settings, with `allow_cafe` as 0/1. Keep it beside the `.lvl`. Decompilation reads
an adjacent sidecar but does not invent one for a retail binary. Compilation
rejects replacing a rules-bearing output with a source without `mission`; resolve
obsolete sidecars explicitly rather than accidentally reusing stale settings.

Local PNG/JPEG backgrounds and custom sprite images are copied unchanged beside
the output binary. `@` references must be bare filenames beside the source,
with no directory components. Keep these images with a shared compiled mission.
Retail background stems still require the original asset directory.
The production reader and converter round-trip 52 original binaries; this does
not establish that a new custom file works in the original commercial game.

## Mission configuration

```json
"mission": {
  "food": 40, "wood": 0, "money": 0, "fuel": 0,
  "rescue_goal": 7, "basin_goal": 0, "allow_cafe": true
}
```

All seven keys are required when `mission` exists. Quantities are integers
0–1000000; `allow_cafe` is boolean. `rescue_goal` counts civilians **plus recruits
who return home**, excluding starting crew. Every authored animal adds a separate
animal objective automatically. `basin_goal`, when nonzero, counts completed
valve activations, not unique water tiles. There are no configurable custom
deadlines, star thresholds, upgrades or campaign progression in this format.

Use one HQ. Starting workers must be **strictly less than 80 world pixels** from
the first base; more distant on-foot workers start stranded and join after
returning home. Include those recruits in the rescue goal. Custom missions use
the first base as their common destination. Limits: 64 worker records,
64 civilians, 16 vehicles.

## Object records

Required: `id`, `type`. Defaults for ordinary records:

| Field | Default | Meaning |
| --- | --- | --- |
| `layer` | 0 | Preserved layer, commonly 5 for gameplay and 6 for actors |
| `name` | empty | Also identifies burning logs and named groups |
| `params` | empty | Sprite name or type-specific parameter text |
| `extra` | empty | State/transport metadata |
| `link_id` | 0 | Type-dependent reference/value; not always another object's ID |
| `x,y` | 0,0 | Bottom-left world pixels, 1366×768 playfield |
| `rotation` | 0 | Retained original transform; not a general gameplay rotation |
| `scale_x,scale_y` | 1,1 | Used for custom scene sprites, not general obstacle collision |
| `auxiliary` | x,y | Only ambient/type 6: initial ambient position pair |

String byte maxima: background 127; name 511; params and extra 4095 each. No NUL.
New text is UTF-8; surrogate escapes preserve non-UTF-8 retail bytes on round-trip.
Traffic records use only type, ID, layer and points (plus optional `_comment`):

```json
{"type":"traffic","id":1,"layer":4,"points":[[200,400],[230,400],[260,400]]}
```

Multiple traffic records concatenate; the combined maximum is 4096 points.

| Alias / code | Supported authoring behavior |
| --- | --- |
| `base` / 110 | Headquarters, no params needed |
| `worker` / 104 | Starting crew/recruit; `extra="offroad"` for supported ATV recruitment |
| `civilian` / 109 | Automatic on-foot evacuation once the route opens |
| `animal` / 105 | `cat` or `dog`; click to capture and return home |
| `barrier` / 102 | `tree00`, `stone00`, `stone_big00`, `bridge_wood00/01`, `bridge_auto00/01`, `broken_road0`, linked `fog_high` |
| `building` / 101 | `sawmill`, `burger`, `house`, `derrick`; `extra="0;0"` working, other values damaged |
| `resource` / 108 | `food1/2`, `wood1/2`, `money1/2`, `gas1/2`; `boat_food` for fishing |
| `lot` / 146 | `build_place`; construct a chosen producer |
| `marker` / 3 | Service/valve approach or named vehicle home marker |
| `valve` / 128 | Named group controller, optional linked workpoint |
| `pollution` / 151 | Named water group; link value can be tile metadata |
| `boat` / 113 | Named boat linked through a home marker to a pier |
| `pier` / 114 | `seaport`; working only when `extra="0;0"` |
| `atv` / 153 | Named vehicle linked through a home marker to a garage |
| `garage` / 154 | `garage`; working only when `extra="0;0"` |
| `offroad_region` / 152 | Center x,y, radius in `link_id` |
| `effect` / 5 | Generic map visual; see art/effects reference |
| `scenery` / 2 | Existing cosmetic animations, e.g. `anim=water0` |
| `ambient` / 6 | Original birds/clouds; preserve params and auxiliary position when adapting |
| `background` / 4 | Preserved retail object; root background drives custom loading |

The compiler accepts further aliases/numeric types for preservation. That does
not establish their mission behavior; check engine support before using them.

## Movement graph

- Every pair of traffic points at distance **strictly less than 50** connects;
  exactly 50 does not. Neither point order nor painted roads defines edges.
- Sample roads about 30–42 pixels apart; helper defaults to 30. Keep unrelated
  routes at least 50 apart, including workpoint/pickup approaches near gates.
- A live barrier blocks any segment passing **strictly less than 20** pixels
  from its center. Gate a narrow throat so nearby cross-links cannot bypass it.
  Avoid placing gates exactly at a sharp corner: diagonal links can cut around
  their blocking radius even when every painted path looks gated.
- The target barrier is excluded from its own collision check; other barriers
  still apply. Avoid gates that mutually block access to each other.
- Every actor/destination attaches to its nearest graph point. Both attachment
  segments are collision checked. Give off-path buildings clear short approaches.
- Check outbound and return paths. Terrain and sprite silhouettes do not make
  collision. Use F1 to inspect all potential geometric links.

`check` warns about disconnected components, distant approaches, missing crew/HQ,
references and goals. It does not prove solvability, animation existence, or the
absence of gate shortcuts. Separate water networks may be intentional.

## Restoration economy

Confirm against `job_cost`, `finish_work`, `update_production`, and vehicle logic
in `src/game.c`. These prices are explicit restoration rules, not recovered retail
constants; do not change them merely to match a screenshot from the retail game.

| Action | Reserved cost | Result |
| --- | --- | --- |
| Clear tree | 20 food | 20 wood on completion |
| Clear small stone | 20 food | Opens route |
| Clear large stone | 40 food + 100 money | Opens route |
| Repair wooden bridge/broken road | 20 wood | Opens route |
| Repair auto bridge | 20 food + 40 wood | Opens route |
| Construct sawmill/cafe/house/gas station | 40/40/60/80 wood | Production |
| Repair those buildings | 20/20/30/40 wood | Restored production |
| Repair pier/garage | 40 wood | Enables service |
| Capture animal | 10 food | Rescue counted after return home |
| Operate valve | 20 money | Clears named effect; optional basin progress |
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

## Links and advanced supported mechanics

**Production outputs:** one `resource.link_id` points to its building/lot owner.
Output stays hidden until operation. Keep it clickable outside damaged/repaired
footprints. Demolition retains the relationship while releasing the lot.
The runtime updates its supply type to match the current producer on replenishment.

**Burning logs:** `barrier params="tree00", name="fire"` adds restored fire/smoke;
an unmarked tree stays unlit. Params literally equal to `fire` refer to a different,
incompletely restored specialist hazard. Avoid mandatory road-fire/dynamite puzzles.

**Valves:** match valve `name` to fog/pollution `name`. An unnamed marker with
`link_id=VALVE_ID` supplies its workpoint. Fog suppresses dispatch within 105
pixels, so keep that approach outside active fog. Named groups and numeric links
are not interchangeable.

**ATV:** vehicle `name` matches a named marker whose `link_id` identifies the
garage. An unnamed linked marker supplies the garage repair approach. The recruit
uses `extra="offroad"` and lies inside a type-152 region whose `link_id` is its
radius. The vehicle follows the authored region and pays fuel once per trip.
This supports offroad recruits, not arbitrary civilian rescues.

**Boat:** use the same named-marker pattern for a boat and pier. Put the home
marker on the water network and the unnamed repair approach on land. Fishing
uses `params="boat_food"`; `name="food2"` delivers 20 food, otherwise 10. The
nearest named pollution tile gates piers/fishing through its valve group, so
separate basins carefully and provide land access, repairs and fuel first.

Original examples: `level011` (boats/piers), `level012` (ATV/garage), `level013`
(valve/fog), `level018` (production outputs). Import retail scenes under `build/`
and preserve their fields. Exact relationship IDs are in the project's
`docs/creating-levels.md`; do not fabricate retail metadata.

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
The retail first-mission runtime relocates its crew to HQ despite distant binary
positions; custom missions use the under-80-pixel rule. Move starting crew near
HQ when adapting that scene. Presence of service records alone proves no behavior.

## Unsupported progression

Helicopter transport, doctors/healing, police/thieves, railways, medicine deadlines,
generators, bonfires, runway lights, boosts, air deliveries and dynamite rules
are not fully restored mission systems. Some render cosmetically. Civilian tags
`heli`, `boat`, `roof`, `sick` do not automatically enforce retail specialist
requirements. Roof visual attachment exists but an open foot route can evacuate
the person. Use a crashed aircraft as scenery unless implementing its mechanics
is explicitly part of the task. Engine feature work needs its own verification.
