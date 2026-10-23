# City after the eruption

Five classic Rescue Team maps: Outskirts, Suburbs, Town Square, Cut-off
District and City Centre. The eruption has already happened. Each map is a
resource-and-route puzzle, with the familiar workers, paid clearing, production
repairs, bridges and automatic evacuation. No moving lava or invented transport.

## Reference and density

Inspected the local RT5 binaries using `tools.retail_levels.decode/analyze`.
Campaign order comes from the original map prerequisites, not file numbering.

| Campaign position | Source | Barriers | Resource records | Building records | Civilians |
| --- | --- | ---: | ---: | ---: | ---: |
| 1 | level015 | 6 | 7 | 0 | 3 |
| 2 | level018 | 7 | 10 | 4 | 2 |
| 3 | level019 | 10 | 10 | 3 | 3 |
| 4 | level012 | 18 | 10 | 3 | 8 |
| 5 | level013 | 24 | 11 | 3 | 1 |

These are authored records, not a claim that every original service is active.
Fog is included among barriers; recruits and animals are separate. Background
decoration is excluded. The fifth original map also has sixteen animals.

Our progression targets 7 / 9 / 11 / 13 / 16 paid route jobs, 2–3 production
repairs, two starting workers and small groups of survivors across several
branches. The first map is a complete level rather than a seven-object demo.
Later maps add route choices and a third worker, not dozens of extra pickups.

## Resources without a collection trail

There are **zero loose pickups and zero recurring output pickups** in all five
maps. Production is credited automatically by the existing economy. A repaired
field kitchen supplies food, a timber yard supplies wood and a district office
supplies money. Salvaging wooden wreckage yields wood directly as existing tree
jobs already do. Starting stock is carried by the rescue team.

Budget every reachable stage. A kitchen must be repairable before starting food
can be exhausted irrecoverably; wooden salvage must fund renewable production.
Provide alternatives in the middle maps, then combine the established systems
in the finale. Production waiting should overlap with work on other branches.

## Art and delivery

Five distinct 4:3 painted maps on the live 1368×1026 canvas. Fixed elevated
camera, small-town streets, ochre walls, teal roofs, ash and permanent cooled
lava formations. Ordinary rescue paths have a 24-pixel clear surface. Static
town blocks and terrain are painted; workers, production buildings, obstructions
and rescues remain runtime sprites. One shared transparent atlas contains all
new props and independently authored damaged/repaired station states.

Each map must have an ordinary-click regression, initial rescue gate checks,
alternative repair priorities, resource recovery, pause/restart and start,
repaired and completed renders. The episode must launch as one sequence, with
next-level navigation and a replay at the final map.
