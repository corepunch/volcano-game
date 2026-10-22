#!/usr/bin/env python3
"""Extract and load the local RT9 Unity reference; never executes retail code."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import webbrowser

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', type=Path, default=ROOT / 'reference/rescue-team-9')
    parser.add_argument('--open', action='store_true', help='Open the reference scene viewer')
    parser.add_argument('--refresh', action='store_true')
    args = parser.parse_args()
    output = args.assets / 'export'
    index = output / 'index.html'
    if args.refresh or not index.exists():
        import UnityPy
        env = UnityPy.load(str(args.assets / 'app/Data/data.unity3d'))
        output.mkdir(parents=True, exist_ok=True)
        textures, texts, atlases, levels, settings = {}, {}, {}, {}, {}
        counts = Counter()
        versions = set()
        for obj in env.objects:
            counts[obj.type.name] += 1
            versions.add(obj.assets_file.unity_version)
            if obj.type.name == 'PlayerSettings':
                data = obj.read_typetree()
                settings = {k: v for k, v in data.items() if any(s in k.lower() for s in ('screen', 'resolution', 'retina', 'fullscreen', 'resizable', 'productname'))}
            elif obj.type.name == 'Texture2D':
                data = obj.read()
                if not data.m_Width or not data.m_Height:
                    continue
                name = re.sub(r'[^\w.-]', '_', data.m_Name) + f'-{obj.path_id}.png'
                data.image.save(output / name)
                textures[data.m_Name] = {'file': name, 'width': data.m_Width, 'height': data.m_Height}
            elif obj.type.name == 'TextAsset':
                data = obj.read()
                name = re.sub(r'[^\w.-]', '_', data.m_Name) + f'-{obj.path_id}.txt'
                raw = data.m_Script.encode('utf8', 'surrogateescape')
                (output / name).write_bytes(raw)
                texts[data.m_Name] = name
                try:
                    doc = json.loads(raw)
                except (ValueError, UnicodeError):
                    continue
                if isinstance(doc, dict):
                    if doc.get('format') == 'meshAnimator':
                        atlases[doc['atlas']] = doc
                    if re.fullmatch(r'level\d+', data.m_Name) and 'layers' in doc:
                        levels[data.m_Name] = doc
        manifest = {'textures': textures, 'texts': texts, 'atlases': atlases, 'levels': levels,
                    'settings': settings, 'object_counts': counts, 'unity_versions': sorted(versions)}
        (output / 'manifest.json').write_text(json.dumps(manifest, indent=2))
        print(f'Extracted {len(textures)} textures, {len(atlases)} atlases, {len(levels)} scenes into {output}')
    # Refresh viewer code without decoding textures again. Inline metadata supports file://.
    manifest = json.loads((output / 'manifest.json').read_text())
    template = (ROOT / 'tools/rt9_viewer.html').read_text()
    payload = json.dumps(manifest).replace('<', '\\u003c')
    index.write_text(template.replace('/*RT9_DATA*/', 'const DATA=' + payload + ';'))
    print(index)
    if args.open:
        webbrowser.open(index.resolve().as_uri())


if __name__ == '__main__':
    main()
