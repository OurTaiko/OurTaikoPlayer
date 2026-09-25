package org.ourtaiko.fanmade;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;
import android.view.Gravity;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Button;
import android.view.View;
import androidx.core.content.FileProvider;
import java.io.File;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Paths;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

/** Permissions and file preparation complete before SDL starts its native thread. */
public class OurTaikoLauncherActivity extends Activity {
    private static final int STORAGE_REQUEST = 1;
    // Serialize setup across Activity recreation; cancel the old work on destroy.
    private static final ExecutorService WORKER = Executors.newSingleThreadExecutor();
    private Future<?> preparation;
    private boolean waitingForPermission;
    private boolean ready;
    private boolean resumed;
    private boolean preparing;
    private TextView status;
    private Button skipUpdate;
    private File pendingApk;
    private boolean updatePromptShown;
    private static final int INSTALL_REQUEST = 2;


    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setPadding(32, 32, 32, 32);
        layout.addView(new ProgressBar(this));
        TextView message = new TextView(this);
        status = message;
        message.setText(R.string.preparing_game_data);
        message.setGravity(Gravity.CENTER);
        message.setPadding(0, 24, 0, 0);
        layout.addView(message);
        skipUpdate = new Button(this);
        skipUpdate.setText(R.string.skip_update);
        skipUpdate.setVisibility(View.GONE);
        skipUpdate.setOnClickListener(v -> {
            if (preparing) return;
            if (preparation != null) preparation.cancel(true);
            pendingApk = null;
            ready = true;
            launchIfReady();
        });
        layout.addView(skipUpdate);
        setContentView(layout);
        waitingForPermission = state != null && state.getBoolean("waitingForPermission");
        if (hasStoragePermission()) prepare();
        else if (!waitingForPermission) requestStoragePermission();
    }

    private boolean hasStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return Environment.isExternalStorageManager();
        }
        return checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED
                && checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED;
    }

    private void requestStoragePermission() {
        waitingForPermission = true;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            try {
                startActivityForResult(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName())), STORAGE_REQUEST);
            } catch (ActivityNotFoundException missingAppPage) {
                try {
                    startActivityForResult(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION), STORAGE_REQUEST);
                } catch (ActivityNotFoundException missingSettings) {
                    waitingForPermission = false;
                    showFailure(getString(R.string.storage_permission_required), this::requestStoragePermission);
                }
            }
        } else {
            requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE}, STORAGE_REQUEST);
        }
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request == STORAGE_REQUEST) permissionReturned();
        if (request == INSTALL_REQUEST) {
            if (getPackageManager().canRequestPackageInstalls()) installUpdate();
            else { updatePromptShown = false; showUpdateIfReady(); }
        }
    }

    @Override
    public void onRequestPermissionsResult(int request, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(request, permissions, results);
        if (request == STORAGE_REQUEST) permissionReturned();
        if (request == INSTALL_REQUEST) {
            if (getPackageManager().canRequestPackageInstalls()) installUpdate();
            else { updatePromptShown = false; showUpdateIfReady(); }
        }
    }

    private void permissionReturned() {
        waitingForPermission = false;
        if (hasStoragePermission()) prepare();
        else showFailure(getString(R.string.storage_permission_required), this::requestStoragePermission);
    }

    private void prepare() {
        if (preparation != null && !preparation.isDone()) return;
        preparing = true;
        AssetManager assets = getApplicationContext().getAssets();
        preparation = WORKER.submit(() -> {
            try {
                GameDataInstaller.Assets source = new GameDataInstaller.Assets() {
                    public InputStream open(String path) throws IOException {
                        return assets.open(path, AssetManager.ACCESS_STREAMING);
                    }
                };
                java.nio.file.Path root = Paths.get(Environment.getExternalStorageDirectory().getAbsolutePath(), "OurTaiko");
                boolean freshSkins = !java.nio.file.Files.exists(root.resolve("Skins"));
                GameDataInstaller.install(source, root);
                BundledSkinUpdater.install(source, root, freshSkins);
                runOnUiThread(() -> {
                    preparing = false;
                    status.setText(R.string.checking_update);
                    skipUpdate.setVisibility(View.VISIBLE);
                });
                File apk = null;
                try {
                    final long[] lastProgress = {0};
                    apk = AndroidUpdater.check(getApplicationContext(), (received, total) -> {
                        long now = android.os.SystemClock.elapsedRealtime();
                        if (now - lastProgress[0] < 200 && received < total) return;
                        lastProgress[0] = now;
                        runOnUiThread(() -> status.setText(getString(R.string.downloading_update, (int)(received * 100 / total))));
                    });
                } catch (Exception error) {
                    Log.w("OurTaiko", "Update unavailable; continuing offline", error);
                }
                UpdateFiles.interrupted();
                final File downloaded = apk;
                runOnUiThread(() -> {
                    if (isFinishing() || isDestroyed()) return;
                    if (downloaded == null) { ready = true; launchIfReady(); }
                    else { pendingApk = downloaded; showUpdateIfReady(); }
                });
            } catch (IOException | RuntimeException error) {
                Log.e("OurTaiko", "Cannot prepare game data", error);
                runOnUiThread(() -> showFailure(getString(R.string.game_data_failed), this::prepare));
            }
        });
    }

    private void showUpdateIfReady() {
        if (pendingApk == null || !resumed || updatePromptShown || isFinishing() || isDestroyed()) return;
        updatePromptShown = true;
        new AlertDialog.Builder(this).setTitle(R.string.app_name).setMessage(R.string.update_ready)
                .setPositiveButton(R.string.install_update, (dialog, which) -> installUpdate())
                .setNegativeButton(R.string.skip_update, (dialog, which) -> {
                    pendingApk = null; ready = true; launchIfReady();
                }).setCancelable(false).show();
    }

    private void installUpdate() {
        if (pendingApk == null || isFinishing() || isDestroyed()) return;
        try {
            if (!getPackageManager().canRequestPackageInstalls()) {
                startActivityForResult(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                        Uri.parse("package:" + getPackageName())), INSTALL_REQUEST);
                return;
            }
            Uri uri = FileProvider.getUriForFile(this, getPackageName() + ".fileprovider", pendingApk);
            Intent intent = new Intent(Intent.ACTION_VIEW).setDataAndType(uri, "application/vnd.android.package-archive")
                    .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            startActivity(intent);
            // No installed marker: cancelling installation allows a retry on next launch.
            finish();
        } catch (RuntimeException error) {
            Log.e("OurTaiko", "Cannot open package installer", error);
            pendingApk = null; ready = true; launchIfReady();
        }
    }

    private void launchIfReady() {
        if (!ready || !resumed || isFinishing() || isDestroyed()) return;
        ready = false;
        startActivity(new Intent(this, OurTaikoActivity.class));
        finish();
    }

    private void showFailure(String message, Runnable retry) {
        if (isFinishing() || isDestroyed()) return;
        new AlertDialog.Builder(this).setTitle(R.string.app_name).setMessage(message)
                .setPositiveButton(R.string.retry, (dialog, which) -> retry.run())
                .setNegativeButton(R.string.close_app, (dialog, which) -> finish())
                .setCancelable(false).show();
    }

    @Override protected void onResume() {
        super.onResume();
        resumed = true;
        showUpdateIfReady();
        launchIfReady();
    }

    @Override protected void onPause() {
        resumed = false;
        super.onPause();
    }

    @Override protected void onSaveInstanceState(Bundle state) {
        state.putBoolean("waitingForPermission", waitingForPermission);
        super.onSaveInstanceState(state);
    }

    @Override protected void onDestroy() {
        if (preparation != null) preparation.cancel(true);
        super.onDestroy();
    }
}
