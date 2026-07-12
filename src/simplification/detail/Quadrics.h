#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"
#include "detail/FeatureGuidance.h"
#include "detail/SimplificationTypes.h"

#include <vector>

namespace manumesh::simplification {

double evaluateQuadric(const Mat4& q, const Vec3& p);
Mat4 planeQuadric(const Vec3& normal, const Vec3& point);
Mat4 pointQuadric(const Vec3& point);
Mat4 lineQuadric(const Vec3& point, const Vec3& normal);

/// Initial per-vertex quadrics plus the queue-priority factors decoupled from
/// them (Wang 2008): feature boosts in adaptiveScale mode only reorder the
/// candidate queue and never distort the placement solve.
struct InitialQuadrics {
    std::vector<Mat4> quadrics;
    /// Per-vertex multipliers (>= 1) for the queue ordering cost. Empty when
    /// no decoupled boost applies; every vertex then uses 1.0.
    std::vector<double> priorityScales;
};

void computeInitialQuadrics(
    const Mesh& mesh,
    const SimplifyOptions& options,
    const FeatureGuidance& featureGuidance,
    InitialQuadrics& initial,
    SimplifyReport& report
);

std::vector<SolveResult> solvePlacementCandidates(const Mat4& q, const Vec3& a, const Vec3& b);
SolveResult solveOptimal(const Mat4& q, const Vec3& a, const Vec3& b);

class InitialQuadricBuilder {
public:
    explicit InitialQuadricBuilder(const SimplifyOptions& options);

    InitialQuadrics build(const Mesh& mesh, const FeatureGuidance& featureGuidance, SimplifyReport& report) const;

private:
    const SimplifyOptions& options_;
};

} // namespace manumesh::simplification
