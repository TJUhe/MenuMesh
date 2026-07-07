#pragma once

#include "detail/SimplificationTypes.h"
#include "manumesh/algorithms/feature_detection/FeatureTypes.h"
#include "manumesh/algorithms/simplification/QEMSimplifier.h"

#include <vector>

namespace manumesh::simplification {

double evaluateQuadric(const Mat4& q, const Vec3& p);
Mat4 planeQuadric(const Vec3& normal, const Vec3& point);
Mat4 pointQuadric(const Vec3& point);
Mat4 lineQuadric(const Vec3& point, const Vec3& normal);

void computeInitialQuadrics(const Mesh& mesh, const SimplifyOptions& options,
                            const feature::FeatureAnalysis* featureAnalysis,
                            std::vector<Mat4>& quadrics, double& minLineWeight,
                            double& maxLineWeight);

std::vector<SolveResult> solvePlacementCandidates(const Mat4& q, const Vec3& a,
                                                  const Vec3& b);
SolveResult solveOptimal(const Mat4& q, const Vec3& a, const Vec3& b);

class InitialQuadricBuilder {
public:
  explicit InitialQuadricBuilder(const SimplifyOptions& options);

  std::vector<Mat4> build(const Mesh& mesh,
                          const feature::FeatureAnalysis* featureAnalysis,
                          SimplifyReport& report) const;

private:
  const SimplifyOptions& options_;
};

} // namespace manumesh::simplification
