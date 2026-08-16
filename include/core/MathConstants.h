/**
 * @file include/core/MathConstants.h
 * @brief 定义公共 API 使用的 C++14 数学常量。
 * @ingroup manumesh_core
 *
 * @details 常量集中定义，避免依赖非标准的 M_PI 宏。
 */

#pragma once

namespace manumesh {

/// 所有模块共享的圆周率常量，使生成器、特征检测和简化使用同一表示。
constexpr double kPi = 3.141592653589793238462643383279502884;

} // 命名空间 manumesh
