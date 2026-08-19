#include "slim/analysis/SlimMeshAnalysis.h"

namespace manumesh {
namespace slim {

MeshStatistics analyzeMesh(const Mesh& mesh) {
    MeshStatistics result;
    result.vertices = mesh.vertices.size();
    result.faces = mesh.faces.size();
    result.surfaceArea = computeSurfaceArea(mesh);
    result.minimum = mesh.bboxMin();
    result.maximum = mesh.bboxMax();
    return result;
}

} // namespace slim
} // namespace manumesh
