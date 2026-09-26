package org.ourtaiko.fanmade;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import org.json.JSONObject;
import java.io.*;
import java.net.*;
import java.nio.file.*;
import java.util.*;

/** Independent of Fanmade, account credentials and the native networking build option. */
final class AndroidUpdater {
    private static final String RELEASE_ROOT = "https://github.com/OurTaiko/OurTaikoPlayer/releases/";

    static File check(Context context, UpdateFiles.Progress progress) throws Exception {
        if (BuildConfig.DEBUG) return null;
        JSONObject manifest = new JSONObject(UpdateFiles.metadata(new URL(
                RELEASE_ROOT + "latest/download/android-update.json")));
        if (manifest.getInt("schema") != 1 || !context.getPackageName().equals(manifest.getString("package")))
            throw new IOException("Wrong update package/schema");
        PackageManager manager = context.getPackageManager();
        PackageInfo installed = manager.getPackageInfo(context.getPackageName(), PackageManager.GET_SIGNING_CERTIFICATES);
        long version = manifest.getLong("versionCode");
        // Installation, not downloading or dismissing a prompt, advances this value.
        if (version <= installed.getLongVersionCode()) return null;
        String url = manifest.getString("url");
        if (!url.startsWith(RELEASE_ROOT + "download/") || !url.endsWith("/OurTaiko-Android.apk"))
            throw new IOException("Update is not an OurTaiko release asset");
        String hash = manifest.getString("sha256");
        long size = manifest.getLong("size");
        if (!hash.matches("[0-9a-f]{64}") || size <= 0 || size > 4L * 1024 * 1024 * 1024)
            throw new IOException("Invalid update metadata");
        Path apk = new File(context.getCacheDir(), "updates/OurTaiko-Android.apk").toPath();
        if (!Files.isRegularFile(apk) || Files.size(apk) != size || !UpdateFiles.hash(apk).equals(hash)) {
            HttpURLConnection connection = UpdateFiles.connect(new URL(url));
            try (InputStream input = connection.getInputStream()) {
                UpdateFiles.receive(input, apk, hash, size, progress);
            } finally { connection.disconnect(); }
        }
        PackageInfo candidate = manager.getPackageArchiveInfo(apk.toString(), PackageManager.GET_SIGNING_CERTIFICATES);
        if (candidate == null || !context.getPackageName().equals(candidate.packageName)
                || candidate.getLongVersionCode() != version || !sameSigner(installed, candidate)) {
            Files.deleteIfExists(apk);
            throw new IOException("Update package, version or signing certificate mismatch");
        }
        return apk.toFile();
    }

    private static boolean sameSigner(PackageInfo installed, PackageInfo candidate) {
        if (installed.signingInfo == null || candidate.signingInfo == null) return false;
        Set<Signature> oldSigners = new HashSet<>(Arrays.asList(installed.signingInfo.getApkContentsSigners()));
        Set<Signature> newSigners = new HashSet<>(Arrays.asList(candidate.signingInfo.getApkContentsSigners()));
        return !oldSigners.isEmpty() && oldSigners.equals(newSigners);
    }
}
