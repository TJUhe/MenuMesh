#pragma once

#include "detail/SimplificationTypes.h"
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"
#include "line_quadrics_qem/features/FeatureDetection.h"

#include <vector>

namespace lq {

double evaluateQuadric(const Mat4& q, const Vec3& p);
Mat4 planeQuadric(const Vec3& normal, const Vec3& point);
Mat4 pointQuadric(const Vec3& point);
Mat4 lineQuadric(const Vec3& point, const Vec3& normal);

void computeInitialQuadrics(const Mesh& mesh, const SimplifyOptions& options,
                            const FeatureAnalysis* featureAnalysis,
                            std::vector<Mat4>& quadrics, double& minLineWeight,
                            double& maxLineWeight);

std::vector<SolveResult> solvePlacementCandidates(const Mat4& q, const Vec3& a,
                                                  const Vec3& b);
SolveResult solveOptimal(const Mat4& q, const Vec3& a, const Vec3& b);

class InitialQuadricBuilder {
public:
  explicit InitialQuadricBuilder(const SimplifyOptions& options);

  std::vector<Mat4> build(const Mesh& mesh, const FeatureAnalysis* featureAnalysis,
                          SimplifyReport& report) const;

private:
  const SimplifyOptions& options_;
};

} // namespace lq
