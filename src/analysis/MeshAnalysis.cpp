/**
 * @file src/analysis/MeshAnalysis.cpp
 * @brief 计算网格质量统计和双向采样曲面距离。
 * @ingroup manumesh_analysis
 *
 * @details 分析例程在文档说明处允许不可用面，并在不修改输入网格的情况下报告测量结果。
 */

#include "algorithms/analysis/MeshAnalysis.h"

#include "common/detail/GeometryPredicates.h"
#include "common/detail/MeshDistanceIndex.h"
#include "core/MeshTopology.h"
#include "core/Tolerances.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace manumesh {
namespace analysis {
namespace {

bool finitePoint(const Vec3& point) {
    return std::isfinite(point.x()) && std::isfinite(point.y()) && std::isfinite(point.z());
}

bool usableFace(const Mesh& mesh, const Face& face) {
    for (int vertex : face.v) {
        if (vertex < 0 || static_cast<std::size_t>(vertex) >= mesh.vertices.size()) {
            return false;
        }
    }
    if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
        return false;
    }

    const Vec3& a = mesh.vertices[face.v[0]];
    const Vec3& b = mesh.vertices[face.v[1]];
    const Vec3& c = mesh.vertices[face.v[2]];
    if (!finitePoint(a) || !finitePoint(b) || !finitePoint(c)) {
        return false;
    }

    const Vec3 ab = b - a;
    const Vec3 bc = c - b;
    const Vec3 ca = a - c;
    if (!finitePoint(ab) || !finitePoint(bc) || !finitePoint(ca) || !std::isfinite(ab.squaredNorm()) ||
        !std::isfinite(bc.squaredNorm()) || !std::isfinite(ca.squaredNorm())) {
        return false;
    }

    const double area = triangleArea(a, b, c);
    return std::isfinite(area) && area > kMinTriangleArea;
}

Mesh usableSurface(const Mesh& mesh) {
    Mesh surface;
    std::vector<int> remap(mesh.vertices.size(), -1);
    surface.faces.reserve(mesh.faces.size());

    for (const Face& face : mesh.faces) {
        if (!usableFace(mesh, face)) {
            continue;
        }

        Face remappedFace;
        for (int corner = 0; corner < 3; ++corner) {
            const int originalVertex = face.v[corner];
            int& remappedVertex = remap[static_cast<std::size_t>(originalVertex)];
            if (remappedVertex < 0) {
                remappedVertex = static_cast<int>(surface.vertices.size());
                surface.vertices.push_back(mesh.vertices[originalVertex]);
            }
            remappedFace.v[corner] = remappedVertex;
        }
        surface.faces.push_back(remappedFace);
    }
    return surface;
}

int clampedCount(std::size_t count) {
    const std::size_t maxCount = static_cast<std::size_t>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(count, maxCount));
}

std::vector<Vec3> sampleSurfacePoints(const Mesh& mesh, int maxSamples) {
    std::vector<double> cumulative;
    cumulative.reserve(mesh.faces.size());
    double totalArea = 0.0;
    for (const Face& face : mesh.faces) {
        const double area = triangleArea(mesh.vertices[face.v[0]], mesh.vertices[face.v[1]], mesh.vertices[face.v[2]]);
        const double nextArea = totalArea + area;
        if (!std::isfinite(area) || area <= kMinTriangleArea || !std::isfinite(nextArea)) {
            return {};
        }
        totalArea = nextArea;
        cumulative.push_back(totalArea);
    }
    if (totalArea <= kMinTriangleArea || maxSamples <= 0) {
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
        const Vec3 point =
            b0 * mesh.vertices[face.v[0]] + b1 * mesh.vertices[face.v[1]] + b2 * mesh.vertices[face.v[2]];
        if (finitePoint(point)) {
            points.push_back(point);
        }
    }
    return points;
}

} // 命名空间

MeshStats computeMeshStats(const Mesh& mesh) {
    MeshStats stats;
    stats.vertices = clampedCount(mesh.vertices.size());
    stats.faces = clampedCount(mesh.faces.size());

    const Mesh surface = usableSurface(mesh);

    const Result<MeshTopology> topologyResult = MeshTopology::build(surface);
    if (!topologyResult.ok()) {
        return stats;
    }
    const MeshTopology& topology = topologyResult.value();

    std::vector<double> edgeLengths;
    edgeLengths.reserve(topology.edges().size());

    long double areaSum = 0.0L;
    long double qualitySum = 0.0L;
    stats.minTriangleQuality = surface.faces.empty() ? 0.0 : std::numeric_limits<double>::infinity();

    for (const Face& face : surface.faces) {
        const Vec3& a = surface.vertices[face.v[0]];
        const Vec3& b = surface.vertices[face.v[1]];
        const Vec3& c = surface.vertices[face.v[2]];
        areaSum += static_cast<long double>(triangleArea(a, b, c));
        const double q = manumesh::common::triangleQuality(a, b, c);
        qualitySum += static_cast<long double>(q);
        stats.minTriangleQuality = std::min(stats.minTriangleQuality, q);
    }

    for (const TopologyEdge& edge : topology.edges()) {
        const int a = edge.vertices[0];
        const int b = edge.vertices[1];
        const double length = (surface.vertices[a] - surface.vertices[b]).norm();
        if (std::isfinite(length)) {
            edgeLengths.push_back(length);
        }
    }

    stats.edges = topology.edgeCount();
    stats.boundaryEdges = topology.boundaryEdgeCount();
    stats.nonManifoldEdges = topology.nonManifoldEdgeCount();
    const double area = static_cast<double>(areaSum);
    stats.area = std::isfinite(area) ? area : 0.0;
    if (!surface.faces.empty()) {
        const double meanQuality = static_cast<double>(qualitySum / static_cast<long double>(surface.faces.size()));
        stats.meanTriangleQuality = std::isfinite(meanQuality) ? meanQuality : 0.0;
    } else {
        stats.minTriangleQuality = 0.0;
    }

    if (!edgeLengths.empty()) {
        const long double lengthSum = std::accumulate(edgeLengths.begin(), edgeLengths.end(), 0.0L);
        const double meanLength = static_cast<double>(lengthSum / static_cast<long double>(edgeLengths.size()));
        stats.meanEdgeLength = std::isfinite(meanLength) ? meanLength : 0.0;
        long double variance = 0.0L;
        for (double value : edgeLengths) {
            const long double d = static_cast<long double>(value) - static_cast<long double>(stats.meanEdgeLength);
            variance += d * d;
        }
        variance /= static_cast<long double>(edgeLengths.size());
        const double cv =
            stats.meanEdgeLength > 1e-30 ? static_cast<double>(std::sqrt(variance) / stats.meanEdgeLength) : 0.0;
        stats.edgeLengthCv = std::isfinite(cv) ? cv : 0.0;
    }

    return stats;
}

DistanceStats compareMeshesBySampledDistance(const Mesh& original, const Mesh& simplified, int maxSamples) {
    DistanceStats stats;
    if (maxSamples <= 0) {
        return stats;
    }

    const Mesh originalSurface = usableSurface(original);
    const Mesh simplifiedSurface = usableSurface(simplified);
    if (originalSurface.empty() || simplifiedSurface.empty()) {
        return stats;
    }

    auto accumulate = [&](const Mesh& from, const Mesh& to, double& mean, double& maxValue) {
        const std::vector<Vec3> points = sampleSurfacePoints(from, maxSamples);
        const manumesh::common::MeshDistanceIndex index(to);
        if (points.empty() || index.empty()) {
            mean = 0.0;
            maxValue = 0.0;
            return;
        }
        double sum = 0.0;
        double maxSq = 0.0;
        std::size_t finiteSampleCount = 0;
        for (const Vec3& point : points) {
            const double d2 = index.distanceSquared(point);
            if (!std::isfinite(d2) || d2 < 0.0) {
                continue;
            }
            const double distance = std::sqrt(d2);
            const double nextSum = sum + distance;
            if (!std::isfinite(distance) || !std::isfinite(nextSum)) {
                continue;
            }
            sum = nextSum;
            maxSq = std::max(maxSq, d2);
            ++finiteSampleCount;
        }
        if (finiteSampleCount == 0) {
            mean = 0.0;
            maxValue = 0.0;
            return;
        }
        mean = sum / static_cast<double>(finiteSampleCount);
        maxValue = std::sqrt(maxSq);
    };

    accumulate(originalSurface, simplifiedSurface, stats.meanOriginalToSimplified, stats.maxOriginalToSimplified);
    accumulate(simplifiedSurface, originalSurface, stats.meanSimplifiedToOriginal, stats.maxSimplifiedToOriginal);
    return stats;
}

} // namespace analysis
} // namespace manumesh
