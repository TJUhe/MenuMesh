/**
 * @file include/algorithms/simplification/MeshCoreQem.h
 * @brief 声明 MeshCore 的特征保护 QEM 简化入口。
 * @ingroup manumesh_simplification
 */

#pragma once

#include "Export.h"
#include "core/Mesh.h"

#include <cstddef>
#include <string>

namespace manumesh {
namespace meshcore {

/**
 * @brief MeshCore QEM 简化的目标与保护策略。
 */
struct SimplifyOptions {
    double ratio = 0.5;
    int targetFaces = 0;
    double featureAngleDegrees = 45.0;
    bool preserveBoundary = true;
    bool preserveFeatures = true;
    double boundaryConstraintWeight = 1.0;
    double featureConstraintWeight = 4.0;
    double minTriangleQuality = 1e-4;
    double maxNormalizedError = 0.0;
    bool preventLocalIntersections = false;
};

/**
 * @brief 一次 MeshCore QEM 简化运行的结果与拒绝统计。
 */
struct SimplifyReport {
    std::size_t initialFaces = 0;
    std::size_t targetFaces = 0;
    std::size_t finalFaces = 0;
    std::size_t collapses = 0;
    std::size_t rejectedCandidates = 0;
    std::size_t staleCandidates = 0;
    std::size_t topologyRejected = 0;
    std::size_t constraintRejected = 0;
    std::size_t geometryRejected = 0;
    std::size_t errorRejected = 0;
    std::size_t intersectionRejected = 0;
    std::size_t queueRebuilds = 0;
    std::string stopReason;
};

/**
 * @brief 在保持边界和锐边的前提下简化三角网格。
 * @param input 输入三角网格。
 * @param options 目标面数、特征角度和几何保护策略。
 * @param report 可选的运行统计。
 * @return 顶点和面已压缩的简化网格。
 */
MANUMESH_API Mesh simplifyQem(const Mesh& input, const SimplifyOptions& options, SimplifyReport* report = nullptr);

} // namespace meshcore
} // namespace manumesh
