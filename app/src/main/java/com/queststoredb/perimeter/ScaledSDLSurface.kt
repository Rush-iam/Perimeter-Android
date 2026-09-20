package com.queststoredb.perimeter

import android.content.Context
import org.libsdl.app.SDLSurface

/**
 * Keeps the Android view fullscreen while SDL renders into a smaller Surface buffer.
 * Android's compositor upscales that buffer to the view's bounds.
 */
class ScaledSDLSurface(context: Context) : SDLSurface(context) {
    private val resolutionScalePercent = ResolutionScale.load(context)

    init {
        val size = ResolutionScale.renderSize(context, resolutionScalePercent)
        holder.setFixedSize(size.first, size.second)
        // A fixed Surface buffer is not automatically stretched by all Android compositor
        // implementations. Scale the surface layer to the full View bounds explicitly.
        val inverseScale = 100f / resolutionScalePercent
        pivotX = 0f
        pivotY = 0f
        scaleX = inverseScale
        scaleY = inverseScale
    }
}
