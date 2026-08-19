/**
 * @file apps/CliOptionBinding.h
 * @brief 声明 CLI 选项到特征检测和网格简化配置的绑定函数。
 * @ingroup manumesh_cli
 *
 * @details 绑定逻辑复用统一的严格解析器，并将 CLI 值转换为公共选项结构。
 */

#pragma once

#include "CliArguments.h"
#include "algorithms/feature_detection/FeatureOptions.h"
#include "algorithms/simplification/SimplificationTypes.h"
#include "core/ExecutionOptions.h"

#include <iosfwd>
#include <string>

namespace manumesh {
namespace cli {

/// 解析用户可见的场景 profile。接受 default、cad、scan 和 smooth。
feature::FeatureProfile parseFeatureProfile(const Args& args);
/// @return 用于帮助、诊断和 resolved-config 输出的稳定 profile 名称。
const char* featureProfileName(feature::FeatureProfile profile) noexcept;
/// 将已校验的参数绑定到新的分组简化配置。显式 CLI 参数覆盖 profile。
simplification::SimplifyConfig parseSimplifyConfig(const Args& args);
/// 将已校验的参数绑定到兼容的平面简化选项结构。
///
/// 新的 CLI 调用应优先使用 parseSimplifyConfig；此函数保留给现有调用方和
/// 旧选项兼容路径，并始终通过 makeSimplifyOptions 适配。
simplification::SimplifyOptions parseSimplifyOptions(const Args& args);
/// 将已校验的参数绑定到独立的特征检测选项结构。
feature::FeatureOptions parseFeatureOptions(const Args& args);
/// 将 --threads 绑定到公共执行约束；未提供时保持串行。
ExecutionOptions parseExecutionOptions(const Args& args);
/// 输出易于复制的有效特征配置摘要。
std::string formatResolvedFeatureOptions(const Args& args, const feature::FeatureOptions& options);
/// 输出易于复制的有效简化配置摘要。
std::string formatResolvedSimplifyOptions(const Args& args, const simplification::SimplifyOptions& options);
/// 报告被忽略、无效或存在旧兼容歧义的 CLI 组合。
void emitOptionWarnings(
    const Args& args, const feature::FeatureOptions& options, bool simplifying, std::ostream& output
);

} // namespace cli
} // namespace manumesh
