#!/usr/bin/env python3
"""Headless regression checks for the curated upstream changes (Clang, macOS/Linux)."""
import argparse
import platform
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(__doc__)
parser.add_argument("--deps", type=Path, default=ROOT / ".cmake-deps")
parser.add_argument("--skins", type=Path, default=ROOT / "Skins")
args = parser.parse_args()
deps = args.deps.resolve()
skins = args.skins.resolve()


def run(command):
    result = subprocess.run([str(arg) for arg in command], cwd=ROOT,
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        print(result.stdout, end="")
        raise SystemExit(result.returncode)
    if result.stdout and command[0] != "clang++":
        print(result.stdout, end="")


with tempfile.TemporaryDirectory(prefix="ourtaiko-upstream-") as directory:
    out = Path(directory)
    run([sys.executable, ROOT / "tools/gen_skin_config.py",
         skins / "PyTaikoGreen/Graphics/skin_config.json", out / "skin_config_generated.h"])
    common = ["clang++", "-std=c++20", "-fsanitize=address,undefined",
              "-ffunction-sections", "-fdata-sections",
              "-Wl,-dead_strip" if platform.system() == "Darwin" else "-Wl,--gc-sections",
              "-I" + str(out)]
    for project, subdir in [("raylib", "src"), ("spdlog", "include"),
                            ("rapidjson", "include"), ("tomlplusplus", "include"),
                            ("sol2", "include"), ("lua", "src")]:
        common.append("-I" + str(deps / (project + "-src") / subdir))
    checks = [
        ("exams", ["tests/dan/exams.cpp"], [ROOT / "Songs"]),
        ("difficulty", ["tests/song_select/difficulty_selection.cpp"], []),
        ("glyphs", ["tests/skins/glyph_fallback.cpp", "src/libs/text.cpp"], []),
        ("textures", ["tests/skins/texture_fallback.cpp", "src/libs/texture.cpp"], []),
    ]
    for name, sources, arguments in checks:
        binary = out / name
        run(common + sources + ["-o", binary])
        run([binary, *arguments])
