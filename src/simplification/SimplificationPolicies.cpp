#include "detail/SimplificationPolicies.h"

#include "detail/SimplificationConstants.h"

#include <algorithm>
#include <cmath>

namespace manumesh::simplification {

int TargetPolicy::resolveTargetFaceCount(int inputFaceCount) const {
  if (targetFaces > 0) {
    return targetFaces;
  }
  return std::max(4, static_cast<int>(std::llround(inputFaceCount * targetRatio)));
}

feature::FeatureOptions FeatureDetectionPolicy::toFeatureOptions() const {
  feature::FeatureOptions options;
  options.featureAngleDeg = featureAngleDeg;
  options.circleFitRelativeThreshold = circleFitRelativeThreshold;
  options.ellipseFitRelativeThreshold = ellipseFitRelativeThreshold;
  options.nearCircleAxisRatioTolerance = nearCircleAxisRatioTolerance;
  options.minFeatureLoopVertices = std::max(5, minFeatureLoopVertices);
  options.useNormalTensorFeatures = useNormalTensorFeatures;
  options.normalTensorFeatureThreshold = normalTensorFeatureThreshold;
  options.normalTensorMinEdgeAlignment = normalTensorMinEdgeAlignment;
  options.normalTensorSmoothingIterations = normalTensorSmoothingIterations;
  options.normalTensorScaleCount = normalTensorScaleCount;
  return options;
}

double LegalityPolicy::resolveMinNormalDot() const {
  return maxNormalDeviationDeg >= 180.0 ? -1.0
                                        : std::cos(maxNormalDeviationDeg * kPi / 180.0);
}

double LegalityPolicy::resolveMaxLocalError(double bboxDiag) const {
  return std::max(maxLocalError, maxLocalErrorRatio * bboxDiag);
}

SimplificationPolicies
SimplificationPolicies::fromOptions(const SimplifyOptions& options) {
  SimplificationPolicies policies;
  policies.target.targetFaces = options.targetFaces;
  policies.target.targetRatio = options.targetRatio;

  policies.features.enabled = options.preserveFeatureCurves;
  policies.features.featureAngleDeg = options.featureAngleDeg;
  policies.features.circleFitRelativeThreshold = options.circleFitRelativeThreshold;
  policies.features.ellipseFitRelativeThreshold = options.ellipseFitRelativeThreshold;
  policies.features.nearCircleAxisRatioTolerance = options.nearCircleAxisRatioTolerance;
  policies.features.minFeatureLoopVertices = options.minFeatureLoopVertices;
  policies.features.useNormalTensorFeatures = options.useNormalTensorFeatures;
  policies.features.normalTensorFeatureThreshold = options.normalTensorFeatureThreshold;
  policies.features.normalTensorMinEdgeAlignment = options.normalTensorMinEdgeAlignment;
  policies.features.normalTensorSmoothingIterations =
      options.normalTensorSmoothingIterations;
  policies.features.normalTensorScaleCount = options.normalTensorScaleCount;

  policies.legality.preserveBoundary = options.preserveBoundary;
  policies.legality.minTriangleQuality = options.minTriangleQuality;
  policies.legality.maxNormalDeviationDeg = options.maxNormalDeviationDeg;
  policies.legality.maxLocalError = options.maxLocalError;
  policies.legality.maxLocalErrorRatio = options.maxLocalErrorRatio;
  policies.legality.preventLocalIntersections = options.preventLocalIntersections;
  return policies;
}

} // namespace manumesh::simplification
