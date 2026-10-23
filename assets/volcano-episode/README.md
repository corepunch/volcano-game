# Volcano episode art

`props.png` is one RGBA atlas generated with the built-in ImageGen tool.
All ten props and damaged/repaired states were authored together in that one
call; the game uses their original pixels directly. `sprites.xml` only selects
their source rectangles, scale and ground pivots. No state was synthesized.
The exact prompt is `props.prompt.txt`.

The atlas contains the field kitchen, timber yard and district office (two
states each), timber wreckage, ash-coated rubble, heavy basalt and wall debris.
Producers use automatic income without linked pickup sprites.

Five separate ImageGen backgrounds and exact prompts live in
`missions/volcano-01-outskirts/` through `missions/volcano-05-centre/`.
`art-source/volcano-episode/art-manifest.json` records source dimensions,
rectangles and hashes. Layout proposals are in `layouts.json`; playable
coordinates must follow the measured painting rather than that initial brief.
