/**
 * @file include/algorithms/feature_detection/FeatureTypes.h
 * @brief Declares feature types facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "core/Mesh.h"

#include <utility>
#include <vector>

namespace manumesh::feature {

/// Upper bounds accepted by validateFeatureOptions for iteration/scale
/// parameters. The implementations never evaluate more rings, scales, or
/// iterations than these limits, so out-of-range requests are rejected up
/// front instead of being silently clamped.
inline constexpr int kMaxNormalTensorSmoothingIterations = 8;
inline constexpr int kMaxNormalTensorScaleCount = 8;
inline constexpr int kMaxSmoothCurvatureBaseNeighborhoodRings = 4;
inline constexpr int kMaxSmoothCurvatureScaleCount = 6;
inline constexpr int kMaxSmoothCurvatureRobustFitIterations = 4;
inline constexpr int kMaxFeatureNormalFilterIterations = 16;

/// Optional normal-domain preprocessing for noisy triangle meshes.
///
/// The filter keeps the input topology and vertex positions unchanged. It
/// alternates an edge indicator with area-weighted face-normal relaxation, so
/// feature evidence can consume stabilized normals without silently replacing
/// the caller's mesh.
struct FeatureNormalFilterOptions {
    bool enabled = false;           ///< Enables preprocessing in the full detector.
    int iterations = 4;             ///< Relaxation passes in [0, kMaxFeatureNormalFilterIterations].
    double angleSigmaDeg = 20.0;    ///< Bilateral angular bandwidth in degrees.
    double preserveAngleDeg = 50.0; ///< Discontinuities at or above this angle are frozen.
    double relaxation = 0.8;        ///< Blend toward the compatible-neighbor mean in [0,1].
};

/// Component-level recovery after local feature-graph cleanup.
struct FeatureGraphConsolidationOptions {
    bool enabled = false;           ///< Enables component-level recovery after cleanup.
    double maxGapLengthRatio = 3.0; ///< Maximum endpoint gap in local edge-length units.
    double minAlignment = 0.75;     ///< Minimum absolute continuation-tangent dot product.
};

/// Optional face partition induced by active feature-graph edges.
struct SurfacePatchOptions {
    bool enabled = false;            ///< Enables face segmentation after graph recovery.
    bool includeWeakEvidence = true; ///< Treat tensor/curvature-only edges as patch boundaries.
};

/// Fitted primitive type for one detected feature loop.
enum class FeaturePrimitiveType {
    Unknown,
    Circle,
    NearCircle,
    Ellipse,
    PolygonalLoop,
};

/// Parameters for crease, boundary, and feature-loop detection.
///
/// The detector is tuned for CAD/STL-style meshes first: explicit boundary and
/// dihedral evidence form the feature graph, while tensor evidence is a
/// secondary signal for weak creases. Thresholds should therefore be chosen
/// against the mesh scale and provenance instead of reused blindly across
/// scanned/noisy and clean CAD inputs.
struct FeatureOptions {
    /// Dihedral angle threshold for hard feature edges, in degrees.
    double featureAngleDeg = 40.0;
    /// Dihedral angle used when tracing feature edges into loop ownership.
    /// Negative means "reuse featureAngleDeg".
    double loopTraceAngleDeg = -1.0;
    /// Relative radial tolerance used when validating circular loops.
    double circleFitRelativeThreshold = 0.05;
    /// Relative residual tolerance used when validating elliptical loops.
    double ellipseFitRelativeThreshold = 0.05;
    /// Axis-ratio tolerance below which an ellipse is treated as near-circular.
    double nearCircleAxisRatioTolerance = 0.08;
    /// Minimum vertices required by recovered-cycle acceptance and primitive fitting.
    /// Directly traced open chains and closed traces are still reported below this
    /// threshold; they simply do not receive a primitive fit through that path.
    int minFeatureLoopVertices = 8;
    /// Enables tensor-derived weak feature candidates in addition to graph edges.
    bool useNormalTensorFeatures = true;
    /// Minimum tensor saliency score for weak feature classification.
    double normalTensorFeatureThreshold = 0.16;
    /// Minimum edge/tangent alignment for accepting tensor-derived edge evidence.
    double normalTensorMinEdgeAlignment = 0.45;
    /// One-ring normal smoothing passes before tensor scoring.
    /// Valid range: [0, kMaxNormalTensorSmoothingIterations].
    int normalTensorSmoothingIterations = 0;
    /// Number of tensor scales sampled for weak feature scoring.
    /// Valid range: [1, kMaxNormalTensorScaleCount].
    int normalTensorScaleCount = 1;
    /// Minimum scales that must support a tensor edge candidate.
    /// Valid range: [1, normalTensorScaleCount].
    int normalTensorMinPersistentScales = 1;
    /// Enables deterministic smooth ridge/valley evidence from local quadric fits.
    /// Kept opt-in because CAD/STL hard-feature and scanned/free-form regimes need
    /// different thresholds and validation data.
    bool useSmoothCurvatureFeatures = false;
    /// Minimum scale-normalized smooth-feature score.
    double smoothCurvatureFeatureThreshold = 0.015;
    /// Minimum alignment between a mesh edge and the recovered curve tangent.
    double smoothCurvatureMinEdgeAlignment = 0.55;
    /// Minimum cross-scale and endpoint tangent agreement.
    double smoothCurvatureMinTangentConsistency = 0.65;
    /// Base topological radius used by local quadric fitting.
    /// Valid range: [1, kMaxSmoothCurvatureBaseNeighborhoodRings].
    int smoothCurvatureBaseNeighborhoodRings = 2;
    /// Number of successively larger quadric-fit neighborhoods.
    /// Valid range: [1, kMaxSmoothCurvatureScaleCount].
    int smoothCurvatureScaleCount = 3;
    /// Minimum scales that must support a smooth feature candidate.
    /// Valid range: [1, smoothCurvatureScaleCount].
    int smoothCurvatureMinPersistentScales = 2;
    /// Deterministic robust reweighting passes for local quadric fitting.
    /// Valid range: [0, kMaxSmoothCurvatureRobustFitIterations].
    int smoothCurvatureRobustFitIterations = 2;
    /// Selects the reference fit scale by cross-scale stability instead of raw
    /// peak score alone.
    bool smoothCurvatureUseStableScaleSelection = false;
    /// Minimum accepted stability of the selected smooth-curvature scale.
    double smoothCurvatureMinScaleStability = 0.0;
    /// Enables local feature-graph cleanup before loop recovery.
    bool cleanupFeatureGraph = true;
    /// Maximum endpoint gap, in local average-edge-length units, bridged by cleanup.
    double featureGraphGapLengthRatio = 1.25;
    /// Maximum weak-evidence (normal-tensor or smooth-curvature) spur length
    /// removed by the legacy edge-count cleanup rule.
    int featureGraphMaxWeakSpurEdges = 2;
    /// Confidence threshold used when reporting high-confidence components.
    double featureComponentMinConfidence = 0.35;
    /// Dimensionless Yoshizawa-style strength threshold for weak spur removal.
    ///
    /// When positive, a dangling weak-evidence chain is removed only when its
    /// curve strength T = (integral ds) * (integral strength ds) stays below
    /// this value, where ds is measured in local average-edge-length units and
    /// the per-edge strength is the persistence score divided by the matching
    /// channel threshold. Long-but-faint chains then survive while
    /// short-but-strong noise spikes are pruned, and chains longer than
    /// featureGraphMaxWeakSpurEdges also become prunable. The default 0 keeps
    /// the legacy behavior of removing every weak spur with at most
    /// featureGraphMaxWeakSpurEdges edges.
    double featureGraphMinWeakSpurStrength = 0.0;

    /// Optional noisy-input preprocessing, component recovery, and face
    /// segmentation stages. They are grouped to keep the main option surface
    /// readable while preserving value semantics.
    FeatureNormalFilterOptions normalFilter;
    FeatureGraphConsolidationOptions graphConsolidation;
    SurfacePatchOptions surfacePatches;
};

/// Diagnostics from one normal-domain preprocessing run.
struct FeatureNormalFilterReport {
    int iterationsCompleted = 0;       ///< Relaxation passes actually executed.
    int changedFaces = 0;              ///< Faces whose output normal differs from the raw normal.
    int preservedEdges = 0;            ///< Strong edges frozen by the preservation gate.
    double meanAngularChangeDeg = 0.0; ///< Mean raw-to-filtered normal angle.
    double maxAngularChangeDeg = 0.0;  ///< Maximum raw-to-filtered normal angle.
    double meanEdgeIndicator = 0.0;    ///< Mean final bilateral edge indicator in [0,1].
};

/// Filtered face normals plus quantitative preprocessing diagnostics.
struct FeatureNormalFilterResult {
    std::vector<Vec3> faceNormals;
    FeatureNormalFilterReport report;
};

/// Parameters for Tsuchie-Higashi style normal-tensor feature scoring.
struct NormalTensorOptions {
    int smoothingIterations = 0; ///< Face-normal smoothing passes before tensor voting.
    int scaleCount = 1;          ///< Number of increasing topological scales.
};

/// Per-vertex normal-tensor decomposition and feature saliency.
struct NormalTensorVertex {
    Vec3 normal = Vec3(0.0, 0.0, 1.0);        ///< Dominant tensor eigenvector.
    Vec3 creaseTangent = Vec3(1.0, 0.0, 0.0); ///< Tangent inferred from the middle eigendirection.
    double surfaceSaliency = 0.0;             ///< Locally planar support.
    double creaseSaliency = 0.0;              ///< Curve-like normal variation.
    double cornerSaliency = 0.0;              ///< Isotropic multi-directional variation.
    double featureScore = 0.0;                ///< Strongest single-scale accepted feature score.
    /// Mean of the per-scale feature scores over all sampled scales, whether
    /// or not a scale supported the winning candidate (sum of every scale's
    /// score divided by the scale count).
    double averageFeatureScore = 0.0;
    double persistentFeatureScore = 0.0; ///< Score after cross-scale persistence gating.
    double localScale = 0.0;             ///< Selected neighborhood radius in model units.
    int persistentScales = 0;            ///< Number of supporting scales.
};

/// Parameters for robust scale-normalized local quadric fitting.
struct SmoothCurvatureOptions {
    int baseNeighborhoodRings = 2;        ///< Topological radius of the finest fit.
    int scaleCount = 3;                   ///< Number of successively larger fits.
    int robustFitIterations = 2;          ///< Deterministic residual-reweighting passes.
    double minTangentConsistency = 0.65;  ///< Cross-scale absolute tangent-dot gate.
    bool useStableScaleSelection = false; ///< Prefer stable scale support over peak raw score.
    double minScaleStability = 0.0;       ///< Minimum accepted scale-stability score.
};

/// Per-vertex smooth ridge/valley evidence from multiscale quadric fitting.
///
/// Curvatures and scores are normalized by the fitted neighborhood radius, so
/// thresholds remain stable under uniform mesh scaling.
struct SmoothCurvatureVertex {
    Vec3 normal = Vec3(0.0, 0.0, 1.0);            ///< Fitted Monge-frame surface normal.
    Vec3 curveTangent = Vec3(1.0, 0.0, 0.0);      ///< Direction along the ridge/valley curve.
    Vec3 extremumDirection = Vec3(0.0, 1.0, 0.0); ///< Principal direction across the curve.
    double principalCurvature = 0.0;              ///< Signed curvature tested for an extremum.
    double secondaryCurvature = 0.0;              ///< Signed orthogonal principal curvature.
    double anisotropy = 0.0;                      ///< Dimensionless principal-curvature separation.
    double extremumStrength = 0.0;                ///< Two-sided directional-extremum strength.
    double featureScore = 0.0;                    ///< Strongest accepted scale-normalized score.
    /// Mean over all sampled scales of the scores from scales that support
    /// the winning candidate only (persistent sign and consistent tangent);
    /// unsupported scales contribute zero. This intentionally differs from
    /// NormalTensorVertex::averageFeatureScore, which averages every scale
    /// unconditionally.
    double averageFeatureScore = 0.0;
    double persistentFeatureScore = 0.0; ///< Score after sign/tangent persistence gating.
    double fitResidual = 0.0;            ///< Normalized robust quadric residual.
    double localScale = 0.0;             ///< Selected fit radius in model units.
    int persistentScales = 0;            ///< Number of supporting scales.
    int selectedScale = -1;              ///< Zero-based reference scale, or -1 when invalid.
    double scaleStability = 0.0;         ///< Agreement of neighboring scale fits in [0,1].
    /// Positive for a ridge, negative for a valley, zero when unclassified.
    int signedKind = 0;
};

/// One connected feature curve or loop detected in the mesh.
struct FeatureLoop {
    /// @name Recovered topology and ownership
    /// @{
    int id = -1;
    int componentId = -1;
    std::vector<int> vertices;
    int edgeCount = 0;
    bool closed = false;
    bool circular = false;
    bool mostlyBoundary = false;
    bool weakFeature = false;
    double componentConfidence = 0.0;
    double primitiveResidual = 0.0;
    /// @}

    /// @name Primitive fit
    /// Circle and near-circle use `radius`; ellipses use the major/minor pair.
    /// @{
    FeaturePrimitiveType primitive = FeaturePrimitiveType::Unknown;
    Vec3 center = Vec3::Zero();
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    Vec3 majorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 minorAxis = Vec3(0.0, 1.0, 0.0);
    double radius = 0.0;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double axisRatio = 0.0;
    double rmsRadialError = 0.0;
    double maxRadialError = 0.0;
    double rmsEllipseError = 0.0;
    double maxEllipseError = 0.0;
    double rmsPlaneError = 0.0;
    double maxPlaneError = 0.0;
    /// @}

    /// @name Signed dihedral summary
    /// @{
    int convexEdges = 0;
    int concaveEdges = 0;
    int unknownSignedEdges = 0;
    /// @}
};

/// Per-vertex feature classification used by feature-preserving simplification.
struct VertexFeature {
    /// @name Ownership and graph role
    /// @{
    bool isFeature = false;
    bool circular = false;
    bool junction = false;
    bool weakFeature = false;
    FeaturePrimitiveType primitive = FeaturePrimitiveType::Unknown;
    int loopId = -1;
    int componentId = -1;
    double confidence = 0.0;
    Vec3 tangent = Vec3::Zero();
    /// @}

    /// @name Circle projection data
    /// @{
    Vec3 circleCenter = Vec3::Zero();
    Vec3 circleNormal = Vec3(0.0, 0.0, 1.0);
    double circleRadius = 0.0;
    /// @}

    /// @name Ellipse projection data
    /// @{
    Vec3 ellipseCenter = Vec3::Zero();
    Vec3 ellipseNormal = Vec3(0.0, 0.0, 1.0);
    Vec3 ellipseMajorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 ellipseMinorAxis = Vec3(0.0, 1.0, 0.0);
    double ellipseMajorRadius = 0.0;
    double ellipseMinorRadius = 0.0;
    /// @}
};

/// One edge in the explicit feature graph.
struct FeatureGraphEdge {
    int a = -1;
    int b = -1;
    bool boundary = false;
    bool dihedral = false;
    bool normalTensor = false;
    bool smoothCurvature = false;
    bool nonManifold = false;
    bool cleanupBridge = false;
    bool consolidationBridge = false;
    bool removedByCleanup = false;
    int signedKind = 0;
};

/// One directed branch leaving a feature-graph vertex.
struct FeatureGraphBranch {
    int edgeId = -1;
    int neighborVertex = -1;
    Vec3 tangent = Vec3::Zero();
    int signedKind = 0;
};

/// Best continuation pairing between two incident branches at a junction.
struct FeatureGraphBranchPair {
    int firstBranch = -1;
    int secondBranch = -1;
    double alignment = 0.0;
};

/// Per-vertex ownership in the explicit feature graph.
///
/// A vertex is a junction only when more than two active edges meet there (or
/// when it is shared between loops); a vertex with exactly one active edge is
/// a chain endpoint, not a junction.
struct FeatureGraphVertex {
    std::vector<int> incidentEdges;
    std::vector<int> loopIds;
    std::vector<FeatureGraphBranch> branches;
    std::vector<FeatureGraphBranchPair> branchPairs;
    bool junction = false;
    bool shared = false;
    bool endpoint = false;
    bool ambiguousJunction = false;
};

/// Explicit graph view of detected feature edges and recovered loops.
struct FeatureGraph {
    std::vector<FeatureGraphEdge> edges;
    std::vector<FeatureGraphVertex> vertices;
    std::vector<int> junctionVertices;
    std::vector<int> sharedVertices;
    std::vector<int> endpointVertices;
};

/// One connected feature-graph component after trace cleanup.
///
/// These diagnostics make weak-feature decisions explicit: downstream
/// simplification can distinguish a closed, strongly supported CAD loop from a
/// sparse, weak-evidence ridge fragment even when both produce feature vertices.
struct FeatureComponent {
    int id = -1;
    std::vector<int> vertices;
    int edgeCount = 0;
    int boundaryEdges = 0;
    int dihedralEdges = 0;
    int normalTensorEdges = 0;
    int smoothCurvatureEdges = 0;
    int nonManifoldEdges = 0;
    int cleanupBridgeEdges = 0;
    int consolidationBridgeEdges = 0;
    int strongEvidenceEdges = 0;
    int weakEvidenceEdges = 0;
    int junctionVertices = 0;
    int endpointVertices = 0;
    int cycleRank = 0;
    bool closed = false;
    double closureRate = 0.0;
    double strongEvidenceRatio = 0.0;
    double meanTensorPersistence = 0.0;
    double meanCurvaturePersistence = 0.0;
    double meanPrimitiveResidual = 0.0;
    double confidence = 0.0;
};

/// One connected face region separated by active feature edges.
struct FeaturePatch {
    int id = -1;
    int faceCount = 0;
    int featureBoundaryEdges = 0;
    int meshBoundaryEdges = 0;
    int nonManifoldBoundaryEdges = 0;
    bool closed = false;
    double area = 0.0;
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    std::vector<int> neighboringPatches;
};

/// Feature-edge adjacency between two surface patches.
struct FeaturePatchAdjacency {
    int firstPatch = -1;
    int secondPatch = -1;
    int featureEdges = 0;
};

/// Full feature-detection result for a mesh.
///
/// Counts distinguish the evidence source used to build the explicit graph.
/// Downstream algorithms should prefer `loops` and `vertices` for feature
/// ownership, and use the counters for diagnostics and policy validation.
///
/// `featureEdges` counts evidence edges only. Bridge edges synthesized by
/// graph cleanup are appended to `graph.edges` (flagged `cleanupBridge`) but
/// are not evidence, so `graph.edges.size()` can exceed `featureEdges`.
struct FeatureAnalysis {
    std::vector<VertexFeature> vertices;
    std::vector<FeatureLoop> loops;
    std::vector<FeatureComponent> components;
    FeatureGraph graph;
    int featureEdges = 0;
    int tracedFeatureEdges = 0;
    int untracedFeatureEdges = 0;
    int graphCleanupBridgedGaps = 0;
    int graphCleanupRemovedSpurs = 0;
    int graphCleanupMergedJunctions = 0;
    int boundaryFeatureEdges = 0;
    int dihedralFeatureEdges = 0;
    int normalTensorFeatureEdges = 0;
    int smoothCurvatureFeatureEdges = 0;
    int nonManifoldFeatureEdges = 0;
    int normalTensorScoredVertices = 0;
    int smoothCurvatureScoredVertices = 0;
    int convexFeatureEdges = 0;
    int concaveFeatureEdges = 0;
    int unknownSignedFeatureEdges = 0;
    int weakFeatureComponents = 0;
    int highConfidenceFeatureComponents = 0;
    double maxNormalTensorFeatureScore = 0.0;
    double maxNormalTensorPersistentScore = 0.0;
    double meanNormalTensorLocalScale = 0.0;
    double meanNormalTensorPersistence = 0.0;
    double maxSmoothCurvatureFeatureScore = 0.0;
    double maxSmoothCurvaturePersistentScore = 0.0;
    double meanSmoothCurvatureLocalScale = 0.0;
    double meanSmoothCurvaturePersistence = 0.0;
    double meanSmoothCurvatureScaleStability = 0.0;
    double meanFeatureComponentConfidence = 0.0;
    double minFeatureComponentConfidence = 0.0;
    /// Interior edges whose two faces disagree on winding order; dihedral
    /// scoring falls back to the unsigned normal angle for these edges.
    int inconsistentWindingEdges = 0;
    /// Cleanup passes skipped because an endpoint/junction hard cap was hit.
    int graphCleanupSkippedByCap = 0;
    /// Circular-cluster recovery components whose triplet scan was truncated.
    int circularRecoveryTruncated = 0;
    /// Input faces tolerated as degenerate (repeated vertex position or
    /// numerically zero area). Their normals are unusable, so per-face
    /// evidence skips their contribution; the count makes that degradation
    /// visible instead of silently absorbing dirty input.
    int degenerateFaces = 0;
    FeatureNormalFilterReport normalFilter;
    int graphConsolidationBridges = 0;
    int graphConsolidationSkippedByCap = 0;
    int junctionBranchPairs = 0;
    int ambiguousJunctions = 0;
    std::vector<int> facePatchIds;
    std::vector<FeaturePatch> patches;
    std::vector<FeaturePatchAdjacency> patchAdjacencies;
    int closedSurfacePatches = 0;
    int segmentationIgnoredRecoveryEdges = 0;
};

/// Ground-truth continuation at one labeled feature junction.
struct FeatureBranchPairLabel {
    int junctionVertex = -1;
    int firstNeighbor = -1;
    int secondNeighbor = -1;
};

/// Extensible labels for edge, junction, branch, and face-patch benchmarks.
struct FeatureBenchmarkLabels {
    std::vector<std::pair<int, int>> edges;
    std::vector<int> junctionVertices;
    std::vector<FeatureBranchPairLabel> branchPairs;
    /// Per-face ground-truth patch id; negative values are unlabeled.
    std::vector<int> facePatchIds;
};

/// Edge-label benchmark summary for one detected feature graph.
struct FeatureEdgeBenchmark {
    int groundTruthEdges = 0;
    int detectedEdges = 0;
    int truePositiveEdges = 0;
    int falsePositiveEdges = 0;
    int falseNegativeEdges = 0;
    int groundTruthJunctions = 0;
    int detectedJunctions = 0;
    int truePositiveJunctions = 0;
    int falsePositiveJunctions = 0;
    int falseNegativeJunctions = 0;
    double edgePrecision = 0.0;
    double edgeRecall = 0.0;
    double edgeF1 = 0.0;
    double junctionPrecision = 0.0;
    double junctionRecall = 0.0;
    double junctionF1 = 0.0;
    double loopClosureRate = 0.0;
    double meanComponentConfidence = 0.0;
    int groundTruthBranchPairs = 0;
    int detectedBranchPairs = 0;
    int truePositiveBranchPairs = 0;
    int falsePositiveBranchPairs = 0;
    int falseNegativeBranchPairs = 0;
    double branchPairPrecision = 0.0;
    double branchPairRecall = 0.0;
    double branchPairF1 = 0.0;
    int labeledFaceAdjacencies = 0;
    int correctFaceAdjacencies = 0;
    double patchAdjacencyAccuracy = 0.0;
};

/// Error of a detected loop against a circular reference curve.
struct DirectionalCurveError {
    int samples = 0;
    double radialRms = 0.0;
    double radialMax = 0.0;
    double planeRms = 0.0;
    double planeMax = 0.0;
};

} // namespace manumesh::feature
