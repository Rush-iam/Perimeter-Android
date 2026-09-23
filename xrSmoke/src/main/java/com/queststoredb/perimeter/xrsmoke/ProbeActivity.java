package com.queststoredb.perimeter.xrsmoke;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/** Independent capability probe; it does not start or configure the game. */
public final class ProbeActivity extends Activity {
    private static final String TAG = "XrSmoke";

    static {
        System.loadLibrary("xr_smoke");
    }

    private native String probe();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        String report = probe();
        File output = new File(getFilesDir(), "xr-capabilities.txt");
        try (FileOutputStream stream = new FileOutputStream(output)) {
            stream.write(report.getBytes(StandardCharsets.UTF_8));
            report += "\nSaved to " + output.getAbsolutePath();
        } catch (IOException error) {
            report += "\nCould not save report: " + error;
        }
        Log.i(TAG, report);
        TextView text = new TextView(this);
        text.setText(report);
        text.setTextIsSelectable(true);
        text.setPadding(24, 24, 24, 24);
        setContentView(text);
    }
}
