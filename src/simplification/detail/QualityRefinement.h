#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "common/detail/MeshDistanceIndex.h"
#include "detail/CollapseTopology.h"
#include "detail/FeatureConstraints.h"
#include "detail/SpatialFaceIndex.h"

#include <vector>

namespace manumesh::simplification {

struct QualityRefinementInput {
    const SimplifyOptions& options;
    std::vector<VertexState>& vertices;
    const std::vector<FaceState>& faces;
    const DynamicTopology& topology;
    const FeatureConstraintPolicy& featurePolicy;
    SpatialFaceIndex* spatialIndex = nullptr;
    const manumesh::detail::MeshDistanceIndex* referenceSurface = nullptr;
    double areaEps = 0.0;
    double minNormalDot = -1.0;
    double maxLocalError = 0.0;
};

void runQualityRefinement(const QualityRefinementInput& input, SimplifyReport& report);

} // namespace manumesh::simplification
