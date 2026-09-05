#include "StdAfx.h"
#include "Runtime.h"
#include <SDL.h>
#include <android/log.h>

void AndroidOverrideResolution() {
#ifdef __ANDROID__
    terFullScreen = 1;
    SDL_DisplayMode mode;
    // We use index 0 for the primary display.
    // Note: SDL_GetCurrentDisplayMode requires SDL to be initialized.
    // In Perimeter, HTManager::init() is called after SDL_Init().
    if (SDL_GetCurrentDisplayMode(0, &mode) == 0) {
        terScreenSizeX = mode.w;
        terScreenSizeY = mode.h;
        if (mode.refresh_rate > 0) {
            terScreenRefresh = mode.refresh_rate;
        }
        __android_log_print(ANDROID_LOG_INFO, "Perimeter", "Android resolution override: %dx%d %dhz", terScreenSizeX, terScreenSizeY, terScreenRefresh);
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "Perimeter", "Failed to get Android display mode: %s", SDL_GetError());
    }
#endif
}
