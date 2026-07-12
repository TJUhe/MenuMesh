#include "CliOptionBinding.h"

#include <algorithm>
#include <stdexcept>

namespace manumesh::cli {

simplification::SimplifyOptions parseSimplifyOptions(const Args& args) {
    if (hasFlag(args, "--smooth-curvature-features") || hasFlag(args, "--smooth-curvature-threshold") ||
        hasFlag(args, "--smooth-curvature-edge-alignment") || hasFlag(args, "--smooth-curvature-tangent-consistency") ||
        hasFlag(args, "--smooth-curvature-base-rings") || hasFlag(args, "--smooth-curvature-scales") ||
        hasFlag(args, "--smooth-curvature-min-persistent-scales") ||
        hasFlag(args, "--smooth-curvature-robust-iterations")) {
        throw std::invalid_argument(
            "Smooth-curvature CLI options are feature-analysis options. Use feature-report, feature-benchmark, or "
            "feature-compare; C++ simplification can consume a precomputed FeatureAnalysis."
        );
    }
    simplification::SimplifyOptions options;
    options.targetRatio = getDoubleArg(args, "--ratio", options.targetRatio);
    options.targetFaces = getIntArg(args, "--target-faces", options.targetFaces);
    options.lineWeight = getDoubleArg(args, "--line-weight", options.lineWeight);
    options.featureBoost = getDoubleArg(args, "--feature-boost", options.featureBoost);
    options.featureAngleDeg = getDoubleArg(args, "--feature-angle-deg", options.featureAngleDeg);
    options.loopTraceAngleDeg = getDoubleArg(args, "--loop-trace-angle-deg", options.loopTraceAngleDeg);
    options.boundaryWeight = getDoubleArg(args, "--boundary-weight", options.boundaryWeight);
    options.featureCurveWeight = getDoubleArg(args, "--feature-curve-weight", options.featureCurveWeight);
    options.maxFeatureCurveDeviationRatio =
        getDoubleArg(args, "--max-feature-curve-deviation-ratio", options.maxFeatureCurveDeviationRatio);
    options.circleFitRelativeThreshold =
        getDoubleArg(args, "--circle-fit-threshold", options.circleFitRelativeThreshold);
    options.ellipseFitRelativeThreshold =
        getDoubleArg(args, "--ellipse-fit-threshold", options.ellipseFitRelativeThreshold);
    options.nearCircleAxisRatioTolerance =
        getDoubleArg(args, "--near-circle-axis-ratio-tolerance", options.nearCircleAxisRatioTolerance);
    options.minFeatureLoopVertices = getIntArg(args, "--min-feature-loop-vertices", options.minFeatureLoopVertices);
    options.minCircularFeatureLoopVertices =
        getIntArg(args, "--min-circular-feature-loop-vertices", options.minCircularFeatureLoopVertices);
    options.adaptiveBaseLineWeight = getDoubleArg(args, "--adaptive-base-line-weight", options.adaptiveBaseLineWeight);
    options.normalTensorFeatureThreshold =
        getDoubleArg(args, "--normal-tensor-threshold", options.normalTensorFeatureThreshold);
    options.normalTensorMinEdgeAlignment =
        getDoubleArg(args, "--normal-tensor-edge-alignment", options.normalTensorMinEdgeAlignment);
    options.normalTensorSmoothingIterations =
        getIntArg(args, "--normal-tensor-smoothing", options.normalTensorSmoothingIterations);
    options.normalTensorScaleCount = getIntArg(args, "--normal-tensor-scales", options.normalTensorScaleCount);
    options.normalTensorMinPersistentScales =
        getIntArg(args, "--normal-tensor-min-persistent-scales", options.normalTensorMinPersistentScales);
    options.featureGraphGapLengthRatio =
        getDoubleArg(args, "--feature-graph-gap-ratio", options.featureGraphGapLengthRatio);
    options.featureGraphMaxWeakSpurEdges =
        getIntArg(args, "--feature-graph-max-weak-spur-edges", options.featureGraphMaxWeakSpurEdges);
    options.featureComponentMinConfidence =
        getDoubleArg(args, "--feature-component-min-confidence", options.featureComponentMinConfidence);
    options.minTriangleQuality = getDoubleArg(args, "--min-triangle-quality", options.minTriangleQuality);
    options.maxNormalDeviationDeg = getDoubleArg(args, "--max-normal-deviation-deg", options.maxNormalDeviationDeg);
    options.maxLocalError = getDoubleArg(args, "--max-local-error", options.maxLocalError);
    options.maxLocalErrorRatio = getDoubleArg(args, "--max-local-error-ratio", options.maxLocalErrorRatio);
    options.qualityRefinementIterations =
        getIntArg(args, "--quality-refinement-iterations", options.qualityRefinementIterations);
    options.verbose = hasFlag(args, "--verbose");
    options.adaptiveScale = hasFlag(args, "--adaptive-scale");
    options.preserveBoundary = hasFlag(args, "--preserve-boundary");
    options.preventLocalIntersections = hasFlag(args, "--prevent-local-intersections");
    options.preserveFeatureCurves = hasFlag(args, "--preserve-feature-curves");
    options.featureProtectionMode = simplification::parseFeatureProtectionMode(
        getArg(args, "--feature-protection-mode", simplification::toString(options.featureProtectionMode))
    );
    options.useNormalTensorFeatures = !hasFlag(args, "--no-normal-tensor-features");
    options.cleanupFeatureGraph = !hasFlag(args, "--no-feature-graph-cleanup");
    if (hasFlag(args, "--industrial-safe")) {
        options.preserveBoundary = true;
        options.minTriangleQuality = std::max(options.minTriangleQuality, 1e-4);
        options.maxNormalDeviationDeg = std::min(options.maxNormalDeviationDeg, 75.0);
        options.maxLocalErrorRatio = std::max(options.maxLocalErrorRatio, 0.02);
        options.preventLocalIntersections = true;
        options.qualityRefinementIterations = std::max(options.qualityRefinementIterations, 2);
    }

    const std::string mode = getArg(args, "--weight-mode", "uniform");
    options.weightMode = simplification::parseWeightMode(mode);

    const std::string method = getArg(args, "--method", "line");
    if (method == "standard" || method == "qem") {
        options.useLineQuadrics = false;
        options.lineWeight = 0.0;
    } else if (method == "line") {
        options.useLineQuadrics = true;
    } else {
        throw std::invalid_argument("Unknown --method. Use standard or line.");
    }
    return options;
}

feature::FeatureOptions parseFeatureOptions(const Args& args) {
    feature::FeatureOptions options;
    options.featureAngleDeg = getDoubleArg(args, "--feature-angle-deg", options.featureAngleDeg);
    options.loopTraceAngleDeg = getDoubleArg(args, "--loop-trace-angle-deg", options.loopTraceAngleDeg);
    options.circleFitRelativeThreshold =
        getDoubleArg(args, "--circle-fit-threshold", options.circleFitRelativeThreshold);
    options.ellipseFitRelativeThreshold =
        getDoubleArg(args, "--ellipse-fit-threshold", options.ellipseFitRelativeThreshold);
    options.nearCircleAxisRatioTolerance =
        getDoubleArg(args, "--near-circle-axis-ratio-tolerance", options.nearCircleAxisRatioTolerance);
    options.minFeatureLoopVertices = getIntArg(args, "--min-feature-loop-vertices", options.minFeatureLoopVertices);
    options.normalTensorFeatureThreshold =
        getDoubleArg(args, "--normal-tensor-threshold", options.normalTensorFeatureThreshold);
    options.normalTensorMinEdgeAlignment =
        getDoubleArg(args, "--normal-tensor-edge-alignment", options.normalTensorMinEdgeAlignment);
    options.normalTensorSmoothingIterations =
        getIntArg(args, "--normal-tensor-smoothing", options.normalTensorSmoothingIterations);
    options.normalTensorScaleCount = getIntArg(args, "--normal-tensor-scales", options.normalTensorScaleCount);
    options.normalTensorMinPersistentScales =
        getIntArg(args, "--normal-tensor-min-persistent-scales", options.normalTensorMinPersistentScales);
    options.smoothCurvatureFeatureThreshold =
        getDoubleArg(args, "--smooth-curvature-threshold", options.smoothCurvatureFeatureThreshold);
    options.smoothCurvatureMinEdgeAlignment =
        getDoubleArg(args, "--smooth-curvature-edge-alignment", options.smoothCurvatureMinEdgeAlignment);
    options.smoothCurvatureMinTangentConsistency =
        getDoubleArg(args, "--smooth-curvature-tangent-consistency", options.smoothCurvatureMinTangentConsistency);
    options.smoothCurvatureBaseNeighborhoodRings =
        getIntArg(args, "--smooth-curvature-base-rings", options.smoothCurvatureBaseNeighborhoodRings);
    options.smoothCurvatureScaleCount = getIntArg(args, "--smooth-curvature-scales", options.smoothCurvatureScaleCount);
    options.smoothCurvatureMinPersistentScales =
        getIntArg(args, "--smooth-curvature-min-persistent-scales", options.smoothCurvatureMinPersistentScales);
    options.smoothCurvatureRobustFitIterations =
        getIntArg(args, "--smooth-curvature-robust-iterations", options.smoothCurvatureRobustFitIterations);
    options.featureGraphGapLengthRatio =
        getDoubleArg(args, "--feature-graph-gap-ratio", options.featureGraphGapLengthRatio);
    options.featureGraphMaxWeakSpurEdges =
        getIntArg(args, "--feature-graph-max-weak-spur-edges", options.featureGraphMaxWeakSpurEdges);
    options.featureComponentMinConfidence =
        getDoubleArg(args, "--feature-component-min-confidence", options.featureComponentMinConfidence);
    options.useNormalTensorFeatures = !hasFlag(args, "--no-normal-tensor-features");
    options.useSmoothCurvatureFeatures = hasFlag(args, "--smooth-curvature-features");
    options.cleanupFeatureGraph = !hasFlag(args, "--no-feature-graph-cleanup");
    return options;
}

} // namespace manumesh::cli
