# iOS port

The iOS target shares the C++ game, SDL3 renderer/input/audio, OpenGL ES 3 shaders,
FFmpeg decoding, and touch drum controls with Android. It builds a landscape app
for iPhone and iPad running iOS 16.3 or later. This is a source port; physical-device
latency and distribution signing still need validation.

## Build on a Mac

Install Xcode (including the iOS SDK) and CMake 3.24 or newer. Select the full Xcode
installation with `sudo xcode-select -s /Applications/Xcode.app/Contents/Developer`
if the command-line tools are selected instead. Python 3, Git, and curl are also
required. The first build downloads and compiles dependencies.

Populate the skin submodules before building on a fresh checkout:

```sh
git submodule update --init --recursive
```

Do not run that command over skin changes you want to keep. If your assets live
elsewhere, pass `-DYATAIDON_SKINS_DIR=/absolute/path/to/Skins` to the build script.
Use `PyTaikoGreen` from the matching submodule revision: native rendering and Lua
scripts must use the same texture API. By default only Green is bundled, including
its graphics, scripts, sounds, models and videos. Pass `-DOURTAIKO_SKIN_PROFILE=all`
to include all three stock skins; see [packaging profiles](../packaging/README.md).
`-DIOS_SONGS_DIR=/absolute/path/to/Songs` changes the bundled
song library. Large skin videos increase both app size and first-launch copy time.

### Simulator

```sh
./build_ios.sh simulator
open build-ios-simulator/OurTaiko.xcodeproj
```

Select the OurTaiko scheme and an installed iPhone or iPad Simulator, then Run.
The script uses your Mac's architecture; set `IOS_ARCH=x86_64` on Intel if needed.
No Apple development team is required for the unsigned Simulator build.

### iPhone or iPad

```sh
IOS_DEVELOPMENT_TEAM=253AX6B3P2 \
IOS_BUNDLE_IDENTIFIER=org.ourtaiko.player \
./build_ios.sh device
open build-ios-device/OurTaiko.xcodeproj
```

The local device build defaults to **Hoshino Network LLC (`253AX6B3P2`)**.
Set `IOS_DEVELOPMENT_TEAM` to another team to override it, or explicitly set it to
an empty string for an unsigned build. CI keeps that variable empty.

Select your connected device and the OurTaiko scheme. Check Signing & Capabilities
and select your Apple team, then Run. Enable Developer Mode on the device when
Xcode requests it. With `IOS_DEVELOPMENT_TEAM=""`, the script builds an unsigned `.app` for compile
checks; it cannot be installed on a physical device until it is signed.
Signing is disabled only for that command-line build, not in the generated Xcode
project. Select your team and a unique Bundle Identifier in Xcode before running.
Pass these same values to the script on subsequent builds so regeneration keeps
your signing configuration. If an older generated project reports “No code
signature found”, regenerate it and rebuild with signing enabled.

The build does not publish to TestFlight or the App Store. Distribution requires
appropriate signing, artwork, and rights to the assets you include.

## Build an unsigned IPA with GitHub Actions

The existing [Release workflow](../.github/workflows/build.yml) includes a
`build-ios` job. In GitHub, open **Actions → Build OurTaiko (Release) → Run workflow**
and select the branch containing the iOS changes. This runs all platform builds.

The iOS job uses a `macos-15` runner and `./build_ios.sh device` to build an ARM64
Release app for iOS 16.3 or later, with Bundle ID `org.ourtaiko.player` and code signing
disabled. It packages the app as `Payload/OurTaiko.app` inside
`OurTaiko-iOS-unsigned.ipa`, alongside `checksums-ios.sha256`. No Apple certificate,
provisioning profile, or App Store Connect credentials are required. There is no
TestFlight or App Store upload step. Sign the downloaded IPA with your own signing
tool and credentials before installing it on an iPhone or iPad.

The job prepares the pinned Green skin through the shared skin action. Fanmade
networking is enabled by default and uses iOS system settings at runtime; no API
credentials are embedded in the build.

Download the `OurTaiko-iOS` artifact from the workflow run after the iOS job
succeeds. When `publish_release` is selected and all platform builds succeed, the new
explicit `release_tag` receives the unsigned IPA and its SHA-256 checksum file. An iOS failure is
included in the build summary and prevents that combined Release from publishing.

The FFmpeg cache is separate from macOS and Simulator builds and includes the
host architecture, device target, minimum iOS version, Xcode version/build, and
FFmpeg build-script hash. The workflow logs the selected Xcode and iOS SDK versions.
Runner images may update; available toolchains are listed in GitHub's
[runner image documentation](https://github.com/actions/runner-images/blob/main/images/macos/macos-15-arm64-Readme.md).
The artifact uses
[compression level 0](https://github.com/actions/upload-artifact#altering-compressions-level-speed-v-size)
because the IPA is already a compressed ZIP archive.

## Songs, skins, and saves

On first launch, bundled resources are copied into the app's Documents directory.
Open **Files → On My iPhone/iPad → OurTaiko**, or use Finder's device File Sharing.
Add song folders under `Songs`, and skins under `Skins`. TJA files and their audio
files must stay together. Restart the app to rescan new content. The shared folder
also contains `config.toml`, score databases, caches, and `latest.log`.

Existing files, including settings and scores, are preserved on app upgrades.
A `.game-data-installed` marker skips resource traversal after successful initial
installation (including subsequent app upgrades). Remove this marker while the
app is closed to reinstall missing bundled files. Bundled shaders alone are
refreshed when their build-time content hash changes after an update. Uninstalling the app deletes its data container, so copy
out any songs and scores you want to keep first.

The September 25 upstream merge uses the new `Fonts/` layout and matching skin
scripts. During this testing phase, use a fresh installation of this version;
no legacy font/skin migration is provided. Back up any songs, scores and settings
before uninstalling. The new iOS bundle identifier is `org.ourtaiko.player`.

Settings are read on each launch. Missing settings are recreated; invalid settings
are backed up with an unused `.bak` suffix and replaced with complete defaults.
Touch input and VSync are enabled in both bundled and recovered mobile defaults. Tap inside the drum for Don
and outside for Kat; left and right halves retain the Android mappings. The top
**Back** control replaces Android's system Back button. **Pause** toggles pause in
single-player and two-player gameplay. Song search and text settings use the iOS
keyboard. SDL also handles supported game controllers.

The UIKit animation callback drives rendering. On backgrounding, the game clock
and SDL audio stream pause; foregrounding resumes them together. Frame rate follows
the display callback rather than the desktop FPS limiter.

## Dependency builds and options

`build_ios.sh` builds FFmpeg automatically when its static libraries are absent.
Device and Simulator libraries are separate even when both use ARM64:

```sh
IOS_SDK=iphoneos tools/build_ffmpeg_ios.sh
IOS_SDK=iphonesimulator tools/build_ffmpeg_ios.sh
cmake --preset ios-simulator
cmake --build --preset ios-simulator
```

The presets assume ARM64. Override `CMAKE_OSX_ARCHITECTURES` and
`IOS_FFMPEG_PREFIX` together for Intel Simulators. The scripts accept
`IOS_DEPLOYMENT_TARGET` (default `16.3`), `IOS_FFMPEG_PREFIX`, `JOBS`, `CMAKE`, and
`CONFIGURATION` (default `Release`). Use a separate build directory when changing
SDK or architecture. `CONFIGURATION=Debug ./build_ios.sh simulator` builds symbols
without the desktop sanitizer flags.
After an Xcode upgrade, configuration automatically clears cached dependency paths
inside removed SDK directories so they are discovered in the current SDK.

## Audio latency

The iOS build retains the reduced AudioQueue buffering and 5 ms hardware I/O
preference validated on an iPad Pro speaker. Temporary latency instrumentation
and its build switch have been removed. See [the latency fix log](LATENCY_FIX.md)
for the investigation, measurements, and instructions for rebuilding diagnostics
if the problem recurs.

For missing audio at the beginning of a song, see [the song startup fix log](SONG_START_FIX.md).
Gameplay now waits for asynchronous audio loading before advancing the chart and
opening transition, so slower decoding does not skip the beginning of the music.

## Online services

iOS uses OurTaiko Fanmade for chart discovery, file downloads, and score sync.
CPR and its pinned curl are built separately for Device and Simulator. HTTPS uses
Apple's Secure Transport and system trust store with certificate verification
required; no Android CA bundle or host macOS OpenSSL installation is needed.

Networking is enabled by default (`FANMADE_NETWORK=ON`). Configure up to five
independent endpoints in **Settings → Apps → OurTaiko**, then fully restart the
game. Each server page contains an enable switch, name, base URL, username,
secure password field, and optional HTTP proxy. New installations start with all
servers disabled; server 1 has the OurTaiko Fanmade public URL prefilled.

On the first upgrade, existing TOML servers are imported once (up to five), unless
the user has already configured system settings. Subsequent iOS online reads use
NSUserDefaults only; disabling every server does not restore old TOML settings.
Saving ordinary game settings on iOS does not write online credentials back to
TOML. Other platforms continue reading and writing `network.servers` in TOML.
Other iOS game settings remain in Documents/config.toml.

See [Fanmade integration](../docs/FANMADE.md) for caching, score upload behavior,
and native fixture commands. Local gameplay and saves remain available offline.
Optional Fumen support still requires the same seeds as other platforms.
The Settings.bundle is a signed app resource and is never copied into Documents.


Platform references: [SDL's iOS integration](https://wiki.libsdl.org/SDL3/README-ios)
and [CMake Apple cross-compilation](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-ios-tvos-visionos-or-watchos).

## Validation

The ARM64 Release build was compiled with Xcode and exercised on an iPhone 17 Pro
Simulator running iOS 26.5. Checks covered first-run asset setup, SDL/CoreAudio
initialization, touch navigation through player entry and song selection, the
bundled TRIPLE HELIX chart, 3D rendering, the Pause control, and returning from the
background. The clock's suspend/resume behavior and the local SQLite database's
integrity were also checked. Physical-device performance, signing, and all alternate
skins have not been validated.
