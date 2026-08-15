/**
 * @file src/simplification/detail/FeatureConstraints.h
 * @brief 声明 ManuMesh 的简化模块的特征约束功能。
 * @ingroup manumesh_simplification
 *
 * @details 本文件属于带特征感知的边折叠流水线。二次误差代价用于排序候选；拓扑、几何、特征、边界、误差及可选纹理策略共同决定一个放置是否可以修改网格。
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "detail/FeatureConstraintGraph.h"
#include "detail/SimplificationTypes.h"
#include "mesh_edit/detail/DynamicTopology.h"

#include <vector>

namespace manumesh {
namespace simplification {

/**
 * @brief 使用确定性的径向回退轴将点投影到拟合圆上。
 */
Vec3 projectToCircle(const Vec3& p, const VertexState& feature, const FeaturePrimitiveFit& fit);
/**
 * @brief 在拟合椭圆的正交坐标系中将点投影到椭圆上。
 */
Vec3 projectToEllipse(const Vec3& p, const VertexState& feature, const FeaturePrimitiveFit& fit);
/**
 * @brief 圆约束顶点移动后刷新其切线。
 */
void refreshCircularTangent(VertexState& vertex, const FeaturePrimitiveFit& fit);
/**
 * @brief 椭圆约束顶点移动后刷新其切线。
 */
void refreshEllipseTangent(VertexState& vertex, const FeaturePrimitiveFit& fit);

/**
 * @brief 至少包含该数量线段的环会建立 PolylineSegmentIndex；较短的环继续使用常数因子更小的普通线性扫描。
 */
constexpr int kPolylineIndexMinSegments = 64;

/**
 * @brief 当折线足够长时构建 curve.segmentIndex。样本确定后每个环只调用一次；查询随后以 O(log L) 运行。该索引类似长多边形特征曲线的 BVH。
 */
void buildPolylineSegmentIndex(FeatureCurveConstraint& curve);

/**
 * @brief 求受保护特征折线上的最近点（闭环会对所有线段进行环回）。有可用索引时使用预构建索引，否则线性扫描。曲线没有线段时，outDistanceSquared 接收正无穷。
 * @return 返回受约束曲线上的最近点，并写入其平方距离。
 */
Vec3 closestPointOnFeatureCurve(const FeatureCurveConstraint& curve, const Vec3& position, double& outDistanceSquared);

/**
 * @brief 当放置点满足两个端点共享特征曲线所隐含的特征曲线偏差预算（maxFeatureCurveDeviationRatio）时返回 true。将两个端点传入同一个顶点即可校验单顶点移动，例如质量细化期间的移动。
 */
bool featureCurveBudgetAllows(
    const VertexState& a,
    const VertexState& b,
    const std::vector<FeatureCurveConstraint>& featureCurves,
    const std::vector<FeaturePrimitiveFit>& primitiveFits,
    const SimplifyOptions& options,
    double meshDiagonal,
    const Vec3& position
);

/**
 * @brief 硬性特征判定所需的边归属和环预算。
 */
struct FeatureCollapseInput {
    CollapseEdge edge;
    const std::vector<VertexState>& vertices;
    const std::vector<int>& activeLoopCounts;
    const FeatureConstraintGraph& constraints;
};

/**
 * @brief 受约束投影所需的已接受原始放置和曲线数据。
 */
struct FeatureProjectionInput {
    CollapseEdge edge;
    const std::vector<VertexState>& vertices;
    const std::vector<FeatureCurveConstraint>& curves;
    const std::vector<FeaturePrimitiveFit>& primitiveFits;
    const FeatureConstraintGraph& constraints;
};

/**
 * @brief 从 SimplifyOptions 派生的无状态硬特征策略评估器。
 */
class FeatureConstraintPolicy {
public:
    /** @brief 将策略绑定到不可变的简化选项。*/
    explicit FeatureConstraintPolicy(const SimplifyOptions& options);

    /** @brief 对边折叠的特征保护原因进行分类。*/
    FeatureCollapseRejectKind collapseRejectKind(const FeatureCollapseInput& input) const;
    /** @brief 判断顶点是否必须由硬保护保持固定。*/
    bool isHardProtectedVertex(
        int vertex, const std::vector<VertexState>& vertices, const FeatureConstraintGraph& constraints
    ) const;
    /** @brief 判断硬保护是否禁止折叠一条边。*/
    bool isHardProtectedCollapse(
        CollapseEdge edge, const std::vector<VertexState>& vertices, const FeatureConstraintGraph& constraints
    ) const;
    /** @brief 将接受的原始放置投影到其受保护的解析图元上。*/
    bool projectPlacement(const FeatureProjectionInput& input, Vec3& position) const;

private:
    const SimplifyOptions& options_;
    int minFeatureLoopVertices_ = 5;
};

} // namespace simplification
} // namespace manumesh
