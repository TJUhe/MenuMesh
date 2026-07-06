#pragma once

#include "detail/SimplificationTypes.h"
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"

#include <vector>

namespace lq::simplification {

Vec3 projectToCircle(const Vec3& p, const VertexState& feature);
Vec3 projectToEllipse(const Vec3& p, const VertexState& feature);
void refreshCircularTangent(VertexState& vertex);
void refreshEllipseTangent(VertexState& vertex);

struct BoundaryProjectionInput {
  CollapseEdge edge;
  const BoundaryCollapseDecision& decision;
  const std::vector<VertexState>& vertices;
};

bool projectBoundaryPlacement(const BoundaryProjectionInput& input, Vec3& position);

struct FeatureCollapseInput {
  CollapseEdge edge;
  const std::vector<VertexState>& vertices;
  const std::vector<int>& activeLoopCounts;
};

struct FeatureProjectionInput {
  CollapseEdge edge;
  const std::vector<VertexState>& vertices;
  const std::vector<FeatureCurveConstraint>& curves;
};

class FeatureConstraintPolicy {
public:
  explicit FeatureConstraintPolicy(const SimplifyOptions& options);

  FeatureCollapseRejectKind collapseRejectKind(const FeatureCollapseInput& input) const;
  bool isHardProtectedCollapse(CollapseEdge edge,
                               const std::vector<VertexState>& vertices) const;
  bool projectPlacement(const FeatureProjectionInput& input, Vec3& position) const;

private:
  const SimplifyOptions& options_;
};

} // namespace lq::simplification
