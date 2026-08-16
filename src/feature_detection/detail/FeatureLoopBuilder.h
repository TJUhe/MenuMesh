/**
 * @file src/feature_detection/detail/FeatureLoopBuilder.h
 * @brief 声明追踪序列到 FeatureLoop 的物化与去重。
 * @ingroup manumesh_feature_detection
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace manumesh {
namespace feature {
namespace detector_detail {

/**
 * @brief 由环的无向边排序键构成的、与方向无关的环标识。
 *        相比旧版字符串拼接，该形式避免为每个候选环分配和格式化文本缓冲区。
 */
using CycleSignature = std::vector<std::uint64_t>;

/**
 * @brief 为规范化环签名计算哈希，用于抑制重复环。
 */
struct CycleSignatureHash {
    /**
     * @return 规范化排序边键序列的稳定哈希值。
     */
    std::size_t operator()(const CycleSignature& signature) const;
};

using CycleSignatureSet = std::unordered_set<CycleSignature, CycleSignatureHash>;

/**
 * @return 与方向无关、按规范顺序排列的无向边环签名。
 */
CycleSignature cycleSignature(const std::vector<int>& vertices);

/**
 * @brief 写入环归属、几何基元投影数据以及顶点切线。
 */
void assignLoopToVertices(
    const FeatureLoop& loop, const Mesh& mesh, const std::vector<std::vector<int>>& adjacency, FeatureAnalysis& analysis
);

/**
 * @brief 根据追踪顶点和证据计数构造公共特征环记录。
 */
FeatureLoop makeLoopFromStats(std::vector<int> vertices, int loopId, const TraceLoopStats& stats);

/**
 * @brief 对直接追踪的链或环进行拟合、校验、记录并写入顶点归属。
 */
void addTracedLoop(
    const Mesh& mesh,
    const FeatureOptions& options,
    const std::vector<std::vector<int>>& adjacency,
    std::vector<int> vertices,
    const TraceLoopStats& stats,
    FeatureAnalysis& analysis,
    int& loopId
);

/**
 * @brief 去重并按条件物化一个恢复出的环。
 * @return 仅当追加了新的已接受环时返回 true。
 */
bool addRecoveredCycle(
    RecoveredCycleKind kind,
    std::vector<int> vertices,
    CycleSignatureSet& seenCycles,
    const Mesh& mesh,
    const FeatureOptions& options,
    const TraceGraph& trace,
    FeatureAnalysis& analysis,
    int& loopId
);

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
