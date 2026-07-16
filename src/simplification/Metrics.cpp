/**
 * @file src/simplification/Metrics.cpp
 * @brief Implements metrics facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#include "algorithms/simplification/Metrics.h"

#include <iomanip>
#include <sstream>

namespace manumesh::simplification {

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

} // namespace manumesh::simplification
