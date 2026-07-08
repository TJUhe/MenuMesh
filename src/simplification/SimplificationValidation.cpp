#include "detail/SimplificationValidation.h"

#include "core/MeshTopology.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace manumesh::simplification {
namespace {

void requireFiniteNonNegative(double value, const char* name) {
  if (!std::isfinite(value) || value < 0.0) {
    throw std::invalid_argument(std::string(name) +
                                " must be finite and non-negative.");
  }
}

} // namespace

void validateSimplifyOptions(const SimplifyOptions& options) {
  if (options.targetFaces == 0 || options.targetFaces < -1) {
    throw std::invalid_argument("targetFaces must be -1 or a positive face count.");
  }
  if (options.targetFaces < 0 &&
      (!std::isfinite(options.targetRatio) || options.targetRatio <= 0.0 ||
       options.targetRatio > 1.0)) {
    throw std::invalid_argument("targetRatio must be finite and in the range (0, 1].");
  }
  requireFiniteNonNegative(options.lineWeight, "lineWeight");
  requireFiniteNonNegative(options.featureBoost, "featureBoost");
  requireFiniteNonNegative(options.adaptiveBaseLineWeight, "adaptiveBaseLineWeight");
  requireFiniteNonNegative(options.boundaryWeight, "boundaryWeight");
  requireFiniteNonNegative(options.featureCurveWeight, "featureCurveWeight");
  requireFiniteNonNegative(options.maxFeatureCurveDeviationRatio,
                           "maxFeatureCurveDeviationRatio");
  requireFiniteNonNegative(options.circleFitRelativeThreshold,
                           "circleFitRelativeThreshold");
  requireFiniteNonNegative(options.ellipseFitRelativeThreshold,
                           "ellipseFitRelativeThreshold");
  requireFiniteNonNegative(options.nearCircleAxisRatioTolerance,
                           "nearCircleAxisRatioTolerance");
  requireFiniteNonNegative(options.normalTensorFeatureThreshold,
                           "normalTensorFeatureThreshold");
  if (!std::isfinite(options.featureAngleDeg) || options.featureAngleDeg < 0.0 ||
      options.featureAngleDeg > 180.0) {
    throw std::invalid_argument("featureAngleDeg must be finite and in [0, 180].");
  }
  if (!std::isfinite(options.normalTensorMinEdgeAlignment) ||
      options.normalTensorMinEdgeAlignment < 0.0 ||
      options.normalTensorMinEdgeAlignment > 1.0) {
    throw std::invalid_argument(
        "normalTensorMinEdgeAlignment must be finite and in [0, 1].");
  }
  if (options.minFeatureLoopVertices < 3) {
    throw std::invalid_argument("minFeatureLoopVertices must be at least 3.");
  }
  if (options.minCircularFeatureLoopVertices < 3) {
    throw std::invalid_argument("minCircularFeatureLoopVertices must be at least 3.");
  }
  if (options.normalTensorSmoothingIterations < 0) {
    throw std::invalid_argument(
        "normalTensorSmoothingIterations must be non-negative.");
  }
  if (options.normalTensorScaleCount < 1) {
    throw std::invalid_argument("normalTensorScaleCount must be positive.");
  }
  requireFiniteNonNegative(options.minTriangleQuality, "minTriangleQuality");
  if (options.minTriangleQuality > 1.0) {
    throw std::invalid_argument("minTriangleQuality must be in [0, 1].");
  }
  requireFiniteNonNegative(options.maxLocalError, "maxLocalError");
  requireFiniteNonNegative(options.maxLocalErrorRatio, "maxLocalErrorRatio");
  if (!std::isfinite(options.maxNormalDeviationDeg) ||
      options.maxNormalDeviationDeg < 0.0 || options.maxNormalDeviationDeg > 180.0) {
    throw std::invalid_argument(
        "maxNormalDeviationDeg must be finite and in [0, 180].");
  }
}

void validateSimplifierInput(const Mesh& input) {
  if (input.empty()) {
    return;
  }
  std::string error;
  if (!validateMeshGeometry(input, &error)) {
    throw std::invalid_argument(error);
  }
  const Result<MeshTopology> topology = MeshTopology::build(input);
  if (!topology.ok()) {
    throw std::invalid_argument(topology.status().message());
  }
}

} // namespace manumesh::simplification
