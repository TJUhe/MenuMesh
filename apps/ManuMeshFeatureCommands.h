/**
 * @file apps/ManuMeshFeatureCommands.h
 * @brief 声明特征分析相关的 CLI 命令处理函数。
 * @ingroup manumesh_cli
 *
 * @details 这些命令将命令行参数转换为特征检测选项并生成诊断报告。
 */

#pragma once

#include "CliArguments.h"

namespace manumesh {
namespace cli {
namespace feature_commands {

/// 输出输入网格的特征分析报告。
int report(const Args& args);
/// 根据标注数据执行特征检测基准测试。
int benchmark(const Args& args);
/// 比较原始网格和简化网格的特征检测结果。
int compare(const Args& args);

} // namespace feature_commands
} // namespace cli
} // namespace manumesh
