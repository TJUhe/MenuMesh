/**
 * @file src/feature_detection/FeatureBenchmark.cpp
 * @brief Implements feature benchmark facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details Computes edge, junction, continuation, and patch-label benchmark metrics.
 * @algorithm Canonical undirected edge keys support exact set comparison;
 * branch pairs are compared without orientation; patch labels use pairwise
 * same/different-region agreement so numeric patch ids need not match.
 */

#include "algorithms/feature_detection/FeatureDetector.h"

#include "common/detail/MeshQueries.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace manumesh::feature {
namespace {

double ratio(int numerator, int denominator) {
    return denominator > 0 ? static_cast<double>(numerator) / static_cast<double>(denominator) : 0.0;
}

double f1(double precision, double recall) {
    return precision + recall > 0.0 ? 2.0 * precision * recall / (precision + recall) : 0.0;
}

struct BranchPairKey {
    int junction = -1;
    int first = -1;
    int second = -1;

    bool operator==(const BranchPairKey& other) const {
        return junction == other.junction && first == other.first && second == other.second;
    }
};

struct BranchPairKeyHash {
    std::size_t operator()(const BranchPairKey& key) const {
        std::size_t seed = std::hash<int>{}(key.junction);
        seed ^= std::hash<int>{}(key.first) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        seed ^= std::hash<int>{}(key.second) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        return seed;
    }
};

BranchPairKey branchPairKey(int junction, int first, int second) {
    return BranchPairKey{junction, std::min(first, second), std::max(first, second)};
}

} // namespace

FeatureEdgeBenchmark benchmarkFeatureEdges(
    const FeatureAnalysis& analysis,
    const std::vector<std::pair<int, int>>& groundTruthEdges,
    const std::vector<int>& groundTruthJunctionVertices
) {
    FeatureEdgeBenchmark result;
    std::unordered_set<std::uint64_t> truthEdges;
    truthEdges.reserve(groundTruthEdges.size());
    for (const auto& [a, b] : groundTruthEdges) {
        if (a >= 0 && b >= 0 && a != b) {
            truthEdges.insert(common::meshEdgeKey(a, b));
        }
    }

    std::unordered_set<std::uint64_t> detectedEdges;
    detectedEdges.reserve(analysis.graph.edges.size());
    for (const FeatureGraphEdge& edge : analysis.graph.edges) {
        if (!edge.removedByCleanup && edge.a >= 0 && edge.b >= 0 && edge.a != edge.b) {
            detectedEdges.insert(common::meshEdgeKey(edge.a, edge.b));
        }
    }

    result.groundTruthEdges = static_cast<int>(truthEdges.size());
    result.detectedEdges = static_cast<int>(detectedEdges.size());
    for (std::uint64_t edge : detectedEdges) {
        if (truthEdges.find(edge) != truthEdges.end()) {
            ++result.truePositiveEdges;
        } else {
            ++result.falsePositiveEdges;
        }
    }
    for (std::uint64_t edge : truthEdges) {
        if (detectedEdges.find(edge) == detectedEdges.end()) {
            ++result.falseNegativeEdges;
        }
    }
    result.edgePrecision = ratio(result.truePositiveEdges, result.truePositiveEdges + result.falsePositiveEdges);
    result.edgeRecall = ratio(result.truePositiveEdges, result.truePositiveEdges + result.falseNegativeEdges);
    result.edgeF1 = f1(result.edgePrecision, result.edgeRecall);

    std::unordered_set<int> truthJunctions;
    truthJunctions.reserve(groundTruthJunctionVertices.size());
    for (int id : groundTruthJunctionVertices) {
        if (id >= 0) {
            truthJunctions.insert(id);
        }
    }
    const std::unordered_set<int> detectedJunctions(
        analysis.graph.junctionVertices.begin(), analysis.graph.junctionVertices.end()
    );
    result.groundTruthJunctions = static_cast<int>(truthJunctions.size());
    result.detectedJunctions = static_cast<int>(detectedJunctions.size());
    for (int id : detectedJunctions) {
        if (truthJunctions.find(id) != truthJunctions.end()) {
            ++result.truePositiveJunctions;
        } else {
            ++result.falsePositiveJunctions;
        }
    }
    for (int id : truthJunctions) {
        if (detectedJunctions.find(id) == detectedJunctions.end()) {
            ++result.falseNegativeJunctions;
        }
    }
    result.junctionPrecision =
        ratio(result.truePositiveJunctions, result.truePositiveJunctions + result.falsePositiveJunctions);
    result.junctionRecall =
        ratio(result.truePositiveJunctions, result.truePositiveJunctions + result.falseNegativeJunctions);
    result.junctionF1 = f1(result.junctionPrecision, result.junctionRecall);

    if (!analysis.components.empty()) {
        double closureSum = 0.0;
        for (const FeatureComponent& component : analysis.components) {
            closureSum += component.closureRate;
        }
        result.loopClosureRate = closureSum / static_cast<double>(analysis.components.size());
    }
    result.meanComponentConfidence = analysis.meanFeatureComponentConfidence;
    return result;
}

FeatureEdgeBenchmark
benchmarkFeatureAnalysis(const Mesh& mesh, const FeatureAnalysis& analysis, const FeatureBenchmarkLabels& labels) {
    FeatureEdgeBenchmark result = benchmarkFeatureEdges(analysis, labels.edges, labels.junctionVertices);

    std::unordered_set<BranchPairKey, BranchPairKeyHash> truthPairs;
    for (const FeatureBranchPairLabel& label : labels.branchPairs) {
        if (label.junctionVertex >= 0 && label.firstNeighbor >= 0 && label.secondNeighbor >= 0 &&
            label.firstNeighbor != label.secondNeighbor) {
            truthPairs.insert(branchPairKey(label.junctionVertex, label.firstNeighbor, label.secondNeighbor));
        }
    }
    std::unordered_set<BranchPairKey, BranchPairKeyHash> detectedPairs;
    for (int junction = 0; junction < static_cast<int>(analysis.graph.vertices.size()); ++junction) {
        const FeatureGraphVertex& vertex = analysis.graph.vertices[junction];
        for (const FeatureGraphBranchPair& pair : vertex.branchPairs) {
            if (pair.firstBranch < 0 || pair.secondBranch < 0 ||
                pair.firstBranch >= static_cast<int>(vertex.branches.size()) ||
                pair.secondBranch >= static_cast<int>(vertex.branches.size())) {
                continue;
            }
            detectedPairs.insert(branchPairKey(
                junction,
                vertex.branches[pair.firstBranch].neighborVertex,
                vertex.branches[pair.secondBranch].neighborVertex
            ));
        }
    }
    result.groundTruthBranchPairs = static_cast<int>(truthPairs.size());
    result.detectedBranchPairs = static_cast<int>(detectedPairs.size());
    for (const BranchPairKey& pair : detectedPairs) {
        if (truthPairs.find(pair) != truthPairs.end()) {
            ++result.truePositiveBranchPairs;
        } else {
            ++result.falsePositiveBranchPairs;
        }
    }
    for (const BranchPairKey& pair : truthPairs) {
        if (detectedPairs.find(pair) == detectedPairs.end()) {
            ++result.falseNegativeBranchPairs;
        }
    }
    result.branchPairPrecision =
        ratio(result.truePositiveBranchPairs, result.truePositiveBranchPairs + result.falsePositiveBranchPairs);
    result.branchPairRecall =
        ratio(result.truePositiveBranchPairs, result.truePositiveBranchPairs + result.falseNegativeBranchPairs);
    result.branchPairF1 = f1(result.branchPairPrecision, result.branchPairRecall);

    if (!labels.facePatchIds.empty()) {
        const common::MeshEdgeInfoMap edgeInfo = common::buildMeshEdgeInfo(mesh);
        for (const auto& [key, info] : edgeInfo) {
            (void)key;
            if (info.faces.size() != 2) {
                continue;
            }
            const int first = info.faces[0];
            const int second = info.faces[1];
            if (first < 0 || second < 0 || first >= static_cast<int>(labels.facePatchIds.size()) ||
                second >= static_cast<int>(labels.facePatchIds.size()) || labels.facePatchIds[first] < 0 ||
                labels.facePatchIds[second] < 0) {
                continue;
            }
            ++result.labeledFaceAdjacencies;
            const bool truthSame = labels.facePatchIds[first] == labels.facePatchIds[second];
            const bool detectedValid = first < static_cast<int>(analysis.facePatchIds.size()) &&
                                       second < static_cast<int>(analysis.facePatchIds.size()) &&
                                       analysis.facePatchIds[first] >= 0 && analysis.facePatchIds[second] >= 0;
            const bool detectedSame = detectedValid && analysis.facePatchIds[first] == analysis.facePatchIds[second];
            if (detectedValid && truthSame == detectedSame) {
                ++result.correctFaceAdjacencies;
            }
        }
        result.patchAdjacencyAccuracy = ratio(result.correctFaceAdjacencies, result.labeledFaceAdjacencies);
    }
    return result;
}

} // namespace manumesh::feature
