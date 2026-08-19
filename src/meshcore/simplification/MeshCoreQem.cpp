/**
 * @file src/meshcore/simplification/MeshCoreQem.cpp
 * @brief 实现 MeshCore 的特征保护 QEM 简化。
 * @ingroup manumesh_simplification
 */

#include "algorithms/simplification/MeshCoreQem.h"

#include "meshcore/features/FeatureClassification.h"

#include <Eigen/LU>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace manumesh {
namespace meshcore {
namespace {

constexpr double kTiny = 1e-20;
constexpr double kRelativeAreaEpsilon = 1e-18;
constexpr double kIntersectionRelativeEpsilon = 1e-10;

struct Edge {
    int first = -1;
    int second = -1;

    bool operator<(const Edge& other) const {
        return first != other.first ? first < other.first : second < other.second;
    }
};

Edge sortedEdge(int first, int second) {
    return first < second ? Edge{first, second} : Edge{second, first};
}

struct FaceKey {
    std::array<int, 3> vertices{};

    bool operator<(const FaceKey& other) const { return vertices < other.vertices; }
};

FaceKey faceKey(const std::array<int, 3>& vertices) {
    FaceKey key{vertices};
    std::sort(key.vertices.begin(), key.vertices.end());
    return key;
}

struct FaceState {
    std::array<int, 3> vertices{};
    bool active = true;
};

struct VertexState {
    Vec3 position = Vec3::Zero();
    Mat4 quadric = Mat4::Zero();
    bool active = true;
    int version = 0;
};

struct SimplificationState {
    std::vector<VertexState> vertices;
    std::vector<FaceState> faces;
    std::vector<std::set<int>> vertexFaces;
    std::map<FaceKey, std::set<int>> facesByKey;
    std::vector<char> protectedFeatureVertices;
    std::size_t activeFaces = 0;
};

bool containsVertex(const FaceState& face, int vertex) {
    return face.vertices[0] == vertex || face.vertices[1] == vertex || face.vertices[2] == vertex;
}

bool validVertex(const SimplificationState& state, int vertex) {
    return vertex >= 0 && vertex < static_cast<int>(state.vertices.size()) && state.vertices[vertex].active;
}

void addFaceToTopology(SimplificationState& state, int faceId) {
    const FaceState& face = state.faces[static_cast<std::size_t>(faceId)];
    for (int vertex : face.vertices) {
        state.vertexFaces[static_cast<std::size_t>(vertex)].insert(faceId);
    }
    state.facesByKey[faceKey(face.vertices)].insert(faceId);
}

void removeFaceFromTopology(SimplificationState& state, int faceId) {
    const FaceState& face = state.faces[static_cast<std::size_t>(faceId)];
    for (int vertex : face.vertices) {
        if (vertex >= 0 && vertex < static_cast<int>(state.vertexFaces.size())) {
            state.vertexFaces[static_cast<std::size_t>(vertex)].erase(faceId);
        }
    }
    const FaceKey key = faceKey(face.vertices);
    const auto keyIt = state.facesByKey.find(key);
    if (keyIt == state.facesByKey.end()) {
        return;
    }
    keyIt->second.erase(faceId);
    if (keyIt->second.empty()) {
        state.facesByKey.erase(keyIt);
    }
}

SimplificationState makeState(const Mesh& input) {
    SimplificationState state;
    state.vertices.resize(input.vertices.size());
    state.vertexFaces.resize(input.vertices.size());
    for (std::size_t vertex = 0; vertex < input.vertices.size(); ++vertex) {
        state.vertices[vertex].position = input.vertices[vertex];
    }
    state.faces.resize(input.faces.size());
    for (std::size_t face = 0; face < input.faces.size(); ++face) {
        state.faces[face].vertices = input.faces[face].v;
        addFaceToTopology(state, static_cast<int>(face));
    }
    state.activeFaces = state.faces.size();
    return state;
}

std::vector<int> commonFaces(const SimplificationState& state, int first, int second) {
    if (!validVertex(state, first) || !validVertex(state, second)) {
        return {};
    }
    const std::set<int>& firstFaces = state.vertexFaces[static_cast<std::size_t>(first)];
    const std::set<int>& secondFaces = state.vertexFaces[static_cast<std::size_t>(second)];
    const std::set<int>* smaller = &firstFaces;
    const std::set<int>* larger = &secondFaces;
    if (secondFaces.size() < firstFaces.size()) {
        smaller = &secondFaces;
        larger = &firstFaces;
    }

    std::vector<int> result;
    result.reserve(smaller->size());
    for (int faceId : *smaller) {
        if (larger->count(faceId) == 0U || faceId < 0 || faceId >= static_cast<int>(state.faces.size())) {
            continue;
        }
        const FaceState& face = state.faces[static_cast<std::size_t>(faceId)];
        if (face.active && containsVertex(face, first) && containsVertex(face, second)) {
            result.push_back(faceId);
        }
    }
    return result;
}

std::set<int> activeNeighbors(const SimplificationState& state, int vertex) {
    std::set<int> result;
    if (!validVertex(state, vertex)) {
        return result;
    }
    for (int faceId : state.vertexFaces[static_cast<std::size_t>(vertex)]) {
        const FaceState& face = state.faces[static_cast<std::size_t>(faceId)];
        if (!face.active) {
            continue;
        }
        for (int neighbor : face.vertices) {
            if (neighbor != vertex && validVertex(state, neighbor)) {
                result.insert(neighbor);
            }
        }
    }
    return result;
}

bool isBoundaryVertex(const SimplificationState& state, int vertex) {
    const std::set<int> neighbors = activeNeighbors(state, vertex);
    for (int neighbor : neighbors) {
        if (commonFaces(state, vertex, neighbor).size() == 1U) {
            return true;
        }
    }
    return false;
}

std::set<Edge> collectAllEdges(const SimplificationState& state) {
    std::set<Edge> edges;
    for (const FaceState& face : state.faces) {
        if (!face.active) {
            continue;
        }
        for (int corner = 0; corner < 3; ++corner) {
            const int first = face.vertices[static_cast<std::size_t>(corner)];
            const int second = face.vertices[static_cast<std::size_t>((corner + 1) % 3)];
            if (first != second && validVertex(state, first) && validVertex(state, second)) {
                edges.insert(sortedEdge(first, second));
            }
        }
    }
    return edges;
}

std::vector<int> touchedFaces(const SimplificationState& state, int first, int second) {
    std::vector<int> result;
    if (validVertex(state, first)) {
        result.insert(
            result.end(),
            state.vertexFaces[static_cast<std::size_t>(first)].begin(),
            state.vertexFaces[static_cast<std::size_t>(first)].end()
        );
    }
    if (validVertex(state, second)) {
        result.insert(
            result.end(),
            state.vertexFaces[static_cast<std::size_t>(second)].begin(),
            state.vertexFaces[static_cast<std::size_t>(second)].end()
        );
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

bool preservesLinkCondition(
    const SimplificationState& state, const Edge& edge, const std::vector<int>& edgeFaces
) {
    if (edgeFaces.empty() || edgeFaces.size() > 2U) {
        return false;
    }

    std::set<int> edgeLink;
    for (int faceId : edgeFaces) {
        if (faceId < 0 || faceId >= static_cast<int>(state.faces.size())) {
            return false;
        }
        const FaceState& face = state.faces[static_cast<std::size_t>(faceId)];
        if (!face.active || !containsVertex(face, edge.first) || !containsVertex(face, edge.second)) {
            return false;
        }
        int opposite = -1;
        for (int vertex : face.vertices) {
            if (vertex != edge.first && vertex != edge.second) {
                if (opposite >= 0) {
                    return false;
                }
                opposite = vertex;
            }
        }
        if (!validVertex(state, opposite) || !edgeLink.insert(opposite).second) {
            return false;
        }
    }
    if (edgeLink.size() != edgeFaces.size()) {
        return false;
    }

    const std::set<int> firstNeighbors = activeNeighbors(state, edge.first);
    const std::set<int> secondNeighbors = activeNeighbors(state, edge.second);
    std::set<int> sharedNeighbors;
    std::set_intersection(
        firstNeighbors.begin(),
        firstNeighbors.end(),
        secondNeighbors.begin(),
        secondNeighbors.end(),
        std::inserter(sharedNeighbors, sharedNeighbors.begin())
    );
    if (sharedNeighbors != edgeLink) {
        return false;
    }

    // Do not collapse the only edge of an isolated open triangle and delete
    // its entire connected component.
    return !(edgeFaces.size() == 1U && state.vertexFaces[static_cast<std::size_t>(edge.first)].size() == 1U &&
             state.vertexFaces[static_cast<std::size_t>(edge.second)].size() == 1U);
}

bool preservesBoundaryTopology(
    const SimplificationState& state, const Edge& edge, const std::vector<int>& edgeFaces, bool preserveBoundary
) {
    if (!preserveBoundary) {
        return true;
    }
    const bool firstBoundary = isBoundaryVertex(state, edge.first);
    const bool secondBoundary = isBoundaryVertex(state, edge.second);
    if (firstBoundary != secondBoundary) {
        return false;
    }
    return !firstBoundary || edgeFaces.size() == 1U;
}

void addPlane(Mat4& quadric, const Vec3& normal, const Vec3& point, double weight) {
    const double normalLength = normal.norm();
    if (!(weight > 0.0) || !std::isfinite(weight) || !std::isfinite(normalLength) || normalLength <= kTiny) {
        return;
    }
    const Vec3 unitNormal = normal / normalLength;
    Eigen::Vector4d plane;
    plane << unitNormal.x(), unitNormal.y(), unitNormal.z(), -unitNormal.dot(point);
    quadric += weight * (plane * plane.transpose());
}

void addLineConstraint(Mat4& quadric, const Vec3& point, const Vec3& direction, double weight) {
    const double directionLength = direction.norm();
    if (!(weight > 0.0) || !std::isfinite(weight) || !std::isfinite(directionLength) || directionLength <= kTiny) {
        return;
    }
    const Vec3 tangent = direction / directionLength;
    Vec3 seed = std::abs(tangent.x()) < 0.8 ? Vec3(1.0, 0.0, 0.0) : Vec3(0.0, 1.0, 0.0);
    Vec3 firstNormal = tangent.cross(seed);
    if (firstNormal.norm() <= kTiny) {
        seed = Vec3(0.0, 0.0, 1.0);
        firstNormal = tangent.cross(seed);
    }
    const double firstLength = firstNormal.norm();
    if (firstLength <= kTiny || !std::isfinite(firstLength)) {
        return;
    }
    firstNormal /= firstLength;
    const Vec3 secondNormal = tangent.cross(firstNormal);
    addPlane(quadric, firstNormal, point, 0.5 * weight);
    addPlane(quadric, secondNormal, point, 0.5 * weight);
}

void buildQuadrics(
    SimplificationState& state, const FeatureReport& features, const SimplifyOptions& options
) {
    for (const FaceState& face : state.faces) {
        const Vec3& first = state.vertices[static_cast<std::size_t>(face.vertices[0])].position;
        const Vec3& second = state.vertices[static_cast<std::size_t>(face.vertices[1])].position;
        const Vec3& third = state.vertices[static_cast<std::size_t>(face.vertices[2])].position;
        const Vec3 normal = (second - first).cross(third - first);
        const double area = 0.5 * normal.norm();
        if (!std::isfinite(area) || area <= kTiny) {
            continue;
        }
        for (int vertex : face.vertices) {
            addPlane(state.vertices[static_cast<std::size_t>(vertex)].quadric, normal, first, area);
        }
    }

    const std::set<Edge> edges = collectAllEdges(state);
    if (options.preserveBoundary && options.boundaryConstraintWeight > 0.0) {
        for (const Edge& edge : edges) {
            const std::vector<int> faces = commonFaces(state, edge.first, edge.second);
            if (faces.size() != 1U) {
                continue;
            }
            const FaceState& face = state.faces[static_cast<std::size_t>(faces.front())];
            int opposite = -1;
            for (int vertex : face.vertices) {
                if (vertex != edge.first && vertex != edge.second) {
                    opposite = vertex;
                    break;
                }
            }
            if (opposite < 0) {
                continue;
            }
            const Vec3& first = state.vertices[static_cast<std::size_t>(edge.first)].position;
            const Vec3& second = state.vertices[static_cast<std::size_t>(edge.second)].position;
            const Vec3& third = state.vertices[static_cast<std::size_t>(opposite)].position;
            const Vec3 edgeDirection = second - first;
            const Vec3 boundaryNormal = edgeDirection.cross(third - first).cross(edgeDirection);
            const double weight = options.boundaryConstraintWeight * edgeDirection.squaredNorm();
            addPlane(state.vertices[static_cast<std::size_t>(edge.first)].quadric, boundaryNormal, first, weight);
            addPlane(state.vertices[static_cast<std::size_t>(edge.second)].quadric, boundaryNormal, first, weight);
        }
    }

    if (!options.preserveFeatures || options.featureConstraintWeight <= 0.0) {
        return;
    }
    for (const MeshEdge& featureEdge : features.sharp) {
        const int firstId = featureEdge.first;
        const int secondId = featureEdge.second;
        if (!validVertex(state, firstId) || !validVertex(state, secondId)) {
            continue;
        }
        const Vec3& first = state.vertices[static_cast<std::size_t>(firstId)].position;
        const Vec3& second = state.vertices[static_cast<std::size_t>(secondId)].position;
        const Vec3 direction = second - first;
        const double weight = options.featureConstraintWeight * direction.squaredNorm();
        addLineConstraint(state.vertices[static_cast<std::size_t>(firstId)].quadric, first, direction, weight);
        addLineConstraint(state.vertices[static_cast<std::size_t>(secondId)].quadric, first, direction, weight);
    }
}

struct Placement {
    Vec3 position = Vec3::Zero();
    double cost = std::numeric_limits<double>::infinity();
};

double evaluateQuadric(const Mat4& quadric, const Vec3& point) {
    const Eigen::Vector4d homogeneous(point.x(), point.y(), point.z(), 1.0);
    return homogeneous.dot(quadric * homogeneous);
}

std::vector<Placement> solvePlacements(const Mat4& quadric, const Vec3& first, const Vec3& second) {
    std::vector<Vec3> candidatePoints;
    candidatePoints.reserve(4);

    const Eigen::Matrix3d coefficients = 0.5 * (
        quadric.block<3, 3>(0, 0) + quadric.block<3, 3>(0, 0).transpose()
    );
    const Eigen::Vector3d rightHandSide = -quadric.block<3, 1>(0, 3);
    Eigen::FullPivLU<Eigen::Matrix3d> solver(coefficients);
    solver.setThreshold(1e-12);
    if (solver.rank() == 3) {
        const Vec3 solution = solver.solve(rightHandSide);
        if (solution.allFinite()) {
            candidatePoints.push_back(solution);
        }
    }
    candidatePoints.push_back(first);
    candidatePoints.push_back(second);
    candidatePoints.push_back(0.5 * (first + second));

    const double referenceLengthSquared = std::max(1e-24, (second - first).squaredNorm());
    const double duplicateToleranceSquared = 1e-24 * referenceLengthSquared;
    std::vector<Placement> placements;
    placements.reserve(candidatePoints.size());
    for (const Vec3& point : candidatePoints) {
        if (!point.allFinite()) {
            continue;
        }
        bool duplicate = false;
        for (const Placement& existing : placements) {
            if ((existing.position - point).squaredNorm() <= duplicateToleranceSquared) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        const double cost = evaluateQuadric(quadric, point);
        if (std::isfinite(cost)) {
            placements.push_back(Placement{point, std::max(0.0, cost)});
        }
    }
    std::sort(placements.begin(), placements.end(), [](const Placement& lhs, const Placement& rhs) {
        if (lhs.cost != rhs.cost) {
            return lhs.cost < rhs.cost;
        }
        if (lhs.position.x() != rhs.position.x()) {
            return lhs.position.x() < rhs.position.x();
        }
        if (lhs.position.y() != rhs.position.y()) {
            return lhs.position.y() < rhs.position.y();
        }
        return lhs.position.z() < rhs.position.z();
    });
    return placements;
}

struct Candidate {
    int keep = -1;
    int remove = -1;
    int keepVersion = -1;
    int removeVersion = -1;
    std::array<Placement, 4> placements{};
    int placementCount = 0;
    double cost = std::numeric_limits<double>::infinity();

    bool operator<(const Candidate& other) const {
        if (cost != other.cost) {
            return cost > other.cost;
        }
        if (keep != other.keep) {
            return keep > other.keep;
        }
        return remove > other.remove;
    }
};

bool makeCandidate(const SimplificationState& state, const Edge& edge, Candidate& candidate) {
    if (!validVertex(state, edge.first) || !validVertex(state, edge.second) || edge.first == edge.second) {
        return false;
    }
    const Mat4 combined = state.vertices[static_cast<std::size_t>(edge.first)].quadric +
                          state.vertices[static_cast<std::size_t>(edge.second)].quadric;
    const std::vector<Placement> placements = solvePlacements(
        combined,
        state.vertices[static_cast<std::size_t>(edge.first)].position,
        state.vertices[static_cast<std::size_t>(edge.second)].position
    );
    if (placements.empty()) {
        return false;
    }
    candidate.keep = edge.first;
    candidate.remove = edge.second;
    candidate.keepVersion = state.vertices[static_cast<std::size_t>(edge.first)].version;
    candidate.removeVersion = state.vertices[static_cast<std::size_t>(edge.second)].version;
    candidate.placementCount = std::min(static_cast<int>(placements.size()), static_cast<int>(candidate.placements.size()));
    for (int index = 0; index < candidate.placementCount; ++index) {
        candidate.placements[static_cast<std::size_t>(index)] = placements[static_cast<std::size_t>(index)];
    }
    candidate.cost = candidate.placements.front().cost;
    return std::isfinite(candidate.cost);
}

bool candidateIsCurrent(const SimplificationState& state, const Candidate& candidate) {
    if (!validVertex(state, candidate.keep) || !validVertex(state, candidate.remove)) {
        return false;
    }
    if (state.vertices[static_cast<std::size_t>(candidate.keep)].version != candidate.keepVersion ||
        state.vertices[static_cast<std::size_t>(candidate.remove)].version != candidate.removeVersion) {
        return false;
    }
    return !commonFaces(state, candidate.keep, candidate.remove).empty();
}

double triangleQuality(const std::array<Vec3, 3>& triangle) {
    const Vec3 firstEdge = triangle[1] - triangle[0];
    const Vec3 secondEdge = triangle[2] - triangle[1];
    const Vec3 thirdEdge = triangle[0] - triangle[2];
    const double denominator = firstEdge.squaredNorm() + secondEdge.squaredNorm() + thirdEdge.squaredNorm();
    if (!std::isfinite(denominator) || denominator <= kTiny) {
        return 0.0;
    }
    const double twiceArea = firstEdge.cross(-thirdEdge).norm();
    const double quality = 2.0 * std::sqrt(3.0) * twiceArea / denominator;
    return std::isfinite(quality) ? std::max(0.0, std::min(1.0, quality)) : 0.0;
}

struct TriangleState {
    std::array<int, 3> ids{};
    std::array<Vec3, 3> points{};
};

enum class RejectKind {
    None,
    Topology,
    Constraint,
    Geometry,
    Error,
    Intersection,
};

bool containsFace(const std::vector<int>& faces, int faceId) {
    return std::binary_search(faces.begin(), faces.end(), faceId);
}

RejectKind buildReplacementTriangles(
    const SimplificationState& state,
    const Edge& edge,
    const Vec3& position,
    const std::vector<int>& affectedFaces,
    double areaEpsilon,
    double minQuality,
    std::vector<TriangleState>& triangles
) {
    std::set<FaceKey> newKeys;
    triangles.clear();
    triangles.reserve(affectedFaces.size());
    for (int faceId : affectedFaces) {
        if (faceId < 0 || faceId >= static_cast<int>(state.faces.size())) {
            return RejectKind::Topology;
        }
        const FaceState& face = state.faces[static_cast<std::size_t>(faceId)];
        if (!face.active) {
            continue;
        }
        const bool usesKeep = containsVertex(face, edge.first);
        const bool usesRemove = containsVertex(face, edge.second);
        if (!usesKeep && !usesRemove) {
            continue;
        }
        if (usesKeep && usesRemove) {
            continue;
        }

        TriangleState replacement;
        replacement.ids = face.vertices;
        for (int& vertex : replacement.ids) {
            if (vertex == edge.second) {
                vertex = edge.first;
            }
        }
        if (replacement.ids[0] == replacement.ids[1] || replacement.ids[1] == replacement.ids[2] ||
            replacement.ids[0] == replacement.ids[2]) {
            return RejectKind::Topology;
        }

        const FaceKey key = faceKey(replacement.ids);
        if (!newKeys.insert(key).second) {
            return RejectKind::Topology;
        }
        const auto existing = state.facesByKey.find(key);
        if (existing != state.facesByKey.end()) {
            for (int otherFaceId : existing->second) {
                if (!containsFace(affectedFaces, otherFaceId) &&
                    state.faces[static_cast<std::size_t>(otherFaceId)].active) {
                    return RejectKind::Topology;
                }
            }
        }

        std::array<Vec3, 3> before{};
        for (int corner = 0; corner < 3; ++corner) {
            const int originalVertex = face.vertices[static_cast<std::size_t>(corner)];
            before[static_cast<std::size_t>(corner)] =
                state.vertices[static_cast<std::size_t>(originalVertex)].position;
            replacement.points[static_cast<std::size_t>(corner)] =
                replacement.ids[static_cast<std::size_t>(corner)] == edge.first
                    ? position
                    : state.vertices[static_cast<std::size_t>(replacement.ids[static_cast<std::size_t>(corner)])].position;
        }
        const Vec3 beforeNormal = (before[1] - before[0]).cross(before[2] - before[0]);
        const Vec3 afterNormal =
            (replacement.points[1] - replacement.points[0]).cross(replacement.points[2] - replacement.points[0]);
        const double afterArea = 0.5 * afterNormal.norm();
        if (!std::isfinite(afterArea) || afterArea <= areaEpsilon || !afterNormal.allFinite() ||
            beforeNormal.dot(afterNormal) <= 0.0) {
            return RejectKind::Geometry;
        }
        if (minQuality > 0.0 && triangleQuality(replacement.points) < minQuality) {
            return RejectKind::Geometry;
        }
        triangles.push_back(replacement);
    }
    return triangles.empty() ? RejectKind::Topology : RejectKind::None;
}

bool sharesVertex(const TriangleState& first, const TriangleState& second) {
    for (int firstId : first.ids) {
        for (int secondId : second.ids) {
            if (firstId == secondId) {
                return true;
            }
        }
    }
    return false;
}

bool aabbOverlap(const TriangleState& first, const TriangleState& second, double epsilon) {
    const Vec3 firstMin = first.points[0].cwiseMin(first.points[1]).cwiseMin(first.points[2]);
    const Vec3 firstMax = first.points[0].cwiseMax(first.points[1]).cwiseMax(first.points[2]);
    const Vec3 secondMin = second.points[0].cwiseMin(second.points[1]).cwiseMin(second.points[2]);
    const Vec3 secondMax = second.points[0].cwiseMax(second.points[1]).cwiseMax(second.points[2]);
    for (int axis = 0; axis < 3; ++axis) {
        if (firstMax[axis] + epsilon < secondMin[axis] || secondMax[axis] + epsilon < firstMin[axis]) {
            return false;
        }
    }
    return true;
}

bool segmentIntersectsTriangle(
    const Vec3& start, const Vec3& end, const std::array<Vec3, 3>& triangle, double barycentricEpsilon
) {
    const Vec3 direction = end - start;
    const Vec3 edgeA = triangle[1] - triangle[0];
    const Vec3 edgeB = triangle[2] - triangle[0];
    const Vec3 p = direction.cross(edgeB);
    const double determinant = edgeA.dot(p);
    const double scale = std::max(1.0, edgeA.norm() * p.norm());
    if (!std::isfinite(determinant) || std::abs(determinant) <= barycentricEpsilon * scale) {
        return false;
    }
    const double inverseDeterminant = 1.0 / determinant;
    const Vec3 offset = start - triangle[0];
    const double u = offset.dot(p) * inverseDeterminant;
    if (u < -barycentricEpsilon || u > 1.0 + barycentricEpsilon) {
        return false;
    }
    const Vec3 q = offset.cross(edgeA);
    const double v = direction.dot(q) * inverseDeterminant;
    if (v < -barycentricEpsilon || u + v > 1.0 + barycentricEpsilon) {
        return false;
    }
    const double t = edgeB.dot(q) * inverseDeterminant;
    return t >= -barycentricEpsilon && t <= 1.0 + barycentricEpsilon;
}

struct Point2 {
    double x = 0.0;
    double y = 0.0;
};

Point2 projectPoint(const Vec3& point, int droppedAxis) {
    if (droppedAxis == 0) {
        return Point2{point.y(), point.z()};
    }
    if (droppedAxis == 1) {
        return Point2{point.x(), point.z()};
    }
    return Point2{point.x(), point.y()};
}

double orientation2d(const Point2& first, const Point2& second, const Point2& third) {
    return (second.x - first.x) * (third.y - first.y) - (second.y - first.y) * (third.x - first.x);
}

bool pointOnSegment2d(const Point2& point, const Point2& first, const Point2& second, double epsilon) {
    return std::abs(orientation2d(first, second, point)) <= epsilon &&
           point.x >= std::min(first.x, second.x) - epsilon && point.x <= std::max(first.x, second.x) + epsilon &&
           point.y >= std::min(first.y, second.y) - epsilon && point.y <= std::max(first.y, second.y) + epsilon;
}

bool segmentsIntersect2d(const Point2& a, const Point2& b, const Point2& c, const Point2& d, double epsilon) {
    const double abC = orientation2d(a, b, c);
    const double abD = orientation2d(a, b, d);
    const double cdA = orientation2d(c, d, a);
    const double cdB = orientation2d(c, d, b);
    if (((abC > epsilon && abD < -epsilon) || (abC < -epsilon && abD > epsilon)) &&
        ((cdA > epsilon && cdB < -epsilon) || (cdA < -epsilon && cdB > epsilon))) {
        return true;
    }
    return pointOnSegment2d(c, a, b, epsilon) || pointOnSegment2d(d, a, b, epsilon) ||
           pointOnSegment2d(a, c, d, epsilon) || pointOnSegment2d(b, c, d, epsilon);
}

bool pointInTriangle2d(const Point2& point, const std::array<Point2, 3>& triangle, double epsilon) {
    const double first = orientation2d(triangle[0], triangle[1], point);
    const double second = orientation2d(triangle[1], triangle[2], point);
    const double third = orientation2d(triangle[2], triangle[0], point);
    return (first >= -epsilon && second >= -epsilon && third >= -epsilon) ||
           (first <= epsilon && second <= epsilon && third <= epsilon);
}

bool coplanarTrianglesIntersect(const TriangleState& first, const TriangleState& second, const Vec3& normal, double epsilon) {
    int droppedAxis = 0;
    if (std::abs(normal.y()) > std::abs(normal.x())) {
        droppedAxis = 1;
    }
    if (std::abs(normal.z()) > std::abs(normal[droppedAxis])) {
        droppedAxis = 2;
    }
    std::array<Point2, 3> firstProjected{};
    std::array<Point2, 3> secondProjected{};
    for (int point = 0; point < 3; ++point) {
        firstProjected[static_cast<std::size_t>(point)] = projectPoint(first.points[static_cast<std::size_t>(point)], droppedAxis);
        secondProjected[static_cast<std::size_t>(point)] =
            projectPoint(second.points[static_cast<std::size_t>(point)], droppedAxis);
    }
    for (int firstEdge = 0; firstEdge < 3; ++firstEdge) {
        for (int secondEdge = 0; secondEdge < 3; ++secondEdge) {
            if (segmentsIntersect2d(
                    firstProjected[static_cast<std::size_t>(firstEdge)],
                    firstProjected[static_cast<std::size_t>((firstEdge + 1) % 3)],
                    secondProjected[static_cast<std::size_t>(secondEdge)],
                    secondProjected[static_cast<std::size_t>((secondEdge + 1) % 3)],
                    epsilon
                )) {
                return true;
            }
        }
    }
    return pointInTriangle2d(firstProjected[0], secondProjected, epsilon) ||
           pointInTriangle2d(secondProjected[0], firstProjected, epsilon);
}

bool trianglesIntersect(const TriangleState& first, const TriangleState& second, double geometryEpsilon) {
    if (!aabbOverlap(first, second, geometryEpsilon)) {
        return false;
    }
    const Vec3 firstNormal = (first.points[1] - first.points[0]).cross(first.points[2] - first.points[0]);
    const Vec3 secondNormal = (second.points[1] - second.points[0]).cross(second.points[2] - second.points[0]);
    const double firstLength = firstNormal.norm();
    const double secondLength = secondNormal.norm();
    if (firstLength <= geometryEpsilon || secondLength <= geometryEpsilon || !std::isfinite(firstLength) ||
        !std::isfinite(secondLength)) {
        return false;
    }
    const Vec3 firstUnit = firstNormal / firstLength;
    const Vec3 secondUnit = secondNormal / secondLength;
    double maxPlaneDistance = 0.0;
    for (const Vec3& point : second.points) {
        maxPlaneDistance = std::max(maxPlaneDistance, std::abs((point - first.points[0]).dot(firstUnit)));
    }
    if (firstUnit.cross(secondUnit).norm() <= 1e-8 && maxPlaneDistance <= geometryEpsilon) {
        return coplanarTrianglesIntersect(first, second, firstUnit, geometryEpsilon);
    }
    constexpr double kBarycentricEpsilon = 1e-10;
    for (int edge = 0; edge < 3; ++edge) {
        if (segmentIntersectsTriangle(
                first.points[static_cast<std::size_t>(edge)],
                first.points[static_cast<std::size_t>((edge + 1) % 3)],
                second.points,
                kBarycentricEpsilon
            ) ||
            segmentIntersectsTriangle(
                second.points[static_cast<std::size_t>(edge)],
                second.points[static_cast<std::size_t>((edge + 1) % 3)],
                first.points,
                kBarycentricEpsilon
            )) {
            return true;
        }
    }
    return false;
}

bool causesLocalSelfIntersection(
    const SimplificationState& state,
    const std::vector<int>& affectedFaces,
    const std::vector<TriangleState>& replacements,
    double geometryEpsilon
) {
    std::set<int> localVertices;
    for (int faceId : affectedFaces) {
        const FaceState& face = state.faces[static_cast<std::size_t>(faceId)];
        if (face.active) {
            localVertices.insert(face.vertices.begin(), face.vertices.end());
        }
    }
    std::set<int> localFaces;
    for (int vertex : localVertices) {
        if (!validVertex(state, vertex)) {
            continue;
        }
        localFaces.insert(
            state.vertexFaces[static_cast<std::size_t>(vertex)].begin(),
            state.vertexFaces[static_cast<std::size_t>(vertex)].end()
        );
    }

    for (std::size_t first = 0; first < replacements.size(); ++first) {
        for (std::size_t second = first + 1; second < replacements.size(); ++second) {
            if (!sharesVertex(replacements[first], replacements[second]) &&
                trianglesIntersect(replacements[first], replacements[second], geometryEpsilon)) {
                return true;
            }
        }
        for (int faceId : localFaces) {
            if (containsFace(affectedFaces, faceId) || faceId < 0 || faceId >= static_cast<int>(state.faces.size())) {
                continue;
            }
            const FaceState& face = state.faces[static_cast<std::size_t>(faceId)];
            if (!face.active) {
                continue;
            }
            TriangleState other;
            other.ids = face.vertices;
            for (int corner = 0; corner < 3; ++corner) {
                other.points[static_cast<std::size_t>(corner)] =
                    state.vertices[static_cast<std::size_t>(face.vertices[static_cast<std::size_t>(corner)])].position;
            }
            if (!sharesVertex(replacements[first], other) && trianglesIntersect(replacements[first], other, geometryEpsilon)) {
                return true;
            }
        }
    }
    return false;
}

struct CollapseEvaluation {
    RejectKind reject = RejectKind::Topology;
    int placement = -1;
};

CollapseEvaluation evaluateCandidate(
    const SimplificationState& state,
    const Candidate& candidate,
    const SimplifyOptions& options,
    double meshDiagonal,
    double areaEpsilon
) {
    const Edge edge{candidate.keep, candidate.remove};
    const std::vector<int> edgeFaces = commonFaces(state, edge.first, edge.second);
    if (!preservesLinkCondition(state, edge, edgeFaces)) {
        return CollapseEvaluation{RejectKind::Topology, -1};
    }
    if (!preservesBoundaryTopology(state, edge, edgeFaces, options.preserveBoundary)) {
        return CollapseEvaluation{RejectKind::Constraint, -1};
    }
    if (options.preserveFeatures &&
        (state.protectedFeatureVertices[static_cast<std::size_t>(edge.first)] != 0 ||
         state.protectedFeatureVertices[static_cast<std::size_t>(edge.second)] != 0)) {
        return CollapseEvaluation{RejectKind::Constraint, -1};
    }
    const std::vector<int> affectedFaces = touchedFaces(state, edge.first, edge.second);
    RejectKind strongestReject = RejectKind::Topology;
    for (int placement = 0; placement < candidate.placementCount; ++placement) {
        const Vec3& position = candidate.placements[static_cast<std::size_t>(placement)].position;
        if (options.maxNormalizedError > 0.0) {
            const Vec3& first = state.vertices[static_cast<std::size_t>(edge.first)].position;
            const Vec3& second = state.vertices[static_cast<std::size_t>(edge.second)].position;
            const double normalizedError = std::max((position - first).norm(), (position - second).norm()) / meshDiagonal;
            if (!std::isfinite(normalizedError) || normalizedError > options.maxNormalizedError) {
                strongestReject = RejectKind::Error;
                continue;
            }
        }

        std::vector<TriangleState> replacements;
        const RejectKind geometry = buildReplacementTriangles(
            state,
            edge,
            position,
            affectedFaces,
            areaEpsilon,
            options.minTriangleQuality,
            replacements
        );
        if (geometry != RejectKind::None) {
            strongestReject = geometry;
            continue;
        }
        if (options.preventLocalIntersections &&
            causesLocalSelfIntersection(
                state,
                affectedFaces,
                replacements,
                std::max(1e-12, meshDiagonal * kIntersectionRelativeEpsilon)
            )) {
            strongestReject = RejectKind::Intersection;
            continue;
        }
        return CollapseEvaluation{RejectKind::None, placement};
    }
    return CollapseEvaluation{strongestReject, -1};
}

void recordRejection(SimplifyReport& report, RejectKind reason) {
    ++report.rejectedCandidates;
    switch (reason) {
    case RejectKind::Topology:
        ++report.topologyRejected;
        break;
    case RejectKind::Constraint:
        ++report.constraintRejected;
        break;
    case RejectKind::Geometry:
        ++report.geometryRejected;
        break;
    case RejectKind::Error:
        ++report.errorRejected;
        break;
    case RejectKind::Intersection:
        ++report.intersectionRejected;
        break;
    case RejectKind::None:
        break;
    }
}

void addAffectedVertex(const SimplificationState& state, std::set<int>& affected, int vertex) {
    if (validVertex(state, vertex)) {
        affected.insert(vertex);
    }
}

void applyCollapse(
    SimplificationState& state, const Candidate& candidate, const Vec3& position, std::set<int>& affected
) {
    const int keep = candidate.keep;
    const int remove = candidate.remove;
    const std::vector<int> changedFaces(
        state.vertexFaces[static_cast<std::size_t>(remove)].begin(),
        state.vertexFaces[static_cast<std::size_t>(remove)].end()
    );
    for (int faceId : touchedFaces(state, keep, remove)) {
        const FaceState& face = state.faces[static_cast<std::size_t>(faceId)];
        for (int vertex : face.vertices) {
            addAffectedVertex(state, affected, vertex);
        }
    }

    state.vertices[static_cast<std::size_t>(keep)].position = position;
    state.vertices[static_cast<std::size_t>(keep)].quadric += state.vertices[static_cast<std::size_t>(remove)].quadric;

    for (int faceId : changedFaces) {
        FaceState& face = state.faces[static_cast<std::size_t>(faceId)];
        if (!face.active) {
            continue;
        }
        removeFaceFromTopology(state, faceId);
        const bool removesFace = containsVertex(face, keep);
        for (int& vertex : face.vertices) {
            if (vertex == remove) {
                vertex = keep;
            }
        }
        if (removesFace || face.vertices[0] == face.vertices[1] || face.vertices[1] == face.vertices[2] ||
            face.vertices[0] == face.vertices[2]) {
            face.active = false;
            --state.activeFaces;
            continue;
        }
        const auto duplicate = state.facesByKey.find(faceKey(face.vertices));
        if (duplicate != state.facesByKey.end() && !duplicate->second.empty()) {
            face.active = false;
            --state.activeFaces;
            continue;
        }
        addFaceToTopology(state, faceId);
    }

    state.vertices[static_cast<std::size_t>(remove)].active = false;
    ++state.vertices[static_cast<std::size_t>(remove)].version;
    state.vertexFaces[static_cast<std::size_t>(remove)].clear();
    addAffectedVertex(state, affected, keep);
    for (int faceId : state.vertexFaces[static_cast<std::size_t>(keep)]) {
        const FaceState& face = state.faces[static_cast<std::size_t>(faceId)];
        for (int vertex : face.vertices) {
            addAffectedVertex(state, affected, vertex);
        }
    }
    for (int vertex : affected) {
        ++state.vertices[static_cast<std::size_t>(vertex)].version;
    }
}

void enqueueEdges(
    const SimplificationState& state, const std::set<Edge>& edges, std::priority_queue<Candidate>& queue
) {
    for (const Edge& edge : edges) {
        Candidate candidate;
        if (makeCandidate(state, edge, candidate)) {
            queue.push(candidate);
        }
    }
}

void enqueueAffectedEdges(
    const SimplificationState& state, const std::set<int>& affected, std::priority_queue<Candidate>& queue
) {
    std::set<Edge> edges;
    for (int vertex : affected) {
        if (!validVertex(state, vertex)) {
            continue;
        }
        const std::set<int> neighbors = activeNeighbors(state, vertex);
        for (int neighbor : neighbors) {
            edges.insert(sortedEdge(vertex, neighbor));
        }
    }
    enqueueEdges(state, edges, queue);
}

std::string exhaustedStopReason(const SimplifyReport& report) {
    if (report.rejectedCandidates == 0U) {
        return "no-candidates";
    }
    if (report.errorRejected > 0U && report.topologyRejected == 0U && report.constraintRejected == 0U &&
        report.geometryRejected == 0U && report.intersectionRejected == 0U) {
        return "error-limit";
    }
    if (report.intersectionRejected > 0U && report.topologyRejected == 0U && report.constraintRejected == 0U &&
        report.geometryRejected == 0U && report.errorRejected == 0U) {
        return "self-intersection-blocked";
    }
    if (report.constraintRejected > 0U && report.topologyRejected == 0U && report.geometryRejected == 0U &&
        report.errorRejected == 0U && report.intersectionRejected == 0U) {
        return "constraints-blocked";
    }
    if (report.topologyRejected > 0U && report.constraintRejected == 0U && report.geometryRejected == 0U &&
        report.errorRejected == 0U && report.intersectionRejected == 0U) {
        return "topology-blocked";
    }
    if (report.geometryRejected > 0U && report.topologyRejected == 0U && report.constraintRejected == 0U &&
        report.errorRejected == 0U && report.intersectionRejected == 0U) {
        return "geometry-blocked";
    }
    return "candidate-exhausted";
}

Mesh compactMesh(const SimplificationState& state) {
    Mesh result;
    std::vector<int> remap(state.vertices.size(), -1);
    result.faces.reserve(state.activeFaces);
    for (const FaceState& face : state.faces) {
        if (!face.active) {
            continue;
        }
        Face outputFace;
        bool valid = true;
        for (int corner = 0; corner < 3; ++corner) {
            const int oldVertex = face.vertices[static_cast<std::size_t>(corner)];
            if (!validVertex(state, oldVertex)) {
                valid = false;
                break;
            }
            int& newVertex = remap[static_cast<std::size_t>(oldVertex)];
            if (newVertex < 0) {
                newVertex = static_cast<int>(result.vertices.size());
                result.vertices.push_back(state.vertices[static_cast<std::size_t>(oldVertex)].position);
            }
            outputFace.v[static_cast<std::size_t>(corner)] = newVertex;
        }
        if (valid) {
            result.faces.push_back(outputFace);
        }
    }
    return result;
}

std::size_t resolvedTargetFaces(std::size_t initialFaces, const SimplifyOptions& options) {
    if (options.targetFaces > 0) {
        return std::min(initialFaces, static_cast<std::size_t>(options.targetFaces));
    }
    return static_cast<std::size_t>(std::floor(static_cast<double>(initialFaces) * options.ratio));
}

void validateOptions(const SimplifyOptions& options) {
    if (!std::isfinite(options.ratio) || options.ratio <= 0.0 || options.ratio > 1.0 || options.targetFaces < 0) {
        throw std::invalid_argument("ratio must be in (0, 1] and target faces must not be negative");
    }
    if (!std::isfinite(options.featureAngleDegrees) || options.featureAngleDegrees <= 0.0 ||
        options.featureAngleDegrees >= 180.0) {
        throw std::invalid_argument("feature angle must be in (0, 180) degrees");
    }
    if (!std::isfinite(options.boundaryConstraintWeight) || options.boundaryConstraintWeight < 0.0 ||
        !std::isfinite(options.featureConstraintWeight) || options.featureConstraintWeight < 0.0) {
        throw std::invalid_argument("constraint weights must be finite and non-negative");
    }
    if (!std::isfinite(options.minTriangleQuality) || options.minTriangleQuality < 0.0 ||
        options.minTriangleQuality > 1.0) {
        throw std::invalid_argument("minimum triangle quality must be in [0, 1]");
    }
    if (!std::isfinite(options.maxNormalizedError) || options.maxNormalizedError < 0.0) {
        throw std::invalid_argument("maximum normalized error must be finite and non-negative");
    }
}

} // namespace

Mesh simplifyQem(const Mesh& input, const SimplifyOptions& options, SimplifyReport* report) {
    validateOptions(options);
    if (input.hasTextureCoordinates()) {
        throw std::invalid_argument("MeshCore simplification does not preserve OBJ texture coordinates");
    }

    std::string error;
    if (!validateMeshGeometry(input, &error)) {
        throw std::invalid_argument("cannot simplify mesh: " + error);
    }

    SimplifyReport local;
    local.initialFaces = input.faces.size();
    local.targetFaces = resolvedTargetFaces(input.faces.size(), options);
    local.finalFaces = input.faces.size();
    if (input.faces.size() <= local.targetFaces) {
        local.stopReason = "already-at-target";
        if (report != nullptr) {
            *report = local;
        }
        return input;
    }

    FeatureOptions featureOptions;
    featureOptions.dihedralAngleDegrees = options.featureAngleDegrees;
    const FeatureReport features = detectFeatures(input, featureOptions);
    if (features.nonManifoldEdges != 0U) {
        throw std::invalid_argument("MeshCore simplification does not accept meshes with non-manifold edges");
    }

    SimplificationState state = makeState(input);
    state.protectedFeatureVertices = features.featureVertices;
    buildQuadrics(state, features, options);
    const double meshDiagonal = std::max(1e-12, input.bboxDiag());
    const double areaEpsilon = std::max(1e-24, meshDiagonal * meshDiagonal * kRelativeAreaEpsilon);

    std::priority_queue<Candidate> queue;
    enqueueEdges(state, collectAllEdges(state), queue);
    std::size_t staleSinceRebuild = 0;
    bool usedExhaustionRebuild = false;

    while (state.activeFaces > local.targetFaces) {
        if (queue.empty()) {
            if (!usedExhaustionRebuild && staleSinceRebuild > 0U) {
                queue = std::priority_queue<Candidate>();
                enqueueEdges(state, collectAllEdges(state), queue);
                ++local.queueRebuilds;
                staleSinceRebuild = 0;
                usedExhaustionRebuild = true;
                continue;
            }
            local.stopReason = exhaustedStopReason(local);
            break;
        }

        const Candidate candidate = queue.top();
        queue.pop();
        if (!candidateIsCurrent(state, candidate)) {
            ++local.staleCandidates;
            ++staleSinceRebuild;
            const std::size_t staleLimit = std::max<std::size_t>(1024U, state.activeFaces * 8U);
            if (staleSinceRebuild > staleLimit) {
                queue = std::priority_queue<Candidate>();
                enqueueEdges(state, collectAllEdges(state), queue);
                ++local.queueRebuilds;
                staleSinceRebuild = 0;
            }
            continue;
        }

        const CollapseEvaluation evaluation = evaluateCandidate(state, candidate, options, meshDiagonal, areaEpsilon);
        if (evaluation.reject != RejectKind::None) {
            recordRejection(local, evaluation.reject);
            continue;
        }

        std::set<int> affected;
        applyCollapse(
            state,
            candidate,
            candidate.placements[static_cast<std::size_t>(evaluation.placement)].position,
            affected
        );
        ++local.collapses;
        staleSinceRebuild = 0;
        usedExhaustionRebuild = false;
        enqueueAffectedEdges(state, affected, queue);
    }

    Mesh output = compactMesh(state);
    local.finalFaces = output.faces.size();
    if (local.stopReason.empty()) {
        local.stopReason = "reached-target";
    }
    if (report != nullptr) {
        *report = local;
    }
    return output;
}

} // namespace meshcore
} // namespace manumesh
