"""Compiler contract and production-reader compatibility, including retail corpus."""
import copy
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import level_source as ls


class LevelSourceTests(unittest.TestCase):
    def setUp(self):
        self.source = ls.load_source(ROOT / "levels/first-rescue.level.json")

    def rejects(self, edit):
        source = copy.deepcopy(self.source)
        edit(source)
        with self.assertRaises(ValueError):
            ls.encode(source)

    def test_defaults_and_aliases(self):
        raw = ls.decode(ls.encode(self.source))
        self.assertEqual(raw["max_id"], 8)
        self.assertEqual(raw["objects"][0]["type"], 18)
        self.assertEqual(raw["objects"][1]["scale_x"], 1)
        self.assertEqual(ls.warnings(self.source), [])

    def test_all_retail_files_roundtrip_through_json_and_c(self):
        directory = Path(os.environ.get("RESCUE_TEAM_ASSETS", ROOT / "reference/rescue-team")) / "level"
        originals = sorted(directory.glob("*.lvl"))
        self.assertEqual(len(originals), 52, "retail corpus required; use RESCUE_TEAM_ASSETS")
        with tempfile.TemporaryDirectory() as folder:
            outputs = []
            for original in originals:
                data = original.read_bytes()
                source = json.loads(json.dumps(ls.decode(data)))
                binary = ls.encode(source)
                self.assertEqual(binary, data, original.name)
                output = Path(folder) / original.name
                output.write_bytes(binary)
                outputs.append(str(output))
            subprocess.run([str(ROOT / "build/check_custom_level"), "--decode", *outputs], check=True, cwd=ROOT)

    def test_special_payloads_and_float_bits(self):
        self.source["objects"] += [
            {"id": 9, "type": "ambient", "params": "bird00_idle", "x": -0.0, "auxiliary": [12.125, -3.5]},
            {"id": 10, "type": "traffic", "layer": 7, "points": [[-0.0, 0.1]]},
            {"id": 11, "type": 999, "name": "\udc80", "extra": "Привет"},
        ]
        data = ls.encode(self.source)
        decoded = json.loads(json.dumps(ls.decode(data)))
        self.assertEqual(ls.encode(decoded), data)
        self.assertEqual(decoded["objects"][-3]["auxiliary"], [12.125, -3.5])
        self.assertEqual(struct.pack("<f", decoded["objects"][-3]["x"]), struct.pack("<f", -0.0))

    def test_bad_source(self):
        edits = [
            lambda s: s.update(version=300),
            lambda s: s.update(typo=1),
            lambda s: s["objects"][1].update(id=1),
            lambda s: s["objects"][1].update(id=True),
            lambda s: s["objects"][1].update(type="unknown"),
            lambda s: s["objects"][1].update(link_id=-1),
            lambda s: s["objects"][1].update(x=float("nan")),
            lambda s: s["objects"][1].update(x=1e100),
            lambda s: s["objects"][1].update(x=10**1000),
            lambda s: s["objects"][1].update(scale=2),
            lambda s: s["objects"][1].update(auxiliary=[0, 0]),
            lambda s: s["objects"][1].update(name="a\0b"),
            lambda s: s["objects"][1].update(name="я" * 256),
            lambda s: s["objects"][0].update(points=[[1]]),
            lambda s: s["objects"][0].update(points=[[0, 0]] * 4097),
            lambda s: s.update(objects=[{"type": 2, "id": i} for i in range(4097)]),
            lambda s: s["mission"].update(allow_cafe=1),
            lambda s: s["mission"].update(food=-1),
            lambda s: s.update(background="@../scene.png"),
            lambda s: s.update(background="@scene.lvl"),
        ]
        for edit in edits:
            with self.subTest(edit=edit):
                self.rejects(edit)

    def test_binary_rejection(self):
        data = ls.encode(self.source)
        for cut in range(len(data)):
            with self.subTest(cut=cut), self.assertRaises(ValueError):
                ls.decode(data[:cut])
        with self.assertRaises(ValueError):
            ls.decode(data + b"extra")
        with self.assertRaises(ValueError):
            ls.decode(struct.pack("<I", 300) + data[4:])

    def test_cli_rules_roundtrip_and_stale_protection(self):
        tool = ROOT / "tools/level_source.py"
        with tempfile.TemporaryDirectory() as folder:
            binary, source = Path(folder) / "demo.lvl", Path(folder) / "demo.json"
            subprocess.run([sys.executable, str(tool), "compile", str(ROOT / "levels/first-rescue.level.json"), str(binary)], check=True)
            subprocess.run([sys.executable, str(tool), "decompile", str(binary), str(source)], check=True)
            self.assertEqual(ls.load_source(source)["mission"], self.source["mission"])
            decoded = ls.decode(binary.read_bytes())
            source.write_text(json.dumps(decoded))
            before = binary.read_bytes()
            result = subprocess.run([sys.executable, str(tool), "compile", str(source), str(binary)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(binary.read_bytes(), before)
            source.write_text('{"format": "a", "format": "b"}')
            with self.assertRaises(ValueError):
                ls.load_source(source)

    def test_runtime_rules_validation_and_optional_sidecar(self):
        with tempfile.TemporaryDirectory() as folder:
            binary = Path(folder) / "demo.lvl"
            binary.write_bytes(ls.encode(self.source))
            rules = Path(str(binary) + ".rules")
            valid = ls.mission_values(self.source["mission"])
            assets = os.environ.get("RESCUE_TEAM_ASSETS", str(ROOT / "reference/rescue-team"))
            command = [str(ROOT / "build/volcano"), "--assets", assets, "--software", "--hidden", "--level-file", str(binary), "--frames", "1"]
            env = dict(os.environ, SDL_VIDEODRIVER="dummy")
            def run():
                return subprocess.run(command, env=env, cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(run().returncode, 0, "sidecar is optional for prototype scenes")
            for bad in [valid.replace("version=1", "version=2"), valid + "food=20\n", valid.replace("food=20", "food=-1"), valid.replace("food=20\n", ""), valid + "typo=1\n", valid.replace("food=20", "food=nan"), valid.replace("food=20", "food=2147483648"), valid + "\0", "x" * 2048]:
                with self.subTest(rules=bad):
                    rules.write_text(bad)
                    self.assertNotEqual(run().returncode, 0)
                    with self.assertRaises(ValueError):
                        ls.parse_rules(bad)
            rules.write_text(valid)
            self.assertEqual(run().returncode, 0)

    def test_custom_background_packaging(self):
        tool = ROOT / "tools/level_source.py"
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            origin = root / "source"
            origin.mkdir()
            source = dict(self.source, background="@scene.png")
            source["objects"] = list(source["objects"]) + [
                {"type": "effect", "id": 90, "params": "sprite;image=@foliage.png;rect=0,0,32,32"}
            ]
            (origin / "level.json").write_text(json.dumps(source))
            pixels = (ROOT / "levels/emerald-falls.png").read_bytes()
            (origin / "scene.png").write_bytes(pixels)
            (origin / "foliage.png").write_bytes(pixels)
            binary = root / "compiled/demo.lvl"
            command = [sys.executable, str(tool), "compile", str(origin / "level.json"), str(binary)]
            subprocess.run(command, check=True)
            self.assertEqual((binary.parent / "scene.png").read_bytes(), pixels)
            self.assertEqual((binary.parent / "foliage.png").read_bytes(), pixels)
            editable = root / "editable/demo.level.json"
            subprocess.run([sys.executable, str(tool), "decompile", str(binary), str(editable)], check=True)
            self.assertEqual((editable.parent / "scene.png").read_bytes(), pixels)
            self.assertEqual((editable.parent / "foliage.png").read_bytes(), pixels)
            before = binary.read_bytes()
            (origin / "foliage.png").unlink()
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(binary.read_bytes(), before)

    def test_map_visual_validation(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "atlas.png").write_bytes((ROOT / "levels/emerald-falls.png").read_bytes())
            binary = root / "demo.lvl"
            assets = os.environ.get("RESCUE_TEAM_ASSETS", str(ROOT / "reference/rescue-team"))
            command = [str(ROOT / "build/volcano"), "--assets", assets, "--software", "--hidden", "--level-file", str(binary), "--frames", "1"]
            env = dict(os.environ, SDL_VIDEODRIVER="dummy")
            def run(params, **fields):
                source = copy.deepcopy(self.source)
                source["objects"].append({"type": "effect", "id": 90, "params": params, "x": 400, "y": 400, **fields})
                binary.write_bytes(ls.encode(source))
                return subprocess.run(command, env=env, cwd=ROOT, capture_output=True).returncode
            self.assertEqual(run("sprite;image=@atlas.png;rect=0,0,32,32;pivot=.5,.9;wind=2"), 0)
            for params in [
                "sprite;image=@../atlas.png", "sprite;image=@atlas.png;rect=0,0,8192,8192",
                "sprite;image=@atlas.png;rect=0,0,0,32", "sprite;image=@atlas.png;rect=0,0,32.5,32",
                "sprite;image=@atlas.png;pivot=2,.9", "sprite;image=@atlas.png;stage=sky",
                "sparkles;speed=0", "sparkles;opacity=nan", "insects;count=33", "insects;count=1.5",
                "insects;animation=no_such_animation", "motes;width=-1",
            ]:
                with self.subTest(params=params):
                    self.assertNotEqual(run(params), 0)
            self.assertNotEqual(run("sprite;image=@atlas.png", scale_x=0), 0)
            self.assertEqual(run("unrestored_retail_controller"), 0)
            source = copy.deepcopy(self.source)
            source["objects"] += [{"type": "effect", "id": i, "params": "foam"} for i in range(100, 1125)]
            binary.write_bytes(ls.encode(source))
            self.assertNotEqual(subprocess.run(command, env=env, cwd=ROOT, capture_output=True).returncode, 0)


if __name__ == "__main__":
    unittest.main()
