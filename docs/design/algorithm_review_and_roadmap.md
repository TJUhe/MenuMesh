# Algorithm Review And Roadmap

Last reviewed: 2026-07-03

Related validation note: `docs/design/current_data_probe_2026_07_02.md`.

## Scope

This review covers the current mesh simplification and feature-preservation
algorithm in this repository. The target data regime is mostly clean CAD/STL/OBJ
triangle meshes with hard creases, circular holes, weak feature curves,
boundaries, and occasional low-quality or noisy tessellation.

The distilled literature suggests treating this as constrained QEM
simplification, not as plain decimation. The most relevant families are:

- QEM and edge-collapse simplification: Garland-Heckbert 1997, Hoppe 1996,
  Lindstrom-Turk 1998.
- Feature-sensitive QEM and weak-feature preservation: local source IDs 005,
  021, 024, 037, 048.
- CAD/STL feature extraction and feature graph tracing: local source IDs 010,
  063, 067.
- Normal tensor or normal voting for robust feature evidence: local source IDs
  031, 057, 066.
- Engineering policy patterns: CGAL Surface Mesh Simplification constrained
  edges/placement, OpenMesh decimation modules, libigl qslim, MeshLab/VCGLib.

## Current Score

Current algorithm score: **8.55 / 10**.

This is no longer a minimal QEM prototype. It has a useful line-quadric cost,
feature detection, circular loop fitting, feature projection, boundary-aware
constrained collapses, topology checks, normal-deviation checks,
triangle-quality checks, spatial-indexed local self-intersection checks,
ellipse primitive placement projection, non-circular feature polyline
projection, a first polygonal feature-curve deviation budget, and gtest
coverage for the latest legality, graph-recovery, primitive, and curve-budget
behavior.

It is still not industrial-grade for arbitrary CAD-like meshes. The main missing
piece is a complete production validation stack: exact-arithmetic geometric
predicates, local error envelopes, full feature-graph ownership/conflict
resolution for large multi-junction curves, and stronger validation against
labeled datasets.

## What Is Strong Now

1. QEM/line-quadric structure is already modular enough to support stronger
   policies without rewriting the whole simplifier.
2. Circular feature loops are detected and can be projected during feature
   collapses.
3. The feature graph now recovers circular cycles that pass through junction
   chains and bounded small-component cycles with multiple junctions, instead of
   only reporting simple degree-2 loops.
4. Collapse rejection is more observable. The report distinguishes feature,
   boundary, topology, normal-flip, triangle-quality, local-intersection, and
   curve-budget rejects.
5. The simplifier can now preserve boundary topology while allowing legal
   same-boundary-edge collapses when requested.
6. Industrial-safe mode exposes conservative boundary, quality, normal, and
   local-intersection guards through the CLI and C API.
7. Local self-intersection checks now use an incrementally updated spatial hash
   instead of scanning every active face for every tested collapse, and include
   a projected 2D coplanar-triangle overlap branch.
8. Ellipse features carry primitive parameters into simplification, are hard
   protected by default, and project placements back to the detected ellipse.
9. Other non-circular feature collapses project placement back to the nearest
   segment of the original feature polyline, or to the tangent line when curve
   data is degenerate.
10. Polygonal feature curves now have a raw-placement deviation budget:
   same-loop collapses that would require a large projection back to the
   original polyline can be rejected before placement projection.
11. New behavior is protected by gtests:
   - circular loop recovery from a branched feature graph,
   - polygonal cycle recovery through three or more junctions,
   - strict triangle-quality rejection,
   - strict normal-deviation rejection,
   - hard boundary preservation,
   - spatial-indexed local self-intersection rejection, including coplanar
     overlap and shared-edge non-regression cases,
   - elliptical primitive projection and post-simplification ellipse error,
   - non-circular feature projection,
   - polygonal feature-curve budget rejection.

## Main Algorithm Gaps

### 1. Boundary preservation is currently too strict

Status: mostly addressed for topological boundary simplification.

`preserveBoundary=true` now allows collapses on the current boundary edge when
both endpoints are original boundary vertices, while rejecting boundary-to-
interior collapses and boundary-vertex pairs that are not current boundary
edges. The placement is projected back to the boundary edge segment before the
normal, topology, and quality checks run.

The remaining limitation is geometric rather than topological: boundary curves
are simplified as polylines, not yet as fitted CAD curves with arclength,
curvature, or primitive-aware placement.

Recommended direction:

- Classify boundary edges and boundary loops.
- Allow boundary-edge collapses when both endpoints are on the same boundary
  loop and the placement stays on the boundary curve.
- Reject boundary-to-interior collapses and collapses that merge two different
  boundary loops.
- Preserve loop count, loop orientation, and open-boundary manifoldness.

GTest acceptance:

- Done: an open rectangular grid with boundary preservation can reduce boundary
  edge and vertex count by legal boundary-edge collapses.
- Done: boundary-to-interior collapse candidates are rejected and reported.
- Done: a hole-plane fixture keeps separate boundary components after
  simplification.
- Remaining: add primitive-aware boundary placement for circular/elliptic/spline
  boundary curves.

### 2. Local self-intersection guard is still approximate

Status: first spatial-indexed local guard with coplanar overlap handling
implemented.

The current legality checks cover link condition, degenerate/duplicate local
faces, normal deviation, triangle quality, and optional local self-intersection
rejection. Newly formed local triangles are checked against non-adjacent active
triangles using an incrementally updated spatial hash, AABB filtering,
segment-triangle tests, and a 2D projected coplanar-triangle overlap predicate.

Recommended direction:

- Keep the local candidate patch from faces incident to the kept/removed vertex.
- Replace the current uniform spatial hash with a reusable AABB tree if large
  meshes show highly non-uniform triangle sizes.
- Replace epsilon-based coplanar tests with exact predicates if downstream CAD
  models require formal robustness under extreme scale or near-degeneracy.

GTest acceptance:

- Done: a synthetic near-overlap mesh rejects the collapse that would create a
  local self-intersection.
- Done: a synthetic coplanar-overlap collapse is rejected.
- Done: coplanar but separated triangles and legal shared coplanar edges do not
  trigger false self-intersection rejects.
- Done: self-intersection rejections have a dedicated report counter.
- Remaining: add large-mesh BVH performance coverage.

### 3. Feature graph recovery is not general enough

Status: first bounded cycle-basis recovery implemented for small feature
components.

The detector now has two recovery layers beyond simple degree-2 loop tracing:
pairwise junction-chain recovery for circular loops and a bounded DFS-based
small cycle-basis pass for small/medium feature components. This recovers
polygonal cycles that pass through three or more junctions. Complex CAD feature
graphs can still contain large components, nested loops, partial boundaries,
tangential branches, and shared feature ownership conflicts that need a more
explicit graph object.

Recommended direction:

- Promote feature edges into an explicit graph object.
- Extend the current bounded simple-cycle extraction into that explicit graph
  object.
- Score candidate cycles by primitive fit, midpoint adherence, continuity,
  boundary ratio, and edge saliency.
- Keep top candidates and resolve vertex ownership conflicts deterministically.

GTest acceptance:

- Done: a polygonal graph with three junctions and attached branches recovers
  the intended closed cycle.
- A graph with three or more junctions recovers all expected circular loops.
- A graph with a spur branch does not create a false circular loop.
- Nested or adjacent loops are both detected without assigning impossible loop
  IDs to shared vertices.

### 4. Non-circular features need stronger curve models

Circular holes and near-circles have explicit detection and projection. Polyline,
ellipse, spline-like crease, and ridge/valley features now receive stronger
placement constraints. Ellipse loop parameters are copied from detection into
simplification, ellipse collapses are hard-protected by default, and placements
project back to the fitted ellipse. Other non-circular feature collapses project
back to the nearest segment on the original feature polyline, or to the local
tangent line if the curve data is degenerate. A new
`maxFeatureCurveDeviationRatio` budget rejects polygonal feature collapses whose
raw QEM placement drifts too far from the original polyline before projection.
Spline-like features still do not have full primitive-aware fitting or
arclength/error-budgeted resampling.

The 2026-07-02 data probe also showed that even circular feature preservation
needed stronger graph recovery. The detector now separates weak feature evidence
from strong loop-tracing edges and has a conservative circular vertex-cluster
fallback for fragmented low-vertex circular loops. That fallback is deliberately
disabled when normal-tensor feature edges are present and capped to small
candidate sets; it is a CAD circular-loop repair, not a general tensor-voting
curve extractor. The fallback is also bounded to 32768 deterministic
three-point circle scans so fragmented feature graphs have a predictable
runtime ceiling before the pipeline returns to the traced graph loops and
ordinary primitive-fit validation. The public feature-detection API now also
documents that split explicitly: graph-supported CAD/STL loops are the primary
path, tensor evidence is a weak-feature signal, and circular vertex-cluster
recovery is a bounded repair rather than a general voting detector.

Recommended direction:

- Represent non-circular feature curves as ordered polylines with local tangent,
  arclength, and optional primitive fit.
- Extend nearest-point-on-polyline projection to arclength-aware spline
  projection.
- Add curve quadrics or anisotropic penalties that penalize drift normal to the
  feature curve more than drift along the curve.
- For ellipses, add stricter center, axis/radius, and phase-continuity budgets.
- Track per-loop error budgets and remaining vertex budgets instead of applying
  one global ratio to all polygonal curves.

GTest acceptance:

- A Fandisk-style hard crease remains traceable after simplification.
- Done: an elliptical hole triggers primitive feature placement projection.
- Done: a simplified elliptical hole remains detectable and keeps axis ratio,
  ellipse RMS, and plane RMS within thresholds on the fixture.
- Done: a polygonal feature loop with an internal chord rejects an off-curve raw
  collapse placement before projection.
- Done: circular vertex-cluster recovery has a bounded three-point scan budget
  instead of exhaustive enumeration over all candidate triples.
- Remaining: aggressive simplification keeps ellipse center, axis/radius, and
  phase-continuity within thresholds.
- A polyline crease can lose vertices but keeps endpoints, tangent continuity,
  and feature-edge recall.

### 5. Normal tensor feature evidence is still lightweight

The current normal tensor path is useful, but the literature repeatedly warns
that single-scale local normal measures are sensitive to sampling density,
noise, and mesh anisotropy.

Recommended direction:

- Normalize neighborhoods by local edge length or area, not only by fixed
  adjacency depth.
- Evaluate multiple radii and keep features that persist across nearby scales.
- Use robust face-normal aggregation with area and distance weighting.
- Separate flat noisy regions from true creases using eigenvalue ratios plus
  edge-alignment checks.

GTest acceptance:

- A noisy flat plane has low false-positive feature edges.
- A noisy ridge fixture keeps high feature recall.
- Subsampled versions of the same CAD fixture produce similar feature counts and
  loop recovery.

### 6. Validation still lacks precision/recall style gates

The test suite has good unit-level fixtures, but industrial algorithm confidence
needs dataset-style metrics. The current docs and tests should move from "it
runs and detects something" to quantifiable feature retention.

The 2026-07-02 probe produced concrete gates that are now partially covered by
gtests: circular-loop rediscovery on `coaxial_hole_plate.obj`, strict
triangle-quality mode on a low-quality Thingi10K output, local self-intersection
rejection on a synthetic near-overlap mesh, and boundary preservation on open
boundary fixtures. The SDK headers now document the same production split used
in this roadmap: QEM and line quadrics rank candidates, then explicit hard
filters account for topology, boundary, normal, quality, local-error,
self-intersection, and feature-policy rejections.

Recommended direction:

- Maintain labeled fixtures for circles, ellipses, holes, creases, boundaries,
  and weak features.
- Compute feature-edge precision/recall, loop recall, geometric drift, Hausdorff
  proxy, normal error, triangle quality, and non-manifold count.
- Add parameter-sensitivity tests around line weight, feature angle, tensor
  threshold, normal-deviation limit, and quality threshold.

GTest acceptance:

- A small labeled CAD fixture reports feature precision/recall above fixed
  thresholds.
- Parameter sweeps do not regress below baseline face count, topology, and
  feature-retention gates.
- Rejection counters remain internally consistent:
  `rejectedCollapses == feature + boundary + topology + normal + quality +
  self_intersection + curve_budget`.
- Done: shared GTest support checks rejection-counter consistency for every
  `simplifyWithReport` fixture, including primitive/generic feature-rejection
  subcounts.

## Recommended Next Implementation Order

1. First-class feature graph ownership during collapse.
   The detector can now recover more cycles, but simplification still needs to
   preserve loop adjacency and shared-junction ownership explicitly.

2. Arclength/error-budgeted spline and polyline constraints.
   The current implementation has nearest-polyline projection and a raw
   placement budget; the next step is per-loop arclength, tangent, endpoint, and
   curvature budgets.

3. Primitive-aware boundary placement.
   This upgrades boundary simplification from polyline topology preservation to
   CAD-like curve preservation.

4. AABB-tree-backed local legality checks if spatial hash performance degrades
   on meshes with extreme triangle-size variation.

5. Multiscale normal tensor feature detection.
   This improves robustness on noisy or unevenly tessellated meshes.

6. Dataset-style precision/recall validation.
   This turns algorithm improvements into release gates instead of visual
   inspection.

## Open-Source Patterns To Borrow

- CGAL SMS: separate cost, placement, and stop predicates; constrained edge maps
  for edges that must not collapse.
- OpenMesh decimation: independent binary legality modules and continuous cost
  modules. This is a good pattern for self-intersection, normal flip, boundary,
  and quality guards.
- libigl qslim: compact QEM reference for edge-collapse queue behavior.
- MeshLab/VCGLib: practical decimation filters with boundary, normal, and quality
  safeguards.

## Release Bar For "Industrial Algorithm" Status

A future score of 9.0+ should require all of the following:

- Boundary loops can be simplified without being destroyed.
- Local self-intersections are rejected.
- Circular, elliptical, and polyline features are constrained, not just reported.
- Complex feature graphs recover expected loops around junctions.
- Noisy and non-uniform fixtures have stable feature detection across scales.
- Tests include precision/recall and parameter-sensitivity gates.
- C++ and C API expose the same important options and report diagnostics.
