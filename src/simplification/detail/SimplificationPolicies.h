/**
 * @file src/simplification/detail/SimplificationPolicies.h
 * @brief 声明 ManuMesh 的简化模块的简化 策略功能。
 * @ingroup manumesh_simplification
 *
 * @details 本文件属于带特征感知的边折叠流水线。二次误差代价用于排序候选；拓扑、几何、特征、边界、误差及可选纹理策略共同决定一个放置是否可以修改网格。
 */

#pragma once

#include "algorithms/feature_detection/FeatureOptions.h"
#include "algorithms/simplification/SimplificationTypes.h"

namespace manumesh {
namespace simplification {

/**
 * @brief 归一化的绝对/相对目标选择策略。
 */
struct TargetPolicy {
    int targetFaces = -1;
    double targetRatio = 0.25;

    /** @brief 解析输入网格的目标面数并进行夹紧。*/
    int resolveTargetFaceCount(int inputFaceCount) const;
};

/**
 * @brief 从简化选项派生的特征分析设置。
 */
struct FeatureDetectionPolicy {
    bool enabled = false;
    feature::FeatureOptions options;
};

/**
 * @brief 解析规范特征检测配置；未显式组合时从旧扁平字段适配。
 */
feature::FeatureOptions
featureOptionsFromSimplifyOptions(const SimplifyOptions& options, int minFeatureLoopVerticesFloor = 0);

/**
 * @brief 供硬折叠过滤器使用的预归一化热循环开关。
 */
struct LegalityPolicy {
    bool preserveBoundary = false;
    double minTriangleQuality = 0.0;
    double maxNormalDeviationDeg = 90.0;
    double maxLocalError = 0.0;
    double maxLocalErrorRatio = 0.0;
    bool preventLocalIntersections = false;

    /** @brief 将法向角度上限转换为余弦阈值。*/
    double resolveMinNormalDot() const;
    /** @brief 解析有效的绝对局部误差上限。*/
    double resolveMaxLocalError(double bboxDiag) const;
};

/**
 * @brief 一次运行中所有折叠尝试共享的不可变归一化策略。
 */
struct SimplificationPolicies {
    TargetPolicy target;
    FeatureDetectionPolicy features;
    LegalityPolicy legality;

    /** @brief 根据公开的简化选项归一化所有热循环策略。*/
    static SimplificationPolicies fromOptions(const SimplifyOptions& options);
};

} // namespace simplification
} // namespace manumesh
