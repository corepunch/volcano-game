# Architecture review — October 2026

> **Status (implemented).** The review below led to the current design in
> [architecture.md](architecture.md). Data uses XML rather than the JSON
> sketched below. Done:
>
> - phases 0–3 (separate test runners, a retail-free placeholder pack,
>   layered `libvolcano_core`, data-defined archetypes/rulesets/campaign,
>   native XML missions, the retail importer as the only retail-aware code,
>   mounted asset packs)
> - most of phase 4: world storage sized from the level, string-free poses,
>   fixed ticks, a precomputed route graph, and footprint-edge approach for
>   workers
> - a self-contained app packager
>
> Still open:
>
> - entities are leaner but not split into components
> - mission scenario tests are C with per-mission flags rather than data
> - large binaries are not yet in Git LFS
> - no CI workflow is configured
> - the retail suite (`make test-retail`) has been ported but must be run
>   where the retail assets are available
>
> The text below is the original review, kept as written.

**Verdict:** the C modules are reasonably clean, but the system around them is
still a prototype. The core problem is that **the retail Rescue Team 5 data
model is the engine's canonical model**, and everything we author ourselves is
squeezed through it. The "custom buildings" in Library Rescue, the coaster debris
in Twisted Tracks, and the HD and Kenney character art are all workarounds for that
one inversion. Until it is reversed, every new mission adds another special case.

The good parts are worth keeping. These include transactional world loading,
an explicit `Game *` context, the `JobDef` callback table, strict
validating parsers, and regressions that drive real input. The rest of this
document covers what is wrong, why it matters for a shippable product, and an
incremental path to fix it.

---

## 1. Root cause: our content goes through a format we don't own

```
levels/*.level.json ──(tools/level_source.py)──▶ retail v301 .lvl  +  .lvl.rules sidecar  +  sibling PNGs
                                                     │
                     src/level.c (retail reader) ◀───┘
                                                     │
                     src/game_spawn.c: retail type codes + sprite-name strings ──▶ gameplay
```

The JSON source is the real authoring format. However, the runtime only reads
the retail binary, so anything retail can't express gets encoded as:

* **Stringly-typed `params` on type-5 "effect" records.** One example is
  `"sprite;image=@library-rescue-props.png;rect=380,110,345,280;pivot=0.5,0.7;state=default"`.
* **Placeholder transforms.** The library presentations use `"x": 0, "y": 768`
  because their position comes from the linked target.
* **A `.lvl.rules` sidecar with exactly seven integer keys**
  (`src/game_spawn.c:46`). It has no way to express prices, rewards, building
  types or obstacle behaviour.
* **Sibling images referenced with an `@` prefix**, resolved relative to the
  `.lvl` path.

### What "custom buildings" actually are

In Library Rescue, the *Field kitchen*, *Salvage supply station* and
*Logistics station* are **retail `burger` / `sawmill` / `house` buildings**. Each
has a linked type-5 sprite record that replaces its picture (`bind_presentations`,
`src/game_spawn.c:180`). The debris works the same way in both missions. For
example, Twisted Tracks' *Derailed coaster car* is a retail `stone_big00` barrier:

| What the player sees | What the engine runs |
| --- | --- |
| "Derailed coaster car" (title from presentation) | `stone_big00`: **40 food + 100 money**, worker plays `worker_pick` |
| "Broken maintenance panels" | `tree00`: 20 food, **+20 wood reward**, `burns=true` |
| "Field kitchen" | `burger`: produces food, costs 40 wood, demolishes to a retail lot |

Rules are chosen by **which retail sprite the art is pretending to be**. A
designer cannot create "a coaster car that costs 60 money and gives no wood"
without editing C. Two concrete symptoms follow:

* `levels/twisted-tracks-debris.atlas.json` already describes the sprites, with
  rect, pivot, scale and even a `job` field. **The runtime never reads it.** The
  same rect/pivot/scale is copied into every level record that uses the sprite.
* The demolition path has to detect "the building is no longer its original
  retail type" before it drops the overlay (`src/game_render.c:23`).

---

## 2. Findings

Findings are ranked by impact on getting to a production build.

### 2.1 The product cannot exist without someone else's copyrighted assets — **blocker**

`reference/rescue-team/` is gitignored and copied from a local Documents folder.
Every mission depends on it, our own included. The dependencies cover the HUD,
fonts, `base0`, workers, highlight rings, progress bars, particles and the
campaign map. As a result:

* A fresh clone cannot build its assets, run, or test anything. CI is impossible.
* There is no shippable configuration. The macOS bundle rsyncs the retail tree
  into `Contents/Resources` (`Makefile`, `app` target).

A production architecture has to stand on **its own content**. The retail
missions should be an optional *import* layer, not the foundation.

### 2.2 Gameplay identity comes from sprite-name strings — **high**

The game decides what something *is* from substrings of art names or free-text
fields. This happens in many places:

| Where | Rule |
| --- | --- |
| `game_rules.c:24` `game_obstacle_definition` | `strstr(params, "tree" / "stone" / "bridge" / "road")` → cost, reward, burns |
| `game_rules.c:48` | pickup value = 20 if the sprite name **ends in `'2'`**, else 10 |
| `game_rules.c:49` | `boat_worker` if params == `"boat_wood"` |
| `game_rules.c:18` | unknown building name → silently becomes Gas Station (`return 3`) |
| `game_spawn.c:16-19` | fire if `name=="fire"`; roof if `extra=="roof"`; ATV if `extra=="offroad"`; floating if anim contains `"_water"` |
| `game_jobs.c:172` | work animation chosen by `strstr(obstacle->match, "tree")` |
| `game_render.c:379-389` | tooltip title by `strstr(e->anim, "stone"/"tree"/"house"/...)` |

The README says "rules resolve once at spawn". That is true for costs. It is
not true for titles, work animations, transport or building art, which still
read strings at runtime.

### 2.3 Two rule engines behind one boolean — **high**

`g->early` is true for campaign missions 1–6 and every custom level, and false for
the prototype missions 7–50. The game code branches on it in 37 places. There are
further checks such as `g->campaign<4` (`game_spawn.c:241`) and `g->campaign==1`
(`game_spawn.c:320`). The data for missions 1–6 is hardcoded in C
(`game_apply_mission_rules`, `game_rules.c:55`), while custom missions use the
sidecar. That gives three sources of mission data and two behaviour sets in one
code path.

### 2.4 Art variants are whole-tree copies — **high**

* `make hd-assets` rsyncs the full retail tree into `build/hd-assets`, then
  **rewrites the retail `.lua` declarations** to point at a new atlas.
* `make character-assets` does the same for Kenney characters into
  `build/kenney-assets`.
* Which tree you pass to `--assets` changes the game. Library Rescue is only
  correct under `kenney-assets`, and its test asserts this. The HD build covers
  only two missions, and both use separate `build/hd-levels` copies.
* `tools/pack_rescue_characters.py:147` reads `reference/rescue-team` directly,
  ignoring `--assets`.

Art selection should be a **layered mount of content packs**, not "pick one of
three copies of the entire game data".

### 2.5 Memory layout sized for the worst case of a 2014 file format — **medium**

These sizes were measured with `sizeof` on the current headers:

| Struct | Size | Cause |
| --- | --- | --- |
| `LevelObject` | 8.7 KB | fixed `name[512]`, `params[4096]`, `extra[4096]` |
| `Level` | **35.9 MB** | 4096 × `LevelObject` |
| `Worker` | 32 KB | `path[MAX_TRAFFIC_POINTS+2]` inline |
| `Game` | **44.7 MB** | `Level` + 4096 entities + 144 `Worker`s + `MapScene` |

Every restart callocs a new 45 MB world. `campaign_load` allocates another
36 MB `Level` and parses all 51 levels to recover the campaign order. The game
keeps the whole raw `Level` alive after spawning, because render, sim and
tooltips keep going back to `g->level.objects[e->source]` for `params`, `type`
and `layer`.

### 2.6 Fat entity struct, overloaded worker struct — **medium**

`Entity` has about 45 fields serving 14 kinds: fire state, roof support, income
clocks, roaming, gates, transport, presentation title, pickup data, building
type and more. `Worker` is reused for workers, vehicles and evacuees, so the
`boat`, `atv` and `recruiting` flags select the behaviour. Any new feature adds
fields that every other kind carries and must keep in a valid state.

### 2.7 The simulation is coupled to presentation and globals — **medium**

* The simulation sets `e->anim` strings directly, for example
  `"%s%u_walk%d"` in `game_sim.c:127`. `JobDef` carries a `work_animation`.
  Spawning fails if `animation_find()` can't resolve a retail sprite name.
  This means the simulation cannot run, or be tested, without the art set loaded.
* `asset_root`, `renderer`, `last_error`, the animation cache, the campaign
  order and the single current world are process globals.
* There is a variable timestep: `game_update(fmin(dt, 0.05))` (`main.c:105`).
  Without a fixed tick, results depend on frame timing, so replays,
  deterministic tests, saves and later networking all stay out of reach.

### 2.8 Build, test and packaging are bespoke per mission — **medium**

* The test suites are linked into the **shipping binary**
  (`SOURCES = src/main.c $(CORE_SOURCES) $(TEST_SOURCES)`). `--self-test` is
  part of the product.
* The Makefile has five near-identical copy-pasted blocks, one per mission. Each
  new mission means more Makefile code.
* `tests/check_custom_level.c` takes mission flags (`--coaster`,
  `--library-rescue`, …) and hardcoded entity seeds. A new mission needs new C
  test code.
* The per-level `.app` launchers are shell scripts that `cd` into **this
  checkout's absolute path** (`tools/package_custom_level.py:29`). They only work
  on the machine that built them.
* Git tracks about 130 MB of binaries without LFS. That includes rejected
  backgrounds, concept art, Blender files and zips.

---

## 3. Target architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│ app/          main loop, window, fixed-tick driver, CLI               │
├──────────────────────────────────────────────────────────────────────┤
│ present/      sprite/anim resolution, render, HUD, tooltips, audio   │
│               reads sim state + archetype presentation; never writes │
├──────────────────────────────────────────────────────────────────────┤
│ sim/          pure C, no SDL: world, entities, jobs, routes, economy │
│               fixed 60 Hz tick, deterministic RNG, stable IDs        │
├──────────────────────────────────────────────────────────────────────┤
│ content/      registries: archetypes, buildings, resources, missions │
│               loaded from packs; validated once; referenced by ID    │
├──────────────────────────────────────────────────────────────────────┤
│ assets/       VFS with layered mounts; atlas + font + image loaders  │
├──────────────────────────────────────────────────────────────────────┤
│ import/retail optional: .lvl v301 + Lua decls → native model         │
│               ALL retail heuristics (strstr, type codes) live here   │
└──────────────────────────────────────────────────────────────────────┘
```

### 3.1 One content model, defined by us

Gameplay objects are instances of **archetypes**, identified by stable string
IDs and declared in data:

```jsonc
// packs/core/archetypes/obstacles.json
"coaster_car": {
  "kind": "obstacle",
  "title": "Derailed coaster car",
  "cost": { "money": 60 },
  "reward": {},
  "work": "dismantle",            // a work *action*, mapped to animation by presentation
  "cleared": "hide",
  "sprite": "twisted-tracks-debris/car"   // atlas-id/sprite-id
}
```

```jsonc
// packs/core/archetypes/buildings.json
"field_kitchen": {
  "kind": "producer",
  "title": "Field kitchen",
  "build_cost": { "wood": 40 }, "repair_cost": { "wood": 20 },
  "produces": { "food": 5, "every": 5 }, "bonus": "food_small",
  "sprites": { "damaged": "library-props/kitchen_damaged",
               "operational": "library-props/kitchen" }
}
```

* Retail objects are just **another pack**. `packs/retail/` maps `tree00` to an
  archetype with retail costs. The obstacle table, building table and mission
  tables for 1–6 move out of C into that pack.
* `ObstacleDef`, `BuildingDef`, pickup rules, titles and work actions become
  fields on an archetype. Every `strstr` in §2.2 disappears from the
  runtime, and the retail importer resolves them once.
* `g->early` is deleted. Mission behaviour comes from `MissionDef` data
  (objectives, start inventory, allowed buildings, vehicles). The prototype
  "escort" behaviour becomes a mission option or is dropped.

### 3.2 A native level format

* The runtime reads the native format directly. JSON is fine for now, with a
  compiled binary pack later for shipping. **The retail v301 binary is only an
  import format.**
* Objects reference archetypes by ID, with optional per-instance overrides.
  There are no `params` mini-languages, placeholder `x: 0, y: 768` transforms,
  linked type-5 overlays or `.rules` sidecar.
* Scenic effects (foam, motes, insects) become typed records rather than strings
  parsed in `scene.c`.
* Retail levels go through `import/retail` into the same in-memory model. The
  importer is the only code that knows type `102`, `extra=="offroad"` or
  "name ends in 2".

### 3.3 Content packs and a virtual file system

```
mount order (later wins):  core  →  retail (optional)  →  hd  →  characters-kenney  →  mod/dev
```

* Each pack has a `pack.json` with an ID, version, dependencies, and the
  atlases, fonts, archetypes and missions it provides.
* Atlases use **our own** manifest. The existing `*.atlas.json` and
  `catalog.json` are close already. Retail Lua declarations are read only by
  the importer.
* HD and Kenney stop being tree copies. They become overlay packs that replace
  sprite IDs. No rsync or Lua rewriting is needed.
* A mission declares the packs it requires. The loader fails clearly at startup
  if one is missing, instead of rendering the wrong workers.

### 3.4 Simulation core

* **Stable IDs, dynamic storage.** Arrays are sized from the level, strings
  interned or referenced by ID, and the raw `Level` is dropped after import. That
  takes the world from 45 MB to well under 1 MB.
* **Entity = core + kind data.** Use a tagged union, or small component arrays
  (transform, job target, producer, gate, vehicle, roamer, burnable). Each
  system iterates only the entities that have its component.
* **Presentation is derived.** The sim exposes state such as `walking`,
  `direction`, `working(action)` or `carrying(resource)`. `present/` maps that to
  animation clips using archetype data.
* **Fixed tick.** Update at 60 Hz with an accumulator, and drive the RNG from
  a seed. This is the precondition for saves, replays and reliable tests.
* **Engine context.** Use a struct holding the renderer, asset VFS, content
  registries and log, instead of globals. Errors return a status plus a message
  buffer owned by the caller.

### 3.5 Build, test, ship

* Build `libvolcano_core` (sim + content + import) as a static library. The
  game, the tools and each test suite are separate executables. No tests in the
  shipping binary.
* CI runs `sim/` and `content/` tests headless against a small **test pack in
  the repo**, with no retail data. Retail-dependent tests are opt-in.
* **Scenario tests are data.** A mission test is a script of clicks, ticks and
  expectations next to the mission (`missions/library-rescue.test.json`), run
  by one generic runner. Mission-specific C, mission flags and Makefile blocks
  go away.
* Packaging is driven by a manifest: `bundle --packs core,hd --missions …`
  produces a self-contained app. There are no launchers pointing back into a
  checkout.
* Large binaries go to Git LFS. Concepts, rejected art and history move out of
  `levels/` into an `art-source/` area or a separate repository.

---

## 4. Migration plan

Each phase leaves every existing mission playable and its regressions green.
They are ordered so the riskiest coupling is removed first.

| Phase | Scope | Exit criteria |
| --- | --- | --- |
| **0. Foundations** | Split `libvolcano_core`; tests become separate binaries; add CI. Create a minimal `packs/test` (a few placeholder sprites, one font, one tiny mission) so core tests run without retail data. | `make test-core` passes on a clean clone in CI. |
| **1. Archetype registry** | Introduce archetype IDs and an in-memory registry. Move the C tables (obstacles, buildings, pickups, missions 1–6) into `packs/retail/*.json`. The retail importer maps sprite names to archetype IDs **once**. Remove all runtime `strstr` and `g->early`. | No `strstr`/`strcmp` on art names outside `import/retail`; campaign 1–6 regressions unchanged. |
| **2. Native level format** | Runtime loader for the native format. Port the five custom missions. Linked type-5 presentations become real archetypes (Field kitchen, coaster car …) using the existing atlas JSONs. Delete `bind_presentations`, the `.rules` sidecar and the `@` sibling hack. | Library Rescue and Twisted Tracks run from native files; their debris/buildings have their own costs and titles. |
| **3. Packs + VFS** | Layered mounts; `pack.json`; HD and Kenney become overlay packs. Delete the rsync and Lua-rewrite steps. | `--packs core,retail,hd` replaces `--assets build/hd-assets`; one asset tree on disk. |
| **4. Sim core hardening** | Dynamic storage, entity components, drop the raw `Level`, presentation-derived animation, fixed tick, engine context. | World < 1 MB; sim tests link without SDL; deterministic replay test. |
| **5. Ship pipeline** | Data-driven scenario runner; manifest-based bundler; LFS; repo cleanup. | A signed, self-contained build from CI, with no file references outside the bundle. |

Phases 0–2 address everything raised in the request: the two asset worlds
and the custom buildings. Phases 3–5 make the result shippable.

---

## 5. Decision needed

**Is retail Rescue Team content an optional compatibility pack, or a required
dependency?** This review assumes the former. It is the only option that allows
a distributable product, CI, and missions whose rules match their art. If the
retail campaign must stay first-class, the plan still holds. The retail pack
simply always gets mounted in development builds, and it stays out of release
builds unless licensing allows otherwise.
