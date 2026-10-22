# Road and bridge drawing dimensions

All dimensions below are **logical game pixels on the 1368 × 1026 custom playfield**,
not desktop screenshot pixels or atlas-cell dimensions. Apply the same standard
in every custom level. A clear deck width is the traversable surface between
railings, measured perpendicular to travel; shadows, piers, curbs and rails do
not count. Damage must not change the intended deck width or attachment points.

## Shared dimensions

| Element | Logical size | 1368 × 1026 background | 2736 × 2052 background / HD texture |
| --- | --- | --- | --- |
| Ordinary single rescue road / trail, clear width | 24 (accept 22–26) | 24 (22–26) | 48 (44–52) |
| Bridge usable deck, perpendicular to travel | exactly 24 | 24 | exactly 48 texture pixels |
| Short bridge deck span, along travel | 88 | 88 | exactly 176 texture pixels |
| Bridge art envelope, excluding shadow | up to 104 × 56 horizontal; 56 × 104 vertical | — | up to 208 × 112; 112 × 208 |

Use the ordinary-road standard for dirt, gravel and pedestrian rescue routes;
material and theme must not change their scale between levels. Junctions,
building pads and rescue clearings may widen deliberately. A separate vehicle
street must be explicitly briefed and measured as a wider road class; do not
use its width to size a footbridge or silently substitute it for a normal trail.
Never measure a junction, clearing, bend's diagonal bounding box or rubble pile
as the normal road width. Measure perpendicular to a straight stretch.

## Historical measurements from the previous forest paintings

Manual edge measurements in the previous 1672 × 941 source PNGs, top-left coordinates:

| Level | Cross-section endpoints | Painted width | Logical width |
| --- | --- | --- | --- |
| Emerald Falls | (550, 417) → (550, 459) | 42 | 34.28 |
| Emerald Falls | (1200, 417) → (1200, 459) | 42 | 34.28 |
| Forest Mayday | (450, 532) → (450, 571) | 39 | 31.83 |
| Forest Mayday | (1000, 534) → (1000, 571) | 37 | 30.20 |

Edges in painterly art are approximate (about ±2 source pixels). These straight
sections share a previous, wider road class and must be narrowed to the retail-sized standard. The
Retina images preserve the same geometry at twice the source resolution.
These historical conversions used 1366 × 768. For current artwork, convert
vertical distances with `logical = source * mission_height / image_height`, and
horizontal distances with `logical = source * mission_width / image_width`.

Emerald Falls' bridge center is (1094.76, 408.89) in bottom-left world
coordinates, approximately (1340.0, 440.0) in the source painting. The river gap
is approximately x=1315–1369 at that crossing: 44 logical pixels. An 88-pixel
span supplies roughly 22 pixels of overlap onto each bank. Center the **deck**
on the road centerline, rather than centering the full shadowed sprite.

## Mandatory sprite brief

Include these numeric requirements in every bridge atlas drawing prompt:

> Shared 1368 × 1026 logical game canvas. Ordinary road clear width is 24 logical
> pixels. Bridge clear deck width is 24 pixels, span is 88 pixels; HD texture
> deck dimensions are 176 × 48 pixels horizontal or 48 × 176 vertical.
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
normal routes within 22–26 pixels outside transitions and clearings. Measure
both sides of every crossing and compare the usable deck against those routes.
Inspect broken and repaired renders at the actual game scale. New or revised
backgrounds must satisfy this standard, including non-forest themes. Existing
unmeasured paintings must be audited before claiming compliance.

## Retail comparison and pixel density

The original 1366 × 768 plates `001_0.jpg`, `103_0.jpg`, `207_0.jpg` and
`311_0.jpg` have ordinary straight asphalt surfaces approximately 20–24 pixels
wide, excluding the dirt/dark shoulders. Example cross-sections (top-left):
`001`: x=850, y=360–380; `103`: x=650, y=463–485; `207`: x=80,
y=335–357; `311`: x=200, y=464–485. This is a sampled comparison, not a
claim of pixel-exact equality across every retail map. The shared target is
24 logical pixels. `assets/road-dimensions.json` is the machine-readable authority.

A 2× sprite atlas uses 48 texture pixels for a 24-pixel deck. Current 1368 × 1026 backgrounds use 24 pixels; their 2736 × 2052
exports use 48 pixels for the same logical width.
Never double logical widths when adding Retina artwork.

## Current artwork audit

Every custom-painted mission now has a revised `missions/NAME/background.png`
and a 2x `background@2x.png`. The current source history and prompts are in `art-source/ipad/`; the previous
road edit is archived in `art-source/road-width/`. Current calipers and hashes are in
`art-source/road-width-measurements.json`. Run
`python3 tools/check_road_dimensions.py` to validate the recorded samples against
the shared width and to reject stale measurements after image changes. This
is sampled visual QA, not automatic detection of the entire painted road area.
The iPad audit explicitly records two reviewed narrow samples: Ancient Echoes
20 and Twisted Tracks 16 logical units. They are deviations from the target, not
claims of 22–26 compliance. First Rescue uses a 4:3 edit of the retail `103` plate
and retains the legacy asphalt style.
