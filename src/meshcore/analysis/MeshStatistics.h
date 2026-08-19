/**
 * @file src/meshcore/analysis/MeshStatistics.h
 * @brief 声明 MeshCore 的轻量网格统计结构。
 * @ingroup manumesh_analysis
 */

#pragma once

#include "core/Mesh.h"

#include <cstddef>

namespace manumesh {
namespace meshcore {

struct MeshStatistics {
    std::size_t vertices = 0;
    std::size_t faces = 0;
    double surfaceArea = 0.0;
    Vec3 minimum = Vec3::Zero();
    Vec3 maximum = Vec3::Zero();
};

MeshStatistics analyzeMesh(const Mesh& mesh);

} // namespace meshcore
} // namespace manumesh
