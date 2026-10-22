# Content reference

Everything the game knows about kinds of objects, rules and missions is data.
This page lists every attribute the loaders accept. The loaders are strict:
unknown attributes or elements, duplicate ids and out-of-range values are
errors that name the file and line.

| File | Purpose | Loader |
| --- | --- | --- |
| `content/archetypes.xml` | Rulesets and archetypes (object kinds) | `src/content/content.c` |
| `content/campaign.xml` | Settings for the original campaign positions 1–50 | `src/content/content.c` |
| `missions/NAME/mission.xml` | One mission: settings, art, objects, roads, effects | `src/content/mission.c` |
| `NAME.sprites.xml` | Sprite sheets: names, rectangles, pivots, frames | `src/assets/assets.c` |

The mission format itself, with examples, is in
[creating-missions.md](creating-missions.md).

## XML rules

Files are XML 1.0, UTF-8. Comments, CDATA and the five predefined entities
(`&lt; &gt; &amp; &quot; &apos;`) plus numeric references work. DOCTYPE and
custom entities are rejected, and so is text mixed with child elements.
Booleans are `true`/`false`. Lists of numbers are comma-separated
(`pivot="0.5,0.7"`); point lists are space-separated `x,y` pairs.

## Rulesets

```xml
<ruleset id="rescue" economy="true" auto-evacuation="true" vehicles="true" gates="true" wildlife="true" goals="true"/>
```

A ruleset names the systems a mission uses. Everything defaults to `false`.

| Attribute | When true |
| --- | --- |
| `economy` | Jobs cost supplies; obstacles pay rewards; producers repair, earn and offer bonuses; operational producers can be demolished |
| `auto-evacuation` | Civilians walk home once a route opens; distant crew start as recruits |
| `vehicles` | ATV and boat trips (10 fuel each) |
| `gates` | Valve groups: fog blocks work nearby, pollution gates piers and fishing |
| `wildlife` | Animals roam, can be captured and count as objectives |
| `goals` | Objectives are people/animals/basins; otherwise "clear everything" |

`rescue` enables everything and is used by every mission. `prototype` enables
nothing; it reproduces the original prototype scenes (campaign 7–50).

## Archetypes

```xml
<archetype id="boulder" extends="stone" sprite="bar_stone_big00">
  <cost food="40" money="100"/>
</archetype>
```

An archetype either names a `class` or `extends` another archetype. Extending
copies every field of the parent; attributes present on the child override
them. A `<cost>`, `<reward>`, `<build>` or `<repair>` child replaces the
inherited amounts for that stock entirely. A mission can declare its own
archetypes the same way, and they stay local to that mission.

### Classes

| Class | Becomes | Job | Default approach |
| --- | --- | --- | --- |
| `obstacle` | Route blocker | Clear for `cost`; pays `reward` | footprint |
| `pickup` | Supply cache | Collect; credited on delivery home | centre |
| `producer` | Building | Repair (`repair`), earn income, demolish | footprint |
| `lot` | Construction site | Build a producer from the menu (its `build` cost) | footprint |
| `service` | Pier or garage | Repair (`repair`) to enable vehicles | footprint |
| `valve` | Valve | Open for `cost`; clears its group | footprint |
| `civilian` | Survivor | Escorted or evacuates on its own | centre |
| `animal` | Animal | Capture for `cost`, lead home | centre |
| `worker` | Crew member | — | — |
| `vehicle` | ATV or fishing boat | — | — |
| `base` | Headquarters | — | centre |
| `water`, `decor` | Scenery | — | — |

**Placement.** An `<object>`'s `x,y` is where it stands. For classes with
a base (`base`, `obstacle`, `pickup`, `lot`, `producer`, `service`, `valve`)
that is the centre of the base: the selection ring is centred there, and the
shown frame's body is centred on it horizontally whatever its pivot (an
animating sprite centres the union of its frames). The pivot's
height sets the sprite's ground line, so it is the only thing art needs to
tune. Place a building at the centre of its painted clearing. Actors and
scenery stand on their sprite's pivot.

**Approach.** With `footprint`, a worker stops where its route enters the
object's selection ellipse and faces the object while working. The ellipse
is the one the game draws on hover: its width is the sprite body width plus
24, clamped to 65–170 units, and its height is 0.62 times that. A placed
`workpoint` always wins over the approach.

### Attributes

| Attribute | Applies to | Meaning |
| --- | --- | --- |
| `id` | all | Unique name, kebab-case by convention |
| `class` / `extends` | all | One of the classes above, or a parent archetype |
| `title` | all | Name shown in tooltips |
| `sprite` | static objects | Sprite name; producers and services use it when working |
| `damaged` | producer, service | Sprite while broken or under construction |
| `idle`, `move`, `carry`, `run`, `work`, `act`, `shadow` | actors | Sprite templates by pose |
| `directions` | actors | Facings in the art (8; ATV 16; boats 24) |
| `variants` | animals | Number of `{v}` art variants |
| `speed` | actors | World units per second |
| `opacity` | all | Draw opacity (clouds use 0.65) |
| `ambient` | decor | `bird` or `cloud`: drifts across the sky |
| `effect` | obstacle, water | `fog`, `fire` or `pollution` drawn as an effect |
| `approach` | targets | `footprint`, `center` or `workpoint` |
| `action` | targets | Worker animation key while working on it, e.g. `wood`, `pick`, `srepair`, `repair`, `tap` |
| `duration` | targets | Seconds of work. Class defaults: obstacle, lot, producer, service and valve 5; civilian 0.8; animal 1.2; pickup 0 |
| `cleared-frame` | obstacle | Frame shown once cleared (bridges); otherwise the obstacle disappears |
| `burnable` | obstacle | Can be placed `burning="true"` |
| `needs-dynamite` | obstacle | Cannot be cleared yet (reserved for the dynamite system) |
| `boat-only` | pickup, worker, vehicle | Collected by, or is, a boat |
| `resource`, `amount` | pickup | What and how much it delivers |
| `produces`, `income`, `income-interval` | producer | Automatic income |
| `bonus`, `bonus-interval` | producer | Pickup offered at its linked output |
| `menu` | producer | Construction menu slot 0–3 |
| `menu-lock` | producer | Message when a mission does not allow building it |

Sprite templates expand `{d}` (direction), `{v}` (variant) and `{action}`
(the target's `action`). For example, the worker's `work="worker_{action}{d}"`
becomes `worker_pick3` while it breaks a stone facing direction 3.

### Stocks

`<cost>`, `<reward>`, `<build>` and `<repair>` take `food`, `wood`, `money` and
`fuel` (0–1000000).

## Campaign

```xml
<campaign rules="prototype">
  <mission number="1" rules="rescue" food="20" people="3" crew-at-hq="true" offroad="false"/>
</campaign>
```

`rules` on the root applies to every unlisted position. A `<mission>`
accepts the same attributes as a mission's `<settings>`.

| Attribute | Meaning |
| --- | --- |
| `rules` | Ruleset id |
| `food`, `wood`, `money`, `fuel` | Starting stock |
| `people`, `basins` | Rescue and purification goals; every animal is an extra goal under `wildlife` |
| `buildable` | Space-separated producers the construction menu offers (default: all) |
| `crew-at-hq` | Start every on-foot worker inside headquarters |
| `offroad` | Activate ATV and garage records (retail missions 1–3 predate them) |

## Sprite sheets

```xml
<sprites image="props.png" scale="0.34">
  <sprite name="library-rescue/field-kitchen" rect="745,110,345,280" pivot="0.5,0.68"/>
  <sprite name="worker_run0" image="kenney-worker-run.png" scale="0.5" fps="15">
    <frame rect="2,2,40,90" pivot="0.5,0.93" repeat="2"/>
  </sprite>
</sprites>
```

Rectangles are image pixels. Everything inside a rectangle is normalized to
it, with `0,0` its top-left and `1,1` its bottom-right, so the numbers stay
the same when the art is redrawn at another resolution.

| Attribute | Meaning |
| --- | --- |
| `name` | Sprite name used by archetypes, objects and effects |
| `image` | Image path relative to the sheet (on the sheet or the sprite) |
| `scale` | World units per image pixel (default 1); world size is `rect` × `scale` |
| `fps` | Animation rate; 0 shows the first frame |
| `rect` | `x,y,w,h` in image pixels from the top-left |
| `pivot` | Ground point, normalized (default `0.5,1`). Frames of trimmed animations may hold it slightly outside `0..1` |
| `body` | Clickable bounds without baked shadow, normalized `x,y,w,h` |
| `repeat` | Hold a frame for several steps |

A sprite has either a single `rect` or `<frame>` children.

## Where sprites come from

The catalog is built from every asset folder, lowest precedence first:

1. Mounted folders (`--assets`, then the built-in `assets/particles`, then each
   `--pack`). In each folder it reads retail declarations
   (`texture/lazy/NAME.lua`) and native sheets (`sprites/NAME.sprites.xml`). A
   higher folder's file with the same name hides the lower one; a sprite
   defined again in a higher folder replaces it.
2. The mission's `<pack>` folders and `<sprites>` files, replaced on every
   level load.

The retail Rescue Team folder supplies the original sprite names
(`worker_run0`, `bar_tree00`, `base0`, …). Any pack can replace them by
name. `assets/kenney-characters` replaces the worker and civilian sprites, and
the HD pack replaces the static objects.

## Mission canvas

The `<mission>` root accepts integer `width` (1024–4096) and `height`
(768–4096) in logical world units. Defaults remain 1366×768 for older files
and retail imports. The iPad missions use `width="1368" height="1026"`.
Backgrounds must have the same aspect ratio as their canvas. The desktop window
uses uniform aspect-fit scaling; HUD panels anchor to the top edges and pause
controls remain centered. Input uses the same bottom-left world transform.

Custom mission settings accept `clear-routes="true"` (default false). When set,
all paid route obstacles must be cleared before victory; the HUD adds a remaining
route-jobs objective. This is enabled for all six iPad missions.
