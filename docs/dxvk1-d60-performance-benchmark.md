# DXVK 1 D60 Performance-Mode Benchmark

## Purpose and status

This document defines the next benchmark matrix for DXVK 1 at the Huawei
SCM-W09's full 60 Hz rate (D60 / interval-1) while the device's system
**Performance** mode is enabled.

The first D60 Performance-mode control results are now recorded below. The existing D30
results in [dxvk1-frame-pacing-benchmark.md](dxvk1-frame-pacing-benchmark.md)
and [dxvk1-frame-pacing-plan.md](dxvk1-frame-pacing-plan.md) were captured with
Huawei Performance mode disabled, in the normal/non-Performance device mode.
They are historical D30 evidence, not D60 controls and not Performance-mode
results.

The current accepted Android stack remains the control: the terrain-revision-aware
shared alternate-LOD cache, corrected one-present-call SwappyVk instrumentation,
and the fixed two-refresh D30 configuration where applicable. Candidate changes
must be tested one at a time against a fresh D60 control.

## Fixed D60 conditions

| Field | Value |
| --- | --- |
| APK | `debug` build (`app-debug.apk`), runtime DXVK 1 |
| Device | Huawei SCM-W09, Android 10, 60 Hz |
| System power mode | Huawei **Performance** mode enabled manually before each session |
| Graphics profile | Max |
| Resolution scale | 50% |
| Launcher options | Record the baseline values; test **Zoom LOD cache** and **DXVK maximum frame latency** set to **1 frame** as separate candidates |
| Target | Full refresh / D60 / interval-1 |
| Sound | Enabled |
| Native timing | `frame_timing=1` |
| Work-log mode | Compact |
| Warm-up | 10 seconds, followed by 3 seconds of high CPU load across all available device CPUs, a 2-second post-load settling wait, and camera motion |
| Measurement window | 20 seconds static/moving; 21 seconds zooming |
| Runs | Four moving control runs for variance, two zooming control runs, and two candidate runs per moving/zooming scenario; static is baseline-only |

Before every session, record a screenshot or device-status note showing that
Huawei Performance mode is enabled. Do not mix a Performance-mode run with the
non-Performance D30 corpus. Android's `setSustainedPerformanceMode` request is
not a substitute: this device reports that sustained performance is unsupported.

## Workload and launch protocol

Use the single-player Campaign mission **Tutorial** and the established launch
sequence from the D30 benchmark:

1. Restart `ContentActivity`.
2. Wait two seconds for the launcher, tap **Play**, then wait 7 seconds for the
   game's landscape main menu.
3. Start Tutorial and wait until the normal gameplay HUD is visible.
4. Use two-second menu transitions and the established mission-load wait.
5. Treat the existing static runs as baseline evidence only; restart Tutorial
   before every new moving and zooming run.
6. The post-load settling wait is 2 seconds so the all-core load pulse does not
   immediately influence the camera window; the established 1-second
   post-capture flush remains unchanged. The earlier 250 ms-gap captures remain
   valid historical results and are not mixed into the new-gap comparison.

For launcher-option studies, keep all other launcher settings fixed and test
each option independently against the same fresh control. Record whether
**Zoom LOD cache** is enabled and whether **DXVK maximum frame latency** is set
to **1 frame**. Do not combine the two options in the first A/B; if either
shows a repeatable gain, run a separate combined follow-up.

| Scenario | Procedure | Fresh Tutorial |
| --- | --- | --- |
| Static | Existing baseline evidence only; no new static-camera runs | No |
| Moving | Hold Right, Up, Left, Down for one second each; repeat five times | Yes |
| Zooming | Zoom out 1.5 seconds, zoom in 1.5 seconds; repeat seven times | Yes |

Run DXVK 1 for all scenarios. Use native `frame-timing.csv` for
engine cadence, `frame-work.csv` for stage attribution, the corrected DXVK
trace for renderer timing, and synchronized SurfaceFlinger sampling for
display-cadence correlation.

Frame timing always uses the compact work log. The compact CSV keeps the
terrain/LOD and D3D submit timing events, and records `bump_tile_calc_count`
plus `tilemap_lod_cache_hit_count` once per frame in the `value` column. Broad
scene/camera events and low-value visibility bookkeeping are not recorded.

## D60 control matrix

Fill this table only with runs made under the fixed conditions above. D60's
nominal display interval is 16.667 ms.

| Renderer / scenario / run | Frames | P50 ms | P95 ms | P99 ms | Max ms | >=20.833 ms | >=33.333 ms | >=50 ms | Status |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| DXVK 1 / static / 1 | 1273 | 16.647 | 17.092 | 17.574 | 35.316 | 9 | 1 | 0 | Baseline-only; pre-load protocol |
| DXVK 1 / static / 2 | 1273 | 16.658 | 17.084 | 17.543 | 37.180 | 4 | 3 | 0 | Baseline-only; pre-load protocol |
| DXVK 1 / moving / 1 | 913 | 19.686 | 35.182 | 42.220 | 47.398 | 383 | 83 | 0 | Captured with finalized short-gap protocol |
| DXVK 1 / moving / 2 | 1008 | 18.555 | 32.071 | 36.330 | 43.215 | 276 | 31 | 0 | Captured with finalized short-gap protocol |
| DXVK 1 / moving / 3 | 982 | 18.489 | 33.268 | 42.462 | 53.718 | 298 | 47 | 1 | Captured with finalized short-gap protocol |
| DXVK 1 / moving / 4 | 927 | 19.428 | 33.771 | 41.644 | 49.705 | 373 | 71 | 0 | Captured with finalized short-gap protocol |
| DXVK 1 / zooming / 1 | 922 | 21.848 | 36.041 | 49.564 | 82.114 | 523 | 68 | 8 | Captured with finalized short-gap protocol |
| DXVK 1 / zooming / 2 | 927 | 21.645 | 33.792 | 43.674 | 72.272 | 516 | 65 | 2 | Captured with finalized short-gap protocol |

The control raw captures are:

- `d60-performance-control-static-dxvk1-full-refresh-run01-20260917-215349`
- `d60-performance-control-static-dxvk1-full-refresh-run02-20260917-215454`
- `d60-performance-control-short-gap-moving-dxvk1-full-refresh-run01-20260918-002756`
- `d60-performance-control-short-gap-moving-dxvk1-full-refresh-run02-20260918-003100`
- `d60-performance-control-short-gap-moving-dxvk1-full-refresh-run03-20260918-004358`
- `d60-performance-control-short-gap-moving-dxvk1-full-refresh-run04-20260918-004650`
- `d60-performance-control-short-gap-zooming-dxvk1-full-refresh-run01-20260918-003351`
- `d60-performance-control-short-gap-zooming-dxvk1-full-refresh-run02-20260918-003651`

The static rows are the earlier baseline-only captures and do not include the
new load/frequency phase. The native timing analyzer reports one
`queue_present` event per native frame in all four new moving/zooming control
windows. The results above are derived from native frame-start intervals inside
each capture's recorded measurement window.

The four-run moving spread is P95 32.071-35.182 ms, P99 36.330-42.462 ms,
and maximum 43.215-53.718 ms. Moving candidate comparisons should retain all
four controls and use this distribution rather than selecting only the best
pair.

### Historical CPU-frequency observations

The historical control captures include `cpu-frequency.csv` for all eight
cores. The active capture harness no longer produces this telemetry. In the
historical data, the 3-second all-core pulse visibly raises the sampled operating points: the
typical load plateau is about 1.421 GHz on cores 0-3, 1.805 GHz on cores 4-5,
and 2.600 GHz on cores 6-7. After the pulse, the little cores commonly return
to 0.830 GHz within roughly one to two samples while the larger cores remain at
higher, varying points. This confirms that the pulse triggers frequency
scaling, but it does not prove that all cores remain boosted for the camera
window; the frequency logs must be considered alongside each run's tail
metrics.

The raw frequency logs are retained inside the four finalized control
directories listed above. Their observed sample intervals remain about 0.42
seconds at the median, not the nominal 250 ms, due to device-shell sysfs-read
overhead.

The shortened-gap diagnostics were then checked separately: moving run 3
(`d60-performance-control-cpu-load-short-gap-moving-dxvk1-full-refresh-run03-20260918-001805`)
measured 33.079 ms P95, 38.711 ms P99, and 50.307 ms max; zooming run 3
(`d60-performance-control-cpu-load-short-gap-zooming-dxvk1-full-refresh-run03-20260918-002135`)
measured 36.776 ms P95, 46.808 ms P99, and 71.731 ms max. Both showed
elevated big-core frequencies at the measurement boundary. These preceded the
finalized two-run control pair and are retained as ancillary diagnostics.

### Previous D60 control SurfaceFlinger correlation

The earlier synchronized passive SurfaceFlinger samples were collected before
the CPU-load/frequency phase was added. The moving correlation capture recorded 277
native intervals at or above 20.833 ms: 235 compositor misses, 41 native-only
intervals, and one interval without an in-window present. The zooming
correlation recorded 345 such intervals: 258 compositor misses, 85
native-only intervals, and two intervals without an in-window present. The
largest correlated SurfaceFlinger gap was 66.667 ms for both workloads.

Raw correlation evidence:

- Moving capture: `d60-performance-correlation-moving-dxvk1-full-refresh-run02-20260917-221617`
- Moving SurfaceFlinger sample: `surfaceflinger-d60-performance-correlation-moving2.csv`
- Zooming capture: `d60-performance-correlation-zooming-dxvk1-full-refresh-run03-20260917-221930`
- Zooming SurfaceFlinger sample: `surfaceflinger-d60-performance-correlation-zooming.csv`

The operator confirmed Huawei Performance mode was enabled manually for the
2026-09-17 D60 control and correlation session; the confirmation is recorded
in `d60-performance-mode-note-20260917.md`.

Fresh synchronized SurfaceFlinger correlation for the CPU-load controls remains
pending; the native/frequency control evidence above is not a substitute for
that display-cadence check.

## Candidate evaluation rules

For every candidate, retain both raw capture directories and a short decision
record. A candidate is retained only when it improves the relevant D60 tail
metrics against all fresh controls (four moving runs and two zooming runs), does not regress moving
workloads, and passes visual terrain/seam validation. Static remains a baseline
reference and is not repeated for launcher-option studies. Record compositor misses
separately from native-only outliers; a lower native interval alone is not
enough if the display cadence worsens.

Prioritize candidates that reduce the number and cost of
`bump_tile_calc_count`, `tilemap_predraw_calc`, and `tilemap_border_rebuild`
operations during zoom. Do not combine two candidates in the first A/B.

## Candidates carried forward from rejected D30 experiments

These changes were rejected by the non-Performance D30 investigation. They are
not accepted implementations. D60 Performance mode is a new experiment because
the full-refresh target changes both the frame budget and the presentation
pressure; each candidate needs a fresh control before being rejected again.

| Candidate | Previous non-Performance D30 result | D60 question |
| --- | --- | --- |
| Pending LOD transaction | 57 rebuild calls p99, 81 max, and a 74.98 ms frame; extra lifetime/transition work increased bursts | Does the larger CPU budget make transaction publication useful at interval-1? |
| Earlier panning alternate-LOD cache | No clear repeatable gain over texture reuse; distinct from the current accepted terrain-revision-aware zoom cache | Does retaining panning variants reduce D60 motion tails without excessive memory? |
| Four replacements per frame | Repeat measured 49.321 ms p99, 74.404 ms max, and 27 long frames | Does a smaller D60 frame budget benefit from a tighter replacement quota? |
| 8% LOD hysteresis | Fewer LOD calls but more long frames; visual LOD policy changed without enough benefit | Does interval-1 make the visual/latency trade-off acceptable? |
| Launcher **DXVK maximum frame latency = 1 frame** (`d3d9.maxFrameLatency=1`) | D30 p99 was 62.772 / 56.102 ms versus default 51.125 / 56.136 ms | Does full-refresh pacing change the queue-depth result? |
| Launcher **Zoom LOD cache** | Not yet tested under D60 Performance mode | Does the launcher cache option improve zoom tails or alter terrain/LOD work? |
| Swappy automatic interval/pipeline mode | Native 50.910 / 79.133 / 228.391 ms p95/p99/max, 77 >=50 ms intervals, and 74 compositor misses | Can automatic interval selection phase-lock D60, or does fixed pacing remain better? |
| Budgeted/deferred LOD builds | Budget 8 raised paired p99/max from 60.288/84.864 ms to 69.579/113.973 ms despite a lower P95 | Does D60 tolerate or benefit from spreading rebuild work? |
| Cache-restore seam skipping | Reduced bump-tile calls about 15%, but no-sampler P95/P99/max increased on average | Does the D60 budget expose a benefit from skipping restored-variant recalculation? |
| Cached texture reuse on restored variants | Pair average P95/P99/max was 42.372/64.861/109.129 ms versus cache-only 41.738/63.347/99.100 ms | Does D60 make texture-page reuse worthwhile when geometry is still rebuilt? |
| Cached point/region reuse on restored variants | Pair average changed from 41.738/63.347/99.100 ms to 39.541/63.848/148.872 ms; P95 improved but P99/max worsened | Can the optimization be constrained to avoid the long-tail regression? |

### Candidate test log

The launcher and terrain candidate rows below were captured before the
CPU-load/frequency phase was added. Their raw traces remain useful historical
evidence, but final candidate decisions require recaptures against the new
moving/zooming controls above.

| Candidate | Control captures | Candidate captures | D60 result | Decision |
| --- | --- | --- | --- | --- |
| Pending LOD transaction |  |  |  | Pending |
| Panning alternate-LOD cache |  |  |  | Pending |
| Four replacements per frame |  |  |  | Pending |
| 8% LOD hysteresis |  |  |  | Pending |
| Launcher DXVK maximum frame latency = 1 frame | `d60-performance-control-{static,moving,zooming}-dxvk1-full-refresh-run0{1,2}` | `d60-performance-candidate-latency1-{static,moving,zooming}-dxvk1-full-refresh-run0{1,2}` | Zoom P99 45.415 / 35.170 ms versus control 40.806 / 48.290 ms; static P95 17.208 / 17.256 ms versus 17.092 / 17.084 ms; moving max 178.094 ms in run 1 | Rejected |
| Launcher Zoom LOD cache | `d60-performance-cacheoff-{moving,zooming}-dxvk1-full-refresh-run0{1,2}` | `d60-performance-cacheon-{moving,zooming}-dxvk1-full-refresh-run0{1,2}` | Moving P95 23.514 / 23.643 ms versus 23.745 / 33.755 ms, but P99 31.724 / 30.226 versus 25.616 / 37.227; zoom P95 26.316 / 26.669 versus 25.623 / 25.459, P99 32.554 / 34.831 versus 28.519 / 27.799, and max 69.585 / 67.607 versus 40.873 / 42.620 | Rejected |
| Swappy automatic interval/pipeline |  |  |  | Pending |
| Budgeted/deferred LOD builds |  |  |  | Pending |
| Cache-restore seam skipping | `d60-performance-cacheon-{moving,zooming}-dxvk1-full-refresh-run0{1,2}` | `d60-performance-candidate-seam-skip-moving-dxvk1-full-refresh-run01-20260917-232050`, `d60-performance-candidate-seam-skip-moving-dxvk1-full-refresh-run02-20260917-232342`, `d60-performance-candidate-seam-skip-zooming-dxvk1-full-refresh-run01-20260917-232639`, `d60-performance-candidate-seam-skip-zooming-dxvk1-full-refresh-run02-20260917-233304` | Moving P95 20.145 / 32.864 ms versus 23.514 / 23.643 ms and P99 24.582 / 35.432 versus 31.724 / 30.226; zoom P95 24.428 / 24.274 versus 26.316 / 26.669 and P99 31.724 / 31.223 versus 32.554 / 34.831, but moving run 2 regressed and zoom max rose to 84.159 / 78.433 ms versus 69.585 / 67.607 | Rejected |
| Cached texture reuse |  |  |  | Pending |
| Cached point/region reuse |  |  |  | Pending |

## Previous raw evidence to reuse for comparison

The source D30 captures remain under `captures/frame-pacing/`. The most relevant
rejected-candidate groups are:

- `swappy-auto-interval-dxvk1-half-refresh-run04-20260912-184356`
- `zoom-cache-restore-nosampler-dxvk1-half-refresh-run13-20260912-200719`
  through `zoom-seam-restore-nosampler-dxvk1-half-refresh-run16-20260912-201650`
- `zoom-lod-budget-ab0-dxvk1-half-refresh-run07-20260912-193028` through
  `zoom-cache-budget8-nosampler-dxvk1-half-refresh-run18-20260912-202950`
- `zoom-cache-texture-reuse-candidate-dxvk1-half-refresh-run19-20260912-204703`
  and run 20
- `zoom-cached-point-reuse-clean-corrected-delay-dxvk1-half-refresh-run29-20260912-215722`
  and run 30

Those captures are useful for implementation context only. They must not be
used as D60 Performance-mode controls.

## D60 completion gates

Before promoting a D60 candidate:

| Gate | Required evidence |
| --- | --- |
| Mode | Huawei Performance mode confirmed for every control and candidate run |
| Cadence | Native P95/P99/max and long-frame counts improve or remain within the agreed control variance |
| Presentation | One `queue_present` per native frame and no new recurring SurfaceFlinger phase correction |
| Engine | Reduced attributed terrain work, or a clearly isolated presentation benefit |
| Visuals | No terrain seams, missing geometry, ownership, or LOD-policy regression |
| Thermal | Repeat after the device reaches a stable sustained state |
| Regression | Two clean moving and zooming runs, each with the 10-second warm-up and 3-second high-CPU phase, with raw traces retained; static remains a baseline reference |

## First run order

1. Capture the moving and zooming DXVK D60 control rows; keep static as a
   baseline-only reference.
2. Correlate the CPU-load DXVK zoom and moving controls with SurfaceFlinger.
3. Test the launcher **DXVK maximum frame latency = 1 frame** option as the
   least invasive presentation candidate.
4. Test the launcher **Zoom LOD cache** option independently.
5. Test one terrain candidate at a time, beginning with the candidate that most
   directly reduces restored-variant rebuild work.
