/**
 * @file src/simplification/detail/SimplificationPolicies.h
 * @brief Declares simplification policies facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "algorithms/simplification/SimplificationTypes.h"

namespace manumesh::simplification {

/**
 * @brief Normalized absolute/relative target selection.
 */
struct TargetPolicy {
    int targetFaces = -1;
    double targetRatio = 0.25;

    /** @brief Resolves the clamped target face count for an input mesh. */
    int resolveTargetFaceCount(int inputFaceCount) const;
};

/**
 * @brief Feature-analysis settings derived from simplification options.
 */
struct FeatureDetectionPolicy {
    bool enabled = false;
    feature::FeatureOptions options;
};

/**
 * @brief Converts simplification feature fields to the standalone detector contract.
 */
feature::FeatureOptions
featureOptionsFromSimplifyOptions(const SimplifyOptions& options, int minFeatureLoopVerticesFloor = 0);

/**
 * @brief Pre-normalized hot-loop switches for hard collapse filters.
 */
struct LegalityPolicy {
    bool preserveBoundary = false;
    double minTriangleQuality = 0.0;
    double maxNormalDeviationDeg = 90.0;
    double maxLocalError = 0.0;
    double maxLocalErrorRatio = 0.0;
    bool preventLocalIntersections = false;

    /** @brief Converts the angular normal limit to a cosine threshold. */
    double resolveMinNormalDot() const;
    /** @brief Resolves the effective absolute local-error limit. */
    double resolveMaxLocalError(double bboxDiag) const;
};

/**
 * @brief Immutable normalized policies shared by every collapse attempt in one run.
 */
struct SimplificationPolicies {
    TargetPolicy target;
    FeatureDetectionPolicy features;
    LegalityPolicy legality;

    /** @brief Normalizes all hot-loop policies from public simplify options. */
    static SimplificationPolicies fromOptions(const SimplifyOptions& options);
};

} // namespace manumesh::simplification
