/**
 * @file include/algorithms/feature_detection/FeatureComparison.h
 * @brief 声明原始网格与处理后网格的圆形特征匹配度量。
 * @ingroup manumesh_feature_detection
 *
 * @details 比较接口消费两份已计算的特征结果，不在匹配过程中重新运行检测。
 */

#pragma once

#include "Export.h"
#include "algorithms/feature_detection/FeatureTypes.h"
#include "core/Status.h"

#include <string>
#include <vector>

namespace manumesh {
namespace feature {

/// 两个特征分析之间贪心匹配圆环时使用的阈值。
///
/// 候选对落在 plausible* 阈值内时视为“可能匹配”；可能匹配还需同时落在更严格的 matched* 阈值内，
/// 才报告为 Matched，否则报告为 WeakMatch。默认值与历史上 `manumesh feature-compare` 的硬编码值一致。
struct LoopMatchOptions {
    /// 可能匹配允许的最大圆心距离，相对于 referenceDiagonal。
    double plausibleCenterErrorRatio = 0.08;
    /// 可能匹配允许的最大 |半径误差| / 原始半径。
    double plausibleRadiusErrorRel = 0.20;
    /// 可能匹配允许的最大法向角度，单位为度。
    double plausibleNormalAngleDeg = 30.0;
    /// 强匹配允许的最大圆心距离，相对于 referenceDiagonal。
    double matchedCenterErrorRatio = 0.04;
    /// 强匹配允许的最大 |半径误差| / 原始半径。
    double matchedRadiusErrorRel = 0.08;
    /// 强匹配允许的最大法向角度，单位为度。
    double matchedNormalAngleDeg = 15.0;
    /// 用于归一化圆心误差的长度，通常为原始网格的包围盒对角线。
    /// 值 <= 0 时回退到简化网格的包围盒对角线。
    double referenceDiagonal = 0.0;
};

/// 校验环匹配阈值。所有值必须有限且非负，角度不得超过 180 度；每个 matched 阈值不得宽于对应的 plausible 阈值。
MANUMESH_API Status validateLoopMatchOptions(const LoopMatchOptions& options);

/// 一个原始圆环的匹配分类。
enum class LoopMatchStatus {
    Matched,
    WeakMatch,
    Missing,
};

/// 原始分析中一个圆环的匹配结果。
///
/// 对 Missing 结果，误差字段为零，simplifiedLoopIndex 为 -1，与历史 CLI 报告语义一致。
struct LoopMatch {
    /// 原始环的 FeatureLoop::id。
    int originalLoopId = -1;
    /// 简化分析 loops 向量中的索引；缺失时为 -1。
    int simplifiedLoopIndex = -1;
    int originalVertices = 0;
    int simplifiedVertices = 0;
    double originalRadius = 0.0;
    double simplifiedRadius = 0.0;
    /// 网格单位下的圆心距离。
    double centerError = 0.0;
    /// 网格单位下的绝对半径差。
    double radiusError = 0.0;
    /// 环法向之间的夹角，单位为度。
    double normalAngleDeg = 0.0;
    /// 将简化环与原始圆比较得到的结果（仅对 Matched/WeakMatch 结果填写）。
    DirectionalCurveError directional;
    LoopMatchStatus status = LoopMatchStatus::Missing;
};

/// 两个特征分析之间的贪心圆环匹配报告。
struct LoopMatchReport {
    /// 原始分析中每个圆环一个条目，按环顺序排列。
    std::vector<LoopMatch> matches;
    int originalCircularLoops = 0;
    int simplifiedCircularLoops = 0;
    /// Matched + WeakMatch 条目数量。
    int matchedLoops = 0;
    int missingLoops = 0;
};

/// 将 `original` 的圆环与 `simplifiedFeatures` 中可能匹配的圆环进行贪心匹配：应用三个 plausible 阈值后，
/// 选择圆心/半径/法向组合分数最低的候选。选中的候选对再根据 `options` 中的 matched 阈值分类。
/// `simplified` 是生成 `simplifiedFeatures` 的网格，用于将匹配环与原始圆进行度量。每个简化环最多被一个
/// 原始环消费。相同输入产生确定性结果。当 `options` 不满足 validateLoopMatchOptions() 时抛出 std::invalid_argument。
/// @param[in] original 参考网格的特征分析。
/// @param[in] simplifiedFeatures `simplified` 的特征分析。
/// @param[in] simplified 包含候选环顶点的网格。
/// @param[in] options 可能匹配和强匹配门限。
/// @return 原始每个圆环一个确定性的匹配记录。
/// @algorithm 按圆心、半径和无方向法向门限过滤候选，然后贪心消费组合归一化误差最低的候选。
/// 更严格的门限将接受的候选分类为强匹配或弱匹配。
/// @complexity O(L_o * L_s)，不含方向曲线度量。
MANUMESH_API LoopMatchReport matchCircularLoops(
    const FeatureAnalysis& original,
    const FeatureAnalysis& simplifiedFeatures,
    const Mesh& simplified,
    const LoopMatchOptions& options = {}
);

/// 环匹配状态的稳定字符串名称（"matched"、"weak_match"、"missing"）。
MANUMESH_API std::string toString(LoopMatchStatus status);

} // namespace feature
} // namespace manumesh
