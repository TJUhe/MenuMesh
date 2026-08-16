/**
 * @file tests/unit/feature_detection/feature_detection_api_tests.cpp
 * @brief 验证特征检测公共 API、结果来源指纹和分析一致性校验。
 * @ingroup manumesh_tests
 */

#include "AnalyticFixtures.h"
#include "FeatureDetectionTestSupport.h"
#include "core/MeshGenerators.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
namespace feature = manumesh::feature;
namespace analytic = manumesh::test::analytic;

using FeatureAnalysis = feature::FeatureAnalysis;
using FeatureDetector = feature::FeatureDetector;
using FeatureOptions = feature::FeatureOptions;
using FeaturePrimitiveType = feature::FeaturePrimitiveType;
using Mesh = manumesh::Mesh;
using Vec3 = manumesh::Vec3;
using manumesh::test::feature_detection::discreteOnlyOptions;

FeatureAnalysis makeSingleEdgeAnalysis(const Mesh& mesh, int a, int b, bool withEvidence = true) {
    FeatureAnalysis analysis;
    analysis.source = feature::featureAnalysisSource(mesh);
    analysis.vertices.assign(mesh.vertices.size(), feature::VertexFeature{});
    analysis.graph.vertices.assign(mesh.vertices.size(), feature::FeatureGraphVertex{});

    feature::FeatureGraphEdge edge;
    edge.a = a;
    edge.b = b;
    edge.dihedral = withEvidence;
    analysis.graph.edges.push_back(edge);

    const Vec3 tangent =
        (mesh.vertices[static_cast<std::size_t>(b)] - mesh.vertices[static_cast<std::size_t>(a)]).normalized();
    analysis.graph.vertices[static_cast<std::size_t>(a)].incidentEdges.push_back(0);
    analysis.graph.vertices[static_cast<std::size_t>(a)].branches.push_back({0, b, tangent, 0});
    analysis.graph.vertices[static_cast<std::size_t>(a)].endpoint = true;
    analysis.graph.vertices[static_cast<std::size_t>(b)].incidentEdges.push_back(0);
    analysis.graph.vertices[static_cast<std::size_t>(b)].branches.push_back({0, a, -tangent, 0});
    analysis.graph.vertices[static_cast<std::size_t>(b)].endpoint = true;
    analysis.graph.endpointVertices = {a, b};
    return analysis;
}

} // namespace

void rebuildDegreeTwoGraphViews(const Mesh& mesh, FeatureAnalysis& analysis) {
    analysis.graph.junctionVertices.clear();
    analysis.graph.sharedVertices.clear();
    analysis.graph.endpointVertices.clear();
    analysis.junctionBranchPairs = 0;
    analysis.ambiguousJunctions = 0;
    for (std::size_t vertexId = 0; vertexId < analysis.graph.vertices.size(); ++vertexId) {
        feature::FeatureGraphVertex& graphVertex = analysis.graph.vertices[vertexId];
        graphVertex.branches.clear();
        graphVertex.branchPairs.clear();
        graphVertex.ambiguousJunction = false;
        int activeEdges = 0;
        for (int edgeId : graphVertex.incidentEdges) {
            const feature::FeatureGraphEdge& edge = analysis.graph.edges[static_cast<std::size_t>(edgeId)];
            if (edge.removedByCleanup) {
                continue;
            }
            ++activeEdges;
            const int owner = static_cast<int>(vertexId);
            const int neighbor = edge.a == owner ? edge.b : edge.a;
            const Vec3 tangent =
                (mesh.vertices[static_cast<std::size_t>(neighbor)] - mesh.vertices[vertexId]).normalized();
            graphVertex.branches.push_back({edgeId, neighbor, tangent, edge.signedKind});
        }
        graphVertex.junction = activeEdges > 2;
        graphVertex.shared = graphVertex.loopIds.size() > 1;
        graphVertex.endpoint = activeEdges == 1;
        if (graphVertex.junction) {
            analysis.graph.junctionVertices.push_back(static_cast<int>(vertexId));
        }
        if (graphVertex.shared) {
            analysis.graph.sharedVertices.push_back(static_cast<int>(vertexId));
        }
        if (graphVertex.endpoint) {
            analysis.graph.endpointVertices.push_back(static_cast<int>(vertexId));
        }
    }
}
TEST(FeatureDetection, FeatureNamespaceApiIsProjectScoped) {
    static_assert(
        std::is_same<FeatureAnalysis, manumesh::feature::FeatureAnalysis>::value,
        "FeatureAnalysis namespace alias changed"
    );
    static_assert(
        std::is_same<FeatureDetector, manumesh::feature::FeatureDetector>::value,
        "FeatureDetector namespace alias changed"
    );
    static_assert(
        std::is_same<FeaturePrimitiveType, manumesh::feature::FeaturePrimitiveType>::value,
        "FeaturePrimitiveType namespace alias changed"
    );

    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};

    FeatureOptions options = discreteOnlyOptions();
    options.minFeatureLoopVertices = 3;
    FeatureDetector detector(options);

    const FeatureAnalysis direct = feature::detectFeatureCurves(mesh, options);
    const FeatureAnalysis objectResult = detector.analyze(mesh);
    const FeatureAnalysis projectScoped = manumesh::feature::detectFeatureCurves(mesh, options);

    EXPECT_EQ(direct.featureEdges, objectResult.featureEdges);
    EXPECT_EQ(direct.loops.size(), projectScoped.loops.size());
    EXPECT_EQ("circle", feature::toString(FeaturePrimitiveType::Circle));
}

TEST(FeatureDetection, FeatureDetectorObjectStoresOptions) {
    FeatureOptions options = discreteOnlyOptions();
    options.minFeatureLoopVertices = 3;
    FeatureDetector detector(options);
    EXPECT_EQ(3, detector.options().minFeatureLoopVertices);

    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};
    const FeatureAnalysis first = detector.analyze(mesh);
    EXPECT_EQ(3, first.featureEdges);

    options.minFeatureLoopVertices = 100;
    detector.setOptions(options);
    EXPECT_EQ(100, detector.options().minFeatureLoopVertices);
    const FeatureAnalysis second = detector.analyze(mesh);
    const FeatureAnalysis direct = feature::detectFeatureCurves(mesh, options);
    EXPECT_EQ(first.featureEdges, second.featureEdges);
    EXPECT_EQ(direct.loops.size(), second.loops.size());
    EXPECT_EQ(direct.featureEdges, second.featureEdges);
}

TEST(FeatureDetection, FeatureDetectorCopiesAndMovesPimplOptions) {
    static_assert(
        std::is_nothrow_move_constructible<FeatureDetector>::value,
        "FeatureDetector move construction must remain noexcept"
    );
    static_assert(
        std::is_nothrow_move_assignable<FeatureDetector>::value, "FeatureDetector move assignment must remain noexcept"
    );

    FeatureOptions originalOptions = discreteOnlyOptions();
    originalOptions.featureAngleDeg = 25.0;
    originalOptions.minFeatureLoopVertices = 5;
    FeatureDetector original(originalOptions);

    FeatureDetector copied(original);
    FeatureOptions changedOptions = originalOptions;
    changedOptions.featureAngleDeg = 70.0;
    changedOptions.minFeatureLoopVertices = 100;
    original.setOptions(changedOptions);

    EXPECT_DOUBLE_EQ(25.0, copied.options().featureAngleDeg);
    EXPECT_EQ(5, copied.options().minFeatureLoopVertices);
    EXPECT_DOUBLE_EQ(70.0, original.options().featureAngleDeg);

    FeatureDetector assigned;
    assigned = copied;
    changedOptions.featureAngleDeg = 35.0;
    copied.setOptions(changedOptions);

    EXPECT_DOUBLE_EQ(25.0, assigned.options().featureAngleDeg);
    EXPECT_EQ(5, assigned.options().minFeatureLoopVertices);
    EXPECT_DOUBLE_EQ(35.0, copied.options().featureAngleDeg);

    FeatureDetector moved(std::move(assigned));
    EXPECT_DOUBLE_EQ(25.0, moved.options().featureAngleDeg);
    EXPECT_EQ(5, moved.options().minFeatureLoopVertices);
    EXPECT_DOUBLE_EQ(FeatureOptions{}.featureAngleDeg, assigned.options().featureAngleDeg);
    EXPECT_NO_THROW(assigned.analyze(Mesh{}));

    FeatureDetector moveAssigned;
    moveAssigned = std::move(moved);
    EXPECT_DOUBLE_EQ(25.0, moveAssigned.options().featureAngleDeg);
    EXPECT_EQ(5, moveAssigned.options().minFeatureLoopVertices);
    EXPECT_DOUBLE_EQ(FeatureOptions{}.featureAngleDeg, moved.options().featureAngleDeg);

    moved.setOptions(originalOptions);
    EXPECT_DOUBLE_EQ(25.0, moved.options().featureAngleDeg);
    EXPECT_NO_THROW(moved.analyze(Mesh{}));
}

TEST(FeatureDetection, RejectsInvalidOptionsAndMeshes) {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};

    FeatureOptions invalidOptions = discreteOnlyOptions();
    invalidOptions.featureAngleDeg = std::numeric_limits<double>::infinity();
    EXPECT_THROW(feature::detectFeatureCurves(mesh, invalidOptions), std::invalid_argument);

    invalidOptions = discreteOnlyOptions();
    invalidOptions.minFeatureLoopVertices = 2;
    EXPECT_THROW((void)FeatureDetector{invalidOptions}, std::invalid_argument);

    FeatureDetector detector(discreteOnlyOptions());
    const double originalAngle = detector.options().featureAngleDeg;
    EXPECT_THROW(detector.setOptions(invalidOptions), std::invalid_argument);
    EXPECT_DOUBLE_EQ(originalAngle, detector.options().featureAngleDeg);

    Mesh invalidMesh = mesh;
    invalidMesh.faces = {{{0, 1, 9}}};
    EXPECT_THROW(feature::detectFeatureCurves(invalidMesh, discreteOnlyOptions()), std::invalid_argument);

    Mesh emptyFaceSet;
    emptyFaceSet.vertices = {Vec3(0.0, 0.0, 0.0)};
    EXPECT_NO_THROW({
        const FeatureAnalysis analysis = feature::detectFeatureCurves(emptyFaceSet, discreteOnlyOptions());
        EXPECT_TRUE(analysis.loops.empty());
    });
}

TEST(FeatureDetection, AnalysisSourceContractRejectsDifferentIndexedGeometry) {
    const Mesh mesh = manumesh::generatePlaneGrid(2, 1.0, false);
    const FeatureAnalysis analysis = feature::detectFeatureCurves(mesh, discreteOnlyOptions());

    EXPECT_EQ(mesh.vertices.size(), analysis.source.vertexCount);
    EXPECT_EQ(mesh.faces.size(), analysis.source.faceCount);
    EXPECT_NE(0u, analysis.source.topologyFingerprint);
    EXPECT_NE(0u, analysis.source.geometryFingerprint);
    EXPECT_NO_THROW(feature::validateFeatureAnalysis(mesh, analysis));

    Mesh differentTopology = mesh;
    std::swap(differentTopology.faces[0].v[0], differentTopology.faces[0].v[1]);
    EXPECT_THROW(feature::validateFeatureAnalysis(differentTopology, analysis), std::invalid_argument);

    Mesh differentGeometry = mesh;
    differentGeometry.vertices[0].z() = 0.125;
    EXPECT_THROW(feature::validateFeatureAnalysis(differentGeometry, analysis), std::invalid_argument);

    Mesh differentSize = manumesh::generatePlaneGrid(3, 1.0, false);
    EXPECT_THROW(feature::validateFeatureAnalysis(differentSize, analysis), std::invalid_argument);
}

TEST(FeatureDetection, AnalysisSourceFingerprintIsDeterministicAndIgnoresTextureData) {
    Mesh mesh = manumesh::generatePlaneGrid(2, 1.0, false);
    const feature::FeatureAnalysisSource first = feature::featureAnalysisSource(mesh);
    const feature::FeatureAnalysisSource second = feature::featureAnalysisSource(mesh);
    EXPECT_EQ(first.vertexCount, second.vertexCount);
    EXPECT_EQ(first.faceCount, second.faceCount);
    EXPECT_EQ(first.topologyFingerprint, second.topologyFingerprint);
    EXPECT_EQ(first.geometryFingerprint, second.geometryFingerprint);

    mesh.faceTexCoords.resize(mesh.faces.size());
    mesh.faceTexCoords.front().valid = true;
    mesh.faceTexCoords.front().uv = {manumesh::Vec2(0.0, 0.0), manumesh::Vec2(1.0, 0.0), manumesh::Vec2(0.0, 1.0)};
    const feature::FeatureAnalysisSource textured = feature::featureAnalysisSource(mesh);
    EXPECT_EQ(first.topologyFingerprint, textured.topologyFingerprint);
    EXPECT_EQ(first.geometryFingerprint, textured.geometryFingerprint);

    Mesh signedZero = mesh;
    signedZero.vertices[0].z() = -0.0;
    EXPECT_EQ(first.geometryFingerprint, feature::featureAnalysisSource(signedZero).geometryFingerprint);
}

TEST(FeatureDetection, AnalysisValidationRejectsCorruptIndicesAndPatchSegmentation) {
    const Mesh mesh = manumesh::generatePlaneGrid(2, 1.0, false);
    const FeatureAnalysis valid = feature::detectFeatureCurves(mesh, discreteOnlyOptions());

    FeatureAnalysis badLoopVertex = valid;
    ASSERT_FALSE(badLoopVertex.loops.empty());
    ASSERT_FALSE(badLoopVertex.loops.front().vertices.empty());
    badLoopVertex.loops.front().vertices.front() = static_cast<int>(mesh.vertices.size());
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badLoopVertex), std::invalid_argument);
    EXPECT_THROW(feature::segmentFeaturePatches(mesh, badLoopVertex), std::invalid_argument);

    FeatureAnalysis badLoopId = valid;
    badLoopId.loops.front().id = static_cast<int>(badLoopId.loops.size());
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badLoopId), std::invalid_argument);

    FeatureAnalysis badGraphEdge = valid;
    ASSERT_FALSE(badGraphEdge.graph.edges.empty());
    badGraphEdge.graph.edges.front().a = -1;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badGraphEdge), std::invalid_argument);

    FeatureAnalysis badTensorWeights = valid;
    badTensorWeights.normalTensorVertexWeights.assign(1, 0.25);
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badTensorWeights), std::invalid_argument);

    FeatureAnalysis invalidTensorWeight = valid;
    invalidTensorWeight.normalTensorVertexWeights.assign(mesh.vertices.size(), 0.0);
    invalidTensorWeight.normalTensorVertexWeights.front() = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, invalidTensorWeight), std::invalid_argument);

    FeatureAnalysis oversizedTensorWeight = valid;
    oversizedTensorWeight.normalTensorVertexWeights.assign(mesh.vertices.size(), 0.0);
    oversizedTensorWeight.normalTensorVertexWeights.front() = 1.0001;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, oversizedTensorWeight), std::invalid_argument);

    FeatureAnalysis segmented = valid;
    EXPECT_NO_THROW(feature::segmentFeaturePatches(mesh, segmented));
    EXPECT_EQ(mesh.faces.size(), segmented.facePatchIds.size());
    EXPECT_NO_THROW(feature::validateFeatureAnalysis(mesh, segmented));
}

TEST(FeatureDetection, AnalysisValidationRejectsUnsafeNumericAndPrimitivePayloads) {
    const Mesh mesh = manumesh::generatePlaneGrid(2, 1.0, false);
    const FeatureAnalysis valid = feature::detectFeatureCurves(mesh, discreteOnlyOptions());
    ASSERT_FALSE(valid.loops.empty());
    ASSERT_FALSE(valid.loops.front().vertices.empty());
    const std::size_t vertexId = static_cast<std::size_t>(valid.loops.front().vertices.front());

    FeatureAnalysis badConfidence = valid;
    badConfidence.vertices[vertexId].confidence = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badConfidence), std::invalid_argument);

    FeatureAnalysis badTangent = valid;
    badTangent.vertices[vertexId].tangent.x() = std::numeric_limits<double>::infinity();
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badTangent), std::invalid_argument);

    FeatureAnalysis badRadius = valid;
    badRadius.vertices[vertexId].circleRadius = -1.0;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badRadius), std::invalid_argument);

    FeatureAnalysis badLoopCenter = valid;
    badLoopCenter.loops.front().center.z() = std::numeric_limits<double>::infinity();
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badLoopCenter), std::invalid_argument);

    FeatureAnalysis badCircularMarker = valid;
    badCircularMarker.loops.front().circular = !badCircularMarker.loops.front().circular;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badCircularMarker), std::invalid_argument);

    FeatureAnalysis badComponentConfidence = valid;
    ASSERT_FALSE(badComponentConfidence.components.empty());
    badComponentConfidence.components.front().confidence = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badComponentConfidence), std::invalid_argument);
}

TEST(FeatureDetection, AnalysisValidationRejectsInconsistentComponentTopologyAndOwnership) {
    const Mesh mesh = manumesh::generatePlaneGrid(2, 1.0, false);
    const FeatureAnalysis valid = feature::detectFeatureCurves(mesh, discreteOnlyOptions());
    ASSERT_FALSE(valid.components.empty());
    ASSERT_FALSE(valid.components.front().vertices.empty());

    FeatureAnalysis badEdgeCount = valid;
    ++badEdgeCount.components.front().edgeCount;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badEdgeCount), std::invalid_argument);

    FeatureAnalysis badCycleRank = valid;
    ++badCycleRank.components.front().cycleRank;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badCycleRank), std::invalid_argument);

    FeatureAnalysis badEvidenceCount = valid;
    ++badEvidenceCount.components.front().boundaryEdges;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badEvidenceCount), std::invalid_argument);

    FeatureAnalysis missingVertexOwnership = valid;
    const int vertexId = missingVertexOwnership.components.front().vertices.front();
    missingVertexOwnership.vertices[static_cast<std::size_t>(vertexId)].componentId = -1;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, missingVertexOwnership), std::invalid_argument);

    FeatureAnalysis disconnectedMembership = valid;
    int unrelatedVertex = -1;
    for (int candidate = 0; candidate < static_cast<int>(mesh.vertices.size()); ++candidate) {
        if (std::find(
                disconnectedMembership.components.front().vertices.begin(),
                disconnectedMembership.components.front().vertices.end(),
                candidate
            ) == disconnectedMembership.components.front().vertices.end()) {
            unrelatedVertex = candidate;
            break;
        }
    }
    ASSERT_NE(-1, unrelatedVertex);
    disconnectedMembership.components.front().vertices.push_back(unrelatedVertex);
    disconnectedMembership.vertices[static_cast<std::size_t>(unrelatedVertex)].isFeature = true;
    disconnectedMembership.vertices[static_cast<std::size_t>(unrelatedVertex)].componentId =
        disconnectedMembership.components.front().id;
    disconnectedMembership.components.front().cycleRank = 0;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, disconnectedMembership), std::invalid_argument);
}

TEST(FeatureDetection, AnalysisValidationRejectsFeatureGraphOwnershipMismatch) {
    const Mesh mesh = manumesh::generatePlaneGrid(2, 1.0, false);
    const FeatureAnalysis valid = feature::detectFeatureCurves(mesh, discreteOnlyOptions());
    ASSERT_FALSE(valid.graph.edges.empty());

    const int edgeId = 0;
    const feature::FeatureGraphEdge& edge = valid.graph.edges[static_cast<std::size_t>(edgeId)];
    int nonEndpoint = -1;
    for (int vertexId = 0; vertexId < static_cast<int>(valid.graph.vertices.size()); ++vertexId) {
        if (vertexId != edge.a && vertexId != edge.b) {
            nonEndpoint = vertexId;
            break;
        }
    }
    ASSERT_NE(-1, nonEndpoint);

    FeatureAnalysis badIncidentOwner = valid;
    badIncidentOwner.graph.vertices[static_cast<std::size_t>(nonEndpoint)].incidentEdges.push_back(edgeId);
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badIncidentOwner), std::invalid_argument);

    int branchOwner = -1;
    for (int vertexId = 0; vertexId < static_cast<int>(valid.graph.vertices.size()); ++vertexId) {
        if (!valid.graph.vertices[static_cast<std::size_t>(vertexId)].branches.empty()) {
            branchOwner = vertexId;
            break;
        }
    }
    ASSERT_NE(-1, branchOwner);

    int nonIncidentEdge = -1;
    for (int candidate = 0; candidate < static_cast<int>(valid.graph.edges.size()); ++candidate) {
        const feature::FeatureGraphEdge& candidateEdge = valid.graph.edges[static_cast<std::size_t>(candidate)];
        if (candidateEdge.a != branchOwner && candidateEdge.b != branchOwner) {
            nonIncidentEdge = candidate;
            break;
        }
    }
    ASSERT_NE(-1, nonIncidentEdge);

    FeatureAnalysis badBranchEdge = valid;
    badBranchEdge.graph.vertices[static_cast<std::size_t>(branchOwner)].branches.front().edgeId = nonIncidentEdge;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badBranchEdge), std::invalid_argument);

    FeatureAnalysis badBranchNeighbor = valid;
    badBranchNeighbor.graph.vertices[static_cast<std::size_t>(branchOwner)].branches.front().neighborVertex =
        branchOwner;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badBranchNeighbor), std::invalid_argument);
}

TEST(FeatureDetection, AnalysisValidationRejectsInconsistentLoopMembership) {
    const Mesh mesh = manumesh::generatePlaneGrid(2, 1.0, false);
    const FeatureAnalysis valid = feature::detectFeatureCurves(mesh, discreteOnlyOptions());
    ASSERT_FALSE(valid.loops.empty());
    ASSERT_FALSE(valid.loops.front().vertices.empty());

    const int loopId = valid.loops.front().id;
    const int vertexId = valid.loops.front().vertices.front();

    FeatureAnalysis duplicateLoopVertex = valid;
    duplicateLoopVertex.loops.front().vertices.push_back(vertexId);
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, duplicateLoopVertex), std::invalid_argument);

    FeatureAnalysis badLoopOrder = valid;
    ASSERT_GE(badLoopOrder.loops.front().vertices.size(), 4u);
    std::swap(badLoopOrder.loops.front().vertices[1], badLoopOrder.loops.front().vertices[3]);
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badLoopOrder), std::invalid_argument);

    FeatureAnalysis badLoopEdgeCount = valid;
    ++badLoopEdgeCount.loops.front().edgeCount;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, badLoopEdgeCount), std::invalid_argument);

    FeatureAnalysis missingGraphMembership = valid;
    std::vector<int>& loopIds = missingGraphMembership.graph.vertices[static_cast<std::size_t>(vertexId)].loopIds;
    const auto membership = std::find(loopIds.begin(), loopIds.end(), loopId);
    ASSERT_NE(loopIds.end(), membership);
    loopIds.erase(membership);
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, missingGraphMembership), std::invalid_argument);

    FeatureAnalysis duplicateGraphMembership = valid;
    duplicateGraphMembership.graph.vertices[static_cast<std::size_t>(vertexId)].loopIds.push_back(loopId);
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, duplicateGraphMembership), std::invalid_argument);

    FeatureAnalysis missingPrimaryLoop = valid;
    missingPrimaryLoop.vertices[static_cast<std::size_t>(vertexId)].loopId = -1;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, missingPrimaryLoop), std::invalid_argument);

    FeatureAnalysis missingFeatureMarker = valid;
    missingFeatureMarker.vertices[static_cast<std::size_t>(vertexId)].isFeature = false;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, missingFeatureMarker), std::invalid_argument);

    int unownedVertex = -1;
    for (int candidate = 0; candidate < static_cast<int>(valid.graph.vertices.size()); ++candidate) {
        const feature::FeatureGraphVertex& graphVertex = valid.graph.vertices[static_cast<std::size_t>(candidate)];
        const bool hasActiveIncidence =
            std::any_of(graphVertex.incidentEdges.begin(), graphVertex.incidentEdges.end(), [&](int edgeId) {
                return edgeId >= 0 && edgeId < static_cast<int>(valid.graph.edges.size()) &&
                       !valid.graph.edges[static_cast<std::size_t>(edgeId)].removedByCleanup;
            });
        if (graphVertex.loopIds.empty() && !hasActiveIncidence) {
            unownedVertex = candidate;
            break;
        }
    }
    ASSERT_NE(-1, unownedVertex);
    FeatureAnalysis untracedFeatureVertex = valid;
    untracedFeatureVertex.vertices[static_cast<std::size_t>(unownedVertex)].isFeature = true;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, untracedFeatureVertex), std::invalid_argument);
}

TEST(FeatureDetection, AnalysisValidationAllowsOnlyBoundedCircularRecoveryGaps) {
    constexpr int sampleCount = 8;
    constexpr double pi = 3.141592653589793238462643383279502884;
    Mesh mesh;
    for (int index = 0; index < sampleCount; ++index) {
        const double angle = 2.0 * pi * static_cast<double>(index) / static_cast<double>(sampleCount);
        mesh.vertices.emplace_back(std::cos(angle), std::sin(angle), 0.0);
    }
    const int center = static_cast<int>(mesh.vertices.size());
    mesh.vertices.emplace_back(0.0, 0.0, 0.0);
    for (int index = 0; index < sampleCount; ++index) {
        mesh.faces.push_back({{{index, (index + 1) % sampleCount, center}}});
    }

    FeatureOptions options = discreteOnlyOptions();
    options.minFeatureLoopVertices = sampleCount;
    const FeatureAnalysis valid = feature::detectFeatureCurves(mesh, options);
    const auto circular = std::find_if(valid.loops.begin(), valid.loops.end(), [=](const feature::FeatureLoop& loop) {
        return loop.circular && loop.vertices.size() == sampleCount;
    });
    ASSERT_NE(valid.loops.end(), circular);

    const auto markMissingLoopEdges = [&](FeatureAnalysis& analysis, int missingCount) {
        const feature::FeatureLoop& loop = analysis.loops[static_cast<std::size_t>(circular->id)];
        for (int index = 0; index < missingCount; ++index) {
            const int a = loop.vertices[static_cast<std::size_t>(index)];
            const int b = loop.vertices[static_cast<std::size_t>((index + 1) % sampleCount)];
            const auto edge = std::find_if(
                analysis.graph.edges.begin(), analysis.graph.edges.end(), [&](const feature::FeatureGraphEdge& value) {
                    return (value.a == a && value.b == b) || (value.a == b && value.b == a);
                }
            );
            ASSERT_NE(analysis.graph.edges.end(), edge);
            edge->removedByCleanup = true;
        }
        rebuildDegreeTwoGraphViews(mesh, analysis);
    };

    FeatureAnalysis boundedGap = valid;
    markMissingLoopEdges(boundedGap, 1);
    feature::FeatureComponent& boundedComponent =
        boundedGap.components[static_cast<std::size_t>(circular->componentId)];
    --boundedComponent.edgeCount;
    --boundedComponent.boundaryEdges;
    --boundedComponent.strongEvidenceEdges;
    boundedComponent.endpointVertices = 2;
    boundedComponent.cycleRank = 0;
    boundedComponent.closed = false;
    boundedComponent.closureRate = 0.25;
    boundedComponent.strongEvidenceRatio = 1.0;
    EXPECT_NO_THROW(feature::validateFeatureAnalysis(mesh, boundedGap));

    FeatureAnalysis excessiveGap = valid;
    markMissingLoopEdges(excessiveGap, 3);
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, excessiveGap), std::invalid_argument);
}

TEST(FeatureDetection, AnalysisValidationRejectsInconsistentGraphViews) {
    const Mesh mesh = manumesh::generatePlaneGrid(2, 1.0, false);
    const FeatureAnalysis valid = feature::detectFeatureCurves(mesh, discreteOnlyOptions());
    ASSERT_FALSE(valid.graph.edges.empty());

    const int edgeId = 0;
    const int endpoint = valid.graph.edges.front().a;
    const std::size_t endpointIndex = static_cast<std::size_t>(endpoint);

    FeatureAnalysis missingEndpointIncidence = valid;
    std::vector<int>& incidentEdges = missingEndpointIncidence.graph.vertices[endpointIndex].incidentEdges;
    const auto incident = std::find(incidentEdges.begin(), incidentEdges.end(), edgeId);
    ASSERT_NE(incidentEdges.end(), incident);
    incidentEdges.erase(incident);
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, missingEndpointIncidence), std::invalid_argument);

    FeatureAnalysis duplicateEndpointIncidence = valid;
    duplicateEndpointIncidence.graph.vertices[endpointIndex].incidentEdges.push_back(edgeId);
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, duplicateEndpointIncidence), std::invalid_argument);

    int branchOwner = -1;
    for (int vertexId = 0; vertexId < static_cast<int>(valid.graph.vertices.size()); ++vertexId) {
        if (!valid.graph.vertices[static_cast<std::size_t>(vertexId)].branches.empty()) {
            branchOwner = vertexId;
            break;
        }
    }
    ASSERT_NE(-1, branchOwner);
    FeatureAnalysis duplicateBranch = valid;
    duplicateBranch.graph.vertices[static_cast<std::size_t>(branchOwner)].branches.push_back(
        duplicateBranch.graph.vertices[static_cast<std::size_t>(branchOwner)].branches.front()
    );
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, duplicateBranch), std::invalid_argument);

    int nonJunction = -1;
    for (int vertexId = 0; vertexId < static_cast<int>(valid.graph.vertices.size()); ++vertexId) {
        if (!valid.graph.vertices[static_cast<std::size_t>(vertexId)].junction) {
            nonJunction = vertexId;
            break;
        }
    }
    ASSERT_NE(-1, nonJunction);
    FeatureAnalysis phantomJunction = valid;
    phantomJunction.graph.junctionVertices.push_back(nonJunction);
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, phantomJunction), std::invalid_argument);

    FeatureAnalysis staleJunctionCount = valid;
    ++staleJunctionCount.junctionBranchPairs;
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, staleJunctionCount), std::invalid_argument);
}

TEST(FeatureDetection, AnalysisValidationRejectsDuplicateAndFabricatedGraphEdges) {
    const Mesh mesh = manumesh::generatePlaneGrid(2, 1.0, false);
    const int realA = mesh.faces.front().v[0];
    const int realB = mesh.faces.front().v[1];
    const FeatureAnalysis singleRealEdge = makeSingleEdgeAnalysis(mesh, realA, realB);
    EXPECT_NO_THROW(feature::validateFeatureAnalysis(mesh, singleRealEdge));

    FeatureAnalysis duplicateEdge = singleRealEdge;
    duplicateEdge.graph.edges.push_back(duplicateEdge.graph.edges.front());
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, duplicateEdge), std::invalid_argument);

    FeatureAnalysis missingEvidence = makeSingleEdgeAnalysis(mesh, realA, realB, false);
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, missingEvidence), std::invalid_argument);

    const int nonMeshA = 0;
    const int nonMeshB = static_cast<int>(mesh.vertices.size()) - 1;
    FeatureAnalysis nonMeshEvidence = makeSingleEdgeAnalysis(mesh, nonMeshA, nonMeshB);
    EXPECT_THROW(feature::validateFeatureAnalysis(mesh, nonMeshEvidence), std::invalid_argument);
}

TEST(FeatureDetection, AnalysisValidationRejectsInconsistentPatchViews) {
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(16, 3, 1.0, 2.0, true);
    FeatureOptions options = discreteOnlyOptions();
    options.surfacePatches.enabled = true;
    options.surfacePatches.includeWeakEvidence = false;
    const FeatureAnalysis valid = feature::detectFeatureCurves(cylinder.mesh, options);
    ASSERT_EQ(cylinder.mesh.faces.size(), valid.facePatchIds.size());
    ASSERT_GT(valid.patches.size(), 1u);
    ASSERT_FALSE(valid.patchAdjacencies.empty());

    FeatureAnalysis badFaceCount = valid;
    ++badFaceCount.patches.front().faceCount;
    EXPECT_THROW(feature::validateFeatureAnalysis(cylinder.mesh, badFaceCount), std::invalid_argument);

    FeatureAnalysis badArea = valid;
    badArea.patches.front().area *= 1.1;
    EXPECT_THROW(feature::validateFeatureAnalysis(cylinder.mesh, badArea), std::invalid_argument);

    FeatureAnalysis badNormal = valid;
    badNormal.patches.front().normal = Vec3(1.0, 0.0, 0.0);
    EXPECT_THROW(feature::validateFeatureAnalysis(cylinder.mesh, badNormal), std::invalid_argument);

    FeatureAnalysis badBoundaryCount = valid;
    ++badBoundaryCount.patches.front().featureBoundaryEdges;
    EXPECT_THROW(feature::validateFeatureAnalysis(cylinder.mesh, badBoundaryCount), std::invalid_argument);

    FeatureAnalysis badClosedMarker = valid;
    badClosedMarker.patches.front().closed = !badClosedMarker.patches.front().closed;
    EXPECT_THROW(feature::validateFeatureAnalysis(cylinder.mesh, badClosedMarker), std::invalid_argument);

    FeatureAnalysis oneWayNeighbor = valid;
    const feature::FeaturePatchAdjacency adjacency = oneWayNeighbor.patchAdjacencies.front();
    std::vector<int>& reverseNeighbors =
        oneWayNeighbor.patches[static_cast<std::size_t>(adjacency.secondPatch)].neighboringPatches;
    const auto reverse = std::find(reverseNeighbors.begin(), reverseNeighbors.end(), adjacency.firstPatch);
    ASSERT_NE(reverseNeighbors.end(), reverse);
    reverseNeighbors.erase(reverse);
    EXPECT_THROW(feature::validateFeatureAnalysis(cylinder.mesh, oneWayNeighbor), std::invalid_argument);

    FeatureAnalysis duplicateNeighbor = valid;
    duplicateNeighbor.patches[static_cast<std::size_t>(adjacency.firstPatch)].neighboringPatches.push_back(
        adjacency.secondPatch
    );
    EXPECT_THROW(feature::validateFeatureAnalysis(cylinder.mesh, duplicateNeighbor), std::invalid_argument);

    FeatureAnalysis duplicateAdjacency = valid;
    duplicateAdjacency.patchAdjacencies.push_back(duplicateAdjacency.patchAdjacencies.front());
    EXPECT_THROW(feature::validateFeatureAnalysis(cylinder.mesh, duplicateAdjacency), std::invalid_argument);

    FeatureAnalysis badAdjacencyCount = valid;
    ++badAdjacencyCount.patchAdjacencies.front().featureEdges;
    EXPECT_THROW(feature::validateFeatureAnalysis(cylinder.mesh, badAdjacencyCount), std::invalid_argument);

    FeatureAnalysis missingAdjacency = valid;
    missingAdjacency.patchAdjacencies.erase(missingAdjacency.patchAdjacencies.begin());
    EXPECT_THROW(feature::validateFeatureAnalysis(cylinder.mesh, missingAdjacency), std::invalid_argument);
}
