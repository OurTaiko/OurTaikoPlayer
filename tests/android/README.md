# Android bundled game data checks

Run the real installer on a temporary directory with a small in-memory ZIP archive:

```sh
javac --release 11 -d /tmp/ourtaiko-android-tests \
  android/app/src/main/java/org/ourtaiko/fanmade/GameDataInstaller.java \
  tests/android/GameDataInstallerTest.java
java -cp /tmp/ourtaiko-android-tests org.ourtaiko.fanmade.GameDataInstallerTest
```

Covers first install, zero APK reads on repeat launch (even after resources or
config are deleted or the bundle changes), one-time legacy migration, user file
preservation, Unicode paths, interrupted copy cleanup/retry, incomplete archive
rejection, unsafe ZIP paths, and cancellation. Only its temporary data is removed.
Configuration recovery is tested separately in [the settings fixture](../config/README.md).

With Android SDK/NDK and the skin submodules available, check Gradle packaging:

```sh
cd android
./gradlew :app:copyGameAssets :app:mergeDebugAssets :app:compileDebugJavaWithJavac
```

Inspect `app/build/generated/game_assets/GameData.zip`: only `Skins/PyTaikoGreen`,
Songs, config.toml, LICENSE and NOTICE should be present, with no `.git` metadata.
The other two skins need not exist in the checkout. Use `-PskinProfile=all` to
package all three stock skins; see [packaging profiles](../../packaging/README.md).
`GameData.count` must equal the number of non-directory ZIP entries. The APK must
store the ZIP without a second compression layer. The packaged config enables
touch input and VSync; repository config stays unchanged. The obsolete generated
`GameData/` directory must be absent. Removing an input asset and rerunning
`copyGameAssets` should remove it from the ZIP without touching device data.

Device checks (Android 10 and Android 11+):

1. Fresh install: deny storage access. The game must not start or crash; the
   launcher explains the permission and offers Retry/Close.
2. Grant access and return: resource preparation runs off the UI thread, then
   starts the game. Inspect `/sdcard/OurTaiko/config.toml` and `Skins/`.
3. Edit config/skin files, add a song and restart: custom content is preserved.
4. Fully close the game, delete config.toml, then launch: native settings defaults
   are saved, the touch drum appears, and no resource extraction occurs. Repeat
   with malformed TOML or an invalid key: inspect the exact `.bak` and new settings.
   Delete a bundled skin file: later launches must not start resource preparation.
   Close the app and remove `.game-data-installed` to explicitly reinstall missing
   resources. Existing player files must remain intact.
5. Interrupt preparation or run out of storage: retry after resolving the issue;
   partially copied files must not be treated as complete.
6. Background the launcher while preparing files: it starts the game only after
   returning to the foreground. The older `/sdcard/YataiDON` directory is unused.

## HTTPS transport regression (macOS host)

The harness compiles the actual Android branch of `fanmade.cpp` against OpenSSL
curl. `fanmade_asset_stub.cpp` substitutes only SDL's APK file reader with a
fixture-controlled file. This checks TLS and transport behavior, not Android
AssetManager, an NDK build, or the full game UI.

With the fetched CPR/curl/SDL sources and Homebrew OpenSSL available, run from the
repository root (all build products go to `/tmp`):

```sh
cmake -S .cmake-deps/cpr-src -B /tmp/ourtaiko-network-cpr \
  -DCPR_USE_SYSTEM_CURL=ON -DCPR_BUILD_TESTS=OFF \
  -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/ourtaiko-network-cpr --parallel 4

cmake -S .cmake-deps/curl-src -B /tmp/ourtaiko-openssl-curl \
  -DCURL_USE_OPENSSL=ON -DCURL_USE_SECTRANSP=OFF \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)" \
  -DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none \
  -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=OFF \
  -DCURL_USE_LIBPSL=OFF -DCURL_USE_LIBSSH2=OFF -DUSE_NGHTTP2=OFF -DUSE_LIBIDN2=OFF
cmake --build /tmp/ourtaiko-openssl-curl --parallel 4

clang++ -std=c++20 -DFANMADE_NETWORK -D__ANDROID__ \
  -I.cmake-deps/sdl3-src/include -I.cmake-deps/cpr-src/include \
  -I/tmp/ourtaiko-network-cpr/cpr_generated_includes \
  -I.cmake-deps/curl-src/include -I.cmake-deps/rapidjson-src/include \
  tests/fanmade/transport.cpp tests/android/fanmade_asset_stub.cpp \
  /tmp/ourtaiko-network-cpr/lib/libcpr.a /tmp/ourtaiko-openssl-curl/lib/libcurl.a \
  -L"$(brew --prefix openssl@3)/lib" -lssl -lcrypto -lz -liconv -lldap -llber \
  -framework CoreFoundation -framework SystemConfiguration \
  -o /tmp/ourtaiko-network-transport
python3 tests/fanmade/transport.py /tmp/ourtaiko-network-transport
```

The test creates ephemeral localhost HTTP/TLS servers and a temporary CA. It
checks trusted TLS, unknown issuer rejection, hostname verification, missing /
empty / invalid CA data, exact response byte limits, overflow, HTTP errors, and
cancellation. No live accounts or game data are used.

See [the Android network repair log](../../docs/ANDROID_NETWORK_FIX.md) for the
separate device probe results and remaining APK verification.
