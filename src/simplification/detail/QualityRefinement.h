/**
 * @file src/simplification/detail/QualityRefinement.h
 * @brief 声明 ManuMesh 的简化模块的质量细化功能。
 * @ingroup manumesh_simplification
 *
 * @details 本文件属于带特征感知的边折叠流水线。二次误差代价用于排序候选；拓扑、几何、特征、边界、误差及可选纹理策略共同决定一个放置是否可以修改网格。
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "common/detail/MeshDistanceIndex.h"
#include "detail/CollapseTopology.h"
#include "detail/FeatureConstraints.h"
#include "detail/SpatialFaceIndex.h"

#include <vector>

namespace manumesh::simplification {

/**
 * @brief 传递给固定拓扑细化的不可变约束和可变网格。
 */
struct QualityRefinementInput {
    const SimplifyOptions& options;
    std::vector<VertexState>& vertices;
    const std::vector<FaceState>& faces;
    const DynamicTopology& topology;
    const FeatureConstraintPolicy& featurePolicy;
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

} // 结束 manumesh::simplification 命名空间
