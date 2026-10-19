# Artwork, coordinates, pivots and ambient effects

## Design canvas and measurement

The renderer's logical playfield is 1366×768. Mission XML coordinates have
bottom-left origin. Art briefs, screenshots, image pixels and custom atlas rectangles normally
have top-left origin. For a pixel in a background of width W and height H:

```
world_x = image_x * 1366 / W
world_y = 768 - image_y * 768 / H
```

For screenshots, first subtract the game viewport's origin and exclude titlebar,
window border and any letterboxing; scale by the actual viewport size. Do not
use the entire desktop capture dimensions. For an existing world point converted
back to screen: `screen_x=x`, `screen_y=768-y`.

HUD corners occupy approximately screen x=0–446,y=0–100 and x=1010–1366,y=0–97.
Keep the playable paths, rescues and building pads clear of these areas. A 16:9
background is mapped across the logical viewport; other aspect ratios distort.
An optional `background@2x.png` beside `background.png` replaces it under `--hd`;
keep the same composition so measured coordinates stay valid.

## What to bake and what to animate

For this project's forest levels, keep trees, bushes, flower beds, inter-tree
shadows, ground shadows, rocks, cliffs and static set pieces in one painted
background. The previous separated-tree experiment reduced quality because its
integrated lighting and overlapping shadows were lost. An airliner wreck can be
baked scenery while workers, survivors, supplies and barriers remain separate.

Animate suitable accents independently: pure white impact foam, surface glints,
butterflies, small drifting motes, and already supported birds/clouds. Existing
water-bobbing sprites are available when their original art/attachment suits the
scene. Do not add effects everywhere or obscure the work area. Wind-animated
`sprite` effects and `decor` objects remain engine capabilities for isolated
props or an explicit future request; do not infer that the entire forest should
become animated sprites.

## Background generation brief

Use the imagegen skill and built-in tool for raster artwork; do not substitute
procedural shapes or draw the background with scripts. If using a local reference,
inspect it first. Label its role as style reference versus edit target. Keep the
previous background intact when creating another level.

Adapt this brief to the actual mission rather than repeating its locations:

> Finished 16:9 background plate for a 1366×768 top-down/gently isometric rescue
> management game. A high fixed camera with no horizon, painterly 3D volume,
> polished casual-PC-game detail, readable warm dirt trails and integrated
> natural shadows. State the user's setting and focal static set piece. Specify
> route centerlines and clearing centers in top-left design coordinates, trail
> width around 40–50 pixels, bare HQ/building pads, rescue clearings and any
> missing bridge gap. Keep gameplay clear of the HUD corners. Bake the forest,
> plants, rocks and overlapping shadows together. Leave trail centers and
> interactive pads bare for runtime objects. No baked people, gameplay buildings,
> resource piles, playable log/rock obstacles, UI, icons, text, markers or watermark.
> Include only static vehicles/set pieces that belong in the requested scenery.

Copy the selected output to `missions/SLUG/background.png` and save the exact
final prompt as `missions/SLUG/background.prompt.txt`; do not leave a dependency
only in a generated images directory. Concepts and superseded stages belong in
`art-source/`. Inspect output and measure its **actual** pads and paths.
A successful generation is not proof that it followed the coordinates.

## Building pivots

Object x,y is the sprite's ground attachment (pivot), not necessarily the visible
center.
Original `texture/lazy/*.lua` declarations contain scalar frame fields
`dx,dy,w,h,u,v,cu,cv`. They are data, not code to execute. Damaged and repaired
frames can have different trims while retaining the same attachment.

For anchor `(x,y)`, visible frame center in world space is:

```
center_x = x + w/2 - dx
center_y = y + h/2 - dy
```

To center a frame at a measured clearing center `(cx,cy)`:

```
x = cx - w/2 + dx
y = cy - h/2 + dy
```

The current retail examples in `texture/lazy/build0.lua`, first frame:

| Sprite | w×h | dx,dy |
| --- | --- | --- |
| `burger0_br` | 94×85 | 45,31 |
| `burger0` | 92×83 | 45,29 |
| `sawmill0_br` | 105×88 | 58,28 |
| `sawmill0` | 103×85 | 58,25 |
| `house0_br` | 126×110 | 61,36 |
| `house0` | 118×117 | 59,36 |

Read current assets rather than treating this table as permanent. The helper
calculates an anchor, prints each requested state's frame rectangles as XML
comments, and ends with an `<object>` to paste (or bare `x`/`y` attributes
without `--id`/`--type`):

```sh
python3 SKILL_DIR/scripts/layout.py anchor \
  --assets reference/rescue-team --sprite burger0_br \
  --center 330.25 327.25 --screen --compare-sprite burger0 --id 30 --type cafe
```

Expected result for that measured screen center:
`<object id="30" type="cafe" x="328.25" y="429.25"/>`. Paste it into
`mission.xml`, then inspect real renders of both states. The
frame rectangle includes its trimmed artwork and possible shadow/empty pixels;
confirm the perceived building placement as well. Do not shift drawing pivots
or all retail buildings to solve one level's authoring problem.

Keep approach points, recurring output pickups, standalone piles and survivors
outside the union of both building footprints. After a move, recheck click
selection, nearest graph attachments, initial gates and return paths. An apparent
visual fix can change economic access or make a pickup hidden behind a building.

## Mission sprites and themed objects

Custom art for jobs, stations and props comes from a mission sprite sheet plus a
mission archetype. Generate every prop and authored state together in one
transparent atlas (for example `missions/SLUG/props.png` with
`props.prompt.txt`), then name its regions in `missions/SLUG/sprites.xml`:

```xml
<sprites>
  <sprite name="SLUG/field-kitchen" image="props.png" rect="745,110,345,280" pivot="0.5,0.68" scale="0.34"/>
  <sprite name="SLUG/field-kitchen-damaged" image="props.png" rect="380,110,345,280" pivot="0.5,0.7" scale="0.34"/>
</sprites>
```

`rect=x,y,w,h` is integral top-left image pixels, wholly inside the image.
`pivot=x,y` is the ground point normalized to that rectangle (0,0 top-left;
default `0.5,1`). World size is `rect` × `scale`; `image` and `scale` may be set
once on `<sprites>`. Image paths are relative to the sheet. Animated sprites use
`<frame rect= anchor="dx,dy" size="w,h" repeat="N"/>` children. See
`src/assets/assets.c` and `docs/content.md#sprite-sheets`.

In `mission.xml`, load the sheet and give an existing archetype the art:

```xml
<sprites path="sprites.xml"/>
<archetype id="field-kitchen" extends="cafe" title="Field kitchen"
           sprite="SLUG/field-kitchen" damaged="SLUG/field-kitchen-damaged"/>
<object id="30" type="field-kitchen" x="245" y="378"/>
```

Put the pivot at the projected ground footprint's center, not the image bottom:
it is the object's ground point, centers the selection ellipse (click bounds and
where workers stop) and is the `x,y` you author. With such a pivot, place the
object at the measured footprint center directly. `layout.py anchor --sprites
missions/SLUG/sprites.xml` instead centers the visible rectangle, which suits
sprites whose pivot is not the footprint center. Measure each state; a damaged
and working producer can trim differently. Cleared obstacles disappear
completely. Never synthesize repaired or UI states by post-processing a normal
sprite.

World-depth scenery props are `<object type="decor" sprite="SLUG/crate"/>`:
depth-sorted with actors, never blocking routes or counting as jobs.

## Generic effects

Map effects are `<effect>` elements with deterministic seeds, shared game time
and a configurable draw stage. They do not block routes or count as rescue jobs.

```xml
<effect kind="foam" x="1113.5" y="635.1" width="30" rate="22"/>
<effect kind="sparkles" x="1099" y="722" width="28" height="45" rate="4" speed=".8" opacity=".6"/>
<effect kind="insects" x="130" y="560" width="45" height="25" count="2" speed=".8" opacity=".9"/>
<effect kind="motes" x="280" y="590" width="90" height="50" count="6" speed=".6" opacity=".35"/>
<effect kind="sprite" x="300" y="520" sprite="SLUG/fern" wind="1" speed=".8"/>
```

| Kind | Important attributes | Purpose / default stage |
| --- | --- | --- |
| `foam` | `width`, `rate`, `opacity`, `speed` | Centered white pool foam / ground |
| `waterfall` | `fall`, `drift`, `width`, `rate` | Foam at the foot below a lip / ground |
| `sparkles` | `width`, `height`, `rate`, `speed`, `opacity` | Surface glints in an ellipse / ground |
| `insects` | `sprite` (default `butterfly`), `width`, `height`, `count`, `speed`, `opacity` | Original animated wings and looping flight / overlay |
| `motes` | `width`, `height`, `count`, `speed`, `opacity` | Sparse drifting light / overlay |
| `sprite` | `sprite` (required), `wind`, `speed`, `scale-x`, `scale-y`, `rotation`, `opacity` | Named sprite / ground |

`wind` bends the sprite's top while retaining its root. This mechanism is
optional; it is not a reason to replace baked forest lighting.

Stages: `stage="ground"` is above background and below actors; `stage="overlay"`
is above world and below HUD. There is no world-depth effect stage; use a `decor`
object for depth-sorted art. Bounds: speed .05–6 (default 1), opacity 0–1,
width/height 1–600, rate .1–180, count integral 1–32, wind 0–16, scale-x/scale-y
.02–5, rotation −360–360. At most 1024 effects per mission. Verify current
`src/content/mission.c` (`load_effect`) and `src/render/effects.c`.

### Water placement

Place foam at **measured white-water impact points**, never along the falling
sheet or arbitrary downstream pools. Foam must stay white, not gray smoke. Keep
it localized; the painted falling water already provides visual volume. White
geometry avoids tinted particle textures. Use a few glint ellipses following
water surfaces, without spilling onto banks, cliffs or bridges.

Use direct `kind="foam"` placement for new emitters. `kind="waterfall"` treats
x,y as the lip and offsets foam to `(x+drift, y-fall)`; `fall` 16–220 (default
72), `drift` −50–50 (default 0). Do not accidentally apply that lip offset to a
directly measured impact center.

Inspect particles at several times, under pause, and after restart. Current
samples: Emerald Falls has six marked foam anchors; Forest Mayday's stream has
separate foam/glint placements. Copy effect syntax, not their coordinates.
