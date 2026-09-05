# Perimeter Android - Project Context

## Project Goals
- **Primary Goal:** Port the classic C++ RTS game **Perimeter** (originally Windows/Linux) to Android.
- **Status:** Currently in the initial phase of getting a working Android build.
- **Future Goal:** Port to **Quest 3 VR Standalone**. Architectural decisions should favor abstractions that can eventually support VR/XR.

## Project Structure
- `/app`: The Android wrapper application (Kotlin/Java & JNI).
- `/Perimeter`: The core C++ engine (Upstream repository). **IMPORTANT:** Minimize changes here to allow for clean upstream merges.
- `/app/src/main/cpp`: Android-specific native code, CMake configuration, and JNI bridges.

## Build & Run
- **Build System:** Gradle (for Android) + CMake (for Native Engine).
- **Gradle Tasks:**
  - `./gradlew :app:assembleDebug`: Build the debug APK.
  - `./gradlew :app:installDebug`: Install the debug APK to a connected device.
- **ABI Filter:** Currently targeting `arm64-v8a`.
- **Min SDK:** 29 (Android 10).
- **Target SDK:** 37 (Android 15).

## Coding Standards & Guidelines
### Native Code (C++)
- **Standard:** C++17.
- **Minimal Invasive Changes:** Do not modify core logic in `/Perimeter` unless absolutely necessary. Use `Exodus` (the engine's platform abstraction layer) or add Android-specific files in `/app/src/main/cpp` (do not prefix names with "Exodus").
- **Platform Abstraction:** Use SDL2 for input, sound, and networking where possible.
- **Graphics:** Prefer **DirectX via DXVK** (primary path); **Sokol** is used only as a fallback.
- **Logging:** Use the Android log redirector (see `AndroidLog.cpp`) for native logs.

### Android Development
- **UI:** The wrapper uses standard Android Views/Activity for now; Jetpack Compose is preferred for any new Android-specific UI.
- **Namespace:** `com.queststoredb.perimeter`.

## Architecture Notes
- The engine is integrated as a library via `add_subdirectory(${PERIMETER_ROOT})` in the native `CMakeLists.txt`.
- Native dependencies (SDL2, SDL2_net, SDL2_mixer, SDL2_image, FFmpeg) are managed via `FetchContent` in CMake.
- Avoid tight coupling between the Android Activity and the Engine logic to facilitate the future Quest 3 (VR) port.
