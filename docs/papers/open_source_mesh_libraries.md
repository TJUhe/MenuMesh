# Open-Source Surface Mesh Library Map

Snapshot date: 2026-07-11. This map covers triangle/polygon surface mesh processing only. A library
may read CAD exchange formats indirectly, but that does not make B-Rep modeling part of ManuMesh.

License notes are factual repository metadata; algorithm comparison is the main purpose of this map.

| Library | Surface-mesh capabilities relevant here | License signal | ManuMesh use |
| --- | --- | --- | --- |
| [OpenMesh](https://www.graphics.rwth-aachen.de/software/openmesh/) | Halfedge connectivity, status flags, collapse/split/flip primitives, decimater framework | BSD 3-Clause | Primary architecture reference for separating mesh kernel/status/garbage collection from algorithm policy. |
| [CGAL Polygon Mesh Processing](https://github.com/CGAL/cgal) | Isotropic remeshing, sharp-edge constraints, repair, distance/intersection predicates | Package-specific GPL/LGPL | Strong behavior and API reference for constrained remeshing tests. |
| [pmp-library](https://github.com/pmp-library/pmp-library) | Compact surface mesh, isotropic/adaptive remeshing, curvature and feature edges | MIT | Best small C++ reference for a practical local-operator remesh loop. |
| [libigl](https://github.com/libigl/libigl) | Geometry operators, curvature, adjacency, predicates, parameterization and optional copyleft modules | Core MPL 2.0; optional modules/dependencies vary | Reference and test oracle for focused geometry operations, not a mutable mesh kernel replacement. |
| [geometry-central](https://github.com/nmwsharp/geometry-central) | Halfedge surface mesh, intrinsic triangulations, tangent data and robust geometric operators | MIT | Reference for intrinsic quantities and topology-aware data ownership. |
| [Geogram](https://github.com/BrunoLevy/geogram) | Restricted Voronoi diagrams, CVT, mesh repair and surface processing | BSD 3-Clause | Reference for global sampling/CVT remeshing and spatial acceleration. |
| [VCGlib](https://github.com/cnr-isti-vclab/vcglib) | Triangle mesh cleaning, remeshing, simplification and feature-aware filters | GPL 3.0 | Algorithm comparison and external validation reference. |
| [MeshLab](https://github.com/cnr-isti-vclab/meshlab) | End-user filters backed largely by VCGlib, including remeshing and cleaning | GPL 3.0 | Manual/result comparison tool, not an SDK dependency. |
| [MMG](https://github.com/MmgTools/mmg) | Metric-driven anisotropic surface remeshing through MMGS | LGPL 3.0-or-later | Reference or optional external-process baseline for anisotropic metric behavior. |
| [Instant Meshes](https://github.com/wjakob/instant-meshes) | Direction-field-aligned triangle/quad mesh generation | BSD-style 3-Clause terms | Far-term field-aligned benchmark; keep separate from isotropic triangle remesh MVP. |

## Recommended Study Order

1. OpenMesh for edit-kernel responsibilities and status/compaction semantics.
2. pmp-library for the smallest readable split/collapse/flip/smooth remeshing loop.
3. CGAL PMP for constrained remeshing contracts and production-grade preconditions.
4. Geogram and M039 for Voronoi/CVT or metric-driven sampling.
5. MMG for anisotropic metric fields and Instant Meshes for field-aligned topology generation.

## Feature Detection Patterns Applied in ManuMesh

- OpenMesh: keep feature policy out of the topology kernel and store the result
  as explicit edge/vertex properties or an analysis object.
- CGAL PMP: treat detected sharp or smooth curves as constrained edge maps before
  segmentation or remeshing consumes them.
- pmp-library: compute curvature with explicit neighborhood, boundary, and
  smoothing choices; test known analytic shapes instead of only visual output.
- libigl: use tangent-frame quadric fitting with k-ring/radius neighborhoods and
  expose both principal values and directions.
- geometry-central: attach differential quantities to the mesh element that owns
  them and invalidate/recompute them deliberately after topology edits.

ManuMesh follows these patterns without importing an external mesh kernel. The
current smooth path adds robust reweighting, dimensionless local-scale
normalization, signed directional extrema, cross-scale persistence, and explicit
feature-graph ownership.

## Boundary for ManuMesh

- Reuse concepts and validate behavior against these libraries; do not mirror their public APIs.
- Keep `mesh_edit` responsible for mutable topology state, index mapping, and compaction.
- Keep `remeshing` responsible for target length/metric fields, operation scheduling, feature and
  boundary policy, projection, quality objectives, and stopping criteria.
- Keep feature detection responsible for evidence, graph/loop ownership, primitive or smooth-curve
  fitting, and confidence. Remeshing consumes that result through a narrow adapter.
- Do not add B-Rep entities, NURBS topology, solid booleans, CAD feature trees, or STEP topology to
  the surface-mesh kernel.
