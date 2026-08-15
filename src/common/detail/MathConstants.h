/**
 * @file src/common/detail/MathConstants.h
 * @brief 声明 ManuMesh 公共几何模块的数学常量设施。
 * @ingroup manumesh_common
 *
 * @details 此处的例程是无策略几何基础，由特征检测、简化、分析和网格编辑共享。
 */

#pragma once

#include "core/MathConstants.h"

namespace manumesh {
namespace common {

// 为现有 manumesh::common::kPi 使用者保留转发别名；规范常量现在位于核心模块
// （include/core/MathConstants.h）。
using manumesh::kPi;

} // namespace common
} // namespace manumesh
