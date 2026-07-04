#pragma once

#include "line_quadrics_qem/Export.h"
#include "line_quadrics_qem/core/Mesh.h"

#include <string>

namespace lq {

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

/// User-facing controls for one simplification run.
struct SimplifyOptions {
  int targetFaces = -1;
  double targetRatio = 0.25;
  bool useLineQuadrics = true;
  double lineWeight = 1e-3;
  WeightMode weightMode = WeightMode::Uniform;
  double featureBoost = 0.05;
  double featureAngleDeg = 40.0;
  bool adaptiveScale = false;
  double adaptiveBaseLineWeight = 1e-2;
  double boundaryWeight = 0.0;
  bool preserveBoundary = false;
  bool preserveFeatureCurves = false;
  bool protectAllFeatureEdges = false;
  double featureCurveWeight = 0.05;
  double maxFeatureCurveDeviationRatio = 0.0;
  double circleFitRelativeThreshold = 0.05;
  double ellipseFitRelativeThreshold = 0.05;
  double nearCircleAxisRatioTolerance = 0.08;
  int minFeatureLoopVertices = 16;
  int minCircularFeatureLoopVertices = 6;
  bool useNormalTensorFeatures = true;
  double normalTensorFeatureThreshold = 0.16;
  double normalTensorMinEdgeAlignment = 0.45;
  int normalTensorSmoothingIterations = 0;
  int normalTensorScaleCount = 1;
  double minTriangleQuality = 0.0;
  double maxNormalDeviationDeg = 90.0;
  double maxLocalError = 0.0;
  double maxLocalErrorRatio = 0.0;
  bool preventLocalIntersections = false;
  bool verbose = false;
};

/// Diagnostics collected during simplification.
struct SimplifyReport {
  int initialVertices = 0;
  int initialFaces = 0;
  int finalVertices = 0;
  int finalFaces = 0;
  int collapsedEdges = 0;
  int rejectedCollapses = 0;
  int solverFallbacks = 0;
  int queueRebuilds = 0;
  int featureLoops = 0;
  int circularFeatureLoops = 0;
  int featureVertices = 0;
  int normalTensorFeatureEdges = 0;
  int featureRejectedCollapses = 0;
  int boundaryRejectedCollapses = 0;
  int topologyRejectedCollapses = 0;
  int normalFlipRejectedCollapses = 0;
  int qualityRejectedCollapses = 0;
  int selfIntersectionRejectedCollapses = 0;
  int curveBudgetRejectedCollapses = 0;
  int errorRejectedCollapses = 0;
  int projectedFeaturePlacements = 0;
  SimplifyTerminationReason terminationReason = SimplifyTerminationReason::NotStarted;
  double minAppliedLineWeight = 0.0;
  double maxAppliedLineWeight = 0.0;
};

/// Stateful object API for configuring and running mesh simplification.
class LQ_API QEMSimplifier {
public:
  QEMSimplifier() = default;
  explicit QEMSimplifier(SimplifyOptions options);

  /// Returns the options used by subsequent simplification runs.
  const SimplifyOptions& options() const { return options_; }
  /// Replaces the options used by subsequent simplification runs.
  void setOptions(SimplifyOptions options);
  /// Returns diagnostics from the most recent simplification run.
  const SimplifyReport& report() const { return report_; }

  /// Simplifies a mesh and stores diagnostics on this object.
  Mesh simplify(const Mesh& input);
  /// Simplifies a mesh, stores diagnostics, and optionally copies them out.
  Mesh simplify(const Mesh& input, SimplifyReport* report);

private:
  SimplifyOptions options_;
  SimplifyReport report_;
};

/// Parses a command/user string into a weight mode.
LQ_API WeightMode parseWeightMode(const std::string& value);
/// Converts a weight mode to its stable lowercase string representation.
LQ_API std::string toString(WeightMode mode);
/// Converts a termination reason to its stable lowercase string representation.
LQ_API std::string toString(SimplifyTerminationReason reason);

/// Simplifies a mesh with standard QEM or line-quadrics-augmented QEM.
/// Prefer QEMSimplifier for new code that needs an object-oriented API.
LQ_API Mesh simplifyMesh(const Mesh& input, const SimplifyOptions& options,
                         SimplifyReport* report = nullptr);

} // namespace lq
