/**
 * @file src/meshcore/analysis/MeshStatistics.cpp
 * @brief 实现 MeshCore 的轻量网格统计。
 * @ingroup manumesh_analysis
 */

#include "meshcore/analysis/MeshStatistics.h"

namespace manumesh {
namespace meshcore {

MeshStatistics analyzeMesh(const Mesh& mesh) {
    MeshStatistics result;
    result.vertices = mesh.vertices.size();
    result.faces = mesh.faces.size();
    result.surfaceArea = computeSurfaceArea(mesh);
    result.minimum = mesh.bboxMin();
    result.maximum = mesh.bboxMax();
    return result;
}

} // namespace meshcore
} // namespace manumesh
