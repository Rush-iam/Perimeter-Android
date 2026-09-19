package com.queststoredb.perimeter

import android.content.Context
import kotlin.math.roundToInt

/** Stores the render-buffer scale independently of the display size. */
object ResolutionScale {
    private const val PREFERENCES = "display_settings"
    private const val RENDER_SCALE_PERCENT = "render_scale_percent"
    private const val LAST_GAME_WIDTH = "last_game_width"
    private const val LAST_GAME_HEIGHT = "last_game_height"
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

    /** Returns the last landscape surface size reported by the game Activity. */
    private fun gameSize(context: Context): Pair<Int, Int> {
        val preferences = context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
        val rememberedWidth = preferences.getInt(LAST_GAME_WIDTH, 0)
        val rememberedHeight = preferences.getInt(LAST_GAME_HEIGHT, 0)
        if (rememberedWidth > 0 && rememberedHeight > 0) {
            return Pair(rememberedWidth, rememberedHeight)
        }

        val metrics = context.resources.displayMetrics
        return Pair(maxOf(metrics.widthPixels, metrics.heightPixels),
            minOf(metrics.widthPixels, metrics.heightPixels))
    }

    /** Saves the full-resolution landscape size represented by a render surface callback. */
    fun rememberGameSurfaceSize(context: Context, width: Int, height: Int, percent: Int) {
        require(percent in percentages)
        if (width <= 0 || height <= 0) return
        val landscapeWidth = maxOf(width, height)
        val landscapeHeight = minOf(width, height)
        context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
            .edit()
            .putInt(LAST_GAME_WIDTH, (landscapeWidth * 100f / percent).roundToInt())
            .putInt(LAST_GAME_HEIGHT, (landscapeHeight * 100f / percent).roundToInt())
            .apply()
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
