/**
 * @file src/simplification/SimplificationPolicies.cpp
 * @brief 实现 ManuMesh 简化模块的策略归一化。
 * @ingroup manumesh_simplification
 *
 * @details 将用户选项归一化为不可变的热循环策略开关和阈值。
 * @invariants 策略派生不执行依赖网格的工作；每个被禁用的可选通道都有明确的零值或 false 表示。
 */

#include "detail/SimplificationPolicies.h"

#include "common/detail/MathConstants.h"

#include <algorithm>
#include <cmath>

namespace manumesh {
namespace simplification {

using manumesh::common::kPi;

namespace {

void applyTargetConfig(const SimplifyTarget& target, SimplifyOptions& options) {
    if (target.kind() == SimplifyTarget::Kind::FaceCount) {
        options.targetFaces = target.faceCount();
        // 旧选项用 -1 表示“改用比例”。显式面数目标不能沿用这个含义，
        // 因此在兼容边界将它保留为无效值，交由现有校验统一拒绝。
        if (options.targetFaces == -1) {
            options.targetFaces = 0;
        }
        return;
    }

    options.targetFaces = -1;
    options.targetRatio = target.ratio();
}

void applyCostConfig(const SimplifyCostOptions& config, SimplifyOptions& options) {
    options.useLineQuadrics = config.lineQuadrics.kind() != LineQuadricConfig::Kind::Disabled;
    options.adaptiveScale = config.lineQuadrics.kind() == LineQuadricConfig::Kind::Adaptive;
    options.lineWeight = options.adaptiveScale || !options.useLineQuadrics ? 0.0 : config.lineQuadrics.weight();
    options.adaptiveBaseLineWeight = options.adaptiveScale ? config.lineQuadrics.weight() : 0.0;
    options.weightMode = config.weightMode;
    options.featureBoost = config.featureBoost;
    options.boundaryWeight = config.boundaryWeight;
}

void applyFeatureConfig(const SimplifyFeatureOptions& config, SimplifyOptions& options) {
    options.preserveFeatureCurves = config.enabled;
    options.featureProtectionMode = config.protectionMode;
    options.featureCurveWeight = config.curveWeight;
    options.maxFeatureCurveDeviationRatio = config.maxCurveDeviationRatio;
    options.minCircularFeatureLoopVertices = config.minCircularLoopVertices;
    options.featureOptionsOverride = config.detection;
}

void applyQualityConfig(const SimplifyQualityOptions& config, SimplifyOptions& options) {
    options.preserveBoundary = config.preserveBoundary;
    options.minTriangleQuality = config.minTriangleQuality;
    options.maxNormalDeviationDeg = config.maxNormalDeviationDeg;
    options.maxLocalError = 0.0;
    options.maxLocalErrorRatio = 0.0;
    if (config.localError.kind() == SimplifyErrorLimit::Kind::Absolute) {
        options.maxLocalError = config.localError.value();
    } else if (config.localError.kind() == SimplifyErrorLimit::Kind::BoundingBoxRatio) {
        options.maxLocalErrorRatio = config.localError.value();
    }
    options.preventLocalIntersections = config.preventLocalIntersections;
    options.qualityRefinementIterations = config.refinementIterations;
}

void applyTextureConfig(const SimplifyTextureOptions& config, SimplifyOptions& options) {
    options.preserveTexture = config.preserveTexture;
    options.textureWeight = config.weight;
    options.textureSeamTolerance = config.seamTolerance;
    options.minTextureAreaRatio = config.minAreaRatio;
}

} // namespace

int TargetPolicy::resolveTargetFaceCount(int inputFaceCount) const {
    if (targetFaces > 0) {
        return targetFaces;
    }
    return std::max(4, static_cast<int>(std::llround(inputFaceCount * targetRatio)));
}

SimplifyOptions makeSimplifyOptions(const SimplifyConfig& config) {
    SimplifyOptions options;
    applyTargetConfig(config.target, options);
    applyCostConfig(config.cost, options);
    applyFeatureConfig(config.features, options);
    applyQualityConfig(config.quality, options);
    applyTextureConfig(config.texture, options);
    options.verbose = config.verbose;
    return options;
}

SimplifyConfig makeSimplifyConfig(feature::FeatureProfile profile) {
    SimplifyConfig config;
    // Profiles are the explicit modern entry point: unlike raw SimplifyConfig{}
    // (which preserves the 0.x threshold of 16), they use FeatureOptions' 8-vertex
    // detector default and then apply only the profile-specific evidence channels.
    config.features.detection = feature::makeFeatureOptions(profile);

    switch (profile) {
    case feature::FeatureProfile::Default:
        break;
    case feature::FeatureProfile::Cad:
        config.cost.weightMode = WeightMode::Dihedral;
        config.features.enabled = true;
        config.features.protectionMode = FeatureProtectionMode::PrimitiveCurves;
        break;
    case feature::FeatureProfile::NoisyScan:
        config.cost.weightMode = WeightMode::NormalTensor;
        config.features.enabled = true;
        // Tensor evidence is deliberately soft by default: noisy local classifications
        // must not prevent the simplifier from reaching a practical target.
        config.features.protectionMode = FeatureProtectionMode::PrimitiveCurves;
        break;
    }
    return config;
}

feature::FeatureOptions
featureOptionsFromSimplifyOptions(const SimplifyOptions& options, int minFeatureLoopVerticesFloor) {
    if (options.featureOptionsOverride.has_value()) {
        feature::FeatureOptions resolved = *options.featureOptionsOverride;
        resolved.minFeatureLoopVertices = std::max(minFeatureLoopVerticesFloor, resolved.minFeatureLoopVertices);
        return resolved;
    }

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

} // namespace simplification
} // namespace manumesh
