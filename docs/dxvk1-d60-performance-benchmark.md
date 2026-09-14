# DXVK 1 D60 Performance-Mode Benchmark

## Purpose and status

This document defines the next benchmark matrix for DXVK 1 at the Huawei
SCM-W09's full 60 Hz rate (D60 / interval-1) while the device's system
**Performance** mode is enabled.

No D60 Performance-mode results have been recorded yet. The existing D30
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
| Warm-up | 10 seconds unless a study records another value |
| Measurement window | 20 seconds static/moving; 21 seconds zooming |
| Runs | Two control runs and two candidate runs per scenario |

Before every session, record a screenshot or device-status note showing that
Huawei Performance mode is enabled. Do not mix a Performance-mode run with the
non-Performance D30 corpus. Android's `setSustainedPerformanceMode` request is
not a substitute: this device reports that sustained performance is unsupported.

## Workload and launch protocol

Use the single-player Campaign mission **Tutorial** and the established launch
sequence from the D30 benchmark:

1. Restart `ContentActivity`.
2. Wait two seconds for the launcher, tap **Play**, then wait 17 seconds for the
   game's landscape main menu.
3. Start Tutorial and wait until the normal gameplay HUD is visible.
4. Use three-second menu transitions and the established mission-load wait.
5. Capture static runs first, then restart Tutorial before every moving and
   zooming run.

For launcher-option studies, keep all other launcher settings fixed and test
each option independently against the same fresh control. Record whether
**Zoom LOD cache** is enabled and whether **DXVK maximum frame latency** is set
to **1 frame**. Do not combine the two options in the first A/B; if either
shows a repeatable gain, run a separate combined follow-up.

| Scenario | Procedure | Fresh Tutorial |
| --- | --- | --- |
| Static | Leave the initial camera unchanged | No |
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
| DXVK 1 / static / 1 |  |  |  |  |  |  |  |  | Planned |
| DXVK 1 / static / 2 |  |  |  |  |  |  |  |  | Planned |
| DXVK 1 / moving / 1 |  |  |  |  |  |  |  |  | Planned |
| DXVK 1 / moving / 2 |  |  |  |  |  |  |  |  | Planned |
| DXVK 1 / zooming / 1 |  |  |  |  |  |  |  |  | Planned |
| DXVK 1 / zooming / 2 |  |  |  |  |  |  |  |  | Planned |
## Candidate evaluation rules

For every candidate, retain both raw capture directories and a short decision
record. A candidate is retained only when it improves the relevant D60 tail
metrics against both fresh control runs, does not regress static or moving
workloads, and passes visual terrain/seam validation. Record compositor misses
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

| Candidate | Control captures | Candidate captures | D60 result | Decision |
| --- | --- | --- | --- | --- |
| Pending LOD transaction |  |  |  | Pending |
| Panning alternate-LOD cache |  |  |  | Pending |
| Four replacements per frame |  |  |  | Pending |
| 8% LOD hysteresis |  |  |  | Pending |
| Launcher DXVK maximum frame latency = 1 frame |  |  |  | Pending |
| Launcher Zoom LOD cache |  |  |  | Pending |
| Swappy automatic interval/pipeline |  |  |  | Pending |
| Budgeted/deferred LOD builds |  |  |  | Pending |
| Cache-restore seam skipping |  |  |  | Pending |
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
| Regression | Two clean static, moving, and zooming runs, with raw traces retained |

## First run order

1. Capture the six DXVK D60 control rows.
2. Correlate the DXVK zoom and moving controls with SurfaceFlinger.
3. Test the launcher **DXVK maximum frame latency = 1 frame** option as the
   least invasive presentation candidate.
4. Test the launcher **Zoom LOD cache** option independently.
5. Test one terrain candidate at a time, beginning with the candidate that most
   directly reduces restored-variant rebuild work.
