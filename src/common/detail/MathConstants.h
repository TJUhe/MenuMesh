/**
 * @file src/common/detail/MathConstants.h
 * @brief 声明 ManuMesh 公共几何模块的数学常量设施。
 * @ingroup manumesh_common
 *
 * @details 此处的例程是无策略几何基础，由特征检测、简化、分析和网格编辑共享。
 */

#pragma once

#include "core/MathConstants.h"

namespace manumesh::common {

// 为现有 manumesh::common::kPi 使用者保留转发别名；规范常量现在位于核心模块
// （include/core/MathConstants.h）。
using manumesh::kPi;

} // 命名空间 manumesh::common

namespace manumesh {
// 过渡别名：manumesh::detail 已重命名为 manumesh::common
// （架构 v2，R6）。新代码必须使用 manumesh::common；此别名将在一个小版本后移除。
namespace detail = common;
} // 命名空间 manumesh
