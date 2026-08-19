#pragma once

#include "core/Mesh.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace manumesh {
namespace slim {

using MeshEdge = std::pair<int, int>;

struct FeatureOptions {
    double dihedralAngleDegrees = 45.0;
};

struct FeatureReport {
    std::size_t boundaryEdges = 0;
    std::size_t sharpEdges = 0;
    std::size_t nonManifoldEdges = 0;
    std::vector<MeshEdge> boundary;
    std::vector<MeshEdge> sharp;
    std::vector<MeshEdge> nonManifold;
    std::vector<char> boundaryVertices;
    std::vector<char> featureVertices;
};

FeatureReport detectFeatures(const Mesh& mesh, const FeatureOptions& options = {});

} // namespace slim
} // namespace manumesh
