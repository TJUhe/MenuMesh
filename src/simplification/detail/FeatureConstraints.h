#pragma once

#include "detail/SimplificationTypes.h"
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"

#include <vector>

namespace lq {

Vec3 projectToCircle(const Vec3& p, const VertexState& feature);
Vec3 projectToEllipse(const Vec3& p, const VertexState& feature);
void refreshCircularTangent(VertexState& vertex);
void refreshEllipseTangent(VertexState& vertex);
bool projectBoundaryPlacement(int keep, int remove,
                              const BoundaryCollapseDecision& decision,
                              const std::vector<VertexState>& vertices, Vec3& position);

class FeatureConstraintPolicy {
public:
  explicit FeatureConstraintPolicy(const SimplifyOptions& options);

  bool canCollapse(int keep, int remove, const std::vector<VertexState>& vertices,
                   const std::vector<int>& activeLoopCounts) const;
  bool projectPlacement(int keep, int remove, const std::vector<VertexState>& vertices,
                        const std::vector<FeatureCurveConstraint>& curves,
                        Vec3& position) const;

private:
  const SimplifyOptions& options_;
};

} // namespace lq
