#include "slim/simplification/SlimQem.h"

#include "slim/features/SlimFeatureDetection.h"

#include <Eigen/LU>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace manumesh {
namespace slim {
namespace {

struct Quadric {
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Zero();

    void addPlane(const Vec3& normal, const Vec3& point) {
        if (normal.squaredNorm() == 0.0) {
            return;
        }
        Eigen::Vector4d plane;
        plane << normal.x(), normal.y(), normal.z(), -normal.dot(point);
        matrix.noalias() += plane * plane.transpose();
    }

    double evaluate(const Vec3& point) const {
        const Eigen::Vector4d homogeneous(point.x(), point.y(), point.z(), 1.0);
        return homogeneous.dot(matrix * homogeneous);
    }

    Vec3 bestPoint(const Vec3& first, const Vec3& second) const {
        const Eigen::Matrix3d coefficients = matrix.topLeftCorner<3, 3>();
        const Eigen::Vector3d rightHandSide = -matrix.topRightCorner<3, 1>();
        Eigen::FullPivLU<Eigen::Matrix3d> solver(coefficients);
        if (solver.isInvertible()) {
            const Vec3 solution = solver.solve(rightHandSide);
            if (solution.allFinite()) {
                return solution;
            }
        }

        const Vec3 midpoint = 0.5 * (first + second);
        Vec3 best = midpoint;
        double bestCost = evaluate(best);
        for (const Vec3* candidate : {&first, &second}) {
            const double cost = evaluate(*candidate);
            if (cost < bestCost) {
                best = *candidate;
                bestCost = cost;
            }
        }
        return best;
    }
};

struct Candidate {
    int keep = -1;
    int remove = -1;
    Vec3 position = Vec3::Zero();
    double cost = std::numeric_limits<double>::infinity();
};

using EdgeFaces = std::map<MeshEdge, std::vector<int>>;

MeshEdge sortedEdge(int first, int second) {
    return first < second ? MeshEdge{first, second} : MeshEdge{second, first};
}

EdgeFaces collectEdges(const Mesh& mesh) {
    EdgeFaces result;
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        const Face& face = mesh.faces[faceIndex];
        for (int corner = 0; corner < 3; ++corner) {
            const int first = face.v[static_cast<std::size_t>(corner)];
            const int second = face.v[static_cast<std::size_t>((corner + 1) % 3)];
            result[sortedEdge(first, second)].push_back(static_cast<int>(faceIndex));
        }
    }
    return result;
}

std::vector<Quadric> buildQuadrics(const Mesh& mesh) {
    std::vector<Quadric> result(mesh.vertices.size());
    for (const Face& face : mesh.faces) {
        const Vec3& first = mesh.vertices[static_cast<std::size_t>(face.v[0])];
        const Vec3& second = mesh.vertices[static_cast<std::size_t>(face.v[1])];
        const Vec3& third = mesh.vertices[static_cast<std::size_t>(face.v[2])];
        const Vec3 normal = triangleNormal(first, second, third);
        for (int vertex : face.v) {
            result[static_cast<std::size_t>(vertex)].addPlane(normal, first);
        }
    }
    return result;
}

bool containsVertex(const Face& face, int vertex) {
    return face.v[0] == vertex || face.v[1] == vertex || face.v[2] == vertex;
}

bool passesLinkCondition(const Mesh& mesh, int first, int second) {
    std::set<int> firstNeighbors;
    std::set<int> secondNeighbors;
    for (const Face& face : mesh.faces) {
        if (containsVertex(face, first)) {
            for (int vertex : face.v) {
                if (vertex != first) {
                    firstNeighbors.insert(vertex);
                }
            }
        }
        if (containsVertex(face, second)) {
            for (int vertex : face.v) {
                if (vertex != second) {
                    secondNeighbors.insert(vertex);
                }
            }
        }
    }

    std::size_t commonNeighbors = 0;
    for (int vertex : firstNeighbors) {
        if (vertex != first && vertex != second && secondNeighbors.count(vertex) != 0U) {
            ++commonNeighbors;
        }
    }
    return commonNeighbors == 2U;
}

bool preservesFaceOrientation(const Mesh& mesh, int keep, int remove, const Vec3& position) {
    for (const Face& face : mesh.faces) {
        const bool usesKeep = containsVertex(face, keep);
        const bool usesRemove = containsVertex(face, remove);
        if (!usesKeep && !usesRemove) {
            continue;
        }
        if (usesKeep && usesRemove) {
            continue;
        }

        Vec3 before[3];
        Vec3 after[3];
        for (int corner = 0; corner < 3; ++corner) {
            const int vertex = face.v[static_cast<std::size_t>(corner)];
            before[corner] = mesh.vertices[static_cast<std::size_t>(vertex)];
            after[corner] = vertex == keep || vertex == remove ? position : before[corner];
        }
        const Vec3 beforeNormal = (before[1] - before[0]).cross(before[2] - before[0]);
        const Vec3 afterNormal = (after[1] - after[0]).cross(after[2] - after[0]);
        const double beforeLength = beforeNormal.norm();
        const double afterLength = afterNormal.norm();
        if (!std::isfinite(afterLength) || afterLength <= beforeLength * 1e-8 ||
            beforeNormal.dot(afterNormal) <= 0.0) {
            return false;
        }
    }
    return true;
}

struct FaceKey {
    std::array<int, 3> vertices{};

    bool operator<(const FaceKey& other) const { return vertices < other.vertices; }
};

FaceKey keyForFace(const Face& face) {
    FaceKey result{{face.v[0], face.v[1], face.v[2]}};
    std::sort(result.vertices.begin(), result.vertices.end());
    return result;
}

void applyCollapse(Mesh& mesh, int keep, int remove, const Vec3& position) {
    std::set<FaceKey> uniqueFaces;
    std::vector<Face> updated;
    updated.reserve(mesh.faces.size());
    for (Face face : mesh.faces) {
        for (int& vertex : face.v) {
            if (vertex == remove) {
                vertex = keep;
            }
        }
        if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
            continue;
        }
        if (uniqueFaces.insert(keyForFace(face)).second) {
            updated.push_back(face);
        }
    }
    mesh.vertices[static_cast<std::size_t>(keep)] = position;
    mesh.faces.swap(updated);
}

std::size_t resolvedTargetFaces(const Mesh& mesh, const SimplifyOptions& options) {
    if (options.targetFaces > 0) {
        return std::min(mesh.faces.size(), static_cast<std::size_t>(options.targetFaces));
    }
    return static_cast<std::size_t>(std::floor(static_cast<double>(mesh.faces.size()) * options.ratio));
}

} // namespace

Mesh simplifyQem(const Mesh& input, const SimplifyOptions& options, SimplifyReport* report) {
    if (!std::isfinite(options.ratio) || options.ratio <= 0.0 || options.ratio > 1.0 || options.targetFaces < 0) {
        throw std::invalid_argument("ratio must be in (0, 1] and target faces must not be negative");
    }
    if (!std::isfinite(options.featureAngleDegrees) || options.featureAngleDegrees <= 0.0 ||
        options.featureAngleDegrees >= 180.0) {
        throw std::invalid_argument("feature angle must be in (0, 180) degrees");
    }
    if (input.hasTextureCoordinates()) {
        throw std::invalid_argument("slim simplification does not preserve OBJ texture coordinates");
    }

    std::string error;
    if (!validateMeshGeometry(input, &error)) {
        throw std::invalid_argument("cannot simplify mesh: " + error);
    }

    Mesh output = input;
    FeatureOptions featureOptions;
    featureOptions.dihedralAngleDegrees = options.featureAngleDegrees;
    if (detectFeatures(output, featureOptions).nonManifoldEdges != 0U) {
        throw std::invalid_argument("slim simplification does not accept non-manifold meshes");
    }

    SimplifyReport local;
    local.initialFaces = output.faces.size();
    local.targetFaces = resolvedTargetFaces(output, options);
    local.finalFaces = output.faces.size();
    if (output.faces.size() <= local.targetFaces) {
        local.stopReason = "already-at-target";
        if (report) {
            *report = local;
        }
        return output;
    }

    std::vector<Quadric> quadrics = buildQuadrics(output);

    while (output.faces.size() > local.targetFaces) {
        const FeatureReport features = detectFeatures(output, featureOptions);
        if (features.nonManifoldEdges != 0U) {
            local.stopReason = "non-manifold-input";
            break;
        }

        const EdgeFaces edges = collectEdges(output);
        Candidate best;
        for (const auto& entry : edges) {
            const MeshEdge& edge = entry.first;
            if (entry.second.size() != 2U) {
                continue;
            }
            const int keep = edge.first;
            const int remove = edge.second;
            if (options.preserveBoundary &&
                (features.boundaryVertices[static_cast<std::size_t>(keep)] != 0 ||
                 features.boundaryVertices[static_cast<std::size_t>(remove)] != 0)) {
                continue;
            }
            if (options.preserveFeatures &&
                (features.featureVertices[static_cast<std::size_t>(keep)] != 0 ||
                 features.featureVertices[static_cast<std::size_t>(remove)] != 0)) {
                continue;
            }
            if (!passesLinkCondition(output, keep, remove)) {
                ++local.rejectedCandidates;
                continue;
            }

            Quadric combined;
            combined.matrix = quadrics[static_cast<std::size_t>(keep)].matrix +
                              quadrics[static_cast<std::size_t>(remove)].matrix;
            const Vec3 position = combined.bestPoint(
                output.vertices[static_cast<std::size_t>(keep)], output.vertices[static_cast<std::size_t>(remove)]
            );
            const double cost = combined.evaluate(position);
            if (!std::isfinite(cost) || !preservesFaceOrientation(output, keep, remove, position)) {
                ++local.rejectedCandidates;
                continue;
            }
            if (cost < best.cost) {
                best.keep = keep;
                best.remove = remove;
                best.position = position;
                best.cost = cost;
            }
        }

        if (best.keep < 0) {
            local.stopReason = "constraints-blocked";
            break;
        }
        quadrics[static_cast<std::size_t>(best.keep)].matrix +=
            quadrics[static_cast<std::size_t>(best.remove)].matrix;
        applyCollapse(output, best.keep, best.remove, best.position);
        ++local.collapses;
    }

    output.removeUnusedVertices();
    local.finalFaces = output.faces.size();
    if (local.stopReason.empty()) {
        local.stopReason = "reached-target";
    }
    if (report) {
        *report = local;
    }
    return output;
}

} // namespace slim
} // namespace manumesh
