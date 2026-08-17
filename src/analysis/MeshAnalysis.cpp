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
#include <cstddef>
#include <limits>
#include <numeric>
#include <vector>

namespace manumesh {
namespace analysis {
namespace {

constexpr int kMaxDistanceSamples = 1000000;

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
    const std::size_t maxInt = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (mesh.vertices.size() > maxInt || mesh.faces.size() > maxInt) {
        return surface;
    }
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
                if (surface.vertices.size() >= maxInt) {
                    return Mesh{};
                }
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
    if (maxSamples <= 0 || mesh.faces.empty()) {
        return {};
    }

    const int samples = std::min(maxSamples, kMaxDistanceSamples);
    std::vector<double> areas;
    areas.reserve(mesh.faces.size());
    double areaScale = 0.0;
    for (const Face& face : mesh.faces) {
        const double area = triangleArea(mesh.vertices[face.v[0]], mesh.vertices[face.v[1]], mesh.vertices[face.v[2]]);
        if (!std::isfinite(area) || area <= kMinTriangleArea) {
            return {};
        }
        areas.push_back(area);
        areaScale = std::max(areaScale, area);
    }
    if (!(areaScale > 0.0) || !std::isfinite(areaScale)) {
        return {};
    }

    std::vector<double> cumulative;
    cumulative.reserve(mesh.faces.size());
    double scaledTotalArea = 0.0;
    for (double area : areas) {
        scaledTotalArea += area / areaScale;
        cumulative.push_back(scaledTotalArea);
    }
    if (!(scaledTotalArea > 0.0) || !std::isfinite(scaledTotalArea)) {
        return {};
    }

    std::vector<Vec3> points;
    points.reserve(samples);

    // Find connected surface components by uniting triangle vertices. When
    // the sample budget permits it, seed one deterministic point per
    // component so a small disconnected feature is not hidden by area ratios.
    std::vector<int> parent(mesh.vertices.size());
    std::iota(parent.begin(), parent.end(), 0);
    const auto findRoot = [&](int vertex) {
        int root = vertex;
        while (parent[static_cast<std::size_t>(root)] != root) {
            root = parent[static_cast<std::size_t>(root)];
        }
        while (parent[static_cast<std::size_t>(vertex)] != vertex) {
            const int next = parent[static_cast<std::size_t>(vertex)];
            parent[static_cast<std::size_t>(vertex)] = root;
            vertex = next;
        }
        return root;
    };
    const auto unite = [&](int lhs, int rhs) {
        const int lhsRoot = findRoot(lhs);
        const int rhsRoot = findRoot(rhs);
        if (lhsRoot != rhsRoot) {
            parent[static_cast<std::size_t>(rhsRoot)] = lhsRoot;
        }
    };
    for (const Face& face : mesh.faces) {
        unite(face.v[0], face.v[1]);
        unite(face.v[0], face.v[2]);
    }
    std::vector<int> componentByRoot(mesh.vertices.size(), -1);
    std::vector<int> firstFaceByComponent;
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        const int root = findRoot(mesh.faces[faceIndex].v[0]);
        int& component = componentByRoot[static_cast<std::size_t>(root)];
        if (component < 0) {
            component = static_cast<int>(firstFaceByComponent.size());
            firstFaceByComponent.push_back(static_cast<int>(faceIndex));
        }
    }
    if (samples >= static_cast<int>(firstFaceByComponent.size())) {
        for (int faceId : firstFaceByComponent) {
            const Face& face = mesh.faces[static_cast<std::size_t>(faceId)];
            const Vec3 point =
                mesh.vertices[face.v[0]] / 3.0 + mesh.vertices[face.v[1]] / 3.0 + mesh.vertices[face.v[2]] / 3.0;
            if (finitePoint(point)) {
                points.push_back(point);
            }
        }
    }

    const int remainingSamples = samples - static_cast<int>(points.size());
    for (int i = 0; i < remainingSamples; ++i) {
        const double target = (static_cast<double>(i) + 0.5) * scaledTotalArea / static_cast<double>(remainingSamples);
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
        const double b1 = su * (1.0 - vSeed);
        const double b2 = su * vSeed;
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3 point = a + b1 * (mesh.vertices[face.v[1]] - a) + b2 * (mesh.vertices[face.v[2]] - a);
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

    long double qualitySum = 0.0L;
    stats.minTriangleQuality = surface.faces.empty() ? 0.0 : std::numeric_limits<double>::infinity();

    for (const Face& face : surface.faces) {
        const Vec3& a = surface.vertices[face.v[0]];
        const Vec3& b = surface.vertices[face.v[1]];
        const Vec3& c = surface.vertices[face.v[2]];
        const double q = manumesh::common::triangleQuality(a, b, c);
        qualitySum += static_cast<long double>(q);
        stats.minTriangleQuality = std::min(stats.minTriangleQuality, q);
    }

    for (const TopologyEdge& edge : topology.edges()) {
        const int a = edge.vertices[0];
        const int b = edge.vertices[1];
        const double length = (surface.vertices[a] - surface.vertices[b]).stableNorm();
        if (std::isfinite(length)) {
            edgeLengths.push_back(length);
        }
    }

    stats.edges = topology.edgeCount();
    stats.boundaryEdges = topology.boundaryEdgeCount();
    stats.nonManifoldEdges = topology.nonManifoldEdgeCount();
    stats.area = computeSurfaceArea(surface);
    if (!surface.faces.empty()) {
        const double meanQuality = static_cast<double>(qualitySum / static_cast<long double>(surface.faces.size()));
        stats.meanTriangleQuality = std::isfinite(meanQuality) ? meanQuality : 0.0;
    } else {
        stats.minTriangleQuality = 0.0;
    }

    if (!edgeLengths.empty()) {
        const double lengthScale = *std::max_element(edgeLengths.begin(), edgeLengths.end());
        double scaledMean = 0.0;
        for (double value : edgeLengths) {
            scaledMean += value / lengthScale;
        }
        scaledMean /= static_cast<double>(edgeLengths.size());
        stats.meanEdgeLength = scaledMean * lengthScale;
        double scaledVariance = 0.0;
        for (double value : edgeLengths) {
            const double difference = value / lengthScale - scaledMean;
            scaledVariance += difference * difference;
        }
        scaledVariance /= static_cast<double>(edgeLengths.size());
        const double cv = scaledMean > 0.0 ? std::sqrt(scaledVariance) / scaledMean : 0.0;
        stats.edgeLengthCv = std::isfinite(cv) ? cv : 0.0;
    }

    return stats;
}

DistanceStats compareMeshesBySampledDistance(const Mesh& original, const Mesh& simplified, int maxSamples) {
    DistanceStats stats;
    if (maxSamples <= 0) {
        return stats;
    }

    const int boundedSamples = std::min(maxSamples, kMaxDistanceSamples);
    const Mesh originalSurface = usableSurface(original);
    const Mesh simplifiedSurface = usableSurface(simplified);
    if (originalSurface.empty() || simplifiedSurface.empty()) {
        return stats;
    }

    auto accumulate = [&](const Mesh& from, const Mesh& to, double& mean, double& maxValue) {
        const std::vector<Vec3> points = sampleSurfacePoints(from, boundedSamples);
        const manumesh::common::MeshDistanceIndex index(to);
        if (points.empty() || index.empty()) {
            mean = 0.0;
            maxValue = 0.0;
            return;
        }
        double runningMean = 0.0;
        double maxSq = 0.0;
        std::size_t finiteSampleCount = 0;
        for (const Vec3& point : points) {
            const double d2 = index.distanceSquared(point);
            if (!std::isfinite(d2) || d2 < 0.0) {
                continue;
            }
            const double distance = std::sqrt(d2);
            if (!std::isfinite(distance)) {
                continue;
            }
            maxSq = std::max(maxSq, d2);
            ++finiteSampleCount;
            runningMean += (distance - runningMean) / static_cast<double>(finiteSampleCount);
        }
        if (finiteSampleCount == 0) {
            mean = 0.0;
            maxValue = 0.0;
            return;
        }
        mean = runningMean;
        maxValue = std::sqrt(maxSq);
    };

    accumulate(originalSurface, simplifiedSurface, stats.meanOriginalToSimplified, stats.maxOriginalToSimplified);
    accumulate(simplifiedSurface, originalSurface, stats.meanSimplifiedToOriginal, stats.maxSimplifiedToOriginal);
    return stats;
}

} // namespace analysis
} // namespace manumesh
