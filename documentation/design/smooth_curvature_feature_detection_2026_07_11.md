# Deterministic Smooth Curvature Feature Detection

Date: 2026-07-11 (updated 2026-07-12: cubic Monge fit, analytic extremality,
Ohtake edge zero-crossing criterion, Yoshizawa component-strength filter;
updated 2026-07-13: per-crossing cyclideness gate against spurious responses
on Dupin cyclides, exposed by the analytic torus fixture; updated 2026-07-15:
current-source audit, pure supporting-scale vote semantics, exact edge acceptance,
source locator, CLI/simplifier boundary, and current performance evidence;
updated again 2026-07-15 with opt-in stable-scale selection and stability diagnostics)

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

## Current source contract

The implementation, not an earlier design sketch, defines the current behavior.
The important source locations are:

| Responsibility | Source symbol | Current location |
| --- | --- | --- |
| Public controls and per-vertex result | `FeatureOptions`, `SmoothCurvatureOptions`, `SmoothCurvatureVertex` | `include/algorithms/feature_detection/FeatureTypes.h` |
| Local normal and k-ring gathering | `computeAreaWeightedVertexNormals`, `gatherNeighborhood` | `src/feature_detection/SmoothCurvature.cpp` |
| Cubic Monge fit and robust solve | `fitScale` | `src/feature_detection/SmoothCurvature.cpp` |
| Per-scale ridge/valley classification | `classifyScaleCandidate` | `src/feature_detection/SmoothCurvature.cpp` |
| Cross-scale support and reference-scale selection | `computeSmoothCurvatureFeaturesCached` | `src/feature_detection/SmoothCurvature.cpp` |
| Vertex evidence to mesh-edge evidence | `smoothCurvatureEdgeCandidate` | `src/feature_detection/FeatureEvidence.cpp` |
| Graph cleanup/consolidation | `cleanupTraceGraph`, `consolidateFeatureGraph` | `FeatureGraphCleanup.cpp`, `FeatureGraphCompatibility.cpp`, `FeatureGraphConsolidation.cpp` |
| CLI option binding | `parseFeatureOptions` | `apps/CliOptionBinding.cpp` |

An earlier revision required the coarsest requested scale to support a candidate.
That veto was removed because it suppressed real features whose physical width
is smaller than the coarsest neighborhood. The current implementation is a pure
supporting-scale vote: supporting scales need not be adjacent and the coarsest
scale is not special. The regression
`FeatureDetectionAnalytic.NarrowRidgeOnDenseSheetSurvivesCoarsestScale`
protects this behavior.

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
9. Select a reference scale. The default keeps the highest-score scale for
   backward compatibility. With `useStableScaleSelection`, valid candidates
   are ranked by cross-scale sign/tangent/score consistency and candidates below
   `minScaleStability` are rejected. Every
   requested scale casts one support vote when all of the following hold:
   (a) it is valid, (b) its score is at least
   `max(persistenceThreshold, 0.30 * referenceScore)`, (c) its signed ridge/valley
   kind matches the reference, and (d) the absolute tangent dot product with
   the reference is at least `minTangentConsistency`. Supporting scales do not
   have to be adjacent, and the coarsest requested scale does not have to vote.
   The implementation then computes

   `persistenceRatio = persistentScales / scaleCount`

   `persistentFeatureScore =
      (0.65 * referenceScore + 0.35 * meanSupportedScore)
      * persistenceRatio * meanSupportedAlignment`

   Unsupported scales contribute zero to `averageFeatureScore`, whose stored
   value is `supportedScoreSum / scaleCount`.
10. Convert persistent vertices to mesh-edge evidence only when both endpoints
    agree on sign, tangent, scale support, and edge alignment.

The resulting score is dimensionless. Uniformly scaling a mesh does not require
retuning the curvature threshold.

## Mesh-edge acceptance

Per-vertex evidence is diagnostic until it is converted into an explicit mesh
edge by `smoothCurvatureEdgeCandidate`:

| Gate | Exact current rule |
| --- | --- |
| Strong-evidence exclusion | Reject when the edge is already boundary, dihedral, or non-manifold. Also reject when either endpoint was marked as a discrete-feature vertex, producing a one-vertex exclusion zone around strong CAD evidence. |
| Scale count | `min(endpointA.persistentScales, endpointB.persistentScales) >= smoothCurvatureMinPersistentScales`. |
| Score | Both endpoint persistent scores must reach `smoothCurvatureFeatureThreshold`. |
| Signed kind | Both endpoints must be nonzero and agree: ridge with ridge or valley with valley. |
| Edge alignment | The edge direction must align with both endpoint curve tangents: `min(|d.tA|, |d.tB|) >= smoothCurvatureMinEdgeAlignment`. |
| Endpoint tangent consistency | `|tA.tB| >= smoothCurvatureMinTangentConsistency`. |

This is intentionally stricter than the current normal-tensor edge rule, which
accepts direction alignment when the better-aligned endpoint reaches its
threshold. A mesh edge may still carry both normal-tensor and smooth-curvature
flags because the two weak strategies are evaluated independently after the
strong-evidence gates.

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
- `smoothCurvatureUseStableScaleSelection` (default false)
- `smoothCurvatureMinScaleStability` (default 0.0)

Graph cleanup additionally exposes `featureGraphMinWeakSpurStrength`
(default 0.0) through C++ `FeatureOptions`, C++ `SimplifyOptions`, both CLI
option tables (`--feature-graph-min-weak-spur-strength`), and the size-aware C
ABI tail. When
positive, dangling weak-evidence chains are judged by the dimensionless
Yoshizawa curve strength `T = (integral ds) * (integral strength ds)` — ds in
local average-edge-length units, per-edge strength as the persistence score
divided by the matching channel threshold — instead of the legacy edge-count
cap. Long-but-faint smooth ridges then survive cleanup while short-but-strong
noise spikes are pruned. The default keeps legacy behavior exactly.

The path is opt-in because clean CAD/STL hard-edge detection and noisy/free-form
smooth-feature detection require different validation sets. Existing
simplification behavior remains unchanged unless the smooth channel is enabled
or a caller supplies a precomputed analysis with smooth features.

The CLI exposes the same controls to `feature-report`, `feature-benchmark`,
`feature-compare`, and `simplify`. On `simplify`,
`--smooth-curvature-features` also enables `preserveFeatureCurves`, so the
detected graph is consumed by the protection policy instead of being computed
and discarded. Stable-scale controls are exposed as
`--smooth-curvature-stable-scale` and
`--smooth-curvature-min-scale-stability`.

## Diagnostics

`FeatureAnalysis` now reports:

- smooth-curvature scored vertices and graph edges;
- maximum raw and persistent scores;
- mean local scale, persistence, and scale stability;
- per-vertex `selectedScale` and `scaleStability`;
- per-edge `smoothCurvature` ownership;
- per-component smooth-edge count and mean curvature persistence.

Component confidence treats normal-tensor and smooth-curvature evidence as
separate weak support. Hard evidence remains dominant.

The per-source edge counters are evidence-channel counts. Because one graph edge
may carry both weak flags, `normalTensorFeatureEdges + smoothCurvatureFeatureEdges`
is not guaranteed to equal the number of unique weak graph edges. Likewise,
cleanup bridges are appended to `FeatureGraph` but are not included in the
original `featureEdges` evidence count.

`featureComponentMinConfidence` is a reporting threshold only: it controls the
`highConfidenceFeatureComponents` counter and does not delete a component or
turn hard protection on/off. Simplification continuously scales the feature
curve soft quadric by `0.35 + 0.65 * confidence` in
`src/simplification/Quadrics.cpp`.

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
- the channel is deliberately excluded from the mandatory 16k-face fast-suite
  wall-clock guard because its cost depends strongly on requested rings, scale
  count, robust iterations, and local valence. The disabled manual Release
  benchmark (`FeatureDetectionPerf.DISABLED_AnalyzeTiming`) measured about
  92 ms for the three-scale/two-robust-pass smooth stage on the current
  8192-face bump fixture on the 2026-07-15 development machine. This number is
  a local observation, not an API performance guarantee; mandatory tests keep
  functional coverage on analytic fixtures and a separate fast guard for the
  default dihedral + normal-tensor path.

## Simplification boundary

`SimplifyOptions` mirrors the smooth-curvature controls, including stable-scale
selection and minimum stability, plus
`featureGraphMinWeakSpurStrength`; `featureOptionsFromSimplifyOptions` maps them
without changing thresholds. C++ callers enable both
`preserveFeatureCurves = true` and `useSmoothCurvatureFeatures = true`. The CLI
does the first part automatically when `--smooth-curvature-features` is present.
`SimplifyReport`, simplify stdout/metrics CSV, and the size-aware C ABI report
carry smooth edge/scored-vertex/persistence diagnostics together with winding,
cleanup-cap, and circular-recovery diagnostics.

The precomputed `FeatureAnalysis` overload remains supported when detection is
shared with repair/remeshing/validation. Both routes converge at
`buildFeatureGuidanceFromAnalysis`; feature policy therefore remains outside the
topology-edit loop, consistent with the constrained-edge/data-adapter practice
represented by CGAL PMP and OpenMesh.
