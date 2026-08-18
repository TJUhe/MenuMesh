/**
 * @file tests/unit/simplification/simplification_feature_protection_tests.cpp
 * @brief 验证特征约束图、合成恢复边和曲线保护策略。
 * @ingroup manumesh_tests
 */

#include "SimplificationTestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/PlainSimplifier.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "core/PlainMesh.h"
#include "simplification/detail/FeatureConstraints.h"
#include "simplification/detail/FeatureGuidance.h"
#include "simplification/detail/SimplificationPolicies.h"

#include "core/Filesystem.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using manumesh::test::countCircularLoops;
using manumesh::test::loadExternalMesh;
using manumesh::test::loadExternalStl;
using manumesh::test::SimplifiedMesh;
using manumesh::test::simplifyWithReport;
using namespace manumesh::test::simplification;

namespace simplification = manumesh::simplification;

namespace {

manumesh::Mesh makeConstraintGraphMesh() {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(2.0, 0.0, 0.0),
        manumesh::Vec3(3.0, 2.0, 0.0),
        manumesh::Vec3(0.0, 2.0, 0.0),
    };
    mesh.faces = {
        manumesh::Face{{0, 1, 3}},
        manumesh::Face{{1, 2, 3}},
    };
    return mesh;
}

manumesh::Mesh makeRingConstraintGraphMesh(int ringVertexCount) {
    manumesh::Mesh mesh;
    mesh.vertices.reserve(static_cast<std::size_t>(ringVertexCount + 1));
    for (int vertex = 0; vertex < ringVertexCount; ++vertex) {
        const double angle =
            2.0 * 3.14159265358979323846 * static_cast<double>(vertex) / static_cast<double>(ringVertexCount);
        mesh.vertices.push_back(manumesh::Vec3(std::cos(angle), std::sin(angle), 0.0));
    }
    const int center = static_cast<int>(mesh.vertices.size());
    mesh.vertices.push_back(manumesh::Vec3::Zero());
    mesh.faces.reserve(static_cast<std::size_t>(ringVertexCount));
    for (int vertex = 0; vertex < ringVertexCount; ++vertex) {
        mesh.faces.push_back(manumesh::Face{{center, vertex, (vertex + 1) % ringVertexCount}});
    }
    return mesh;
}

manumesh::feature::FeatureGraphEdge evidenceEdge(int a, int b) {
    manumesh::feature::FeatureGraphEdge edge;
    edge.a = a;
    edge.b = b;
    edge.boundary = true;
    return edge;
}

manumesh::feature::FeatureAnalysis
makeLoopAnalysis(const std::vector<int>& loopVertices, int syntheticA = -1, int syntheticB = -1, int vertexCount = 4) {
    manumesh::feature::FeatureAnalysis analysis;
    analysis.vertices.resize(static_cast<std::size_t>(vertexCount));
    analysis.graph.vertices.resize(static_cast<std::size_t>(vertexCount));

    manumesh::feature::FeatureComponent component;
    component.id = 0;
    component.confidence = 0.83;
    analysis.components.push_back(component);

    manumesh::feature::FeatureLoop loop;
    loop.id = 0;
    loop.componentId = 0;
    loop.vertices = loopVertices;
    loop.edgeCount = static_cast<int>(loopVertices.size());
    loop.closed = true;
    loop.primitive = manumesh::feature::FeaturePrimitiveType::PolygonalLoop;
    loop.componentConfidence = component.confidence;
    analysis.loops.push_back(loop);

    for (int vertex : loopVertices) {
        analysis.vertices[static_cast<std::size_t>(vertex)].isFeature = true;
        analysis.vertices[static_cast<std::size_t>(vertex)].loopId = 0;
        analysis.vertices[static_cast<std::size_t>(vertex)].componentId = 0;
        analysis.vertices[static_cast<std::size_t>(vertex)].confidence = component.confidence;
        analysis.vertices[static_cast<std::size_t>(vertex)].primitive =
            manumesh::feature::FeaturePrimitiveType::PolygonalLoop;
        analysis.graph.vertices[static_cast<std::size_t>(vertex)].loopIds.push_back(0);
    }

    for (int pair = 0; pair < static_cast<int>(loopVertices.size()); ++pair) {
        const int a = loopVertices[static_cast<std::size_t>(pair)];
        const int b = loopVertices[static_cast<std::size_t>(pair + 1) % loopVertices.size()];
        manumesh::feature::FeatureGraphEdge edge = evidenceEdge(a, b);
        if ((a == syntheticA && b == syntheticB) || (a == syntheticB && b == syntheticA)) {
            edge.boundary = false;
            edge.cleanupBridge = true;
        }
        analysis.graph.edges.push_back(edge);
    }
    return analysis;
}

std::vector<simplification::VertexState> policyVertices(const simplification::FeatureGuidance& guidance) {
    std::vector<simplification::VertexState> vertices(guidance.vertices.size());
    for (int vertex = 0; vertex < static_cast<int>(vertices.size()); ++vertex) {
        vertices[static_cast<std::size_t>(vertex)].isFeature =
            guidance.vertices[static_cast<std::size_t>(vertex)].isFeature;
        vertices[static_cast<std::size_t>(vertex)].featureJunction =
            guidance.vertices[static_cast<std::size_t>(vertex)].junction;
        vertices[static_cast<std::size_t>(vertex)].featureLoopId =
            guidance.vertices[static_cast<std::size_t>(vertex)].loopId;
        vertices[static_cast<std::size_t>(vertex)].featurePrimitive =
            guidance.vertices[static_cast<std::size_t>(vertex)].primitive;
    }
    return vertices;
}

} // namespace

TEST(ManuMesh, SimplificationMapsNormalTensorAndWeakSpurOptions) {
    simplification::SimplifyOptions options;
    options.useNormalTensorFeatures = true;
    options.normalTensorFeatureThreshold = 0.021;
    options.normalTensorMinEdgeAlignment = 0.61;
    options.normalTensorSmoothingIterations = 1;
    options.normalTensorScaleCount = 4;
    options.normalTensorMinPersistentScales = 3;
    options.featureGraphMinWeakSpurStrength = 0.42;

    const manumesh::feature::FeatureOptions mapped = simplification::featureOptionsFromSimplifyOptions(options);

    EXPECT_TRUE(mapped.useNormalTensorFeatures);
    EXPECT_DOUBLE_EQ(options.normalTensorFeatureThreshold, mapped.normalTensorFeatureThreshold);
    EXPECT_DOUBLE_EQ(options.normalTensorMinEdgeAlignment, mapped.normalTensorMinEdgeAlignment);
    EXPECT_EQ(options.normalTensorSmoothingIterations, mapped.normalTensorSmoothingIterations);
    EXPECT_EQ(options.normalTensorScaleCount, mapped.normalTensorScaleCount);
    EXPECT_EQ(options.normalTensorMinPersistentScales, mapped.normalTensorMinPersistentScales);
    EXPECT_DOUBLE_EQ(options.featureGraphMinWeakSpurStrength, mapped.featureGraphMinWeakSpurStrength);
}

TEST(ManuMesh, ExplicitFeatureOptionsOverrideLegacyFlatFields) {
    simplification::SimplifyOptions options;
    options.featureAngleDeg = 12.0;
    options.useNormalTensorFeatures = false;
    options.minFeatureLoopVertices = 64;

    manumesh::feature::FeatureOptions composed;
    composed.featureAngleDeg = 73.0;
    composed.useNormalTensorFeatures = true;
    composed.minFeatureLoopVertices = 3;
    composed.surfacePatches.enabled = true;
    options.featureOptionsOverride = composed;

    const manumesh::feature::FeatureOptions resolved = simplification::featureOptionsFromSimplifyOptions(options);
    EXPECT_DOUBLE_EQ(73.0, resolved.featureAngleDeg);
    EXPECT_TRUE(resolved.useNormalTensorFeatures);
    EXPECT_EQ(3, resolved.minFeatureLoopVertices);
    EXPECT_TRUE(resolved.surfacePatches.enabled);

    const manumesh::feature::FeatureOptions floored = simplification::featureOptionsFromSimplifyOptions(options, 5);
    EXPECT_EQ(5, floored.minFeatureLoopVertices);
}

TEST(ManuMesh, FeatureConstraintPolicyUsesComposedMinimumLoopSize) {
    simplification::SimplifyOptions options;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::AllFeatureEdges;
    options.minFeatureLoopVertices = 64;

    manumesh::feature::FeatureOptions composed;
    composed.minFeatureLoopVertices = 3;
    options.featureOptionsOverride = composed;

    const manumesh::Mesh mesh = makeRingConstraintGraphMesh(6);
    const manumesh::feature::FeatureAnalysis analysis =
        makeLoopAnalysis({0, 1, 2, 3, 4, 5}, -1, -1, static_cast<int>(mesh.vertices.size()));
    simplification::FeatureDetectionPolicy detectionPolicy;
    detectionPolicy.enabled = true;
    const simplification::FeatureGuidance guidance =
        simplification::buildFeatureGuidance(mesh, detectionPolicy, &analysis);
    const std::vector<simplification::VertexState> vertices = policyVertices(guidance);
    const simplification::FeatureConstraintPolicy policy(options);
    const std::vector<int> inflatedLoopCount = {100};
    simplification::FeatureConstraintGraph contracted = guidance.constraints;
    EXPECT_EQ(6, contracted.protectedComponentVertexCount(0, 1));
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::None,
        policy.collapseRejectKind({{0, 1}, vertices, inflatedLoopCount, contracted})
    );
    ASSERT_TRUE(contracted.contractVertex(0, 1));
    EXPECT_EQ(5, contracted.protectedComponentVertexCount(0, 2));
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::Generic,
        policy.collapseRejectKind({{0, 2}, vertices, inflatedLoopCount, contracted})
    );

    options.featureOptionsOverride.reset();
    const simplification::FeatureConstraintPolicy legacyPolicy(options);
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::Generic,
        legacyPolicy.collapseRejectKind({{0, 1}, vertices, inflatedLoopCount, guidance.constraints})
    );
}

TEST(ManuMesh, SharedProtectedEdgeAppliesMinimumBudgetToEveryLoop) {
    const manumesh::Mesh mesh = makeRingConstraintGraphMesh(6);
    const manumesh::feature::FeatureAnalysis analysis =
        makeLoopAnalysis({0, 1, 2, 3, 4, 5}, -1, -1, static_cast<int>(mesh.vertices.size()));
    simplification::FeatureDetectionPolicy detectionPolicy;
    detectionPolicy.enabled = true;
    const simplification::FeatureGuidance guidance =
        simplification::buildFeatureGuidance(mesh, detectionPolicy, &analysis);
    simplification::FeatureConstraintGraph constraints = guidance.constraints;
    simplification::FeatureConstraintEdge* sharedEdge = constraints.findMutableEdge(0, 1);
    ASSERT_NE(nullptr, sharedEdge);
    sharedEdge->loopIds = {0, 1};

    simplification::SimplifyOptions options;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::AllFeatureEdges;
    options.maxFeatureCurveDeviationRatio = 0.01;
    manumesh::feature::FeatureOptions featureOptions;
    featureOptions.minFeatureLoopVertices = 3;
    options.featureOptionsOverride = featureOptions;
    const simplification::FeatureConstraintPolicy policy(options);
    const std::vector<simplification::VertexState> vertices = policyVertices(guidance);

    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::Generic,
        policy.collapseRejectKind({{0, 1}, vertices, {12, 3}, constraints})
    );
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::None,
        policy.collapseRejectKind({{0, 1}, vertices, {12, 4}, constraints})
    );
}

TEST(ManuMesh, DynamicConstraintRolesBlockCollapseEvenWhenVertexSnapshotIsStale) {
    const manumesh::Mesh mesh = makeConstraintGraphMesh();
    const manumesh::feature::FeatureAnalysis analysis = makeLoopAnalysis({0, 1, 2, 3});
    simplification::FeatureDetectionPolicy detectionPolicy;
    detectionPolicy.enabled = true;
    const simplification::FeatureGuidance guidance =
        simplification::buildFeatureGuidance(mesh, detectionPolicy, &analysis);
    const std::vector<simplification::VertexState> vertices = policyVertices(guidance);

    simplification::SimplifyOptions options;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::AllFeatureEdges;
    options.maxFeatureCurveDeviationRatio = 0.01;
    manumesh::feature::FeatureOptions featureOptions;
    featureOptions.minFeatureLoopVertices = 3;
    options.featureOptionsOverride = featureOptions;
    const simplification::FeatureConstraintPolicy policy(options);
    const std::vector<int> activeLoopCounts = {8};

    simplification::FeatureConstraintGraph constraints = guidance.constraints;
    ASSERT_FALSE(vertices[1].featureJunction);
    ASSERT_FALSE(constraints.vertices[1].junction);
    constraints.vertices[1].shared = true;
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::Generic,
        policy.collapseRejectKind({{1, 2}, vertices, activeLoopCounts, constraints})
    );

    constraints.vertices[1].shared = false;
    constraints.vertices[1].junction = true;
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::Generic,
        policy.collapseRejectKind({{1, 2}, vertices, activeLoopCounts, constraints})
    );

    constraints.vertices[1].junction = false;
    constraints.vertices[1].ambiguousJunction = true;
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::Generic,
        policy.collapseRejectKind({{1, 2}, vertices, activeLoopCounts, constraints})
    );
}

TEST(ManuMesh, FeatureCurveBudgetChecksEveryLoopOwnedByCollapseEdge) {
    std::vector<simplification::VertexState> vertices(2);
    for (simplification::VertexState& vertex : vertices) {
        vertex.isFeature = true;
        vertex.featureLoopId = 0;
    }

    simplification::FeatureCurveConstraint firstCurve;
    firstCurve.valid = true;
    firstCurve.primitive = simplification::FeatureCurveKind::PolygonalLoop;
    firstCurve.samples = {manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(1.0, 0.0, 0.0)};
    simplification::FeatureCurveConstraint secondCurve;
    secondCurve.valid = true;
    secondCurve.primitive = simplification::FeatureCurveKind::PolygonalLoop;
    secondCurve.samples = {manumesh::Vec3(10.0, 0.0, 0.0), manumesh::Vec3(11.0, 0.0, 0.0)};
    const std::vector<simplification::FeatureCurveConstraint> curves = {firstCurve, secondCurve};
    const std::vector<simplification::FeaturePrimitiveFit> primitiveFits;

    simplification::FeatureConstraintGraph constraints;
    constraints.vertices.resize(2);
    simplification::FeatureConstraintEdge edge;
    edge.a = 0;
    edge.b = 1;
    edge.loopIds = {0, 1};
    constraints.edges.push_back(edge);
    constraints.rebuildIndex();

    simplification::SimplifyOptions options;
    options.preserveFeatureCurves = true;
    options.maxFeatureCurveDeviationRatio = 0.1;
    const manumesh::Vec3 position(0.5, 0.0, 0.0);
    EXPECT_FALSE(
        simplification::featureCurveBudgetAllows(
            vertices[0],
            vertices[1],
            curves,
            primitiveFits,
            options,
            1.0,
            position,
            &constraints,
            simplification::CollapseEdge{0, 1}
        )
    );

    constraints.edges[0].loopIds = {0};
    EXPECT_TRUE(
        simplification::featureCurveBudgetAllows(
            vertices[0],
            vertices[1],
            curves,
            primitiveFits,
            options,
            1.0,
            position,
            &constraints,
            simplification::CollapseEdge{0, 1}
        )
    );

    constraints.vertices[0].loopIds = {0, 1};
    EXPECT_FALSE(
        simplification::featureCurveBudgetAllows(
            vertices[0],
            vertices[0],
            curves,
            primitiveFits,
            options,
            1.0,
            position,
            &constraints,
            simplification::CollapseEdge{0, 0}
        )
    );
    constraints.vertices[0].loopIds = {0};
    EXPECT_TRUE(
        simplification::featureCurveBudgetAllows(
            vertices[0],
            vertices[0],
            curves,
            primitiveFits,
            options,
            1.0,
            position,
            &constraints,
            simplification::CollapseEdge{0, 0}
        )
    );
}

TEST(ManuMesh, FeatureCurveBudgetUsesThePrimitiveFitOwnedByEachLoop) {
    std::vector<simplification::VertexState> vertices(2);
    for (simplification::VertexState& vertex : vertices) {
        vertex.isFeature = true;
        vertex.circularFeature = true;
        vertex.featurePrimitive = simplification::FeatureCurveKind::Circle;
        vertex.featureLoopId = 0;
        vertex.primitiveFitId = 0;
        vertex.p = manumesh::Vec3(1.0, 0.0, 0.0);
    }

    std::vector<simplification::FeaturePrimitiveFit> primitiveFits(2);
    primitiveFits[0].circleCenter = manumesh::Vec3::Zero();
    primitiveFits[0].circleRadius = 1.0;
    primitiveFits[1].circleCenter = manumesh::Vec3(10.0, 0.0, 0.0);
    primitiveFits[1].circleRadius = 1.0;

    std::vector<simplification::FeatureCurveConstraint> curves(2);
    for (simplification::FeatureCurveConstraint& curve : curves) {
        curve.valid = true;
        curve.closed = true;
        curve.primitive = simplification::FeatureCurveKind::Circle;
    }
    curves[0].primitiveFitId = 0;
    curves[1].primitiveFitId = 1;

    simplification::FeatureConstraintGraph constraints;
    constraints.vertices.resize(2);
    simplification::FeatureConstraintEdge edge;
    edge.a = 0;
    edge.b = 1;
    edge.loopIds = {0, 1};
    constraints.edges.push_back(edge);
    constraints.rebuildIndex();

    simplification::SimplifyOptions options;
    options.preserveFeatureCurves = true;
    options.maxFeatureCurveDeviationRatio = 0.01;
    EXPECT_FALSE(
        simplification::featureCurveBudgetAllows(
            vertices[0],
            vertices[1],
            curves,
            primitiveFits,
            options,
            1.0,
            manumesh::Vec3(1.0, 0.0, 0.0),
            &constraints,
            simplification::CollapseEdge{0, 1}
        )
    );

    constraints.edges[0].loopIds = {0};
    EXPECT_TRUE(
        simplification::featureCurveBudgetAllows(
            vertices[0],
            vertices[1],
            curves,
            primitiveFits,
            options,
            1.0,
            manumesh::Vec3(1.0, 0.0, 0.0),
            &constraints,
            simplification::CollapseEdge{0, 1}
        )
    );
}

TEST(ManuMesh, FeatureGuidanceStoresAnIndependentPrimitiveFitForEachAnalyticLoop) {
    const manumesh::Mesh mesh = makeRingConstraintGraphMesh(6);
    manumesh::feature::FeatureAnalysis analysis =
        makeLoopAnalysis({0, 1, 2, 3, 4, 5}, -1, -1, static_cast<int>(mesh.vertices.size()));
    manumesh::feature::FeatureLoop& loop = analysis.loops[0];
    loop.circular = true;
    loop.primitive = manumesh::feature::FeaturePrimitiveType::Circle;
    loop.center = manumesh::Vec3::Zero();
    loop.normal = manumesh::Vec3(0.0, 0.0, 1.0);
    loop.radius = 1.0;
    for (int vertexId : loop.vertices) {
        manumesh::feature::VertexFeature& vertex = analysis.vertices[static_cast<std::size_t>(vertexId)];
        vertex.circular = true;
        vertex.primitive = manumesh::feature::FeaturePrimitiveType::Circle;
        vertex.circleCenter = loop.center;
        vertex.circleNormal = loop.normal;
        vertex.circleRadius = loop.radius;
    }

    simplification::FeatureDetectionPolicy detectionPolicy;
    detectionPolicy.enabled = true;
    const simplification::FeatureGuidance guidance =
        simplification::buildFeatureGuidance(mesh, detectionPolicy, &analysis);

    ASSERT_EQ(1u, guidance.curves.size());
    const simplification::FeatureCurveConstraint& curve = guidance.curves[0];
    ASSERT_GE(curve.primitiveFitId, 0);
    ASSERT_LT(curve.primitiveFitId, static_cast<int>(guidance.primitiveFits.size()));
    const simplification::FeaturePrimitiveFit& fit =
        guidance.primitiveFits[static_cast<std::size_t>(curve.primitiveFitId)];
    EXPECT_EQ(simplification::FeatureCurveKind::Circle, curve.primitive);
    EXPECT_NEAR(0.0, (fit.circleCenter - loop.center).norm(), 1e-12);
    EXPECT_NEAR(0.0, (fit.circleNormal - loop.normal).norm(), 1e-12);
    EXPECT_DOUBLE_EQ(loop.radius, fit.circleRadius);
    for (int vertexId : loop.vertices) {
        EXPECT_NE(curve.primitiveFitId, guidance.vertices[static_cast<std::size_t>(vertexId)].primitiveFitId);
    }
}

TEST(ManuMesh, CanonicalFeatureConstraintGraphRejectsChordAndAllowsEvidenceEdge) {
    const manumesh::Mesh mesh = makeConstraintGraphMesh();
    manumesh::feature::FeatureAnalysis analysis = makeLoopAnalysis({0, 1, 2, 3});
    analysis.graph.vertices[0].loopIds.push_back(7);

    simplification::FeatureDetectionPolicy detectionPolicy;
    detectionPolicy.enabled = true;
    const simplification::FeatureGuidance guidance =
        simplification::buildFeatureGuidance(mesh, detectionPolicy, &analysis);
    const std::vector<simplification::VertexState> vertices = policyVertices(guidance);

    simplification::SimplifyOptions options;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::AllFeatureEdges;
    options.maxFeatureCurveDeviationRatio = 0.01;
    manumesh::feature::FeatureOptions featureOptions;
    featureOptions.minFeatureLoopVertices = 3;
    options.featureOptionsOverride = featureOptions;
    const simplification::FeatureConstraintPolicy policy(options);
    const std::vector<int> activeLoopCounts = {6};

    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::Generic,
        policy.collapseRejectKind({{1, 3}, vertices, activeLoopCounts, guidance.constraints})
    );
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::None,
        policy.collapseRejectKind({{1, 2}, vertices, activeLoopCounts, guidance.constraints})
    );

    const simplification::FeatureConstraintEdge* edge = guidance.constraints.findEdge(1, 2);
    ASSERT_NE(nullptr, edge);
    EXPECT_TRUE(edge->inputMeshEdge);
    EXPECT_TRUE(edge->protectedFeature);
    EXPECT_TRUE(edge->boundary);
    EXPECT_NEAR(0.83, edge->confidence, 1e-12);
    EXPECT_EQ(std::vector<int>({0}), guidance.vertices[0].loopIds);
    EXPECT_EQ(std::vector<int>({0, 7}), guidance.constraints.vertices[0].sourceLoopIds);
    EXPECT_EQ(std::vector<int>({0}), edge->loopIds);
}

TEST(ManuMesh, UntracedEvidenceEdgeStillCreatesHardFeatureProtection) {
    const manumesh::Mesh mesh = makeConstraintGraphMesh();
    manumesh::feature::FeatureAnalysis analysis;
    analysis.vertices.resize(mesh.vertices.size());
    analysis.graph.vertices.resize(mesh.vertices.size());
    manumesh::feature::FeatureGraphEdge edge;
    edge.a = 1;
    edge.b = 3;
    edge.dihedral = true;
    analysis.graph.edges.push_back(edge);
    analysis.untracedFeatureEdges = 1;

    simplification::FeatureDetectionPolicy detectionPolicy;
    detectionPolicy.enabled = true;
    const simplification::FeatureGuidance guidance =
        simplification::buildFeatureGuidance(mesh, detectionPolicy, &analysis);
    ASSERT_TRUE(guidance.constraints.isProtectedPathEdge(1, 3));
    EXPECT_TRUE(guidance.vertices[1].isFeature);
    EXPECT_TRUE(guidance.vertices[3].isFeature);
    EXPECT_GT(guidance.vertices[1].tangent.norm(), 0.0);
    EXPECT_EQ(2, std::count_if(guidance.vertices.begin(), guidance.vertices.end(), [](const auto& vertex) {
                  return vertex.isFeature;
              }));

    simplification::SimplifyOptions options;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::AllFeatureEdges;
    const simplification::FeatureConstraintPolicy policy(options);
    const std::vector<simplification::VertexState> vertices = policyVertices(guidance);
    const std::vector<int> noLoops;
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::Generic,
        policy.collapseRejectKind({{1, 3}, vertices, noLoops, guidance.constraints})
    );
}

TEST(ManuMesh, SyntheticRecoveryRolesDoNotFreezeTheProtectedSubgraph) {
    const manumesh::Mesh mesh = makeConstraintGraphMesh();
    manumesh::feature::FeatureAnalysis analysis = makeLoopAnalysis({0, 1, 3, 2}, 2, 0);
    analysis.graph.vertices[1].junction = true;
    analysis.graph.vertices[1].shared = true;
    analysis.graph.vertices[1].ambiguousJunction = true;
    analysis.graph.vertices[1].loopIds.push_back(7);

    simplification::FeatureDetectionPolicy detectionPolicy;
    detectionPolicy.enabled = true;
    const simplification::FeatureGuidance guidance =
        simplification::buildFeatureGuidance(mesh, detectionPolicy, &analysis);

    const simplification::FeatureConstraintVertex& canonical = guidance.constraints.vertices[1];
    EXPECT_FALSE(canonical.junction);
    EXPECT_FALSE(canonical.shared);
    EXPECT_FALSE(canonical.ambiguousJunction);
    EXPECT_EQ(std::vector<int>({0}), canonical.loopIds);
    EXPECT_TRUE(canonical.sourceJunction);
    EXPECT_TRUE(canonical.sourceShared);
    EXPECT_TRUE(canonical.sourceAmbiguousJunction);
    EXPECT_EQ(std::vector<int>({0, 7}), canonical.sourceLoopIds);
    EXPECT_FALSE(guidance.vertices[1].junction);

    simplification::SimplifyOptions options;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::AllFeatureEdges;
    options.maxFeatureCurveDeviationRatio = 0.01;
    manumesh::feature::FeatureOptions featureOptions;
    featureOptions.minFeatureLoopVertices = 3;
    options.featureOptionsOverride = featureOptions;
    const simplification::FeatureConstraintPolicy policy(options);
    const std::vector<simplification::VertexState> vertices = policyVertices(guidance);
    const std::vector<int> activeLoopCounts = {6};
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::None,
        policy.collapseRejectKind({{0, 1}, vertices, activeLoopCounts, guidance.constraints})
    );
}

TEST(ManuMesh, SyntheticOnlySourcePrimitiveCannotOverrideCanonicalPolygonalConstraint) {
    const manumesh::Mesh mesh = makeConstraintGraphMesh();
    manumesh::feature::FeatureAnalysis analysis = makeLoopAnalysis({0, 1, 3, 2}, 2, 0);
    manumesh::feature::VertexFeature& source = analysis.vertices[1];
    source.loopId = 7;
    source.componentId = 7;
    source.circular = true;
    source.primitive = manumesh::feature::FeaturePrimitiveType::Circle;
    source.circleCenter = manumesh::Vec3(50.0, 50.0, 0.0);
    source.circleNormal = manumesh::Vec3(0.0, 0.0, 1.0);
    source.circleRadius = 10.0;

    simplification::FeatureDetectionPolicy detectionPolicy;
    detectionPolicy.enabled = true;
    const simplification::FeatureGuidance guidance =
        simplification::buildFeatureGuidance(mesh, detectionPolicy, &analysis);

    const simplification::FeatureVertexGuidance& vertex = guidance.vertices[1];
    EXPECT_FALSE(vertex.circular);
    EXPECT_EQ(simplification::FeatureCurveKind::PolygonalLoop, vertex.primitive);
    EXPECT_EQ(0, vertex.loopId);
    EXPECT_EQ(std::vector<int>({0}), vertex.loopIds);
    EXPECT_EQ(-1, vertex.primitiveFitId);

    std::vector<simplification::VertexState> vertices = policyVertices(guidance);
    for (int vertexId = 0; vertexId < static_cast<int>(vertices.size()); ++vertexId) {
        vertices[static_cast<std::size_t>(vertexId)].p = mesh.vertices[static_cast<std::size_t>(vertexId)];
        vertices[static_cast<std::size_t>(vertexId)].circularFeature =
            guidance.vertices[static_cast<std::size_t>(vertexId)].circular;
    }

    simplification::SimplifyOptions options;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::AllFeatureEdges;
    const simplification::FeatureConstraintPolicy policy(options);
    std::vector<simplification::FeaturePrimitiveFit> primitiveFits;
    manumesh::Vec3 position(1.0, 0.5, 0.0);
    double expectedDistanceSquared = 0.0;
    const manumesh::Vec3 expected =
        simplification::closestPointOnFeatureCurve(guidance.curves[0], position, expectedDistanceSquared);
    ASSERT_TRUE(
        policy.projectPlacement({{0, 1}, vertices, guidance.curves, primitiveFits, guidance.constraints}, position)
    );
    EXPECT_NEAR(0.0, (position - expected).norm(), 1e-12);
    EXPECT_GT(expectedDistanceSquared, 0.0);
}

TEST(ManuMesh, SyntheticRecoverySplitCannotLoseItsLastProtectedSubsegmentEdge) {
    const manumesh::Mesh mesh = makeConstraintGraphMesh();
    manumesh::feature::FeatureAnalysis analysis = makeLoopAnalysis({0, 1, 3, 2}, 2, 0);
    for (manumesh::feature::FeatureGraphEdge& edge : analysis.graph.edges) {
        if (std::min(edge.a, edge.b) == 1 && std::max(edge.a, edge.b) == 3) {
            edge.boundary = false;
            edge.cleanupBridge = true;
        }
    }

    simplification::FeatureDetectionPolicy detectionPolicy;
    detectionPolicy.enabled = true;
    const simplification::FeatureGuidance guidance =
        simplification::buildFeatureGuidance(mesh, detectionPolicy, &analysis);
    ASSERT_TRUE(guidance.constraints.isOnlyProtectedEdgeInComponent(0, 1));
    ASSERT_TRUE(guidance.constraints.isOnlyProtectedEdgeInComponent(2, 3));

    simplification::SimplifyOptions options;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::AllFeatureEdges;
    manumesh::feature::FeatureOptions featureOptions;
    featureOptions.minFeatureLoopVertices = 3;
    options.featureOptionsOverride = featureOptions;
    const simplification::FeatureConstraintPolicy policy(options);
    const std::vector<simplification::VertexState> vertices = policyVertices(guidance);
    const std::vector<int> inflatedWholeLoopCount = {8};
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::Generic,
        policy.collapseRejectKind({{0, 1}, vertices, inflatedWholeLoopCount, guidance.constraints})
    );
}

TEST(ManuMesh, SyntheticRecoverySplitsMinimumBudgetByRealProtectedComponent) {
    const manumesh::Mesh mesh = makeRingConstraintGraphMesh(9);
    manumesh::feature::FeatureAnalysis analysis =
        makeLoopAnalysis({0, 1, 2, 3, 4, 5, 6, 7, 8}, 2, 3, static_cast<int>(mesh.vertices.size()));
    for (manumesh::feature::FeatureGraphEdge& edge : analysis.graph.edges) {
        if (std::min(edge.a, edge.b) == 0 && std::max(edge.a, edge.b) == 8) {
            edge.boundary = false;
            edge.cleanupBridge = true;
        }
    }

    simplification::FeatureDetectionPolicy detectionPolicy;
    detectionPolicy.enabled = true;
    const simplification::FeatureGuidance guidance =
        simplification::buildFeatureGuidance(mesh, detectionPolicy, &analysis);
    ASSERT_TRUE(guidance.constraints.isSyntheticRecoveryEdge(2, 3));
    ASSERT_TRUE(guidance.constraints.isSyntheticRecoveryEdge(8, 0));
    EXPECT_EQ(3, guidance.constraints.protectedComponentVertexCount(0, 1));
    EXPECT_EQ(6, guidance.constraints.protectedComponentVertexCount(4, 5));

    simplification::SimplifyOptions options;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::AllFeatureEdges;
    manumesh::feature::FeatureOptions featureOptions;
    featureOptions.minFeatureLoopVertices = 3;
    options.featureOptionsOverride = featureOptions;
    const simplification::FeatureConstraintPolicy policy(options);
    const std::vector<simplification::VertexState> vertices = policyVertices(guidance);
    const std::vector<int> inflatedWholeLoopCount = {9};
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::Generic,
        policy.collapseRejectKind({{0, 1}, vertices, inflatedWholeLoopCount, guidance.constraints})
    );
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::None,
        policy.collapseRejectKind({{4, 5}, vertices, inflatedWholeLoopCount, guidance.constraints})
    );
}

TEST(ManuMesh, NonFeatureContractionFastPathDoesNotTouchConstraintGraph) {
    manumesh::Mesh mesh = makeRingConstraintGraphMesh(64);
    const int firstNonFeature = static_cast<int>(mesh.vertices.size());
    mesh.vertices.push_back(manumesh::Vec3(3.0, 0.0, 0.0));
    mesh.vertices.push_back(manumesh::Vec3(4.0, 0.0, 0.0));
    mesh.vertices.push_back(manumesh::Vec3(3.0, 1.0, 0.0));
    mesh.faces.push_back(manumesh::Face{{firstNonFeature, firstNonFeature + 1, firstNonFeature + 2}});

    std::vector<int> loopVertices(64);
    for (int vertex = 0; vertex < static_cast<int>(loopVertices.size()); ++vertex) {
        loopVertices[static_cast<std::size_t>(vertex)] = vertex;
    }
    const manumesh::feature::FeatureAnalysis analysis =
        makeLoopAnalysis(loopVertices, -1, -1, static_cast<int>(mesh.vertices.size()));
    simplification::FeatureConstraintGraph constraints = simplification::buildFeatureConstraintGraph(mesh, analysis);
    const std::size_t edgeCount = constraints.edges.size();
    const std::vector<int> neighbors = constraints.protectedNeighbors(0);
    const int componentVertexCount = constraints.protectedComponentVertexCount(0, 1);

    for (int repetition = 0; repetition < 256; ++repetition) {
        EXPECT_FALSE(constraints.contractVertex(firstNonFeature, firstNonFeature + 1));
    }
    EXPECT_EQ(edgeCount, constraints.edges.size());
    EXPECT_EQ(neighbors, constraints.protectedNeighbors(0));
    EXPECT_EQ(componentVertexCount, constraints.protectedComponentVertexCount(0, 1));
}

TEST(ManuMesh, SyntheticRecoveryBridgeIsNotCollapsibleOrProjectableGeometry) {
    const manumesh::Mesh mesh = makeConstraintGraphMesh();
    const manumesh::feature::FeatureAnalysis analysis = makeLoopAnalysis({0, 1, 3, 2}, 2, 0);
    simplification::FeatureDetectionPolicy detectionPolicy;
    detectionPolicy.enabled = true;
    const simplification::FeatureGuidance guidance =
        simplification::buildFeatureGuidance(mesh, detectionPolicy, &analysis);

    ASSERT_TRUE(guidance.constraints.isSyntheticRecoveryEdge(0, 2));
    EXPECT_FALSE(guidance.constraints.isProtectedPathEdge(0, 2));
    ASSERT_EQ(1u, guidance.curves.size());
    EXPECT_FALSE(guidance.curves[0].closed);
    ASSERT_EQ(3u, guidance.curves[0].segments.size());

    double distanceSquared = 0.0;
    const manumesh::Vec3 bridgeMidpoint = 0.5 * (mesh.vertices[0] + mesh.vertices[2]);
    simplification::closestPointOnFeatureCurve(guidance.curves[0], bridgeMidpoint, distanceSquared);
    EXPECT_GT(distanceSquared, 1e-4);

    simplification::SimplifyOptions options;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::AllFeatureEdges;
    const simplification::FeatureConstraintPolicy policy(options);
    const std::vector<simplification::VertexState> vertices = policyVertices(guidance);
    const std::vector<int> activeLoopCounts = {4};
    EXPECT_EQ(
        simplification::FeatureCollapseRejectKind::Generic,
        policy.collapseRejectKind({{0, 2}, vertices, activeLoopCounts, guidance.constraints})
    );
}

TEST(ManuMesh, ContractedFeatureEdgeProducesOnlyPathBackedSuccessor) {
    const manumesh::Mesh mesh = makeConstraintGraphMesh();
    const manumesh::feature::FeatureAnalysis analysis = makeLoopAnalysis({0, 1, 2, 3});
    simplification::FeatureConstraintGraph constraints = simplification::buildFeatureConstraintGraph(mesh, analysis);

    ASSERT_TRUE(constraints.isProtectedPathEdge(0, 1));
    ASSERT_TRUE(constraints.isProtectedPathEdge(1, 2));
    constraints.contractVertex(0, 1);

    const simplification::FeatureConstraintEdge* successor = constraints.findEdge(0, 2);
    ASSERT_NE(nullptr, successor);
    EXPECT_TRUE(successor->protectedFeature);
    EXPECT_TRUE(successor->pathBacked);
    EXPECT_FALSE(successor->inputMeshEdge);
    EXPECT_FALSE(successor->syntheticRecovery);
    EXPECT_EQ(std::vector<int>({0}), successor->loopIds);
}

TEST(ManuMesh, StrictPolygonalFeatureProtectionRejectsChordPlacement) {
    const manumesh::Mesh input = makePolygonalFeatureChordMesh();

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
    options.targetFaces = 1;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = manumesh::simplification::FeatureProtectionMode::AllFeatureEdges;
    options.useNormalTensorFeatures = false;
    options.featureAngleDeg = 179.0;
    options.circleFitRelativeThreshold = 0.0;
    options.ellipseFitRelativeThreshold = 0.0;
    options.minFeatureLoopVertices = 3;
    options.maxFeatureCurveDeviationRatio = 1e-9;
    options.maxNormalDeviationDeg = 180.0;
    options.minTriangleQuality = 0.0;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_GT(result.report.featureLoops, 0);
    EXPECT_GT(result.report.featureRejectedCollapses, 0);
    EXPECT_GT(result.report.genericFeatureRejectedCollapses, 0);
    EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::RejectionLimit, result.report.terminationReason);
    EXPECT_EQ(
        result.report.rejectedCollapses,
        result.report.topologyRejectedCollapses + result.report.normalFlipRejectedCollapses +
            result.report.qualityRejectedCollapses + result.report.boundaryRejectedCollapses +
            result.report.selfIntersectionRejectedCollapses + result.report.curveBudgetRejectedCollapses +
            result.report.errorRejectedCollapses + result.report.featureRejectedCollapses
    );
}

TEST(ManuMesh, PrimitiveModeKeepsPolygonalFeaturesSoftByDefault) {
    const manumesh::Mesh input = manumesh::generateCubeGrid(4, 1.0);

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.35);
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = manumesh::simplification::FeatureProtectionMode::PrimitiveCurves;
    options.useNormalTensorFeatures = false;
    options.featureAngleDeg = 25.0;
    options.minFeatureLoopVertices = 4;
    options.maxNormalDeviationDeg = 180.0;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_GT(result.report.featureLoops, 0);
    EXPECT_GT(result.report.featureVertices, 0);
    EXPECT_EQ(0, result.report.genericFeatureRejectedCollapses);
}

TEST(ManuMesh, ReportsFeatureLoopsOnCylinderCreases) {
    const manumesh::Mesh input = manumesh::generateCylinderGrid(32, 4, 1.0, 2.0);
    manumesh::feature::FeatureOptions options;
    options.featureAngleDeg = 30.0;

    const manumesh::feature::FeatureAnalysis features = manumesh::feature::detectFeatureCurves(input, options);

    EXPECT_EQ(64, features.featureEdges);
    EXPECT_EQ(64, features.dihedralFeatureEdges);
    EXPECT_EQ(0, features.boundaryFeatureEdges);
    ASSERT_EQ(2u, features.loops.size());
    for (const manumesh::feature::FeatureLoop& loop : features.loops) {
        EXPECT_TRUE(loop.closed);
        EXPECT_TRUE(loop.circular);
        EXPECT_EQ(32u, loop.vertices.size());
        EXPECT_NEAR(1.0, loop.radius, 1e-9);
        EXPECT_NEAR(1.0, std::abs(loop.center.z()), 1e-9);
    }
}

TEST(ManuMesh, MeasuresCircularFeatureLoopAgainstDetectedCircle) {
    const manumesh::Mesh input = manumesh::generateCylinderGrid(32, 4, 1.0, 2.0);
    const manumesh::feature::FeatureAnalysis features =
        manumesh::feature::detectFeatureCurves(input, circularFeatureOptions());
    ASSERT_GT(countCircularLoops(features), 0);

    const auto loopIt =
        std::find_if(features.loops.begin(), features.loops.end(), [](const manumesh::feature::FeatureLoop& loop) {
            return loop.circular;
        });
    ASSERT_NE(loopIt, features.loops.end());

    const manumesh::feature::DirectionalCurveError error =
        manumesh::feature::measureLoopAgainstCircle(input, *loopIt, loopIt->center, loopIt->normal, loopIt->radius);

    EXPECT_EQ(error.samples, static_cast<int>(loopIt->vertices.size()));
    EXPECT_NEAR(error.radialRms, loopIt->rmsRadialError, 1e-10);
    EXPECT_NEAR(error.planeRms, loopIt->rmsPlaneError, 1e-10);
    EXPECT_LT(error.radialMax, 1e-10);
    EXPECT_LT(error.planeMax, 1e-10);
}
