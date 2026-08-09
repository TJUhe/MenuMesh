/**
 * @file src/feature_detection/detail/PrimitiveFit.h
 * @brief 声明 ManuMesh 特征检测模块的图元拟合功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 本文件属于确定性的三角曲面特征流水线。局部证据与图清理、轨迹追踪、
 *          图元恢复及面片分割相互独立，各阶段均有明确的接口契约。
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"

#include <vector>

namespace manumesh::feature::primitive_fit_detail {

/**
 * @brief 保存平面、圆和椭圆拟合的内部结果。
 */
struct PrimitiveFit {
    bool valid = false;
    FeaturePrimitiveType primitive = FeaturePrimitiveType::Unknown;
    Vec3 center = Vec3::Zero();
    /**
     * @brief 直接拟合椭圆的中心（Halir-Flusser）；
     *        对称采样时与 center 重合，非对称顶点采样时可能不同。
     */
    Vec3 ellipseCenter = Vec3::Zero();
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    Vec3 majorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 minorAxis = Vec3(0.0, 1.0, 0.0);
    double radius = 0.0;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double axisRatio = 0.0;
    double rmsRadialError = 0.0;
    double maxRadialError = 0.0;
    double rmsEllipseError = 0.0;
    double maxEllipseError = 0.0;
    double rmsPlaneError = 0.0;
    double maxPlaneError = 0.0;
};

/**
 * @brief 在环的最佳拟合平面内拟合支持的解析图元。
 */
PrimitiveFit fitPrimitive(const Mesh& mesh, const FeatureLoop& loop, const FeatureOptions& options);
/**
 * @brief 将有效的内部拟合结果复制到公共特征环结构。
 */
void applyPrimitiveFit(const PrimitiveFit& fit, FeatureLoop& loop);
/**
 * @return 所有相邻环边均在策略允许误差内遵循拟合圆时返回 true。
 */
bool cycleEdgesFollowCircle(
    const std::vector<int>& vertices, const PrimitiveFit& fit, const Mesh& mesh, const FeatureOptions& options
);
/**
 * @brief 在不重新拟合的情况下，用给定圆度量环上的采样点。
 */
DirectionalCurveError measureLoopAgainstCircle(
    const Mesh& mesh, const FeatureLoop& loop, const Vec3& center, const Vec3& normalIn, double radius
);

} // 命名空间 manumesh::feature::primitive_fit_detail
