#pragma once

#include "Export.h"

#include <string>

namespace manumesh::simplification {

/// Built-in strategies for spatially varying line-quadric weights.
enum class WeightMode {
  Uniform,
  Dihedral,
  NormalTensor,
  Height,
  XBand,
};

/// Reason one simplification run stopped.
enum class SimplifyTerminationReason {
  NotStarted,
  ReachedTarget,
  AlreadyAtOrBelowTarget,
  NoCandidates,
  RejectionLimit,
};

/// Hard feature-constraint policy used when preserveFeatureCurves is enabled.
///
/// The policy only controls hard collapse rejections. Soft costs from line
/// quadrics and feature-curve quadrics can still influence candidate ranking.
enum class FeatureProtectionMode {
  /// Disable hard feature-curve protection.
  None,
  /// Hard-protect circular and near-circular loops only.
  CircularOnly,
  /// Hard-protect fitted primitive loops: circles, near-circles, and ellipses.
  PrimitiveCurves,
  /// Strict mode: hard-protect every detected feature edge.
  AllFeatureEdges,
};

/// User-facing controls for one simplification run.
///
/// Options are grouped conceptually into target selection, QEM/line-quadric
/// costs, feature detection, hard legality filters, and diagnostics. The
/// simplifier treats QEM and line quadrics as candidate-ranking costs, then
/// applies explicit filters for topology, boundary behavior, normals, triangle
/// quality, local error, self-intersection, and feature-curve policy.
struct SimplifyOptions {
  // Target selection.
  /// Absolute output face target. When positive, this overrides targetRatio.
  int targetFaces = -1;
  /// Output face ratio used when targetFaces is not set.
  double targetRatio = 0.25;

  // QEM and line-quadric ranking costs.
  /// Adds line quadrics to reduce tangential drift in underconstrained regions.
  bool useLineQuadrics = true;
  /// Base line-quadric weight. Keep this small enough that plane quadrics and
  /// hard filters still control geometric fidelity.
  double lineWeight = 1e-3;
  /// Spatial weighting policy for line quadrics and feature-sensitive costs.
  WeightMode weightMode = WeightMode::Uniform;
  /// Extra line-quadric weight applied near detected feature evidence.
  double featureBoost = 0.05;
  /// Dihedral threshold, in degrees, for hard-edge feature detection.
  double featureAngleDeg = 40.0;
  /// Scales line weights from local mesh size instead of using lineWeight alone.
  bool adaptiveScale = false;
  double adaptiveBaseLineWeight = 1e-2;
  /// Soft cost for boundary adherence. Use preserveBoundary for hard topology.
  double boundaryWeight = 0.0;
  /// Rejects collapses that would merge or remove open-boundary structure.
  bool preserveBoundary = false;

  // Feature detection, feature projection, and hard feature policies.
  /// Enables feature detection, feature quadrics, projection, and protection.
  bool preserveFeatureCurves = false;
  /// Preferred hard feature policy. Defaults to primitive curves so generic
  /// creases stay soft unless the caller explicitly requests strict locking.
  FeatureProtectionMode featureProtectionMode = FeatureProtectionMode::PrimitiveCurves;
  /// Soft feature-curve quadric weight applied to detected loops.
  double featureCurveWeight = 0.05;
  /// Rejects raw collapse placements that drift too far from a feature curve
  /// before projection. Zero disables the curve-distance budget.
  double maxFeatureCurveDeviationRatio = 0.0;
  /// Primitive-fit thresholds used by feature detection before simplification.
  double circleFitRelativeThreshold = 0.05;
  double ellipseFitRelativeThreshold = 0.05;
  double nearCircleAxisRatioTolerance = 0.08;
  /// Minimum loop sizes for generic and circular/near-circular features.
  int minFeatureLoopVertices = 16;
  int minCircularFeatureLoopVertices = 6;
  /// Enables normal-tensor weak-feature evidence in addition to dihedral edges.
  bool useNormalTensorFeatures = true;
  double normalTensorFeatureThreshold = 0.16;
  double normalTensorMinEdgeAlignment = 0.45;
  int normalTensorSmoothingIterations = 0;
  int normalTensorScaleCount = 1;

  // Hard post-placement filters and diagnostics.
  /// Hard post-placement filters. Zero local-error budgets disable those tests.
  double minTriangleQuality = 0.0;
  double maxNormalDeviationDeg = 90.0;
  double maxLocalError = 0.0;
  double maxLocalErrorRatio = 0.0;
  bool preventLocalIntersections = false;
  bool verbose = false;
};

/// Diagnostics collected during simplification.
///
/// Rejection counters describe the first hard filter that rejected a current
/// collapse candidate. They are intended for parameter tuning and regression
/// tests, not as independent totals of every failed subcheck.
struct SimplifyReport {
  // Mesh-size summary.
  int initialVertices = 0;
  int initialFaces = 0;
  int finalVertices = 0;
  int finalFaces = 0;

  // Queue and solve progress.
  int collapsedEdges = 0;
  int rejectedCollapses = 0;
  /// Number of current collapse candidates whose placement solve used fallback
  /// endpoint/midpoint candidates instead of a stable linear-system optimum.
  int solverFallbacks = 0;
  int queueRebuilds = 0;

  // Feature-analysis summary captured at run start.
  int featureLoops = 0;
  int circularFeatureLoops = 0;
  int featureVertices = 0;
  int normalTensorFeatureEdges = 0;

  // First-reject counters for current collapse candidates.
  int featureRejectedCollapses = 0;
  int primitiveFeatureRejectedCollapses = 0;
  int genericFeatureRejectedCollapses = 0;
  int boundaryRejectedCollapses = 0;
  int topologyRejectedCollapses = 0;
  int normalFlipRejectedCollapses = 0;
  int qualityRejectedCollapses = 0;
  int selfIntersectionRejectedCollapses = 0;
  int curveBudgetRejectedCollapses = 0;
  int errorRejectedCollapses = 0;
  int projectedFeaturePlacements = 0;

  // Final termination and applied weight range.
  SimplifyTerminationReason terminationReason = SimplifyTerminationReason::NotStarted;
  double minAppliedLineWeight = 0.0;
  double maxAppliedLineWeight = 0.0;
};

/// Parses a command/user string into a weight mode.
MANUMESH_API WeightMode parseWeightMode(const std::string& value);
/// Converts a weight mode to its stable lowercase string representation.
MANUMESH_API std::string toString(WeightMode mode);
/// Converts a termination reason to its stable lowercase string representation.
MANUMESH_API std::string toString(SimplifyTerminationReason reason);
/// Parses a feature-protection mode from a stable lowercase string.
MANUMESH_API FeatureProtectionMode parseFeatureProtectionMode(const std::string& value);
/// Converts a feature-protection mode to its stable lowercase string representation.
MANUMESH_API std::string toString(FeatureProtectionMode mode);

} // namespace manumesh::simplification
