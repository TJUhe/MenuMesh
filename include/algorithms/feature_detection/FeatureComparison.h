/**
 * @file include/algorithms/feature_detection/FeatureComparison.h
 * @brief Declares feature comparison facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "Export.h"
#include "algorithms/feature_detection/FeatureTypes.h"
#include "core/Status.h"

#include <string>
#include <vector>

namespace manumesh::feature {

/// Thresholds for greedy circular-loop matching between two feature analyses.
///
/// A candidate pair is *plausible* when it stays inside the plausible*
/// thresholds; a plausible pair is reported as Matched when it also stays
/// inside the tighter matched* thresholds, otherwise as WeakMatch. Defaults
/// equal the historical `manumesh feature-compare` hard-coded values.
struct LoopMatchOptions {
    /// Max center distance for a plausible match, relative to referenceDiagonal.
    double plausibleCenterErrorRatio = 0.08;
    /// Max |radius error| / original radius for a plausible match.
    double plausibleRadiusErrorRel = 0.20;
    /// Max normal angle for a plausible match, in degrees.
    double plausibleNormalAngleDeg = 30.0;
    /// Max center distance for a strong match, relative to referenceDiagonal.
    double matchedCenterErrorRatio = 0.04;
    /// Max |radius error| / original radius for a strong match.
    double matchedRadiusErrorRel = 0.08;
    /// Max normal angle for a strong match, in degrees.
    double matchedNormalAngleDeg = 15.0;
    /// Length used to normalize center errors (typically the original mesh's
    /// bounding-box diagonal). Values <= 0 fall back to the simplified mesh's
    /// bounding-box diagonal.
    double referenceDiagonal = 0.0;
};

/// Validates loop-match thresholds. All values must be finite and
/// non-negative, angles must be at most 180 degrees, and each matched
/// threshold must be no wider than its plausible counterpart.
MANUMESH_API Status validateLoopMatchOptions(const LoopMatchOptions& options);

/// Match classification for one original circular loop.
enum class LoopMatchStatus {
    Matched,
    WeakMatch,
    Missing,
};

/// Match result for one circular loop of the original analysis.
///
/// For Missing results the error fields are zero and simplifiedLoopIndex is
/// -1, mirroring the historical CLI report semantics.
struct LoopMatch {
    /// FeatureLoop::id of the original loop.
    int originalLoopId = -1;
    /// Index into the simplified analysis' loops vector, -1 when missing.
    int simplifiedLoopIndex = -1;
    int originalVertices = 0;
    int simplifiedVertices = 0;
    double originalRadius = 0.0;
    double simplifiedRadius = 0.0;
    /// Center distance in mesh units.
    double centerError = 0.0;
    /// Absolute radius difference in mesh units.
    double radiusError = 0.0;
    /// Angle between loop normals in degrees.
    double normalAngleDeg = 0.0;
    /// Simplified loop measured against the original circle (only filled for
    /// Matched/WeakMatch results).
    DirectionalCurveError directional;
    LoopMatchStatus status = LoopMatchStatus::Missing;
};

/// Greedy circular-loop match report between two feature analyses.
struct LoopMatchReport {
    /// One entry per circular loop of the original analysis, in loop order.
    std::vector<LoopMatch> matches;
    int originalCircularLoops = 0;
    int simplifiedCircularLoops = 0;
    /// Matched + WeakMatch entries.
    int matchedLoops = 0;
    int missingLoops = 0;
};

/// Greedily matches the circular loops of `original` against plausible
/// circular loops of `simplifiedFeatures`, choosing the lowest combined
/// center/radius/normal score after applying all three plausible thresholds.
/// Chosen pairs are classified with the matched thresholds in `options`.
/// `simplified` is the mesh that produced `simplifiedFeatures`; it is used to
/// measure matched loops against the original circle. Each simplified loop is
/// consumed by at most one original loop. Deterministic for identical inputs.
/// Throws std::invalid_argument when `options` violates
/// validateLoopMatchOptions().
/// @param[in] original Feature analysis of the reference mesh.
/// @param[in] simplifiedFeatures Feature analysis of `simplified`.
/// @param[in] simplified Mesh containing candidate loop vertices.
/// @param[in] options Plausible and strong matching gates.
/// @return One deterministic match record per original circular loop.
/// @algorithm Filters candidates by center, radius, and unoriented normal
/// gates, then greedily consumes the lowest combined normalized error. The
/// tighter gates classify accepted pairs as strong or weak matches.
/// @complexity O(L_o * L_s), excluding directional curve measurement.
MANUMESH_API LoopMatchReport matchCircularLoops(
    const FeatureAnalysis& original,
    const FeatureAnalysis& simplifiedFeatures,
    const Mesh& simplified,
    const LoopMatchOptions& options = {}
);

/// Stable string name for a loop-match status ("matched", "weak_match",
/// "missing").
MANUMESH_API std::string toString(LoopMatchStatus status);

} // namespace manumesh::feature
