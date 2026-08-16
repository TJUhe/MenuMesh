/**
 * @file src/feature_detection/FeatureComparison.cpp
 * @brief 匹配两份分析中的圆形特征并计算保持误差。
 * @ingroup manumesh_feature_detection
 *
 * @details 匹配两份分析中的圆形特征，用于评估特征保持情况。
 * @algorithm 先按中心、半径和无向法向的可行范围筛选候选；确定性贪心匹配
 *            每次消耗归一化误差最低的候选，并对强证据与弱证据使用不同的门限。
 */

#include "algorithms/feature_detection/FeatureComparison.h"
#include "algorithms/feature_detection/FeatureAnalysisViews.h"
#include "core/MathUtils.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "core/MathConstants.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace manumesh {
namespace feature {
namespace {

std::vector<int> circularLoopIndices(const FeatureCurveView& curves) {
    std::vector<int> indices;
    for (int i = 0; i < static_cast<int>(curves.loops().size()); ++i) {
        if (curves.loops()[i].circular) {
            indices.push_back(i);
        }
    }
    return indices;
}

} // namespace

Status validateLoopMatchOptions(const LoopMatchOptions& options) {
    auto finiteNonNegative = [](double value) {
        return std::isfinite(value) && value >= 0.0;
    };
    if (!finiteNonNegative(options.plausibleCenterErrorRatio) || !finiteNonNegative(options.plausibleRadiusErrorRel) ||
        !finiteNonNegative(options.matchedCenterErrorRatio) || !finiteNonNegative(options.matchedRadiusErrorRel)) {
        return Status::invalidArgument("Loop match distance and radius thresholds must be finite and non-negative.");
    }
    if (!finiteNonNegative(options.plausibleNormalAngleDeg) || options.plausibleNormalAngleDeg > 180.0 ||
        !finiteNonNegative(options.matchedNormalAngleDeg) || options.matchedNormalAngleDeg > 180.0) {
        return Status::invalidArgument("Loop match normal-angle thresholds must be finite and in [0, 180].");
    }
    if (!finiteNonNegative(options.referenceDiagonal)) {
        return Status::invalidArgument("Loop match referenceDiagonal must be finite and non-negative.");
    }
    if (options.matchedCenterErrorRatio > options.plausibleCenterErrorRatio ||
        options.matchedRadiusErrorRel > options.plausibleRadiusErrorRel ||
        options.matchedNormalAngleDeg > options.plausibleNormalAngleDeg) {
        return Status::invalidArgument("Matched loop thresholds must not be wider than plausible thresholds.");
    }
    return Status::success();
}

LoopMatchReport matchCircularLoops(
    const FeatureAnalysis& original,
    const FeatureAnalysis& simplifiedFeatures,
    const Mesh& simplified,
    const LoopMatchOptions& options
) {
    const Status optionsStatus = validateLoopMatchOptions(options);
    if (!optionsStatus.ok()) {
        throw std::invalid_argument(optionsStatus.message());
    }
    validateFeatureAnalysis(simplified, simplifiedFeatures);

    const FeatureCurveView originalCurves = viewFeatureCurves(original);
    const FeatureCurveView simplifiedCurves = viewFeatureCurves(simplifiedFeatures);
    LoopMatchReport report;
    const std::vector<int> originalCircular = circularLoopIndices(originalCurves);
    const std::vector<int> simplifiedCircular = circularLoopIndices(simplifiedCurves);
    report.originalCircularLoops = static_cast<int>(originalCircular.size());
    report.simplifiedCircularLoops = static_cast<int>(simplifiedCircular.size());
    report.matches.reserve(originalCircular.size());

    const double diag =
        std::max(1e-12, options.referenceDiagonal > 0.0 ? options.referenceDiagonal : simplified.bboxDiag());
    std::vector<char> usedSimplified(simplifiedCurves.loops().size(), 0);

    for (int originalId : originalCircular) {
        const FeatureLoop& origLoop = originalCurves.loops()[originalId];
        int bestLoopId = -1;
        double bestScore = std::numeric_limits<double>::infinity();
        double bestCenterError = 0.0;
        double bestRadiusError = 0.0;
        double bestNormalAngleDeg = 0.0;

        for (int simplifiedId : simplifiedCircular) {
            if (usedSimplified[simplifiedId]) {
                continue;
            }
            const FeatureLoop& simpLoop = simplifiedCurves.loops()[simplifiedId];
            const double centerError = (origLoop.center - simpLoop.center).norm();
            const double radiusError = std::abs(origLoop.radius - simpLoop.radius);
            const double normalDot = manumesh::clampValue(
                std::abs(origLoop.normal.normalized().dot(simpLoop.normal.normalized())), 0.0, 1.0
            );
            const double normalAngle = std::acos(normalDot);
            const double normalAngleDeg = normalAngle * 180.0 / kPi;
            const double radiusRel = radiusError / std::max(1e-12, origLoop.radius);
            if (centerError > options.plausibleCenterErrorRatio * diag || radiusRel > options.plausibleRadiusErrorRel ||
                normalAngleDeg > options.plausibleNormalAngleDeg) {
                continue;
            }
            const double score = centerError / diag + radiusRel + normalAngle / kPi;
            if (score < bestScore) {
                bestScore = score;
                bestLoopId = simplifiedId;
                bestCenterError = centerError;
                bestRadiusError = radiusError;
                bestNormalAngleDeg = normalAngleDeg;
            }
        }

        LoopMatch match;
        match.originalLoopId = origLoop.id;
        match.originalVertices = static_cast<int>(origLoop.vertices.size());
        match.originalRadius = origLoop.radius;

        if (bestLoopId >= 0) {
            const FeatureLoop& simpLoop = simplifiedCurves.loops()[bestLoopId];
            usedSimplified[bestLoopId] = 1;
            match.directional =
                measureLoopAgainstCircle(simplified, simpLoop, origLoop.center, origLoop.normal, origLoop.radius);
            match.simplifiedVertices = static_cast<int>(simpLoop.vertices.size());
            match.simplifiedRadius = simpLoop.radius;
            const double radiusRel = bestRadiusError / std::max(1e-12, origLoop.radius);
            match.simplifiedLoopIndex = bestLoopId;
            match.centerError = bestCenterError;
            match.radiusError = bestRadiusError;
            match.normalAngleDeg = bestNormalAngleDeg;
            match.status =
                (bestCenterError <= options.matchedCenterErrorRatio * diag &&
                 radiusRel <= options.matchedRadiusErrorRel && bestNormalAngleDeg <= options.matchedNormalAngleDeg)
                    ? LoopMatchStatus::Matched
                    : LoopMatchStatus::WeakMatch;
            ++report.matchedLoops;
        } else {
            match.status = LoopMatchStatus::Missing;
            ++report.missingLoops;
        }
        report.matches.push_back(match);
    }
    return report;
}

std::string toString(LoopMatchStatus status) {
    switch (status) {
    case LoopMatchStatus::Matched:
        return "matched";
    case LoopMatchStatus::WeakMatch:
        return "weak_match";
    case LoopMatchStatus::Missing:
        return "missing";
    }
    return "missing";
}

} // namespace feature
} // namespace manumesh
