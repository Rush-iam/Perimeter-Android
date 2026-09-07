package com.queststoredb.perimeter.dxvksmoke;

import org.libsdl.app.SDLActivity;

/** Independent package: launching the probe never starts or configures the game. */
public class SmokeActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "c++_shared", "SDL2", "dxvk_d3d9", "dxvk_smoke" };
    }
}
