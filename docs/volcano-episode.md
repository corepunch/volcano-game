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

## Implemented episode

| Map | Folder | Route jobs | Stations | People (including recruits) | Animals |
| --- | --- | ---: | ---: | ---: | ---: |
| Outskirts | volcano-01-outskirts | 7 | 2 | 4 | 0 |
| Suburbs | volcano-02-suburbs | 9 | 3 | 6 | 1 |
| Town Square | volcano-03-square | 11 | 3 | 8 | 1 |
| Cut-off District | volcano-04-district | 13 | 3 | 9 | 2 |
| City Centre | volcano-05-centre | 16 | 3 | 11 | 2 |

Each map starts with two workers, 40 food and 20 wood. Maps 2–5 have one
stranded worker. Kitchens and timber yards produce 5 every 5 seconds; offices
produce 10 money every 5 seconds to keep heavy-basalt work moving. All route
jobs are objectives, so alternate access does not leave street restoration
irrelevant. Route repairs use broken paving, not bridges across painted land.

The first two maps branch from a small supply foothold. The square and centre
include interior loops; the district has two approaches around a permanent lava
island. Clearable jobs are separate from the permanent volcanic formations.
There is no collection trail and no output pickup at any station.

`next` links in mission XML connect all five maps. Their short `briefing` messages
replace the default collection instruction. Last-map victory offers replay.
`make run-volcano-episode` starts the sequence; `make volcano-episode-app`
packages `build/Volcano 01 Outskirts.app`, including the shared prop atlas.

Tests: `make test` covers all eleven missions. The `--volcano` regression runs
three repair orders per map, initial rescue gating, depleted-food recovery,
nonnegative stock, every objective, pause and restart. The first-map test also
plays the whole linked sequence, checks replay/next buttons and keyboard
progression, final-map replay and recovery from a missing next file. No episode
playthrough injects resources or completes jobs directly.

Render QA: `VOLCANO_TEST_SHOTS=1 make test-missions
MISSION_ASSETS=reference/rescue-team` writes start/repaired/completed screenshots
to `build/`. Actual source-pixel roads and station pads are recorded in
`art-source/volcano-episode/measured-layouts.json`. Sampled road widths include
reviewed painterly deviations from the 24-pixel target in
`art-source/road-width-measurements.json`; these are documented rather than
presented as exact compliance. All ten prop/state images use the single authored
ImageGen atlas directly. Exact prompts are alongside each background and atlas.

Verification outcome: `make test`, `make sanitize` and all eleven mission
regressions on the retail artwork pass. The separate full `make test-retail`
stalls on original campaign level 6 (one person, no fishing trips). Rebuilt and
ran commit `08a1059` in a temporary checkout with the pre-navigation engine:
the same level stalls with identical counts and inventory. This is a pre-existing
boat-level regression, not a passing check or an episode dependency.
