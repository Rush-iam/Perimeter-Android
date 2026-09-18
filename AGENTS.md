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

### Gradle from the Codex Sandbox
- The sandbox PowerShell environment may expose Java's `user.home` as the filesystem root. Use the repository cache and quote the Java user-home argument so the Windows Gradle wrapper passes it through correctly:
  ```powershell
  $env:GRADLE_USER_HOME = Join-Path (Get-Location) '.gradle'
  .\gradlew.bat "-Duser.home=$env:USERPROFILE" --no-daemon :app:assembleDebug --console=plain
  ```
- For a connected-device install, use the same environment and replace `:app:assembleDebug` with `:app:installDebug`. Set `$env:HOME = $env:USERPROFILE` before invoking `adb` if it reports `Cannot mkdir '\.android': Permission denied`.
- Leave `ANDROID_USER_HOME` and `ANDROID_SDK_HOME` unset. `-Duser.home=$env:USERPROFILE` keeps Android tooling and debug signing pointed at the normal `%USERPROFILE%\.android` location; do not redirect them to a repository or sandbox-local keystore.
- If the sandbox denies access to `%USERPROFILE%\.android`, rerun the same Gradle command with elevated execution rather than redirecting Android's user directory or debug keystore.

### Signing and Device Installs
- Use Android Studio's default debug keystore at `%USERPROFILE%\.android\debug.keystore` for debug APKs installed on the configured Android device.
- The debug key alias is `androiddebugkey`. Keep the keystore outside the repository; do not substitute a repository-local or sandbox-local debug keystore.
- Do not set `ANDROID_USER_HOME` to a repository-local path when building or installing. The Android Gradle plugin derives the default debug keystore from that location, which would silently select `.android\debug.keystore` inside the repository and produce an APK that cannot update the Android Studio-signed install.
- If an isolated Gradle cache is needed, set `GRADLE_USER_HOME` only; leave `ANDROID_USER_HOME` unset so signing resolves to `%USERPROFILE%\.android\debug.keystore`.
- In shells where `adb` reports `Cannot mkdir '\.android': Permission denied`, the shell's `HOME` is empty. Set it from the Windows user profile before using `adb`: `$env:HOME = $env:USERPROFILE`. This keeps the device-auth directory at `%USERPROFILE%\.android`; do not point it at the repository. `ANDROID_USER_HOME` and `ANDROID_SDK_HOME` are not reliable fixes for this `adb` error.

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

### Frame-Pacing Benchmark Workflow
- Use the Android Studio-installed `debug` build and the established scripts under `scripts/frame-pacing/`.
- After restarting `ContentActivity`, wait **2 seconds** for the launcher, tap **Play**, then wait **16 seconds after that tap** for the game main menu. Do not run `Start-TutorialBenchmark.ps1` while the launcher is still visible.
- Obtain the current **Play** button bounds from `adb shell uiautomator dump /data/local/tmp/perimeter-ui.xml` before an automated tap, and tap their center. The launcher can appear in portrait or landscape, so fixed screen coordinates are unreliable.
- Start `Start-TutorialBenchmark.ps1` from the game's top menu with `-InitialMenuSeconds 0 -TransitionSeconds 2`; use the established mission-load wait and verify the gameplay HUD before capture. The script's menu navigation must not begin from the launcher or a submenu.
- `Capture-FramePacingRun.ps1` defaults to a **10-second warm-up**. The controlled zoom case is 1.5 seconds out, 1.5 seconds in, repeated seven times; capture it from a fresh Tutorial mission.
- Record native DXVK timing and, when analyzing presentation, run the passive SurfaceFlinger sampler concurrently.

### Commit Plan Workflow
- When asked to prepare a commit-splitting plan, inspect the Git repository named by the user. If the path is a nested repository or submodule, run Git from that repository's root; do not substitute the parent repository.
- Respect the requested change state exactly: use `git diff --cached` for staged changes and `git diff` for unstaged tracked changes. Do not stage, unstage, commit, or otherwise alter the index while preparing the plan.
- If the requested repository/state has no changes, report that fact instead of analyzing another repository or change state.
- Write or update a plan document in the inspected repository, with proposed commits grouped and sorted by category. Use these category icons consistently: `🛠️` build, `🐛` bugfix, `🕹️` controls, `🎨` graphics, `📝` logging, `🚀` optimization, `♻️` refactoring, `🧹` cleanup, `📚` docs, `🧪` testing, `🧰` tooling, and `📊` benchmark.
- Review every proposed commit for correctness, regressions, and bugs before presenting the plan. Inspect the complete diff assigned to each commit, including interactions across commit boundaries and nested repositories; record findings in the plan and revise the proposed boundaries or descriptions when needed.
- The commit-splitting plan is a working artifact and must never be included in a proposed commit or committed to the repository.
- Prefix each title with its category icon and category tag. Because this is the Android project, omit the redundant `[Android]` tag from its commits; use `[Android]` only when a mixed-scope repository needs to distinguish Android-only work.
- Make every commit description standalone and describe the completed change. Do not include instructions about what to perform, test, stage, validate, or fix, and do not make a commit description depend on another commit's description.
- Write each commit description in imperative mood, beginning with a command such as `Add`, `Update`, `Remove`, or `Fix` rather than a third-person form such as `Adds` or `Updates`.
- In the plan document, make the first paragraph under each proposed commit the exact standalone commit description; do not add a label such as `Standalone description:` before it.
- Include file and line-number references for every proposed commit. Split mixed-purpose files at hunk level, and exclude whitespace-only or no-op changes from functional commits.
- Keep dependency-aware ordering, cross-repository prerequisites, and review notes outside commit descriptions. Treat the described changes as already tested unless the user explicitly asks for a testing plan.

#### Approved Plan Commit Process
- Once the user approves the plan, recheck the inspected repository from its own root: confirm the intended branch, review the current worktree and index, and reconcile any changes made since the plan was prepared. Do not assume the plan is still an exact match.
- Commits in the core `/Perimeter` submodule must always be made on its `android` branch. Confirm that branch before committing and stop if the submodule is on another branch.
- For other repositories, commit only on the branch explicitly requested by the user. Do not switch branches, push, or modify the parent repository unless the user requests it.
- Follow the plan's dependency-aware order. For each commit, stage only the listed files and hunks; preserve unrelated staged, unstaged, untracked, whitespace-only, and no-op changes.
- Before each commit, inspect `git diff --cached --stat`, `git diff --cached --name-only`, and `git diff --cached`. Run `git diff --cached --check` with the repository's line-ending configuration so existing CRLF files are not rewritten merely to satisfy the check.
- Use the approved title and standalone description from the plan verbatim unless the user explicitly requests a description change. Create one commit at a time and verify its result before proceeding to the next.
- After the final commit, verify the branch, recent commit order, staged state, and worktree status. Report any intentionally preserved changes and confirm that nothing was pushed unless pushing was explicitly requested.

## Architecture Notes
- The engine is integrated as a library via `add_subdirectory(${PERIMETER_ROOT})` in the native `CMakeLists.txt`.
- Native dependencies (SDL2, SDL2_net, SDL2_mixer, SDL2_image, FFmpeg) are managed via `FetchContent` in CMake.
- Builds must not require or default to tools, sources, or caches under `.artifacts`; use host-installed tools, CMake/FetchContent fallbacks, or explicitly supplied external paths instead.
- Avoid tight coupling between the Android Activity and the Engine logic to facilitate the future Quest 3 (VR) port.
