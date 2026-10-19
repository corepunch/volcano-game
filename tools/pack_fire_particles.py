#!/usr/bin/env python3
"""Pack the downloaded CC0 fire frames and smoke textures, preserving alpha."""
from io import BytesIO
from pathlib import Path
from zipfile import ZipFile

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "assets/particles"


def main():
    # Original 81x123 frames, with a transparent two-pixel gutter on each side.
    atlas = Image.new("RGBA", (85 * 8, 127 * 10))
    with ZipFile(ART / "source/FIRE.zip") as archive:
        for layer, suffix in enumerate("bf"):
            for frame in range(40):
                name = f"fire_1_40/fire_1{suffix}_40_{frame + 1}.png"
                image = Image.open(BytesIO(archive.read(name))).convert("RGBA")
                if image.size != (81, 123):
                    raise ValueError(f"Unexpected fire frame dimensions: {name}")
                slot = layer * 40 + frame
                atlas.paste(image, ((slot % 8) * 85 + 2, (slot // 8) * 127 + 2))
    atlas.save(ART / "log-fire.png")
    smoke = Image.new("RGBA", (132 * 4, 132))
    for slot, frame in enumerate((0, 4, 8, 12)):
        image = Image.open(ART / f"source/whitePuff{frame:02d}.png").convert("RGBA")
        image.thumbnail((128, 128), Image.Resampling.LANCZOS)
        smoke.paste(image, (slot * 132 + (132 - image.width) // 2,
                            (132 - image.height) // 2))
    smoke.save(ART / "log-smoke.png")
    print("Packed 80 fire frames and four smoke textures in assets/particles")


if __name__ == "__main__":
    main()
