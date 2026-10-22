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
