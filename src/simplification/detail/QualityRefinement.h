/**
 * @file src/simplification/detail/QualityRefinement.h
 * @brief 声明边坍缩完成后的固定拓扑质量优化。
 * @ingroup manumesh_simplification
 *
 * @details 细化只移动允许移动的顶点，并复用边界、特征和几何合法性约束。
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "common/detail/MeshDistanceIndex.h"
#include "detail/CollapseTopology.h"
#include "detail/FeatureConstraints.h"
#include "detail/SpatialFaceIndex.h"

#include <vector>

namespace manumesh {
namespace simplification {

/**
 * @brief 传递给固定拓扑细化的不可变约束和可变网格。
 */
struct QualityRefinementInput {
    const SimplifyOptions& options;
    std::vector<VertexState>& vertices;
    const std::vector<FaceState>& faces;
    const DynamicTopology& topology;
    const FeatureConstraintPolicy& featurePolicy;
    const FeatureConstraintGraph& featureConstraints;
    const std::vector<FeatureCurveConstraint>& featureCurves;
    const std::vector<FeaturePrimitiveFit>& primitiveFits;
    SpatialFaceIndex* spatialIndex = nullptr;
    const manumesh::common::MeshDistanceIndex* referenceSurface = nullptr;
    double meshDiagonal = 0.0;
    double areaEps = 0.0;
    double minNormalDot = -1.0;
    double maxLocalError = 0.0;
};

/**
 * @brief 执行有界的切向移动并更新细化诊断信息。
 */
void runQualityRefinement(const QualityRefinementInput& input, SimplifyReport& report);

} // namespace simplification
} // namespace manumesh
