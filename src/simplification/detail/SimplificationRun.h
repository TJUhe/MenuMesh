/**
 * @file src/simplification/detail/SimplificationRun.h
 * @brief 声明 ManuMesh 的简化模块的简化 运行功能。
 * @ingroup manumesh_simplification
 *
 * @details 本文件属于带特征感知的边折叠流水线。二次误差代价用于排序候选；拓扑、几何、特征、边界、误差及可选纹理策略共同决定一个放置是否可以修改网格。
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "common/detail/MeshDistanceIndex.h"
#include "core/Mesh.h"
#include "detail/CandidateQueue.h"
#include "detail/CollapseAttempt.h"
#include "detail/CollapseTopology.h"
#include "detail/FeatureConstraints.h"
#include "detail/FeatureGuidance.h"
#include "detail/Quadrics.h"
#include "detail/SimplificationPolicies.h"
#include "detail/SpatialFaceIndex.h"
#include "detail/TextureProtection.h"

#include <memory>
#include <unordered_set>
#include <vector>

namespace manumesh {
namespace feature {
struct FeatureAnalysis;
} // namespace feature
} // namespace manumesh

namespace manumesh {
namespace simplification {

/**
 * @brief 一次边折叠简化运行的可变、单次执行对象。
 */
class SimplificationRun {
public:
    /**
     * @brief 创建一个根据 options 计算特征分析的运行。
     * @param[in] input 不可变源网格，其生命周期必须覆盖整个运行。
     * @param[in] options 不可变策略，其生命周期必须覆盖整个运行。
     */
    SimplificationRun(const Mesh& input, const SimplifyOptions& options);
    /**
     * @brief 创建一个可以复用调用方特征分析的运行。
     * @param[in] input 不可变源网格，其生命周期必须覆盖整个运行。
     * @param[in] options 不可变策略，其生命周期必须覆盖整个运行。
     * @param[in] features 可选分析结果，其网格必须与 input 匹配。
     */
    SimplificationRun(const Mesh& input, const SimplifyOptions& options, const feature::FeatureAnalysis* features);

    /**
     * @brief 执行初始化、折叠、可选细化和压缩。
     */
    Mesh execute(SimplifyReport* outReport);

private:
    /** @brief 重置所有报告字段并记录输入网格尺寸。*/
    void initializeReport();
    /** @brief 复用或计算特征分析并构建引导表。*/
    void analyzeFeatures();
    /** @brief 创建可变顶点记录和初始二次误差。*/
    void initializeVertices();
    /** @brief 将特征归属和约束复制到一个顶点。*/
    void initializeVertexFeature(int vertexId);
    /** @brief 创建可变面、UV、拓扑和空间索引状态。*/
    void initializeFaces();
    /** @brief 解析目标数量和依赖尺度的合法性预算。*/
    void initializeBudget();
    /** @brief 根据当前全部活动边重建候选堆。*/
    void rebuildQueue();
    /**
     * @brief 只求解一次边的放置，使用同一求解结果计算纹理保护代价，并将带缓存放置的候选压入队列。当（近似）中点放置被纹理拒绝时返回 true，用于初始构建阶段的 textureProtectedEdges 诊断。
     */
    bool pushEdgeCandidate(int a, int b);
    /** @brief 弹出并评估候选，直到达到配置的停止条件。*/
    void collapseUntilTarget();
    /** @brief 执行可选的固定拓扑质量细化。*/
    void refineQuality();
    /** @brief 当活动拓扑仍可推进时重建耗尽的候选堆。*/
    bool ensureQueueHasCandidates();
    /** @brief 检查队列候选的端点活动状态和版本戳。*/
    bool isCurrentCandidate(const Candidate& candidate) const;
    /** @brief 记录过期队列条目，并按周期触发重建。*/
    void handleStaleCandidate();
    /** @brief 当放置通过时评估并应用一个当前候选。*/
    bool tryCollapse(const Candidate& candidate);
    /** @brief 将分类后的失败尝试映射到报告计数器。*/
    void recordRejectedCollapse(const CollapseAttemptResult& result);
    /** @brief 使任一端点相邻的队列候选失效。*/
    void bumpVersions(int keep, int remove);
    /** @brief 提交拓扑、几何、二次误差、UV 和索引更新。*/
    void applyCollapse(
        int keep, int remove, const Vec3& position, const Mat4& mergedQ, const TextureUpdatePlan& texturePlan
    );
    /** @brief 收集宽相位登记可能发生变化的面。*/
    std::unordered_set<int> collectAffectedFacesForCollapse(int keep, int remove) const;
    /** @brief 折叠后重写相邻面并移除重复面。*/
    void rewriteIncidentFaces(int keep, int remove);

    const Mesh& input_;
    const SimplifyOptions& options_;
    const feature::FeatureAnalysis* precomputedFeatures_ = nullptr;
    std::unique_ptr<feature::FeatureAnalysis> ownedFeatureAnalysis_;
    const feature::FeatureAnalysis* featureAnalysis_ = nullptr;
    SimplificationPolicies policies_;
    SimplifyReport report_;
    FeatureGuidance featureGuidance_;
    std::vector<char> boundaryVertices_;
    std::vector<VertexState> vertices_;
    std::vector<FaceState> faces_;
    std::vector<FaceTexCoords> faceTexCoords_;
    std::unique_ptr<DynamicTopology> topology_;
    SpatialFaceIndex spatialIndex_;
    std::unique_ptr<manumesh::common::MeshDistanceIndex> referenceSurface_;
    std::vector<int> activeLoopCounts_;
    /**
     * @brief 圆/椭圆拟合数据的紧凑旁表；只有拟合图元环上的特征顶点才在其中拥有条目（VertexState::primitiveFitId）。
     */
    std::vector<FeaturePrimitiveFit> primitiveFits_;
    CandidateQueue queue_;
    InitialQuadricBuilder quadrics_;
    FeatureConstraintPolicy featurePolicy_;
    TextureProtection textureProtection_;
    int activeFaceCount_ = 0;
    int targetFaces_ = 0;
    double areaEps_ = 0.0;
    /**
     * @brief 输入包围盒对角线，在 initializeBudget 中计算一次。tryCollapse 会在每次折叠尝试中运行，因此不能重新执行这个 O(V) 扫描（否则整个运行会随网格规模呈二次增长）。
     */
    double meshDiagonal_ = 0.0;
    double minNormalDot_ = 0.0;
    double maxLocalError_ = 0.0;
    int maxAttemptsWithoutCollapse_ = 0;
    int attemptsWithoutCollapse_ = 0;
    int stalePops_ = 0;
    bool queueBuiltOnce_ = false;
};

} // namespace simplification
} // namespace manumesh
