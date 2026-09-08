package com.queststoredb.perimeter

import android.content.Context
import android.view.SurfaceHolder
import org.libsdl.app.SDLSurface

/**
 * Keeps the Android view fullscreen while SDL renders into a smaller Surface buffer.
 * Android's compositor upscales that buffer to the view's bounds.
 */
class ScaledSDLSurface(context: Context) : SDLSurface(context) {
    private var renderWidth = 1
    private var renderHeight = 1

    init {
        val percent = ResolutionScale.load(context)
        val size = ResolutionScale.renderSize(context, percent)
        renderWidth = size.first
        renderHeight = size.second
        holder.setFixedSize(renderWidth, renderHeight)
        // A fixed Surface buffer is not automatically stretched by all Android compositor
        // implementations. Scale the surface layer to the full View bounds explicitly.
        val inverseScale = 100f / percent
        pivotX = 0f
        pivotY = 0f
        scaleX = inverseScale
        scaleY = inverseScale
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        renderWidth = width
        renderHeight = height
        super.surfaceChanged(holder, format, width, height)
    }

}
