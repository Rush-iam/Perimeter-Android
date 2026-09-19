# Tilemap Rebuild Stepping Plan

## Goal

Reduce avoidable terrain LOD rebuild work during camera movement and zooming on
the Huawei SCM-W09, while preserving immediate terrain updates, correct seams,
and the current synchronous render path. This is a workload-shaping experiment;
it does not change CPU affinity, governors, thermal policy, or renderer
selection.

The experiments use the Android 10 `releaseBenchmark` build, DXVK 1, D60/full
refresh, Huawei Performance mode, and the established Tutorial capture protocol.
Run exactly two moving and two zooming captures for every candidate. Keep each
candidate independent and compare it with the control rows below.

## What is being measured

`tilemap_predraw_calc` is the wall-clock interval around `CalcTileMap()`. It
includes visibility and LOD selection, bump-tile allocation/cache work, render
list construction, border rebuilding, and the calls to `sBumpTile::Calc()`.

`tilemap_border_rebuild` is the nested second pass that updates neighbor seam
LODs and rebuilds initialized, terrain-updated, or seam-changing tiles.

`bump_tile_calc_total` is the per-frame sum of all `sBumpTile::Calc()` durations.
It includes optional texture regeneration, point/region processing, topology,
vertex writes, and index uploads. It is a summed work metric rather than a
wall-clock interval, so it must not be added to `tilemap_predraw_calc`.

The native-work values below are durations from `frame-work.csv`, filtered to
the same frame-ID measurement windows as the frame statistics. All values are
milliseconds and are reported as P95/P99/maximum.

## Reduced two-run control matrix

This is the two-moving/two-zooming subset copied from the D60 benchmark format.
The existing D60 document retains additional moving controls for its separate
benchmark purpose; this plan intentionally uses two runs per workload.

The three final columns report `bump_tile_calc_total`, `tilemap_predraw_calc`,
and `tilemap_border_rebuild`; each metric is P95/P99/max.

| Scenario / run | Frames | Frame ms (P50/P95/P99/max) | Long frames (>=20.833 / >=33.333 / >=50) | Bump tile ms (P95/P99/max) | Predraw ms (P95/P99/max) | Border rebuild ms (P95/P99/max) | Status |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| DXVK 1 / moving / 1 | 2870 | 18.642 / 33.883 / 41.111 / 51.703 | 1083 / 226 / 4 | 1.015 / 1.331 / 47.848 | 1.296 / 1.618 / 48.120 | 1.060 / 1.378 / 47.904 | Control |
| DXVK 1 / moving / 2 | 2981 | 18.395 / 33.362 / 39.954 / 48.207 | 915 / 154 / 0 | 1.014 / 1.439 / 26.791 | 1.275 / 1.723 / 27.104 | 1.050 / 1.492 / 26.859 | Control |
| DXVK 1 / zooming / 1 | 3117 | 19.906 / 28.186 / 35.292 / 97.716 | 1281 / 43 / 5 | 2.293 / 3.477 / 57.531 | 2.627 / 3.826 / 57.890 | 2.360 / 3.542 / 57.616 | Control |
| DXVK 1 / zooming / 2 | 2690 | 22.218 / 40.021 / 46.504 / 70.418 | 1497 / 368 / 7 | 2.746 / 4.029 / 47.119 | 3.093 / 4.401 / 47.357 | 2.821 / 4.093 / 47.203 | Control |

Raw controls:

- `huawei-cpu-consistency-control-moving-dxvk1-full-refresh-run01-20260919-001223`
- `huawei-cpu-consistency-control-moving-dxvk1-full-refresh-run02-20260919-001643`
- `huawei-cpu-consistency-control-zooming-dxvk1-full-refresh-run01-20260919-002028`
- `huawei-cpu-consistency-control-zooming-dxvk1-full-refresh-run02-20260919-002611`

## Safest options, in order

The current policy rebuilds a tile when it is new, terrain-updated, or affected
by a neighboring LOD change. Small camera movement inside the same visibility
and LOD bands normally does not call `Calc()`. Zooming and threshold crossings
are the main sources of bursts.

1. **LOD hysteresis - first candidate.** Add separate enter and leave bands
   around each `DistLevelDetail` threshold. A tile near a boundary retains its
   current LOD until the camera crosses the wider transition band. This is the
   smallest behavior change and should reduce repeated seam and bump-tile work
   without delaying a confirmed transition. Start with an 8% band, exposed as a
   optimization experiment option that is off by default. An older D30 hysteresis test
   is historical evidence only; recapture it against the control above.

2. **Quantized LOD decision.** If hysteresis is insufficient, quantize the
   camera-distance input used for the discrete LOD decision. Keep visibility and
   terrain updates immediate; only the LOD decision becomes stepped. Validate
   seams and camera reversals, because this can make transitions more visible.

3. **Bounded rebuild queue.** As a last resort, queue confirmed LOD rebuilds and
   process a small number per frame, prioritizing visible tiles and terrain
   revisions. This may leave old geometry or seams visible temporarily and has
   the highest correctness risk.

The existing `zoom_lod_cache=1` setting remains unchanged in every comparison.
It can reduce texture/resource churn, but the current restore path still marks a
restored tile uninitialized and calls `Calc()` again; it is not a stepped-build
policy by itself.

Do not combine hysteresis or a rebuild budget in a production candidate. The
quantization experiment below is historical and its implementation has been
removed after a neutral-to-rejected result.
A candidate passes only if it reduces frame P95/P99 spread and terrain-work
tails without increasing long-frame counts, visible seams, terrain-update
latency, temperature, or Android thermal status.

## Experiment checklist

- [x] Implement the optimization-experiment 8% LOD-hysteresis option, default off.
- [x] Build/install the candidate without changing the control renderer,
  refresh, load-pulse, or affinity settings.
- [x] Capture two moving and two zooming runs.
- [x] Compare frame statistics and `bump_tile_calc_total`,
  `tilemap_border_rebuild`, and `tilemap_predraw_calc` against the control rows.
- [x] Run the isolated 8% quantization matrix; it was neutral-to-rejected and
  the implementation was removed.
- [x] Run the isolated budget-4 matrix.
- [ ] Validate terrain seams, camera reversals, terrain updates, and budget
  transition lag.
- [ ] Select a production candidate only after the visual and correctness gate.

## 8% LOD-hysteresis candidate results

The candidate was captured with `tilemap_lod_hysteresis=8`, with
`zoom_lod_cache=1` unchanged. The four captures used the same
reduced matrix and the standard 10-second warm-up, 3-second all-core pulse,
2-second settling wait, and 60/63-second measurement windows.

The three final columns report `bump_tile_calc_total`, `tilemap_predraw_calc`,
and `tilemap_border_rebuild`; each metric is P95/P99/max.

| Scenario / run | Frames | Frame ms (P50/P95/P99/max) | Long frames (>=20.833 / >=33.333 / >=50) | Bump tile ms (P95/P99/max) | Predraw ms (P95/P99/max) | Border rebuild ms (P95/P99/max) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Hysteresis / moving / 1 | 3031 | 17.596 / 33.444 / 39.970 / 51.211 | 870 / 165 / 2 | 0.894 / 1.204 / 39.079 | 1.148 / 1.488 / 39.369 | 0.916 / 1.232 / 39.131 |
| Hysteresis / moving / 2 | 3137 | 17.426 / 33.210 / 40.092 / 51.119 | 742 / 142 / 2 | 0.888 / 1.321 / 36.016 | 1.128 / 1.573 / 36.315 | 0.902 / 1.331 / 36.088 |
| Hysteresis / zooming / 1 | 2786 | 20.814 / 39.197 / 45.046 / 80.744 | 1390 / 338 / 9 | 2.363 / 3.528 / 49.940 | 2.648 / 3.547 / 50.337 | 2.362 / 3.229 / 50.027 |
| Hysteresis / zooming / 2 | 2842 | 21.232 / 35.174 / 45.948 / 98.580 | 1483 / 216 / 14 | 2.630 / 4.329 / 30.129 | 2.845 / 4.388 / 30.408 | 2.565 / 4.068 / 30.197 |

Raw candidate captures:

- `huawei-cpu-consistency-lod-hysteresis-moving-dxvk1-full-refresh-run01-20260919-124939`
- `huawei-cpu-consistency-lod-hysteresis-moving-dxvk1-full-refresh-run02-20260919-125439`
- `huawei-cpu-consistency-lod-hysteresis-zooming-dxvk1-full-refresh-run01-20260919-125807`
- `huawei-cpu-consistency-lod-hysteresis-zooming-dxvk1-full-refresh-run02-20260919-130137`

Moving runs show modestly lower frame and tilemap-work percentiles than their
control counterparts. Zooming is mixed: run 1 has more long frames than its
control, while run 2 improves the >=33.333 ms count and all tilemap-work tails
but has a higher maximum frame time. This is not a clear performance pass yet;
the candidate remains pending visual inspection for seams, transition
stability, and terrain-update latency. The quantization candidate was rejected
and removed; do not re-enable it.

## Historical 8% LOD-quantization results (removed)

The quantization candidate was captured with
`tilemap_lod_hysteresis=0` and `tilemap_lod_quantization=8`, with affinity
and `zoom_lod_cache=1` unchanged. Quantization snaps only the distance
used by the discrete LOD threshold comparison; visibility and terrain updates
remain immediate.

| Scenario / run | Frames | Frame ms (P50/P95/P99/max) | Long frames (>=20.833 / >=33.333 / >=50) | Bump tile ms (P95/P99/max) | Predraw ms (P95/P99/max) | Border rebuild ms (P95/P99/max) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Quantization / moving / 1 | 3460 | 17.374 / 23.607 / 25.184 / 58.046 | 297 / 3 / 1 | 0.867 / 1.237 / 22.268 | 1.146 / 1.509 / 22.530 | 0.910 / 1.279 / 22.318 |
| Quantization / moving / 2 | 2970 | 18.612 / 33.348 / 38.451 / 51.827 | 951 / 151 / 1 | 0.990 / 1.379 / 43.361 | 1.249 / 1.682 / 43.646 | 1.020 / 1.427 / 43.430 |
| Quantization / zooming / 1 | 2824 | 21.433 / 35.093 / 43.048 / 64.658 | 1500 / 240 / 4 | 2.792 / 4.209 / 44.067 | 3.142 / 4.571 / 44.427 | 2.848 / 4.242 / 44.151 |
| Quantization / zooming / 2 | 2755 | 21.812 / 37.192 / 44.736 / 70.466 | 1526 / 291 / 5 | 2.760 / 4.134 / 31.696 | 3.110 / 4.510 / 32.003 | 2.835 / 4.191 / 31.781 |

One zooming run reported a sustained busy downclock event. Moving run 1 is a
strong improvement, but the other runs do not establish a consistent gain:
zooming run 1 has higher P95 and long-frame counts than its control, while
zooming run 2 improves those measures but still has a similar maximum. Treat
the 8% quantization candidate as neutral-to-rejected for the consistency gate;
do not enable it by default.

Raw quantization captures:

- `huawei-cpu-consistency-lod-quantization-moving-dxvk1-full-refresh-run01-20260919-132854`
- `huawei-cpu-consistency-lod-quantization-moving-dxvk1-full-refresh-run02-20260919-133220`
- `huawei-cpu-consistency-lod-quantization-zooming-dxvk1-full-refresh-run01-20260919-133543`
- `huawei-cpu-consistency-lod-quantization-zooming-dxvk1-full-refresh-run02-20260919-133910`

## Budget-4 LOD-rebuild candidate results

The bounded candidate was captured with `tilemap_lod_hysteresis=0`,
`tilemap_lod_quantization=0`, and `tilemap_lod_rebuild_budget=4`. It limits
confirmed discrete LOD switches to four per frame when an initialized tile
already has geometry; visibility and terrain updates remain immediate. The
option is an optimization experiment and defaults to off.

| Scenario / run | Frames | Frame ms (P50/P95/P99/max) | Long frames (>=20.833 / >=33.333 / >=50) | Bump tile ms (P95/P99/max) | Predraw ms (P95/P99/max) | Border rebuild ms (P95/P99/max) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Budget 4 / moving / 1 | 3276 | 17.840 / 25.808 / 33.400 / 49.257 | 547 / 37 / 0 | 0.938 / 1.288 / 39.400 | 1.211 / 1.578 / 39.679 | 0.974 / 1.332 / 39.459 |
| Budget 4 / moving / 2 | 2934 | 18.615 / 33.469 / 39.922 / 95.124 | 985 / 177 / 2 | 1.018 / 1.286 / 41.252 | 1.294 / 1.586 / 41.520 | 1.064 / 1.329 / 41.305 |
| Budget 4 / zooming / 1 | 3044 | 20.057 / 30.594 / 42.977 / 81.119 | 1314 / 90 / 8 | 2.385 / 3.434 / 51.272 | 2.676 / 3.811 / 51.531 | 2.402 / 3.508 / 51.345 |
| Budget 4 / zooming / 2 | 3057 | 20.262 / 29.337 / 36.584 / 68.556 | 1345 / 60 / 3 | 2.536 / 3.697 / 28.972 | 2.874 / 4.066 / 29.248 | 2.586 / 3.707 / 29.033 |

The budget candidate reduced zooming P95 and >=33.333 ms counts in both runs,
and moving run 1 improved substantially. Moving run 2 remained mixed, with a
95.124 ms maximum despite only two >=50 ms frames. No sustained busy downclock
event was reported. The metrics are promising enough to require visual
validation, but the budget must not be enabled by default until seams,
transition lag, and terrain-update correctness are checked.

Raw budget captures:

- `huawei-cpu-consistency-lod-rebuild-budget-moving-dxvk1-full-refresh-run01-20260919-134726`
- `huawei-cpu-consistency-lod-rebuild-budget-moving-dxvk1-full-refresh-run02-20260919-135053`
- `huawei-cpu-consistency-lod-rebuild-budget-zooming-dxvk1-full-refresh-run01-20260919-135419`
- `huawei-cpu-consistency-lod-rebuild-budget-zooming-dxvk1-full-refresh-run02-20260919-135750`

## Combined 8%/8%/budget-4 candidate results

This explicit combination enabled all three optimization options together:
`tilemap_lod_hysteresis=8`, `tilemap_lod_quantization=8`, and
`tilemap_lod_rebuild_budget=4`. It is documented separately because the
isolated experiments were intentionally not combined.

| Scenario / run | Frames | Frame ms (P50/P95/P99/max) | Long frames (>=20.833 / >=33.333 / >=50) | Bump tile ms (P95/P99/max) | Predraw ms (P95/P99/max) | Border rebuild ms (P95/P99/max) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Combined / moving / 1 | 3149 | 17.648 / 33.003 / 39.728 / 51.274 | 702 / 128 / 2 | 0.859 / 1.125 / 23.767 | 1.105 / 1.396 / 24.129 | 0.878 / 1.157 / 23.839 |
| Combined / moving / 2 | 2902 | 18.346 / 33.870 / 41.648 / 49.691 | 1044 / 220 / 0 | 0.910 / 1.301 / 26.509 | 1.169 / 1.589 / 26.812 | 0.927 / 1.311 / 26.570 |
| Combined / zooming / 1 | 3034 | 19.972 / 33.222 / 43.037 / 72.244 | 1299 / 139 / 6 | 2.345 / 3.503 / 42.459 | 2.576 / 3.764 / 42.796 | 2.318 / 3.459 / 42.546 |
| Combined / zooming / 2 | 2723 | 21.868 / 38.986 / 45.179 / 65.740 | 1477 / 369 / 4 | 2.579 / 3.590 / 40.327 | 2.805 / 3.701 / 40.657 | 2.520 / 3.389 / 40.411 |

The combined candidate regressed against the isolated budget-4 candidate in
P95 frame time for all four runs: moving `25.808/33.469` to
`33.003/33.870`, and zooming `30.594/29.337` to `33.222/38.986` ms. Zooming
long-frame counts also regressed in both runs. Keep the combination disabled
for production; it does not provide additive benefits.

Raw combined captures:

- `huawei-cpu-consistency-lod-combined-moving-dxvk1-full-refresh-run01-20260919-140918`
- `huawei-cpu-consistency-lod-combined-moving-dxvk1-full-refresh-run02-20260919-141243`
- `huawei-cpu-consistency-lod-combined-zooming-dxvk1-full-refresh-run01-20260919-141607`
- `huawei-cpu-consistency-lod-combined-zooming-dxvk1-full-refresh-run02-20260919-141933`

## Hysteresis-plus-budget results (quantization excluded)

This follow-up enabled `tilemap_lod_hysteresis=8` and
`tilemap_lod_rebuild_budget=4`, while explicitly setting
`tilemap_lod_quantization=0`.

| Scenario / run | Frames | Frame ms (P50/P95/P99/max) | Long frames (>=20.833 / >=33.333 / >=50) | Bump tile ms (P95/P99/max) | Predraw ms (P95/P99/max) | Border rebuild ms (P95/P99/max) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Hysteresis+budget / moving / 1 | 3025 | 17.808 / 33.373 / 39.947 / 50.304 | 932 / 159 / 1 | 0.905 / 1.363 / 32.006 | 1.163 / 1.624 / 32.302 | 0.917 / 1.365 / 32.066 |
| Hysteresis+budget / moving / 2 | 3178 | 17.364 / 32.843 / 39.116 / 54.557 | 672 / 126 / 1 | 0.849 / 1.289 / 25.229 | 1.096 / 1.564 / 25.499 | 0.859 / 1.324 / 25.297 |
| Hysteresis+budget / zooming / 1 | 3054 | 20.016 / 31.591 / 39.254 / 75.999 | 1334 / 95 / 2 | 2.424 / 3.611 / 36.851 | 2.638 / 3.813 / 37.211 | 2.355 / 3.489 / 36.932 |
| Hysteresis+budget / zooming / 2 | 2969 | 20.370 / 33.342 / 40.690 / 64.874 | 1396 / 149 / 4 | 2.363 / 3.438 / 52.601 | 2.610 / 3.675 / 52.944 | 2.321 / 3.335 / 52.674 |

Excluding quantization improved the combined result, especially zooming run 2
(P95 `38.986` to `33.342` ms), confirming that quantization was a major source
of the three-way regression. However, hysteresis-plus-budget still did not
consistently beat budget-only: moving run 1 and both zooming runs were worse in
P95, while moving run 2 improved slightly. Keep quantization disabled; treat
budget-only as the stronger performance candidate, pending visual validation.

Raw hysteresis-plus-budget captures:

- `huawei-cpu-consistency-lod-hysteresis-budget-moving-dxvk1-full-refresh-run01-20260919-142657`
- `huawei-cpu-consistency-lod-hysteresis-budget-moving-dxvk1-full-refresh-run02-20260919-143025`
- `huawei-cpu-consistency-lod-hysteresis-budget-zooming-dxvk1-full-refresh-run01-20260919-143349`
- `huawei-cpu-consistency-lod-hysteresis-budget-zooming-dxvk1-full-refresh-run02-20260919-143717`
