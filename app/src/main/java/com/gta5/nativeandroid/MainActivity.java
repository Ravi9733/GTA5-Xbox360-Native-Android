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
    private native String nativeBuildStatus();
    private native String nativeInitVulkan(Surface surface);
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
                + "\n\nVulkan check will run when the Android surface is ready."
                + "\nThis is not yet a playable game port.");
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
        statusView.setText(nativeInitVulkan(holder.getSurface())
                + "\n\nVulkan surface probe only — game rendering is not integrated yet.");
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // The game renderer will consume surface dimensions once it is integrated.
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        nativeShutdownVulkan();
        if (statusView != null) {
            statusView.setText("Android surface closed. Vulkan resources released.");
        }
    }

    @Override
    protected void onDestroy() {
        nativeShutdownVulkan();
        super.onDestroy();
    }
}
