#include "AndroidTouchInput.h"

#include <SDL.h>
#include <jni.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr float kGestureDeltaPrecision = 1024.0f;

int quantizeGestureDelta(float delta) {
    if (!std::isfinite(delta)) {
        return 0;
    }
    return std::clamp(static_cast<int>(std::lround(delta * kGestureDeltaPrecision)),
                      -32768, 32767);
}
}

Uint32 androidTouchCameraDragEventType() {
    static const Uint32 eventType = SDL_RegisterEvents(1);
    return eventType;
}

Uint32 androidTouchCameraRotationEventType() {
    static const Uint32 eventType = SDL_RegisterEvents(1);
    return eventType;
}

Uint32 androidTouchTwoFingerGestureEventType() {
    static const Uint32 eventType = SDL_RegisterEvents(1);
    return eventType;
}

extern "C" JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_onNativeCameraDrag(JNIEnv*, jclass, jboolean active) {
    const Uint32 eventType = androidTouchCameraDragEventType();
    if (eventType == static_cast<Uint32>(-1)) {
        return;
    }

    SDL_Event event{};
    event.type = eventType;
    event.user.type = eventType;
    event.user.code = active ? 1 : 0;
    if (SDL_PushEvent(&event) < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Could not queue Android camera drag event: %s", SDL_GetError());
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_onNativeMouseButton(
        JNIEnv*, jclass, jint button, jboolean pressed, jfloat x, jfloat y) {
    if (pressed) {
        // Prime SDL's internal cursor position before the game switches to
        // relative mouse mode for camera look.
        SDL_WarpMouseInWindow(nullptr, static_cast<int>(std::lround(x)),
                              static_cast<int>(std::lround(y)));
    }

    SDL_Event event{};
    event.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    event.button.type = event.type;
    event.button.button = static_cast<Uint8>(button);
    event.button.state = pressed ? SDL_PRESSED : SDL_RELEASED;
    event.button.clicks = 1;
    event.button.x = static_cast<int>(x);
    event.button.y = static_cast<int>(y);
    if (SDL_PushEvent(&event) < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Could not queue Android mouse button event: %s", SDL_GetError());
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_onNativeMouseButtonNoWarp(
        JNIEnv*, jclass, jint button, jboolean pressed) {
    int x = 0;
    int y = 0;
    SDL_GetMouseState(&x, &y);

    SDL_Event event{};
    event.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    event.button.type = event.type;
    event.button.button = static_cast<Uint8>(button);
    event.button.state = pressed ? SDL_PRESSED : SDL_RELEASED;
    event.button.clicks = 1;
    event.button.x = x;
    event.button.y = y;
    if (SDL_PushEvent(&event) < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Could not queue Android mouse button event: %s", SDL_GetError());
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_onNativeCameraRotation(
        JNIEnv*, jclass, jfloat horizontalDelta, jfloat verticalDelta) {
    const Uint32 eventType = androidTouchCameraRotationEventType();
    if (eventType == static_cast<Uint32>(-1)) {
        return;
    }

    const Uint32 horizontalBits = static_cast<Uint32>(
            quantizeGestureDelta(horizontalDelta) + 32768);
    const Uint32 verticalBits = static_cast<Uint32>(
            quantizeGestureDelta(verticalDelta) + 32768);
    const Uint32 packed = (horizontalBits << 16) | verticalBits;

    SDL_Event event{};
    event.type = eventType;
    event.user.type = eventType;
    std::memcpy(&event.user.code, &packed, sizeof(packed));
    if (SDL_PushEvent(&event) < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Could not queue Android camera rotation event: %s", SDL_GetError());
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_onNativeTwoFingerGesture(
        JNIEnv*, jclass, jfloat verticalWheelDelta, jfloat pinchZoomDelta) {
    const Uint32 eventType = androidTouchTwoFingerGestureEventType();
    if (eventType == static_cast<Uint32>(-1)) {
        return;
    }

    // Pack signed fixed-point deltas into the custom event code so the SDL event
    // queue owns the complete payload without a separately allocated object.
    const Uint32 wheelBits = static_cast<Uint32>(quantizeGestureDelta(verticalWheelDelta) + 32768);
    const Uint32 zoomBits = static_cast<Uint32>(quantizeGestureDelta(pinchZoomDelta) + 32768);
    const Uint32 packed = (wheelBits << 16) | zoomBits;

    SDL_Event event{};
    event.type = eventType;
    event.user.type = eventType;
    std::memcpy(&event.user.code, &packed, sizeof(packed));
    if (SDL_PushEvent(&event) < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Could not queue Android two-finger gesture event: %s", SDL_GetError());
    }
}
