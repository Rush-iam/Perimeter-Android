"""Add the Android pause barrier needed by the Vulkan renderer."""
import pathlib
import sys


root = pathlib.Path(sys.argv[1])


def replace(relative_path, old, new):
    path = root / relative_path
    text = path.read_text(encoding="utf-8")
    if new in text:
        return
    if text.count(old) != 1:
        raise RuntimeError(
            f"Unexpected pinned SDL source in {relative_path}: {old!r}")
    path.write_text(text.replace(old, new), encoding="utf-8", newline="\n")


def replace_all(relative_path, old, new, expected_count):
    path = root / relative_path
    text = path.read_text(encoding="utf-8")
    if text.count(new) == expected_count:
        return
    if text.count(old) != expected_count:
        raise RuntimeError(
            f"Unexpected pinned SDL source in {relative_path}: {old!r}")
    path.write_text(text.replace(old, new), encoding="utf-8", newline="\n")


replace("src/video/android/SDL_androidvideo.h",
        "extern SDL_sem *Android_PauseSem, *Android_ResumeSem;",
        "extern SDL_sem *Android_PauseSem, *Android_ResumeSem, *Android_PauseAckSem;")

replace("src/video/android/SDL_androidvideo.c",
        "SDL_sem *Android_PauseSem = NULL;\nSDL_sem *Android_ResumeSem = NULL;",
        "SDL_sem *Android_PauseSem = NULL;\nSDL_sem *Android_ResumeSem = NULL;\nSDL_sem *Android_PauseAckSem = NULL;")

replace("src/core/android/SDL_android.c",
        """    Android_ResumeSem = SDL_CreateSemaphore(0);
    if (!Android_ResumeSem) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "failed to create Android_ResumeSem semaphore");
    }""",
        """    Android_ResumeSem = SDL_CreateSemaphore(0);
    if (!Android_ResumeSem) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "failed to create Android_ResumeSem semaphore");
    }

    Android_PauseAckSem = SDL_CreateSemaphore(0);
    if (!Android_PauseAckSem) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "failed to create Android_PauseAckSem semaphore");
    }""")

replace("src/core/android/SDL_android.c",
        """    if (Android_ResumeSem) {
        SDL_DestroySemaphore(Android_ResumeSem);
        Android_ResumeSem = NULL;
    }""",
        """    if (Android_ResumeSem) {
        SDL_DestroySemaphore(Android_ResumeSem);
        Android_ResumeSem = NULL;
    }

    if (Android_PauseAckSem) {
        SDL_DestroySemaphore(Android_PauseAckSem);
        Android_PauseAckSem = NULL;
    }""")

replace("src/core/android/SDL_android.c",
        """    SDL_SemPost(Android_PauseSem);
}""",
        """    SDL_SemPost(Android_PauseSem);
    if (Android_PauseAckSem)
        SDL_SemWait(Android_PauseAckSem);
}""")

# SDL event watches run synchronously from SDL_PushEvent. Posting the
# acknowledgement after WILLENTERBACKGROUND therefore means all DXVK event
# watches have completed their device-idle barrier before Java releases the
# old SurfaceView ANativeWindow.
replace_all("src/video/android/SDL_androidevents.c",
            "                SDL_SendAppEvent(SDL_APP_WILLENTERBACKGROUND);",
            """                SDL_SendAppEvent(SDL_APP_WILLENTERBACKGROUND);
                if (Android_PauseAckSem)
                    SDL_SemPost(Android_PauseAckSem);""",
            2)
