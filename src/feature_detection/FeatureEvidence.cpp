#include "detail/FeatureEvidence.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "common/detail/MeshQueries.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace manumesh::feature::detector_detail {
namespace {

struct EdgeEvidenceContext {
    EdgeEvidenceContext(const Mesh& inputMesh, const FeatureOptions& inputOptions, FeatureAnalysis& outputAnalysis)
        : mesh(inputMesh),
          options(inputOptions),
          analysis(outputAnalysis),
          normals(manumesh::detail::computeFaceNormals(mesh)),
          edges(manumesh::detail::buildMeshEdgeInfo(mesh)),
          dihedralThreshold(options.featureAngleDeg * kPi / 180.0),
          tensor(
              options.useNormalTensorFeatures ? computeNormalTensorFeatures(
                                                    mesh,
                                                    NormalTensorOptions{
                                                        options.normalTensorSmoothingIterations,
                                                        options.normalTensorScaleCount,
                                                    },
                                                    options.normalTensorFeatureThreshold
                                                )
                                              : std::vector<NormalTensorVertex>()
          ),
          discreteFeatureVertex(mesh.vertices.size(), 0) {
        summarizeNormalTensorVertices();
        markDiscreteFeatureVertices();
    }

    const Mesh& mesh;
    const FeatureOptions& options;
    FeatureAnalysis& analysis;
    std::vector<Vec3> normals;
    manumesh::detail::MeshEdgeInfoMap edges;
    double dihedralThreshold = 0.0;
    std::vector<NormalTensorVertex> tensor;
    std::vector<char> discreteFeatureVertex;

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
        for (const auto& [key, info] : edges) {
            bool discrete = false;
            if (info.faces.size() == 1 || info.faces.size() > 2) {
                discrete = true;
            } else if (info.faces.size() == 2) {
                const double dot = std::clamp(std::abs(normals[info.faces[0]].dot(normals[info.faces[1]])), -1.0, 1.0);
                discrete = std::acos(dot) >= dihedralThreshold;
            }
            if (discrete) {
                const auto [a, b] = manumesh::detail::unpackMeshEdgeKey(key);
                discreteFeatureVertex[a] = 1;
                discreteFeatureVertex[b] = 1;
            }
        }
    }
};

int signedDihedralKind(
    const Mesh& mesh, const std::vector<Vec3>& normals, const manumesh::detail::MeshEdgeInfo& info, int a, int b
) {
    if (info.faces.size() != 2) {
        return 0;
    }

    const int f0 = info.faces[0];
    const int f1 = info.faces[1];
    Vec3 edge = mesh.vertices[b] - mesh.vertices[a];
    if (edge.norm() <= 1e-20 || normals[f0].norm() <= 1e-20 || normals[f1].norm() <= 1e-20) {
        return 0;
    }
    edge.normalize();

    const Vec3 c0 = manumesh::detail::faceCentroid(mesh, mesh.faces[f0]);
    const Vec3 c1 = manumesh::detail::faceCentroid(mesh, mesh.faces[f1]);
    const double side0 = edge.cross(normals[f0]).dot(c1 - c0);
    const double side1 = edge.cross(normals[f1]).dot(c0 - c1);
    if (std::abs(side0) <= 1e-12 || std::abs(side1) <= 1e-12) {
        return 0;
    }
    const bool normalsPointTowardEachOther = side0 > 0.0 && side1 > 0.0;
    return normalsPointTowardEachOther ? -1 : 1;
}

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

class EdgeEvidenceStrategy {
public:
    virtual ~EdgeEvidenceStrategy() = default;
    virtual void
    classify(CandidateEdge& edge, const manumesh::detail::MeshEdgeInfo& info, EdgeEvidenceContext& context) const = 0;
};

class BoundaryEvidenceStrategy final : public EdgeEvidenceStrategy {
public:
    void
    classify(CandidateEdge& edge, const manumesh::detail::MeshEdgeInfo& info, EdgeEvidenceContext&) const override {
        edge.boundary = info.faces.size() == 1;
    }
};

class NonManifoldEvidenceStrategy final : public EdgeEvidenceStrategy {
public:
    void
    classify(CandidateEdge& edge, const manumesh::detail::MeshEdgeInfo& info, EdgeEvidenceContext&) const override {
        edge.nonManifold = info.faces.size() > 2;
    }
};

class DihedralEvidenceStrategy final : public EdgeEvidenceStrategy {
public:
    void classify(
        CandidateEdge& edge, const manumesh::detail::MeshEdgeInfo& info, EdgeEvidenceContext& context
    ) const override {
        if (info.faces.size() != 2) {
            return;
        }

        const double dot =
            std::clamp(std::abs(context.normals[info.faces[0]].dot(context.normals[info.faces[1]])), -1.0, 1.0);
        edge.angleRad = std::acos(dot);
        edge.dihedral = edge.angleRad >= context.dihedralThreshold;
        if (edge.dihedral) {
            edge.signedKind = signedDihedralKind(context.mesh, context.normals, info, edge.a, edge.b);
        }
    }
};

class NormalTensorEvidenceStrategy final : public EdgeEvidenceStrategy {
public:
    void
    classify(CandidateEdge& edge, const manumesh::detail::MeshEdgeInfo&, EdgeEvidenceContext& context) const override {
        if (edge.boundary || edge.dihedral || edge.nonManifold) {
            return;
        }
        edge.normalTensor = normalTensorEdgeCandidate(
            edge, context.tensor, context.discreteFeatureVertex, context.mesh, context.options, context.analysis
        );
    }
};

} // namespace

std::vector<CandidateEdge>
collectFeatureEdges(const Mesh& mesh, const FeatureOptions& options, FeatureAnalysisBuilder& builder) {
    std::vector<CandidateEdge> result;
    EdgeEvidenceContext context(mesh, options, builder.analysis());
    const BoundaryEvidenceStrategy boundaryEvidence;
    const DihedralEvidenceStrategy dihedralEvidence;
    const NonManifoldEvidenceStrategy nonManifoldEvidence;
    const NormalTensorEvidenceStrategy normalTensorEvidence;
    const std::array<const EdgeEvidenceStrategy*, 4> strategies = {
        &boundaryEvidence, &dihedralEvidence, &nonManifoldEvidence, &normalTensorEvidence
    };

    for (const auto& [key, info] : context.edges) {
        CandidateEdge edge;
        const auto [a, b] = manumesh::detail::unpackMeshEdgeKey(key);
        edge.a = a;
        edge.b = b;

        for (const EdgeEvidenceStrategy* strategy : strategies) {
            strategy->classify(edge, info, context);
        }

        if (edge.boundary || edge.dihedral || edge.normalTensor || edge.nonManifold) {
            result.push_back(edge);
            builder.recordFeatureEdge(edge);
        }
    }
    return result;
}

} // namespace manumesh::feature::detector_detail
