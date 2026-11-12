import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("release_assets", Path(__file__).resolve().parents[1] / "platform/windows/Validate-ReleaseAssets.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ReleaseAssetsTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.assets = self.root / "release"
        self.assets.mkdir()
        (self.root / "CMakeLists.txt").write_text("project(gaze-grid VERSION 1.2.3 LANGUAGES CXX)")
        (self.root / "platform/windows").mkdir(parents=True)
        self.msi = self.assets / "gaze-grid-1.2.3-Windows-x64.msi"
        self.msi.write_bytes(b"signed MSI from isolated signing job")
        (self.assets / "gaze-grid.cer").write_bytes(b"pinned publisher certificate")
        self.digest = hashlib.sha256(self.msi.read_bytes()).hexdigest()
        cert_hash = hashlib.sha256((self.assets / "gaze-grid.cer").read_bytes()).hexdigest()
        (self.root / "platform/windows/release-trust.json").write_text(json.dumps({"certificateSha256": cert_hash}))
        (self.assets / "SHA256SUMS.txt").write_text(f"{self.digest}  {self.msi.name}\n{cert_hash}  gaze-grid.cer\n")
        (self.assets / "RELEASE-NOTES.md").write_text("Untrusted notes from installer execution")

    def validate(self):
        return module.validate(self.root, self.assets, self.digest, "trusted-commit")

    def test_valid_assets_use_trusted_notes(self):
        self.assertEqual(self.validate(), "1.2.3")
        self.assertNotIn("Untrusted", (self.assets / "RELEASE-NOTES.md").read_text())
        self.assertIn("trusted-commit", (self.assets / "RELEASE-NOTES.md").read_text())

    def test_replaced_msi_rejected_even_with_updated_checksums(self):
        self.msi.write_bytes(b"replacement after installer test")
        manifest = self.assets / "SHA256SUMS.txt"
        manifest.write_text(manifest.read_text().replace(self.digest, hashlib.sha256(self.msi.read_bytes()).hexdigest()))
        with self.assertRaisesRegex(ValueError, "signing-job"):
            self.validate()

    def test_replaced_certificate_rejected(self):
        (self.assets / "gaze-grid.cer").write_bytes(b"other identity")
        with self.assertRaisesRegex(ValueError, "certificate"):
            self.validate()

    def test_extra_asset_rejected(self):
        (self.assets / "extra.msi").write_bytes(b"unreviewed")
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            self.validate()

    def test_checksum_path_injection_rejected(self):
        (self.assets / "SHA256SUMS.txt").write_text("0" * 64 + "  ../outside\n")
        with self.assertRaisesRegex(ValueError, "manifest"):
            self.validate()


if __name__ == "__main__":
    unittest.main()
