# Architecture

Volcano is a C11 game with SDL2 as its only runtime library. It plays the
project's own missions and can also import the original Rescue Team 5
scenes. The engine is built so that our content never has to pretend to be
retail content.

## Layers

```
app/        main.c: window, command line, frame loop
game/       session: startup, active world, transactional loading, input
render/     SDL drawing: primitives, map effects, world view, HUD and picking
─────────── nothing above this line is visible below it; nothing below uses SDL
sim/        world, spawning, jobs, navigation, economy, fixed-step update, appearance
content/    archetypes, rulesets, campaign, LevelSpec, mission loader, retail importer
assets/     sprite and font catalog: native sheets, retail declaration importer, images
core/       errors, arenas, strict XML reader, layered asset mounts (VFS)
```

Each layer uses only the layers below it. The Makefile builds `core`,
`assets`, `content` and `sim` into `libvolcano_core.a` without SDL headers on
the include path, so an accidental dependency on the renderer is a compile
error. `build/test_core` links only that library and runs the simulation
headless.

## Data flow

```
missions/NAME/mission.xml ─┐                      content/archetypes.xml
                           ├─► LevelSpec ─► world ─► render
retail level/levelNNN.lvl ─┘   (placements,   (entities, actors,
   via retail_import.c         traffic, effects) fixed ticks)
```

A **LevelSpec** describes a level independently of its file format. It holds
placements (archetype, position, relations), traffic points, map effects, the
mission settings, the asset packs and sprite files the level needs, and any
archetypes the mission defines. The world takes ownership of it.

There are two producers of LevelSpecs. Neither touches the simulation:

- `content/mission.c` reads our XML missions.
- `content/retail_import.c` reads original binaries. It is the only code that
  knows retail type codes, parameter strings (`tree00`, `extra="offroad"`,
  `name="fire"`, "names ending in 2 are large piles") and the proximity
  heuristics that recover relations the binaries leave implicit (valve
  groups, vehicle homes, roof supports). It maps every record to an
  archetype from `content/archetypes.xml`.

## Content

An **archetype** is one kind of object: its class, art, job duration, work
animation, costs, rewards, production and approach. Missions place
archetypes by id and can define their own by extending shared ones. Library
Rescue's field kitchen is `extends="cafe"` with its own title and sprites; the
coaster car in Twisted Tracks is `extends="boulder"`. Changing art never
changes rules, and rules never depend on art names.

A **ruleset** names the systems a mission uses (economy, evacuation, vehicles,
gates, wildlife, goal objectives). The restored missions use `rescue`, and the
original prototype scenes use `prototype`. The simulation checks these named
policies instead of a single "restored mission" flag.

See [content.md](content.md) for every attribute.

## Assets

Asset folders are **mounted** in order: the retail folder, the built-in
`assets/particles`, any `--pack` overlays, then the mission's own packs and
sprite sheets. Higher layers hide lower files of the same name and redefine
sprites of the same name. The HD statics (`make hd-pack`) and the Kenney
characters (`assets/kenney-characters`) are overlays, not copies of the
retail folder. The catalog reads retail atlas declarations through an
importer and our own `*.sprites.xml` natively. Lookups go through a hash
index.

## Simulation

- **World.** Storage is sized from the level: entities, actors with
  per-actor path buffers, and a compressed adjacency list of the traffic
  graph. A world is a few kilobytes plus its placements. The previous design
  used a fixed 44.7 MB.
- **Entities** refer to their archetype and store state: position, pose,
  direction, job flags and relations. They hold no sprite strings. Lots and
  producers swap archetypes when built or demolished. Rebuilding the producer
  a mission placed restores the mission's variant.
- **Placement.** An object with a base (headquarters, buildings, lots,
  services, valves, obstacles, supplies) is positioned by its base centre.
  Its selection ring is centred on that point, and its sprite is drawn with
  the body's horizontal geometric centre on it, so every state of a building
  sits in the middle of its ring. Only the height of the ground line comes
  from the sprite's pivot. Actors and scenery are positioned by their pivot.
  `archetype_sprite_origin` is the one rule; drawing and picking both use it.
  Retail coordinates are pivot positions, so the importer converts them to
  base centres.
- **Appearance** (`sim/appearance.c`) derives the sprite from archetype,
  state and pose, and measures the footprint from that sprite. The renderer
  draws the selection ring from the same footprint the navigator uses, so
  where a worker stops always matches the ring the player sees.
- **Navigation** runs Dijkstra over the precomputed adjacency. Live obstacles
  block segments within 20 units, and the job's own target is exempt. For
  footprint targets, the route is cut where it first enters the ellipse.
- **Jobs.** The scheduler reserves a worker, spends the job's cost exactly
  once, moves the worker, times the work and starts the return. Completion
  callbacks per class apply the effect.
- **Time.** `game_update(seconds)` accumulates real time and advances in
  fixed 1/60 s ticks, at most 0.1 s per call. Visual effects derive from time
  and seeds, so pause and restart are deterministic.

## Session and failure handling

`game_init*` builds and validates a complete candidate world, including the
mission's art layer. Only on success does it replace the active world. On
failure the active world keeps running and its art layer is restored. Errors
are reported as `file:line: message` through one error slot (`last_error`)
and never printed by library code.

## Tests

| Suite | Needs | Covers |
| --- | --- | --- |
| `make test-core` | placeholder pack | XML, mounts, catalog, content inheritance, every mission, headless footprint approach. No SDL |
| `make test-missions` | placeholder pack | All six missions through the real UI and scheduler; world isolation and rollback; coaster debris lifecycle |
| `make test-tools` | — | Mission checker and retail level reader |
| `make test-retail ASSETS=…` | retail folder | Original campaign 1–6, prototype scenes, retail art and binaries |
| `make sanitize` | placeholder pack | Retail-free suites under ASan and UBSan |

`tools/make_test_pack.py` generates the placeholder pack: flat-colour sprites,
fonts and GUI atlases under every name the engine and missions use. CI and
fresh checkouts therefore never need retail data.
