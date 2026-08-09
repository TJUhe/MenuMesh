/**
 * @file src/feature_detection/detail/FeatureDetectionTypes.h
 * @brief 声明 ManuMesh 特征检测流水线使用的内部数据类型。
 * @ingroup manumesh_feature_detection
 *
 * @details 本文件属于确定性的三角曲面特征流水线。局部证据与图清理、轨迹追踪、
 *          图元恢复及面片分割相互独立，各阶段均有明确的接口契约。
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "common/detail/MathConstants.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace manumesh::feature::detector_detail {

using manumesh::common::kPi;

/**
 * @brief 保存检测阶段收集的各类边证据。
 */
struct CandidateEdge {
    int a = -1;
    int b = -1;
    bool boundary = false;
    bool dihedral = false;
    bool normalTensor = false;
    bool smoothCurvature = false;
    bool nonManifold = false;
    bool cleanupBridge = false;
    bool consolidationBridge = false;
    int signedKind = 0;
    double angleRad = 0.0;
    double tensorPersistentScore = 0.0;
    int tensorPersistentScales = 0;
    double curvaturePersistentScore = 0.0;
    int curvaturePersistentScales = 0;
};

/**
 * @brief 按特征图边唯一保存证据属性。
 *
 * 早期实现使用十一张具有相同键的并行哈希表；现在每个键对应一个结构体，
 * 既减少内存访问，也让热点循环一次查找即可取得全部属性。
 *
 * 该属性记录必须与轨迹图中的无向边一一对应，并在图变更后同步更新。
 */
struct TraceEdgeAttrs {
    bool boundary = false;
    bool dihedral = false;
    bool normalTensor = false;
    bool smoothCurvature = false;
    bool nonManifold = false;
    bool cleanupBridge = false;
    bool consolidationBridge = false;
    int signedKind = 0;
    double tensorPersistence = 0.0;
    int tensorPersistentScales = 0;
    double curvaturePersistence = 0.0;
    int curvaturePersistentScales = 0;
};

/**
 * @brief 供清理、追踪和特征环恢复共用的紧凑特征图。
 */
struct TraceGraph {
    std::vector<std::vector<int>> adjacency;
    std::vector<char> traceVertex;
    std::unordered_map<std::uint64_t, TraceEdgeAttrs> edgeAttrs;
    std::vector<std::pair<int, int>> graphEdges;
};

/**
 * @brief 追踪一条图链时累积的证据计数。
 */
struct TraceLoopStats {
    int edgeCount = 0;
    int boundaryEdges = 0;
    int dihedralEdges = 0;
    int normalTensorEdges = 0;
    int smoothCurvatureEdges = 0;
    int nonManifoldEdges = 0;
    int cleanupBridgeEdges = 0;
    int convexEdges = 0;
    int concaveEdges = 0;
    int unknownSignedEdges = 0;
    bool closed = false;
};

/**
 * @brief 用于选择环接受策略的恢复来源类型。
 */
enum class RecoveredCycleKind {
    Circular,
    Polygonal,
};

/**
 * @brief 集中特征分析结果记账的可变累加器。
 */
class FeatureAnalysisBuilder {
public:
    /** @brief 为网格中的每个顶点分配一个公共特征记录。 */
    explicit FeatureAnalysisBuilder(int vertexCount) { analysis_.vertices.assign(vertexCount, VertexFeature{}); }

    /** @brief 返回当前正在累积的分析结果。 */
    FeatureAnalysis& analysis() { return analysis_; }
    /** @brief 返回已累积分析结果的只读视图。 */
    const FeatureAnalysis& analysis() const { return analysis_; }

    /** @brief 返回恢复阶段使用的单调递增环 ID 计数器。 */
    int& nextLoopId() { return nextLoopId_; }

    /** @brief 将已完成的分析结果移动给调用方。 */
    FeatureAnalysis build() { return std::move(analysis_); }

    /** @brief 将一条已接受的边计入所有匹配的证据计数器。 */
    void recordFeatureEdge(const CandidateEdge& edge) {
        ++analysis_.featureEdges;
        if (edge.boundary)
            ++analysis_.boundaryFeatureEdges;
        if (edge.dihedral)
            ++analysis_.dihedralFeatureEdges;
        if (edge.normalTensor)
            ++analysis_.normalTensorFeatureEdges;
        if (edge.smoothCurvature)
            ++analysis_.smoothCurvatureFeatureEdges;
        if (edge.nonManifold)
            ++analysis_.nonManifoldFeatureEdges;
        if (edge.signedKind > 0)
            ++analysis_.convexFeatureEdges;
        if (edge.signedKind < 0)
            ++analysis_.concaveFeatureEdges;
        if (edge.dihedral && edge.signedKind == 0) {
            ++analysis_.unknownSignedFeatureEdges;
        }
    }

private:
    FeatureAnalysis analysis_;
    int nextLoopId_ = 0;
};

} // 命名空间 manumesh::feature::detector_detail
