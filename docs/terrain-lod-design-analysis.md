# Terrain LOD Design Analysis

## Summary

The terrain LOD system has a major architectural performance problem: a
camera-driven LOD or seam transition can reconstruct terrain content that has
not changed. This is the principal cause of the measured frame-time spikes
while panning the camera.

The fixed 64-by-64 tile grid is not inherently unsuitable for Perimeter's
deformable RTS terrain. The expensive part is coupling geometric LOD, seam
stitching, terrain ownership classification, texture generation, and GPU
resource allocation. Once the camera is stationary, the system can be
reasonably efficient. While LOD boundaries move, it performs synchronous CPU
work that reached approximately 25--30 ms at p99 in the Android measurements.

This document describes the current working tree as of September 11, 2026.
The working tree contains uncommitted terrain-cache and staged-transition
experiments in `Perimeter/Source/Render/tilemap`; those are identified
separately from the upstream behavior below.

## Current algorithm

The terrain is divided into fixed 64-by-64 map-unit tiles and supports five
discrete geometric LODs:

| LOD | Sample step | Vertices per tile | Triangles per tile |
|---:|---:|---:|---:|
| 0 | 2 | 33 x 33 = 1,089 | 2,048 |
| 1 | 4 | 17 x 17 = 289 | 512 |
| 2 | 8 | 9 x 9 = 81 | 128 |
| 3 | 16 | 5 x 5 = 25 | 32 |
| 4 | 32 | 3 x 3 = 9 | 8 |

The tile and LOD constants are defined in
`Perimeter/Source/Render/tilemap/TileMap.h` and
`Perimeter/Source/Render/tilemap/TileMapBumpTile.h`.

For each terrain pre-draw, the renderer:

1. Clips the camera volume and rasterizes it into a tile visibility map.
2. Selects a visible tile's LOD from the 3D distance between the camera and the
   tile center.
3. Compares each resident tile with the LODs of its four neighbors.
4. Collapses boundary vertices on the more detailed tile to match a coarser
   neighbor.
5. Rebuilds affected vertex and per-player index data.
6. Adds each visible tile to a texture-pool render list.

LOD thresholds are calculated in
`Perimeter/Source/Render/tilemap/TileMapRender.cpp` in
`cTileMapRender::CalcTileMap`. Seam reconstruction is performed by
`sBumpTile::CalcPoint` and `sBumpTile::FixLine` in
`Perimeter/Source/Render/tilemap/TileMapBumpTile.cpp`.

## Confirmed primary bottleneck

In the upstream implementation, a neighboring LOD change sets `update_line`
and calls `sBumpTile::Calc(false)`. Although only an outside edge changed, that
path historically performed most of a complete tile rebuild:

- initialize every point in the tile grid;
- test points against every player's terrain ownership column;
- project nearby ownership-region border points;
- reconstruct per-player triangle topology;
- rewrite the tile vertex buffer;
- free and recreate dynamic per-player index pages.

Terrain ownership lookup compounds the cost. `Column::filled` delegates to
`CellLine::filled`, which linearly scans the row's intervals. Performing that
test across the full grid, for potentially several players, is poorly suited
to a camera-driven seam update. See `Perimeter/Source/Game/Region.h`.

Repository benchmark notes recorded the following representative results:

| Measurement during camera motion | Approximate p99 |
|---|---:|
| Post-visibility LOD-border rebuild | 29.60 ms |
| Aggregate `sBumpTile::Calc` | 30.63 ms |
| CPU mesh/topology portion | 22.05 ms |
| Vertex-pool lock/write | 2.36 ms |
| Point/region projection | 16.79 ms |
| Per-cell topology construction | 5.12 ms |
| Index-pool assembly | 0.07 ms |

The matched stationary capture had no tile rebuilds in its measurement window.
These results isolate CPU reconstruction as the dominant panning stall. Buffer
upload synchronization, DXVK pipeline compilation, and visibility testing do
not explain the main spike. Full benchmark history is recorded in
`docs/dxvk1-frame-pacing-plan.md`.

## Design flaws

### 1. Seam changes reconstruct mostly invariant tile data

The seam depends only on a tile's outside boundary. Nevertheless, the original
path reclassified the full point grid and rebuilt all topology and GPU pages.
One neighboring transition can therefore turn a small edge correction into
thousands of ownership queries and triangle decisions.

This is the largest confirmed performance flaw.

### 2. Neighbor-dependent meshes amplify LOD transitions

A tile mesh is determined by both its own LOD and four `border_lod` values.
When one tile crosses a threshold, as many as four neighbors may require seam
updates. During camera motion, the moving LOD contour can trigger many tile and
neighbor rebuilds in the same frame.

The stitcher collapses every other boundary vertex. It does not construct a
general ratio-specific transition for arbitrary differences between adjacent
LODs. Correctness therefore implicitly depends on adjacent tiles differing by
no more than one level, but LOD selection does not explicitly enforce that
invariant.

### 3. Geometry LOD is coupled to texture generation and allocation

Changing LOD replaces the `sBumpTile`. Construction allocates a vertex-buffer
page and a texture-atlas page, and mixed-ownership tiles may allocate several
index pages. Initializing the replacement calls `Calc(true)`, including
`TerraInterface::GetTileColor`, even though camera movement did not modify the
terrain.

A geometric LOD transition should ordinarily select persistent geometry or
index data. It should not regenerate terrain color content.

### 4. Hard thresholds provide no stable transition policy

LOD is selected from hard distance thresholds using the tile center. There is
no persistent hysteresis, screen-space geometric-error metric, geomorphing, or
transition priority. Tiles near a threshold can change repeatedly during small
camera movements.

An Android-only 8 percent hysteresis experiment reduced recorded rebuild calls
from 582 to 543, but did not reliably improve overall tail latency. Hysteresis
may be useful as a secondary measure, but it cannot compensate for an
expensive transition operation.

The distance metric also ignores actual terrain roughness and projected error.
It can spend triangles on flat distant terrain while undersampling silhouettes
or rough terrain at an equivalent center distance.

### 5. GPU submission is fragmented

Each tile has its own vertex buffer. Terrain ownership can divide its triangles
into multiple per-player index pages, and `DrawBump` submits each non-empty
section separately. Distant tiles can contain as few as nine vertices while
still incurring buffer binding, color state, and draw-submission overhead.

Measured terrain submission p99 rose from approximately 2.76 ms while static
to 4.59 ms while moving. This is material on Android, although it remains
secondary to CPU reconstruction.

### 6. Full-map and heap-heavy work remains in the frame path

Several passes scan the entire tile grid every pre-draw. Visibility generation
also constructs and clips temporary `CMesh`, `std::set`, and vector structures.
`BuildRegionPoint` walks terrain borders for every player even when only a
subset of tiles accepts the callbacks.

These are worthwhile later targets, but measured visibility-map p99 was about
3.10 ms, far below the approximately 30 ms seam-rebuild spike. They should not
be optimized before the transition architecture.

## Current worktree experiments

The current working tree contains caches for:

- immutable base `VectDelta` point grids;
- resolved interior points;
- ownership candidates capable of affecting tile boundaries;
- seam-independent interior topology;
- boundary-only vertex rewriting.

The boundary-candidate cache produced a repeatable improvement: border-rebuild
p99 fell from approximately 29--30 ms to 24.37 and 24.66 ms in two moving-camera
runs. These caches preserve the terrain-update path and are useful incremental
optimizations, but the number of simultaneous LOD transitions still dominates
the tail.

The worktree also prepares at most four replacement tiles per frame and waits
until every mismatched visible tile is ready before committing any of them.
This avoids exposing a mixed seam state, but global atomicity introduces new
risks:

- transition latency is at least `ceil(changed tile count / 4)` frames;
- current and pending tiles temporarily consume resources together;
- continuous camera motion can invalidate prepared replacements before commit;
- one unprepared visible tile delays every otherwise-ready replacement;
- row-major preparation does not prioritize screen coverage or proximity.

Seam correctness is a local adjacency constraint. Requiring an atomic commit
across the entire visible map is stronger and less scalable than necessary.
This staged implementation should be benchmarked as an experiment, not treated
as the final architecture.

## Recommended redesign

### Priority 1: Make LOD selection independent of terrain reconstruction

Keep sampled height and ownership data persistent until an actual terrain
revision. A camera-driven LOD change should select an existing representation,
not call the full terrain-update path.

For the current renderer architecture, viable approaches include:

- shared index patterns for each LOD and four-edge stitch mask;
- lazily cached per-tile LOD/edge variants retained across camera movement;
- skirts as a simpler temporary seam solution;
- local two-phase transitions for compatible neighboring tile groups.

### Priority 2: Decouple textures from geometric LOD

Keep a stable terrain texture per resident tile and use mipmaps or texture
sampling to handle distance. Regenerate texture content only when terrain
appearance changes. This removes `GetTileColor` and texture-atlas churn from
ordinary camera movement.

### Priority 3: Remove ownership from geometric topology

The per-player triangle split prevents broad reuse of regular index buffers and
increases draw calls. Encode ownership/color selection in a texture, vertex
attribute, or compatible shader input so a tile can use one geometric index
stream where possible.

This is more invasive than caching but has the largest long-term batching
benefit. It should be isolated inside the render/platform layer to minimize
changes to core terrain simulation.

### Priority 4: Add a local, stable transition policy

After transitions are cheap:

- enforce an adjacent-LOD delta of at most one;
- add measured hysteresis or dwell time;
- prioritize pending transitions by projected screen area and distance;
- commit locally compatible groups instead of the full visible grid;
- use screen-space error rather than tile-center distance where practical.

### Priority 5: Batch and simplify submission

Combine tile data into larger persistent buffers, reduce per-player draw
sections, and avoid tiny per-tile vertex buffers. This addresses the secondary
terrain submission cost and better matches Android and future Quest 3 GPU/CPU
constraints.

## Validation requirements

Any redesign must preserve Perimeter's deformable terrain and ownership
boundaries. Validate at least:

- stationary and continuously panning camera paths;
- repeated movement across LOD thresholds;
- terrain deformation during and after an LOD transition;
- zero-layer/player ownership boundaries;
- tile edges, corners, holes, and texture seams;
- main, reflection, and shadow-map camera passes;
- bounded CPU time, draw count, memory growth, and resource churn;
- Sokol and primary DXVK paths;
- eventual Quest 3 camera configurations.

Record p50, p95, p99, maximum frame time, transition count, rebuilt tile count,
draw count, and CPU time split between classification, topology, vertex writes,
texture generation, and submission. Use repeated matched runs because the
number of threshold crossings is not identical between manual camera paths.

## Conclusion

The terrain LOD system's central defect is not the fixed tile grid. It is that
camera-driven LOD and seam changes are represented as terrain-content rebuilds.
That design creates transition fan-out, repeats invariant CPU work, churns
resources, and fragments rendering. Existing measurements show that it is the
dominant cause of the observed camera-motion terrain stalls.

Incremental caches reduce the cost but do not change the scaling behavior. The
durable fix is to make terrain revision, material/ownership data, geometric LOD,
and seam selection independent pieces of state, so ordinary camera motion only
selects persistent render data.
