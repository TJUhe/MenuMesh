#include "algorithms/feature_detection/FeatureComparison.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "core/MathConstants.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace manumesh::feature {
namespace {

std::vector<int> circularLoopIndices(const FeatureAnalysis& analysis) {
    std::vector<int> indices;
    for (int i = 0; i < static_cast<int>(analysis.loops.size()); ++i) {
        if (analysis.loops[i].circular) {
            indices.push_back(i);
        }
    }
    return indices;
}

} // namespace

Status validateLoopMatchOptions(const LoopMatchOptions& options) {
    auto finiteNonNegative = [](double value) { return std::isfinite(value) && value >= 0.0; };
    if (!finiteNonNegative(options.plausibleCenterErrorRatio) ||
        !finiteNonNegative(options.plausibleRadiusErrorRel) ||
        !finiteNonNegative(options.matchedCenterErrorRatio) ||
        !finiteNonNegative(options.matchedRadiusErrorRel)) {
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

    LoopMatchReport report;
    const std::vector<int> originalCircular = circularLoopIndices(original);
    const std::vector<int> simplifiedCircular = circularLoopIndices(simplifiedFeatures);
    report.originalCircularLoops = static_cast<int>(originalCircular.size());
    report.simplifiedCircularLoops = static_cast<int>(simplifiedCircular.size());
    report.matches.reserve(originalCircular.size());

    const double diag =
        std::max(1e-12, options.referenceDiagonal > 0.0 ? options.referenceDiagonal : simplified.bboxDiag());
    std::vector<char> usedSimplified(simplifiedFeatures.loops.size(), 0);

    for (int originalId : originalCircular) {
        const FeatureLoop& origLoop = original.loops[originalId];
        int bestLoopId = -1;
        double bestScore = std::numeric_limits<double>::infinity();
        double bestCenterError = 0.0;
        double bestRadiusError = 0.0;
        double bestNormalAngleDeg = 0.0;

        for (int simplifiedId : simplifiedCircular) {
            if (usedSimplified[simplifiedId]) {
                continue;
            }
            const FeatureLoop& simpLoop = simplifiedFeatures.loops[simplifiedId];
            const double centerError = (origLoop.center - simpLoop.center).norm();
            const double radiusError = std::abs(origLoop.radius - simpLoop.radius);
            const double normalDot =
                std::clamp(std::abs(origLoop.normal.normalized().dot(simpLoop.normal.normalized())), 0.0, 1.0);
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
            const FeatureLoop& simpLoop = simplifiedFeatures.loops[bestLoopId];
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

} // namespace manumesh::feature
