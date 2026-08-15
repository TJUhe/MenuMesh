/**
 * @file src/simplification/detail/SimplificationTypes.h
 * @brief 声明 ManuMesh 的简化模块的简化 类型功能。
 * @ingroup manumesh_simplification
 *
 * @details 本文件属于带特征感知的边折叠流水线。二次误差代价用于排序候选；拓扑、几何、特征、边界、误差及可选纹理策略共同决定一个放置是否可以修改网格。
 */

#pragma once

#include "core/Mesh.h"
#include "mesh_edit/detail/MeshEditTypes.h"

#include <array>
#include <vector>

namespace manumesh {
namespace simplification {

/**
 * @brief 面向简化器的已恢复特征曲线分类。
 */
enum class FeatureCurveKind {
    Unknown,
    Circle,
    NearCircle,
    Ellipse,
    PolygonalLoop,
};

/**
 * @brief 仅在一次简化运行期间使用的可变顶点记录。
 * 字段保持扁平，因为热路径会从多个模块读取它们。概念上可分为三组：几何/QEM 状态、特征归属和队列失效信息。较大的圆/椭圆拟合参数存放在由 primitiveFitId 引用的紧凑旁表（FeaturePrimitiveFit）中，因此非特征顶点不携带拟合数据。
 */
struct VertexState {
    // 几何和 QEM 状态。
    Vec3 p = Vec3::Zero();
    Mat4 q = Mat4::Zero();
    bool active = true;
    /**
     * @brief 从特征证据派生的队列优先级乘数（自适应缩放模式，Wang 2008 解耦）：只缩放队列中的候选排序代价，不进入二次误差矩阵或放置求解。
     */
    double priorityScale = 1.0;

    // 从 FeatureGuidance 复制的特征归属。
    bool isFeature = false;
    bool isBoundary = false;
    bool circularFeature = false;
    bool featureJunction = false;
    bool weakFeature = false;
    FeatureCurveKind featurePrimitive = FeatureCurveKind::Unknown;
    int featureLoopId = -1;
    int featureComponentId = -1;
    double featureConfidence = 0.0;
    Vec3 curveTangent = Vec3::Zero();
    /**
     * @brief 运行的 FeaturePrimitiveFit 旁表索引；顶点不拥有拟合圆/椭圆时为 -1。条目在运行期间不可变，折叠时保留顶点沿用自身条目。
     */
    int primitiveFitId = -1;

    // 折叠后递增，使队列候选能够检测过期端点。
    int version = 0;
};

/**
 * @brief 一个特征顶点的圆/椭圆拟合参数。数据按行外置（见 VertexState::primitiveFitId），使每顶点热状态保持紧凑；只有拟合图元环上的顶点才拥有条目。
 */
struct FeaturePrimitiveFit {
    Vec3 circleCenter = Vec3::Zero();
    Vec3 circleNormal = Vec3(0.0, 0.0, 1.0);
    double circleRadius = 0.0;
    Vec3 ellipseCenter = Vec3::Zero();
    Vec3 ellipseNormal = Vec3(0.0, 0.0, 1.0);
    Vec3 ellipseMajorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 ellipseMinorAxis = Vec3(0.0, 1.0, 0.0);
    double ellipseMajorRadius = 0.0;
    double ellipseMinorRadius = 0.0;
};

/**
 * @brief 获取顶点的图元拟合数据；若顶点没有拟合数据，则返回半径为零的默认值。投影将零半径视为“无图元”并原样传递位置，因此该默认值是安全的空操作。
 */
inline const FeaturePrimitiveFit&
primitiveFitOf(const VertexState& vertex, const std::vector<FeaturePrimitiveFit>& fits) {
    static const FeaturePrimitiveFit kNoFit{};
    if (vertex.primitiveFitId >= 0 && vertex.primitiveFitId < static_cast<int>(fits.size())) {
        return fits[static_cast<std::size_t>(vertex.primitiveFitId)];
    }
    return kNoFit;
}

using FaceState = mesh_edit::EditableFace;

/**
 * @brief 有向边折叠选择：保留一个端点并移除另一个端点。
 */
struct CollapseEdge {
    int keep = -1;
    int remove = -1;
};

/**
 * @brief 候选折叠放置及其评估出的二次误差代价。
 */
struct SolveResult {
    Vec3 position = Vec3::Zero();
    double cost = 0.0;
    bool usedFallback = false;
};

/**
 * @brief 优先级队列条目。为适配 std::priority_queue，比较顺序被反转，从而优先弹出最低代价候选。代价相等时回退到规范边键 (a, b)，保持弹出顺序确定。
 * 条目携带在压入时求解的放置候选。只要版本戳匹配，它们就保持有效：合并后的二次误差矩阵和两个端点位置只能通过折叠改变，而折叠会递增端点版本。这样 pop/tryCollapse 可以复用求解结果，无需重新运行 3x3 谱分析。
 */
struct Candidate {
    double cost = 0.0;
    int a = -1;
    int b = -1;
    int versionA = 0;
    int versionB = 0;
    /**
     * @brief 已缓存的放置候选，按二次误差代价升序排列。
     */
    std::array<SolveResult, 4> placements{};
    int placementCount = 0;

    /** @brief 为 priority_queue 实现确定性的最小代价排序。*/
    bool operator<(const Candidate& other) const {
        if (cost != other.cost) {
            return cost > other.cost;
        }
        if (a != other.a) {
            return a > other.a;
        }
        return b > other.b;
    }
};

/** @brief 负责拒绝折叠的特征策略类。*/
enum class FeatureCollapseRejectKind {
    None,
    Primitive,
    Generic,
};

/** @brief 首个拒绝放置的硬性几何或拓扑检查。*/
enum class CollapseRejectReason {
    None,
    Topology,
    NormalFlip,
    TriangleQuality,
    SelfIntersection,
    LocalError,
};

/** @brief 拒绝放置的纹理图表约束。*/
enum class TextureCollapseRejectReason {
    None,
    ChartMismatch,
    TriangleFlip,
};

/**
 * @brief 边界拓扑决策和边界边分类。
 */
struct BoundaryCollapseDecision {
    bool allowed = true;
    bool boundaryEdge = false;
};

/**
 * @brief 一条特征折线各线段上的静态 AABB 树。每个环只在足够长时构建一次，使最近点查询从 O(L) 降为 O(log L)。构建和遍历顺序具有确定性：划分使用带索引平局规则的 nth_element，查询优先访问较近子节点并采用严格改善剪枝。
 */
struct PolylineSegmentIndex {
    /** @brief 覆盖一段特征曲线线段范围的 AABB 树节点。*/
    struct Node {
        Vec3 lo = Vec3::Zero();
        Vec3 hi = Vec3::Zero();
        int left = -1;
        int right = -1;
        int begin = 0;
        int end = 0;

        /** @brief 判断该节点是否直接存储线段范围。*/
        bool leaf() const { return left < 0; }
    };
    std::vector<Node> nodes;
    std::vector<int> segmentOrder;

    /** @brief 判断线段加速结构是否已构建。*/
    bool built() const { return !nodes.empty(); }
};

/**
 * @brief 一条受保护曲线的样本和可选加速数据。
 */
struct FeatureCurveConstraint {
    bool valid = false;
    bool closed = false;
    FeatureCurveKind primitive = FeatureCurveKind::Unknown;
    std::vector<Vec3> samples;
    /** Explicit detector-evidence segments. Recovery bridges are never stored here. */
    std::vector<std::array<Vec3, 2>> segments;
    /**
     * @brief 折线线段上的可选加速结构；短环保持为空并继续使用普通线性扫描。
     */
    PolylineSegmentIndex segmentIndex;
};

} // namespace simplification
} // namespace manumesh
