# Terrain LOD Reimplementation Plan

## Decision

Reimplement the renderer-side terrain LOD system while preserving Perimeter's
terrain simulation, deformation behavior, ownership semantics, and
`TerraInterface` contract.

Do not rewrite the complete terrain subsystem. The performance problem is in
how render tiles, geometric LOD, seams, textures, ownership topology, and GPU
resources are coupled. Replacing that layer is lower risk than replacing the
gameplay terrain model and more likely to produce a maintainable Android and
Quest 3 renderer than continuing to add transition state to the legacy path.

The existing renderer must remain available as a reference and fallback until
the replacement passes visual, functional, and performance gates.

See `docs/terrain-lod-design-analysis.md` for the source analysis and measured
performance evidence supporting this decision.

## Scope

Preserve:

- `TerraInterface` and the underlying height/color data;
- terrain deformation and update notifications;
- zero-layer and player ownership semantics;
- existing game and simulation behavior;
- reflection and shadow-camera behavior;
- the legacy terrain renderer as a comparison path during migration.

Replace:

- camera-driven LOD selection and transition state;
- tile render-object lifetime and GPU resource ownership;
- seam construction;
- the dependency between geometric LOD and terrain texture generation;
- per-frame terrain draw-list construction and batching;
- ownership-driven topology rebuilding where renderer capabilities allow it.

The replacement should be exposed behind a narrow renderer interface instead
of increasing coupling between the Android activity and the engine. Prefer
Android-specific integration under `app/src/main/cpp`; keep any necessary
changes under `Perimeter/` small and suitable for an upstreamable renderer
abstraction.

## Why replacement is preferable

The legacy implementation assumes that:

- an LOD change replaces a render tile;
- seam correction mutates tile vertices and indices;
- terrain ownership determines triangle topology;
- texture resolution, allocation, and LOD use the same render-tile lifetime;
- LOD transitions may allocate and upload GPU resources synchronously;
- tiles and ownership sections are submitted independently.

These assumptions cross class and resource boundaries. Optimizing one stage
therefore leaves the other coupling costs in place. The current caches reduce
some redundant CPU work, but they cannot turn an LOD transition into a cheap
selection operation.

The experimental pending-tile implementation illustrates the limit of
incremental repair. It spreads construction across frames but requires duplicate resources
and an all-visible-grid commit to preserve seams. Continuous camera movement
can invalidate prepared work, and one unready tile can delay all otherwise
ready replacements. This is more complicated without changing the underlying
cost model.

## Target architecture

### Persistent terrain tiles

Maintain persistent render data for each resident 64-by-64 terrain tile. A
camera movement must not invalidate terrain height, color, or ownership data.

Tile content is rebuilt only when an actual terrain revision intersects that
tile. Track a monotonically increasing revision or explicit dirty flags for:

- height data;
- material/color data;
- ownership or zero-layer data;
- bounds used for culling.

Process dirty tiles through a bounded queue when necessary, but do not apply a
camera-driven budget to correctness-critical terrain revisions without a
defined visual fallback.

### Cheap geometric LOD selection

All supported LOD representations should be persistent or generated lazily and
retained. Changing LOD should select existing index data and draw ranges; it
must not regenerate textures, reclassify ownership, or reconstruct the base
terrain grid.

Initially retain the current five LOD sample steps to limit visual changes.
Once parity is established, tune the levels using measured screen-space error
rather than changing representation and policy simultaneously.

### Reusable seam representation

Prefer shared stitch index patterns keyed by:

```text
(tile LOD, left edge, right edge, top edge, bottom edge)
```

Each edge state indicates whether that edge borders the next coarser LOD. This
requires enforcing an adjacent-LOD delta of at most one.

Skirts are an acceptable first implementation because they make transitions
local and independent. They should be tested for visible gaps, overdraw,
terrain holes, zero-layer intersections, reflection artifacts, and steep
height discontinuities. Replace them with exact stitch indices only if their
visual limitations are material.

Do not require an atomic transition across every visible tile. Seam correctness
is local. Commit an individual tile or a small compatible neighbor group when
its boundary representation is valid.

### Textures independent of geometric LOD

Keep one stable terrain texture allocation per resident tile, with mipmaps or
ordinary texture sampling handling distance. Regenerate it only when terrain
appearance changes.

An LOD change must not call `TerraInterface::GetTileColor`, allocate a new
atlas page, or migrate texture content. Texture streaming, if later needed,
should have its own residency policy and must not share geometric LOD state.

### Ownership independent of triangle topology

The preferred long-term representation is:

```text
persistent terrain vertices
    + shared LOD/seam indices
    + terrain material texture
    + ownership/zero-layer mask
```

Encode ownership or zero-layer selection in a mask texture, compact vertex
attribute, or compatible shader input. This permits regular shared indices and
reduces per-player draw sections.

Exact visual behavior must be established before selecting the representation.
If shader-based ownership cannot initially match the legacy renderer, retain
cached per-tile ownership index groups in the first replacement version. Those
groups must be rebuilt only after ownership changes, never because the camera
moved.

### Persistent and batched GPU resources

Avoid a separate small vertex buffer for every tile. Use larger persistent
buffers divided into stable tile ranges or another renderer-native allocation
scheme that supports partial updates.

The target draw path should:

- bind terrain resources in batches;
- avoid per-tile allocation during normal camera movement;
- minimize per-player state and draw changes;
- submit distant low-polygon tiles without disproportionate CPU overhead;
- retain compatibility with both the primary DXVK path and Sokol fallback.

Indirect drawing or GPU-driven culling is not required for the first version.
The first objective is to eliminate synchronous reconstruction and resource
churn.

### Stable LOD policy

After transitions are cheap and correct:

- use projected screen-space error where practical;
- add measured hysteresis or a minimum dwell period;
- enforce an adjacent-LOD delta of at most one;
- prioritize transitions by projected area and proximity;
- keep main, reflection, and shadow passes on a coherent terrain revision;
- avoid allowing auxiliary cameras to create competing render-tile LOD state.

Preserve the current threshold policy for initial parity. Policy tuning should
be a separate benchmarked change.

## Proposed renderer interface

The exact names may change during implementation, but the engine boundary
should remain approximately:

```cpp
class TerrainRenderBackend {
public:
    virtual ~TerrainRenderBackend() = default;

    virtual void setTerrain(TerraInterface* terrain) = 0;
    virtual void resize(const Vect2i& mapSize, int ownershipCount) = 0;
    virtual void invalidate(const Vect2i& min, const Vect2i& max) = 0;
    virtual void prepare(const TerrainView& view) = 0;
    virtual void draw(const TerrainDrawContext& context) = 0;
    virtual void releaseDeviceResources() = 0;
    virtual void restoreDeviceResources() = 0;
};
```

`TerrainView` should contain renderer-neutral camera and viewport information.
`TerrainDrawContext` should distinguish main, reflection, and shadow passes
without exposing Android activity state.

Implementations during migration:

- `LegacyTerrainRenderer`: adapter around the existing tile-map renderer;
- `PersistentTileTerrainRenderer`: the replacement path.

Avoid exposing `sBumpTile`, texture-pool pages, or renderer-specific buffer
handles through this interface.

## Migration plan

### Phase 0: Freeze and measure the legacy reference

- Preserve the existing profiling counters and benchmark camera path.
- Record stationary, panning, deformation, ownership, reflection, and shadow
  baselines for DXVK and Sokol.
- Record screenshots or image comparisons at known camera positions.
- Retain the accepted legacy caches needed to keep development builds usable.
- Avoid further complex changes to global pending-grid transition state.

### Phase 1: Introduce the backend boundary

- Add the renderer interface with no intended visual change.
- Wrap the existing implementation as the legacy backend.
- Add a build or diagnostic runtime selector.
- Confirm the adapter produces unchanged screenshots and timings.

This phase is the only expected architectural touch point in the upstream
engine. Keep it small to preserve clean merges.

### Phase 2: Persistent geometry prototype

- Implement persistent tile height vertices.
- Retain current LOD levels and visual thresholds.
- Start with skirts or shared edge-mask indices.
- Make camera movement perform selection only.
- Retain legacy ownership index groups if necessary.
- Support terrain height invalidation and partial buffer updates.

Success gate: camera panning causes no terrain texture generation, ownership
classification, heap allocation, or tile vertex reconstruction.

### Phase 3: Stable textures and terrain revisions

- Give each resident tile stable texture storage.
- Generate mipmaps where supported.
- Update texture content only for intersecting terrain dirty regions.
- Validate deformation at tile borders and during LOD transitions.

Success gate: camera movement produces zero `GetTileColor` calls.

### Phase 4: Ownership representation

- Define exact legacy ownership/zero-layer visual rules.
- Prototype a mask texture or vertex attribute.
- Remove per-player geometry topology when parity is achieved.
- Collapse tile rendering toward one terrain submission per material/pass.

Success gate: ownership changes update only affected data and do not force
camera-driven topology regeneration.

### Phase 5: Batching and LOD policy

- Consolidate tile resources into larger buffers.
- Reduce state changes and draw calls.
- Add adjacent-level enforcement and measured hysteresis.
- Evaluate screen-space error selection.
- Prioritize local transitions by visual importance.

### Phase 6: Default and retirement

- Run the complete regression and sustained-performance matrix.
- Make the replacement default only after all gates pass.
- Keep the legacy backend available for at least one stabilization cycle.
- Remove the legacy path only after saved games, representative maps, render
  backends, and target devices show no dependency on its behavior.

## Validation matrix

### Correctness

- terrain height and deformation;
- deformation crossing tile boundaries;
- zero layer, player ownership, and mixed-ownership tiles;
- adjacent LOD edges and four-way corners;
- holes, steep slopes, and terrain silhouettes;
- main camera, reflection, shadow, and shadow-map passes;
- camera teleport, rapid zoom, and continuous panning;
- device loss, activity pause/resume, and resource restoration;
- representative campaign and skirmish maps.

### Performance

Capture at least:

- engine frame p50, p95, p99, and maximum;
- terrain preparation and draw p50/p95/p99;
- LOD transition count;
- terrain revision and rebuilt-tile count;
- CPU classification, topology, texture, and upload time;
- draw and buffer-binding counts;
- allocations and bytes uploaded during camera-only motion;
- resident terrain memory and peak transition memory;
- sustained performance and thermal behavior.

For a camera-only benchmark after initial warm-up, target:

- zero terrain-content rebuilds;
- zero terrain texture regenerations;
- zero per-tile GPU allocations;
- bounded local LOD transitions;
- no terrain-related long frames;
- materially lower terrain preparation p99 than the legacy 25--30 ms result.

Use repeated matched runs. Manual camera paths can cross different numbers of
thresholds, so a single before/after capture is not sufficient.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| Ownership rendering differs from legacy visuals | Keep cached legacy ownership indices until mask-based parity is proven. |
| Skirts show through holes or reflections | Validate early; retain exact stitch indices as the fallback design. |
| Terrain deformation updates become stale | Use explicit per-domain revisions and dirty-tile assertions. |
| Memory grows by retaining all LODs | Share index patterns and store one persistent height representation where possible. |
| Upstream merges become difficult | Limit core changes to the backend interface and keep implementation isolated. |
| DXVK and Sokol diverge | Keep renderer-neutral terrain data and validate both backends at every phase. |
| Quest stereo doubles preparation | Prepare view-independent terrain once and derive eye visibility without rebuilding content. |

## Legacy-path investment policy

Continue improving the legacy renderer only when a change is low risk,
measurable, and useful while the replacement is developed. Appropriate work
includes diagnostics, correctness fixes, and proven caches that do not expand
transition-state complexity.

Do not make major investments in:

- whole-visible-grid transaction coordination;
- additional copies of pending tile resources;
- more caches whose sole purpose is to make camera-driven full reconstruction
  less expensive;
- DXVK-specific workarounds for CPU terrain reconstruction;
- changes to gameplay terrain logic solely for render LOD performance.

## Final recommendation

Proceed with a parallel renderer-side reimplementation. Preserve the terrain
model and dirty-update contract, introduce a narrow backend boundary, and make
the new implementation persistent and selection-based. Use the legacy path for
A/B validation until the replacement demonstrates deformation and ownership
parity on both renderers.

The defining acceptance criterion is simple: after warm-up, moving the camera
must change which terrain representation is drawn without reconstructing what
the terrain contains.
