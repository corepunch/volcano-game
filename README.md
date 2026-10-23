> **Rescue Team 9: Evil Genius Collector’s Edition** is available for art
> reference only; no gameplay import is planned. Run `make reference` (or `make run-rt9`) to extract and open
> the local RT9 scene/atlas viewer. See [RT9 setup and findings](docs/rescue-team-9.md).
> Existing playable missions below still use the RT5 compatibility importer.

# Volcano

A disaster-rescue strategy game in **C11 + SDL2**. Clear debris, repair
producers and send workers out to bring people and animals home. It plays the
project's own missions and can import the original **Rescue Team 5** scenes
from a local copy of that game.

All of the game's own content is plain data: object kinds, prices, rules and
missions are XML files, and art comes from mountable asset packs. Retail Rescue
Team files are one optional asset source and import format. They are never the
engine's data model.

## Quick start

Requires a C11 compiler, `make`, `pkg-config`, SDL2 and Python 3 with Pillow
(Pillow is only needed for tools and tests).

Use native SDL2, not SDL3 or `sdl2-compat`. On macOS, Homebrew's `sdl2`
may resolve to the SDL3-backed compatibility layer. Run `make sdl2` once to
build checksum-pinned upstream SDL2 2.32.10 into `build/sdl2-native`; subsequent
builds use that static library automatically, including sanitizer builds and
app bundles. This needs `curl`, a compiler and network access on first download.
An existing native static installation can be selected with
`make SDL2_CONFIG=/path/to/bin/sdl2-config`. System native SDL2 via `pkg-config`
remains supported. `make clean` also removes the local SDL2 build.

```sh
make                                   # build/volcano
make test                              # retail-free test suites
make run MISSION=library-rescue        # needs the retail assets, see below
```

The game draws shared sprites (HUD, fonts, headquarters, workers, original
obstacles) from the original Rescue Team 5 resources. They are not part of
this repository:

```sh
mkdir -p reference/rescue-team
rsync -a --exclude='.DS_Store' "$HOME/Documents/Rescue Team/" reference/rescue-team/
```

`reference/` is ignored by Git. You can also pass `--assets PATH` or set
`RESCUE_TEAM_ASSETS`. Without retail assets, every test still runs on a
generated placeholder pack (`make test`).

## Running

```sh
./build/volcano --mission missions/emerald-falls/mission.xml
./build/volcano --campaign 3              # original campaign position 1-50
./build/volcano --level 19                # original level file number
./build/volcano --pack build/packs/hd --hd --mission missions/forest-mayday/mission.xml
```

| Option | Meaning |
| --- | --- |
| `--assets DIR` | Base asset folder (default `$RESCUE_TEAM_ASSETS` or `reference/rescue-team`) |
| `--pack DIR` | Mount another asset folder above it; later packs win |
| `--content DIR` | Content folder (default `content/`) |
| `--hd` | Prefer `background@2x.png` mission backgrounds |
| `--frames N`, `--screenshot FILE.bmp`, `--hidden`, `--software` | Headless runs and captures |

| Make target | Does |
| --- | --- |
| `make run-NAME` (or `make NAME`) | Play `missions/NAME`, e.g. `make run-twisted-tracks`; `make missions` lists names |
| `make run-NAME-hd`, `make NAME-app`, `make test-NAME` | The same mission with HD art, as a macOS bundle, or its regression |
| `make run MISSION=NAME` | Same as `run-NAME` |
| `make run-campaign CAMPAIGN=N` | Play an original campaign mission |
| `make hd-pack`, `make run-hd MISSION=NAME` | Build the HD statics overlay and play with it |
| `make app [APP_MISSION=NAME]` | Self-contained macOS bundle in `build/` |

**Controls.** Click debris, supplies, buildings or animals to send a worker.
Click a queued job again to cancel it.

- **Space / P:** pause or resume
- **R:** restart
- **Enter / N:** next level after a campaign victory
- **F1:** show traffic links and worker paths
- **Escape:** close the construction menu, or pause

## Missions

**After the Eruption** is a five-map episode: outskirts, suburbs, town square,
cut-off district and city centre. Run `make run-volcano-episode`, or build its
self-contained macOS app with `make volcano-episode-app`. After victory, click
**Next level** or press **Enter/N**; **Play again** and **R** replay the current
map. The final map offers replay.

The episode has 7/9/11/13/16 paid route jobs and **no loose or recurring resource
pickups**. Repair field kitchens, timber yards and district offices for automatic
income; salvaged roof beams yield wood. Difficulty comes from route dependencies
and repair priorities. See [episode design and verification](docs/volcano-episode.md).

| Mission | Theme |
| --- | --- |
| `first-rescue` | Two-route starter: earn wood from a tree, repair a bridge |
| `emerald-falls` | Forest and waterfalls; three branches tie timber, food and money together |
| `forest-mayday` | Airliner crash in a forest; restore a cafe, forestry shed and ranger house |
| `ancient-echoes` | Collapsed temple; makeshift tracks through shattered colonnades |
| `twisted-tracks` | Wrecked roller coaster; ten themed debris jobs |
| `library-rescue` | Collapsed library; field kitchen, salvage and logistics stations; project-rendered characters |
| `volcano-01-outskirts` | Episode 1/5: four residents, two stations and seven route jobs |
| `volcano-02-suburbs` | Episode 2/5: district office, heavy basalt and a stranded recruit |
| `volcano-03-square` | Episode 3/5: inner streets, two approaches and eleven route jobs |
| `volcano-04-district` | Episode 4/5: reconnect roads around permanent cooled lava |
| `volcano-05-centre` | Episode 5/5: sixteen route jobs, eleven people and two animals |

Each mission is a folder under `missions/` with its `mission.xml`, background,
art and provenance notes. Mission-specific objects are archetypes that extend
shared ones. Library Rescue's field kitchen extends the cafe, so it has the
cafe's rules with its own title and art. Twisted Tracks' derailed car extends
the boulder. See [docs/creating-missions.md](docs/creating-missions.md) to write
your own, and check one with:

```sh
python3 tools/mission.py check missions/NAME/mission.xml
```

## Gameplay

Workers leave headquarters along the authored traffic network, which links
points closer than 50 units. Live obstacles block any link passing within 20
units. A building's or obstacle's position is the centre of its base: its
selection ring is centred there and its art is centred on it horizontally,
with the sprite's pivot setting only the ground line. A worker walks up to the
edge of an obstacle or building, stopping on
the selection ring you see on hover, faces it and works. Then it goes straight
back to headquarters. Supplies are picked up on arrival and credited once
carried home. Survivors walk home by themselves when a route opens. Workers
stranded far from headquarters join the crew once they reach it.

Producers that are working pay 5 units every 5 seconds. They also offer a
10-unit bonus at their linked supply point every 10 seconds while none is
outstanding. Damaged producers cost half their construction price to repair.
Lots open a four-button construction menu (Sawmill and Burger Cafe 40 wood,
House 60, Gas Station 80). A working producer can be demolished to free its
lot.

| Job | Cost | Result |
| --- | --- | --- |
| Tree | 20 food | +20 wood |
| Stone / large stone | 20 food / 40 food + 100 money | Opens the route |
| Wooden bridge / large bridge / road | 20 wood / 20 food + 40 wood / 20 wood | Opens the route |
| Animal capture | 10 food | Rescued when led home |
| Valve | 20 money | Clears its fog or pollution group |
| Pier or garage repair | 40 wood | Enables the boat or ATV |
| ATV or fishing trip | 10 fuel | Recruit delivered, or food caught |

**These prices and intervals are this restoration's balance, not recovered
retail constants.** They live in `content/archetypes.xml`.

### Original campaign

Positions 1–6 use the restored rescue rules. The campaign order is recovered
from the original map's prerequisite chain, starting with `level015`,
`level018` and `level019`. Starting stock and goals are in
`content/campaign.xml`. Levels 1–3 match the original screenshots and the
[GameHouse walkthrough](https://www.gamehouse.com/blog/2015/10/rescue-team-5-walkthrough/).

| Campaign level | Objectives | Starting food / wood / money / fuel |
| --- | --- | --- |
| 1 | Rescue 3 people | 20 / 0 / 0 / 0 |
| 2 | Rescue 3 people | 50 / 30 / 20 / 0 |
| 3 | Rescue 5 people | 10 / 100 / 30 / 0 |
| 4 | Rescue 13 people, including 5 workers requiring the ATV | 60 / 80 / 70 / 70 |
| 5 | Rescue 6 people and 16 animals | 70 / 80 / 40 / 0 |
| 6 | Purify 2 basins and rescue 2 people and 6 animals | 60 / 90 / 40 / 220 |

Level 4 sends the ATV from its garage to offroad workers. Level 5 adds roaming
animals and fog cleared through valves. In level 6, valves purify basins and
unlock piers; boats fish within their basin, and the cafe cannot be built.
Positions 7–50 and `level000` run the original prototype interactions:
clicking survivors escorts them, and the level ends when every job is done.
Timed stars, upgrades, save profiles and later campaign systems are not
implemented.

Scenery from the original files is preserved. Birds and clouds cross the
sky, floating objects bob, and rooftop survivors ride their flooded house.
Logs named `fire` burn with CC0 flame and smoke textures from
[`assets/particles`](assets/particles/README.md).

## Art and asset packs

The game looks up every sprite by name in a catalog built from mounted asset
folders, from lowest to highest precedence:

1. the retail folder (`--assets`)
2. `assets/particles` (fire and smoke)
3. `--pack` folders
4. the mission's own `<pack>` folders and sprite sheets

A higher layer replaces sprites of the same name. Two packs come with the
project:

- **Kenney characters** (`assets/kenney-characters`): workers and civilians
  rendered from an editable Blender rig in eight directions. They replace the
  retail character sprites by name. Library Rescue mounts this pack itself.
  See the [character rendering guide](docs/rendering-characters.md).
- **HD statics** (`make hd-pack` → `build/packs/hd`): every declaration in
  both retail building atlases (all production tiers, emergency facilities,
  docks, stations, garages, bases, landing pads and the construction lot, plus
  their animated flags, valves and beacons), and every debris and supply
  sprite. In total that is 61 sprites and 129 authored frames at twice the
  texture resolution, with authored ground shadows. Every building state has
  an explicit ground-footprint pivot, so its painted base stays centred on its
  selection ring across damage, construction and upgrades. The sources,
  prompts and manifests are in [`assets/hd-static`](assets/hd-static).
  `make test-hd` checks that all 101 original building and attachment frames
  use the HD texture with their original timing and body bounds.
  `make hd-pivot-previews` renders every packed state with its pivot, bounds
  and ring. This pack is built from the retail declarations, so it needs
  `ASSETS`.

All eleven missions use a 1368×1026 (4:3) playfield. `--hd` selects a
`background@2x.png` export when present, without changing world coordinates.
The older missions' 2× exports are interpolated; the eruption episode uses its
native ImageGen plates. See the [iPad comparison and checks](docs/ipad-levels.md).
See the [background guide](docs/creating-backgrounds.md) for generation and
placement.

## Architecture

```
app → game → render → sim → content → assets → core
```

The layers below `render/` build without SDL headers into
`libvolcano_core.a`. Our missions and retail scenes both become a
format-neutral `LevelSpec`. `content/retail_import.c` is the only code that
knows retail type codes and naming heuristics. The simulation stores state,
not sprite names, and advances in fixed 1/60 s ticks. Loading is
transactional: a failed load leaves the running level untouched. See
[docs/architecture.md](docs/architecture.md) and the
[content reference](docs/content.md).

## Verification

```sh
make test                 # core (no SDL), all missions on placeholder art, tool tests
make sanitize             # the same under ASan/UBSan
make test-retail          # original campaign and retail art (needs ASSETS)
VOLCANO_TEST_SHOTS=1 make test   # also save frames to build/
```

- **Core suite:** the XML reader, mounts, sprite catalog, content inheritance,
  every mission file, and headless checks that workers stop at the footprint
  edge facing their target.
- **Mission suite:** plays all six missions through normal clicks with three
  repair orders each, without injecting resources. It checks gates, exact
  budgets, queued jobs, production, recruits, victory, pause and restart. It
  also covers world isolation, rollback after a failed load, and the coaster
  debris lifecycle.
- **Retail suite:** reads all 52 original binaries to their exact end,
  completes campaign levels 1–6, and exercises construction, vehicles,
  valves, fishing, fire, scenery and draw order.

## Tools

| Tool | Purpose |
| --- | --- |
| `tools/mission.py check` | Validate missions and warn about likely design problems |
| `tools/retail_levels.py` | Dump or analyze original level binaries |
| `tools/make_test_pack.py` | Generate the placeholder asset pack used by tests |
| `tools/build_hd_assets.py`, `tools/preview_hd_pivots.py`, `tools/check_hd_building_parity.py` | Build, review and verify the HD statics overlay |
| `tools/pack_rescue_characters.py` | Pack rendered characters and write their sprite sheet |
| `tools/package_app.py` | Self-contained macOS bundle (`make app`) |
| `tools/audit_rescue_team.py` | Audit report of original campaign coverage |

## Scope and provenance

The restored gameplay follows the former `samples/Volcano` prototype, first
introduced in ORCA commit `09a68b1d` (2026-02-25). Its later movement and work
callbacks were commented out. This project restores those interactions in C
and reads the original binary levels instead of the archived Lua level table.

The installed resources identify themselves as Rescue Team 5 in
`conf/locale_en.lua`. Original artwork and data stay in the ignored
`reference/rescue-team/` directory and are not redistributable; a shipping
build needs its own replacements for the shared sprites. All 50 original
missions load and render. Unknown retail controllers stay in the parsed data
without being mistaken for missing art, and unknown sprite references fail
explicitly.

The image decoder is `stb_image.h`, copied with its license intact from ORCA.
All application sources are `.c` and `.h` files; SDL2 is the only external
runtime library.

## Shared road and bridge dimensions

`assets/road-dimensions.json` defines a 24-pixel clear road width and an
88-pixel bridge deck span on the 1366 × 768 logical canvas. The authored
horizontal and vertical bridge states share those world dimensions; Retina
packing doubles texture density. `make hd-pack` installs the asset tools in
`build/python-venv` using `requirements-assets.txt`.
