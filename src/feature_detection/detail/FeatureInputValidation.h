/**
 * @file src/feature_detection/detail/FeatureInputValidation.h
 * @brief 声明 ManuMesh 特征检测模块的输入校验功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 本文件属于确定性的三角曲面特征流水线。局部证据与图清理、轨迹追踪、
 *          图元恢复及面片分割相互独立，各阶段均有明确的接口契约。
 */

#pragma once

#include "core/Mesh.h"

#include <stdexcept>
#include <string>

namespace manumesh::feature::detector_detail {

/**
 * @brief 公共特征检测 API 共用的入口校验。
 *
 * 没有面的网格会被接受，使只有顶点的输入返回空结果；其他网格在读取任何面数据前，
 * 必须通过宽松几何校验（索引在范围内、坐标有限、每个面不重复顶点索引）。
 * 零面积面会被容忍，因为脏 CAD/扫描输入很常见；证据阶段会跳过其贡献，而不会使整个
 * 分析失败。调用方可通过 FeatureAnalysis::degenerateFaces 查看被容忍的退化面数量。
 */
inline void validateFeatureMeshInput(const Mesh& mesh) {
    if (mesh.faces.empty()) {
        return;
    }
    std::string error;
    if (!validateMeshGeometryLenient(mesh, &error)) {
        throw std::invalid_argument(error);
    }
}

} // 命名空间 manumesh::feature::detector_detail
