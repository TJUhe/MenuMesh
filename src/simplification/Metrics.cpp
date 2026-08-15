/**
 * @file src/simplification/Metrics.cpp
 * @brief 实现 ManuMesh 的简化模块的度量功能。
 * @ingroup manumesh_simplification
 *
 * @details 本文件属于带特征感知的边折叠流水线。二次误差代价用于排序候选；拓扑、几何、特征、边界、误差及可选纹理策略共同决定一个放置是否可以修改网格。
 */

#include "algorithms/simplification/Metrics.h"

#include <iomanip>
#include <sstream>

namespace manumesh {
namespace simplification {

MeshStats computeMeshStats(const Mesh& mesh) { return manumesh::analysis::computeMeshStats(mesh); }

DistanceStats compareMeshesBySampledDistance(const Mesh& original, const Mesh& simplified, int maxSamples) {
    return manumesh::analysis::compareMeshesBySampledDistance(original, simplified, maxSamples);
}

std::string statsHeaderCsv() {
    return "label,vertices,faces,edges,boundary_edges,non_manifold_edges,area,"
           "mean_triangle_quality,min_triangle_quality,mean_edge_length,"
           "edge_length_cv,mean_orig_to_simp,max_orig_to_simp,"
           "mean_simp_to_orig,max_simp_to_orig";
}

std::string statsRowCsv(const std::string& label, const MeshStats& stats, const DistanceStats* distance) {
    std::ostringstream out;
    out << std::setprecision(12);
    out << label << "," << stats.vertices << "," << stats.faces << "," << stats.edges << "," << stats.boundaryEdges
        << "," << stats.nonManifoldEdges << "," << stats.area << "," << stats.meanTriangleQuality << ","
        << stats.minTriangleQuality << "," << stats.meanEdgeLength << "," << stats.edgeLengthCv;
    if (distance) {
        out << "," << distance->meanOriginalToSimplified << "," << distance->maxOriginalToSimplified << ","
            << distance->meanSimplifiedToOriginal << "," << distance->maxSimplifiedToOriginal;
    } else {
        out << ",,,,";
    }
    return out.str();
}

} // namespace simplification
} // namespace manumesh
