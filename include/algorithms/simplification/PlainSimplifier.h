/**
 * @file include/algorithms/simplification/PlainSimplifier.h
 * @brief 提供不暴露 Eigen 的 PlainMesh 简化入口。
 * @ingroup manumesh_simplification
 *
 * @details 入口在 SDK 边界完成 PlainMesh 与 Mesh 的显式转换，简化行为与 QEMSimplifier 保持一致。
 */

#pragma once

#include "Export.h"
#include "algorithms/simplification/SimplificationTypes.h"
#include "core/PlainMesh.h"

namespace manumesh {
namespace simplification {

/// 通过无 Eigen 的 C++ 交换类型简化网格。
///
/// 此入口使面向宿主的 C++ 边界不依赖 Eigen。实现会先转换为内部 Eigen 网格，
/// 使用与 `simplifyMesh` 相同的简化器，再将结果转换回 `PlainMesh`。
/// @param[in] input 无 Eigen 的三角网格交换值。
/// @param[in] options 简化目标、代价和硬性策略。
/// @param[out] report 可选的运行诊断信息。
/// @return 简化后的无 Eigen 网格。
/// @throws std::invalid_argument 当输入或选项违反 C++ API 约定时抛出。
MANUMESH_API PlainMesh
simplifyPlainMesh(const PlainMesh& input, const SimplifyOptions& options, SimplifyReport* report = nullptr);

} // namespace simplification
} // namespace manumesh
