"""Validate the publishing boundary using trusted source and signing-job output."""
import hashlib
import json
import os
from pathlib import Path
import re


def validate(root: Path, assets: Path, expected_hash: str, commit: str) -> str:
    if not re.fullmatch(r"[a-fA-F0-9]{64}", expected_hash):
        raise ValueError("Missing signing-job digest")
    version = re.search(r"project\(gaze-grid\s+VERSION\s+(\d+\.\d\.\d)\s",
                        (root / "CMakeLists.txt").read_text()).group(1)
    msi_name = f"gaze-grid-{version}-Windows-x64.msi"
    expected_names = {msi_name, "gaze-grid.cer", "SHA256SUMS.txt", "RELEASE-NOTES.md"}
    if {p.name for p in assets.iterdir()} != expected_names:
        raise ValueError("Unexpected release assets")
    if any(p.is_symlink() or not p.is_file() for p in assets.iterdir()):
        raise ValueError("Release assets must be regular files")
    trust = json.loads((root / "platform/windows/release-trust.json").read_text())
    digests = {name: hashlib.sha256((assets / name).read_bytes()).hexdigest()
               for name in (msi_name, "gaze-grid.cer")}
    if digests[msi_name] != expected_hash.lower():
        raise ValueError("MSI differs from the signing-job output")
    if digests["gaze-grid.cer"] != trust["certificateSha256"]:
        raise ValueError("Publisher certificate mismatch")
    checksums = [f"{digest}  {name}" for name, digest in digests.items()]
    if (assets / "SHA256SUMS.txt").read_text().splitlines() != checksums:
        raise ValueError("Checksum manifest mismatch")
    # Use notes from trusted source, never executable-test output.
    notes = root / f"docs/releases/{version}.md"
    changes = notes.read_text(encoding="utf-8-sig") if notes.is_file() else f"gaze-grid {version} for Windows x64."
    (assets / "RELEASE-NOTES.md").write_text(f"{changes}\n\nSource commit: {commit}\n", encoding="utf-8")
    return version


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[2]
    version = validate(root, root / "release", os.environ["EXPECTED_MSI_HASH"], os.environ["GITHUB_SHA"])
    if os.environ["RELEASE_VERSION"] != version or os.environ["RELEASE_TAG"] != f"v{version}":
        raise SystemExit("Release metadata differs from trusted source")
