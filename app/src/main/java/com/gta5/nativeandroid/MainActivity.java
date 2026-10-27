package com.gta5.nativeandroid;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.view.Gravity;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayDeque;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

public final class MainActivity extends Activity implements SurfaceHolder.Callback {
    private static final int PICK_GAME_FOLDER = 7401;
    private static final String PREFS = "native_port_settings";
    private static final String GAME_TREE_URI = "game_tree_uri";

    static { System.loadLibrary("gta5native"); }

    private TextView statusView;
    private TextView gameFilesView;
    private volatile boolean renderRequested;
    private Thread renderThread;
    private SurfaceHolder surfaceHolder;

    private native String nativeBuildStatus();
    private native String nativeInitVulkan(Surface surface);
    private native boolean nativeRenderFrame();
    private native void nativeShutdownVulkan();
    private native String nativeInspectDocument(android.content.ContentResolver resolver, Uri documentUri);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setBackgroundColor(Color.rgb(20, 24, 28));
        layout.setPadding(20, 18, 20, 18);

        TextView title = new TextView(this);
        title.setTextColor(Color.WHITE);
        title.setTextSize(20);
        title.setGravity(Gravity.CENTER);
        title.setText("GTA V Xbox 360 — Native Android");
        layout.addView(title);

        statusView = new TextView(this);
        statusView.setTextColor(Color.rgb(220, 230, 235));
        statusView.setTextSize(14);
        statusView.setPadding(0, 12, 0, 12);
        statusView.setText(nativeBuildStatus()
                + "\n\nSelect your own extracted Xbox 360 game folder to check required files.");
        layout.addView(statusView);

        Button chooseFolder = new Button(this);
        chooseFolder.setText("Select GTA V game folder");
        chooseFolder.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                    | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                    | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
            startActivityForResult(intent, PICK_GAME_FOLDER);
        });
        layout.addView(chooseFolder);

        gameFilesView = new TextView(this);
        gameFilesView.setTextColor(Color.rgb(185, 205, 190));
        gameFilesView.setTextSize(13);
        gameFilesView.setPadding(0, 8, 0, 12);
        layout.addView(gameFilesView);
        restoreSelectedFolder();

        SurfaceView surfaceView = new SurfaceView(this);
        surfaceView.getHolder().addCallback(this);
        layout.addView(surfaceView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1.0f));
        setContentView(layout);
    }

    private void restoreSelectedFolder() {
        String saved = getSharedPreferences(PREFS, MODE_PRIVATE).getString(GAME_TREE_URI, null);
        if (saved == null) {
            gameFilesView.setText("Game folder: not selected");
            return;
        }
        Uri uri = Uri.parse(saved);
        try {
            int flags = Intent.FLAG_GRANT_READ_URI_PERMISSION;
            getContentResolver().takePersistableUriPermission(uri, flags);
            gameFilesView.setText("Saved game folder permission found. Rechecking required files…");
            new Thread(() -> checkGameFolder(uri), "Game-folder-check").start();
        } catch (SecurityException ex) {
            gameFilesView.setText("Saved folder permission expired. Select the folder again.");
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_GAME_FOLDER || resultCode != RESULT_OK || data == null) return;
        Uri uri = data.getData();
        if (uri == null) return;
        int flags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        try {
            getContentResolver().takePersistableUriPermission(uri,
                    flags & Intent.FLAG_GRANT_READ_URI_PERMISSION);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putString(GAME_TREE_URI, uri.toString()).apply();
            gameFilesView.setText("Checking selected folder…");
            new Thread(() -> checkGameFolder(uri), "Game-folder-check").start();
        } catch (SecurityException ex) {
            gameFilesView.setText("Android could not keep access to this folder. Please select it again.");
        }
    }

    private void checkGameFolder(Uri treeUri) {
        Uri defaultXexUri = null;
        Uri xbox360RpfUri = null;
        int visited = 0;
        ArrayDeque<String> pending = new ArrayDeque<>();
        Set<String> seen = new HashSet<>();
        try {
            pending.add(DocumentsContract.getTreeDocumentId(treeUri));
            while (!pending.isEmpty() && visited < 12000 && (defaultXexUri == null || xbox360RpfUri == null)) {
                String parentId = pending.removeFirst();
                if (!seen.add(parentId)) continue;
                Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentId);
                try (Cursor cursor = getContentResolver().query(childrenUri,
                        new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                                DocumentsContract.Document.COLUMN_MIME_TYPE},
                        null, null, null)) {
                    if (cursor == null) continue;
                    while (cursor.moveToNext() && visited < 12000) {
                        visited++;
                        String id = cursor.getString(0);
                        String name = cursor.getString(1);
                        String mime = cursor.getString(2);
                        if (name != null) {
                            String normalized = name.toLowerCase(Locale.ROOT);
                            if (normalized.equals("default.xex")) defaultXexUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, id);
                            if (normalized.equals("xbox360.rpf")) xbox360RpfUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, id);
                        }
                        if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)
                                && !seen.contains(id) && pending.size() < 12000) {
                            pending.addLast(id);
                        }
                        if (defaultXexUri != null && xbox360RpfUri != null) break;
                    }
                }
            }
            final boolean hasXex = defaultXexUri != null;
            final boolean hasRpf = xbox360RpfUri != null;
            final String xexCheck = inspectMagic(defaultXexUri, new byte[]{'X','E','X','2'});
            final String rpfCheck = inspectMagic(xbox360RpfUri, new byte[]{'R','P','F','7'});
            final String xexNative = hasXex ? nativeInspectDocument(getContentResolver(), defaultXexUri) : "not found";
            final String rpfNative = hasRpf ? nativeInspectDocument(getContentResolver(), xbox360RpfUri) : "not found";
            final int scanned = visited;
            runOnUiThread(() -> {
                String report = "Folder scan: " + scanned + " entries checked\n"
                        + "default.xex: " + (hasXex ? xexCheck : "not found") + "\n"
                        + "xbox360.rpf: " + (hasRpf ? rpfCheck : "not found") + "\\n"
                        + "Native file bridge — XEX: " + xexNative + "\\n"
                        + "Native file bridge — RPF: " + rpfNative;
                if (hasXex && hasRpf) {
                    report += "\nHeader checks completed. This app does not yet load or run the game.";
                } else if (scanned >= 12000) {
                    report += "\nScan limit reached; select a folder closer to the extracted game root.";
                } else {
                    report += "\nChoose the extracted Xbox 360 game root. This is only a file check.";
                }
                gameFilesView.setText(report);
            });
        } catch (Exception ex) {
            runOnUiThread(() -> gameFilesView.setText(
                    "Could not read this folder. Re-select it and allow access.\n" + ex.getClass().getSimpleName()));
        }
    }

    // Validate through the same seekable SAF abstraction intended for the future filesystem bridge.
    // A SAF Uri is not a POSIX path, and this header check still does not load or mount the game.
    private String inspectMagic(Uri documentUri, byte[] expected) {
        if (documentUri == null) return "not found";
        try (SafRandomAccessFile file = SafRandomAccessFile.open(getContentResolver(), documentUri)) {
            if (file.length() < expected.length) return "found, too short to validate";
            byte[] header = new byte[expected.length];
            int count = file.readAt(0, header, 0, header.length);
            if (count != expected.length) return "found, too short to validate";
            for (int i = 0; i < expected.length; i++) {
                if (header[i] != expected[i]) return "readable, unexpected header";
            }
            return "readable, header OK";
        } catch (Exception ex) {
            return "found, read failed (" + ex.getClass().getSimpleName() + ")";
        }
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) { surfaceHolder = holder; }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        surfaceHolder = holder;
        initializeSurface(holder);
    }

    private void initializeSurface(SurfaceHolder holder) {
        if (holder == null || !holder.getSurface().isValid()) return;
        stopRenderLoop();
        nativeShutdownVulkan();
        String status = nativeInitVulkan(holder.getSurface());
        statusView.setText(status);
        if (status.startsWith("Vulkan swapchain ready.")) startRenderLoop();
    }

    private void startRenderLoop() {
        renderRequested = true;
        renderThread = new Thread(() -> {
            while (renderRequested) {
                if (!nativeRenderFrame()) {
                    try { Thread.sleep(16L); }
                    catch (InterruptedException ignored) {
                        Thread.currentThread().interrupt();
                        break;
                    }
                }
            }
        }, "GTA5-Vulkan-Render");
        renderThread.start();
    }

    private void stopRenderLoop() {
        renderRequested = false;
        Thread thread = renderThread;
        renderThread = null;
        if (thread != null && thread != Thread.currentThread()) {
            thread.interrupt();
            try { thread.join(1500L); }
            catch (InterruptedException ignored) { Thread.currentThread().interrupt(); }
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        surfaceHolder = null;
        stopRenderLoop();
        nativeShutdownVulkan();
        if (statusView != null) statusView.setText("Android surface closed. Vulkan resources released.");
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (surfaceHolder != null) initializeSurface(surfaceHolder);
    }

    @Override
    protected void onPause() {
        stopRenderLoop();
        nativeShutdownVulkan();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        stopRenderLoop();
        nativeShutdownVulkan();
        super.onDestroy();
    }
}
