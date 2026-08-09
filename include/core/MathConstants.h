/**
 * @file include/core/MathConstants.h
 * @brief 声明 ManuMesh 核心网格模块的数学常量设施。
 * @ingroup manumesh_core
 *
 * @details 核心类型建立所有算法模块共同使用的存储、校验、容差、拓扑和状态契约。
 */

#pragma once

namespace manumesh {

/// 所有模块共享的圆周率常量，使生成器、特征检测和简化使用同一表示。
inline constexpr double kPi = 3.141592653589793238462643383279502884;

} // 命名空间 manumesh
