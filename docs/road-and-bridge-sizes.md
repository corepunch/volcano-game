# Road and bridge drawing dimensions

All dimensions below are **logical game pixels on the 1366 × 768 playfield**,
not desktop screenshot pixels or atlas-cell dimensions. Apply the same standard
in every custom level. A clear deck width is the traversable surface between
railings, measured perpendicular to travel; shadows, piers, curbs and rails do
not count. Damage must not change the intended deck width or attachment points.

## Shared dimensions

| Element | Logical size | 1672 × 941 background | 3344 × 1882 background / HD texture |
| --- | --- | --- | --- |
| Ordinary single rescue road / trail, clear width | 32 (accept 28–36) | approximately 39 (34–44) | approximately 78 (69–88) |
| Bridge usable deck, perpendicular to travel | exactly 32 | approximately 39 | exactly 64 texture pixels |
| Short bridge deck span, along travel | 88 | approximately 108 | exactly 176 texture pixels |
| Bridge art envelope, excluding shadow | up to 104 × 56 horizontal; 56 × 104 vertical | — | up to 208 × 112; 112 × 208 |

Use the ordinary-road standard for dirt, gravel and pedestrian rescue routes;
material and theme must not change their scale between levels. Junctions,
building pads and rescue clearings may widen deliberately. A separate vehicle
street must be explicitly briefed and measured as a wider road class; do not
use its width to size a footbridge or silently substitute it for a normal trail.
Never measure a junction, clearing, bend's diagonal bounding box or rubble pile
as the normal road width. Measure perpendicular to a straight stretch.

## Measurements from the existing forest paintings

Manual edge measurements in the 1672 × 941 source PNGs, top-left coordinates:

| Level | Cross-section endpoints | Painted width | Logical width |
| --- | --- | --- | --- |
| Emerald Falls | (550, 417) → (550, 459) | 42 | 34.28 |
| Emerald Falls | (1200, 417) → (1200, 459) | 42 | 34.28 |
| Forest Mayday | (450, 532) → (450, 571) | 39 | 31.83 |
| Forest Mayday | (1000, 534) → (1000, 571) | 37 | 30.20 |

Edges in painterly art are approximate (about ±2 source pixels). These straight
sections share a 32-pixel nominal road class and fit the 28–36 tolerance. The
Retina images preserve the same geometry at twice the source resolution.
Convert a vertical image distance with `logical = source * 768 / image_height`;
convert a horizontal one with `logical = source * 1366 / image_width`.

Emerald Falls' bridge center is (1094.76, 408.89) in bottom-left world
coordinates, approximately (1340.0, 440.0) in the source painting. The river gap
is approximately x=1315–1369 at that crossing: 44 logical pixels. An 88-pixel
span supplies roughly 22 pixels of overlap onto each bank. Center the **deck**
on the road centerline, rather than centering the full shadowed sprite.

## Mandatory sprite brief

Include these numeric requirements in every bridge atlas drawing prompt:

> Shared 1366 × 768 logical game canvas. Ordinary road clear width is 32 logical
> pixels. Bridge clear deck width is 32 pixels, span is 88 pixels; HD texture
> deck dimensions are 176 × 64 pixels horizontal or 64 × 176 vertical.
> Rails/abutments may extend outside the deck, within a 104 × 56 logical-pixel
> horizontal envelope, transposed for vertical bridges. Only horizontal and
> vertical travel axes. Open level approaches, no stairs or crosswise thresholds
> on road bridges. All bridge types and all independently authored repair states
> in one transparent atlas. Every repair stage shares exactly the same deck
> dimensions and center attachment. Keep holes inside the same intended deck.

Atlas source resolution can be larger. Record each deck's source rectangle and
pack that rectangle to the fixed HD dimensions; do not fit the entire sprite
into the old retail frame. Retain the source artwork directly, including every
authored repair state. Do not rotate one bridge to invent another orientation.

## Acceptance for every level

Measure at least two straight route cross-sections before accepting a painting;
record source coordinates, image dimensions and converted logical widths. Keep
normal routes within 28–36 pixels outside transitions and clearings. Measure
both sides of every crossing and compare the usable deck against those routes.
Inspect broken and repaired renders at the actual game scale. New or revised
backgrounds must satisfy this standard, including non-forest themes. Existing
unmeasured paintings must be audited before claiming compliance.
