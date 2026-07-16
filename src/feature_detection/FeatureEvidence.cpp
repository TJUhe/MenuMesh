/**
 * @file src/feature_detection/FeatureEvidence.cpp
 * @brief Implements feature evidence facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details Converts mesh-local observations into typed candidate-edge evidence.
 * @algorithm Boundary and non-manifold incidence are recorded directly;
 * manifold interior edges use winding-aware dihedral classification. Optional
 * normal-tensor and smooth-curvature channels contribute only when endpoint
 * persistence, tangent agreement, and edge alignment pass their thresholds.
 * @failuremodes Inconsistent winding keeps unsigned strength but reports an
 * unknown convex/concave sign. Degenerate faces contribute no normal evidence.
 */

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

using DihedralAngle = manumesh::common::OrientedDihedralAngle;

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
          windingFlip(manumesh::common::harmonizeFaceWindings(inputMesh, cache.edgeInfo())),
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
                                                           options.smoothCurvatureUseStableScaleSelection,
                                                           options.smoothCurvatureMinScaleStability,
                                                       },
                                                       options.smoothCurvatureFeatureThreshold
                                                   )
                                                 : std::vector<SmoothCurvatureVertex>()
          ),
          discreteFeatureVertex(mesh.vertices.size(), 0) {
        analysis.normalFilter = cache.normalFilterReport();
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
                const DihedralAngle dihedral =
                    manumesh::common::computeOrientedDihedralAngle(mesh, normals, windingFlip, info, a, b);
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
        double stabilitySum = 0.0;
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
            stabilitySum += vertex.scaleStability;
        }

        if (analysis.smoothCurvatureScoredVertices > 0) {
            const double count = static_cast<double>(analysis.smoothCurvatureScoredVertices);
            analysis.meanSmoothCurvatureLocalScale = localScaleSum / count;
            analysis.meanSmoothCurvaturePersistence = persistenceSum / count;
            analysis.meanSmoothCurvatureScaleStability = stabilitySum / count;
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
