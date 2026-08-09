/**
 * @file apps/CliOptionBinding.h
 * @brief 声明 CLI 选项到特征检测和网格简化配置的绑定函数。
 * @ingroup manumesh_cli
 *
 * @details 绑定逻辑复用统一的严格解析器，并将 CLI 值转换为公共选项结构。
 */

#pragma once

#include "CliArguments.h"
#include "algorithms/feature_detection/FeatureTypes.h"
#include "algorithms/simplification/SimplificationTypes.h"

namespace manumesh::cli {

/// 将已校验的参数绑定到简化选项结构。
simplification::SimplifyOptions parseSimplifyOptions(const Args& args);
/// 将已校验的参数绑定到独立的特征检测选项结构。
feature::FeatureOptions parseFeatureOptions(const Args& args);

} // 命令行命名空间
