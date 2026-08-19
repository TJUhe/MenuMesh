/**
 * @file src/feature_detection/FeatureEvidence.cpp
 * @brief 从边界、二面角、法向张量和曲率收集边证据。
 * @ingroup manumesh_feature_detection
 *
 * @details 将网格局部观测转换为带类型的候选边证据。
 * @algorithm 边界和非流形关联直接记录；流形内部边使用考虑绕序的二面角分类。
 *            可选的法向张量和光顺曲率通道仅在端点持久性、切线一致性与边对齐度
 *            均通过门限时贡献证据。
 * @failuremodes 绕序不一致时保留无符号强度，并报告未知凸/凹符号；退化面不贡献法向证据。
 */

#include "detail/FeatureEvidence.h"
#include "core/MathUtils.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "common/detail/MeshQueries.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace manumesh {
namespace feature {
namespace detector_detail {
namespace {

using DihedralAngle = manumesh::common::OrientedDihedralAngle;

/** @brief 供所有证据策略共享的缓存和选项状态。 */
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
          windingFlip(cache.faceWindingFlips()),
          dihedralThreshold(options.featureAngleDeg * kPi / 180.0),
          tensor(
              options.useNormalTensorFeatures ? computeNormalTensorFeaturesCached(
                                                    mesh,
                                                    cache,
                                                    NormalTensorOptions{
                                                        options.normalTensorSmoothingIterations,
                                                        options.normalTensorScaleCount,
                                                        options.normalFilter,
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

    /**
     * @brief 返回一条双面边缓存的有向二面角。
     * markDiscreteFeatureVertices 会为每条内部边精确计算一次二面角，
     * 二面角证据策略复用该结果。
     */
    const DihedralAngle* dihedralAngle(std::uint64_t key) const {
        const auto it = dihedralAngles.find(key);
        return it == dihedralAngles.end() ? nullptr : &it->second;
    }

    const Mesh& mesh;
    const FeatureOptions& options;
    FeatureAnalysis& analysis;
    const std::vector<Vec3>& normals;
    const manumesh::common::MeshEdgeInfoMap& edges;
    const std::vector<char>& windingFlip;
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

        analysis.normalTensorVertexWeights.assign(tensor.size(), 0.0);
        const int requiredPersistentScales = manumesh::clampValue(
            options.normalTensorMinPersistentScales, 1, std::max(1, options.normalTensorScaleCount)
        );
        double localScaleSum = 0.0;
        double persistenceSum = 0.0;
        for (std::size_t vertexId = 0; vertexId < tensor.size(); ++vertexId) {
            const NormalTensorVertex& vertex = tensor[vertexId];
            analysis.maxNormalTensorFeatureScore = std::max(analysis.maxNormalTensorFeatureScore, vertex.featureScore);
            analysis.maxNormalTensorPersistentScore =
                std::max(analysis.maxNormalTensorPersistentScore, vertex.persistentFeatureScore);
            if (vertex.persistentScales >= requiredPersistentScales) {
                analysis.normalTensorVertexWeights[vertexId] = vertex.persistentFeatureScore;
            }
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
        for (const auto& pairEntry : edges) {
            const auto& key = pairEntry.first;
            const auto& info = pairEntry.second;
            const std::pair<int, int> edgeVertices = manumesh::common::unpackMeshEdgeKey(key);
            const int a = edgeVertices.first;
            const int b = edgeVertices.second;
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
    const bool aDiscrete = edge.a < static_cast<int>(discreteFeatureVertex.size()) && discreteFeatureVertex[edge.a];
    const bool bDiscrete = edge.b < static_cast<int>(discreteFeatureVertex.size()) && discreteFeatureVertex[edge.b];
    // 硬-硬边已经由边界/二面角/非流形通道表达。允许单个软端点连接到硬端点，
    // 使脊线或张量折痕可以终止在显式硬特征 junction，而不是被整点屏蔽。
    if (aDiscrete && bDiscrete) {
        return false;
    }

    const int softEndpointCount = (aDiscrete ? 0 : 1) + (bDiscrete ? 0 : 1);
    const double score =
        ((aDiscrete ? 0.0 : tensor[edge.a].featureScore) + (bDiscrete ? 0.0 : tensor[edge.b].featureScore)) /
        static_cast<double>(softEndpointCount);
    analysis.maxNormalTensorFeatureScore = std::max(analysis.maxNormalTensorFeatureScore, score);
    const double persistentScore = ((aDiscrete ? 0.0 : tensor[edge.a].persistentFeatureScore) +
                                    (bDiscrete ? 0.0 : tensor[edge.b].persistentFeatureScore)) /
                                   static_cast<double>(softEndpointCount);
    analysis.maxNormalTensorPersistentScore = std::max(analysis.maxNormalTensorPersistentScore, persistentScore);
    const int requiredPersistentScales =
        manumesh::clampValue(options.normalTensorMinPersistentScales, 1, std::max(1, options.normalTensorScaleCount));
    const int minPersistentScales =
        aDiscrete ? tensor[edge.b].persistentScales
                  : (bDiscrete ? tensor[edge.a].persistentScales
                               : std::min(tensor[edge.a].persistentScales, tensor[edge.b].persistentScales));
    edge.tensorPersistentScore = persistentScore;
    edge.tensorPersistentScales = minPersistentScales;
    if (minPersistentScales < requiredPersistentScales) {
        return false;
    }
    const double minEndpointScore =
        aDiscrete
            ? tensor[edge.b].persistentFeatureScore
            : (bDiscrete ? tensor[edge.a].persistentFeatureScore
                         : std::min(tensor[edge.a].persistentFeatureScore, tensor[edge.b].persistentFeatureScore));
    if (minEndpointScore < options.normalTensorFeatureThreshold) {
        return false;
    }
    if ((!aDiscrete && tensor[edge.a].creaseSaliency < tensor[edge.a].cornerSaliency) ||
        (!bDiscrete && tensor[edge.b].creaseSaliency < tensor[edge.b].cornerSaliency)) {
        return false;
    }

    Vec3 direction = mesh.vertices[edge.b] - mesh.vertices[edge.a];
    const double length = direction.norm();
    if (length <= 1e-20) {
        return false;
    }
    direction /= length;
    const double alignA = aDiscrete ? 1.0 : std::abs(direction.dot(tensor[edge.a].creaseTangent));
    const double alignB = bDiscrete ? 1.0 : std::abs(direction.dot(tensor[edge.b].creaseTangent));
    return std::min(alignA, alignB) >= options.normalTensorMinEdgeAlignment;
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
    const bool aDiscrete = edge.a < static_cast<int>(discreteFeatureVertex.size()) && discreteFeatureVertex[edge.a];
    const bool bDiscrete = edge.b < static_cast<int>(discreteFeatureVertex.size()) && discreteFeatureVertex[edge.b];
    if (aDiscrete && bDiscrete) {
        return false;
    }

    const SmoothCurvatureVertex& a = curvature[edge.a];
    const SmoothCurvatureVertex& b = curvature[edge.b];
    const int softEndpointCount = (aDiscrete ? 0 : 1) + (bDiscrete ? 0 : 1);
    const double featureScore = ((aDiscrete ? 0.0 : a.featureScore) + (bDiscrete ? 0.0 : b.featureScore)) /
                                static_cast<double>(softEndpointCount);
    analysis.maxSmoothCurvatureFeatureScore = std::max(analysis.maxSmoothCurvatureFeatureScore, featureScore);
    const double persistentScore =
        ((aDiscrete ? 0.0 : a.persistentFeatureScore) + (bDiscrete ? 0.0 : b.persistentFeatureScore)) /
        static_cast<double>(softEndpointCount);
    analysis.maxSmoothCurvaturePersistentScore = std::max(analysis.maxSmoothCurvaturePersistentScore, persistentScore);
    const int requiredPersistentScales = manumesh::clampValue(
        options.smoothCurvatureMinPersistentScales, 1, std::max(1, options.smoothCurvatureScaleCount)
    );
    const int minPersistentScales =
        aDiscrete ? b.persistentScales
                  : (bDiscrete ? a.persistentScales : std::min(a.persistentScales, b.persistentScales));
    const double minEndpointScore =
        aDiscrete
            ? b.persistentFeatureScore
            : (bDiscrete ? a.persistentFeatureScore : std::min(a.persistentFeatureScore, b.persistentFeatureScore));
    edge.curvaturePersistentScore = persistentScore;
    edge.curvaturePersistentScales = minPersistentScales;
    if (minPersistentScales < requiredPersistentScales || minEndpointScore < options.smoothCurvatureFeatureThreshold ||
        (!aDiscrete && a.signedKind == 0) || (!bDiscrete && b.signedKind == 0) ||
        (!aDiscrete && !bDiscrete && a.signedKind != b.signedKind)) {
        return false;
    }

    Vec3 direction = mesh.vertices[edge.b] - mesh.vertices[edge.a];
    if (direction.norm() <= 1e-20) {
        return false;
    }
    direction.normalize();
    const double alignA = aDiscrete ? 1.0 : std::abs(direction.dot(a.curveTangent));
    const double alignB = bDiscrete ? 1.0 : std::abs(direction.dot(b.curveTangent));
    const double tangentConsistency = aDiscrete || bDiscrete ? 1.0 : std::abs(a.curveTangent.dot(b.curveTangent));
    if (std::min(alignA, alignB) < options.smoothCurvatureMinEdgeAlignment ||
        tangentConsistency < options.smoothCurvatureMinTangentConsistency) {
        return false;
    }
    edge.signedKind = aDiscrete ? b.signedKind : a.signedKind;
    return true;
}

/** @brief 将只关联一个面的边标记为边界证据。 */
void classifyBoundaryEvidence(CandidateEdge& edge, const manumesh::common::MeshEdgeInfo& info) {
    edge.boundary = info.faces.size() == 1;
}

/** @brief 将面关联数超过两个的边标记为非流形证据。 */
void classifyNonManifoldEvidence(CandidateEdge& edge, const manumesh::common::MeshEdgeInfo& info) {
    edge.nonManifold = info.faces.size() > 2;
}

/** @brief 根据考虑绕序的二面角，将尖锐边分类为带符号证据。 */
void classifyDihedralEvidence(
    CandidateEdge& edge, const manumesh::common::MeshEdgeInfo& info, EdgeEvidenceContext& context
) {
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

/** @brief 添加多尺度持久法向张量证据。 */
void classifyNormalTensorEvidence(CandidateEdge& edge, EdgeEvidenceContext& context) {
    if (edge.boundary || edge.dihedral || edge.nonManifold) {
        return;
    }
    edge.normalTensor = normalTensorEdgeCandidate(
        edge, context.tensor, context.discreteFeatureVertex, context.mesh, context.options, context.analysis
    );
}

/** @brief 添加由光顺曲率拟合得到的持久脊线/谷线证据。 */
void classifySmoothCurvatureEvidence(CandidateEdge& edge, EdgeEvidenceContext& context) {
    if (edge.boundary || edge.dihedral || edge.nonManifold) {
        return;
    }
    edge.smoothCurvature = smoothCurvatureEdgeCandidate(
        edge, context.curvature, context.discreteFeatureVertex, context.mesh, context.options, context.analysis
    );
}

} // namespace

std::vector<CandidateEdge> collectFeatureEdges(
    const Mesh& mesh, const FeatureOptions& options, FeatureDetectionCache& cache, FeatureAnalysisBuilder& builder
) {
    std::vector<CandidateEdge> result;
    EdgeEvidenceContext context(mesh, options, cache, builder.analysis());

    for (const auto& pairEntry : context.edges) {
        const auto& key = pairEntry.first;
        const auto& info = pairEntry.second;
        CandidateEdge edge;
        const std::pair<int, int> edgeVertices = manumesh::common::unpackMeshEdgeKey(key);
        const int a = edgeVertices.first;
        const int b = edgeVertices.second;
        edge.a = a;
        edge.b = b;

        classifyBoundaryEvidence(edge, info);
        classifyDihedralEvidence(edge, info, context);
        classifyNonManifoldEvidence(edge, info);
        classifyNormalTensorEvidence(edge, context);
        classifySmoothCurvatureEvidence(edge, context);

        if (edge.boundary || edge.dihedral || edge.normalTensor || edge.smoothCurvature || edge.nonManifold) {
            result.push_back(edge);
        }
    }

    // edge-info 映射的遍历顺序未指定；先按边键排序，
    // 使后续图构建、追踪和环恢复看到确定性的候选序列。
    std::sort(result.begin(), result.end(), [](const CandidateEdge& lhs, const CandidateEdge& rhs) {
        return lhs.a != rhs.a ? lhs.a < rhs.a : lhs.b < rhs.b;
    });
    for (const CandidateEdge& edge : result) {
        builder.recordFeatureEdge(edge);
    }
    return result;
}

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
