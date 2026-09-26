# OurTaiko

**Author and maintainer: OurTaiko.** Based on YataiDON by **Yono (Yonokid)**.

A TJA player and Taiko simulator written in C++ using the [raylib](https://www.raylib.com/) library.

![License](https://img.shields.io/badge/license-GPLv3-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux%20%7C%20Android%20%7C%20iOS-blue)
[![GitHub Stars](https://img.shields.io/github/stars/OurTaiko/OurTaikoPlayer?style=flat&label=stars)](https://github.com/OurTaiko/OurTaikoPlayer/stargazers)
[![Discord Members](https://img.shields.io/discord/722513061419810946.svg?label=Discord&logo=discord)](https://discord.gg/XHcVYKW)
[![Builds](https://github.com/OurTaiko/OurTaikoPlayer/actions/workflows/build.yml/badge.svg)](https://github.com/OurTaiko/OurTaikoPlayer/actions/workflows/build.yml)

## Features

- Cross-platform compatibility (Windows, Linux, macOS, Android, iOS source port)
- Controller Support
- Low latency audio via ASIO or WDM-KS
- Recursive and Dynamic Song Select Menu
- Multithreaded Input
- Multiple chart format support (TJA, Fumen, OSU/OSZ)
- Background video playback via FFmpeg
- Skin system with customizable graphics, sounds, and Lua scripting
- Branches (Normal/Expert/Master difficulty branches)
- Song search and favorites system
- Horizontal and vertical song select layout option
- Multi-language support
- 3D characters with animations and costume/color customization
- Online profile sync (scores, title, costume) via a network account

## Modes

- **1 Player**: Single player mode.
- **2 Player**: Up to 2 players simultaneously.
- **Dan Dojo**: Challenge mode for back to back songs.
- **Practice Mode**: Freely scrobble through a song and practice

## System Requirements

- **Windows**: Windows 10 or higher
- **macOS**: Built via CI, largely untested
- **Linux**: Ubuntu 20.04 or higher (other distributions may work but are untested)
- **Android**: Android 10 (API 29) or higher
- **iOS**: iOS 16.3 or higher; build from source with Xcode (see [iOS guide](ios/README.md))

## FAQ

Q: I'm on Windows and my latency is really high!<br>
A: Change your `device_type` in game settings to `WDM-KS`, `WASAPI`, or `ASIO` if your computer supports it

## Installation

### Pre-built Binaries

Download the latest release for your operating system from the [releases page](https://github.com/OurTaiko/OurTaikoPlayer/releases).

#### Windows

1. Install the [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe) from Microsoft
2. Run `OurTaiko.exe`

#### Linux

1. Run `./OurTaiko`
2. Optionally run `python3 install-desktop-entry.py` from the extracted package to add OurTaiko and its icon to your application menu. Run it again if you move the package.

#### Android

1. Install `OurTaiko-Android.apk` (enable "install from unknown sources" if needed)
2. Allow OurTaiko to access files when prompted. The app prepares bundled skins, songs, and default settings before starting the game; this resource installation runs only once.
3. Add your own songs to `/sdcard/OurTaiko/Songs/` and skins to `/sdcard/OurTaiko/Skins/`.

After successful extraction, later launches skip the initial resource scan.
Bundled skins have a separate version check: after an APK update,
the matching skins replace the previous bundled folders, which are backed up.
Custom skin folders, songs, settings and scores are preserved. Existing player
files are preserved during the one-time migration.
Settings are read on every launch: a missing file is recreated; an invalid file is
backed up as `config.toml.bak` (additional `.bak` suffixes preserve older backups)
and replaced with complete defaults. Android and iOS defaults enable touch controls
and VSync. Delete `config.toml` and restart to reset settings. To reinstall missing
bundled resources, close the app and remove `/sdcard/OurTaiko/.game-data-installed`;
existing player files are preserved.

Release APKs check OurTaiko GitHub Releases at launch and can download an update.
Choose **Start game** to skip, or **Install** when the verified APK is ready; Android
requires installation confirmation. There is no independent online skin updater.
See [Android updates](docs/ANDROID_UPDATES.md).

#### macOS

1. Extract `OurTaiko-macOS.zip` and open `OurTaiko.app`, or run `./OurTaiko` from its directory. Keep the app beside the executable and game data; it is a launcher for the portable package.

#### iOS

1. Download `OurTaiko-iOS-unsigned.ipa`, sign it for your device, and install it (see the [iOS guide](ios/README.md)).

The display name and build products use OurTaiko. Android uses
`org.ourtaiko.fanmade`; iOS uses `org.ourtaiko.player` as the application ID.
Android prepares files through
`org.ourtaiko.fanmade.OurTaikoLauncherActivity`, then starts the game in
`org.ourtaiko.fanmade.OurTaikoActivity`. The new application ID installs separately
from the original app; existing private app data is not transferred automatically.
Android reads shared assets only from `/sdcard/OurTaiko`; no legacy data directory
is read or migrated. The GitHub repository is `OurTaiko/OurTaikoPlayer`.

## Building from Source

- [GitHub Actions builds and Android release signing](docs/CI.md)

The wiki links below are the original upstream build guides.

- [Linux](https://github.com/Yonokid/YataiDON/wiki/Linux)
- [macOS](https://github.com/Yonokid/YataiDON/wiki/Mac-OS)
- [Windows (MSYS2/MinGW64)](<https://github.com/Yonokid/YataiDON/wiki/Windows-(MSYS2-MingW64)>)
- [Windows (MSVC)](<https://github.com/Yonokid/YataiDON/wiki/Windows-(MSVC)>) *(page pending)*
- [Android](https://github.com/Yonokid/YataiDON/wiki/Android)
- [iOS (Xcode, iPhone/iPad and Simulator)](ios/README.md)

## Controls

- Press **F1** during gameplay for quick restart
- Press **ESC** during any screen to go back
- Press **Space Bar** during song select to favorite a song
- Press **Left Control** or **Right Control** to skip songs by 7
- Press **F5** during song select to refresh
- Generic drum keybinds can be customized in `config.toml` or through the in-game settings menu

## Contributing

Contributions are welcome! Please keep in mind:

- Check the [issues page](https://github.com/OurTaiko/OurTaikoPlayer/issues) for enhancements and bugs before starting work
- Feel free to open new issues for bugs or feature requests

## Known Issues

See the [issues page](https://github.com/OurTaiko/OurTaikoPlayer/issues) or the Discord for current bugs and planned enhancements.

## License

OurTaiko is a modified version of [YataiDON](https://github.com/Yonokid/YataiDON),
originally authored by Yono (Yonokid). It retains the original GNU General Public
License, version 3, in [LICENSE](LICENSE). Original author attribution and third-party
notices are preserved; see [NOTICE](NOTICE). OurTaiko is the author and maintainer
of this modified version. Release packages include both files.

## Acknowledgments

Dependencies used in the project:

- [raylib](https://www.raylib.com/) - A simple and easy-to-use library to enjoy videogames programming.
- [SDL3](https://github.com/libsdl-org/SDL) - Cross-platform development library for audio, input, and graphics.
- [SQLite](https://www.sqlite.org/) - Self-contained, serverless SQL database engine.
- [RapidJSON](https://github.com/Tencent/rapidjson) - Fast JSON parser/generator for C++.
- [toml++](https://github.com/marzer/tomlplusplus) - TOML config file parser and writer for C++.
- [spdlog](https://github.com/gabime/spdlog) - Fast C++ logging library.
- [Lua](https://github.com/marovira/lua) - Lightweight, embeddable scripting language.
- [sol2](https://github.com/ThePhD/sol2) - C++ binding library for Lua.
- [cpptrace](https://github.com/jeremy-rifkin/cpptrace) - Stacktrace library for C++.
- [libsndfile](https://github.com/libsndfile/libsndfile) - Library for reading and writing audio files.
- [libsamplerate](https://github.com/libsndfile/libsamplerate) - Sample rate converter for audio.
- [FFmpeg](https://ffmpeg.org/) - Complete, cross-platform multimedia framework.
- [PortAudio](http://www.portaudio.com/) - Cross-platform audio I/O library.
- [miniz](https://github.com/richgel999/miniz) - Single-file ZIP/DEFLATE library.

People (in no particular order):

- [IID](https://github.com/IepIweidieng/)
- [mc08](https://github.com/splitlane/)
- [QBaraki](https://github.com/QBaraki)
- [KabanFriends](https://github.com/KabanFriends/)
- [DragonRatTiger](https://github.com/DragonRatTiger/)
- [Komi](https://github.com/0auBSQ/)
- [sumandrew](https://github.com/somepin)
- [WallK](https://github.com/WallK)
- [dpkgluci](https://github.com/germe-deb)
- [chuchy](https://github.com/magickale) (osu parser)
- [churrochef](github.com/churro-chef) (vertical scroll)
- [cainan](https://github.com/cainan-c/) (3d don chan)
- [RyutoSetsujin](https://github.com/RyutoSetsujin/)
- [Nyannurs](https://github.com/nyannurs) (testing)
- [fehpizza](https://github.com/fehh44) (logo)
- Radix (testing)
- Thonktheta (testing)
- SomehowScarlet (nix os)
- SunnyDeez (testing)

---
