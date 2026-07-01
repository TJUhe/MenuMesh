#pragma once

#include "line_quadrics_qem/Export.h"
#include "line_quadrics_qem/core/Mesh.h"

#include <string>

namespace lq {

/// Built-in strategies for spatially varying line-quadric weights.
enum class WeightMode {
  Uniform,
  Dihedral,
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
  int featureRejectedCollapses = 0;
  int projectedFeaturePlacements = 0;
  double minAppliedLineWeight = 0.0;
  double maxAppliedLineWeight = 0.0;
};

/// Parses a command/user string into a weight mode.
LQ_API WeightMode parseWeightMode(const std::string& value);
/// Converts a weight mode to its stable lowercase string representation.
LQ_API std::string toString(WeightMode mode);

/// Simplifies a mesh with standard QEM or line-quadrics-augmented QEM.
LQ_API Mesh simplifyMesh(const Mesh& input, const SimplifyOptions& options,
                         SimplifyReport* report = nullptr);

} // namespace lq
