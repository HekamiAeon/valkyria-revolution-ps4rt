"""FSR4 v07 manifest and asset integrity regressions."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from fsr4_assets import load_lock, verified_assets

TARGET = (
    "fsr4_shaders/"
    "fsr4_model_v07_i8_quality_4320_pass12.spv"
)


class FSR4AssetTests(unittest.TestCase):
    def altered_lock(self, change):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "packaging").mkdir()
            lock = json.loads(
                (ROOT / "packaging/fsr4-v07-assets.json").read_text()
            )
            change(lock)
            (root / "packaging/fsr4-v07-assets.json").write_text(
                json.dumps(lock)
            )
            with self.assertRaises(ValueError):
                load_lock(root)

    def test_complete_inventory(self):
        lock = load_lock(ROOT)
        self.assertEqual(
            lock["output_tiers"], ["1080", "2160", "4320"]
        )
        self.assertEqual(len(lock["files"]), 273)
        verified_assets(ROOT)

    def test_missing_stage_rejected(self):
        self.altered_lock(lambda lock: lock["files"].pop(TARGET))

    def test_missing_tier_rejected(self):
        self.altered_lock(
            lambda lock: lock["output_tiers"].remove("4320")
        )

    def test_invalid_checksum_metadata_rejected(self):
        self.altered_lock(
            lambda lock: lock["files"][TARGET].update(
                sha256="invalid"
            )
        )

    def test_corrupted_shader_rejected(self):
        original = Path.read_bytes
        corrupted = ROOT / TARGET

        def read_bytes(path):
            if path == corrupted:
                return b"corrupted"
            return original(path)

        with patch.object(Path, "read_bytes", read_bytes):
            with self.assertRaises(ValueError):
                verified_assets(ROOT)


if __name__ == "__main__":
    unittest.main()
