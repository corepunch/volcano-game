#!/usr/bin/env python3
"""Compare authored gameplay records with RT5 campaign scenes, excluding scenery.

Counts describe content, not a proof of equal difficulty. Retail binary numbering
is not campaign order. No original artwork or full record dumps are exported.
"""
import argparse
import collections
import json
from pathlib import Path
import xml.etree.ElementTree as ET
from mission import load_archetypes
from retail_levels import decode

ROOT = Path(__file__).resolve().parents[1]
ORDER = ['first-rescue', 'emerald-falls', 'forest-mayday', 'ancient-echoes', 'twisted-tracks', 'library-rescue']
# Verified against reference/rescue-team/level/map.lvl prerequisite chain.
RETAIL = [15, 18, 19, 12, 13, 11]


def compare(assets):
    classes = load_archetypes(ROOT / 'content')
    missions, retail = [], []
    for name in ORDER:
        root = ET.parse(ROOT / 'missions' / name / 'mission.xml').getroot()
        local = dict(classes)
        for node in root.findall('archetype'):
            local[node.get('id')] = node.get('class') or local[node.get('extends')]
        objects = root.findall('object')
        count = collections.Counter(local[o.get('type')] for o in objects)
        missions.append(dict(name=name, objects=len(objects), obstacles=count['obstacle'],
            producers=count['producer'], pickups=count['pickup'], civilians=count['civilian'],
            workers=count['worker'], animals=count['animal'], valves=count['valve'],
            services=count['service'], vehicles=count['vehicle'], lots=count['lot'],
            people_goal=int(root.find('settings').get('people', 0))))
    for position, index in enumerate(RETAIL, 1):
        data = decode((assets / 'level' / f'level{index:03}.lvl').read_bytes())
        count = collections.Counter(o['type'] for o in data['objects'])
        retail.append(dict(campaign=position, scene=index,
            obstacles=count[102], producers=count[101], pickups=count[108], civilians=count[109],
            workers=count[104], animals=count[105], valves=count[128], lots=count[146],
            services=count[114]+count[154], vehicles=count[113]+count[153]))
    return dict(method='Authored gameplay records; scenery, effects and traffic excluded. '
        'Retail service records may be inactive; counts are not unique puzzles or difficulty scores.',
        missions=missions, retail=retail)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', type=Path, default=ROOT / 'reference/rescue-team')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = compare(args.assets)
    output = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output)
    print(output)


if __name__ == '__main__':
    main()
