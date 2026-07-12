# Deterministic Smooth Curvature Feature Detection

Date: 2026-07-11

This upgrade is limited to triangle and polygon surface meshes. It does not
introduce B-Rep entities, solid modeling, CAD feature trees, learned scoring,
neural networks, or training data.

## Why a separate evidence path

Boundary, non-manifold, and dihedral edges are discontinuities in the discrete
mesh. Smooth ridges and valleys are differential events on a locally smooth
surface. Combining their raw scores makes thresholds hard to interpret and lets
noise compete directly with strong topology evidence.

ManuMesh therefore keeps two paths:

1. Hard evidence: boundary, non-manifold, and dihedral edges.
2. Weak smooth evidence: normal tensor and multiscale quadric curvature.

Both paths meet only at the explicit `FeatureGraph`, where source ownership,
continuity, junctions, cleanup, components, and confidence are available to
downstream simplification and future remeshing adapters.

## Algorithm

For every vertex and every requested topological scale:

1. Build a k-ring neighborhood and an area-weighted local normal.
2. Normalize coordinates by the local average edge length times the ring count.
3. Fit the Monge patch

   `w = a*u^2 + b*u*v + c*v^2 + d*u + e*v`

   with distance weights and deterministic Huber reweighting.
4. Form the first and second fundamental forms and solve the generalized
   self-adjoint eigenproblem for two signed principal curvatures and directions.
5. For each principal direction, sample normal curvature on both sides. Accept a
   ridge or valley only when the center is a signed directional extremum.
6. Score the candidate with scale-normalized curvature magnitude, anisotropy,
   extremum contrast, and fit residual quality.
7. Keep support only when sign and curve tangent agree across nearby scales and
   the response survives to the coarsest requested neighborhood.
8. Convert persistent vertices to mesh-edge evidence only when both endpoints
   agree on sign, tangent, scale support, and edge alignment.

The resulting score is dimensionless. Uniformly scaling a mesh does not require
retuning the curvature threshold.

## Public controls

`FeatureOptions` exposes an opt-in smooth path:

- `useSmoothCurvatureFeatures`
- `smoothCurvatureFeatureThreshold`
- `smoothCurvatureMinEdgeAlignment`
- `smoothCurvatureMinTangentConsistency`
- `smoothCurvatureBaseNeighborhoodRings` (default 2; one-ring fits are too noise-sensitive for general use)
- `smoothCurvatureScaleCount`
- `smoothCurvatureMinPersistentScales`
- `smoothCurvatureRobustFitIterations`

The path is opt-in because clean CAD/STL hard-edge detection and noisy/free-form
smooth-feature detection require different validation sets. Existing
simplification behavior remains unchanged unless a caller supplies a precomputed
analysis with smooth features.

The CLI exposes the same controls to `feature-report`, `feature-benchmark`, and
`feature-compare` through `--smooth-curvature-*` options.

## Diagnostics

`FeatureAnalysis` now reports:

- smooth-curvature scored vertices and graph edges;
- maximum raw and persistent scores;
- mean local scale and persistence;
- per-edge `smoothCurvature` ownership;
- per-component smooth-edge count and mean curvature persistence.

Component confidence treats normal-tensor and smooth-curvature evidence as
separate weak support. Hard evidence remains dominant.

## Open-source implementation lessons

- libigl: use a local tangent frame, k-ring or radius neighborhoods, quadric
  fitting, and explicit principal directions. ManuMesh adds robust reweighting,
  scale normalization, directional extrema, and persistence.
- pmp-library: keep curvature as a mesh property with explicit boundary and
  smoothing policy. ManuMesh keeps scoring separate from graph ownership.
- CGAL PMP: materialize feature edges as an explicit constrained edge map before
  patch or remeshing operations. `FeatureGraphEdge` is the ManuMesh equivalent.
- OpenMesh: keep topology/status/property storage separate from feature policy.
  The detector depends on core mesh queries and does not own edit operations.

## Recent deterministic literature

The implementation is anchored by the local M014/M042-M044 curvature and
quadric references, then checked against recent non-AI work:

| Year | Work | Engineering lesson used here |
| --- | --- | --- |
| 2017/2018 | Yamakawa and Shimada, *Feature Edge Extraction Via Angle-Based Edge Collapsing and Recovery*, DOI `10.1115/1.4037227` | Multiscale simplification can expose small fillet centers; a single local angle threshold is not enough. |
| 2019 | Lu et al., *Feature Curve Network Extraction via Quadric Surface Fitting*, M044 | Quadric fits must feed curve continuity and junction/network reasoning. |
| 2020 | Romanengo et al., *HT-Based Identification of 3D Feature Curves and Their Insertion into 3D Meshes*, DOI `10.1016/j.cag.2020.05.012` | Detected curves should become explicit mesh constraints rather than remain detached samples. |
| 2024 | Xu et al., *CWF: Consolidating Weak Features in High-quality Mesh Simplification*, M026 | Weak evidence needs consolidation and confidence before hard downstream protection. |
| 2025 | Cai et al., *Feature Line Extraction Based on Winding Number*, DOI `10.1016/j.gmod.2025.101296` | Global curve evidence is a useful future complement when local differential evidence fragments. |

No neural or learned method is used or recommended by this implementation.

## Tests and remaining limits

Tests cover exact-plane rejection, smooth-bump response, uniform-scale
invariance, multiscale persistence, noisy-response suppression at the final
feature-graph stage, graph-source ownership, parameter validation, and existing
hard-feature regression suites. Raw curvature candidates remain diagnostic input;
the graph constraints decide which responses become reusable feature edges.
The labeled Gaussian ridge/valley fixture measures precision and segment recall
with a one-local-edge curve-drift tolerance, matching how remeshing constraints
consume a discrete approximation rather than an exact analytic curve.

Remaining limitations:

- one dominant smooth tangent is stored per vertex, so very tight smooth
  multi-branch junctions remain difficult;
- the detector does not yet perform global Hough or winding-number curve
  recovery;
- incremental neighborhood updates after split/collapse are not implemented;
- scan-specific benchmark fixtures with labeled ridge/valley curves are still
  needed before enabling this path by default.
