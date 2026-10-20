# Artwork, coordinates, pivots and ambient effects

## Design canvas and measurement

The renderer's logical playfield is 1366×768. JSON coordinates have bottom-left
origin. Art briefs, screenshots, image pixels and custom atlas rectangles normally
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

## What to bake and what to animate

For this project's forest levels, keep trees, bushes, flower beds, inter-tree
shadows, ground shadows, rocks, cliffs and static set pieces in one painted
background. The previous separated-tree experiment reduced quality because its
integrated lighting and overlapping shadows were lost. An airliner wreck can be
baked scenery while workers, survivors, supplies and barriers remain separate.

Animate suitable accents independently: pure white impact foam, surface glints,
butterflies, small drifting motes, and already supported birds/clouds. Existing
water-bobbing sprites are available when their original art/attachment suits the
scene. Do not add effects everywhere or obscure the work area. Tree sprites
remain an engine capability for isolated props or an explicit future request;
do not infer that the entire forest should become animated sprites.

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
> clear width 32 logical pixels (28–36 accepted), shared across levels, bare HQ/building pads, rescue clearings and any
> missing bridge gap. Keep gameplay clear of the HUD corners. Bake the forest,
> plants, rocks and overlapping shadows together. Leave trail centers and
> interactive pads bare for runtime objects. No baked people, gameplay buildings,
> resource piles, playable log/rock obstacles, UI, icons, text, markers or watermark.
> Include only static vehicles/set pieces that belong in the requested scenery.

Save the exact final prompt beside the selected project image, and copy the
selected output into the project; do not leave a dependency only in a generated
images directory. Inspect output and measure its **actual** pads and paths.
A successful generation is not proof that it followed the coordinates.

## Building pivots

Source x,y is an authored sprite attachment, not necessarily the visible center.
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
calculates an anchor and prints each requested state's frame rectangles:

```sh
python3 SKILL_DIR/scripts/layout.py anchor \
  --assets reference/rescue-team --sprite burger0_br \
  --center 330.25 327.25 --screen --compare-sprite burger0
```

Expected anchor for that measured screen center: world `(328.25,429.25)`.
Use the resulting x,y in JSON, then inspect real renders of both states. The
frame rectangle includes its trimmed artwork and possible shadow/empty pixels;
confirm the perceived building placement as well. Do not shift drawing pivots
or all retail buildings to solve one level's authoring problem.

Keep approach points, recurring output pickups, standalone piles and survivors
outside the union of both building footprints. After a move, recheck click
selection, nearest graph attachments, initial gates and return paths. An apparent
visual fix can change economic access or make a pickup hidden behind a building.

## Generic effects

Map visuals are type-5 `effect` records in one `MapScene`, with deterministic
seeds, shared game time and configurable draw stage. They do not block routes or
count as rescue jobs. Use distinct IDs and descriptive names.

```json
{"type":"effect","id":100,"layer":3,"name":"impact foam",
 "x":1113.5,"y":635.1,"params":"foam;width=30;rate=22"}
```

| Prefix | Important parameters | Purpose/default stage |
| --- | --- | --- |
| `foam` | `width`, `rate`, `opacity`, `speed` | Centered white pool foam / ground |
| `sparkles` | `width`, `height`, `rate`, `speed`, `opacity` | Surface glints in an ellipse / ground |
| `insects` | `animation=butterfly`, `width`, `height`, `count`, `speed`, `opacity` | Original animated wings and looping flight / overlay |
| `motes` | `width`, `height`, `count`, `speed`, `opacity` | Sparse drifting light / overlay |
| `sprite` | `image`, `rect`, `pivot`, `wind`, `speed`, transforms | Optional local sprite/atlas region / world |

Useful forms:

```
foam;width=32;rate=26
sparkles;width=28;height=45;rate=4;speed=.8;opacity=.6
insects;animation=butterfly;width=45;height=25;count=2;speed=.8;opacity=.9
motes;width=90;height=50;count=6;speed=.6;opacity=.35
sprite;image=@props.png;rect=0,0,120,160;pivot=.5,.95;wind=1;speed=.8;stage=world
```

`rect=x,y,width,height` is integral top-left source-image pixels, wholly inside
the image. `pivot=x,y` is normalized to that rectangle. Object scale/rotation
transform custom sprites around their pivot. `wind` bends the canopy while
retaining its root. This mechanism is optional; it is not a reason to replace
baked forest lighting.

Stages: `ground` is above background and below actors; `world` shares actor depth
sorting at the visual's ground position; `overlay` is above world and below HUD.
Bounds: speed .05–6, opacity 0–1, effect region width/height 1–600, rate .1–180,
insect/mote count integral 1–32, wind 0–16, custom sprite scale .02–5. At most
1024 visuals within the total 4096-record limit. Verify current `src/scene.c`.

### Water placement

Place foam at **measured white-water impact points**, never along the falling
sheet or arbitrary downstream pools. Foam must stay white, not gray smoke. Keep
it localized; the painted falling water already provides visual volume. White
geometry avoids tinted particle textures. Use a few glint ellipses following
water surfaces, without spilling onto banks, cliffs or bridges.

Use direct `foam` placement for new emitters. Legacy
`waterfall;width=32;height=72;rate=26;drift=0` treats x,y as the lip and offsets
foam to `(x+drift,y-height)`; height 16–220, drift -50–50. Do not accidentally
apply that lip offset to a directly measured impact center.

Inspect particles at several times, under pause, and after restart. Current
samples: Emerald Falls has six marked foam anchors; Forest Mayday's stream has
separate foam/glint placements. Copy effect syntax, not their coordinates.

## Shared road and bridge sizes

Read [the road and bridge drawing spec](../../../docs/road-and-bridge-sizes.md)
before drawing roads or bridge sprites. Measure clear surface width perpendicular
to travel. The 32-pixel road standard applies across levels; the HD bridge deck
is 64 texture pixels wide, independent of railings and shadows. Record actual
source cross-sections and inspect broken and repaired crossings.
