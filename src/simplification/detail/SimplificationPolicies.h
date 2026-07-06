#pragma once

#include "line_quadrics_qem/algorithms/feature_detection/FeatureTypes.h"
#include "line_quadrics_qem/algorithms/simplification/SimplificationTypes.h"

namespace lq::simplification {

struct TargetPolicy {
  int targetFaces = -1;
  double targetRatio = 0.25;

  int resolveTargetFaceCount(int inputFaceCount) const;
};

struct FeatureDetectionPolicy {
  bool enabled = false;
  double featureAngleDeg = 40.0;
  double circleFitRelativeThreshold = 0.05;
  double ellipseFitRelativeThreshold = 0.05;
  double nearCircleAxisRatioTolerance = 0.08;
  int minFeatureLoopVertices = 16;
  bool useNormalTensorFeatures = true;
  double normalTensorFeatureThreshold = 0.16;
  double normalTensorMinEdgeAlignment = 0.45;
  int normalTensorSmoothingIterations = 0;
  int normalTensorScaleCount = 1;

  FeatureOptions toFeatureOptions() const;
};

struct LegalityPolicy {
  bool preserveBoundary = false;
  double minTriangleQuality = 0.0;
  double maxNormalDeviationDeg = 90.0;
  double maxLocalError = 0.0;
  double maxLocalErrorRatio = 0.0;
  bool preventLocalIntersections = false;

  double resolveMinNormalDot() const;
  double resolveMaxLocalError(double bboxDiag) const;
};

struct SimplificationPolicies {
  TargetPolicy target;
  FeatureDetectionPolicy features;
  LegalityPolicy legality;

  static SimplificationPolicies fromOptions(const SimplifyOptions& options);
};

} // namespace lq::simplification
