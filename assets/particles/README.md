# Burning-log particles

The runtime uses `log-fire.png` (two 40-frame layers, eight columns) and
`log-smoke.png` (four smoke variations). Repack with
`python3 tools/pack_fire_particles.py`. Packing preserves the fire's authored
colors and straight alpha; smoke images are proportionally reduced to 128 px.
No runtime network access is needed.

- Fire: **Mikodrak**, [Fire FX – Burning Fire Animation](https://opengameart.org/content/fire-fx-burning-fire-animation),
  CC0. Original download: https://opengameart.org/sites/default/files/FIRE.zip
  (retained in `source/FIRE.zip`). Both the back (`b`) and front (`f`) layers
  are used, with their original 81×123 frames and two-pixel atlas gutters.
- Smoke: **Kenney Vleugels**, [Smoke particle assets](https://opengameart.org/content/smoke-particle-assets),
  CC0. Original download: https://opengameart.org/sites/default/files/smokeParticleAssets.zip
  Selected originals `whitePuff00`, `04`, `08`, `12` and the original license
  are retained in `source/`. The remaining unused pack is not required.

Both packs were downloaded on 2026-10-19. CC0:
https://creativecommons.org/publicdomain/zero/1.0/

The normal and HD application bundles include these textures under
`Resources/texture/particles`, with this credit file and the Kenney license.
Development executables find `assets/particles` relative to the executable.
