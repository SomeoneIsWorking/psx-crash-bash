package io.github.someoneisworking.crashbash;

import android.app.Activity;
import android.content.Intent;
import android.os.ParcelFileDescriptor;
import io.github.someoneisworking.lucent.LucentDocumentImport;
import java.io.File;
import java.io.IOException;
import java.util.Locale;

/**
 * Crash Bash's title-owned media-import policy.
 *
 * <p>Lucent owns Storage Access Framework access, bounded private staging, and atomic promotion.
 * This class owns the title's accepted media names and the rule that native validation must complete
 * an installation inside that staging directory before it can replace the current game.</p>
 */
final class CrashBashMediaImport {
    static final int PICK_GAME_INPUT = 1001;
    private static final String INSTALL_DIRECTORY = "game";
    private static final String[] ACCEPTED_SUFFIXES = {".chd", ".iso", ".zip"};
    // A PSX CD image is below 1 GB; import one document, leaving archive-member limits to the
    // title-native ZIP validator rather than trusting a provider's metadata.
    private static final LucentDocumentImport.Limits LIMITS =
            new LucentDocumentImport.Limits(1, 1_000_000_000L, 64 * 1024);

    enum Rejection {
        UNREADABLE,
        WRONG_TYPE,
        IDENTITY_MISMATCH,
    }

    interface Callback {
        void onInstalled(File directory);

        void onRejected(Rejection rejection);

        void onCancelled();
    }

    /**
     * Validates the selected input and builds a complete install under {@code stagingDirectory}.
     * Returning true authorizes Lucent to atomically promote that directory to the live selection.
     */
    interface StagedInstallValidator {
        boolean validateAndInstall(int descriptor, String displayName, long byteCount, String stagingDirectory);
    }

    private final LucentDocumentImport documents;
    private final StagedInstallValidator validator;

    CrashBashMediaImport(Activity activity, StagedInstallValidator validator) {
        if (validator == null) {
            throw new IllegalArgumentException("staged install validator is required");
        }
        documents = new LucentDocumentImport(activity, LIMITS);
        this.validator = validator;
    }

    static File installedDirectory(File appFilesDirectory) {
        return new File(appFilesDirectory, INSTALL_DIRECTORY);
    }

    static boolean acceptsDocumentName(String name) {
        if (name == null) {
            return false;
        }
        String lower = name.toLowerCase(Locale.ROOT);
        for (String suffix : ACCEPTED_SUFFIXES) {
            if (lower.endsWith(suffix)) {
                return true;
            }
        }
        return false;
    }

    void choose(Callback callback) {
        if (callback == null) {
            throw new IllegalArgumentException("media import callback is required");
        }
        documents.pickDocument(PICK_GAME_INPUT, new LucentDocumentImport.Callback() {
            @Override
            public void onImported(LucentDocumentImport.Result result) {
                validateAndPromote(result, callback);
            }

            @Override
            public void onCancelled() {
                callback.onCancelled();
            }

            @Override
            public void onFailed(String message) {
                callback.onRejected(Rejection.UNREADABLE);
            }
        });
    }

    boolean handleActivityResult(int requestCode, int resultCode, Intent data) {
        return documents.handleActivityResult(requestCode, resultCode, data);
    }

    void cleanStaleImports() {
        documents.cleanStaleImports();
    }

    void cancel() {
        documents.cancel();
    }

    private void validateAndPromote(LucentDocumentImport.Result result, Callback callback) {
        if (!acceptsDocumentName(result.documentName)) {
            reject(result, callback, Rejection.WRONG_TYPE);
            return;
        }
        File stagedInput = new File(result.stagingDirectory, result.documentName);
        long byteCount = stagedInput.length();
        if (!stagedInput.isFile() || byteCount <= 0L) {
            reject(result, callback, Rejection.UNREADABLE);
            return;
        }

        boolean accepted;
        try (ParcelFileDescriptor descriptor =
                     ParcelFileDescriptor.open(stagedInput, ParcelFileDescriptor.MODE_READ_ONLY)) {
            accepted = validator.validateAndInstall(
                    descriptor.getFd(), result.documentName, byteCount, result.stagingDirectory.getAbsolutePath());
        } catch (IOException | SecurityException error) {
            reject(result, callback, Rejection.UNREADABLE);
            return;
        }
        if (!accepted) {
            reject(result, callback, Rejection.IDENTITY_MISMATCH);
            return;
        }

        try {
            callback.onInstalled(documents.promoteValidated(result, INSTALL_DIRECTORY));
        } catch (IOException | SecurityException error) {
            callback.onRejected(Rejection.UNREADABLE);
        }
    }

    private void reject(LucentDocumentImport.Result result, Callback callback, Rejection rejection) {
        try {
            documents.discard(result);
            callback.onRejected(rejection);
        } catch (IOException | SecurityException error) {
            callback.onRejected(Rejection.UNREADABLE);
        }
    }
}
