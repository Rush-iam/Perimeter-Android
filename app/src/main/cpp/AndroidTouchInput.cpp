#include "AndroidTouchInput.h"

#include <SDL.h>
#include <jni.h>

Uint32 androidTouchCameraDragEventType() {
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
