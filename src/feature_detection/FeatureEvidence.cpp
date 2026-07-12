#include "detail/FeatureEvidence.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "common/detail/MeshQueries.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace manumesh::feature::detector_detail {
namespace {

/// Returns +1 when the face traverses the directed edge a->b, -1 for b->a,
/// and 0 when the face does not contain the edge at all.
int faceEdgeDirection(const Face& face, int a, int b) {
    for (int i = 0; i < 3; ++i) {
        const int u = face.v[i];
        const int v = face.v[(i + 1) % 3];
        if (u == a && v == b) {
            return 1;
        }
        if (u == b && v == a) {
            return -1;
        }
    }
    return 0;
}

/// Detection-internal winding-harmonization marks: flip[f] == 1 means face f
/// is interpreted with reversed vertex order (face normal negated, edge
/// traversal reversed) for all dihedral purposes. The input mesh itself is
/// never modified.
///
/// Built with one deterministic BFS over face adjacency, O(F) total: every
/// face pushes each of its three edges exactly once and every interior edge
/// stores at most two faces. Determinism: components are seeded in ascending
/// face-index order, each face expands its edges in vertex order, and
/// buildMeshEdgeInfo stores the two incident faces of an edge in ascending
/// face-index order, so the flip assignment never depends on hash-map
/// iteration order.
///
/// Orientation is propagated only across manifold interior edges (exactly
/// two incident faces); boundary and non-manifold edges carry no winding
/// relation. Interior edges whose two faces still traverse the edge in the
/// same direction after propagation are unresolvable orientation conflicts
/// (non-orientable neighbourhoods, e.g. a Moebius band closure); they are
/// detected per edge by orientedDihedralAngle, which falls back to the
/// unsigned normal angle and keeps the inconsistentWindingEdges diagnostic.
///
/// After a component is traversed, marks are normalized so the majority of
/// its faces keeps the input orientation (ties keep the seed face's
/// orientation). A consistently wound mesh therefore gets the identity
/// marking, and reversing a minority patch of faces reproduces the original
/// orientation - and hence the original feature classification - exactly.
std::vector<char> harmonizeFaceWindings(const Mesh& mesh, const manumesh::common::MeshEdgeInfoMap& edges) {
    const int faceCount = static_cast<int>(mesh.faces.size());
    std::vector<char> flip(static_cast<std::size_t>(faceCount), 0);
    std::vector<char> visited(static_cast<std::size_t>(faceCount), 0);
    std::vector<int> queue;
    for (int seed = 0; seed < faceCount; ++seed) {
        if (visited[seed]) {
            continue;
        }
        visited[seed] = 1;
        queue.clear();
        queue.push_back(seed);
        for (std::size_t head = 0; head < queue.size(); ++head) {
            const int f = queue[head];
            const Face& face = mesh.faces[f];
            const int signF = flip[f] ? -1 : 1;
            for (int e = 0; e < 3; ++e) {
                const int a = face.v[e];
                const int b = face.v[(e + 1) % 3];
                const auto it = edges.find(manumesh::common::meshEdgeKey(a, b));
                if (it == edges.end() || it->second.faces.size() != 2) {
                    continue;
                }
                const int g = it->second.faces[0] == f ? it->second.faces[1] : it->second.faces[0];
                if (g == f || visited[g]) {
                    continue;
                }
                const int dirF = faceEdgeDirection(face, a, b);
                const int dirG = faceEdgeDirection(mesh.faces[g], a, b);
                if (dirF == 0 || dirG == 0) {
                    continue;
                }
                // Consistent winding requires the two faces to traverse the
                // shared edge in opposite directions after flips:
                //   dirF * signF == -(dirG * signG)  =>  signG = -dirF * dirG * signF.
                visited[g] = 1;
                flip[g] = (-dirF * dirG * signF) < 0 ? 1 : 0;
                queue.push_back(g);
            }
        }
        // Majority normalization: flipping every mark in a component keeps
        // all pairwise winding relations, so pick the assignment that keeps
        // most faces in their input orientation.
        int flippedCount = 0;
        for (int f : queue) {
            flippedCount += flip[f] ? 1 : 0;
        }
        if (2 * flippedCount > static_cast<int>(queue.size())) {
            for (int f : queue) {
                flip[f] = flip[f] ? 0 : 1;
            }
        }
    }
    return flip;
}

struct DihedralAngle {
    double angleRad = 0.0;
    bool inconsistentWinding = false;
    /// +1 convex ridge, -1 concave valley, 0 flat/unknown.
    int signedKind = 0;
};

/// Computes the turning angle and convexity across a two-face edge from the
/// winding-harmonized face normals.
///
/// Angle: with harmonized windings the plain normal dot product
/// distinguishes shallow creases from reflex knife edges (theta > 90
/// degrees) even when a patch of the input mesh was wound backwards. Edges
/// whose faces still traverse the edge in the same direction after
/// harmonization (non-orientable neighbourhoods) fall back to the unsigned
/// angle, which can only under-report sharpness.
///
/// Convexity (Jiao 2008 style, using the face's own traversal direction):
/// let d be the unit direction in which harmonized face 0 traverses the
/// shared edge, and n0, n1 the harmonized unit normals. Then
///
///     kind = sign((n0 x n1) . d),   + convex, - concave.
///
/// Derivation with two hand-checked 90-degree cases (edge along +y, face 0
/// horizontal in z = 0 covering x <= 0 with n0 = +z and d = +y; both cases
/// have consistent winding, i.e. face 1 traverses the edge along -y):
///  - Convex ridge (table edge): face 1 hangs straight down (x = 0,
///    z <= 0), outward normal n1 = +x. (n0 x n1) . d = (z x x) . y =
///    y . y = +1 > 0  => convex.
///  - Concave valley (room corner): face 1 rises straight up (x = 0,
///    z >= 0), outward normal n1 = -x (pointing back over the floor).
///    (n0 x n1) . d = (z x -x) . y = -y . y = -1 < 0  => concave.
/// Swapping the roles of the faces flips both d and the cross product, so
/// the sign is independent of which incident face is "face 0". The triple
/// product of three unit vectors is dimensionless and its magnitude is
/// sin(theta) at a clean crease, so a fixed 1e-12 cutoff only suppresses
/// numerically flat or degenerate configurations.
DihedralAngle orientedDihedralAngle(
    const Mesh& mesh,
    const std::vector<Vec3>& normals,
    const std::vector<char>& windingFlip,
    const manumesh::common::MeshEdgeInfo& info,
    int a,
    int b
) {
    DihedralAngle result;
    const int f0 = info.faces[0];
    const int f1 = info.faces[1];
    const int s0 = windingFlip[f0] ? -1 : 1;
    const int s1 = windingFlip[f1] ? -1 : 1;
    double dot = std::clamp(normals[f0].dot(normals[f1]), -1.0, 1.0) * static_cast<double>(s0 * s1);
    const int direction0 = faceEdgeDirection(mesh.faces[f0], a, b) * s0;
    const int direction1 = faceEdgeDirection(mesh.faces[f1], a, b) * s1;
    if (direction0 == 0 || direction1 == 0 || direction0 == direction1) {
        result.inconsistentWinding = true;
        dot = std::abs(dot);
    }
    result.angleRad = std::acos(dot);
    if (!result.inconsistentWinding) {
        Vec3 edge = mesh.vertices[b] - mesh.vertices[a];
        const double edgeLength = edge.norm();
        if (edgeLength > 1e-20) {
            const Vec3 d = edge * (static_cast<double>(direction0) / edgeLength);
            const double side =
                (normals[f0] * static_cast<double>(s0)).cross(normals[f1] * static_cast<double>(s1)).dot(d);
            if (side > 1e-12) {
                result.signedKind = 1;
            } else if (side < -1e-12) {
                result.signedKind = -1;
            }
        }
    }
    return result;
}

struct EdgeEvidenceContext {
    EdgeEvidenceContext(
        const Mesh& inputMesh,
        const FeatureOptions& inputOptions,
        FeatureDetectionCache& cache,
        FeatureAnalysis& outputAnalysis
    )
        : mesh(inputMesh),
          options(inputOptions),
          analysis(outputAnalysis),
          normals(cache.faceNormals()),
          edges(cache.edgeInfo()),
          windingFlip(harmonizeFaceWindings(inputMesh, cache.edgeInfo())),
          dihedralThreshold(options.featureAngleDeg * kPi / 180.0),
          tensor(
              options.useNormalTensorFeatures ? computeNormalTensorFeaturesCached(
                                                    mesh,
                                                    cache,
                                                    NormalTensorOptions{
                                                        options.normalTensorSmoothingIterations,
                                                        options.normalTensorScaleCount,
                                                    },
                                                    options.normalTensorFeatureThreshold
                                                )
                                              : std::vector<NormalTensorVertex>()
          ),
          curvature(
              options.useSmoothCurvatureFeatures ? computeSmoothCurvatureFeaturesCached(
                                                       mesh,
                                                       cache,
                                                       SmoothCurvatureOptions{
                                                           options.smoothCurvatureBaseNeighborhoodRings,
                                                           options.smoothCurvatureScaleCount,
                                                           options.smoothCurvatureRobustFitIterations,
                                                           options.smoothCurvatureMinTangentConsistency,
                                                       },
                                                       options.smoothCurvatureFeatureThreshold
                                                   )
                                                 : std::vector<SmoothCurvatureVertex>()
          ),
          discreteFeatureVertex(mesh.vertices.size(), 0) {
        summarizeNormalTensorVertices();
        summarizeSmoothCurvatureVertices();
        markDiscreteFeatureVertices();
    }

    /// Returns the cached oriented dihedral angle for a two-face edge.
    /// markDiscreteFeatureVertices computes every interior-edge angle exactly
    /// once; the dihedral evidence strategy reuses the same values.
    const DihedralAngle* dihedralAngle(std::uint64_t key) const {
        const auto it = dihedralAngles.find(key);
        return it == dihedralAngles.end() ? nullptr : &it->second;
    }

    const Mesh& mesh;
    const FeatureOptions& options;
    FeatureAnalysis& analysis;
    const std::vector<Vec3>& normals;
    const manumesh::common::MeshEdgeInfoMap& edges;
    std::vector<char> windingFlip;
    double dihedralThreshold = 0.0;
    std::vector<NormalTensorVertex> tensor;
    std::vector<SmoothCurvatureVertex> curvature;
    std::vector<char> discreteFeatureVertex;
    std::unordered_map<std::uint64_t, DihedralAngle> dihedralAngles;

private:
    void summarizeNormalTensorVertices() {
        if (tensor.empty()) {
            return;
        }

        double localScaleSum = 0.0;
        double persistenceSum = 0.0;
        for (const NormalTensorVertex& vertex : tensor) {
            analysis.maxNormalTensorFeatureScore = std::max(analysis.maxNormalTensorFeatureScore, vertex.featureScore);
            analysis.maxNormalTensorPersistentScore =
                std::max(analysis.maxNormalTensorPersistentScore, vertex.persistentFeatureScore);
            if (vertex.featureScore <= 1e-12 && vertex.persistentFeatureScore <= 1e-12) {
                continue;
            }
            ++analysis.normalTensorScoredVertices;
            localScaleSum += vertex.localScale;
            persistenceSum += static_cast<double>(vertex.persistentScales);
        }

        if (analysis.normalTensorScoredVertices > 0) {
            const double count = static_cast<double>(analysis.normalTensorScoredVertices);
            analysis.meanNormalTensorLocalScale = localScaleSum / count;
            analysis.meanNormalTensorPersistence = persistenceSum / count;
        }
    }

    void markDiscreteFeatureVertices() {
        dihedralAngles.reserve(edges.size());
        for (const auto& [key, info] : edges) {
            const auto [a, b] = manumesh::common::unpackMeshEdgeKey(key);
            bool discrete = false;
            if (info.faces.size() == 1 || info.faces.size() > 2) {
                discrete = true;
            } else if (info.faces.size() == 2) {
                const DihedralAngle dihedral = orientedDihedralAngle(mesh, normals, windingFlip, info, a, b);
                dihedralAngles.emplace(key, dihedral);
                discrete = dihedral.angleRad >= dihedralThreshold;
            }
            if (discrete) {
                discreteFeatureVertex[a] = 1;
                discreteFeatureVertex[b] = 1;
            }
        }
    }

    void summarizeSmoothCurvatureVertices() {
        if (curvature.empty()) {
            return;
        }

        double localScaleSum = 0.0;
        double persistenceSum = 0.0;
        for (const SmoothCurvatureVertex& vertex : curvature) {
            analysis.maxSmoothCurvatureFeatureScore =
                std::max(analysis.maxSmoothCurvatureFeatureScore, vertex.featureScore);
            analysis.maxSmoothCurvaturePersistentScore =
                std::max(analysis.maxSmoothCurvaturePersistentScore, vertex.persistentFeatureScore);
            if (vertex.featureScore <= 1e-12 && vertex.persistentFeatureScore <= 1e-12) {
                continue;
            }
            ++analysis.smoothCurvatureScoredVertices;
            localScaleSum += vertex.localScale;
            persistenceSum += static_cast<double>(vertex.persistentScales);
        }

        if (analysis.smoothCurvatureScoredVertices > 0) {
            const double count = static_cast<double>(analysis.smoothCurvatureScoredVertices);
            analysis.meanSmoothCurvatureLocalScale = localScaleSum / count;
            analysis.meanSmoothCurvaturePersistence = persistenceSum / count;
        }
    }
};

bool normalTensorEdgeCandidate(
    CandidateEdge& edge,
    const std::vector<NormalTensorVertex>& tensor,
    const std::vector<char>& discreteFeatureVertex,
    const Mesh& mesh,
    const FeatureOptions& options,
    FeatureAnalysis& analysis
) {
    if (!options.useNormalTensorFeatures || edge.a < 0 || edge.b < 0 || edge.a >= static_cast<int>(tensor.size()) ||
        edge.b >= static_cast<int>(tensor.size())) {
        return false;
    }
    if (edge.a < static_cast<int>(discreteFeatureVertex.size()) &&
        edge.b < static_cast<int>(discreteFeatureVertex.size()) &&
        (discreteFeatureVertex[edge.a] || discreteFeatureVertex[edge.b])) {
        return false;
    }

    const double score = 0.5 * (tensor[edge.a].featureScore + tensor[edge.b].featureScore);
    analysis.maxNormalTensorFeatureScore = std::max(analysis.maxNormalTensorFeatureScore, score);
    const double persistentScore =
        0.5 * (tensor[edge.a].persistentFeatureScore + tensor[edge.b].persistentFeatureScore);
    analysis.maxNormalTensorPersistentScore = std::max(analysis.maxNormalTensorPersistentScore, persistentScore);
    const int requiredPersistentScales =
        std::clamp(options.normalTensorMinPersistentScales, 1, std::max(1, options.normalTensorScaleCount));
    const int minPersistentScales = std::min(tensor[edge.a].persistentScales, tensor[edge.b].persistentScales);
    edge.tensorPersistentScore = persistentScore;
    edge.tensorPersistentScales = minPersistentScales;
    if (minPersistentScales < requiredPersistentScales) {
        return false;
    }
    const double minEndpointScore =
        std::min(tensor[edge.a].persistentFeatureScore, tensor[edge.b].persistentFeatureScore);
    if (minEndpointScore < options.normalTensorFeatureThreshold) {
        return false;
    }
    if (tensor[edge.a].creaseSaliency < tensor[edge.a].cornerSaliency ||
        tensor[edge.b].creaseSaliency < tensor[edge.b].cornerSaliency) {
        return false;
    }

    Vec3 direction = mesh.vertices[edge.b] - mesh.vertices[edge.a];
    const double length = direction.norm();
    if (length <= 1e-20) {
        return false;
    }
    direction /= length;
    const double alignA = std::abs(direction.dot(tensor[edge.a].creaseTangent));
    const double alignB = std::abs(direction.dot(tensor[edge.b].creaseTangent));
    return std::max(alignA, alignB) >= options.normalTensorMinEdgeAlignment;
}

bool smoothCurvatureEdgeCandidate(
    CandidateEdge& edge,
    const std::vector<SmoothCurvatureVertex>& curvature,
    const std::vector<char>& discreteFeatureVertex,
    const Mesh& mesh,
    const FeatureOptions& options,
    FeatureAnalysis& analysis
) {
    if (!options.useSmoothCurvatureFeatures || edge.a < 0 || edge.b < 0 ||
        edge.a >= static_cast<int>(curvature.size()) || edge.b >= static_cast<int>(curvature.size())) {
        return false;
    }
    if (edge.a < static_cast<int>(discreteFeatureVertex.size()) &&
        edge.b < static_cast<int>(discreteFeatureVertex.size()) &&
        (discreteFeatureVertex[edge.a] || discreteFeatureVertex[edge.b])) {
        return false;
    }

    const SmoothCurvatureVertex& a = curvature[edge.a];
    const SmoothCurvatureVertex& b = curvature[edge.b];
    analysis.maxSmoothCurvatureFeatureScore =
        std::max(analysis.maxSmoothCurvatureFeatureScore, 0.5 * (a.featureScore + b.featureScore));
    const double persistentScore = 0.5 * (a.persistentFeatureScore + b.persistentFeatureScore);
    analysis.maxSmoothCurvaturePersistentScore = std::max(analysis.maxSmoothCurvaturePersistentScore, persistentScore);
    const int requiredPersistentScales =
        std::clamp(options.smoothCurvatureMinPersistentScales, 1, std::max(1, options.smoothCurvatureScaleCount));
    const int minPersistentScales = std::min(a.persistentScales, b.persistentScales);
    edge.curvaturePersistentScore = persistentScore;
    edge.curvaturePersistentScales = minPersistentScales;
    if (minPersistentScales < requiredPersistentScales ||
        std::min(a.persistentFeatureScore, b.persistentFeatureScore) < options.smoothCurvatureFeatureThreshold ||
        a.signedKind == 0 || b.signedKind == 0 || a.signedKind != b.signedKind) {
        return false;
    }

    Vec3 direction = mesh.vertices[edge.b] - mesh.vertices[edge.a];
    if (direction.norm() <= 1e-20) {
        return false;
    }
    direction.normalize();
    const double alignA = std::abs(direction.dot(a.curveTangent));
    const double alignB = std::abs(direction.dot(b.curveTangent));
    const double tangentConsistency = std::abs(a.curveTangent.dot(b.curveTangent));
    if (std::min(alignA, alignB) < options.smoothCurvatureMinEdgeAlignment ||
        tangentConsistency < options.smoothCurvatureMinTangentConsistency) {
        return false;
    }
    edge.signedKind = a.signedKind;
    return true;
}

class EdgeEvidenceStrategy {
public:
    virtual ~EdgeEvidenceStrategy() = default;
    virtual void
    classify(CandidateEdge& edge, const manumesh::common::MeshEdgeInfo& info, EdgeEvidenceContext& context) const = 0;
};

class BoundaryEvidenceStrategy final : public EdgeEvidenceStrategy {
public:
    void
    classify(CandidateEdge& edge, const manumesh::common::MeshEdgeInfo& info, EdgeEvidenceContext&) const override {
        edge.boundary = info.faces.size() == 1;
    }
};

class NonManifoldEvidenceStrategy final : public EdgeEvidenceStrategy {
public:
    void
    classify(CandidateEdge& edge, const manumesh::common::MeshEdgeInfo& info, EdgeEvidenceContext&) const override {
        edge.nonManifold = info.faces.size() > 2;
    }
};

class DihedralEvidenceStrategy final : public EdgeEvidenceStrategy {
public:
    void classify(
        CandidateEdge& edge, const manumesh::common::MeshEdgeInfo& info, EdgeEvidenceContext& context
    ) const override {
        if (info.faces.size() != 2) {
            return;
        }

        const DihedralAngle* dihedral = context.dihedralAngle(manumesh::common::meshEdgeKey(edge.a, edge.b));
        if (dihedral == nullptr) {
            return;
        }
        if (dihedral->inconsistentWinding) {
            ++context.analysis.inconsistentWindingEdges;
        }
        edge.angleRad = dihedral->angleRad;
        edge.dihedral = edge.angleRad >= context.dihedralThreshold;
        if (edge.dihedral) {
            edge.signedKind = dihedral->signedKind;
        }
    }
};

class NormalTensorEvidenceStrategy final : public EdgeEvidenceStrategy {
public:
    void
    classify(CandidateEdge& edge, const manumesh::common::MeshEdgeInfo&, EdgeEvidenceContext& context) const override {
        if (edge.boundary || edge.dihedral || edge.nonManifold) {
            return;
        }
        edge.normalTensor = normalTensorEdgeCandidate(
            edge, context.tensor, context.discreteFeatureVertex, context.mesh, context.options, context.analysis
        );
    }
};

class SmoothCurvatureEvidenceStrategy final : public EdgeEvidenceStrategy {
public:
    void
    classify(CandidateEdge& edge, const manumesh::common::MeshEdgeInfo&, EdgeEvidenceContext& context) const override {
        if (edge.boundary || edge.dihedral || edge.nonManifold) {
            return;
        }
        edge.smoothCurvature = smoothCurvatureEdgeCandidate(
            edge, context.curvature, context.discreteFeatureVertex, context.mesh, context.options, context.analysis
        );
    }
};

} // namespace

std::vector<CandidateEdge> collectFeatureEdges(
    const Mesh& mesh, const FeatureOptions& options, FeatureDetectionCache& cache, FeatureAnalysisBuilder& builder
) {
    std::vector<CandidateEdge> result;
    EdgeEvidenceContext context(mesh, options, cache, builder.analysis());
    const BoundaryEvidenceStrategy boundaryEvidence;
    const DihedralEvidenceStrategy dihedralEvidence;
    const NonManifoldEvidenceStrategy nonManifoldEvidence;
    const NormalTensorEvidenceStrategy normalTensorEvidence;
    const SmoothCurvatureEvidenceStrategy smoothCurvatureEvidence;
    const std::array<const EdgeEvidenceStrategy*, 5> strategies = {
        &boundaryEvidence, &dihedralEvidence, &nonManifoldEvidence, &normalTensorEvidence, &smoothCurvatureEvidence
    };

    for (const auto& [key, info] : context.edges) {
        CandidateEdge edge;
        const auto [a, b] = manumesh::common::unpackMeshEdgeKey(key);
        edge.a = a;
        edge.b = b;

        for (const EdgeEvidenceStrategy* strategy : strategies) {
            strategy->classify(edge, info, context);
        }

        if (edge.boundary || edge.dihedral || edge.normalTensor || edge.smoothCurvature || edge.nonManifold) {
            result.push_back(edge);
        }
    }

    // The edge-info map iterates in an unspecified order; sort by the edge key
    // once so downstream graph construction, tracing, and loop recovery see a
    // deterministic candidate sequence.
    std::sort(result.begin(), result.end(), [](const CandidateEdge& lhs, const CandidateEdge& rhs) {
        return lhs.a != rhs.a ? lhs.a < rhs.a : lhs.b < rhs.b;
    });
    for (const CandidateEdge& edge : result) {
        builder.recordFeatureEdge(edge);
    }
    return result;
}

} // namespace manumesh::feature::detector_detail
