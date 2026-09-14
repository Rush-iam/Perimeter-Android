# DXVK 1 Frame-Pacing Benchmark

## Status and scope

This document defines the reproducible Android benchmark and records timing
evidence. The current investigation is D30: 75% rendering scale, VSync, and
half-refresh on the Huawei SCM-W09 (Android 10, 60 Hz). D60 / interval-1 is
explicitly deferred to the separate
[D60 Performance-mode benchmark](dxvk1-d60-performance-benchmark.md).

All D30 test results and diagnostics recorded in this document were performed
with Huawei system Performance mode disabled, in the normal/non-Performance
device mode. They must not be described as Performance-mode results or reused
as D60 Performance-mode controls.

All reported benchmarks and diagnostics use the in-game **Fast** graphics
profile. Do not compare runs that differ in content, renderer, display mode,
resolution scale, frame-rate setting, or graphics profile.

## Fixed test conditions

| Field | Value |
| --- | --- |
| APK | `debug` build (`app-debug.apk`), runtime DXVK 1 |
| Device | Huawei SCM-W09, Android 10, 60 Hz |
| Graphics profile | Fast |
| Resolution scale | 75% |
| Target | Half-refresh / D30 |
| Sound | Enabled |
| Native timing | `frame_timing=1` |
| Work-log mode | Compact |
| Warm-up | 10 seconds |
| CPU power mode | Normal/non-Performance mode; Huawei Performance mode was disabled |
| Measurement window | 20 seconds static/moving; 21 seconds zooming |
| Runs | Two per renderer/scenario |

## Workload

The workload is the single-player Campaign mission **Tutorial**.

| Scenario | Camera procedure | Fresh Tutorial required? |
| --- | --- | --- |
| Static | Leave the initial camera unchanged | No; static runs may repeat in place |
| Moving | Hold Right, Up, Left, Down for one second each; repeat five times | Yes; every moving run starts from a fresh Tutorial |
| Zooming | Zoom out for 1.5 seconds, zoom in for 1.5 seconds; repeat seven times | Yes; every zooming run starts from a fresh Tutorial |

The held-square sequence lasts 20 seconds. It avoids the old click-driven loop,
which produced rare movements while spending much of a capture blocked at a
map edge. The zooming sequence lasts 21 seconds and uses the same fixed input
duration for every run.

### Reliable launch sequence

`Start-TutorialBenchmark.ps1` starts from Perimeter's landscape main menu.
After restarting `ContentActivity`, wait two seconds for the launcher, tap
**Play**, then wait 17 seconds after that tap for the game main menu before
invoking it. Use three-second menu transitions. The device has
occasionally needed longer post-Play and mission-load waits; do not begin a
capture until the normal gameplay HUD is visible.

## Capture procedure

1. Verify renderer, Fast profile, 75% scale, half-refresh, diagnostics, and
   that Huawei Performance mode is disabled.
2. Start Tutorial and wait for the HUD.
3. Capture static runs first with `-CameraPan none`.
4. Restart Tutorial before every moving run.
5. Restart Tutorial before every zooming run.
6. Retain the entire generated capture directory.

Examples:

```powershell
./scripts/frame-pacing/Capture-FramePacingRun.ps1 `
  -Renderer dxvk1 -Run 1 -Scenario tutorial-held-square-final-accepted-stack `
  -WarmupSeconds 10 -CaptureSeconds 20 -CameraPan held-square-5x -AssumeReady

./scripts/frame-pacing/Analyze-FrameTiming.ps1 `
  -CaptureDirectory captures/frame-pacing/<capture-directory>
```

Zooming baseline capture:

```powershell
./scripts/frame-pacing/Capture-FramePacingRun.ps1 `
  -Renderer dxvk1 -Run 1 -Scenario tutorial-zoom-out-in-baseline `
  -WarmupSeconds 10 -CaptureSeconds 21 -CameraPan zoom-out-in-7x -AssumeReady
```

`gfxinfo` describes Android wrapper frames only. Use native `frame-timing.csv`
for engine cadence; use `frame-work.csv` for engine-stage attribution; use
DXVK's trace only for DXVK stages. None of these is a compositor presentation
timestamp.

## Current final-stack comparison

The current accepted zoom stack is the shared alternate-LOD cache. Candidate
seam restoration, budgeted builds, texture reuse, point/region reuse, and
topology reuse were removed after controlled A/B comparisons. The rejected
optimizations are now tracked as possible D60 Performance-mode candidates in
[dxvk1-d60-performance-benchmark.md](dxvk1-d60-performance-benchmark.md).

### Static camera

| Renderer / run | Frames | P50 ms | P99 ms | Max ms |
| --- | ---: | ---: | ---: | ---: |
| DXVK 1 / 1 | 599 | 33.314 | 34.337 | 40.175 |
| DXVK 1 / 2 | 599 | 33.309 | 34.657 | 36.091 |
| Sokol / 1 | 599 | 33.256 | 35.569 | 38.082 |
| Sokol / 2 | 599 | 33.166 | 35.412 | 36.424 |

### Moving camera

| Renderer / run | Frames | P95 ms | P99 ms | Max ms | >=37.5 ms | >=50 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| DXVK 1 / 1 | 629 | 35.454 | 49.998 | 72.953 | 21 | 6 |
| DXVK 1 / 2 | 599 | 34.829 | 45.202 | 77.499 | 12 | 1 |
| Sokol / 1 | 629 | 36.827 | 40.220 | 41.968 | 26 | 0 |
| Sokol / 2 | 599 | 36.564 | 41.631 | 45.679 | 16 | 0 |

Capture prefixes: `tutorial-static-full-topology-cache`,
`tutorial-held-square-final-accepted-stack-dxvk1`, and
`tutorial-*-final-accepted-stack-sokol`.

The shared zoom-cache build also passed one DXVK moving-camera regression run:

| Renderer / run | Frames | P95 ms | P99 ms | Max ms | >=37.5 ms | >=50 ms | Cache hits |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| DXVK 1 / shared-cache regression 1 | 599 | 34.078 | 38.177 | 53.028 | 7 | 1 | 1,042 |
| DXVK 1 / shared-cache regression 2 | 599 | 34.297 | 41.505 | 51.054 | 9 | 2 | 1,187 |

Raw captures: `captures/frame-pacing/tutorial-held-square-shared-cache-regression-dxvk1-dxvk1-half-refresh-run01-20260912-135243` and
`captures/frame-pacing/tutorial-held-square-shared-cache-regression-dxvk1-dxvk1-half-refresh-run02-20260912-135559`.
Both are below the prior DXVK moving P99 values of 49.998 / 45.202 ms; no
Sokol capture was needed because the implementation uses only shared tilemap
resource APIs.

### >=50 ms outlier correlation

`scripts/frame-pacing/Correlate-FramePacingOutliers.ps1` joins each frame
interval at or above 50 ms with its originating frame's native work records
and overlapping DXVK timing events. It writes `outlier-correlation.csv` into
each capture directory. `tilemap_work_ms` is a diagnostic sum and includes
nested events; use `tilemap_predraw_calc_ms` and
`tilemap_border_rebuild_ms` for the top-level tile-preparation signal.
Likewise, DXVK overlap fields are event overlap, not exclusive wall time. Some
retained reports were captured while the generated DXVK source still contained
duplicate `queue_present` instrumentation, so their summed `queue_present`
overlap is historical evidence only and must not be interpreted as multiple
actual present calls. Corrected traces record one `queue_present` call per
native frame.

| Workload | Runs | >=50 ms outliers | Tilemap + presentation | Presentation-only |
| --- | --- | ---: | ---: | ---: |
| Zoom baseline | 1 / 2 | 17 / 15 | 17 / 15 | 0 / 0 |
| Zoom shared cache | 1 / 2 | 12 / 7 | 10 / 7 | 2 / 0 |
| Moving baseline | 1 / 2 | 6 / 1 | 5 / 1 | 1 / 0 |
| Moving shared cache | 1 / 2 | 1 / 2 | 1 / 1 | 0 / 1 |

The zoom cache reduced the tilemap side of the tails, but it did not remove
presentation blocking. The retained cache correlation includes a
70.093 ms summed `queue_present` overlap for frame 9570; because that capture
predates the instrumentation fix, use its native present/wait records and the
later SurfaceFlinger correlation rather than treating that sum as two actual
presents. The remaining zoom outliers with large tilemap calculation were
generally 41-60 ms in `tilemap_predraw_calc`.

The moving cache runs show the same split: frame 4712 was mixed (11.636 ms
tilemap calculation and a 31.126 ms present wait), while frame 10519 was
presentation-dominant (2.667 ms tilemap calculation and a 40.831 ms present
wait). `queue_submit` remained sub-millisecond in these outliers, and
`frame_latency_wait` was effectively zero except where the present wait itself
was long. The engine cache therefore addresses the rebuild component; the next
investigation is compositor/presentation timing rather than another tilemap
cache variant.

### SurfaceFlinger correlation

`scripts/frame-pacing/Correlate-CompositorFrames.ps1` joins each native
interval at or above 50 ms with the `actual_present_ns` records from a
passive SurfaceFlinger latency sample. The sample must overlap the native
measurement window; the synchronized capture below was started from one
PowerShell process so the monotonic timestamp ranges overlap. The report also
checks for a SurfaceFlinger cadence gap of at least 42.5 ms crossing each
native interval.

Raw files:

- Native/DXVK capture:
  `captures/frame-pacing/tutorial-zoom-out-in-shared-cache-compositor-dxvk1-half-refresh-run02-20260912-145221`
- SurfaceFlinger sample:
  `captures/frame-pacing/tutorial-zoom-out-in-shared-cache-compositor-run02-20260912-150000.csv`
- Correlation report:
  `compositor-correlation.csv` in the native capture directory.

The synchronized results are:

| Workload | SF records | SF P50 / P95 / P99 / max | Native >=50 ms | Compositor miss | Native-only |
| --- | ---: | --- | ---: | ---: | ---: |
| Zoom shared cache, run 2 | 1,497 | 33.325 / 49.985 / 49.992 / 83.312 ms | 8 | 7 | 1 |
| Moving shared cache, run 1 | 1,500 | 33.325 / 33.363 / 50.014 / 66.688 ms | 3 | 2 | 1 |

In the zoom window, seven of eight native intervals at or above 50 ms were
crossed by a SurfaceFlinger gap of at least 42.5 ms. The native-only frame was
frame 9631 (53.843 ms); it had one actual present in the interval and a
33.327 ms maximum adjacent compositor gap. In the moving window, frame 7566
was a mixed tilemap/compositor miss (65.748 ms native, 60.324 ms tilemap
calculation), frame 8080 was presentation-heavy and also crossed a 50.015 ms
compositor gap, and frame 7640 was native-only by this criterion. This is
direct evidence that most of the remaining tail is visible as a compositor
present miss, while a smaller part remains engine-side or is masked by a
normally paced display interval.

This is still latency sampling rather than a full FrameTimeline trace, so it
does not identify the display consumer or GPU queue owner. It is sufficient to
prioritize compositor/presentation investigation ahead of another terrain
cache variant.

The corrected SwappyVk instrumentation was also checked in a diagnostic zoom
run after a 60-second warm-up. It recorded 600 `queue_present` calls for 600
native frames. An earlier count of two records per frame was caused by duplicate
instrumentation left in the generated DXVK source, not by two actual present
calls. This establishes the one-present-call rate, but not display phase lock;
future captures use the shorter 10-second warm-up because the longer warm-up
did not remove the remaining compositor/native tails.

### Corrected SwappyVk validation

Three fresh DXVK zoom runs used the then-standard 15-second launcher-to-game-menu
wait, three-second menu transitions, a 10-second warm-up, and synchronized
passive SurfaceFlinger sampling. Future fresh launches use the corrected
17-second wait:

| Run | Native frames | Native P95 / P99 / max | Native >=50 ms | SF frames | SF P95 / P99 / max | SF gaps >=42.5 ms | Compositor miss / native-only / no-present | `queue_present` / native |
| --- | ---: | --- | ---: | ---: | --- | ---: | --- | ---: |
| 1 | 630 | 40.603 / 63.354 / 102.077 ms | 14 | 1,341 | 33.358 / 50.018 / 99.974 ms | 61 | 10 / 1 / 3 | 630 / 630 |
| 2 | 600 | 44.834 / 69.431 / 82.480 ms | 20 | 1,338 | 49.987 / 50.031 / 100.020 ms | 96 | 13 / 1 / 6 | 600 / 600 |
| 3 | 600 | 49.619 / 79.221 / 182.072 ms | 23 | 1,052 | 49.988 / 66.693 / 166.626 ms | 82 | 15 / 5 / 3 | 600 / 600 |

The one-present-call gate therefore passes, with no pipeline compilations in
either measurement window, but the display-cadence gate fails: most native
outliers coincide with a SurfaceFlinger gap. This does not justify another
Swappy call-rate change. The engine-side trigger remains visible in run 2;
frame 21009 alone spent 60.771 ms in `tilemap_predraw_calc`, 56.278 ms in
`tilemap_border_rebuild`, and made 116 `bump_tile_calc_call` operations. The
third synchronized run reproduced the split: 15 of 23 native intervals at or
above 50 ms were compositor misses, five were native-only, and three had no
present inside the interval. Its largest native interval was dominated by
engine work (`tilemap_predraw_calc` p99 62.022 ms), while the present call
remained one record per native frame.

Raw captures:

- `captures/frame-pacing/tutorial-zoom-out-in-swappy-10s-dxvk1-half-refresh-run01-20260912-155904`
- `captures/frame-pacing/tutorial-zoom-out-in-swappy-10s-dxvk1-half-refresh-run02-20260912-161321`
- `captures/frame-pacing/tutorial-zoom-out-in-swappy-10s-compositor-run01.csv`
- `captures/frame-pacing/tutorial-zoom-out-in-swappy-10s-compositor-run02.csv`
- `captures/frame-pacing/swappy-cadence-next-dxvk1-half-refresh-run03-20260912-183017`
- `captures/frame-pacing/surfaceflinger-swappy-cadence-run03.csv`

The Swappy automatic interval/pipeline A/B was rejected. Its result and raw
captures are preserved with the other D30 rejected optimizations in
[dxvk1-d60-performance-benchmark.md](dxvk1-d60-performance-benchmark.md),
where it is tracked as a possible D60 Performance-mode candidate. The fixed
two-refresh configuration remains the D30 control.

### Zooming camera baseline

The zooming baseline is intentionally a separate workload from panning. Record
two fresh runs per renderer before optimization. Use native `frame-timing.csv`
for cadence and `frame-work.csv` to quantify the zoom-triggered engine work.
The controlled sequence is 1.5 seconds zooming out, 1.5 seconds zooming in,
repeated seven times (21 seconds total). Earlier 3-second/four-cycle and
2-second/five-cycle captures are superseded and must not be entered in this
table.

| Renderer / run | Frames | P95 ms | P99 ms | Max ms | >=37.5 ms | >=50 ms | Status |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| DXVK 1 / 1 | 629 | 42.691 | 79.988 | 96.795 | 38 | 17 | Baseline captured |
| DXVK 1 / 2 | 629 | 37.716 | 52.317 | 98.958 | 33 | 15 | Baseline captured |
| Sokol / 1 | 329 | 79.419 | 80.607 | 82.864 | 290 | 272 | Baseline captured |
| Sokol / 2 | 299 | 80.032 | 82.896 | 84.408 | 273 | 258 | Baseline captured |

### DXVK zoom cache result

After the baseline, DXVK was rerun with the bounded, terrain-update-aware
alternate-LOD cache enabled. The cache uses shared tilemap resources and is
enabled for both renderers by the Android launch default
`zoom_lod_cache=1`; use `zoom_lod_cache=0` for an explicit control
run. Sokol-specific tuning was not added.

| Renderer / run | Frames | P95 ms | P99 ms | Max ms | >=37.5 ms | >=50 ms | Cache hits | Status |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| DXVK 1 / 1 | 629 | 35.192 | 56.396 | 76.729 | 26 | 12 | 10,157 | Adopted |
| DXVK 1 / 2 | 629 | 34.301 | 50.805 | 72.938 | 17 | 7 | 10,735 | Adopted |

The cache-run raw captures are:

- `captures/frame-pacing/tutorial-zoom-out-in-cache-experiment-dxvk1-dxvk1-half-refresh-run01-20260912-133746`
- `captures/frame-pacing/tutorial-zoom-out-in-cache-experiment-dxvk1-dxvk1-half-refresh-run02-20260912-134038`

Compared with the corresponding baseline rows, the cache reduced the worst
P99 from 79.988 to 56.396 ms and reduced the second run's >=50 ms intervals
from 15 to 7. The result is retained as the current shared zoom optimization;
Sokol remains a regression/reference capture.

### D30 optimization disposition

The cache-restore seam, budgeted/deferred LOD, cached-texture, and cached
point/region experiments were rejected in the D30 matrix. Their complete A/B
results and raw capture paths now live in
[dxvk1-d60-performance-benchmark.md](dxvk1-d60-performance-benchmark.md),
where they are tracked as possible D60 Performance-mode candidates.

The full budgeted/deferred LOD and cached-texture A/B results and raw capture
paths are preserved in
[dxvk1-d60-performance-benchmark.md](dxvk1-d60-performance-benchmark.md) as
possible D60 Performance-mode candidates.

The DXVK zoom baseline is already materially worse than the moving-camera
baseline. The corresponding `frame-work.csv` p99 values for runs 1 / 2 were:
`scene_tilemap_predraw` 39.991 / 36.560 ms, `tilemap_predraw_calc` 63.796 /
37.308 ms, `tilemap_border_rebuild` 61.807 / 35.769 ms, and
`bump_tile_calc_total` 71.428 / 36.162 ms. This is the pre-optimization
reference; the run-to-run spread is retained as part of the baseline rather
than averaged away.

The Sokol zoom runs were slower still: P99 was 80.607 / 82.896 ms, with
258 / 272 intervals at >=50 ms. Their `tilemap_predraw_calc` p99 was 27.739 /
28.386 ms and `bump_tile_calc_total` p99 was 26.302 / 27.230 ms; Sokol also
spent 18.557 / 21.806 ms p99 in `tilemap_draw_bump`. This confirms that the
zoom workload has a shared engine preparation cost, with renderer-specific
draw/presentation costs determining how severely it misses D30.

Keep the raw capture directories. Compare zooming against the same renderer's
static and moving runs; do not combine the three workloads into one aggregate.

### Current conclusion

Static cadence is clean for both renderers. Camera motion affects both paths,
which confirms an engine-side workload component. DXVK retains larger rare
tails: both moving DXVK windows contain >=50 ms frames, while neither matched
Sokol window does. Neither final DXVK window compiled pipelines.

The original DXVK zooming baseline was substantially worse than the moving
case, with P99 values of 79.988 and 52.317 ms. The shared cache remains the
adopted zoom optimization. The removed seam-restore, budgeted-build,
cached-texture, and cached point/region experiments are tracked as possible D60
Performance-mode candidates in
[dxvk1-d60-performance-benchmark.md](dxvk1-d60-performance-benchmark.md).
Sokol zooming remains a reference at 80.607 / 82.896 ms p99.

### Diagnostic CSV flush A/B

The native benchmark files are separate from ordinary engine logging. The
current launcher runs with `console=1`, which routes ordinary logs to Android
Logcat; the frame and work CSVs are written directly by the timing
instrumentation. The former instrumentation flushed `frame-timing.csv` every
30 frames and could flush `frame-work.csv` at the same boundary. That was a
plausible source of periodic render-thread stalls, so the control was changed
to use 16 MiB buffers and one explicit flush after the 21-second capture.
The capture harness records the zoom motion's native start/end event, so this
control does not depend on `/proc/uptime` (which includes suspended device
time while the native clock does not).

| Variant / run | Native frames | P95 ms | P99 ms | Max ms | >=50 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| No mid-run CSV flush / 1 | 605 | 39.674 | 75.584 | 290.710 | 18 |
| No mid-run CSV flush / 2 | 617 | 40.641 | 63.347 | 95.676 | 15 |
| Pair average | 611 | 40.158 | 69.466 | 193.193 | 16.5 |
| Earlier shared-cache control / 1 | 629 | 40.328 | 61.871 | 86.309 | 16 |
| Earlier shared-cache control / 2 | 629 | 43.147 | 64.823 | 111.891 | 21 |

The strict no-flush pair did not improve the tail: average P95 was slightly
lower, but P99 and maximum were higher because of ordinary run-to-run
variation. Its outlier reports still contain both long tilemap-rebuild
clusters and isolated presentation-only waits; run 35 included a 271.126 ms
present wait. Logging flush is therefore not the primary explanation for the
spikes. Keep the no-periodic-flush diagnostic mode so instrumentation does not
intentionally add a known periodic stall, but prioritize terrain rebuild cost
and compositor/presentation scheduling for optimization.

Strict no-flush captures:

- `captures/frame-pacing/zoom-no-log-flush-strict-dxvk1-half-refresh-run35-20260912-223235`
- `captures/frame-pacing/zoom-no-log-flush-strict-dxvk1-half-refresh-run36-20260912-223518`

### Power-mode qualification

Huawei system Performance mode was disabled for every D30 result and diagnostic
recorded in this document. Performance mode was not used as a D30 control; the
new D60 benchmark requires it to be enabled and records that matrix separately
in [dxvk1-d60-performance-benchmark.md](dxvk1-d60-performance-benchmark.md).

The Android sustained-performance request is diagnostics-only and unsupported
on this Huawei SCM-W09 / Android 10 device, so it is not a substitute for the
manual Performance-mode setting required by the D60 benchmark.

### DXVK single-present D30 decision

The retained 60-second D30 diagnostic pair compared the ordinary interval-2
path with `DXVK single-present D30` under the same 75% DXVK/half-refresh setup.
Single-present reduced the moving-camera tail. The retained DXVK event-row
count also fell to one `queue_present` record per native frame:

| Variant | Run | P95 ms | P99 ms | Max ms | >=50 ms | `queue_present` / native frame |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Single-present off | 1 | 42.599 | 64.866 | 102.448 | 50 | 2.00 |
| Single-present off | 2 | 46.164 | 51.203 | 97.758 | 43 | 2.00 |
| Single-present on | 1 | 34.182 | 37.360 | 73.421 | 5 | 1.00 |
| Single-present on | 2 | 34.025 | 36.479 | 63.519 | 2 | 1.00 |

The event-row counts above come from the pre-correction diagnostic traces; the
later corrected SwappyVk traces separately verified one `queue_present` call
per native frame. Their duplicate-row residue must not be interpreted as two
actual present calls.

The enabled variant is therefore the automatic DXVK behavior whenever the
Android half-refresh limit is enabled. Full-refresh launches omit the D30
single-present argument. The former toggle has been removed; stale saved
values are ignored and are not migrated.

Raw captures:

- Off: `captures/frame-pacing/tutorial-dxvk-wsi-fresh-dxvk1-half-refresh-run01-20260909-102818` and `...run02-20260909-103011`
- On: `captures/frame-pacing/tutorial-dxvk-single-present-d30-dxvk1-half-refresh-run01-20260909-201556` and `...run02-20260909-201746`

## Historical diagnostic evidence

Older 60-second captures remain retained as diagnostic evidence. They used an
earlier motion procedure and must not replace the current controlled matrix.

| Study | Result | Decision |
| --- | --- | --- |
| Initial D30 vs S30 | DXVK interval P99 53.216 / 61.914 ms; Sokol 36.736 / 36.859 ms | Established DXVK tail problem |
| DXVK WSI trace | `vkQueuePresentKHR` P99 27.468 / 26.642 ms; acquisition P99 about 16 ms | Present path is variable; acquisition is comparatively stable |
| Complete blocking trace | Present-submission wait P99 24.602 / 28.575 ms; explicit latency wait about 0.005 ms | Queue/present wait, not D3D9 latency throttle, blocks the game thread |
| Warm pipeline correlation | 95 / 116 long frames, zero pipeline compilations | Steady-state stutter is not runtime pipeline compilation |
| Single-present interval-2 diagnostic | Engine P99 37.360 / 36.479 ms; <1% >=37.5 ms | Duplicate present is a major engine-cadence defect |
| SurfaceFlinger single-present trace | Actual-present P99 about 50 ms with repeated 16.7/50 ms phase correction | CPU limiter is not display-phase locked |

### DXVK WSI-stage reference

These values are milliseconds from retained fresh D30 WSI captures.

| Stage | Run 1 P50 / P95 / P99 / Max | Run 2 P50 / P95 / P99 / Max |
| --- | --- | --- |
| `vkQueuePresentKHR` | 2.773 / 15.234 / 27.468 / 78.703 | 2.918 / 15.398 / 26.642 / 44.780 |
| Next-image acquisition | 11.095 / 15.188 / 15.966 / 26.495 | 11.165 / 15.228 / 16.070 / 27.890 |
| DXVK FPS limiter | 0.002 / 0.002 / 0.007 / 0.407 | 0.001 / 0.002 / 0.007 / 1.155 |
| Queue submission | 0.159 / 0.538 / 1.302 / 8.102 | 0.160 / 0.572 / 1.482 / 18.034 |
| Present-submission wait | 12.104 / 17.336 / 24.602 / 64.493 | 11.956 / 18.019 / 28.575 / 64.721 |
| Frame-latency wait | 0.001 / 0.001 / 0.005 / 0.019 | 0.001 / 0.002 / 0.005 / 0.016 |

## Interpretation boundaries

- Native frame intervals measure engine cadence, not what the display showed.
- SurfaceFlinger latency and a future Perfetto/FrameTimeline trace are needed
  to decide whether a native outlier becomes a visible present miss.
- Cold-cache loading behavior remains separate from the steady-state workload.
- D60 / interval-1 is deferred from this historical matrix; its Performance-mode
  test plan is [dxvk1-d60-performance-benchmark.md](dxvk1-d60-performance-benchmark.md).

## Next measurement

Profile the remaining zoom CPU tails, especially `tilemap_border_rebuild` and
the associated bump-tile calculations, before considering another DXVK-only
presentation change. Do not add another low-risk terrain cache without a
specific attribution hypothesis.
