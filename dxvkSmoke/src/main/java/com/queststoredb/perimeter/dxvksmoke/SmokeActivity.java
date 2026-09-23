package com.queststoredb.perimeter.dxvksmoke;

import org.libsdl.app.SDLActivity;

/** Independent package: launching the probe never starts or configures the game. */
public class SmokeActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "c++_shared", "SDL2", "dxvk_d3d9_v2", "dxvk_smoke" };
    }

    @Override
    protected String[] getArguments() {
        final int requested = getIntent().getIntExtra("xr_probe_seconds", 10);
        final int seconds = requested >= 1 && requested <= 60 ? requested : 10;
        final int requestedLossFrame = getIntent().getIntExtra("xr_simulate_session_loss_after_frames", 0);
        final int lossFrame = requestedLossFrame >= 1 && requestedLossFrame <= 2000
                ? requestedLossFrame : 0;
        final boolean failAfterEyeWait = getIntent().getBooleanExtra("xr_fail_after_eye_wait", false);
        final String[] args = new String[1 + (failAfterEyeWait ? 1 : 0) + (lossFrame > 0 ? 1 : 0)];
        args[0] = "--xr-seconds=" + seconds;
        int next = 1;
        if (failAfterEyeWait) args[next++] = "--xr-fail-after-eye-wait";
        if (lossFrame > 0) args[next] = "--xr-simulate-session-loss-after-frames=" + lossFrame;
        return args;
    }
}
