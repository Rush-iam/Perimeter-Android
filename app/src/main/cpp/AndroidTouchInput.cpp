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
