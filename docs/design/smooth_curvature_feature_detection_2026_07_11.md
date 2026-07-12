# Deterministic Smooth Curvature Feature Detection

Date: 2026-07-11 (updated 2026-07-12: cubic Monge fit, analytic extremality,
Ohtake edge zero-crossing criterion, Yoshizawa component-strength filter;
updated 2026-07-13: per-crossing cyclideness gate against spurious responses
on Dupin cyclides, exposed by the analytic torus fixture)

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

1. Build a k-ring neighborhood and an area-weighted local normal, using the
   shared `FeatureDetectionCache` (neighbors, edge info, average edge lengths
   are built once per detection run and reused across all evidence stages).
2. Normalize coordinates by the local average edge length times the ring count.
3. Fit the cubic Monge patch (Yoshizawa, M021)

   `w = a*u^2 + b*u*v + c*v^2 + d*u + e*v
        + c0*u^3 + c1*u^2*v + c2*u*v^2 + c3*v^3`

   with Gaussian distance weights and deterministic Huber reweighting. The
   nine-unknown system is solved through incrementally accumulated weighted
   normal equations (upper triangle only), which replaces one dense QR per
   robust iteration; the radius normalization keeps the 9x9 system well
   conditioned. Fits with fewer than nine usable neighbors are rejected as
   underdetermined.
4. Form the first and second fundamental forms from the quadratic block and
   solve the generalized self-adjoint eigenproblem for two signed principal
   curvatures and directions.
5. Evaluate the analytic extremality `e_i = grad(kappa_i) . t_i` for each
   principal direction directly from the cubic block as the third-order
   directional form `e ~ 6*(c0*t1^3 + c1*t1^2*t2 + c2*t1*t2^2 + c3*t2^3)`.
   No finite differencing against neighbor curvatures is involved.
6. Classify ridge/valley support with the Ohtake edge zero-crossing criterion
   (M011 Eq. 3-4): principal directions form a line field, so the neighbor's
   tangent and extremality are sign-synchronized first; a vertex supports a
   candidate only when (a) both edge endpoints pass the curvature dominance
   test (`kappa_max > |kappa_min|` for ridges, `kappa_min < -|kappa_max|` for
   valleys), (b) the extremality changes sign along an incident edge roughly
   following the principal direction, and (c) the first-order maximum test
   holds at both endpoints (the practical replacement Ohtake recommends for
   the second-order derivative test). Sub-vertex ownership uses Ohtake's
   inverse interpolation: only the endpoint with the smaller |e| claims the
   crossing, which keeps the detected band one vertex wide.
7. Gate each accepted crossing by its cyclideness (Yoshizawa, M021 Eq. 5-6).
   On a Dupin cyclide (sphere, cylinder, cone, torus) the extremality field
   vanishes identically, so a detected sign change there is discretization
   noise rather than a curvature extremum: the whole extremal circle is a
   stationary set of kappa and carries no crest. Near a genuine crest |e|
   grows like |de/dt| times the distance off the crest line, so the mean
   endpoint magnitude of |e| — which linearly interpolates Yoshizawa's
   cyclideness C at the crossing — measures the extremal significance. The
   gate requires

   `0.5 * (|e_center| + |e_neighbor|) >= kMinCrossingCyclidenessRatio * kappa^2`

   with `kMinCrossingCyclidenessRatio = 0.15`. Dividing by kappa^2 makes the
   ratio dimensionless (both e and kappa^2 scale as 1/L^2 and are estimated
   in the same radius-normalized units) and exactly invariant under uniform
   scaling: it asks whether kappa varies by at least a fixed fraction of
   itself over one curvature radius along its own line of curvature — zero on
   cyclides, O(1) on true crests. Measured landmarks (2026-07): true crests
   on the Gaussian ridge/bump fixtures sit at ratios >= 0.38 (p10) with
   medians well above 1, while the spurious inner-side valley band of a torus
   peaks at 0.06 across 24-48 minor segments, so 0.15 keeps a 2.5x margin to
   both populations. Before this gate the torus produced a persistent
   spurious valley with max persistent scores 0.097/0.038/0.026/0.011 at
   24/32/36/48 minor segments against the working threshold 0.008; after it
   the torus is exactly silent (max persistent score 0.0) at all tested
   densities, and the Gaussian ridge response is bit-identical.
8. Score the candidate with scale-normalized curvature magnitude, anisotropy,
   zero-crossing strength (mean |e| times the tangential edge extent, in
   radius-normalized units), and fit residual quality.
9. Keep support only when sign and curve tangent agree across nearby scales and
   the response survives to the coarsest requested neighborhood.
10. Convert persistent vertices to mesh-edge evidence only when both endpoints
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

Graph cleanup additionally exposes `featureGraphMinWeakSpurStrength`
(default 0.0, C++ `FeatureOptions` only, no CLI/C ABI binding yet). When
positive, dangling weak-evidence chains are judged by the dimensionless
Yoshizawa curve strength `T = (integral ds) * (integral strength ds)` — ds in
local average-edge-length units, per-edge strength as the persistence score
divided by the matching channel threshold — instead of the legacy edge-count
cap. Long-but-faint smooth ridges then survive cleanup while short-but-strong
noise spikes are pruned. The default keeps legacy behavior exactly.

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

The implementation is anchored by the local Ohtake (M011) and Yoshizawa (M021)
crest-line recipes — cubic fit, analytic extremality, edge zero-crossing,
curve-strength filtering — together with the M014/M042-M044 curvature and
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

Since 2026-07 the path is additionally validated against analytic ground-truth
fixtures (`tests/support/AnalyticFixtures.{h,cpp}`,
`tests/unit/feature_detection/feature_detection_analytic_tests.cpp`): spheres,
cylinders, and tori — all Dupin cyclides with identically vanishing
extremality — must stay exactly silent, and the Gaussian ridge sheet checks
the quantitative crest-curvature accuracy of the Monge fit (|f''(0)| = 2hs,
median relative deviation bound 15% derived from the neighborhood-averaging
bias). The torus fixture is what exposed the spurious inner-side valley band
fixed by the cyclideness gate above.

Remaining limitations:

- one dominant smooth tangent is stored per vertex, so very tight smooth
  multi-branch junctions remain difficult;
- the detector does not yet perform global Hough or winding-number curve
  recovery;
- incremental neighborhood updates after split/collapse are not implemented;
- scan-specific benchmark fixtures with labeled ridge/valley curves are still
  needed before enabling this path by default;
- the multiscale fit costs about 6 s on a 16k-face sphere, so the channel is
  deliberately excluded from the fast-suite performance guard
  (`tests/unit/perf/pipeline_perf_guard_tests.cpp`); its functional behavior
  is covered on smaller analytic fixtures instead.
