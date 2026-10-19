# Interior roads and themed obstacles

Use these principles for disaster scenes where a large wreck or ruined structure
is the visual focus. They describe the approved Twisted Tracks direction, not a
requirement to turn every mission into an amusement park.

## Design playable space before preserving wreck geometry

Plan a readable ground-road network with meaningful jobs, junctions and rescue
pockets inside the scene's center as well as its edges. A visually large wreck
does not make that space playable. Check where workers actually travel and where
objectives sit; a ring of perimeter roads around an inaccessible center is a
warning sign. Keep existing visitor/service-road cues: worn gravel, paving
fragments and occasional damaged curbs, rather than broad new sandy shortcuts.

If the current painting prevents interior routes, reorganize the ruin decisively.
Move tall drops, dense track tangles and foundations into substantial islands
between roads. Retain the structure's identity, scale and rich painted lighting,
but let branches reach ground-level clearings within its footprint. A large
recognizable loop can frame an interior rescue pocket. Shrinking the entire
disaster to one edge or clearing vast empty lawns loses the intended atmosphere.
Do not route a road through a solid foundation or paint dirt over a steel rail.

Keep the selected game's formula. In the classic ground-road clearing mode,
workers reopen established roads by removing discrete debris. Ladders, new
catwalks, climbing and building routes over the wreck are different mechanics;
use them only when the user chooses that scope. Repairing an existing bridge is
not evidence of a game supporting construction of arbitrary new traversal.

## Separate scenery from clearable jobs

Permanent terrain, vegetation, integrated shadows and large static wreck islands
belong in the background. Workers, survivors, supplies, buildings and removable
obstructions must not be painted into the production background. An approved
concept can contain these for explanation; make a clean production plate that
preserves its composition and roads while removing all temporary actors/jobs.
Inspect every removal site and preserve the road underneath. Do not merely place
a sprite over a painted obstacle: clearing would leave the same obstruction.

Match jobs to the environment. For a coaster, short detached red track panels,
yellow support fragments with concrete collars, torn enclosure gates, cable or
wheel-assembly debris and a detached blue car provide variety. The shape, paint,
shadow and nearby broken source should explain why each piece is there. Avoid
repeating generic stone/log piles in a scene with more appropriate debris.
A compact broken fragment is a plausible ordinary clearing task; an entire
intact tower or loop should remain scenery. Different visual families can use
the same ordinary worker/queue/resource rules without new tool inventories.

Generate every custom prop in the set together in one transparent atlas call.
When authored visual states are needed, include all those states in that call;
do not synthesize their appearance with post-processing. Cropping/packing and
atlas metadata may reference the authored artwork without redrawing it. Save the
exact prompt and source atlas. Inspect actual alpha, cell bounds, scale and
ground pivots; transparent padding is not the visible obstacle footprint.
Custom sprite presentation must preserve independently defined job cost, reward,
duration, collision, click bounds, queue state and complete removal on clearing.

The restoration supports this through a separate type-5 `effect` with a nonzero
`link_id` naming the base/barrier/building. Keep the target's ordinary gameplay
`params` (for example `tree00` for salvageable timber or `stone00` for a small
clearance job). The linked record uses
`sprite;image=@debris.png;rect=x,y,w,h;pivot=.5,.65;stage=world`, its measured
`scale_x/scale_y`, and an optional `name` for the tooltip title. The target owns
position, layer, costs and lifetime; there is no passive duplicate of the prop.
World-stage rigid presentations require zero wind/rotation. See the current
`docs/creating-levels.md` section on authored stationary-object artwork before
using building states. Do not implement a parallel sprite/obstacle system when
this presentation mechanism already fits the task.

## Check the layout as a puzzle

Put each blocker across a narrow road throat, with the same old road visible on
both sides. Wreck/fence edges should explain why walking around it is impossible.
Terrain does not enforce collision: audit the actual distance-based traffic
graph. In a road loop, gate every bypass or intentionally leave a branch
optional; do not assume a painted obstacle locks a rescue pocket.

Measure the finished plate. Keep separate branches at least 50 world pixels
apart unless an intersection is intended; gates block segments within 20 pixels.
Check worker approach and return, nearest-node attachments, all gated survivors,
resource-depleted recovery and alternative spending orders. Compare start and
cleared renders: sprites should disappear fully, revealing intact traversable
road without removing adjacent static wreck. Keep HUD and building footprints
clear of jobs and road centers. A concept image alone proves none of this.
