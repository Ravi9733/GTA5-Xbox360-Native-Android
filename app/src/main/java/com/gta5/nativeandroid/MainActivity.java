package com.gta5.nativeandroid;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.LinearLayout;
import android.widget.TextView;

public final class MainActivity extends Activity implements SurfaceHolder.Callback {
    static {
        System.loadLibrary("gta5native");
    }

    private TextView statusView;
    private volatile boolean renderRequested;
    private Thread renderThread;

    private native String nativeBuildStatus();
    private native String nativeInitVulkan(Surface surface);
    private native boolean nativeRenderFrame();
    private native void nativeShutdownVulkan();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setBackgroundColor(Color.rgb(20, 24, 28));
        layout.setPadding(24, 24, 24, 24);

        TextView title = new TextView(this);
        title.setTextColor(Color.WHITE);
        title.setTextSize(20);
        title.setGravity(Gravity.CENTER);
        title.setText("GTA V Xbox 360 — Android ARM64 Port Lab");
        layout.addView(title, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));

        statusView = new TextView(this);
        statusView.setTextColor(Color.rgb(220, 230, 235));
        statusView.setTextSize(15);
        statusView.setPadding(0, 24, 0, 24);
        statusView.setText(nativeBuildStatus()
                + "\n\nWaiting for Android display surface.\n"
                + "This foundation is not yet a playable GTA V port.");
        layout.addView(statusView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));

        SurfaceView surfaceView = new SurfaceView(this);
        surfaceView.getHolder().addCallback(this);
        layout.addView(surfaceView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1.0f));
        setContentView(layout);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        // Initialize after surfaceChanged provides the actual drawable dimensions.
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        stopRenderLoop();
        nativeShutdownVulkan();
        String status = nativeInitVulkan(holder.getSurface());
        statusView.setText(status);
        if (status.startsWith("Vulkan swapchain ready.")) {
            startRenderLoop();
        }
    }

    private void startRenderLoop() {
        renderRequested = true;
        renderThread = new Thread(() -> {
            while (renderRequested) {
                boolean rendered = nativeRenderFrame();
                if (!rendered) {
                    // Avoid a hot loop if Android invalidates the surface or the GPU reports an error.
                    try {
                        Thread.sleep(16L);
                    } catch (InterruptedException ignored) {
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
            try {
                thread.join(1500L);
            } catch (InterruptedException ignored) {
                Thread.currentThread().interrupt();
            }
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        stopRenderLoop();
        nativeShutdownVulkan();
        if (statusView != null) {
            statusView.setText("Android surface closed. Vulkan resources released.");
        }
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
