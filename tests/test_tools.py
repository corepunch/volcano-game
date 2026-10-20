"""Tool regressions: mission checker and retail level reader.

    python3 -m unittest discover -s tests -p 'test_tools.py'
Retail round trips run only when RESCUE_TEAM_ASSETS names the original folder.
"""
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import mission  # noqa: E402
import retail_levels  # noqa: E402


class MissionCheck(unittest.TestCase):
    def setUp(self):
        self.classes = mission.load_archetypes(ROOT / "content")

    def test_every_mission_is_valid_without_warnings(self):
        paths = sorted(ROOT.glob("missions/*/mission.xml")) + [ROOT / "skills/rescue-team-levels/assets/starter.mission.xml"]
        self.assertGreaterEqual(len(paths), 7)
        for path in paths:
            errors, warnings = mission.check(path, self.classes)
            self.assertEqual((errors, warnings), ([], []), path)

    def test_mission_archetypes_inherit_classes(self):
        self.assertEqual(self.classes["dog"], "animal")
        self.assertEqual(self.classes["boulder"], "obstacle")

    def check_text(self, text):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "mission.xml"
            path.write_text(text)
            return mission.check(path, self.classes)

    def test_problems_are_reported(self):
        base = '<mission format="volcano-mission-1" retail-background="103"><settings rules="rescue" people="1"/>{}</mission>'
        errors, _ = self.check_text(base.format('<object id="1" type="nope"/>'))
        self.assertTrue(any("unknown type" in e for e in errors))
        errors, _ = self.check_text(base.format('<object id="1" type="hq"/><object id="1" type="hq"/>'))
        self.assertTrue(any("duplicate object id" in e for e in errors))
        errors, _ = self.check_text(base.format('<object id="1" type="food-small" producer="5"/>'))
        self.assertTrue(any("names no object" in e for e in errors))
        errors, warnings = self.check_text(base.format(
            '<traffic>0,0 30,0 300,0</traffic><object id="1" type="hq" x="0" y="0"/>'
            '<object id="2" type="worker" x="500" y="500"/><object id="3" type="civilian" x="300" y="0"/>'))
        self.assertEqual(errors, [])
        self.assertTrue(any("separate networks" in w for w in warnings))
        self.assertTrue(any("starts stranded" in w for w in warnings))
        self.assertTrue(any("people goal is 1" in w for w in warnings))

    def test_cli_exit_status(self):
        tool = ROOT / "tools/mission.py"
        good = subprocess.run([sys.executable, str(tool), "check", str(ROOT / "missions/first-rescue/mission.xml")])
        self.assertEqual(good.returncode, 0)
        with tempfile.TemporaryDirectory() as folder:
            bad = Path(folder) / "mission.xml"
            bad.write_text("<mission/>")
            result = subprocess.run([sys.executable, str(tool), "check", str(bad)], capture_output=True)
            self.assertEqual(result.returncode, 1)


class RetailLevels(unittest.TestCase):
    SAMPLE = {"background": "103", "max_id": 7, "objects": [
        {"type": 18, "id": 1, "layer": 4, "points": [[1.5, 2.0], [3.0, 4.25]]},
        {"type": 102, "id": 4, "layer": 5, "name": "fire", "params": "tree00", "extra": "", "link_id": 0,
         "x": 10.0, "y": 20.0, "rotation": 0.0, "scale_x": 1.0, "scale_y": 1.0},
        {"type": 6, "id": 7, "layer": 9, "name": "", "params": "bird0;3;140", "extra": "", "link_id": 0,
         "x": 0.0, "y": 0.0, "rotation": 0.0, "scale_x": 1.0, "scale_y": 1.0, "auxiliary": [5.0, 6.0]}]}

    def test_synthetic_round_trip_and_dump(self):
        data = retail_levels.encode(self.SAMPLE)
        decoded = retail_levels.decode(data)
        self.assertEqual(retail_levels.encode(decoded), data)
        xml = retail_levels.dump(decoded)
        self.assertIn('<record type="102" kind="barrier" id="4" layer="5" name="fire" params="tree00"', xml)
        self.assertIn('auxiliary="5,6"', xml)

    def test_malformed_binaries_are_rejected(self):
        data = retail_levels.encode(self.SAMPLE)
        for bad in (data[:-1], data + b"\0", struct.pack("<I", 300) + data[4:]):
            with self.assertRaises(ValueError):
                retail_levels.decode(bad)

    @unittest.skipUnless(os.environ.get("RESCUE_TEAM_ASSETS"), "retail assets not configured")
    def test_all_retail_files_round_trip(self):
        folder = Path(os.environ["RESCUE_TEAM_ASSETS"]) / "level"
        report = retail_levels.analyze(folder)
        self.assertEqual(report["roundtripped_files"], 52)
        self.assertEqual(report["campaign_order"][:3], ["level015", "level018", "level019"])


if __name__ == "__main__":
    unittest.main()
