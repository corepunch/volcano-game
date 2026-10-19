# Rendering the Kenney rescue characters

Run all commands from the repository root. The saved models, textures, animations
and sprite camera are in
[`assets/kenney-characters/blender/rescue-characters.blend`](../assets/kenney-characters/blender/rescue-characters.blend).
The [asset overview](../assets/kenney-characters/README.md) lists the animations,
equipment and corresponding game resource names.

## Requirements

- Blender 5.2 or newer, with the Blender MCP extension enabled. The commands below
  use the installed macOS executable at `/Applications/Blender.app/Contents/MacOS/Blender`.
- The extension must expose `--command blender_mcp`. `tools/blender_mcp.py` uses its
  local, null-delimited `execute` socket protocol with `strict_json=True`.
  A different Blender MCP server may use a different protocol.
- Python 3 and Pillow for packing and preview generation. Check with
  `python3 -c 'from PIL import Image; print(Image.__version__)'`.
- The game dependencies and retail resources described in the
  [project README](../README.md#build-and-run) for installation and game checks.

Use a separate Blender process for this work. In the Codex filesystem sandbox,
Blender's GPU initialization and the localhost socket client have required
execution outside the sandbox; these are local operations, with no cloud service.
The server binds only to `127.0.0.1`.

## Render the existing saved model

In terminal A, start Blender with the actual saved scene:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background \
  assets/kenney-characters/blender/rescue-characters.blend --online-mode \
  --command blender_mcp --host 127.0.0.1 --port 9877
```

Keep terminal A open. In terminal B, render, pack, test and package:

```sh
python3 tools/blender_mcp.py tools/render_rescue_characters.py
python3 tools/pack_rescue_characters.py --pack --declare
make test
make run MISSION=library-rescue
```

The render command operates on the scene already loaded in the MCP process. It
returns JSON with `status: "ok"` and a `result` dictionary containing the number
of newly rendered frames. Zero rendered frames is normal when the cache is
already complete. The packer must finish successfully before building the game.

If you saved edits from another Blender window, restart this background process
with the updated file before rendering. Saving a file elsewhere does not reload
it into the MCP process. The renderer does not save changes back to the `.blend`.

`--pack` writes the atlases and `sprites/catalog.json`; `--declare` writes
`assets/kenney-characters/sprites/kenney.sprites.xml`, which serves the retail
worker and civilian sprite names (`worker_*`, `man_*`, `wman_*`) from those
atlases. The folder `assets/kenney-characters` is itself an asset pack: nothing
is copied or installed. Library Rescue mounts it with
`<pack path="../../assets/kenney-characters"/>`; any other mission or campaign
level can use it with `./build/volcano --pack assets/kenney-characters ...`.
`--declare` alone regenerates the declarations from the saved catalog without
invoking Blender or re-rendering animations.

Stop only your dedicated background process when finished. You can identify the
listener with `lsof -nP -iTCP:9877 -sTCP:LISTEN`, verify its PID belongs to your
Blender process, and run `kill -TERM PID`. Do not terminate an unrelated Blender
session. For another port, change both the server's `--port` and the client's
`--port` option.

## Rebuild the model from source instead

This path recreates the entire scene and overwrites the saved `.blend`, baked
uniform and animation metadata. Put any edits you want to preserve into the
source tools first, or retain a separate copy of the manually edited scene.

With the dedicated MCP server running:

```sh
python3 tools/blender_mcp.py tools/build_rescue_characters.py
```

Check the resulting model and animations, then commit this completed authoring
step before starting the render step, as required by `AGENTS.md`. Continue with
the render, pack and game-check commands above. The builder already loads a
fresh empty scene and leaves the newly built model loaded in that MCP process.

| File | Responsibility |
| --- | --- |
| `tools/build_rescue_characters.py` | Import and retarget FBX clips, author work actions, gear, rig, camera, lighting and visibility; save the model and `actions.json` |
| `tools/texture_rescue_uniform.py` | Paint/bake the shared uniform atlas using Kenney's original body mesh and UV layout; make the simple helmet |
| `tools/animate_rescue_run.py` | Bounce, torso recoil, arm drive, reduced head swing and carrying pose |
| `tools/render_rescue_characters.py` | Render poses and directions; write the render manifest |
| `tools/pack_rescue_characters.py` | Validate/crop frames, pack atlases, generate previews (`--pack`) and write the pack's sprite declarations (`--declare`) |

The FBX clips have different rest poses. Keep the builder's Targeting Pose
retargeting; copying local quaternion tracks directly between these rigs gives
incorrect poses. Preserve the original simple Kenney body. Paint uniform details
into the texture rather than adding clothing geometry.

The source builder applies `enhance_run()` once to a freshly retargeted Run and
uses `reduce_head_swing(rig, .25)` to produce the current quarter-strength head
rotation. Do not apply `enhance_run()` again to the already enhanced saved action.
`reduce_head_swing(rig, factor)` scales the **current** angular swing: `.5` halves
it again and multiplies the action's `head_swing_scale` metadata. The head still
travels with the moving torso. After editing Run, rebuild Carry with
`rebuild_carry(rig)` so it inherits the new motion and keeps its chest-relative
arm pose. Update the source tools as well as the saved scene for repeatability.

## Render settings and outputs

The renderer creates 29 clips: 17 worker actions and six actions each for the
male and female civilians. Running is shared across all three characters.

| Setting | Current value |
| --- | --- |
| Engine | Cycles, 12 samples, denoising enabled |
| Background | Transparent RGBA PNG |
| Frame size | 128 × 160; 160 × 160 for Hammer, Repair, Chop and Pickaxe |
| Camera | Saved orthographic `Sprite_Camera`, scale 6.2 |
| Directions | `0=E, 1=NE, 2=N, 3=NW, 4=W, 5=SW, 6=S, 7=SE` |
| Turning | `Direction_Pivot.rotation_euler.z = pi/2 + direction*pi/4` |
| Poses | One-based Blender frames; zero-based filenames, e.g. `7-000.png` |
| Run / Carry | 16 rendered poses; frame 17 is the matching, excluded loop endpoint |
| Source timing | 24 FPS; installer retains each retail animation's own frame count and FPS |
| Texture scale | 2×, with pivots computed from the projected world origin |

The rig is `RescueWorker_Rig`. Assign both `animation_data.action` and its
`action_slot` when selecting a `Rescue_*` action in Blender 5.2. The rig property
`character` selects `0` worker, `1` male or `2` female. Gear visibility is driven by
action properties and is evaluated with the pose. Call `scene.frame_set(frame)`
and update the view layer before rendering. For manual pose overlays, evaluate
an action switch before assigning bone matrices; otherwise the first dependency
graph update can replace the pose with the action's existing keys.

| Output | Purpose |
| --- | --- |
| `build/kenney-character-frames/CHARACTER-ACTION/DIRECTION-INDEX.png` | Resumable full-frame render cache |
| `build/kenney-character-frames/progress.json` | Last completed clip and newly rendered frame count for this invocation |
| `assets/kenney-characters/render-manifest.json` | Clip/frame sizes, cache paths and ground anchors for packing |
| `assets/kenney-characters/sprites/*.png` | Packed transparent atlases committed to Git |
| `assets/kenney-characters/sprites/catalog.json` | Frame rectangles, pivots, texture sizes and SHA-256 hashes |
| `assets/kenney-characters/sprites/kenney.sprites.xml` | Sprite sheet mapping retail character names to the atlases, keeping retail frame counts and FPS |
| `assets/kenney-characters/previews/worker-actions.jpg` | Four poses of each worker action |
| `assets/kenney-characters/previews/worker-directions.jpg` | Eight-direction review of standing, running and hammering |
| `assets/kenney-characters/previews/rescue-worker.gif` | Animated review including Run and Carry |

The packer regenerates those three previews. `previews/dynamic-run.gif` is a
separate side/three-quarter capture; the normal pack command does not refresh it.
Use the newly generated `rescue-worker.gif` to review the latest packed motion.

## Re-render only changed animations

By default, the renderer fingerprints the saved `.blend` bytes plus its own
Python source. A fingerprint change deletes the entire frame cache. This is the
safe default for camera, lighting, texture, mesh, shared rig or visibility edits.
It also skips any existing PNG in a valid cache, so an interrupted render can
resume without repeating completed frames.

For an **animation-only** Run/Carry edit, all other clip caches can be retained
if you have verified their actions and the rendering setup are unchanged.
After saving the edited `.blend` and loading it in the MCP process, run this in
the normal terminal before invoking the renderer:

```sh
python3 - <<'PY'
from pathlib import Path
import hashlib
import shutil

root = Path.cwd()
cache = root / 'build/kenney-character-frames'
assert (cache / 'fingerprint.txt').is_file(), 'No cache to preserve; render normally.'
for name in ['worker-Run', 'worker-Carry', 'male-Run', 'female-Run']:
    folder = cache / name
    if folder.exists():
        shutil.rmtree(folder)
fingerprint = hashlib.sha256(
    (root / 'assets/kenney-characters/blender/rescue-characters.blend').read_bytes()
    + (root / 'tools/render_rescue_characters.py').read_bytes()
).hexdigest()
(cache / 'fingerprint.txt').write_text(fingerprint)
PY
python3 tools/blender_mcp.py tools/render_rescue_characters.py
python3 tools/pack_rescue_characters.py --pack --declare
make test
```

This explicitly marks the other cached clips as valid. Do not use it for changes
that affect their appearance. With the current frame counts, these four changed
clips require 512 directional frames. For any other action edit, invalidate every
character/action folder that uses the edited action. Do not preserve a cache
whose unchanged clips you cannot verify.

To force a full render, delete just the generated frame cache and render again:

```sh
rm -rf build/kenney-character-frames
python3 tools/blender_mcp.py tools/render_rescue_characters.py
```

## Quick pose preview

The renderer has a four-pose preview mode: direction 7, at cycle start, quarter,
half and three-quarter. It uses the same cache, skips existing files and does
not write a new render manifest. To enable it through the socket client, create
a small wrapper in the ignored build directory:

```sh
mkdir -p build
cat > build/preview-rescue-poses.py <<'PY'
from pathlib import Path

root = Path(__file__).resolve().parents[1]
script = root / 'tools/render_rescue_characters.py'
scope = {'__file__': str(script), 'RESCUE_PREVIEW_ONLY': True}
exec(compile(script.read_text(), str(script), 'exec'), scope)
result = scope['result']
PY
python3 tools/blender_mcp.py build/preview-rescue-poses.py
```

The renderer needs its own `__file__` in the wrapper namespace to find the repo
root correctly. A custom MCP script must return a `result` **dictionary**.
After checking the PNGs, run the normal full renderer before packing. Preview
mode alone does not create a complete atlas cache. For animated inspection in
Blender, select Run or Carry and play frames 1–16 at 24 FPS.

## Verification and recovery

Before considering an animation complete:

- Check the loop seam, foot contact, head motion and equipment attachment in
  side and three-quarter views. Review all eight directions for clipping.
- Confirm the uniform remains textured and geometry has not grown unexpectedly.
- Run the packer: it rejects wrong frame sizes, empty alpha, edge clipping and
  unlit silhouettes. It crops alpha and preserves the world-origin ground pivot;
  do not center each trimmed frame independently.
- Run `make test`: the `--library-rescue` regression checks that the worker and
  civilian sprites resolve to the Kenney atlases. Then inspect
  `make run MISSION=library-rescue` with the retail assets. Missions that do not
  mount the pack keep the original character resources.

Keep `scene.render.use_persistent_data=False` and the atlas reload in the
renderer. Reusing Cycles state previously produced near-black silhouettes with
valid alpha. If a cached image is corrupt or dark, delete it and render again;
the resume logic tests file existence, not image validity.

For `cannot save` or an interrupted connection, check disk space (`df -h .`) and
the cache directory's write access, then rerun the render command. Completed
frames are reused. Remove any partially written PNG before retrying. If the MCP
process stopped, restart it with the saved `.blend` first. For connection refused,
check the port listener and confirm the Blender extension is enabled; for an
unknown `blender_mcp` command, the required extension is not available in that
Blender installation. Do not rebuild the model merely to recover a render.

Commit the completed authoring step before rendering, then commit the updated
sprite atlases, catalog, `kenney.sprites.xml`, manifest if changed, and generated
previews after the render checks pass. `build/` is ignored. Blender may create a `.blend1` backup;
it is not an input to this pipeline, so retain it outside the change or remove
the backup created by your own save before committing. Preserve unrelated edits
from other work in this shared repository.
