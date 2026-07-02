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
  bool preserveFeatureCurves = false;
  bool protectAllFeatureEdges = false;
  double featureCurveWeight = 0.05;
  double circleFitRelativeThreshold = 0.05;
  int minFeatureLoopVertices = 16;
  bool useNormalTensorFeatures = true;
  double normalTensorFeatureThreshold = 0.16;
  double normalTensorMinEdgeAlignment = 0.45;
  int normalTensorSmoothingIterations = 0;
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
  int projectedFeaturePlacements = 0;
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

/// Simplifies a mesh with standard QEM or line-quadrics-augmented QEM.
/// Prefer QEMSimplifier for new code that needs an object-oriented API.
LQ_API Mesh simplifyMesh(const Mesh& input, const SimplifyOptions& options,
                         SimplifyReport* report = nullptr);

} // namespace lq
