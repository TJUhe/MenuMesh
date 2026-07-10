#include "detail/CollapseLegality.h"

#include "common/detail/GeometryPredicates.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>

namespace manumesh::simplification {
namespace {

struct NewTriangle {
    int faceId = -1;
    std::array<int, 3> ids{};
    std::array<Vec3, 3> p{};
};

struct OldTriangle {
    std::array<Vec3, 3> p{};
};

std::unordered_set<int> collectTouchedFaces(const CollapseLegalityInput& input) {
    std::unordered_set<int> touchedFaces = input.mesh.topology.vertexFaces[input.edge.keep];
    const auto& removeFaces = input.mesh.topology.vertexFaces[input.edge.remove];
    touchedFaces.insert(removeFaces.begin(), removeFaces.end());
    return touchedFaces;
}

void addTriangleSamples(const std::array<Vec3, 3>& triangle, std::vector<Vec3>& samples) {
    const Vec3& a = triangle[0];
    const Vec3& b = triangle[1];
    const Vec3& c = triangle[2];
    samples.push_back(a);
    samples.push_back(b);
    samples.push_back(c);
    samples.push_back(0.5 * (a + b));
    samples.push_back(0.5 * (b + c));
    samples.push_back(0.5 * (c + a));
    samples.push_back((a + b + c) / 3.0);
}

bool containsBothEndpoints(const FaceState& face, int keep, int remove) {
    const bool containsKeep = face.v[0] == keep || face.v[1] == keep || face.v[2] == keep;
    const bool containsRemove = face.v[0] == remove || face.v[1] == remove || face.v[2] == remove;
    return containsKeep && containsRemove;
}

bool remapCollapsedFace(const FaceState& face, int keep, int remove, std::array<int, 3>& mapped) {
    bool touches = false;
    mapped = face.v;
    for (int& id : mapped) {
        if (id == keep || id == remove) {
            touches = true;
        }
        if (id == remove) {
            id = keep;
        }
    }
    return touches;
}

std::array<Vec3, 3> mappedTrianglePositions(
    const std::array<int, 3>& mapped, int keep, const Vec3& newPosition, const std::vector<VertexState>& vertices
) {
    std::array<Vec3, 3> p = {
        vertices[mapped[0]].p,
        vertices[mapped[1]].p,
        vertices[mapped[2]].p,
    };
    for (int i = 0; i < 3; ++i) {
        if (mapped[i] == keep) {
            p[i] = newPosition;
        }
    }
    return p;
}

CollapseRejectReason collectNewTriangles(
    const CollapseLegalityInput& input,
    const std::unordered_set<int>& touchedFaces,
    std::vector<OldTriangle>& oldTriangles,
    std::vector<NewTriangle>& newTriangles,
    std::vector<Vec3>& localReferencePoints
) {
    const int keep = input.edge.keep;
    const int remove = input.edge.remove;
    const std::vector<FaceState>& faces = input.mesh.faces;
    const std::vector<VertexState>& vertices = input.mesh.vertices;
    const bool measureLocalError = input.maxLocalError > 0.0;

    for (int faceId : touchedFaces) {
        const FaceState& face = faces[faceId];
        if (!face.active) {
            continue;
        }
        if (measureLocalError) {
            const std::array<Vec3, 3> oldTriangle = {
                vertices[face.v[0]].p,
                vertices[face.v[1]].p,
                vertices[face.v[2]].p,
            };
            oldTriangles.push_back(OldTriangle{oldTriangle});
            addTriangleSamples(oldTriangle, localReferencePoints);
        }

        std::array<int, 3> mapped{};
        const bool touches = remapCollapsedFace(face, keep, remove, mapped);
        if (!touches || containsBothEndpoints(face, keep, remove)) {
            continue;
        }
        if (mapped[0] == mapped[1] || mapped[1] == mapped[2] || mapped[0] == mapped[2]) {
            return CollapseRejectReason::Topology;
        }
        const Vec3 oldNormal = triangleNormal(vertices[face.v[0]].p, vertices[face.v[1]].p, vertices[face.v[2]].p);
        const std::array<Vec3, 3> p = mappedTrianglePositions(mapped, keep, input.newPosition, vertices);
        const Vec3& a = p[0];
        const Vec3& b = p[1];
        const Vec3& c = p[2];
        const double area = triangleArea(a, b, c);
        if (area <= input.areaEps) {
            return CollapseRejectReason::Topology;
        }
        if (input.minTriangleQuality > 0.0 && manumesh::detail::triangleQuality(a, b, c) < input.minTriangleQuality) {
            return CollapseRejectReason::TriangleQuality;
        }
        if (input.minNormalDot > -1.0 && oldNormal.norm() > 1e-20) {
            const Vec3 newNormal = triangleNormal(a, b, c);
            if (newNormal.norm() <= 1e-20 || oldNormal.dot(newNormal) < input.minNormalDot) {
                return CollapseRejectReason::NormalFlip;
            }
        }
        if (input.preventLocalIntersections || measureLocalError) {
            newTriangles.push_back(NewTriangle{faceId, mapped, p});
        }
    }
    return CollapseRejectReason::None;
}

CollapseRejectReason checkLocalError(
    const CollapseLegalityInput& input,
    const std::vector<OldTriangle>& oldTriangles,
    const std::vector<NewTriangle>& newTriangles,
    const std::vector<Vec3>& localReferencePoints
) {
    if (input.maxLocalError > 0.0 && !localReferencePoints.empty()) {
        if (newTriangles.empty()) {
            return CollapseRejectReason::LocalError;
        }
        const double maxError2 = input.maxLocalError * input.maxLocalError;
        for (const Vec3& point : localReferencePoints) {
            double best = std::numeric_limits<double>::infinity();
            for (const NewTriangle& tri : newTriangles) {
                best =
                    std::min(best, manumesh::detail::pointTriangleDistanceSquared(point, tri.p[0], tri.p[1], tri.p[2]));
            }
            if (!std::isfinite(best) || best > maxError2) {
                return CollapseRejectReason::LocalError;
            }
        }

        std::vector<Vec3> newSamples;
        newSamples.reserve(newTriangles.size() * 7);
        for (const NewTriangle& triangle : newTriangles) {
            addTriangleSamples(triangle.p, newSamples);
        }
        for (const Vec3& point : newSamples) {
            double bestOld = std::numeric_limits<double>::infinity();
            for (const OldTriangle& triangle : oldTriangles) {
                bestOld = std::min(
                    bestOld,
                    manumesh::detail::pointTriangleDistanceSquared(point, triangle.p[0], triangle.p[1], triangle.p[2])
                );
            }
            if (!std::isfinite(bestOld) || bestOld > maxError2) {
                return CollapseRejectReason::LocalError;
            }
            if (input.referenceSurface && !input.referenceSurface->empty()) {
                const double referenceDistance = input.referenceSurface->distanceSquared(point);
                if (!std::isfinite(referenceDistance) || referenceDistance > maxError2) {
                    return CollapseRejectReason::LocalError;
                }
            }
        }
    }
    return CollapseRejectReason::None;
}

bool sharesVertex(const std::array<int, 3>& a, const std::array<int, 3>& b) {
    for (int lhs : a) {
        for (int rhs : b) {
            if (lhs == rhs) {
                return true;
            }
        }
    }
    return false;
}

CollapseRejectReason checkLocalIntersections(
    const CollapseLegalityInput& input,
    const std::unordered_set<int>& touchedFaces,
    const std::vector<NewTriangle>& newTriangles
) {
    if (!input.preventLocalIntersections) {
        return CollapseRejectReason::None;
    }

    const double eps = std::sqrt(std::max(input.areaEps, 1e-30));
    const std::vector<FaceState>& faces = input.mesh.faces;
    const std::vector<VertexState>& vertices = input.mesh.vertices;
    const bool useSpatialCandidates = input.spatialIndex && input.spatialIndex->enabled();
    for (const NewTriangle& tri : newTriangles) {
        const auto [triLo, triHi] = manumesh::detail::triangleAabb(tri.p, eps);
        const std::vector<int> spatialCandidates =
            useSpatialCandidates ? input.spatialIndex->query(triLo, triHi) : std::vector<int>();
        const int candidateCount =
            useSpatialCandidates ? static_cast<int>(spatialCandidates.size()) : static_cast<int>(faces.size());
        for (int candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex) {
            const int faceId = useSpatialCandidates ? spatialCandidates[candidateIndex] : candidateIndex;
            if (!faces[faceId].active || touchedFaces.find(faceId) != touchedFaces.end()) {
                continue;
            }
            const FaceState& face = faces[faceId];
            if (sharesVertex(tri.ids, face.v)) {
                continue;
            }
            const std::array<Vec3, 3> other = {
                vertices[face.v[0]].p,
                vertices[face.v[1]].p,
                vertices[face.v[2]].p,
            };
            if (manumesh::detail::trianglesIntersect(tri.p, other, eps)) {
                return CollapseRejectReason::SelfIntersection;
            }
        }
    }
    return CollapseRejectReason::None;
}

} // namespace

CollapseRejectReason collapseRejectReason(const CollapseLegalityInput& input) {
    const int keep = input.edge.keep;
    const int remove = input.edge.remove;
    if (!collapseWouldPreserveLinkCondition(keep, remove, input.mesh.faces, input.mesh.vertices, input.mesh.topology)) {
        return CollapseRejectReason::Topology;
    }

    std::vector<NewTriangle> newTriangles;
    std::vector<OldTriangle> oldTriangles;
    std::vector<Vec3> localReferencePoints;
    if (input.maxLocalError > 0.0) {
        localReferencePoints.push_back(input.mesh.vertices[keep].p);
        localReferencePoints.push_back(input.mesh.vertices[remove].p);
    }

    const std::unordered_set<int> touchedFaces = collectTouchedFaces(input);
    CollapseRejectReason reason =
        collectNewTriangles(input, touchedFaces, oldTriangles, newTriangles, localReferencePoints);
    if (reason != CollapseRejectReason::None) {
        return reason;
    }

    reason = checkLocalError(input, oldTriangles, newTriangles, localReferencePoints);
    if (reason != CollapseRejectReason::None) {
        return reason;
    }

    reason = checkLocalIntersections(input, touchedFaces, newTriangles);
    if (reason != CollapseRejectReason::None) {
        return reason;
    }

    return CollapseRejectReason::None;
}

} // namespace manumesh::simplification
