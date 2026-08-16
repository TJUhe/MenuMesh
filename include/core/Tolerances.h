/**
 * @file include/core/Tolerances.h
 * @brief 定义跨模块共用的三角形退化阈值。
 * @ingroup manumesh_core
 *
 * @details 校验、法向、距离索引和质量度量从同一组常量派生阈值，避免模块间出现不同的退化判定。
 */

#pragma once

namespace manumesh {

/// 模型单位下共享的退化容差。
///
/// 所有三角形退化测试都从同一个规范最小面积派生，因此校验、法向计算、
/// 距离索引和质量指标会将相同的三角形判定为退化。统一值采用此前分散阈值
/// 中最保守的一个：旧检查拒绝的任何三角形在此处仍会被拒绝。

/// 最小三角形面积（单位：length^2）。面积小于等于此值的三角形视为退化。
constexpr double kMinTriangleArea = 1e-24;

/// 未归一化三角形法向量的最小长度，即叉积的模；它等于三角形面积的两倍
/// （单位：length^2）。
constexpr double kMinNormalLength = 2.0 * kMinTriangleArea;

/// 未归一化三角形法向量的最小长度平方（单位：length^4）。
/// 等于 kMinNormalLength 的平方。
constexpr double kMinSquaredNormalLength = kMinNormalLength * kMinNormalLength;

/// 三角形边长平方和的最小值（单位：length^2）；低于该值时形状质量比报告为零。
/// 保持其等于 kMinNormalLength，使所有退化测试共享同一尺度。
constexpr double kMinSquaredEdgeLengthSum = kMinNormalLength;

} // 命名空间 manumesh
