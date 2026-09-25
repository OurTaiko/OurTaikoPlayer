# Android application updates

Release builds check the latest **OurTaiko/OurTaiko** GitHub Release after local
resource preparation, before starting SDL. Debug builds skip this check. The Java
updater is independent of Fanmade accounts, credentials and `FANMADE_NETWORK`.
iOS does not use this updater.

The launcher downloads `android-update.json`, compares its version code with the
actually installed package, and downloads the APK from the immutable release URL
in that manifest. Transfers use Android's HTTPS trust store, bounded metadata,
streamed APK writes, timeouts, size limits and SHA-256 validation. The downloaded
APK must have the same package ID and current signing certificates as the installed
app, and the declared newer version. Failed or interrupted transfers never replace
a verified cached APK. Offline/check failures continue into the game.

The user can choose **Start game** to cancel/skip the check or download. Once an
update is ready, **Install** opens the Android package installer; if necessary,
the launcher first opens this app's “install unknown apps” permission page. The
system owns installation confirmation. Downloading or cancelling installation
does not mark a version installed: the next launch checks the actual installed
version and can reuse the verified download. The FileProvider exposes only the
private cache's `updates/` directory, not shared songs or settings.

## Bundled skins

There is **no online skin repository updater**. `.skin-repo` and moving upstream
branches are not consulted. The skins inside the installed APK are the versions
pinned by the main repository and selected by the packaging profile (Green by
default). After an APK change, the launcher stages those skins from `GameData.zip`
and checks the declared file counts and configuration files before replacing the
bundled skin directories. SDL starts only after preparation succeeds.

Existing bundled skin directories, including any local edits, are moved into
`/sdcard/OurTaiko/.skin-backups/` before replacement. A failed replacement restores
that directory when possible; incomplete setup has no completion marker and is
retried at the next launch. Custom skin directories outside the bundled profile,
songs, settings and scores are preserved. Removed profile members are not deleted.
Backups are retained; the application does not automatically prune them.

`GameData.skins` contains the packaged archive hash and per-skin file counts.
`.bundled-skins-version` avoids another archive scan on unchanged launches. A new
installation reuses the initial resource copy instead of extracting skins twice.
Deleting `.bundled-skins-version` requests a bundled-skin reinstall; deleting only
`.game-data-installed` retains the existing preserve-files behavior.

## Publishing

The Release workflow assigns an Android version code from UTC seconds since
2020-01-01 and passes it to Gradle as `-PourtaikoVersionCode`. Local builds default
to version code 1; use that property when building a release manually. Publish
with the same long-lived signing key and an increasing version code. Certificate
rotation is not implemented by this updater.

`tools/android_update_manifest.py` generates `android-update.json` and
`checksums-android.sha256` beside the APK. Both are included in the artifact and
published Release. The manifest uses the workflow's explicit immutable release
tag, so a changing `latest` redirect cannot pair a manifest with a different APK.
An existing Release without this metadata simply does not offer an in-app update.
Publishing remains manual; merging or pushing does not publish a Release.

## Verification

`tests/android/UpdateFilesTest.java` covers valid downloads, hash/size rejection,
interruption cleanup, HTTPS-only URLs, matching-revision no-op, bundled skin
replacement/backup, custom file preservation, invalid paths, incomplete archives,
and retry after interruption. `tests/packaging/run.py --gradle ...` checks the real
ZIP and revision manifest generation. These host tests do not exercise Android's
package installer, permission UI or certificate extraction; those need a signed
Android device test with two releases, including denied permission and cancelled
installation.
