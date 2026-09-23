#!/usr/bin/env python3
"""Exercise real packaging scripts against isolated assets; no SDK is required."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

REPO = Path(__file__).resolve().parents[2]
GREEN = "PyTaikoGreen"
ALL = {GREEN, "YataiDONNijiiro", "YataiDONRed"}


def write(root, name, content="fixture"):
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def run(command, *, cwd=None, succeeds=True):
    env = dict(os.environ)
    env.pop("OURTAIKO_SKIN_PROFILE", None)
    result = subprocess.run(command, cwd=cwd, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    assert (result.returncode == 0) == succeeds, result.stdout


def cmake(source, output, profile=None, *, ios=False, succeeds=True):
    command = ["cmake", f"-DSKINS_DIR={source / 'Skins'}"]
    if ios:
        command += [f"-DSOURCE_DIR={source}", f"-DSONGS_DIR={source / 'Songs'}",
                    f"-DDEST_DIR={output}"]
    else:
        command += [f"-DSKINS_DEST_DIR={output}"]
    if profile is not None:
        command += [f"-DOURTAIKO_SKIN_PROFILE={profile}"]
    command += ["-P", str(REPO / "cmake" / ("ios_assets.cmake" if ios else "package_skins.cmake"))]
    run(command, succeeds=succeeds)


def check_tree(path, skins):
    assert {p.name for p in path.iterdir()} == skins
    files = {p.relative_to(path).as_posix() for p in path.rglob("*") if p.is_file()}
    for category in ("Graphics", "Scripts", "Sounds", "Models", "Videos"):
        assert f"{GREEN}/{category}/日本語.txt" in files
    assert not any(".git" in name or ".DS_Store" in name for name in files)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gradle", help="Optional Gradle executable for Android ZIP checks")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="ourtaiko-packaging-") as directory:
        root = Path(directory)
        source = root / "source"
        original_config = "[general]\nskin = 'PyTaikoGreen'\ntouch_input = false\nvsync = false\n"
        write(source, "config.toml", original_config)
        for name in ("LICENSE", "NOTICE", "Songs/日本語/song.tja", "shader/es/test.glsl"):
            write(source, name)
        write(source, f"Skins/{GREEN}/Graphics/skin_config.json", "{}")
        for category in ("Graphics", "Scripts", "Sounds", "Models", "Videos"):
            write(source, f"Skins/{GREEN}/{category}/日本語.txt")
        for metadata in (".git", ".gitignore", "Graphics/.DS_Store"):
            write(source, f"Skins/{GREEN}/{metadata}")

        # A checkout containing only Green succeeds on both CMake entry points.
        cmake(source, root / "desktop")
        check_tree(root / "desktop", {GREEN})
        cmake(source, root / "ios", ios=True)
        check_tree(root / "ios/Skins", {GREEN})
        config = (root / "ios/config.toml").read_text()
        assert "touch_input = true" in config and "vsync = true" in config
        assert (root / "ios/.shader-version").is_file()
        for profile in ("all", "unknown", "../green"):
            cmake(source, root / "desktop", profile, succeeds=False)
        check_tree(root / "desktop", {GREEN})  # Failed selection preserves previous output.
        cmake(source, source / "Skins", succeeds=False)
        cmake(source, source, succeeds=False)
        for skin in ALL - {GREEN}:
            write(source, f"Skins/{skin}/Graphics/skin_config.json", "{}")
        for ios in (False, True):
            output = root / ("ios" if ios else "desktop")
            skins = output / "Skins" if ios else output
            cmake(source, output, "all", ios=ios)
            check_tree(skins, ALL)
            cmake(source, output, ios=ios)
            check_tree(skins, {GREEN})
        assert (source / "config.toml").read_text() == original_config
        assert {p.name for p in (source / "Skins").iterdir()} == ALL
        print("PASS: desktop/iOS defaults, full profile, stale cleanup, missing assets, source preservation")

        if args.gradle:
            shutil.copytree(REPO / "packaging", source / "packaging")
            project = source / "android"
            write(project, "settings.gradle", "rootProject.name = 'skin-packaging-test'\n")
            script = str(REPO / "android/app/game-assets.gradle").replace("'", "\\'")
            write(project, "build.gradle", f"apply from: '{script}'\n")
            for skin in ALL - {GREEN}:
                shutil.rmtree(source / "Skins" / skin)

            def gradle(profile=None, succeeds=True):
                command = [args.gradle, "--offline", "--no-daemon", "--console=plain", "copyGameAssets"]
                if profile is not None:
                    command += [f"-PskinProfile={profile}"]
                run(command, cwd=project, succeeds=succeeds)

            def check_zip(skins):
                generated = project / "build/generated/game_assets"
                with zipfile.ZipFile(generated / "GameData.zip") as archive:
                    files = [n for n in archive.namelist() if not n.endswith("/")]
                    assert {n.split("/")[1] for n in files if n.startswith("Skins/")} == skins
                    assert not any(".git" in n or ".DS_Store" in n for n in files)
                    assert int((generated / "GameData.count").read_text()) == len(files)
                    for category in ("Graphics", "Scripts", "Sounds", "Models", "Videos"):
                        assert f"Skins/{GREEN}/{category}/日本語.txt" in files
                    config = archive.read("config.toml").decode()
                    assert "touch_input = true" in config and "vsync = true" in config
                    assert {"Songs/日本語/song.tja", "NOTICE", "LICENSE"}.issubset(files)
                assert (generated / "shader/es/test.glsl").is_file()

            gradle()
            check_zip({GREEN})
            gradle("all", succeeds=False)
            for skin in ALL - {GREEN}:
                write(source, f"Skins/{skin}/Graphics/skin_config.json", "{}")
            gradle("all")
            check_zip(ALL)
            gradle()
            check_zip({GREEN})
            write(source, f"Skins/{GREEN}/Scripts/new.lua")
            gradle()
            (source / "Skins" / GREEN / "Scripts/new.lua").unlink()
            gradle()
            with zipfile.ZipFile(project / "build/generated/game_assets/GameData.zip") as archive:
                assert f"Skins/{GREEN}/Scripts/new.lua" not in archive.namelist()
            assert (source / "config.toml").read_text() == original_config
            print("PASS: Android Green-only checkout, full profile, incremental ZIP cleanup, counts, config")


if __name__ == "__main__":
    main()
