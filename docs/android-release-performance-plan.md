# Android Release Performance Optimization Plan

## Goal

Improve sustained game performance in Android release builds while preserving
gameplay correctness, deterministic simulation behavior, Android device
compatibility, and a clean path to a future Quest 3-specific build.

Keep implementation primarily in `app/` and `app/src/main/cpp`. Avoid changes
to the upstream `Perimeter/` tree unless an Android-guarded integration change
is unavoidable.

## Current baseline

- [x] Confirm the Android application targets `arm64-v8a`.
- [x] Confirm native code uses C++17 and the Android NDK Clang toolchain.
- [x] Confirm optimized native builds currently use `-O2` and `-DNDEBUG`.
- [x] Confirm function/data section splitting is enabled by the NDK.
- [x] Confirm stack protection and `_FORTIFY_SOURCE` remain enabled.
- [x] Confirm strict floating-point behavior is intentional:
  `-fno-fast-math -ffp-contract=off`.
- [x] Confirm DXVK is configured with Meson's `release` build type.
- [x] Confirm native LTO and PGO are not currently enabled.
- [x] Confirm AGP release optimization is explicitly disabled with
  `optimization.enable = false`.
- [ ] Generate and inspect an actual Release `compile_commands.json` and final
  native link command, rather than relying only on RelWithDebInfo output and
  CMake configuration.

## Phase 1: Establish a runtime benchmark

- [ ] Define a reproducible benchmark scenario with a fixed map, player count,
  camera path, unit load, and duration.
- [ ] Include representative campaign, skirmish, terrain deformation, heavy-unit,
  loading, and renderer workloads.
- [ ] Record average FPS and frame-time percentiles (P50, P90, P95, and P99).
- [ ] Record simulation/update time and render-thread time where available.
- [ ] Record CPU utilization, memory use, loading time, device temperature, and
  thermal throttling state.
- [ ] Warm the device before each run and measure sustained performance, not
  only cold-start performance.
- [ ] Repeat each test enough times to identify run-to-run variance.
- [ ] Save the device model, Android version, NDK version, renderer selection,
  build revision, and exact build options with every result.
- [ ] Retain a symbolized RelWithDebInfo profiling build alongside the stripped
  Release build.

## Phase 2: Add Android-scoped compiler optimization controls

- [ ] Add an Android-owned CMake interface target for release optimization
  settings.
- [ ] Apply it only to Perimeter-owned native targets that contribute to the
  Android game library.
- [ ] Add a configurable optimization-level option instead of permanently
  forcing aggressive flags.
- [ ] Benchmark `-O2` as the control against `-O3` on the same workload.
- [ ] Preserve `-fno-fast-math`, `-ffp-contract=off`, and `-fsigned-char`.
- [ ] Preserve the NDK's security and ABI flags.
- [ ] Verify that all engine static libraries receive the intended Release
  flags, not only the final shared-library target.
- [ ] Accept `-O3` only if sustained performance improves without unacceptable
  code-size, loading-time, stability, or thermal regressions.
- [ ] Do not enable `-Ofast`, `-ffast-math`, or `-march=native` globally.

## Phase 3: Evaluate ThinLTO

- [ ] Add a CMake option such as `PERIMETER_ANDROID_THIN_LTO`.
- [ ] Enable ThinLTO through CMake IPO support or `-flto=thin` for both compile
  and final link steps.
- [ ] Ensure participating engine static libraries use compatible LTO settings.
- [ ] Keep incompatible third-party and prebuilt libraries outside LTO.
- [ ] Confirm the Android linker used for the final shared library supports the
  chosen configuration.
- [ ] Compare build/link time, native-library size, APK size, startup time,
  frame-time percentiles, and sustained CPU performance against the control.
- [ ] Keep ThinLTO enabled by default only if the measured release benefit
  justifies its build cost and remains stable with both renderer selections.

## Phase 4: Enable Android app optimization

- [ ] Change the release build to `optimization.enable = true`.
- [ ] Add `.keep` rules for any JNI, reflection, or dynamically referenced Java
  and Kotlin classes that require them.
- [ ] Test all Activity entry points, JNI registration, native library loading,
  menus, content launch flows, and renderer selections.
- [ ] Compare APK/AAB size, startup time, and Java/Kotlin runtime behavior.
- [ ] Confirm native engine frame performance separately; R8/resource
  optimization is not expected to materially accelerate native game loops.

## Phase 5: Add a controlled PGO pipeline

- [ ] Stabilize the benchmark workload before collecting profiles.
- [ ] Add explicit CMake modes for PGO generation and PGO use.
- [ ] Compile and link the instrumented build with `-fprofile-generate`.
- [ ] Add an Android-safe mechanism to select the writable profile path.
- [ ] Explicitly call the LLVM profile-write function because Android process
  termination does not reliably run `atexit` handlers.
- [ ] Collect profiles from all representative runtime workloads.
- [ ] Pull `.profraw` data from the device and merge it with `llvm-profdata`.
- [ ] Use `llvm-profdata` from the same NDK version used to compile the app.
- [ ] Build the optimized release with `-fprofile-use`.
- [ ] Treat missing or badly outdated profile data as a clear build error or
  explicit fallback, rather than silently producing an unknown configuration.
- [ ] Record the source revision, benchmark revision, renderer selection, and NDK
  version associated with each profile.
- [ ] Compare PGO against the best non-PGO Release configuration.

## Phase 6: Keep CPU specialization optional

- [ ] Keep the general Android release on the portable `arm64-v8a` baseline.
- [ ] Do not add Quest-specific `-mcpu` or architecture-extension flags to the
  general Android build.
- [ ] Add a separate, clearly named Quest 3 optimization configuration only
  when Quest hardware testing begins.
- [ ] Verify that any Quest-specific instructions are supported across every
  CPU core on which Android may schedule the game.
- [ ] Benchmark Quest-specific tuning against the portable ARM64 build on the
  headset under sustained thermal load.
- [ ] Retain a portable fallback until the Quest-only artifact and distribution
  boundary is explicit.

## Phase 7: Release packaging and symbols

- [ ] Strip native symbols from packaged Release binaries.
- [ ] Produce and archive matching native debug symbols for crash
  symbolication.
- [ ] Verify stack traces can be symbolicated from a packaged release build.
- [ ] Compare `c++_shared` with static libc++ only if measurements identify
  startup time or package size as a meaningful issue.
- [ ] Use an Android App Bundle or ABI-specific artifacts for distribution.
- [ ] Replace debug signing with a proper release-signing configuration before
  publishing.

## Validation matrix

Run the following matrix after each compiler, linker, or PGO change:

- [ ] DXVK 1 Release builds successfully.
- [ ] DXVK 2 Release builds successfully.
- [ ] The Sokol fallback remains present and functional.
- [ ] The app installs and launches on the general Android test device.
- [ ] Campaign loading and gameplay complete without crashes or corruption.
- [ ] Save/load behavior remains correct.
- [ ] Multiplayer or deterministic simulation checks show no divergence.
- [ ] Movies, audio, input, networking, and terrain deformation remain correct.
- [ ] No new Clang or linker warnings indicate ignored optimization flags.
- [ ] Final compile and link commands contain the intended flags exactly once
  or in a deliberate overriding order.
- [ ] Performance results beat the recorded control outside normal variance.
- [ ] Performance remains acceptable after thermal steady state is reached.

## Recommended implementation order

- [ ] 1. Create the runtime benchmark and record the current Release baseline.
- [ ] 2. Add configurable Android-scoped `-O2`/`-O3` testing.
- [ ] 3. Add and benchmark optional ThinLTO.
- [ ] 4. Enable AGP release optimization and validate keep rules.
- [ ] 5. Configure stripping and release symbol archives.
- [ ] 6. Add PGO generation/use after the benchmark is stable.
- [ ] 7. Evaluate Quest 3-specific CPU tuning on headset hardware.

## Decision log

For every tested configuration, record:

- [ ] Exact compiler and linker flags.
- [ ] Build variant and renderer selection.
- [ ] Binary and package sizes.
- [ ] Build and link duration.
- [ ] Runtime benchmark results and variance.
- [ ] Thermal conditions and sustained-performance results.
- [ ] Correctness or compatibility failures.
- [ ] Decision: adopt, reject, or investigate further.
