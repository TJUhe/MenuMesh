# Mesh Kernel Architecture Direction

This project is moving from a single QEM simplification prototype toward an
industrial polygon-mesh geometry kernel. QEM and line quadrics remain important,
but they are one remeshing/decimation capability inside a wider SDK boundary.

## Target Capabilities

The long-term target is comparable in scope to industrial polygon mesh kernels:

- mesh validation and analysis,
- topology repair and healing,
- feature detection and preservation,
- simplification and remeshing,
- Boolean section/union/subtract/intersect operations,
- offsetting/thickening,
- stable C ABI integration for host applications.

## Layering

```text
core        handles, status/result, tolerances, attributes
mesh        exchange Mesh plus topology caches and future editable topology
io          STL/OBJ now, PLY/3MF later
analysis    stats, manifoldness, distance, self-intersection, curvature
repair      merge duplicates, orient, remove degenerates, fill holes, make manifold
features    boundaries, creases, loops, fitted circles/cylinders/planes
remesh      QEM, line quadrics, split/flip/smooth, sizing fields
boolean     BVH, triangle intersections, classification, stitching
offset      shell and solid offsets
c_api       opaque handles and stable binary integration
cli         batch/debug consumer, not the library boundary
```

## Physical Source Layout

The physical tree mirrors the architecture boundary:

```text
include/line_quadrics_qem/      installed SDK headers
src/<domain>/*.cpp              implementation entry points
src/<domain>/detail/*.h         private implementation helpers
apps/                           CLI/app consumers
examples/                       external-consumer smoke tests
tests/                          regression and validation tests
docs/                           design, guides, paper notes, generated notes
```

This is deliberately smaller than a full OCCT module forest at the current
size: keep the library compact, but make the ownership of each header obvious.
As the kernel grows toward repair, remesh, boolean, and offset modules, the
OCCT lesson becomes more important: do not let public API, implementation
helpers, samples, tests, and tools collapse into one directory.

The current simplification internals (`DynamicTopology`, `SpatialFaceIndex`,
`CollapseLegality`, geometry predicates, and per-run state) live under
`src/simplification/detail/`. They are intentionally not installed and
may change with the algorithm. External consumers should include only
`include/line_quadrics_qem/...`.

## Data-Structure Policy

`Mesh` stays as the simple exchange format: dense vertices plus triangle faces.
It is cheap to pass through the C++ API, easy to expose through the C ABI, and
suitable for I/O.

Algorithms that need repeated adjacency must not rebuild unordered maps in each
module. They should build `MeshTopology` once and then iterate dense arrays of
edges and vertex incidents. This is the first stable topology layer and is
appropriate for validation, statistics, feature detection, repair preflight, and
read-only analysis.

Future topology-editing algorithms should use typed handles (`VertexId`,
`EdgeId`, `HalfedgeId`, `FaceId`) with dense storage, generation-aware free
lists, and explicit compaction. That gives stable algorithm references while
keeping cache-friendly iteration.

Attributes should live beside topology, not inside core vertex/face structs.
Normals, UVs, colors, feature flags, source face IDs, region IDs, and sizing
fields should be stored in typed attribute arrays so Boolean, repair, and remesh
pipelines can preserve or remap them intentionally.

## API Pattern

Prefer value-style operations with options and reports:

```cpp
Result<Mesh> repairMesh(const Mesh& input,
                        const RepairOptions& options,
                        RepairReport* report);

Result<Mesh> decimateMesh(const Mesh& input,
                          const DecimationOptions& options,
                          DecimationReport* report);
```

Existing exception-based convenience APIs can remain for compatibility, but new
industrial surfaces should be able to return `Status`/`Result` without crossing
ABI boundaries with C++ exceptions.

Internally, complex algorithms should be composed from policies:

- cost policy,
- placement policy,
- legality policy,
- tolerance policy,
- attribute propagation policy.

This keeps QEM, line quadrics, feature projection, and future curvature-aware
or Boolean-aware behavior swappable without turning the public API into an
inheritance-heavy framework.
