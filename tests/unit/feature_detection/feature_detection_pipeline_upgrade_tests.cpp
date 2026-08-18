/**
 * @file tests/unit/feature_detection/feature_detection_pipeline_upgrade_tests.cpp
 * @brief 验证法向滤波、图合并、分支配对和曲面分区。
 * @ingroup manumesh_tests
 */

#include "AnalyticFixtures.h"
#include "FeatureDetectionTestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "common/detail/MeshQueries.h"
#include "detail/FeatureDetectionCache.h"
#include "detail/FeatureDetectionTypes.h"
#include "detail/FeatureGraph.h"
#include "detail/FeatureGraphConsolidation.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace manumesh {
namespace tests {
namespace analytic = manumesh::test::analytic;
namespace {

feature::FeatureOptions hardFeatureOptions() {
    feature::FeatureOptions options;
    options.featureAngleDeg = 40.0;
    options.loopTraceAngleDeg = 40.0;
    options.useNormalTensorFeatures = false;
    options.cleanupFeatureGraph = true;
    return options;
}

Mesh makeSeparatedAlignedChains() {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(2.0, 0.0, 0.0),
        Vec3(3.0, 0.0, 0.0),
        Vec3(4.0, 0.0, 0.0),
        Vec3(5.0, 0.0, 0.0),
        Vec3(2.5, 1.0, 0.0),
    };
    mesh.faces = {
        Face{{0, 1, 6}},
        Face{{1, 2, 6}},
        Face{{3, 4, 6}},
        Face{{4, 5, 6}},
    };
    return mesh;
}

void addWeakEdge(
    feature::detector_detail::TraceGraph& trace,
    feature::FeatureAnalysis& analysis,
    int first,
    int second,
    int signedKind
) {
    feature::detector_detail::CandidateEdge edge;
    edge.a = first;
    edge.b = second;
    edge.normalTensor = true;
    edge.tensorPersistentScore = 0.3;
    edge.tensorPersistentScales = 3;
    edge.signedKind = signedKind;
    feature::detector_detail::addTraceGraphEdge(trace, analysis, edge);
}

} // namespace

TEST(FeatureDetectionUpgrade, NormalFilterStabilizesNoisyNormalsAndPreservesRims) {
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(48, 12, 1.0, 2.0, true);
    const Mesh noisy =
        analytic::withDeterministicNoise(cylinder.mesh, 0.08 * analytic::meanEdgeLength(cylinder.mesh), 20260715u);

    feature::FeatureNormalFilterOptions filterOptions;
    filterOptions.enabled = true;
    filterOptions.iterations = 4;
    filterOptions.angleSigmaDeg = 18.0;
    filterOptions.preserveAngleDeg = 55.0;
    const feature::FeatureNormalFilterResult filtered = feature::filterFeatureNormals(noisy, filterOptions);

    ASSERT_EQ(noisy.faces.size(), filtered.faceNormals.size());
    EXPECT_EQ(4, filtered.report.iterationsCompleted);
    EXPECT_GT(filtered.report.changedFaces, 0);
    EXPECT_GT(filtered.report.preservedEdges, 0);
    EXPECT_GT(filtered.report.meanAngularChangeDeg, 0.0);
    for (const Vec3& normal : filtered.faceNormals) {
        if (normal.squaredNorm() > 1e-30) {
            EXPECT_NEAR(1.0, normal.norm(), 1e-12);
        }
    }

    feature::FeatureOptions options = hardFeatureOptions();
    options.circleFitRelativeThreshold = 0.10;
    options.normalFilter = filterOptions;
    const feature::FeatureAnalysis analysis = feature::detectFeatureCurves(noisy, options);
    const feature::FeatureEdgeBenchmark benchmark =
        feature::benchmarkFeatureEdges(analysis, cylinder.groundTruthFeatureEdges());
    EXPECT_EQ(4, analysis.normalFilter.iterationsCompleted);
    EXPECT_GE(benchmark.edgeRecall, 0.95);
}

TEST(FeatureDetectionUpgrade, NormalTensorConsumesFilteredFaceNormalsFromTheSharedCache) {
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(48, 12, 1.0, 2.0, true);
    const Mesh noisy =
        analytic::withDeterministicNoise(cylinder.mesh, 0.08 * analytic::meanEdgeLength(cylinder.mesh), 20260715u);

    feature::FeatureNormalFilterOptions filterOptions;
    filterOptions.enabled = true;
    filterOptions.iterations = 4;
    filterOptions.angleSigmaDeg = 18.0;
    filterOptions.preserveAngleDeg = 55.0;

    feature::NormalTensorOptions tensorOptions;
    tensorOptions.smoothingIterations = 1;
    tensorOptions.scaleCount = 3;

    feature::detector_detail::FeatureDetectionCache rawCache(noisy);
    feature::detector_detail::FeatureDetectionCache filteredCache(noisy, filterOptions);
    const std::vector<feature::NormalTensorVertex> raw =
        feature::detector_detail::computeNormalTensorFeaturesCached(noisy, rawCache, tensorOptions, 0.02);
    const std::vector<feature::NormalTensorVertex> filtered =
        feature::detector_detail::computeNormalTensorFeaturesCached(noisy, filteredCache, tensorOptions, 0.02);

    ASSERT_EQ(raw.size(), filtered.size());
    double maxScoreDelta = 0.0;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        maxScoreDelta =
            std::max(maxScoreDelta, std::abs(raw[i].persistentFeatureScore - filtered[i].persistentFeatureScore));
    }
    EXPECT_GT(filteredCache.normalFilterReport().changedFaces, 0);
    EXPECT_GT(maxScoreDelta, 1e-8);

    tensorOptions.normalFilter = filterOptions;
    const std::vector<feature::NormalTensorVertex> publicFiltered =
        feature::computeNormalTensorFeatures(noisy, tensorOptions, 0.02);
    ASSERT_EQ(filtered.size(), publicFiltered.size());
    for (std::size_t i = 0; i < filtered.size(); ++i) {
        EXPECT_NEAR(filtered[i].persistentFeatureScore, publicFiltered[i].persistentFeatureScore, 1e-12);
        EXPECT_NEAR(filtered[i].localScale, publicFiltered[i].localScale, 1e-12);
    }
}

TEST(FeatureDetectionUpgrade, ConsolidationBridgesOnlyCompatibleComponents) {
    const Mesh mesh = makeSeparatedAlignedChains();
    feature::FeatureAnalysis analysis;
    analysis.vertices.assign(mesh.vertices.size(), feature::VertexFeature{});
    analysis.graph.vertices.assign(mesh.vertices.size(), feature::FeatureGraphVertex{});
    feature::detector_detail::TraceGraph trace;
    trace.adjacency.resize(mesh.vertices.size());
    trace.traceVertex.assign(mesh.vertices.size(), 0);

    addWeakEdge(trace, analysis, 0, 1, 1);
    addWeakEdge(trace, analysis, 1, 2, 1);
    addWeakEdge(trace, analysis, 3, 4, 1);
    addWeakEdge(trace, analysis, 4, 5, 1);

    feature::FeatureOptions options;
    options.graphConsolidation.enabled = true;
    options.graphConsolidation.maxGapLengthRatio = 2.0;
    options.graphConsolidation.minAlignment = 0.8;
    feature::detector_detail::FeatureDetectionCache cache(mesh);
    feature::detector_detail::consolidateFeatureGraph(mesh, options, cache, trace, analysis);

    EXPECT_TRUE(feature::detector_detail::traceGraphHasEdge(trace, 2, 3));
    EXPECT_EQ(1, analysis.graphConsolidationBridges);
    const auto bridge = std::find_if(analysis.graph.edges.begin(), analysis.graph.edges.end(), [](const auto& edge) {
        return edge.consolidationBridge;
    });
    ASSERT_NE(analysis.graph.edges.end(), bridge);
    EXPECT_EQ(1, bridge->signedKind);
}

TEST(FeatureDetectionUpgrade, ConsolidationRejectsOppositeSignedCurves) {
    const Mesh mesh = makeSeparatedAlignedChains();
    feature::FeatureAnalysis analysis;
    analysis.vertices.assign(mesh.vertices.size(), feature::VertexFeature{});
    analysis.graph.vertices.assign(mesh.vertices.size(), feature::FeatureGraphVertex{});
    feature::detector_detail::TraceGraph trace;
    trace.adjacency.resize(mesh.vertices.size());
    trace.traceVertex.assign(mesh.vertices.size(), 0);

    addWeakEdge(trace, analysis, 0, 1, 1);
    addWeakEdge(trace, analysis, 1, 2, 1);
    addWeakEdge(trace, analysis, 3, 4, -1);
    addWeakEdge(trace, analysis, 4, 5, -1);

    feature::FeatureOptions options;
    options.graphConsolidation.enabled = true;
    options.graphConsolidation.maxGapLengthRatio = 2.0;
    options.graphConsolidation.minAlignment = 0.8;
    feature::detector_detail::FeatureDetectionCache cache(mesh);
    feature::detector_detail::consolidateFeatureGraph(mesh, options, cache, trace, analysis);

    EXPECT_FALSE(feature::detector_detail::traceGraphHasEdge(trace, 2, 3));
    EXPECT_EQ(0, analysis.graphConsolidationBridges);
}

TEST(FeatureDetectionUpgrade, JunctionsExposeBranchContinuationPairs) {
    const analytic::ChamferBoxFixture box = analytic::makeChamferBox(2.0, 0.25, 4);
    const feature::FeatureAnalysis analysis = feature::detectFeatureCurves(box.mesh, hardFeatureOptions());

    ASSERT_FALSE(analysis.graph.junctionVertices.empty());
    EXPECT_GT(analysis.junctionBranchPairs, 0);
    EXPECT_EQ(0, analysis.ambiguousJunctions);
    for (int junction : analysis.graph.junctionVertices) {
        const feature::FeatureGraphVertex& vertex = analysis.graph.vertices[junction];
        EXPECT_GE(vertex.branches.size(), 3u);
        EXPECT_FALSE(vertex.branchPairs.empty());
    }
}

TEST(FeatureDetectionUpgrade, JunctionContinuationDoesNotPairSameSideBranches) {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(1.0, 0.1, 0.0),
        Vec3(0.0, 1.0, 0.0),
    };

    feature::FeatureAnalysis analysis;
    analysis.vertices.assign(mesh.vertices.size(), feature::VertexFeature{});
    std::vector<feature::detector_detail::CandidateEdge> edges;
    for (int neighbor = 1; neighbor < static_cast<int>(mesh.vertices.size()); ++neighbor) {
        feature::detector_detail::CandidateEdge edge;
        edge.a = 0;
        edge.b = neighbor;
        edge.dihedral = true;
        edges.push_back(edge);
    }
    feature::detector_detail::initializeFeatureGraph(edges, analysis);
    feature::detector_detail::finalizeFeatureGraphMarkers(mesh, analysis);

    ASSERT_EQ(1u, analysis.graph.junctionVertices.size());
    const feature::FeatureGraphVertex& junction = analysis.graph.vertices[0];
    EXPECT_TRUE(junction.branchPairs.empty());
    EXPECT_TRUE(junction.ambiguousJunction);
    EXPECT_EQ(1, analysis.ambiguousJunctions);
}

TEST(FeatureDetectionUpgrade, CappedCylinderPartitionsIntoSideAndCapPatches) {
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(32, 6, 1.0, 2.0, true);
    feature::FeatureOptions options = hardFeatureOptions();
    options.surfacePatches.enabled = true;
    options.surfacePatches.includeWeakEvidence = false;
    const feature::FeatureAnalysis analysis = feature::detectFeatureCurves(cylinder.mesh, options);

    EXPECT_EQ(cylinder.mesh.faces.size(), analysis.facePatchIds.size());
    ASSERT_EQ(3u, analysis.patches.size());
    EXPECT_EQ(3, analysis.closedSurfacePatches);
    EXPECT_EQ(2u, analysis.patchAdjacencies.size());
    const auto sidePatch = std::max_element(
        analysis.patches.begin(),
        analysis.patches.end(),
        [](const feature::FeaturePatch& lhs, const feature::FeaturePatch& rhs) {
            return lhs.faceCount < rhs.faceCount;
        }
    );
    ASSERT_NE(analysis.patches.end(), sidePatch);
    EXPECT_TRUE(sidePatch->normal.isApprox(Vec3(0.0, 0.0, 1.0), 1e-12));
    for (const feature::FeaturePatch& patch : analysis.patches) {
        EXPECT_GT(patch.faceCount, 0);
        EXPECT_TRUE(patch.closed);
    }

    feature::FeatureBenchmarkLabels labels;
    labels.facePatchIds.assign(cylinder.mesh.faces.size(), -1);
    for (int faceId = 0; faceId < static_cast<int>(cylinder.mesh.faces.size()); ++faceId) {
        const Face& face = cylinder.mesh.faces[faceId];
        const Vec3 normal = (cylinder.mesh.vertices[face.v[1]] - cylinder.mesh.vertices[face.v[0]])
                                .cross(cylinder.mesh.vertices[face.v[2]] - cylinder.mesh.vertices[face.v[0]])
                                .normalized();
        labels.facePatchIds[faceId] = std::abs(normal.z()) > 0.9 ? (normal.z() > 0.0 ? 1 : 2) : 0;
    }
    const feature::FeatureEdgeBenchmark benchmark = feature::benchmarkFeatureAnalysis(cylinder.mesh, analysis, labels);
    EXPECT_GT(benchmark.labeledFaceAdjacencies, 0);
    EXPECT_DOUBLE_EQ(1.0, benchmark.patchAdjacencyAccuracy);
}

TEST(FeatureDetectionUpgrade, PatchBenchmarkDoesNotRewardMissingPredictions) {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        Vec3(1.0, 1.0, 0.0),
    };
    mesh.faces = {
        Face{{0, 1, 2}},
        Face{{1, 3, 2}},
    };

    feature::FeatureAnalysis analysis;
    analysis.source = feature::featureAnalysisSource(mesh);
    analysis.vertices.resize(mesh.vertices.size());
    analysis.graph.vertices.resize(mesh.vertices.size());
    feature::FeatureBenchmarkLabels labels;
    labels.facePatchIds = {0, 1};
    const feature::FeatureEdgeBenchmark benchmark = feature::benchmarkFeatureAnalysis(mesh, analysis, labels);

    EXPECT_EQ(1, benchmark.labeledFaceAdjacencies);
    EXPECT_EQ(0, benchmark.correctFaceAdjacencies);
    EXPECT_DOUBLE_EQ(0.0, benchmark.patchAdjacencyAccuracy);
}

TEST(FeatureDetectionUpgrade, RejectsInvalidUpgradeOptions) {
    const Mesh mesh = analytic::makeCylinder(16, 2, 1.0, 1.0, true).mesh;

    feature::FeatureOptions options;
    options.normalFilter.angleSigmaDeg = 0.0;
    EXPECT_THROW(feature::detectFeatureCurves(mesh, options), std::invalid_argument);

    options = feature::FeatureOptions{};
    options.graphConsolidation.minAlignment = -0.1;
    EXPECT_THROW(feature::detectFeatureCurves(mesh, options), std::invalid_argument);

    feature::FeatureNormalFilterOptions filterOptions;
    filterOptions.iterations = feature::kMaxFeatureNormalFilterIterations + 1;
    EXPECT_THROW(feature::filterFeatureNormals(mesh, filterOptions), std::invalid_argument);

    feature::NormalTensorOptions tensorOptions;
    tensorOptions.normalFilter = filterOptions;
    EXPECT_THROW(feature::computeNormalTensorFeatures(mesh, tensorOptions), std::invalid_argument);
}

} // namespace tests
} // namespace manumesh
