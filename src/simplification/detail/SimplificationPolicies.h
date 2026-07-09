#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "algorithms/simplification/SimplificationTypes.h"

namespace manumesh::simplification {

struct TargetPolicy {
  int targetFaces = -1;
  double targetRatio = 0.25;

  int resolveTargetFaceCount(int inputFaceCount) const;
};

struct FeatureDetectionPolicy {
  bool enabled = false;
  feature::FeatureOptions options;
};

feature::FeatureOptions
featureOptionsFromSimplifyOptions(const SimplifyOptions& options,
                                  int minFeatureLoopVerticesFloor = 0);

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

} // namespace manumesh::simplification
