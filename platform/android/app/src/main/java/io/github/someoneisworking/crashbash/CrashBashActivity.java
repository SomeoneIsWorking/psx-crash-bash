package io.github.someoneisworking.crashbash;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.File;
import java.io.IOException;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.attribute.BasicFileAttributes;

public final class CrashBashActivity extends Activity {
    private static final String PREFERENCES = "crashbash_install";
    private static final String INSTALLED = "installed";

    private static final boolean NATIVE_RUNTIME_AVAILABLE;

    static {
        boolean loaded = false;
        try {
            System.loadLibrary("main");
            loaded = true;
        } catch (UnsatisfiedLinkError ignored) {
            // Debug shell builds deliberately exercise setup without pretending the game runtime exists.
        }
        NATIVE_RUNTIME_AVAILABLE = loaded;
    }

    private TextView status;
    private Button play;
    private CrashBashMediaImport mediaImport;

    private static native boolean nativeValidateAndInstall(
        int fileDescriptor,
        String displayName,
        long byteCount,
        String installDirectory
    );

    private static native void nativeStartGame(String installDirectory);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mediaImport = new CrashBashMediaImport(this, CrashBashActivity::nativeValidateAndInstall);
        mediaImport.cleanStaleImports();
        showSetupScreen();
    }

    private void showSetupScreen() {
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setGravity(Gravity.CENTER_HORIZONTAL);
        content.setPadding(dp(32), dp(32), dp(32), dp(32));
        content.setBackgroundColor(Color.rgb(19, 27, 58));

        TextView title = text(getString(R.string.setup_title), 28);
        title.setTextColor(Color.WHITE);
        content.addView(title, matchWidth());

        TextView explanation = text(getString(R.string.setup_explanation), 17);
        explanation.setTextColor(Color.rgb(214, 222, 242));
        content.addView(explanation, spacedWidth());

        status = text("", 15);
        status.setTextColor(Color.rgb(255, 211, 79));
        content.addView(status, spacedWidth());

        Button choose = new Button(this);
        choose.setText(R.string.choose_game_file);
        choose.setOnClickListener(view -> chooseGameInput());
        content.addView(choose, spacedWidth());

        play = new Button(this);
        play.setText(R.string.play);
        play.setOnClickListener(view -> startInstalledGame());
        content.addView(play, spacedWidth());

        Button forget = new Button(this);
        forget.setText(R.string.forget_selection);
        forget.setOnClickListener(view -> forgetSelection());
        content.addView(forget, spacedWidth());

        setContentView(content);
        refreshState();
    }

    private void chooseGameInput() {
        if (!NATIVE_RUNTIME_AVAILABLE) {
            refreshState();
            return;
        }
        mediaImport.choose(new CrashBashMediaImport.Callback() {
            @Override
            public void onInstalled(File directory) {
                preferences().edit().putBoolean(INSTALLED, true).apply();
                refreshState();
            }

            @Override
            public void onRejected(CrashBashMediaImport.Rejection rejection) {
                status.setText(selectionStatus(rejection));
            }

            @Override
            public void onCancelled() {
                refreshState();
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (mediaImport.handleActivityResult(requestCode, resultCode, data)) {
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    private int selectionStatus(CrashBashMediaImport.Rejection rejection) {
        return switch (rejection) {
            case WRONG_TYPE -> R.string.selection_wrong_type;
            case IDENTITY_MISMATCH -> R.string.selection_identity_mismatch;
            case UNREADABLE -> R.string.selection_unreadable;
        };
    }

    private void refreshState() {
        boolean installed = preferences().getBoolean(INSTALLED, false);
        play.setEnabled(installed && NATIVE_RUNTIME_AVAILABLE);
        if (installed) {
            status.setText(R.string.selection_ready);
        } else if (!NATIVE_RUNTIME_AVAILABLE) {
            status.setText(R.string.native_runtime_missing);
        } else {
            status.setText(R.string.selection_needed);
        }
    }

    private void startInstalledGame() {
        if (!preferences().getBoolean(INSTALLED, false) || !NATIVE_RUNTIME_AVAILABLE) {
            refreshState();
            return;
        }
        nativeStartGame(CrashBashMediaImport.installedDirectory(getFilesDir()).getAbsolutePath());
    }

    private void forgetSelection() {
        if (!deleteInstallTree(CrashBashMediaImport.installedDirectory(getFilesDir()))) {
            status.setText(R.string.reset_failed);
            return;
        }
        preferences().edit().clear().apply();
        refreshState();
    }

    private boolean deleteInstallTree(File installDirectory) {
        Path appFiles = getFilesDir().toPath().toAbsolutePath().normalize();
        Path target = installDirectory.toPath().toAbsolutePath().normalize();
        if (!appFiles.equals(target.getParent())) {
            return false;
        }
        if (!Files.exists(target)) {
            return true;
        }
        try {
            Files.walkFileTree(target, new SimpleFileVisitor<>() {
                @Override
                public FileVisitResult visitFile(Path file, BasicFileAttributes attributes)
                    throws IOException {
                    Files.delete(file);
                    return FileVisitResult.CONTINUE;
                }

                @Override
                public FileVisitResult postVisitDirectory(Path directory, IOException error)
                    throws IOException {
                    if (error != null) {
                        throw error;
                    }
                    Files.delete(directory);
                    return FileVisitResult.CONTINUE;
                }
            });
            return true;
        } catch (IOException | SecurityException error) {
            return false;
        }
    }

    private SharedPreferences preferences() {
        return getSharedPreferences(PREFERENCES, MODE_PRIVATE);
    }

    private TextView text(String value, int sizeSp) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(sizeSp);
        return view;
    }

    private LinearLayout.LayoutParams matchWidth() {
        return new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        );
    }

    private LinearLayout.LayoutParams spacedWidth() {
        LinearLayout.LayoutParams params = matchWidth();
        params.topMargin = dp(18);
        return params;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    @Override
    protected void onDestroy() {
        if (isFinishing()) {
            mediaImport.cancel();
        }
        super.onDestroy();
    }
}
