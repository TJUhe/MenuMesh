/**
 * @file src/simplification/detail/Quadrics.h
 * @brief 声明 QEM 累积、候选位置求解和优先级缩放。
 * @ingroup manumesh_simplification
 *
 * @details 二次误差负责候选代价和位置；是否允许修改网格由独立合法性策略决定。
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"
#include "detail/FeatureGuidance.h"
#include "detail/SimplificationTypes.h"

#include <vector>

namespace manumesh {
namespace simplification {

/**
 * @brief 计算齐次误差 [p,1]^T q [p,1]。
 */
double evaluateQuadric(const Mat4& q, const Vec3& p);
/**
 * @return 经过给定点且法向为单位长度的点到平面二次误差矩阵。
 */
Mat4 planeQuadric(const Vec3& normal, const Vec3& point);
/**
 * @return 以 point 为中心的各向同性点距离二次误差矩阵。
 */
Mat4 pointQuadric(const Vec3& point);
/**
 * @return 经过 point 且沿 normal 的点到直线二次误差矩阵。
 */
Mat4 lineQuadric(const Vec3& point, const Vec3& normal);

/**
 * @brief 初始逐顶点二次误差及与其解耦的队列优先级因子。
 * 自适应模式下，Wang 风格的特征增益只重新排序队列，不扭曲放置求解。
 */
struct InitialQuadrics {
    std::vector<Mat4> quadrics;
    /**
     * @brief 用于队列排序代价的逐顶点乘数（>= 1）。没有解耦增益时为空，此时每个顶点使用 1.0。
     */
    std::vector<double> priorityScales;
};

/**
 * @brief 累加所有启用的初始二次误差项，并填写报告诊断信息。
 */
void computeInitialQuadrics(
    const Mesh& mesh,
    const SimplifyOptions& options,
    const FeatureGuidance& featureGuidance,
    InitialQuadrics& initial,
    SimplifyReport& report
);

/**
 * @brief 有已校验分析结果时复用其中的紧凑检测证据。
 */
void computeInitialQuadrics(
    const Mesh& mesh,
    const SimplifyOptions& options,
    const FeatureGuidance& featureGuidance,
    const feature::FeatureAnalysis* precomputedFeatures,
    InitialQuadrics& initial,
    SimplifyReport& report
);

/**
 * @brief 返回按二次误差代价升序排列的唯一有限放置候选。
 */
std::vector<SolveResult> solvePlacementCandidates(const Mat4& q, const Vec3& a, const Vec3& b);
/**
 * @deprecated 优先使用 solvePlacementCandidates，以便合法性检查尝试回退候选。
 */
SolveResult solveOptimal(const Mat4& q, const Vec3& a, const Vec3& b);

/**
 * @brief 为一个不可变输入网格构建所有初始几何和引导二次误差矩阵。
 */
class InitialQuadricBuilder {
public:
    /** @brief 绑定一组不可变的简化选项。*/
    explicit InitialQuadricBuilder(const SimplifyOptions& options);

    /** @brief 计算初始二次误差、优先级缩放因子和报告诊断信息。*/
    InitialQuadrics build(const Mesh& mesh, const FeatureGuidance& featureGuidance, SimplifyReport& report) const;

    /** @brief 优先复用已校验特征分析中的紧凑逐顶点证据。 */
    InitialQuadrics build(
        const Mesh& mesh,
        const FeatureGuidance& featureGuidance,
        const feature::FeatureAnalysis* precomputedFeatures,
        SimplifyReport& report
    ) const;

private:
    const SimplifyOptions& options_;
};

} // namespace simplification
} // namespace manumesh
