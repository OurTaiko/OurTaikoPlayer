#!/usr/bin/env python3
"""Create the Android updater manifest for an immutable OurTaiko release tag."""
import argparse
import hashlib
import json
from pathlib import Path
from urllib.parse import quote


def generate(apk: Path, version: int, tag: str, destination: Path):
    if not 1 < version <= 2100000000 or not tag or tag == "latest":
        raise ValueError("A positive version code and an immutable release tag are required")
    with apk.open("rb") as source:
        digest = hashlib.file_digest(source, "sha256").hexdigest()
    destination.mkdir(parents=True, exist_ok=True)
    data = {"schema": 1, "package": "org.ourtaiko.fanmade", "versionCode": version,
            "url": "https://github.com/OurTaiko/OurTaikoPlayer/releases/download/" + quote(tag, safe="") + "/OurTaiko-Android.apk",
            "sha256": digest, "size": apk.stat().st_size}
    (destination / "android-update.json").write_text(json.dumps(data, indent=2) + "\n")
    (destination / "checksums-android.sha256").write_text(digest + "  OurTaiko-Android.apk\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("apk", type=Path)
    parser.add_argument("--version-code", type=int, required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    generate(args.apk, args.version_code, args.tag, args.output)
