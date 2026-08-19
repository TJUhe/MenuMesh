#pragma once

#include "core/Mesh.h"

#include <cstddef>

namespace manumesh {
namespace slim {

struct MeshStatistics {
    std::size_t vertices = 0;
    std::size_t faces = 0;
    double surfaceArea = 0.0;
    Vec3 minimum = Vec3::Zero();
    Vec3 maximum = Vec3::Zero();
};

MeshStatistics analyzeMesh(const Mesh& mesh);

} // namespace slim
} // namespace manumesh
