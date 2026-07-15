#include "detail/QualityRefinement.h"

#include "common/detail/GeometryPredicates.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>

namespace manumesh::simplification {
namespace {

struct LocalTriangle {
    int faceId = -1;
    std::array<int, 3> ids{};
    std::array<Vec3, 3> p{};
};

std::array<Vec3, 7> triangleSamples(const std::array<Vec3, 3>& triangle) {
    const Vec3& a = triangle[0];
    const Vec3& b = triangle[1];
    const Vec3& c = triangle[2];
    return {a, b, c, 0.5 * (a + b), 0.5 * (b + c), 0.5 * (c + a), (a + b + c) / 3.0};
}

std::vector<int> incidentFaces(int vertex, const QualityRefinementInput& input) {
    std::vector<int> result;
    if (vertex < 0 || vertex >= static_cast<int>(input.topology.vertexFaces.size())) {
        return result;
    }
    result.reserve(input.topology.vertexFaces[vertex].size());
    for (int faceId : input.topology.vertexFaces[vertex]) {
        if (faceId >= 0 && faceId < static_cast<int>(input.faces.size()) && input.faces[faceId].active) {
            result.push_back(faceId);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<LocalTriangle> makeLocalTriangles(
    int vertex, const Vec3& position, const std::vector<int>& faceIds, const QualityRefinementInput& input
) {
    std::vector<LocalTriangle> result;
    result.reserve(faceIds.size());
    for (int faceId : faceIds) {
        const FaceState& face = input.faces[faceId];
        LocalTriangle triangle;
        triangle.faceId = faceId;
        triangle.ids = face.v;
        for (int corner = 0; corner < 3; ++corner) {
            const int id = face.v[corner];
            triangle.p[corner] = id == vertex ? position : input.vertices[id].p;
        }
        result.push_back(triangle);
    }
    return result;
}

double minimumSampleDistanceSquared(const Vec3& point, const std::vector<LocalTriangle>& triangles) {
    double best = std::numeric_limits<double>::infinity();
    for (const LocalTriangle& triangle : triangles) {
        best = std::min(
            best, manumesh::common::pointTriangleDistanceSquared(point, triangle.p[0], triangle.p[1], triangle.p[2])
        );
    }
    return best;
}

bool respectsErrorEnvelope(
    const std::vector<LocalTriangle>& oldTriangles,
    const std::vector<LocalTriangle>& newTriangles,
    const QualityRefinementInput& input
) {
    if (input.maxLocalError <= 0.0) {
        return true;
    }
    const double maxError2 = input.maxLocalError * input.maxLocalError;
    for (const LocalTriangle& triangle : oldTriangles) {
        for (const Vec3& sample : triangleSamples(triangle.p)) {
            if (minimumSampleDistanceSquared(sample, newTriangles) > maxError2) {
                return false;
            }
        }
    }
    for (const LocalTriangle& triangle : newTriangles) {
        for (const Vec3& sample : triangleSamples(triangle.p)) {
            if (minimumSampleDistanceSquared(sample, oldTriangles) > maxError2) {
                return false;
            }
            if (input.referenceSurface) {
                const double referenceDistance = input.referenceSurface->distanceSquared(sample);
                if (!std::isfinite(referenceDistance) || referenceDistance > maxError2) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool createsLocalIntersection(
    const std::vector<LocalTriangle>& newTriangles,
    const std::unordered_set<int>& touchedFaces,
    const QualityRefinementInput& input
) {
    if (!input.options.preventLocalIntersections) {
        return false;
    }
    // Same dimension chain as CollapseLegality::checkLocalIntersections:
    // trianglesIntersect takes a RELATIVE tolerance normalized by the local
    // triangle scale, and the spatial query window is padded by the matching
    // length-dimension slack.
    constexpr double kRelativeIntersectionEps = 1e-9;

    for (std::size_t i = 0; i < newTriangles.size(); ++i) {
        for (std::size_t j = i + 1; j < newTriangles.size(); ++j) {
            if (manumesh::common::trianglesIntersectBeyondSharedTopology(
                    newTriangles[i].ids,
                    newTriangles[i].p,
                    newTriangles[j].ids,
                    newTriangles[j].p,
                    kRelativeIntersectionEps
                )) {
                return true;
            }
        }
    }

    const bool useSpatialCandidates = input.spatialIndex && input.spatialIndex->enabled();
    for (const LocalTriangle& triangle : newTriangles) {
        auto [lo, hi] = manumesh::common::triangleAabb(triangle.p, 0.0);
        const double pad = kRelativeIntersectionEps * (hi - lo).maxCoeff();
        lo -= Vec3::Constant(pad);
        hi += Vec3::Constant(pad);
        const std::vector<int> spatialCandidates =
            useSpatialCandidates ? input.spatialIndex->query(lo, hi) : std::vector<int>();
        const int candidateCount =
            useSpatialCandidates ? static_cast<int>(spatialCandidates.size()) : static_cast<int>(input.faces.size());
        for (int candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex) {
            const int faceId = useSpatialCandidates ? spatialCandidates[candidateIndex] : candidateIndex;
            if (!input.faces[faceId].active || touchedFaces.find(faceId) != touchedFaces.end()) {
                continue;
            }
            const FaceState& face = input.faces[faceId];
            const std::array<Vec3, 3> other = {
                input.vertices[face.v[0]].p,
                input.vertices[face.v[1]].p,
                input.vertices[face.v[2]].p,
            };
            if (manumesh::common::trianglesIntersectBeyondSharedTopology(
                    triangle.ids, triangle.p, face.v, other, kRelativeIntersectionEps
                )) {
                return true;
            }
        }
    }
    return false;
}

bool improvesLocalQuality(
    const std::vector<LocalTriangle>& oldTriangles,
    const std::vector<LocalTriangle>& newTriangles,
    const QualityRefinementInput& input
) {
    double oldMin = std::numeric_limits<double>::infinity();
    double newMin = std::numeric_limits<double>::infinity();
    double oldSum = 0.0;
    double newSum = 0.0;
    for (std::size_t i = 0; i < oldTriangles.size(); ++i) {
        const LocalTriangle& oldTriangle = oldTriangles[i];
        const LocalTriangle& newTriangle = newTriangles[i];
        if (triangleArea(newTriangle.p[0], newTriangle.p[1], newTriangle.p[2]) <= input.areaEps) {
            return false;
        }
        const double oldQuality =
            manumesh::common::triangleQuality(oldTriangle.p[0], oldTriangle.p[1], oldTriangle.p[2]);
        const double newQuality =
            manumesh::common::triangleQuality(newTriangle.p[0], newTriangle.p[1], newTriangle.p[2]);
        oldMin = std::min(oldMin, oldQuality);
        newMin = std::min(newMin, newQuality);
        oldSum += oldQuality;
        newSum += newQuality;

        const Vec3 oldNormal = triangleNormal(oldTriangle.p[0], oldTriangle.p[1], oldTriangle.p[2]);
        const Vec3 newNormal = triangleNormal(newTriangle.p[0], newTriangle.p[1], newTriangle.p[2]);
        const double normalDot = oldNormal.dot(newNormal);
        if (oldNormal.norm() <= 1e-20 || newNormal.norm() <= 1e-20 || normalDot <= 1e-12 ||
            (input.minNormalDot > -1.0 && normalDot < input.minNormalDot)) {
            return false;
        }
    }
    const double improvementTolerance = 1e-10 * std::max(1.0, std::abs(oldMin));
    const double meanTolerance = 1e-12 * static_cast<double>(oldTriangles.size());
    return newMin > oldMin + improvementTolerance && newSum + meanTolerance >= oldSum;
}

Vec3 tangentialCentroidCandidate(int vertex, const QualityRefinementInput& input) {
    const std::vector<int> neighbors = activeNeighborsOf(vertex, input.faces, input.vertices, input.topology);
    if (neighbors.empty()) {
        return input.vertices[vertex].p;
    }
    Vec3 centroid = Vec3::Zero();
    for (int neighbor : neighbors) {
        centroid += input.vertices[neighbor].p;
    }
    centroid /= static_cast<double>(neighbors.size());

    Vec3 normal = Vec3::Zero();
    for (int faceId : input.topology.vertexFaces[vertex]) {
        if (faceId < 0 || faceId >= static_cast<int>(input.faces.size()) || !input.faces[faceId].active) {
            continue;
        }
        const FaceState& face = input.faces[faceId];
        const Vec3& a = input.vertices[face.v[0]].p;
        const Vec3& b = input.vertices[face.v[1]].p;
        const Vec3& c = input.vertices[face.v[2]].p;
        normal += (b - a).cross(c - a);
    }
    if (normal.norm() <= 1e-20) {
        return input.vertices[vertex].p;
    }
    normal.normalize();
    const Vec3 displacement = centroid - input.vertices[vertex].p;
    return input.vertices[vertex].p + displacement - normal * displacement.dot(normal);
}

/// Local curve tangent for a soft-protected feature vertex: the finite
/// difference of its two same-loop neighbors in the current mesh. Returns
/// false for endpoints, junction-like configurations (neighbor count != 2),
/// and degenerate spans. Neighbors come from activeNeighborsOf, which sorts
/// ascending, so the difference order is deterministic.
bool localFeatureCurveTangent(int vertex, const QualityRefinementInput& input, Vec3& outTangent) {
    const VertexState& state = input.vertices[vertex];
    if (state.featureLoopId < 0) {
        return false;
    }
    const std::vector<int> neighbors = activeNeighborsOf(vertex, input.faces, input.vertices, input.topology);
    int first = -1;
    int second = -1;
    int sameLoopCount = 0;
    for (int neighbor : neighbors) {
        const VertexState& other = input.vertices[neighbor];
        if (!other.isFeature || other.featureLoopId != state.featureLoopId) {
            continue;
        }
        if (sameLoopCount == 0) {
            first = neighbor;
        } else if (sameLoopCount == 1) {
            second = neighbor;
        }
        ++sameLoopCount;
    }
    if (sameLoopCount != 2) {
        return false;
    }
    const Vec3 span = input.vertices[second].p - input.vertices[first].p;
    const double length = span.norm();
    if (length <= 1e-20) {
        return false;
    }
    outTangent = span / length;
    return true;
}

/// Relaxation displacement for one vertex with the feature constraint tier
/// applied (see skill note on remeshing constraints): free vertices move in
/// the tangent plane, soft-protected feature-curve vertices are restricted
/// to the one-dimensional local curve tangent so repeated refinement passes
/// can only slide along the crease instead of rounding it, and junction or
/// endpoint feature vertices stay frozen (zero displacement).
Vec3 refinementDisplacement(int vertex, const QualityRefinementInput& input) {
    const VertexState& state = input.vertices[vertex];
    const Vec3 target = tangentialCentroidCandidate(vertex, input);
    Vec3 displacement = target - state.p;
    if (!state.isFeature) {
        return displacement;
    }
    if (state.featureJunction) {
        return Vec3::Zero();
    }
    Vec3 tangent = Vec3::Zero();
    if (!localFeatureCurveTangent(vertex, input, tangent)) {
        return Vec3::Zero();
    }
    return tangent * displacement.dot(tangent);
}

bool tryRefineVertex(int vertex, const Vec3& displacement, const QualityRefinementInput& input) {
    const std::vector<int> faceIds = incidentFaces(vertex, input);
    if (faceIds.size() < 3) {
        return false;
    }
    const Vec3 oldPosition = input.vertices[vertex].p;
    if (!displacement.allFinite() || displacement.squaredNorm() <= 1e-24) {
        return false;
    }

    const std::vector<LocalTriangle> oldTriangles = makeLocalTriangles(vertex, oldPosition, faceIds, input);
    const std::unordered_set<int> touchedFaces(faceIds.begin(), faceIds.end());
    const bool featureVertex = input.vertices[vertex].isFeature;
    constexpr std::array<double, 5> kLineSearch = {1.0, 0.5, 0.25, 0.125, 0.0625};
    for (double alpha : kLineSearch) {
        const Vec3 candidate = oldPosition + alpha * displacement;
        // Soft feature vertices stay movable, but each relocation must respect
        // the same curve-deviation budget as collapse placements.
        if (featureVertex && !featureCurveBudgetAllows(
                                 input.vertices[vertex],
                                 input.vertices[vertex],
                                 input.featureCurves,
                                 input.primitiveFits,
                                 input.options,
                                 input.meshDiagonal,
                                 candidate
                             )) {
            continue;
        }
        const std::vector<LocalTriangle> newTriangles = makeLocalTriangles(vertex, candidate, faceIds, input);
        if (!improvesLocalQuality(oldTriangles, newTriangles, input) ||
            !respectsErrorEnvelope(oldTriangles, newTriangles, input) ||
            createsLocalIntersection(newTriangles, touchedFaces, input)) {
            continue;
        }

        if (input.spatialIndex) {
            for (int faceId : faceIds) {
                input.spatialIndex->removeFace(faceId);
            }
        }
        input.vertices[vertex].p = candidate;
        if (input.spatialIndex) {
            for (int faceId : faceIds) {
                input.spatialIndex->updateFace(faceId, input.faces[faceId], input.vertices);
            }
        }
        return true;
    }
    return false;
}

} // namespace

void runQualityRefinement(const QualityRefinementInput& input, SimplifyReport& report) {
    for (int iteration = 0; iteration < input.options.qualityRefinementIterations; ++iteration) {
        int acceptedThisIteration = 0;
        ++report.qualityRefinementIterationsCompleted;
        for (int vertex = 0; vertex < static_cast<int>(input.vertices.size()); ++vertex) {
            const VertexState& state = input.vertices[vertex];
            if (!state.active || (input.options.preserveBoundary && state.isBoundary) ||
                input.featurePolicy.isHardProtectedVertex(vertex, input.vertices)) {
                continue;
            }
            const Vec3 displacement = refinementDisplacement(vertex, input);
            if (displacement.squaredNorm() <= 1e-24) {
                continue;
            }
            ++report.qualityRefinementAttemptedMoves;
            if (tryRefineVertex(vertex, displacement, input)) {
                ++report.qualityRefinementAcceptedMoves;
                ++acceptedThisIteration;
            }
        }
        if (acceptedThisIteration == 0) {
            break;
        }
    }
}

} // namespace manumesh::simplification
