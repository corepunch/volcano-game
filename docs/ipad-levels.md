# iPad playfields and Rescue Team 5 comparison

Research date: 2026-10-22. Baseline: `level-balance-before.json`; regenerate the
current inventory with `python3 tools/compare_missions.py` (requires local RT5).

## Display and 2D asset policy

There is no single iPad resolution. Apple's current specifications list landscape
Pro 13 at 2752×2064 (4:3), Pro 11 at 2420×1668, Air 13 at 2732×2048, and
Air 11 at 2360×1640. Physical pixels are not logical input points.

Use a 1368×1026 logical playfield (exact 4:3), uniform aspect-fit presentation,
and a complete background in the same aspect ratio. Preserve sprite proportions,
road widths, route distances and the association of art with collision geometry.
Extend scenery rather than stretching a 16:9 image. Keep important controls and
playable targets away from rounded screen corners and the home indicator.
Maintain high-resolution raster masters separately from world coordinates; use
texture atlases for sprites, preload/cache textures and avoid per-frame decoding.
A 2736×2052 RGBA background uses about 21.4 MiB before mipmaps; release textures
when their level is no longer needed. Retina pixels do not require larger world
coordinates or movement speeds.

Apple recommends aiming for 44×44-point tap targets and 17-point body text on
iPad. Verify at the smallest intended window, including resizes; physical-device
touch, safe-area and memory validation remain part of an eventual iPadOS port.
This repository currently builds a desktop SDL2 game, not an installable iPad app.

Sources:
- [Apple iPad Pro specifications](https://www.apple.com/ipad-pro/specs/)
- [Apple iPad Air specifications](https://www.apple.com/ipad-air/specs/)
- [Apple: Design advanced games, WWDC24](https://developer.apple.com/videos/play/wwdc2024/10085/)
- [Apple: Layout](https://developer.apple.com/design/human-interface-guidelines/layout)
- [Apple: Maximizing texture performance](https://developer.apple.com/documentation/spritekit/maximizing-texture-performance)

## Baseline gameplay

| Custom mission | Objects | Paid route obstacles | Producers | Pickups | People goal | Animals |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| First Rescue | 7 | 2 | 0 | 1 | 2 | 0 |
| Emerald Falls | 28 | 5 | 3 | 8 | 7 | 2 |
| Forest Mayday | 31 | 5 | 3 | 8 | 10 | 2 |
| Ancient Echoes | 30 | 6 | 3 | 7 | 9 | 2 |
| Twisted Tracks | 35 | 10 | 3 | 7 | 10 | 2 |
| Library Rescue | 31 | 6 | 3 | 7 | 10 | 2 |

RT5's first six campaign scenes contain respectively 6/7/10/18/24/8 barrier
records, 0/4/3/3/3/3 producers, 7/10/10/10/11/26 resource records, and
0/0/1/3/6/2 construction lots. Scenes 4–6 introduce ATV access, valve/fog gates
and separate boat networks. These counts come directly from local binaries
`level015`, `018`, `019`, `012`, `013`, `011`, in campaign order.
Scenery/effects are excluded. The first scene includes garage/ATV records that
are not active early-campaign mechanics; record presence alone is not gameplay.

A barrier is a job, not necessarily a distinct puzzle. Count dependency chains,
alternative routes, renewable-resource decisions, service prerequisites and
worker contention separately. Most existing disaster missions repeat three
producers plus an expensive money gate, despite their different paintings.
Increasing civilian counts alone adds little strategic difficulty because
ordinary civilians evacuate automatically once a route opens.

The [GameHouse RT5 guide](https://www.gamehouse.com/blog/2015/10/rescue-team-5-walkthrough/)
describes Normal, Multi-click and untimed Relaxed modes. Our scheduler is queued
and untimed: comparisons are closest to Relaxed mode. Neither binary counts nor
automated completion establish equal human difficulty or timed-star parity.

## Engine verification

The canvas regression exercises 1368×1026 rendering, world clicks, top-edge
pause controls, centered Continue, and loading a different canvas. Invalid
dimensions are rejected by the native XML loader. Previous-level texture caches
are released after a successful world transition; a failed load preserves the
active world. All six custom mission suites pass. The retail campaign-6 test
stalls identically on the baseline commit and the modified engine (one of two
people, zero of six animals, two purified basins); this pre-existing issue is
not evidence of successful retail parity.

## Background delivery

All six missions now use 1368×1026 world coordinates and 4:3 PNG backgrounds.
The six edits and source history are recorded in `art-source/ipad/manifest.json`.
Native ImageGen output is 1448×1086. Exports at 1368×1026 and 2736×2052 use sips
resampling; the larger export does not imply additional generated detail.
Five maps translate routes, placements and effects together by (1,129), retaining
all routing distances. First Rescue uses x+1, y*1.10+165 to match measured canyon
landmarks. Full playthroughs and start/repaired renders were checked on retail art.
The HUD is above all gameplay, and the bottom scenery provides home-indicator room.

The road-caliper audit now uses each mission's logical dimensions. It records
reviewed narrowing in two painted corridor samples (20 and 16 units); these are
visual paths rather than tap targets. Their original 24±2 calipers are archived,
not reused against new art. First Rescue retains its legacy asphalt style.

## Rebalanced campaign

Current machine-readable inventory: `level-balance-after.json`.

| Mission | Objects before → after | Paid jobs before → after | New decision/dependency |
| --- | ---: | ---: | --- |
| First Rescue | 7 → 18 | 2 → 6 | Eastern fork, third evacuee, finite food/wood caches |
| Emerald Falls | 28 → 31 | 5 → 8 | Longer branches and resource reservation across three producers |
| Forest Mayday | 31 → 37 | 5 → 10 | Construction lot: choose and fund a money-producing house |
| Ancient Echoes | 30 → 35 | 6 → 11 | Staged approach to the expensive rescue gate |
| Twisted Tracks | 35 → 43 | 10 → 18 | More competing clearing jobs on the existing service-road loop |
| Library Rescue | 31 → 41 | 6 → 16 | Salvage money finances logistics repair before final excavation |

All six explicitly require clearing evacuation routes as well as rescuing people
and animals. The HUD exposes this goal; victory cannot leave unpaid route jobs
behind. Existing debris atlas artwork is reused. Forest Mayday includes an
extra reachable timber cache so choosing the house before repairing the cafe
cannot exhaust the recovery budget. The lot can still be rebuilt if a different
producer was selected.

All six pass normal-click playthroughs with placeholder and retail art. Production
missions pass three repair orders; the zero-food recovery and expensive-gate
reservation checks remain active. Simulated completion times for the five larger
maps are approximately 153–172, 180, 186, 276 and 221–230 seconds respectively.
These are automated scheduler times, not estimates of human completion time.
The dedicated coaster suite clears all eighteen jobs and verifies all eight
atlas variants, complete removal and one-time charging.

This raises job density and resource planning toward early RT5 Relaxed levels.
It does not reproduce all of RT5's distinct puzzles: boats, ATV transport and
valve/fog networks are absent from this custom campaign. Equal human difficulty
still needs timed sessions with players; obstacle counts alone cannot prove it.

## Native SDL2 validation

The project-local dependency is upstream SDL2 2.32.10, built statically by
`make sdl2` with a pinned SHA-256 archive. Make rejects the detected Homebrew
`sdl2-compat` provider. `otool -L build/volcano` confirms no SDL3 or SDL2-compat
runtime dependency. The C11 warning and dependency flags also apply to sanitizer
builds, preventing stale object layouts after content-header changes.

On native SDL2, `make test`, all six mission suites with retail sprites, and
AddressSanitizer/UndefinedBehaviorSanitizer pass. The viewport regression checks
six window sizes (1024×768, 1376×1032, 1210×834, 1180×820, 1133×744 and 768×1024)
for complete centered aspect-fit and matching input coordinates. These are
desktop window tests, not physical iPad or iPadOS certification. The pre-existing
retail campaign-6 failure described above remains reproducible on native SDL2.
