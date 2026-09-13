# DXVK 1 Android Frame-Pacing Plan

## Objective

Reduce visible DXVK 1 stutter on Android while preserving terrain and Zeroplast
correctness. DXVK is the primary renderer; Sokol is a cadence reference, not a
performance or optimization target. Keep Android-specific integration in `app/` where
possible, and minimize changes to `Perimeter/`.

Scope is the Huawei SCM-W09 at 60 Hz, 75% scale, half-refresh D30, and the
in-game **Fast** profile. All results recorded in this D30 plan were performed
with Huawei system Performance mode disabled, in the normal/non-Performance
device mode. DXVK 2 and D60 / interval-1 are outside this historical matrix;
the new D60 Performance-mode benchmark is documented in
[dxvk1-d60-performance-benchmark.md](dxvk1-d60-performance-benchmark.md).

## Current state

| Area | Status | Evidence / decision |
| --- | --- | --- |
| Reproducible workload | Complete for static/moving and zooming on both renderers | Tutorial Campaign mission; 10 s warm-up; 20 s static/held-square motion or 21 s zoom-out/in motion; two runs per case |
| Common native timing | Complete | Engine, frame work, and DXVK stage records share a monotonic timeline |
| Sokol comparison | Complete for static/moving/zooming | Motion affects both renderers; final Sokol moving p99 is 40.220 / 41.631 ms, while zooming p99 is 80.607 / 82.896 ms |
| Current DXVK stack | Shared zoom cache only | Boundary candidates, safe 0-to-1 texture reuse, full topology reuse, and the shared zoom LOD cache are enabled; seam-restore, budgeted LOD builds, cached-texture reuse, and cached point/region reuse are removed |
| Remaining defect | Open | Presentation tails and residual point/region/topology/mesh work remain in restored-variant rebuilds |
| Pipeline compilation | Ruled out for warm steady state | Zero pipeline compilations in final DXVK moving windows |
| D30 rejected optimizations | Moved to D60 candidate matrix | See [dxvk1-d60-performance-benchmark.md](dxvk1-d60-performance-benchmark.md) |
| Corrected SwappyVk interval-2 path | Call-rate gate passed; cadence gate open | Three fresh zoom traces record one `queue_present` per native frame, but SurfaceFlinger still has 61 / 96 / 82 large gaps |

## Final controlled comparison

All values are native engine frame intervals in milliseconds. See
[dxvk1-frame-pacing-benchmark.md](dxvk1-frame-pacing-benchmark.md) for capture
procedure and per-run values.

| Renderer | Static P99 | Moving P99 | Moving max | Moving >=50 ms | Finding |
| --- | ---: | ---: | ---: | ---: | --- |
| DXVK 1 | 34.337 / 34.657 | 49.998 / 45.202 | 72.953 / 77.499 | 6 / 1 | Rare large motion tails remain |
| Sokol | 35.569 / 35.412 | 40.220 / 41.631 | 41.968 / 45.679 | 0 / 0 | Same workload sensitivity, milder tails |

Conclusion: camera movement has a shared engine-workload cause. DXVK amplifies
the rare tail; it is not explained by warm-cache pipeline compilation.

## Zooming-camera baseline

Zooming is a separate workload because changing camera distance can invalidate
different terrain/LOD preparation paths than planar camera movement. The
controlled sequence is:

1. Start from a fresh Tutorial mission after the HUD is visible.
2. Zoom out continuously for 1.5 seconds.
3. Zoom in continuously for 1.5 seconds.
4. Repeat the out/in pair seven times, for a 21-second capture.

Run it twice per renderer with the standard 10-second warm-up and D30 settings.
Record native frame P95/P99/max, counts at >=37.5 ms and >=50 ms, and the
`frame-work.csv` totals for scene preparation, bump-tile calculation, and any
LOD/topology work. Retain the raw DXVK and compositor traces alongside the
capture. The benchmark table is in
[dxvk1-frame-pacing-benchmark.md](dxvk1-frame-pacing-benchmark.md); the DXVK
rows are the pre-optimization baseline and the Sokol rows are retained as
reference only. The two DXVK captures recorded P99 values of 79.988
and 52.317 ms, with maxima of 96.795 and 98.958 ms; the Sokol captures
recorded 80.607 and 82.896 ms p99.

### Zoom-specific root-cause evidence

The zoom trace identifies a rebuild storm at LOD boundaries:

| Evidence | Zoom result | Interpretation |
| --- | ---: | --- |
| `bump_tile_calc_call` count per frame | P99 123 / 136; moving reference P99 39 | Many visible tiles are recalculated during a zoom step |
| `scene_tilemap_predraw` | P99 39.991 / 36.560 ms | Tile preparation dominates the long zoom frames |
| `tilemap_predraw_calc` | P99 63.796 / 37.308 ms | LOD selection/allocation and visibility-side preparation are expensive |
| `bump_tile_calc_total` | P99 71.428 / 36.162 ms | Rebuilding `sBumpTile` geometry is a major direct cost |
| `bump_tile_point_region_total` | P99 11.497 / 11.377 ms | Region projection is repeated for each rebuilt tile |
| `bump_tile_topology_total` | P99 6.382 / 6.435 ms | Topology work is repeated with each rebuilt tile |
| LOD texture work | LOD1 total 2726.2 / 2636.6 ms; LOD2 total 722.0 / 583.1 ms | Zoom also regenerates many texture pages, especially when crossing LOD2 |
| Cross-renderer result | Sokol zoom P99 80.607 / 82.896 ms; tilemap calc P99 27.739 / 28.386 ms | The workload is engine-heavy on both renderers; DXVK adds a separate tail, but is not the root cause |

The source path explains the amplification. `CalcTileMap()` computes an LOD
from camera distance for every visible tile. When it changes, the current
`sBumpTile` is retired and a new one is allocated; the new object then enters
`sBumpTile::Calc(update_texture=true, ...)`, which rebuilds texture, points,
region projections, and topology. The existing point/interior/topology caches
are attached to the old tile object, so they do not help this first rebuild.
Texture-page reuse only helps transitions whose `bumpTexScale` matches (LOD 0
and 1); it does not reuse the rebuilt geometry or topology.

### Zoom optimization hypothesis

Retain alternate LOD variants per tile. Keep a bounded cache of recently built
`sBumpTile` resources keyed by terrain revision, tile coordinate, and LOD.
Reverse zooms could reactivate a prepared variant instead of rebuilding points,
topology, and textures. The cache must invalidate on terrain/ownership updates
and have an explicit memory limit. This hypothesis produced the accepted shared
zoom LOD cache described below.

Moving variant preparation off the render-critical path remains a future design
study if the engine lifetime model permits it. The other rejected LOD policies
are tracked in the D60 candidate document.

The first experiment should be a small, terrain-revision-aware alternate-LOD
cache or an isolated zoom hysteresis toggle. Do not combine both in the first
comparison; the baseline shows that reducing the number of `sBumpTile::Calc`
calls is the primary success criterion. The cache uses shared tilemap
resources, so it is enabled for both Android renderers; DXVK remains the
primary optimization target and Sokol remains a regression/reference check.

### Shared zoom LOD cache

The first zoom-specific experiment is now adopted for both Android renderers. It keeps
at most two inactive `sBumpTile` variants per tile, keyed by their LOD. A
reverse zoom can restore a prepared variant instead of allocating a new
resource. Cached variants are discarded on `Tile.GetUpdate()` terrain or
ownership changes. Android launches for either renderer supply
`zoom_lod_cache=1` by default, while `zoom_lod_cache=0` remains
available for an A/B capture.

The two post-change DXVK captures were repeatable against the two-run baseline:

| Run | Baseline P99 / max | Cache P99 / max | Baseline >=50 ms | Cache >=50 ms | Cache hits |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 79.988 / 96.795 ms | 56.396 / 76.729 ms | 17 | 12 | 10,157 |
| 2 | 52.317 / 98.958 ms | 50.805 / 72.938 ms | 15 | 7 | 10,735 |

The cache also reduced `tilemap_predraw_calc` P99 to 45.430 / 41.728 ms from
63.796 / 37.308 ms, and `bump_tile_calc_total` P99 to 43.771 / 38.677 ms from
71.428 / 36.162 ms. The cache is shared by both renderers; no
Sokol-specific tuning was added.

The cache-restore seam, budgeted/deferred LOD, and cached point/region results
were rejected in the D30 matrix. Their measured evidence and possible D60
Performance-mode follow-up candidates are now recorded in
[dxvk1-d60-performance-benchmark.md](dxvk1-d60-performance-benchmark.md).

## Root-cause evidence

| Investigation step | Static / moving result | Conclusion |
| --- | --- | --- |
| Scene preparation split | Tile-map pre-draw rose from 5.59 to 30.42 ms p99 on DXVK | Camera preparation, not lock contention, dominates |
| Tile-map split | Border rebuild rose from 0.28 to 29.60 ms p99 | Visibility tests are not the primary cost |
| Bump-tile aggregate | No static calls; moving aggregate about 30.63 ms p99 | LOD-border-triggered `sBumpTile::Calc` is the core CPU spike |
| CPU split | Point/region projection 16.79 ms p99; topology 5.12 ms; index upload 0.07 ms | Full-grid point work, not index upload, was the first target |
| WSI / pipeline trace | No final-window compilation; queue-present and present wait vary | Presentation remains a separate tail contributor |

## Accepted engine changes

| Change | Safety rule | Measured result | Validation |
| --- | --- | --- | --- |
| Base-grid and interior-point caches | Rebuild on terrain update; reuse only for `Calc(false)` | Point-init p99 fell from about 13.79 ms to 6.41-7.23 ms | Post-pan terrain clean |
| Boundary-region candidate cache | Candidate list refreshes only on terrain revision | Border-rebuild p99 24.37 / 24.66 ms vs about 29-30 ms | Post-pan screenshot clean |
| Boundary-only vertex writes | Interior vertices remain unchanged for seam-only stitch | Vertex-write p99 about 1.41 ms vs about 2.1 ms | Clean terrain seams |
| Safe LOD 0-to-1 texture reuse | Same page pool only; never after `Tile.GetUpdate()` | Frame p99 41.69 / 43.44 ms vs 49.43 / 48.68 ms candidate-cache baseline | Two clean post-pan runs |
| Full per-player topology reuse | Only pure LOD stitches; terrain/ownership update rebuilds normally | Initial p99 34.695 / 37.953 ms | Manual Zeroplast expansion while panning confirmed correct edges |

The final confirmation pair was more variable than the first topology pair.
That variability is recorded rather than hidden; seam-restore was removed
because its mixed no-sampler result did not demonstrate a zoom improvement.

## D60 candidate handoff

The D30 rejected optimizations, their measured results, and their possible D60
Performance-mode follow-up tests have moved to
[dxvk1-d60-performance-benchmark.md](dxvk1-d60-performance-benchmark.md).
They remain rejected for D30 and are not part of the accepted stack.

## Presentation-path findings

| Question | Finding | Consequence |
| --- | --- | --- |
| Does interval 2 duplicate presentation? | The initial trace appeared to, but duplicate `queue_present` rows were instrumentation residue; the corrected diagnostic trace recorded 600 calls for 600 native frames | Do not treat duplicate trace rows as proof of duplicate presents; compositor cadence still needs validation |
| Does pipeline compilation explain steady-state tails? | No; zero overlaps in warm measurements | Keep cold-cache work separate |
| Does single-present improve engine cadence? | Yes; 36.479-37.360 ms p99 and <1% long frames | Retain as the measured cadence baseline; current runtime use of SwappyVk is confirmed separately |
| Did the prior single-present limiter present smoothly? | No; its SurfaceFlinger trace shows recurring 16.7/50 ms phase corrections | That capture does not establish SwappyVk pacing behavior; use a fresh Swappy-enabled capture |
| Is the DXVK 1 SwappyVk path integrated and used? | Confirmed; three corrected fresh zoom runs recorded 630/630, 600/600, and 600/600 `queue_present` calls with zero pipeline compilations | One-present-call gate passes; compositor phase-lock gate fails |

## Prioritized remaining work

1. **Attribute final DXVK outliers.** Complete for native/DXVK timing: the
   cache reports show mixed tilemap/presentation tails and presentation-only
   frames. The synchronized zoom and moving cache samples add compositor
   evidence: the latest Swappy zoom sample classified 15 of 23 >=50 ms native
   intervals as compositor misses, while the moving cache sample independently
   classified 2 of 3 as compositor misses and 1 as native-only.
2. **Collect a compositor trace.** Use Perfetto/FrameTimeline where available;
   on this Android 10 device retain SurfaceFlinger latency sampling. The
   synchronized zoom and moving samples are complete. Across the fresh Swappy
   zoom pair plus the latest run, SurfaceFlinger still shows recurring large
   gaps (61 / 96 / 82); use this evidence before making a presentation-path
   change.
3. **Validate the existing SwappyVk presentation path.** Complete for the
   one-present-call gate: three fresh zoom runs recorded one `queue_present`
   call per native frame. The actual-cadence gate is not met, with 61, 96, and
   82 large SurfaceFlinger gaps; do not change Swappy call rate based on this
   evidence.
4. **Revisit terrain architecture with the measured zoom hypothesis.** The
   accepted point/region reuse reduces restored-variant preparation, but
   `tilemap_predraw_calc` and `tilemap_border_rebuild` still contain expensive
   topology/mesh tails. Further gains require reducing dynamic seam/topology
   work or preparing variants off the render-critical path.
5. **Deferred studies.** Cold vs warm cache, compiler-thread count, and input
   latency remain useful. D60 / interval-1 work is tracked in the separate
   [D60 Performance-mode benchmark](dxvk1-d60-performance-benchmark.md).

## SwappyVk validation and promotion gates

Before enabling the DXVK 1 SwappyVk pacing path by default:

| Gate | Required evidence |
| --- | --- |
| Correct rate | One application submission per D30 frame; no duplicate FIFO loop |
| Actual cadence | Compositor timestamps near 33.33 ms without recurring 16.7/50 ms correction; current fresh zoom runs fail with 61 / 96 / 82 large gaps |
| Engine cadence | No regression in P95/P99 or long-frame count |
| Input | Input-latency measurement, not an assumption from queue depth |
| Lifecycle | Correct swapchain/window recreation and frame-rate-hint cleanup |
| Regression | Two clean static, moving, and zooming Tutorial runs, retained with raw traces |

## Historical capture policy

Older captures remain under `captures/frame-pacing/` as supporting evidence.
Use the current controlled matrix for decisions. Every new result should add a
row to the benchmark table and a decision to the relevant table above rather
than another chronological checklist paragraph.
