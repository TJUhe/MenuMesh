/**
 * @file src/simplification/detail/Quadrics.h
 * @brief Declares quadrics facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"
#include "detail/FeatureGuidance.h"
#include "detail/SimplificationTypes.h"

#include <vector>

namespace manumesh::simplification {

/**
 * @brief Evaluates homogeneous error `[p,1]^T q [p,1]`.
 */
double evaluateQuadric(const Mat4& q, const Vec3& p);
/**
 * @return Point-to-plane quadric through `point` with unit `normal`.
 */
Mat4 planeQuadric(const Vec3& normal, const Vec3& point);
/**
 * @return Isotropic point-distance quadric centered at `point`.
 */
Mat4 pointQuadric(const Vec3& point);
/**
 * @return Point-to-line quadric through `point` along `normal`.
 */
Mat4 lineQuadric(const Vec3& point, const Vec3& normal);

/**
 * @brief Initial per-vertex quadrics plus queue-priority factors decoupled from them.
 *
 * In adaptive mode, Wang-style feature boosts only reorder the queue and do
 * not distort the placement solve.
 */
struct InitialQuadrics {
    std::vector<Mat4> quadrics;
    /**
     * @brief Per-vertex multipliers (>= 1) for the queue ordering cost. Empty when
     * no decoupled boost applies; every vertex then uses 1.0.
     */
    std::vector<double> priorityScales;
};

/**
 * @brief Accumulates all enabled initial quadric terms and report diagnostics.
 */
void computeInitialQuadrics(
    const Mesh& mesh,
    const SimplifyOptions& options,
    const FeatureGuidance& featureGuidance,
    InitialQuadrics& initial,
    SimplifyReport& report
);

/**
 * @brief Returns unique finite placement candidates sorted by ascending quadric cost.
 */
std::vector<SolveResult> solvePlacementCandidates(const Mat4& q, const Vec3& a, const Vec3& b);
/**
 * @deprecated Prefer solvePlacementCandidates so legality can try fallbacks.
 */
SolveResult solveOptimal(const Mat4& q, const Vec3& a, const Vec3& b);

/**
 * @brief Builds all initial geometry and guidance quadrics for one immutable input mesh.
 */
class InitialQuadricBuilder {
public:
    /** @brief Binds one immutable simplification option set. */
    explicit InitialQuadricBuilder(const SimplifyOptions& options);

    /** @brief Computes initial quadrics, priority scales, and report diagnostics. */
    InitialQuadrics build(const Mesh& mesh, const FeatureGuidance& featureGuidance, SimplifyReport& report) const;

private:
    const SimplifyOptions& options_;
};

} // namespace manumesh::simplification
