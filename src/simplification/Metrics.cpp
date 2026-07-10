#include "algorithms/simplification/Metrics.h"

#include "common/detail/GeometryPredicates.h"
#include "common/detail/MeshDistanceIndex.h"
#include "core/MeshTopology.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <vector>

namespace manumesh::simplification {
namespace {

std::vector<Vec3> sampleSurfacePoints(const Mesh& mesh, int maxSamples) {
    std::vector<double> cumulative;
    cumulative.reserve(mesh.faces.size());
    double totalArea = 0.0;
    for (const Face& face : mesh.faces) {
        const double area = triangleArea(mesh.vertices[face.v[0]], mesh.vertices[face.v[1]], mesh.vertices[face.v[2]]);
        if (area > 1e-24) {
            totalArea += area;
        }
        cumulative.push_back(totalArea);
    }
    if (totalArea <= 1e-24 || maxSamples <= 0) {
        return {};
    }

    const int samples = maxSamples;
    std::vector<Vec3> points;
    points.reserve(samples);
    for (int i = 0; i < samples; ++i) {
        const double target = (static_cast<double>(i) + 0.5) * totalArea / static_cast<double>(samples);
        auto it = std::lower_bound(cumulative.begin(), cumulative.end(), target);
        int faceId = static_cast<int>(std::distance(cumulative.begin(), it));
        faceId = std::min(faceId, static_cast<int>(mesh.faces.size()) - 1);
        while (faceId > 0 && cumulative[faceId] == cumulative[faceId - 1]) {
            --faceId;
        }

        const Face& face = mesh.faces[faceId];
        const double uSeed = std::fmod((static_cast<double>(i) + 0.5) * 0.7548776662, 1.0);
        const double vSeed = std::fmod((static_cast<double>(i) + 0.5) * 0.5698402967, 1.0);
        const double su = std::sqrt(uSeed);
        const double b0 = 1.0 - su;
        const double b1 = su * (1.0 - vSeed);
        const double b2 = su * vSeed;
        points.push_back(b0 * mesh.vertices[face.v[0]] + b1 * mesh.vertices[face.v[1]] + b2 * mesh.vertices[face.v[2]]);
    }
    return points;
}

} // namespace

MeshStats computeMeshStats(const Mesh& mesh) {
    MeshStats stats;
    stats.vertices = static_cast<int>(mesh.vertices.size());
    stats.faces = static_cast<int>(mesh.faces.size());

    const Result<MeshTopology> topologyResult = MeshTopology::build(mesh);
    if (!topologyResult.ok()) {
        return stats;
    }
    const MeshTopology& topology = topologyResult.value();

    std::vector<double> edgeLengths;
    edgeLengths.reserve(topology.edges().size());

    double qualitySum = 0.0;
    stats.minTriangleQuality = mesh.faces.empty() ? 0.0 : std::numeric_limits<double>::infinity();

    for (const Face& face : mesh.faces) {
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3& b = mesh.vertices[face.v[1]];
        const Vec3& c = mesh.vertices[face.v[2]];
        stats.area += triangleArea(a, b, c);
        const double q = manumesh::detail::triangleQuality(a, b, c);
        qualitySum += q;
        stats.minTriangleQuality = std::min(stats.minTriangleQuality, q);
    }

    for (const TopologyEdge& edge : topology.edges()) {
        const int a = edge.vertices[0];
        const int b = edge.vertices[1];
        edgeLengths.push_back((mesh.vertices[a] - mesh.vertices[b]).norm());
    }

    stats.edges = topology.edgeCount();
    stats.boundaryEdges = topology.boundaryEdgeCount();
    stats.nonManifoldEdges = topology.nonManifoldEdgeCount();
    stats.meanTriangleQuality = mesh.faces.empty() ? 0.0 : qualitySum / static_cast<double>(mesh.faces.size());
    if (mesh.faces.empty()) {
        stats.minTriangleQuality = 0.0;
    }

    if (!edgeLengths.empty()) {
        stats.meanEdgeLength =
            std::accumulate(edgeLengths.begin(), edgeLengths.end(), 0.0) / static_cast<double>(edgeLengths.size());
        double variance = 0.0;
        for (double value : edgeLengths) {
            const double d = value - stats.meanEdgeLength;
            variance += d * d;
        }
        variance /= static_cast<double>(edgeLengths.size());
        stats.edgeLengthCv = stats.meanEdgeLength > 1e-30 ? std::sqrt(variance) / stats.meanEdgeLength : 0.0;
    }

    return stats;
}

DistanceStats compareMeshesBySampledDistance(const Mesh& original, const Mesh& simplified, int maxSamples) {
    DistanceStats stats;
    if (original.empty() || simplified.empty()) {
        return stats;
    }

    auto accumulate = [&](const Mesh& from, const Mesh& to, double& mean, double& maxValue) {
        const std::vector<Vec3> points = sampleSurfacePoints(from, maxSamples);
        const manumesh::detail::MeshDistanceIndex index(to);
        if (points.empty() || index.empty()) {
            mean = 0.0;
            maxValue = 0.0;
            return;
        }
        double sum = 0.0;
        double maxSq = 0.0;
        for (const Vec3& point : points) {
            const double d2 = index.distanceSquared(point);
            sum += std::sqrt(d2);
            maxSq = std::max(maxSq, d2);
        }
        mean = sum / static_cast<double>(points.size());
        maxValue = std::sqrt(maxSq);
    };

    accumulate(original, simplified, stats.meanOriginalToSimplified, stats.maxOriginalToSimplified);
    accumulate(simplified, original, stats.meanSimplifiedToOriginal, stats.maxSimplifiedToOriginal);
    return stats;
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
