/**
 * @file tests/unit/simplification/simplification_parallel_tests.cpp
 * @brief 验证 QEM 初始化并行化不改变动态拓扑提交结果。
 */

#include "TestSupport.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"

#include <gtest/gtest.h>

#include <cstddef>

namespace {

using manumesh::ExecutionMode;
using manumesh::ExecutionOptions;
using manumesh::Mesh;
using manumesh::Vec3;
using manumesh::simplification::SimplifyOptions;
using manumesh::simplification::SimplifyReport;

ExecutionOptions parallelOptions() {
    ExecutionOptions options;
    options.mode = ExecutionMode::Parallel;
    options.maxConcurrency = 4;
    options.minItemsPerTask = 32;
    return options;
}

void expectMeshEqual(const Mesh& first, const Mesh& second) {
    ASSERT_EQ(first.vertices.size(), second.vertices.size());
    ASSERT_EQ(first.faces.size(), second.faces.size());
    ASSERT_EQ(first.hasTextureCoordinates(), second.hasTextureCoordinates());
    for (std::size_t i = 0; i < first.vertices.size(); ++i) {
        const Vec3& a = first.vertices[i];
        const Vec3& b = second.vertices[i];
        EXPECT_DOUBLE_EQ(a.x(), b.x()) << "vertex=" << i;
        EXPECT_DOUBLE_EQ(a.y(), b.y()) << "vertex=" << i;
        EXPECT_DOUBLE_EQ(a.z(), b.z()) << "vertex=" << i;
    }
    for (std::size_t i = 0; i < first.faces.size(); ++i) {
        EXPECT_EQ(first.faces[i].v[0], second.faces[i].v[0]) << "face=" << i;
        EXPECT_EQ(first.faces[i].v[1], second.faces[i].v[1]) << "face=" << i;
        EXPECT_EQ(first.faces[i].v[2], second.faces[i].v[2]) << "face=" << i;
    }
}

void expectReportsEquivalent(const SimplifyReport& first, const SimplifyReport& second) {
    EXPECT_EQ(first.initialVertices, second.initialVertices);
    EXPECT_EQ(first.initialFaces, second.initialFaces);
    EXPECT_EQ(first.finalVertices, second.finalVertices);
    EXPECT_EQ(first.finalFaces, second.finalFaces);
    EXPECT_EQ(first.collapsedEdges, second.collapsedEdges);
    EXPECT_EQ(first.rejectedCollapses, second.rejectedCollapses);
    EXPECT_EQ(first.featureRejectedCollapses, second.featureRejectedCollapses);
    EXPECT_EQ(first.boundaryRejectedCollapses, second.boundaryRejectedCollapses);
    EXPECT_EQ(first.topologyRejectedCollapses, second.topologyRejectedCollapses);
    EXPECT_EQ(first.normalFlipRejectedCollapses, second.normalFlipRejectedCollapses);
    EXPECT_EQ(first.qualityRejectedCollapses, second.qualityRejectedCollapses);
    EXPECT_EQ(first.selfIntersectionRejectedCollapses, second.selfIntersectionRejectedCollapses);
    EXPECT_EQ(first.curveBudgetRejectedCollapses, second.curveBudgetRejectedCollapses);
    EXPECT_EQ(first.errorRejectedCollapses, second.errorRejectedCollapses);
    EXPECT_EQ(first.solverFallbacks, second.solverFallbacks);
    EXPECT_EQ(first.terminationReason, second.terminationReason);
    EXPECT_EQ(first.featureLoops, second.featureLoops);
    EXPECT_EQ(first.featureVertices, second.featureVertices);
    EXPECT_EQ(first.featureComponents, second.featureComponents);
    EXPECT_EQ(first.tracedFeatureEdges, second.tracedFeatureEdges);
    EXPECT_EQ(first.untracedFeatureEdges, second.untracedFeatureEdges);
    EXPECT_EQ(first.normalTensorFeatureEdges, second.normalTensorFeatureEdges);
    EXPECT_EQ(first.normalTensorScoredVertices, second.normalTensorScoredVertices);
    EXPECT_EQ(first.smoothCurvatureFeatureEdges, second.smoothCurvatureFeatureEdges);
    EXPECT_EQ(first.smoothCurvatureScoredVertices, second.smoothCurvatureScoredVertices);
    EXPECT_DOUBLE_EQ(first.minAppliedLineWeight, second.minAppliedLineWeight);
    EXPECT_DOUBLE_EQ(first.maxAppliedLineWeight, second.maxAppliedLineWeight);
    EXPECT_EQ(first.qualityRefinementIterationsCompleted, second.qualityRefinementIterationsCompleted);
    EXPECT_EQ(first.qualityRefinementAttemptedMoves, second.qualityRefinementAttemptedMoves);
    EXPECT_EQ(first.qualityRefinementAcceptedMoves, second.qualityRefinementAcceptedMoves);
}

} // namespace

TEST(SimplificationParallel, InitialCandidateParallelismPreservesQemOutputAndReport) {
    const Mesh input = manumesh::generateCylinderGrid(36, 12, 1.0, 2.0);
    SimplifyOptions options = manumesh::test::lineOptions(0.72);
    options.preserveBoundary = true;
    options.maxNormalDeviationDeg = 180.0;
    options.minTriangleQuality = 0.0;

    SimplifyReport serialReport;
    SimplifyReport parallelReport;
    ExecutionOptions serialOptions;
    const Mesh serial = manumesh::simplification::simplifyMesh(input, options, serialOptions, &serialReport);
    const Mesh parallel =
        manumesh::simplification::simplifyMesh(input, options, parallelOptions(), &parallelReport);

    expectMeshEqual(serial, parallel);
    expectReportsEquivalent(serialReport, parallelReport);
}

TEST(SimplificationParallel, ObjectAndFreeFunctionKeepExplicitExecutionContract) {
    const Mesh input = manumesh::generatePlaneGrid(32, 1.0, false);
    SimplifyOptions options = manumesh::test::lineOptions(0.65);
    options.preserveBoundary = false;
    options.maxNormalDeviationDeg = 180.0;

    manumesh::simplification::QEMSimplifier object(options);
    object.setExecutionOptions(parallelOptions());
    SimplifyReport objectReport;
    const Mesh objectResult = object.simplify(input, &objectReport);

    SimplifyReport freeReport;
    const Mesh freeResult = manumesh::simplification::simplifyMesh(input, options, parallelOptions(), &freeReport);

    expectMeshEqual(objectResult, freeResult);
    expectReportsEquivalent(objectReport, freeReport);
    EXPECT_EQ(ExecutionMode::Parallel, object.executionOptions().mode);
    EXPECT_EQ(4, object.executionOptions().maxConcurrency);
}
