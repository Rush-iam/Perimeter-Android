package com.queststoredb.perimeter

import android.content.Context

/** Stores the render-buffer scale independently of the display size. */
object ResolutionScale {
    private const val PREFERENCES = "display_settings"
    private const val RENDER_SCALE_PERCENT = "render_scale_percent"
    const val DEFAULT_PERCENT = 100

    val percentages = intArrayOf(50, 75, 100)

    /** Returns the render buffer dimensions used by the landscape game activity. */
    fun renderSize(context: Context, percent: Int): Pair<Int, Int> {
        require(percent in percentages)
        val metrics = context.resources.displayMetrics
        val fullWidth = maxOf(metrics.widthPixels, metrics.heightPixels)
        val fullHeight = minOf(metrics.widthPixels, metrics.heightPixels)
        return Pair(
            (fullWidth * percent / 100f).toInt().coerceAtLeast(1),
            (fullHeight * percent / 100f).toInt().coerceAtLeast(1)
        )
    }

    fun load(context: Context): Int {
        val value = context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
            .getInt(RENDER_SCALE_PERCENT, DEFAULT_PERCENT)
        return value.takeIf { it in percentages } ?: DEFAULT_PERCENT
    }

    fun save(context: Context, percent: Int) {
        require(percent in percentages)
        context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
            .edit()
            .putInt(RENDER_SCALE_PERCENT, percent)
            .apply()
    }
}
