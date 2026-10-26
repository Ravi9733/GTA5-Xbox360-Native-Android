package com.gta5.nativeandroid;

import android.app.Activity;
import android.os.Bundle;
import android.graphics.Color;
import android.view.Gravity;
import android.widget.TextView;

public final class MainActivity extends Activity {
    static { System.loadLibrary("gta5native"); }
    private native String nativeBuildStatus();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        TextView view = new TextView(this);
        view.setTextColor(Color.WHITE);
        view.setBackgroundColor(Color.rgb(20, 24, 28));
        view.setGravity(Gravity.CENTER);
        view.setPadding(32, 32, 32, 32);
        view.setTextSize(18);
        view.setText("GTA V Xbox 360 — Native Android Dev\n\n" + nativeBuildStatus()
                + "\n\nDevelopment foundation only; not a playable game port.");
        setContentView(view);
    }
}
