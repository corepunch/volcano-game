# Volcano SDL2

A standalone **C11 + SDL2** restoration of ORCA's Volcano game prototype,
using the exact **Rescue Team 5** resources copied into `resources/`.

The source files in `~/Documents/Rescue Team` stay in place. The local copy is
ignored by Git. The program reads binary `.lvl` files,
PNG/JPEG atlases, JPEG `.alpha` masks, and bitmap font metrics directly at
runtime. Its small data parser accepts the original Lua-shaped atlas/font
syntax as declarations. **There is no Lua library, script execution, asset
conversion, generated XML, or dependency on ORCA.**

## Build and run

Requires a C11 compiler, `make`, `pkg-config`, and SDL2. These are already
available on this Mac.

```sh
cd ~/Developer/volcano-sdl2
make run
```

On macOS, `make app` also creates the double-clickable `build/Volcano.app`
with its own bundled resource copy. The app does not need the Documents folder.
The tracked bundle metadata lives in `packaging/Info.plist`.

To populate the ignored assets on a fresh checkout:

```sh
mkdir -p resources
rsync -a --exclude='.DS_Store' "$HOME/Documents/Rescue Team/" resources/
```

To use another original resource folder:

```sh
./build/volcano --assets "/path/to/Rescue Team"
```

`RESCUE_TEAM_ASSETS` also sets the resource folder. The default is the local
`resources/` folder, resolved from the working directory or executable location.
An app bundle uses its `Contents/Resources` copy.

Click debris to send an available worker, collect supplies, and escort
survivors. Workers follow the original traffic points, clear blocked paths,
perform timed work, and return home. Pickups happen immediately on arrival;
workers carry supplies back before crediting the inventory. After clearing or
building, workers wait 1.5 seconds at the worksite and can take a new order there.
Boats collect floating wood. Small and large supply piles yield 10 and 20 units. The top bar
uses the retail atlas, original Calibri bitmap fonts, resource icons, timer
meter, three objective rows, and the authored hover/pressed button states.
The pause button opens the retail panel with Continue and Play again controls.
Hover over targets for tooltips and green/red selection circles. Accepted and
queued jobs show green checkmarks; click a queued job again to cancel it.
Working targets use the original progress-bar artwork. Available workers inside
HQ appear as helmet icons above its roof and emerge when dispatched.

Click a building lot to open the four-button construction menu: Sawmill and
Burger Cafe cost 40 wood, House costs 60, and Gas Station costs 80. Choosing an
affordable building dispatches a worker for five seconds of construction once
the route is clear. Prices turn red and authored disabled artwork appears when
wood is insufficient. Escape or clicking outside dismisses the menu. Building
production and upgrades are outside this prototype's restored mechanics.
Objectives reflect the restored prototype mechanics. Money stays at zero,
and the timer stays full because this prototype is untimed; the supplied
level data contains no retail time-limit configuration.

- **Space / P:** pause or resume
- **R:** restart the current level
- **F1:** show traffic connections and worker paths
- **Escape:** dismiss the construction menu, or open/close the pause menu
- Close the window to quit

## Verification

```sh
make test
make sanitize
```

Tests read all 52 original levels to their exact end, validate atlas frames,
reject malformed data, and simulate the recovered level through its 31 jobs:
10 obstacles, 15 supply pickups, and 6 escorted survivors. They check movement,
blocked paths, work timing, pause, return journeys, inventories, boats, and
restart, HUD hit regions, retail font bounds, and button artwork in normal,
hover, and pressed states. Additional regressions cover all four construction
choices and costs, immediate pickups and cargo delivery, worker waiting and
chaining, queued-job cancellation, and in-house helmets. Tests use SDL's software
renderer without opening a window. Set `VOLCANO_TEST_SHOTS=1` with `make test`
to save deterministic interaction frames under `build/`.

A reproducible frame can be saved without a window:

```sh
SDL_VIDEODRIVER=dummy ./build/volcano --software --hidden \
  --frames 60 --screenshot build/preview.bmp
```

## Scope and provenance

The restored gameplay follows the former `samples/Volcano` prototype, first
introduced in ORCA commit `09a68b1d` (2026-02-25). Its later movement and work
callbacks were commented out. This project restores those interactions in C
and reads the original binary level instead of the archived Lua level table.

The installed resources identify themselves as Rescue Team 5 in
`conf/locale_en.lua`. The worker atlas matches the former Volcano atlas byte
for byte. Original artwork and data are kept in the ignored `resources/` directory
and are not intended to be committed.

Level 000 is the verified playable prototype. The binary reader understands
all 52 level files; later levels contain additional game mechanics beyond
the restored prototype. `--level N` selects a source level for inspection.

The image decoder is the existing `stb_image.h`, copied with its license
intact from ORCA. All authored application sources are `.c` and `.h` files;
SDL2 is the only external runtime library.
