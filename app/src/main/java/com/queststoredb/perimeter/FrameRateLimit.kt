package com.queststoredb.perimeter

import android.content.Context
import android.hardware.display.DisplayManager
import android.view.Display

/** Stores whether rendering is capped to every second display refresh. */
object FrameRateLimit {
    private const val PREFERENCES = "display_settings"
    private const val LIMIT_TO_HALF_REFRESH = "limit_frame_rate_half_refresh"

    fun displayRefreshRate(context: Context): Float {
        val displayManager = context.getSystemService(DisplayManager::class.java)
        return displayManager.getDisplay(Display.DEFAULT_DISPLAY)?.refreshRate
            ?.takeIf { it.isFinite() && it > 0f }
            ?: 60f
    }

    fun displayedFramesPerSecond(context: Context): Int =
        kotlin.math.round(displayRefreshRate(context) / 2f).toInt()

    fun load(context: Context): Boolean =
        context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
            .getBoolean(LIMIT_TO_HALF_REFRESH, false)

    fun save(context: Context, enabled: Boolean) {
        context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
            .edit()
            .putBoolean(LIMIT_TO_HALF_REFRESH, enabled)
            .apply()
    }
}
