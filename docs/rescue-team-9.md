# Rescue Team 9 reference

The active reference for new engine work is **Rescue Team: Evil Genius
Collector's Edition (Rescue Team 9)**. Local retail files are kept in
`reference/rescue-team-9/`, which is ignored by Git.

The Windows installer in Downloads is
`Rescue-Team-9-Evil-Genius-Collectors-Edition.exe`. Extract it without running it:

```sh
innoextract --output-dir reference/rescue-team-9 \
  "$HOME/Downloads/Rescue-Team-9-Evil-Genius-Collectors-Edition.exe"
```

The game is Unity based. Its data is in `app/Data/data.unity3d`, with managed
assemblies in `app/Data/Managed/`. It does **not** use RT5's `texture/lazy/*.lua`
atlases or version 301 level files. Passing this directory to the RT5 importer
is insufficient; the older importer remains available for existing missions.

Do not assume Retina support from the release number. Inspect the player
settings, source texture sizes, sprite rectangles and canvas scaling before
adopting its rendering conventions.

## Load and inspect

```sh
build/python-venv/bin/python -m pip install -r requirements-reference.txt
build/python-venv/bin/python tools/load_rt9.py --open
# Or: make reference
```

The exporter decodes original textures to PNG, preserves TextAssets, and writes
`export/manifest.json` plus a local `export/index.html` viewer under the ignored
reference folder. Subsequent loads use the cached export; pass `--refresh` to
rebuild it. The viewer displays authored level layers and meshAnimator sequences
at their original world coordinates (100 pixels per world unit), including
source pivots, UV rectangles, scale and animation FPS. Its canvas uses 1920×1080
logical coordinates. Atlas mode shows original texture pixels. Click objects
to inspect metadata. Composite sprites currently show their base artwork; secondary parts, shadows
and keyframe transforms are not yet reproduced. Retail scripts, runtime spawning, particles and gameplay
are not implemented by this viewer. The SDL game still supports RT5 missions;
RT9 requires a new gameplay importer rather than changing that asset path.

## Display findings from this installer

- `PlayerSettings.macRetinaSupport` is **false**. This Windows build does not
  establish that the Mac release supports Retina.
- `defaultIsNativeResolution` is **true**, `fullscreenMode` is **1**,
  `resizableWindow` is **false**; the stored default size is 1024×768.
- Background declarations use 19.2×10.8 world units and approximately
  1920×1080 source pixels (with padding in the texture).
- Several character sheets are around 4K: `worker` is 4016×3480 and `fireman`
  is 4096×1680. Atlas dimensions alone do not imply a 2× sprite density.

For our renderer, retain resolution-independent placement and source pivots,
use these higher-resolution sources without downsampling, and keep the existing
SDL high-DPI drawable support. Do not copy the disabled Retina flag.
