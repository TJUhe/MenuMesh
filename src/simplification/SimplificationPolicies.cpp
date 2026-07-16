/**
 * @file src/simplification/SimplificationPolicies.cpp
 * @brief Implements simplification policies facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details Normalizes user options into immutable hot-loop policy switches and thresholds.
 * @invariants Policy derivation performs no mesh-dependent work and every
 * disabled optional channel has an explicit zero/false representation.
 */

#include "detail/SimplificationPolicies.h"

#include "common/detail/MathConstants.h"

#include <algorithm>
#include <cmath>

namespace manumesh::simplification {

using manumesh::common::kPi;

int TargetPolicy::resolveTargetFaceCount(int inputFaceCount) const {
    if (targetFaces > 0) {
        return targetFaces;
    }
    return std::max(4, static_cast<int>(std::llround(inputFaceCount * targetRatio)));
}

feature::FeatureOptions
featureOptionsFromSimplifyOptions(const SimplifyOptions& options, int minFeatureLoopVerticesFloor) {
    feature::FeatureOptions featureOptions;
    featureOptions.featureAngleDeg = options.featureAngleDeg;
    featureOptions.loopTraceAngleDeg = options.loopTraceAngleDeg;
    featureOptions.circleFitRelativeThreshold = options.circleFitRelativeThreshold;
    featureOptions.ellipseFitRelativeThreshold = options.ellipseFitRelativeThreshold;
    featureOptions.nearCircleAxisRatioTolerance = options.nearCircleAxisRatioTolerance;
    featureOptions.minFeatureLoopVertices = std::max(minFeatureLoopVerticesFloor, options.minFeatureLoopVertices);
    featureOptions.useNormalTensorFeatures = options.useNormalTensorFeatures;
    featureOptions.normalTensorFeatureThreshold = options.normalTensorFeatureThreshold;
    featureOptions.normalTensorMinEdgeAlignment = options.normalTensorMinEdgeAlignment;
    featureOptions.normalTensorSmoothingIterations = options.normalTensorSmoothingIterations;
    featureOptions.normalTensorScaleCount = options.normalTensorScaleCount;
    featureOptions.normalTensorMinPersistentScales = options.normalTensorMinPersistentScales;
    featureOptions.useSmoothCurvatureFeatures = options.useSmoothCurvatureFeatures;
    featureOptions.smoothCurvatureFeatureThreshold = options.smoothCurvatureFeatureThreshold;
    featureOptions.smoothCurvatureMinEdgeAlignment = options.smoothCurvatureMinEdgeAlignment;
    featureOptions.smoothCurvatureMinTangentConsistency = options.smoothCurvatureMinTangentConsistency;
    featureOptions.smoothCurvatureBaseNeighborhoodRings = options.smoothCurvatureBaseNeighborhoodRings;
    featureOptions.smoothCurvatureScaleCount = options.smoothCurvatureScaleCount;
    featureOptions.smoothCurvatureMinPersistentScales = options.smoothCurvatureMinPersistentScales;
    featureOptions.smoothCurvatureRobustFitIterations = options.smoothCurvatureRobustFitIterations;
    featureOptions.smoothCurvatureUseStableScaleSelection = options.smoothCurvatureUseStableScaleSelection;
    featureOptions.smoothCurvatureMinScaleStability = options.smoothCurvatureMinScaleStability;
    featureOptions.cleanupFeatureGraph = options.cleanupFeatureGraph;
    featureOptions.featureGraphGapLengthRatio = options.featureGraphGapLengthRatio;
    featureOptions.featureGraphMaxWeakSpurEdges = options.featureGraphMaxWeakSpurEdges;
    featureOptions.featureGraphMinWeakSpurStrength = options.featureGraphMinWeakSpurStrength;
    featureOptions.featureComponentMinConfidence = options.featureComponentMinConfidence;
    featureOptions.normalFilter.enabled = options.useFeatureNormalFilter;
    featureOptions.normalFilter.iterations = options.featureNormalFilterIterations;
    featureOptions.normalFilter.angleSigmaDeg = options.featureNormalFilterAngleSigmaDeg;
    featureOptions.normalFilter.preserveAngleDeg = options.featureNormalFilterPreserveAngleDeg;
    featureOptions.normalFilter.relaxation = options.featureNormalFilterRelaxation;
    featureOptions.graphConsolidation.enabled = options.consolidateFeatureGraph;
    featureOptions.graphConsolidation.maxGapLengthRatio = options.featureGraphConsolidationGapLengthRatio;
    featureOptions.graphConsolidation.minAlignment = options.featureGraphConsolidationMinAlignment;
    return featureOptions;
}

double LegalityPolicy::resolveMinNormalDot() const {
    return maxNormalDeviationDeg >= 180.0 ? -1.0 : std::cos(maxNormalDeviationDeg * kPi / 180.0);
}

double LegalityPolicy::resolveMaxLocalError(double bboxDiag) const {
    return std::max(maxLocalError, maxLocalErrorRatio * bboxDiag);
}

SimplificationPolicies SimplificationPolicies::fromOptions(const SimplifyOptions& options) {
    SimplificationPolicies policies;
    policies.target.targetFaces = options.targetFaces;
    policies.target.targetRatio = options.targetRatio;

    policies.features.enabled = options.preserveFeatureCurves;
    policies.features.options = featureOptionsFromSimplifyOptions(options, 5);

    policies.legality.preserveBoundary = options.preserveBoundary;
    policies.legality.minTriangleQuality = options.minTriangleQuality;
    policies.legality.maxNormalDeviationDeg = options.maxNormalDeviationDeg;
    policies.legality.maxLocalError = options.maxLocalError;
    policies.legality.maxLocalErrorRatio = options.maxLocalErrorRatio;
    policies.legality.preventLocalIntersections = options.preventLocalIntersections;
    return policies;
}

} // namespace manumesh::simplification
