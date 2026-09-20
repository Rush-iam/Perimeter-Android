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
        val (fullWidth, fullHeight) = gameSize(context)
        return renderSize(fullWidth, fullHeight, percent)
    }

    private fun renderSize(fullWidth: Int, fullHeight: Int, percent: Int): Pair<Int, Int> =
        Pair(
            (fullWidth * percent / 100f).toInt().coerceAtLeast(1),
            (fullHeight * percent / 100f).toInt().coerceAtLeast(1)
        )

    /** Returns the current display dimensions in landscape order. */
    private fun gameSize(context: Context): Pair<Int, Int> {
        val metrics = context.resources.displayMetrics
        return Pair(maxOf(metrics.widthPixels, metrics.heightPixels),
            minOf(metrics.widthPixels, metrics.heightPixels))
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
