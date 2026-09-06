package com.queststoredb.perimeter

import android.content.pm.ActivityInfo
import android.os.Build
import android.os.Bundle
import android.view.KeyEvent
import android.window.OnBackInvokedDispatcher
import org.libsdl.app.SDL
import org.libsdl.app.SDLActivity

class MainActivity : SDLActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            onBackInvokedDispatcher.registerOnBackInvokedCallback(
                OnBackInvokedDispatcher.PRIORITY_DEFAULT
            ) { sendEscapeClick() }
        }
    }

    @Suppress("DEPRECATION", "OVERRIDE_DEPRECATION")
    override fun onBackPressed() {
        sendEscapeClick()
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (event.keyCode == KeyEvent.KEYCODE_BACK) {
            if (event.action == KeyEvent.ACTION_UP && !event.isCanceled) {
                sendEscapeClick()
            }
            return true
        }
        return super.dispatchKeyEvent(event)
    }

    private fun sendEscapeClick() {
        if (mBrokenLibraries) return
        onNativeKeyDown(KeyEvent.KEYCODE_ESCAPE)
        onNativeKeyUp(KeyEvent.KEYCODE_ESCAPE)
    }

    override fun setOrientationBis(w: Int, h: Int, resizable: Boolean, hint: String?) {
        // SDL's default can replace the manifest orientation with FULL_USER, honoring rotation lock.
        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
    }

    override fun loadLibraries() {
        // The engine's global error handler requests SDL_GetPrefPath during library loading.
        SDL.setContext(this)
        super.loadLibraries()
    }

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

    override fun getArguments(): Array<String> {
        val storage = GameContentStorage(this)
        val path = checkNotNull(storage.localFilesystemPath()) {
            "Grant all-files access and select the game folder before starting the engine"
        }
        return GameLaunchOptions(this).arguments(path)
    }
}
