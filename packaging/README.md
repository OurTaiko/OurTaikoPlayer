# Skin packaging profiles

Release and Debug packages default to `skins-green.txt`: only `PyTaikoGreen` is
bundled. Its entire directory is included (Graphics, Scripts, Sounds, Models and
Videos), excluding Git metadata and `.DS_Store`. `skins-all.txt` includes the three
stock skins. Profiles list one directory name per line and are shared by Android,
iOS and desktop CI packaging; `config.toml` and `CMakePresets.json` are unchanged.
Any skin selected by a profile must have `Graphics/skin_config.json`. Green-only
builds do not require either of the other skin submodules to be populated.

Default commands remain unchanged. To select the full package:

```sh
# iOS (extra CMake arguments take precedence over the script's default)
./build_ios.sh simulator -DOURTAIKO_SKIN_PROFILE=all

# Android, from android/
./gradlew assembleRelease -PskinProfile=all

# Desktop packaging, from the repository root
cmake -DSKINS_DIR="$PWD/Skins" -DSKINS_DEST_DIR="$PWD/release/OurTaiko/Skins" \
  -DOURTAIKO_SKIN_PROFILE=all -P cmake/package_skins.cmake
```

`OURTAIKO_SKIN_PROFILE=all` in the environment also selects the full profile for
the build scripts and desktop packaging. Direct iOS CMake configuration uses the
`OURTAIKO_SKIN_PROFILE` cache option (default `green`). The next default
`build_ios.sh` invocation selects Green again. Local desktop builds still run
against the checkout's assets; only release staging is filtered. Release and Debug
CI explicitly select `green`; their shared preparation action fetches, caches and
checks only `PyTaikoGreen`, with a separate cache key from older three-skin builds.

Switching profiles removes stale skins from generated packages. It does not
delete source skins or anything already installed on a user's device. iOS retains its existing one-time setup: copy/extract bundled content on
first launch, preserve existing files, then skip extraction while
`.game-data-installed` exists. An upgrade to a Green-only package therefore keeps
previously installed skins and the user's selected skin. On Android, a separate bundled-skin revision applies the installed APK’s matching
skins before SDL starts and backs up replaced folders; see [Android updates](../docs/ANDROID_UPDATES.md).
Missing manually removed
skins are not repaired automatically on unchanged versions; no new runtime skin fallback is introduced.

## Validation

Run `python3 tests/packaging/run.py` for isolated desktop/iOS packaging checks.
Add `--gradle /absolute/path/to/gradle` with a Gradle-compatible `JAVA_HOME` to
exercise the real Android ZIP tasks without requiring the Android SDK. The fixture
covers a checkout containing only Green, both profiles, switching back to Green,
stale asset removal, metadata exclusions, mobile config overrides, archive counts
and preservation of source assets/settings. Android extraction behavior is covered
by [the installer fixture](../tests/android/README.md).

Verified on September 22, 2026: both packaging fixtures (Gradle 8.9 / Java 17),
the Android installer fixture, and an iOS Simulator Release build. The final
`OurTaiko.app/GameData/Skins` contains only `PyTaikoGreen`, including after rebuilding
over a previous three-skin App bundle. This does not constitute an Android APK
build or a new device gameplay test.
