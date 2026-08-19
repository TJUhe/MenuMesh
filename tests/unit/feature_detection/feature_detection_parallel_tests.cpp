/**
 * @file tests/unit/feature_detection/feature_detection_parallel_tests.cpp
 * @brief 验证特征检测的并行范围与串行确定性结果一致。
 */

#include "AnalyticFixtures.h"
#include "FeatureDetectionTestSupport.h"
#include "TestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "common/detail/MeshQueries.h"
#include "common/detail/ParallelExecution.h"
#include "core/MeshGenerators.h"
#include "feature_detection/detail/FeaturePrimitiveRecovery.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {

namespace feature = manumesh::feature;
using manumesh::ExecutionMode;
using manumesh::ExecutionOptions;
using manumesh::Mesh;
using manumesh::Vec3;

struct PrimitiveComponentFixture {
    Mesh mesh;
    feature::detector_detail::TraceGraph trace;
};

ExecutionOptions parallelOptions() {
    ExecutionOptions options;
    options.mode = ExecutionMode::Parallel;
    options.maxConcurrency = 4;
    options.minItemsPerTask = 32;
    return options;
}

void expectVec3Equal(const Vec3& first, const Vec3& second) {
    EXPECT_DOUBLE_EQ(first.x(), second.x());
    EXPECT_DOUBLE_EQ(first.y(), second.y());
    EXPECT_DOUBLE_EQ(first.z(), second.z());
}

Mesh makeNoisyIrregularFixture() {
    Mesh mesh = manumesh::generatePlaneGrid(36, 2.0, true);
    for (Vec3& vertex : mesh.vertices) {
        const double x = vertex.x();
        const double y = vertex.y();
        vertex.z() =
            0.16 * std::exp(-8.0 * x * x) + 0.012 * std::sin(4.5 * x + 1.9 * y) + 0.008 * std::cos(3.2 * y - 1.1 * x);
    }
    return manumesh::test::analytic::withDeterministicNoise(mesh, 0.0015, 0x7f4a7c159e3779b9ULL);
}

Mesh makeReversedWindingFilterFixture() {
    Mesh mesh = makeNoisyIrregularFixture();
    const std::size_t faceId = mesh.faces.size() / 2;
    std::swap(mesh.faces[faceId].v[1], mesh.faces[faceId].v[2]);
    return mesh;
}

Mesh makeHighValenceSmoothFixture() {
    constexpr int segments = 257;
    constexpr double twoPi = 6.28318530717958647692;
    Mesh mesh;
    mesh.vertices.reserve(segments + 1);
    mesh.faces.reserve(segments);
    mesh.vertices.emplace_back(0.0, 0.0, 0.18);
    for (int segment = 0; segment < segments; ++segment) {
        const double angle = twoPi * static_cast<double>(segment) / static_cast<double>(segments);
        const double radius = 1.0 + 0.08 * std::cos(3.0 * angle);
        mesh.vertices.emplace_back(
            radius * std::cos(angle), radius * std::sin(angle), 0.07 * std::cos(2.0 * angle) + 0.02 * std::sin(5.0 * angle)
        );
    }
    for (int segment = 0; segment < segments; ++segment) {
        manumesh::Face face;
        face.v = {{0, segment + 1, (segment + 1) % segments + 1}};
        mesh.faces.push_back(face);
    }
    return mesh;
}

PrimitiveComponentFixture makePrimitiveComponentFixture(int componentCount, int samplesPerComponent) {
    PrimitiveComponentFixture fixture;
    fixture.mesh.vertices.reserve(static_cast<std::size_t>(componentCount * samplesPerComponent));
    constexpr double twoPi = 6.28318530717958647692;
    for (int component = 0; component < componentCount; ++component) {
        const int first = static_cast<int>(fixture.mesh.vertices.size());
        const Vec3 center(4.0 * static_cast<double>(component), 0.0, 0.0);
        for (int sample = 0; sample < samplesPerComponent; ++sample) {
            const double angle = twoPi * static_cast<double>(sample) / static_cast<double>(samplesPerComponent);
            fixture.mesh.vertices.emplace_back(center.x() + std::cos(angle), center.y() + std::sin(angle), center.z());
        }
        for (int sample = 0; sample < samplesPerComponent; ++sample) {
            const int a = first + sample;
            const int b = first + (sample + 1) % samplesPerComponent;
            fixture.trace.adjacency.resize(fixture.mesh.vertices.size());
            fixture.trace.adjacency[a].push_back(b);
            fixture.trace.adjacency[b].push_back(a);
        }
    }
    fixture.trace.adjacency.resize(fixture.mesh.vertices.size());
    return fixture;
}

void expectNormalTensorEqual(
    const std::vector<feature::NormalTensorVertex>& first, const std::vector<feature::NormalTensorVertex>& second
) {
    ASSERT_EQ(first.size(), second.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        const feature::NormalTensorVertex& a = first[i];
        const feature::NormalTensorVertex& b = second[i];
        expectVec3Equal(a.normal, b.normal);
        expectVec3Equal(a.creaseTangent, b.creaseTangent);
        EXPECT_DOUBLE_EQ(a.surfaceSaliency, b.surfaceSaliency);
        EXPECT_DOUBLE_EQ(a.creaseSaliency, b.creaseSaliency);
        EXPECT_DOUBLE_EQ(a.cornerSaliency, b.cornerSaliency);
        EXPECT_DOUBLE_EQ(a.featureScore, b.featureScore);
        EXPECT_DOUBLE_EQ(a.averageFeatureScore, b.averageFeatureScore);
        EXPECT_DOUBLE_EQ(a.persistentFeatureScore, b.persistentFeatureScore);
        EXPECT_DOUBLE_EQ(a.localScale, b.localScale);
        EXPECT_EQ(a.persistentScales, b.persistentScales);
        EXPECT_EQ(a.selectedScale, b.selectedScale);
        EXPECT_EQ(a.smoothingSteps, b.smoothingSteps);
        EXPECT_DOUBLE_EQ(a.effectiveRadius, b.effectiveRadius);
    }
}

void expectSmoothCurvatureEqual(
    const std::vector<feature::SmoothCurvatureVertex>& first, const std::vector<feature::SmoothCurvatureVertex>& second
) {
    ASSERT_EQ(first.size(), second.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        const feature::SmoothCurvatureVertex& a = first[i];
        const feature::SmoothCurvatureVertex& b = second[i];
        expectVec3Equal(a.normal, b.normal);
        expectVec3Equal(a.curveTangent, b.curveTangent);
        expectVec3Equal(a.extremumDirection, b.extremumDirection);
        EXPECT_DOUBLE_EQ(a.principalCurvature, b.principalCurvature);
        EXPECT_DOUBLE_EQ(a.secondaryCurvature, b.secondaryCurvature);
        EXPECT_DOUBLE_EQ(a.anisotropy, b.anisotropy);
        EXPECT_DOUBLE_EQ(a.extremumStrength, b.extremumStrength);
        EXPECT_DOUBLE_EQ(a.featureScore, b.featureScore);
        EXPECT_DOUBLE_EQ(a.averageFeatureScore, b.averageFeatureScore);
        EXPECT_DOUBLE_EQ(a.persistentFeatureScore, b.persistentFeatureScore);
        EXPECT_DOUBLE_EQ(a.fitResidual, b.fitResidual);
        EXPECT_DOUBLE_EQ(a.localScale, b.localScale);
        EXPECT_EQ(a.persistentScales, b.persistentScales);
        EXPECT_EQ(a.selectedScale, b.selectedScale);
        EXPECT_DOUBLE_EQ(a.scaleStability, b.scaleStability);
        EXPECT_EQ(a.signedKind, b.signedKind);
    }
}

void expectGraphEqual(const feature::FeatureAnalysis& first, const feature::FeatureAnalysis& second) {
    ASSERT_EQ(first.graph.edges.size(), second.graph.edges.size());
    for (std::size_t i = 0; i < first.graph.edges.size(); ++i) {
        const feature::FeatureGraphEdge& a = first.graph.edges[i];
        const feature::FeatureGraphEdge& b = second.graph.edges[i];
        EXPECT_EQ(a.a, b.a);
        EXPECT_EQ(a.b, b.b);
        EXPECT_EQ(a.boundary, b.boundary);
        EXPECT_EQ(a.dihedral, b.dihedral);
        EXPECT_EQ(a.normalTensor, b.normalTensor);
        EXPECT_EQ(a.smoothCurvature, b.smoothCurvature);
        EXPECT_EQ(a.nonManifold, b.nonManifold);
        EXPECT_EQ(a.cleanupBridge, b.cleanupBridge);
        EXPECT_EQ(a.consolidationBridge, b.consolidationBridge);
        EXPECT_EQ(a.removedByCleanup, b.removedByCleanup);
        EXPECT_EQ(a.signedKind, b.signedKind);
        EXPECT_DOUBLE_EQ(a.tensorPersistence, b.tensorPersistence);
        EXPECT_EQ(a.tensorPersistentScales, b.tensorPersistentScales);
        EXPECT_DOUBLE_EQ(a.curvaturePersistence, b.curvaturePersistence);
        EXPECT_EQ(a.curvaturePersistentScales, b.curvaturePersistentScales);
    }
    ASSERT_EQ(first.graph.vertices.size(), second.graph.vertices.size());
    for (std::size_t i = 0; i < first.graph.vertices.size(); ++i) {
        const feature::FeatureGraphVertex& a = first.graph.vertices[i];
        const feature::FeatureGraphVertex& b = second.graph.vertices[i];
        EXPECT_EQ(a.incidentEdges, b.incidentEdges);
        EXPECT_EQ(a.loopIds, b.loopIds);
        EXPECT_EQ(a.junction, b.junction);
        EXPECT_EQ(a.shared, b.shared);
        EXPECT_EQ(a.endpoint, b.endpoint);
        EXPECT_EQ(a.ambiguousJunction, b.ambiguousJunction);
        ASSERT_EQ(a.branches.size(), b.branches.size());
        for (std::size_t j = 0; j < a.branches.size(); ++j) {
            EXPECT_EQ(a.branches[j].edgeId, b.branches[j].edgeId);
            EXPECT_EQ(a.branches[j].neighborVertex, b.branches[j].neighborVertex);
            expectVec3Equal(a.branches[j].tangent, b.branches[j].tangent);
            EXPECT_EQ(a.branches[j].signedKind, b.branches[j].signedKind);
        }
        ASSERT_EQ(a.branchPairs.size(), b.branchPairs.size());
        for (std::size_t j = 0; j < a.branchPairs.size(); ++j) {
            EXPECT_EQ(a.branchPairs[j].firstBranch, b.branchPairs[j].firstBranch);
            EXPECT_EQ(a.branchPairs[j].secondBranch, b.branchPairs[j].secondBranch);
            EXPECT_DOUBLE_EQ(a.branchPairs[j].alignment, b.branchPairs[j].alignment);
        }
    }
    EXPECT_EQ(first.graph.junctionVertices, second.graph.junctionVertices);
    EXPECT_EQ(first.graph.sharedVertices, second.graph.sharedVertices);
    EXPECT_EQ(first.graph.endpointVertices, second.graph.endpointVertices);
}

void expectFeatureComponentEqual(const feature::FeatureComponent& first, const feature::FeatureComponent& second) {
    EXPECT_EQ(first.id, second.id);
    EXPECT_EQ(first.vertices, second.vertices);
    EXPECT_EQ(first.edgeCount, second.edgeCount);
    EXPECT_EQ(first.boundaryEdges, second.boundaryEdges);
    EXPECT_EQ(first.dihedralEdges, second.dihedralEdges);
    EXPECT_EQ(first.normalTensorEdges, second.normalTensorEdges);
    EXPECT_EQ(first.smoothCurvatureEdges, second.smoothCurvatureEdges);
    EXPECT_EQ(first.nonManifoldEdges, second.nonManifoldEdges);
    EXPECT_EQ(first.cleanupBridgeEdges, second.cleanupBridgeEdges);
    EXPECT_EQ(first.consolidationBridgeEdges, second.consolidationBridgeEdges);
    EXPECT_EQ(first.strongEvidenceEdges, second.strongEvidenceEdges);
    EXPECT_EQ(first.weakEvidenceEdges, second.weakEvidenceEdges);
    EXPECT_EQ(first.junctionVertices, second.junctionVertices);
    EXPECT_EQ(first.endpointVertices, second.endpointVertices);
    EXPECT_EQ(first.cycleRank, second.cycleRank);
    EXPECT_EQ(first.closed, second.closed);
    EXPECT_DOUBLE_EQ(first.closureRate, second.closureRate);
    EXPECT_DOUBLE_EQ(first.strongEvidenceRatio, second.strongEvidenceRatio);
    EXPECT_DOUBLE_EQ(first.meanTensorPersistence, second.meanTensorPersistence);
    EXPECT_DOUBLE_EQ(first.meanCurvaturePersistence, second.meanCurvaturePersistence);
    EXPECT_DOUBLE_EQ(first.meanPrimitiveResidual, second.meanPrimitiveResidual);
    EXPECT_DOUBLE_EQ(first.confidence, second.confidence);
}

void expectFeaturePatchEqual(const feature::FeaturePatch& first, const feature::FeaturePatch& second) {
    EXPECT_EQ(first.id, second.id);
    EXPECT_EQ(first.faceCount, second.faceCount);
    EXPECT_EQ(first.featureBoundaryEdges, second.featureBoundaryEdges);
    EXPECT_EQ(first.meshBoundaryEdges, second.meshBoundaryEdges);
    EXPECT_EQ(first.nonManifoldBoundaryEdges, second.nonManifoldBoundaryEdges);
    EXPECT_EQ(first.closed, second.closed);
    EXPECT_DOUBLE_EQ(first.area, second.area);
    expectVec3Equal(first.normal, second.normal);
    EXPECT_EQ(first.neighboringPatches, second.neighboringPatches);
}

void expectFeaturePatchAdjacencyEqual(
    const feature::FeaturePatchAdjacency& first, const feature::FeaturePatchAdjacency& second
) {
    EXPECT_EQ(first.firstPatch, second.firstPatch);
    EXPECT_EQ(first.secondPatch, second.secondPatch);
    EXPECT_EQ(first.featureEdges, second.featureEdges);
}

void expectFeatureAnalysisEquivalent(const feature::FeatureAnalysis& first, const feature::FeatureAnalysis& second) {
    EXPECT_EQ(first.vertices.size(), second.vertices.size());
    ASSERT_EQ(first.vertices.size(), second.vertices.size());
    for (std::size_t i = 0; i < first.vertices.size(); ++i) {
        const feature::VertexFeature& a = first.vertices[i];
        const feature::VertexFeature& b = second.vertices[i];
        EXPECT_EQ(a.isFeature, b.isFeature);
        EXPECT_EQ(a.circular, b.circular);
        EXPECT_EQ(a.junction, b.junction);
        EXPECT_EQ(a.weakFeature, b.weakFeature);
        EXPECT_EQ(a.primitive, b.primitive);
        EXPECT_EQ(a.loopId, b.loopId);
        EXPECT_EQ(a.componentId, b.componentId);
        EXPECT_DOUBLE_EQ(a.confidence, b.confidence);
        expectVec3Equal(a.tangent, b.tangent);
        expectVec3Equal(a.circleCenter, b.circleCenter);
        expectVec3Equal(a.circleNormal, b.circleNormal);
        EXPECT_DOUBLE_EQ(a.circleRadius, b.circleRadius);
        expectVec3Equal(a.ellipseCenter, b.ellipseCenter);
        expectVec3Equal(a.ellipseNormal, b.ellipseNormal);
        expectVec3Equal(a.ellipseMajorAxis, b.ellipseMajorAxis);
        expectVec3Equal(a.ellipseMinorAxis, b.ellipseMinorAxis);
        EXPECT_DOUBLE_EQ(a.ellipseMajorRadius, b.ellipseMajorRadius);
        EXPECT_DOUBLE_EQ(a.ellipseMinorRadius, b.ellipseMinorRadius);
    }

    ASSERT_EQ(first.loops.size(), second.loops.size());
    for (std::size_t i = 0; i < first.loops.size(); ++i) {
        const feature::FeatureLoop& a = first.loops[i];
        const feature::FeatureLoop& b = second.loops[i];
        EXPECT_EQ(a.id, b.id);
        EXPECT_EQ(a.componentId, b.componentId);
        EXPECT_EQ(a.vertices, b.vertices);
        EXPECT_EQ(a.edgeCount, b.edgeCount);
        EXPECT_EQ(a.closed, b.closed);
        EXPECT_EQ(a.circular, b.circular);
        EXPECT_EQ(a.mostlyBoundary, b.mostlyBoundary);
        EXPECT_EQ(a.weakFeature, b.weakFeature);
        EXPECT_DOUBLE_EQ(a.componentConfidence, b.componentConfidence);
        EXPECT_DOUBLE_EQ(a.primitiveResidual, b.primitiveResidual);
        EXPECT_EQ(a.primitive, b.primitive);
        expectVec3Equal(a.center, b.center);
        expectVec3Equal(a.normal, b.normal);
        expectVec3Equal(a.majorAxis, b.majorAxis);
        expectVec3Equal(a.minorAxis, b.minorAxis);
        EXPECT_DOUBLE_EQ(a.radius, b.radius);
        EXPECT_DOUBLE_EQ(a.majorRadius, b.majorRadius);
        EXPECT_DOUBLE_EQ(a.minorRadius, b.minorRadius);
        EXPECT_DOUBLE_EQ(a.axisRatio, b.axisRatio);
        EXPECT_DOUBLE_EQ(a.rmsRadialError, b.rmsRadialError);
        EXPECT_DOUBLE_EQ(a.maxRadialError, b.maxRadialError);
        EXPECT_DOUBLE_EQ(a.rmsEllipseError, b.rmsEllipseError);
        EXPECT_DOUBLE_EQ(a.maxEllipseError, b.maxEllipseError);
        EXPECT_DOUBLE_EQ(a.rmsPlaneError, b.rmsPlaneError);
        EXPECT_DOUBLE_EQ(a.maxPlaneError, b.maxPlaneError);
        EXPECT_EQ(a.convexEdges, b.convexEdges);
        EXPECT_EQ(a.concaveEdges, b.concaveEdges);
        EXPECT_EQ(a.unknownSignedEdges, b.unknownSignedEdges);
    }

    ASSERT_EQ(first.components.size(), second.components.size());
    for (std::size_t i = 0; i < first.components.size(); ++i) {
        expectFeatureComponentEqual(first.components[i], second.components[i]);
    }

    expectGraphEqual(first, second);
    EXPECT_EQ(first.facePatchIds, second.facePatchIds);
    ASSERT_EQ(first.patches.size(), second.patches.size());
    for (std::size_t i = 0; i < first.patches.size(); ++i) {
        expectFeaturePatchEqual(first.patches[i], second.patches[i]);
    }
    ASSERT_EQ(first.patchAdjacencies.size(), second.patchAdjacencies.size());
    for (std::size_t i = 0; i < first.patchAdjacencies.size(); ++i) {
        expectFeaturePatchAdjacencyEqual(first.patchAdjacencies[i], second.patchAdjacencies[i]);
    }
    EXPECT_EQ(first.source.vertexCount, second.source.vertexCount);
    EXPECT_EQ(first.source.faceCount, second.source.faceCount);
    EXPECT_EQ(first.source.topologyFingerprint, second.source.topologyFingerprint);
    EXPECT_EQ(first.source.geometryFingerprint, second.source.geometryFingerprint);
    EXPECT_EQ(first.normalTensorVertexWeights, second.normalTensorVertexWeights);
    EXPECT_EQ(first.smoothCurvatureVertexWeights, second.smoothCurvatureVertexWeights);

#define EXPECT_ANALYSIS_FIELD(field) EXPECT_EQ(first.field, second.field)
    EXPECT_ANALYSIS_FIELD(featureEdges);
    EXPECT_ANALYSIS_FIELD(tracedFeatureEdges);
    EXPECT_ANALYSIS_FIELD(untracedFeatureEdges);
    EXPECT_ANALYSIS_FIELD(graphCleanupBridgedGaps);
    EXPECT_ANALYSIS_FIELD(graphCleanupRemovedSpurs);
    EXPECT_ANALYSIS_FIELD(graphCleanupMergedJunctions);
    EXPECT_ANALYSIS_FIELD(boundaryFeatureEdges);
    EXPECT_ANALYSIS_FIELD(dihedralFeatureEdges);
    EXPECT_ANALYSIS_FIELD(normalTensorFeatureEdges);
    EXPECT_ANALYSIS_FIELD(smoothCurvatureFeatureEdges);
    EXPECT_ANALYSIS_FIELD(nonManifoldFeatureEdges);
    EXPECT_ANALYSIS_FIELD(normalTensorScoredVertices);
    EXPECT_ANALYSIS_FIELD(smoothCurvatureScoredVertices);
    EXPECT_ANALYSIS_FIELD(convexFeatureEdges);
    EXPECT_ANALYSIS_FIELD(concaveFeatureEdges);
    EXPECT_ANALYSIS_FIELD(unknownSignedFeatureEdges);
    EXPECT_ANALYSIS_FIELD(weakFeatureComponents);
    EXPECT_ANALYSIS_FIELD(highConfidenceFeatureComponents);
    EXPECT_ANALYSIS_FIELD(inconsistentWindingEdges);
    EXPECT_ANALYSIS_FIELD(graphCleanupSkippedByCap);
    EXPECT_ANALYSIS_FIELD(circularRecoveryTruncated);
    EXPECT_ANALYSIS_FIELD(degenerateFaces);
    EXPECT_ANALYSIS_FIELD(graphConsolidationBridges);
    EXPECT_ANALYSIS_FIELD(graphConsolidationSkippedByCap);
    EXPECT_ANALYSIS_FIELD(junctionBranchPairs);
    EXPECT_ANALYSIS_FIELD(ambiguousJunctions);
    EXPECT_ANALYSIS_FIELD(closedSurfacePatches);
    EXPECT_ANALYSIS_FIELD(segmentationIgnoredRecoveryEdges);
#undef EXPECT_ANALYSIS_FIELD
    EXPECT_DOUBLE_EQ(first.maxNormalTensorFeatureScore, second.maxNormalTensorFeatureScore);
    EXPECT_DOUBLE_EQ(first.maxNormalTensorPersistentScore, second.maxNormalTensorPersistentScore);
    EXPECT_DOUBLE_EQ(first.meanNormalTensorLocalScale, second.meanNormalTensorLocalScale);
    EXPECT_DOUBLE_EQ(first.meanNormalTensorPersistence, second.meanNormalTensorPersistence);
    EXPECT_DOUBLE_EQ(first.maxSmoothCurvatureFeatureScore, second.maxSmoothCurvatureFeatureScore);
    EXPECT_DOUBLE_EQ(first.maxSmoothCurvaturePersistentScore, second.maxSmoothCurvaturePersistentScore);
    EXPECT_DOUBLE_EQ(first.meanSmoothCurvatureLocalScale, second.meanSmoothCurvatureLocalScale);
    EXPECT_DOUBLE_EQ(first.meanSmoothCurvaturePersistence, second.meanSmoothCurvaturePersistence);
    EXPECT_DOUBLE_EQ(first.meanSmoothCurvatureScaleStability, second.meanSmoothCurvatureScaleStability);
    EXPECT_DOUBLE_EQ(first.meanFeatureComponentConfidence, second.meanFeatureComponentConfidence);
    EXPECT_DOUBLE_EQ(first.minFeatureComponentConfidence, second.minFeatureComponentConfidence);
    EXPECT_EQ(first.normalFilter.iterationsCompleted, second.normalFilter.iterationsCompleted);
    EXPECT_EQ(first.normalFilter.changedFaces, second.normalFilter.changedFaces);
    EXPECT_EQ(first.normalFilter.preservedEdges, second.normalFilter.preservedEdges);
    EXPECT_DOUBLE_EQ(first.normalFilter.meanAngularChangeDeg, second.normalFilter.meanAngularChangeDeg);
    EXPECT_DOUBLE_EQ(first.normalFilter.maxAngularChangeDeg, second.normalFilter.maxAngularChangeDeg);
    EXPECT_DOUBLE_EQ(first.normalFilter.meanEdgeIndicator, second.normalFilter.meanEdgeIndicator);
}

} // namespace

TEST(FeatureDetectionParallel, IndependentStagesMatchSerialResultsExactly) {
    const Mesh mesh = makeNoisyIrregularFixture();
    feature::FeatureNormalFilterOptions filterOptions;
    filterOptions.enabled = true;
    filterOptions.iterations = 2;
    filterOptions.angleSigmaDeg = 18.0;
    filterOptions.preserveAngleDeg = 55.0;
    filterOptions.relaxation = 0.75;
    const feature::NormalTensorOptions tensorOptions{1, 3, filterOptions};
    const feature::SmoothCurvatureOptions curvatureOptions{2, 3, 2, 0.65, true, 0.0};
    const ExecutionOptions parallel = parallelOptions();

    const feature::FeatureNormalFilterResult serialFilter = feature::filterFeatureNormals(mesh, filterOptions);
    const feature::FeatureNormalFilterResult parallelFilter =
        feature::filterFeatureNormals(mesh, filterOptions, parallel);
    ASSERT_EQ(serialFilter.faceNormals.size(), parallelFilter.faceNormals.size());
    for (std::size_t i = 0; i < serialFilter.faceNormals.size(); ++i) {
        expectVec3Equal(serialFilter.faceNormals[i], parallelFilter.faceNormals[i]);
    }
    EXPECT_EQ(serialFilter.report.iterationsCompleted, parallelFilter.report.iterationsCompleted);
    EXPECT_EQ(serialFilter.report.changedFaces, parallelFilter.report.changedFaces);
    EXPECT_EQ(serialFilter.report.preservedEdges, parallelFilter.report.preservedEdges);
    EXPECT_DOUBLE_EQ(serialFilter.report.meanAngularChangeDeg, parallelFilter.report.meanAngularChangeDeg);
    EXPECT_DOUBLE_EQ(serialFilter.report.maxAngularChangeDeg, parallelFilter.report.maxAngularChangeDeg);
    EXPECT_DOUBLE_EQ(serialFilter.report.meanEdgeIndicator, parallelFilter.report.meanEdgeIndicator);

    const std::vector<feature::NormalTensorVertex> serialTensor =
        feature::computeNormalTensorFeatures(mesh, tensorOptions, 0.0);
    const std::vector<feature::NormalTensorVertex> parallelTensor =
        feature::computeNormalTensorFeatures(mesh, tensorOptions, 0.0, parallel);
    expectNormalTensorEqual(serialTensor, parallelTensor);

    const std::vector<feature::SmoothCurvatureVertex> serialCurvature =
        feature::computeSmoothCurvatureFeatures(mesh, curvatureOptions, 0.0);
    const std::vector<feature::SmoothCurvatureVertex> parallelCurvature =
        feature::computeSmoothCurvatureFeatures(mesh, curvatureOptions, 0.0, parallel);
    expectSmoothCurvatureEqual(serialCurvature, parallelCurvature);
}

TEST(FeatureDetectionParallel, SmoothCurvatureHighValenceNeighborhoodMatchesSerialResultsExactly) {
    const Mesh mesh = makeHighValenceSmoothFixture();
    const feature::SmoothCurvatureOptions curvatureOptions{1, 1, 0, 0.0};
    ExecutionOptions parallel = parallelOptions();
    parallel.minItemsPerTask = 16;

    const std::vector<feature::SmoothCurvatureVertex> serial =
        feature::computeSmoothCurvatureFeatures(mesh, curvatureOptions, 0.0);
    const std::vector<feature::SmoothCurvatureVertex> parallelResult =
        feature::computeSmoothCurvatureFeatures(mesh, curvatureOptions, 0.0, parallel);

    // The central vertex has 257 one-ring neighbors, forcing the sparse task-local set to rehash.
    expectSmoothCurvatureEqual(serial, parallelResult);
}

TEST(FeatureDetectionParallel, CompleteFeatureGraphMatchesSerialResultsExactly) {
    const Mesh mesh = makeNoisyIrregularFixture();
    feature::FeatureOptions options;
    options.featureAngleDeg = 179.0;
    options.loopTraceAngleDeg = 179.0;
    options.minFeatureLoopVertices = 6;
    options.useNormalTensorFeatures = true;
    options.normalTensorFeatureThreshold = 1e-8;
    options.normalTensorMinEdgeAlignment = 0.0;
    options.normalTensorScaleCount = 3;
    options.normalTensorSmoothingIterations = 1;
    options.normalTensorMinPersistentScales = 1;
    options.useSmoothCurvatureFeatures = true;
    options.smoothCurvatureFeatureThreshold = 1e-8;
    options.smoothCurvatureMinEdgeAlignment = 0.0;
    options.smoothCurvatureMinTangentConsistency = 0.0;
    options.smoothCurvatureBaseNeighborhoodRings = 2;
    options.smoothCurvatureScaleCount = 3;
    options.smoothCurvatureMinPersistentScales = 1;
    options.smoothCurvatureRobustFitIterations = 1;
    options.normalFilter = feature::FeatureNormalFilterOptions{true, 3, 25.0, 120.0, 0.45};
    options.surfacePatches.enabled = true;
    const ExecutionOptions parallel = parallelOptions();
    const feature::FeatureDetector detector(options);

    const feature::FeatureAnalysis serial = detector.analyze(mesh);
    const feature::FeatureAnalysis parallelResult = detector.analyze(mesh, parallel);
    expectFeatureAnalysisEquivalent(serial, parallelResult);
    EXPECT_GT(serial.normalTensorScoredVertices, 0);
    EXPECT_GT(serial.smoothCurvatureScoredVertices, 0);
    EXPECT_GT(serial.normalTensorFeatureEdges, 0);
    EXPECT_GT(serial.smoothCurvatureFeatureEdges, 0);
}

TEST(FeatureDetectionParallel, ReversedWindingWithNormalFilteringMatchesSerialAtAllWorkerCounts) {
    const Mesh mesh = makeReversedWindingFilterFixture();
    const manumesh::common::MeshEdgeInfoMap edgeInfo = manumesh::common::buildMeshEdgeInfo(mesh);
    const std::vector<char> windingFlips = manumesh::common::harmonizeFaceWindings(mesh, edgeInfo);
    EXPECT_GT(std::count(windingFlips.begin(), windingFlips.end(), static_cast<char>(1)), 0);

    feature::FeatureOptions options;
    options.useNormalTensorFeatures = false;
    options.normalFilter = feature::FeatureNormalFilterOptions{true, 3, 25.0, 120.0, 0.45};
    const feature::FeatureDetector detector(options);
    const feature::FeatureAnalysis serial = detector.analyze(mesh);

    for (const int maxConcurrency : {0, 1, 2, 4, 8}) {
        ExecutionOptions parallel;
        parallel.mode = ExecutionMode::Parallel;
        parallel.maxConcurrency = maxConcurrency;
        parallel.minItemsPerTask = 1;

        const feature::FeatureAnalysis parallelResult = detector.analyze(mesh, parallel);
        expectFeatureAnalysisEquivalent(serial, parallelResult);
        EXPECT_EQ(0, parallelResult.inconsistentWindingEdges);
        EXPECT_EQ(3, parallelResult.normalFilter.iterationsCompleted);
    }
}

TEST(FeatureDetectionParallel, PrimitiveComponentFitsCommitInStableOrder) {
    const PrimitiveComponentFixture fixture = makePrimitiveComponentFixture(24, 32);
    feature::FeatureOptions options;
    options.minFeatureLoopVertices = 12;
    options.circleFitRelativeThreshold = 0.01;

    feature::FeatureAnalysis serial;
    serial.vertices.assign(fixture.mesh.vertices.size(), feature::VertexFeature{});
    serial.graph.vertices.assign(fixture.mesh.vertices.size(), feature::FeatureGraphVertex{});
    // The first component has already been claimed by an earlier recovery
    // stage and must remain excluded from primitive recovery.
    serial.vertices.front().circular = true;

    int serialLoopId = 11;
    manumesh::common::parallel::RangeExecutionOptions serialExecution;
    serialExecution.enabled = false;
    serialExecution.grainSize = 1;
    feature::detector_detail::recoverPrimitiveComponents(
        fixture.mesh, options, fixture.trace, serial, serialLoopId, serialExecution
    );
    ASSERT_EQ(23u, serial.loops.size());
    for (std::size_t index = 0; index < serial.loops.size(); ++index) {
        EXPECT_EQ(11 + static_cast<int>(index), serial.loops[index].id);
        EXPECT_EQ(32u, serial.loops[index].vertices.size());
        EXPECT_EQ(feature::FeaturePrimitiveType::Circle, serial.loops[index].primitive);
        EXPECT_NEAR(serial.loops[index].center.x(), 4.0 * static_cast<double>(index + 1), 1e-10);
    }

    for (const int maxConcurrency : {0, 1, 2, 4, 8}) {
        feature::FeatureAnalysis parallelResult;
        parallelResult.vertices.assign(fixture.mesh.vertices.size(), feature::VertexFeature{});
        parallelResult.graph.vertices.assign(fixture.mesh.vertices.size(), feature::FeatureGraphVertex{});
        parallelResult.vertices.front().circular = true;
        int parallelLoopId = 11;
        manumesh::common::parallel::RangeExecutionOptions parallelExecution;
        parallelExecution.enabled = true;
        parallelExecution.maxConcurrency = maxConcurrency;
        parallelExecution.grainSize = 1;

        feature::detector_detail::recoverPrimitiveComponents(
            fixture.mesh, options, fixture.trace, parallelResult, parallelLoopId, parallelExecution
        );
        expectFeatureAnalysisEquivalent(serial, parallelResult);
        EXPECT_EQ(serialLoopId, parallelLoopId);
    }
}
