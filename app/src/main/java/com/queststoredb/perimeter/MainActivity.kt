package com.queststoredb.perimeter

import org.libsdl.app.SDLActivity

class MainActivity : SDLActivity() {
    override fun getMainSharedObject(): String {
        return "libperimeter.so"
    }

    override fun getLibraries(): Array<String> {
        return arrayOf(
            "c++_shared",
            "ffmpeg",
            "SDL2",
            "SDL2_image",
            "SDL2_mixer",
            "SDL2_net",
            "perimeter"
        )
    }
}
