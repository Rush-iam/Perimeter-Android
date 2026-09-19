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

Use the workload, launch, timing, and capture procedure in
[dxvk1-d60-performance-benchmark.md](dxvk1-d60-performance-benchmark.md),
with the APK changed from its historical `debug` setting to the Android
`releaseBenchmark` variant. This variant is initialized from `Release` and is
debuggable only so the established `run-as` harness can retrieve native timing
files; the production `Release` APK remains non-debuggable and is built
separately. The existing D60 captures cannot serve as release controls because
they were captured from a debug build. The performance corpus is
intentionally limited to the DXVK 1 path and the two camera workloads that
exercise the expensive terrain preparation code:

- [x] Build the production Android `Release` APK and the matching
  `releaseBenchmark` APK with DXVK 1. Install `releaseBenchmark` for capture,
  record the build revision and native/compiler configuration, and verify that
  its native configuration matches the production Release variant.
- [x] Run the Campaign **Tutorial** mission on the fixed Android test device,
  with a fresh mission before every moving or zooming capture.
- [x] Run only DXVK 1 moving and zooming cases. Do not add static, DXVK 2, or
  Sokol performance runs to this corpus; those remain separate regression or
  reference checks.
- [x] For moving, hold Right, Up, Left, and Down for one second each and repeat
  the sequence five times. For the canonical zooming control, zoom out for
  1.5 seconds and in for 1.5 seconds, repeating the pair 21 times. The original
  seven-cycle zoom control remains as a superseded short-window diagnostic.
- [x] Use the documented 10-second warm-up, 3-second all-core CPU-load pulse,
  post-load timing flush, and 20-second moving / 63-second canonical zooming
  measurement windows.
- [x] Capture two moving control runs and two zooming control runs for the
  release baseline. Capture two runs per candidate and workload, retaining
  every raw capture directory. This two-run release matrix is an explicit
  scope choice; the referenced D60 document contains a larger historical
  moving-control corpus.
- [x] Record native frame intervals (P50, P90, P95, P99, maximum), long-frame
  counts, and the corrected DXVK timing trace.
- [ ] Correlate with SurfaceFlinger sampling when presentation behavior is under
  investigation.
- [x] Use the combined tile-preparation CPU time as a primary engine metric:
  per-frame duration of `tilemap_predraw_calc` +
  `tilemap_border_rebuild` + `bump_tile_calc_total` from `frame-work.csv`.
  Report its P50, P95, P99, maximum, and measurement-window total, together
  with each component's duration so a change cannot hide a regression in one
  stage.
- [x] Report `bump_tile_calc_count` alongside the combined duration as the
  workload/count signal. It is a per-frame count, not a duration; retain its
  P95, P99, maximum, and total to distinguish fewer calculations from cheaper
  calculations.
- [ ] Record CPU utilization, memory use, loading time, device temperature,
  thermal throttling state, device model, Android
  version, NDK version, build revision, renderer selection, and exact build
  options with every result.
- [ ] Warm the device and evaluate sustained performance under the documented
  Performance-mode conditions, not only cold-start performance.
- [ ] Retain a symbolized RelWithDebInfo profiling build alongside the stripped
  Release build.

### Canonical Release/DXVK1 control matrix

The first release baseline uses the `releaseBenchmark` APK, which is
initialized from the production `Release` build and is debuggable only for the
timing harness. Huawei Performance mode was manually confirmed for this
session. The production `Release` APK was built separately and is not used to
retrieve diagnostic files. The captures were made on revision
`bb3092f649364191943527604c5887a04149952c` with a dirty working tree because
the benchmark-variant changes were present.

The combined signal below is the per-frame sum of the recorded
`tilemap_predraw_calc`, `tilemap_border_rebuild`, and `bump_tile_calc_total`
durations. These events can be nested, so the sum is an attributed CPU-work
signal rather than a wall-clock frame duration. Component P99 values are shown
as `predraw / border / bump` in milliseconds.

| Scenario | Run | Frames | Native P50 / P95 / P99 / max (ms) | >=20.833 / >=33.333 / >=50 ms | Combined P50 / P95 / P99 / max (ms) | Combined total (ms) | Component P99 (ms) | `bump_tile_calc_count` P50 / P95 / P99 / max / total |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Moving | 1 | 2718 | 18.985 / 33.651 / 39.898 / 50.282 | 1111 / 205 / 1 | 1.588 / 3.528 / 4.791 / 105.241 | 5155.298 | 1.769 / 1.536 / 1.485 | 6 / 17 / 26 / 36 / 19285 |
| Moving | 2 | 3104 | 17.694 / 31.024 / 35.452 / 156.200 | 620 / 82 / 1 | 1.471 / 3.154 / 4.224 / 109.018 | 5396.244 | 1.579 / 1.344 / 1.286 | 6 / 15 / 20 / 33 / 19268 |
| Zooming | 1 | 3100 | 19.515 / 27.048 / 32.897 / 109.198 | 1106 / 26 / 2 | 3.878 / 7.499 / 10.801 / 127.989 | 13262.561 | 3.843 / 3.519 / 3.435 | 36 / 65 / 75 / 98 / 117749 |
| Zooming | 2 | 3056 | 19.793 / 27.661 / 33.072 / 78.667 | 1189 / 29 / 1 | 3.866 / 7.723 / 10.774 / 135.769 | 13003.785 | 3.838 / 3.532 / 3.451 | 36 / 66 / 78 / 99 / 117864 |

Raw captures:

- `d60-performance-release-long-control-moving-retry-dxvk1-full-refresh-run01-20260918-215934`
- `d60-performance-release-long-control-moving-retry-dxvk1-full-refresh-run02-20260918-220304`
- `d60-performance-release-long-control-zooming-dxvk1-full-refresh-run01-20260918-213953`
- `d60-performance-release-long-control-zooming-dxvk1-full-refresh-run02-20260918-214322`

The original short moving and zooming captures are retained as superseded
diagnostics:

- `d60-performance-release-control-moving-dxvk1-full-refresh-run01-20260918-210246`
- `d60-performance-release-control-moving-dxvk1-full-refresh-run02-20260918-210557`
- `d60-performance-release-control-zooming-dxvk1-full-refresh-run01-20260918-211105`
- `d60-performance-release-control-zooming-dxvk1-full-refresh-run02-20260918-211404`

The first long moving pair is retained as an additional diagnostic:

- `d60-performance-release-long-control-moving-dxvk1-full-refresh-run01-20260918-213251`
- `d60-performance-release-long-control-moving-dxvk1-full-refresh-run02-20260918-213624`

The earlier failed non-debuggable `Release` attempt and the interrupted third
moving attempt are not part of any matrix.

### Baseline variance and measurement strategy

The original short-window pair showed material frame-tail spread. Moving native
P95/P99/max were 24.057/29.332/36.736 ms and 33.669/40.362/45.351 ms, while
the short zooming pair reported 42.986/49.912/69.008 ms and
33.308/45.495/86.989 ms. The combined totals and total bump-tile counts were
already comparatively stable in those short captures, differing by about 2.0%
and 0.7% for moving and 1.7% and 3.5% for zooming.

The three-times-longer zoom window materially improved repeatability. Native
P95/P99 spread fell to about 2%/0.5%, and combined P95/P99 spread fell to about
3%/0.3%; combined totals differed by about 2.0% and bump-tile count totals by
about 0.1%. The 63-second, 21-cycle zoom control is therefore now canonical.

The first three-times-longer moving pair was inconclusive: its combined totals
and bump-tile counts differed by about 50% and 59%. Repeating the pair with the
same protocol produced combined P95/P99 values of 3.528/4.791 ms and
3.154/4.224 ms, combined totals within about 4.6%, and bump-tile count totals
within about 0.1%. Native P95/P99 spread also fell to about 8%/11%. The retry
pair is therefore now the canonical moving control. Its second run includes
18 pipeline compilations and a 156.200 ms native maximum; that presentation
tail is recorded as a known harness/device artifact rather than a terrain-work
regression.

Tripling the measurement window improves percentile sampling: a P99 is now
based on roughly 30 samples in the long runs instead of roughly 8-11. It does
not remove run-to-run variance caused by CPU scheduling, presentation timing,
thermal/frequency state, or rare rebuild placement. Continue standardizing
cooldown and Performance-mode state, and collect synchronized SurfaceFlinger
data when presentation tails are under investigation. Compare both runs rather
than selecting the better-looking run.

The canonical matrix now uses the 60-second moving/15-cycle and 63-second
zooming/21-cycle controls, with exactly two runs per workload.

The original release controls and ThinLTO moving captures used a 250 ms
post-load timing-flush wait and remain valid results for their recorded
protocol. New captures use a 2-second post-load settling wait to reduce the
chance that the all-core frequency-boost pulse affects the camera window; the
gap is recorded in each capture's metadata and comparisons keep the protocols
separate.

For each candidate, compare the engine and presentation metrics against all
fresh controls for the same workload. Treat a lower combined tile-preparation
time as meaningful only when it does not regress native frame tails, visual
terrain correctness, or display cadence. Do not combine candidates in the
first A/B comparison.

## Phase 2: Add Android-scoped compiler optimization controls

- [x] Add an Android-owned CMake interface target for release optimization
  settings.
- [x] Apply it only to Perimeter-owned native targets that contribute to the
  Android game library.
- [x] Add a configurable optimization-level option instead of permanently
  forcing aggressive flags.
- [x] Benchmark `-O2` as the control against `-O3` on the same workload.
- [x] Preserve `-fno-fast-math`, `-ffp-contract=off`, and `-fsigned-char`.
- [x] Preserve the NDK's security and ABI flags.
- [x] Verify that all engine static libraries receive the intended Release
  flags, not only the final shared-library target.
- [ ] Accept `-O3` only if sustained performance improves without unacceptable
  code-size, loading-time, stability, or thermal regressions.
- [ ] Do not enable `-Ofast`, `-ffast-math`, or `-march=native` globally.

### Phase 2 O3 candidate result

The Android-owned switch was tested with the same `releaseBenchmark`, DXVK1,
50% resolution, full-refresh, Performance-mode procedure and canonical two-run
moving/zooming matrix. The default O2 build was verified with one `-O2` in the
Perimeter compile commands. The O3 build deliberately appends one `-O3` after
that baseline flag; all 319 Perimeter source compile commands include it, and
the DXVK dependencies remain outside the Android-owned target list.

| Scenario / run | O2 native P95 / P99 (ms) | O3 native P95 / P99 (ms) | O2 combined P95 / P99 (ms) | O3 combined P95 / P99 (ms) | O2 / O3 count total |
| --- | ---: | ---: | ---: | ---: | ---: |
| Moving / 1 | 33.651 / 39.898 | 24.637 / 33.212 | 3.528 / 4.791 | 2.983 / 4.091 | 19285 / 19721 |
| Moving / 2 | 31.024 / 35.452 | 27.053 / 33.993 | 3.154 / 4.224 | 2.970 / 4.193 | 19268 / 19275 |
| Zooming / 1 | 27.048 / 32.897 | 29.619 / 39.301 | 7.499 / 10.801 | 7.520 / 11.130 | 117749 / 116737 |
| Zooming / 2 | 27.661 / 33.072 | 37.043 / 44.792 | 7.723 / 10.774 | 7.858 / 12.740 | 117864 / 113293 |

O3 improves the moving workload's native and combined tails, with similar
`bump_tile_calc_count` totals. It does not improve the zooming workload: both
zoom runs have worse native tails, and combined P99 is higher in both runs.
The lower O3 zooming measurement-window totals are accompanied by fewer
captured frames, especially in run 2, so they are not evidence of a CPU-work
win. Keep O2 as the default and reject O3 as the general Android optimization
level; retain the switch for future workload-specific experiments.

O3 raw captures:

- `d60-performance-o3-moving-dxvk1-full-refresh-run01-20260918-223543`
- `d60-performance-o3-moving-dxvk1-full-refresh-run02-20260918-223913`
- `d60-performance-o3-zooming-dxvk1-full-refresh-run01-20260918-224247`
- `d60-performance-o3-zooming-dxvk1-full-refresh-run02-20260918-224620`

## Phase 3: Evaluate ThinLTO

- [x] Add the Android-owned `PERIMETER_ANDROID_THIN_LTO` CMake option and
  `androidThinLto` Gradle property.
- [x] Enable ThinLTO through `-flto=thin` for both compile
  and final link steps.
- [x] Ensure participating engine static libraries use compatible LTO settings.
- [x] Keep incompatible third-party and prebuilt libraries outside LTO.
- [x] Confirm the Android linker used for the final shared library supports the
  chosen configuration.
- [x] Compare build/link time, native-library size, APK size, and frame-time
  percentiles against the control for the DXVK1 benchmark path.
- [ ] Keep ThinLTO enabled by default only if the measured release benefit
  justifies its build cost and remains stable with both renderer selections.

### Phase 3 ThinLTO DXVK1 result

ThinLTO was tested with the same Android Studio-compatible `releaseBenchmark`
build, DXVK1 renderer, 50% resolution, full refresh, Performance mode, and
two-run-per-workload protocol. The moving comparison retains the established
250 ms post-load gap. Because the gap was then increased to reduce possible
load-pulse carry-over, the zoom comparison uses fresh O2 and ThinLTO pairs
with a 2,000 ms post-load settling wait. The earlier 250 ms captures remain
valid historical results and are not mixed into the new-gap zoom comparison.

| Scenario / run | O2 native P95 / P99 (ms) | ThinLTO native P95 / P99 (ms) | O2 combined P95 / P99 (ms) | ThinLTO combined P95 / P99 (ms) | O2 / ThinLTO count total |
| --- | ---: | ---: | ---: | ---: | ---: |
| Moving / 1 (250 ms gap) | 33.651 / 39.898 | 19.584 / 23.574 | 3.528 / 4.791 | 2.870 / 3.905 | 19285 / 19486 |
| Moving / 2 (250 ms gap) | 31.024 / 35.452 | 24.762 / 30.351 | 3.154 / 4.224 | 3.019 / 3.892 | 19268 / 19335 |
| Zooming / 1 (2 s gap) | 31.838 / 35.366 | 28.557 / 33.056 | 8.637 / 12.273 | 7.322 / 11.049 | 115993 / 115083 |
| Zooming / 2 (2 s gap) | 30.236 / 33.464 | 25.220 / 26.767 | 8.157 / 12.736 | 6.926 / 10.241 | 116243 / 117498 |

ThinLTO improves the native and combined tails in both workloads, with similar
`bump_tile_calc_count` totals. The zoom comparison also has more captured
frames in both ThinLTO runs than in the corresponding O2 controls, so the
lower combined totals are not caused by a shorter measurement window. DXVK1
pipeline compilation counts were zero for all four new-gap zoom captures and
both complete ThinLTO moving captures.

The observed clean configuration switches took about 9m27s for ThinLTO and
8m16s for O2; these include native dependency reconfiguration and are not a
stable incremental-build measurement. ThinLTO produced a 54,019,820-byte
benchmark APK and an 11,272,952-byte stripped `libperimeter.so`, versus
54,414,144 bytes and 12,670,816 bytes for O2. The unstripped native library
was 106,430,824 bytes with ThinLTO versus 134,408,600 bytes with O2.

Keep ThinLTO optional for now: the DXVK1 evidence is positive, but the plan's
default-adoption gate also requires the remaining renderer/correctness and
sustained-performance checks. O2 is restored on the test device after the
experiment.

ThinLTO captures:

- `d60-performance-thinlto-moving-dxvk1-full-refresh-run01-20260918-231035`
- `d60-performance-thinlto-moving-dxvk1-full-refresh-run02-20260918-231725`
- `d60-performance-thinlto-zooming-2s-settle-dxvk1-full-refresh-run01-20260918-232451`
- `d60-performance-thinlto-zooming-2s-settle-dxvk1-full-refresh-run02-20260918-232824`

Matching new-gap O2 zoom controls:

- `d60-performance-o2-zooming-2s-settle-dxvk1-full-refresh-run01-20260918-234201`
- `d60-performance-o2-zooming-2s-settle-dxvk1-full-refresh-run02-20260918-234523`

The first interrupted ThinLTO moving run 2 later completed in the background
but remains superseded by the explicit recapture:
`d60-performance-thinlto-moving-dxvk1-full-refresh-run02-20260918-231402`.

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

The runtime performance measurement in this plan remains the DXVK 1
moving/zooming corpus defined in Phase 1. The other renderer rows below are
build and correctness gates, not additional performance workloads.

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

- [x] 1. Capture the DXVK 1 moving and zooming control corpus, including the
  combined tile-preparation CPU metric, and record the current Release baseline.
- [x] 2. Add configurable Android-scoped `-O2`/`-O3` testing.
- [x] 3. Add and benchmark optional ThinLTO.
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
- [ ] Combined `tilemap_predraw_calc` + `tilemap_border_rebuild` +
  `bump_tile_calc_total` time, its component durations, and
  `bump_tile_calc_count` statistics.
- [ ] Thermal conditions and sustained-performance results.
- [ ] Correctness or compatibility failures.
- [ ] Decision: adopt, reject, or investigate further.
