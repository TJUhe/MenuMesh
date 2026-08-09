/**
 * @file tests/unit/feature_detection/feature_detection_robustness_tests.cpp
 * @brief 验证 ManuMesh 测试中的特征检测 稳健性测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#include "FeatureDetectionTestSupport.h"
#include "TestSupport.h"

#include "common/detail/MeshQueries.h"
#include "core/MeshGenerators.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

namespace feature = manumesh::feature;

using FeatureAnalysis = feature::FeatureAnalysis;
using FeatureOptions = feature::FeatureOptions;
using Mesh = manumesh::Mesh;
using Vec3 = manumesh::Vec3;
using manumesh::test::loadFixtureMesh;
using manumesh::test::feature_detection::discreteOnlyOptions;
using manumesh::test::feature_detection::makeBranchedCircularBoundaryMesh;
using manumesh::test::feature_detection::makeFragmentedCircleWithTensorRidgeMesh;

Mesh makeOutOfRangeIndexMesh() {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 9}}};
    return mesh;
}

Mesh makeNonFiniteVertexMesh() {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.0, std::numeric_limits<double>::quiet_NaN(), 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};
    return mesh;
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
Mesh makeFoldedFinMesh(double foldAngleDeg) {
    const double angle = foldAngleDeg * std::acos(-1.0) / 180.0;
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        Vec3(-1.0, 0.5, 0.0),
        Vec3(std::cos(angle), 0.5, std::sin(angle)),
    };
    mesh.faces = {
        {{0, 1, 2}},
        {{1, 0, 3}},
    };
    return mesh;
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
Mesh makeInconsistentWindingFlatMesh() {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        Vec3(-1.0, 0.5, 0.0),
        Vec3(1.0, 0.5, 0.0),
    };
    mesh.faces = {
        {{0, 1, 2}},
        {{0, 1, 3}},
    };
    return mesh;
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
Mesh makeMoebiusBandMesh(int segments) {
    Mesh mesh;
    const double radius = 2.0;
    const double halfWidth = 0.4;
    for (int i = 0; i < segments; ++i) {
        const double u = 2.0 * std::acos(-1.0) * static_cast<double>(i) / static_cast<double>(segments);
        const double half = 0.5 * u; // 该实现需保持边界条件，并保证结果具有确定性。
        for (double s : {-halfWidth, halfWidth}) {
            const double r = radius + s * std::cos(half);
            mesh.vertices.emplace_back(r * std::cos(u), r * std::sin(u), s * std::sin(half));
        }
    }
    for (int i = 0; i < segments; ++i) {
        const int a0 = 2 * i;
        const int a1 = 2 * i + 1;
        int b0 = 2 * ((i + 1) % segments);
        int b1 = b0 + 1;
        if (i + 1 == segments) {
            // 检查该步骤的边界条件，并确保结果保持确定性。
            std::swap(b0, b1);
        }
        mesh.faces.push_back({{a0, b0, b1}});
        mesh.faces.push_back({{a0, b1, a1}});
    }
    return mesh;
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
Mesh makeRectangularBipyramidMesh() {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.3),
        Vec3(0.0, 0.0, -0.3),
        Vec3(2.0, 0.3, 0.0),
        Vec3(-2.0, 0.3, 0.0),
        Vec3(-2.0, -0.3, 0.0),
        Vec3(2.0, -0.3, 0.0),
    };
    for (int i = 0; i < 4; ++i) {
        const int a = 2 + i;
        const int b = 2 + ((i + 1) % 4);
        mesh.faces.push_back({{0, b, a}});
        mesh.faces.push_back({{1, a, b}});
    }
    return mesh;
}

void expectIdenticalAnalyses(const FeatureAnalysis& lhs, const FeatureAnalysis& rhs) {
    EXPECT_EQ(lhs.featureEdges, rhs.featureEdges);
    EXPECT_EQ(lhs.tracedFeatureEdges, rhs.tracedFeatureEdges);
    EXPECT_EQ(lhs.untracedFeatureEdges, rhs.untracedFeatureEdges);
    EXPECT_EQ(lhs.boundaryFeatureEdges, rhs.boundaryFeatureEdges);
    EXPECT_EQ(lhs.dihedralFeatureEdges, rhs.dihedralFeatureEdges);
    EXPECT_EQ(lhs.normalTensorFeatureEdges, rhs.normalTensorFeatureEdges);
    EXPECT_EQ(lhs.graphCleanupBridgedGaps, rhs.graphCleanupBridgedGaps);
    EXPECT_EQ(lhs.graphCleanupRemovedSpurs, rhs.graphCleanupRemovedSpurs);
    EXPECT_EQ(lhs.graphCleanupMergedJunctions, rhs.graphCleanupMergedJunctions);
    EXPECT_EQ(lhs.graph.junctionVertices, rhs.graph.junctionVertices);
    EXPECT_EQ(lhs.graph.sharedVertices, rhs.graph.sharedVertices);
    EXPECT_EQ(lhs.graph.endpointVertices, rhs.graph.endpointVertices);

    ASSERT_EQ(lhs.graph.edges.size(), rhs.graph.edges.size());
    for (std::size_t i = 0; i < lhs.graph.edges.size(); ++i) {
        const feature::FeatureGraphEdge& le = lhs.graph.edges[i];
        const feature::FeatureGraphEdge& re = rhs.graph.edges[i];
        EXPECT_EQ(le.a, re.a);
        EXPECT_EQ(le.b, re.b);
        EXPECT_EQ(le.boundary, re.boundary);
        EXPECT_EQ(le.dihedral, re.dihedral);
        EXPECT_EQ(le.normalTensor, re.normalTensor);
        EXPECT_EQ(le.smoothCurvature, re.smoothCurvature);
        EXPECT_EQ(le.nonManifold, re.nonManifold);
        EXPECT_EQ(le.cleanupBridge, re.cleanupBridge);
        EXPECT_EQ(le.removedByCleanup, re.removedByCleanup);
        EXPECT_EQ(le.signedKind, re.signedKind);
    }

    ASSERT_EQ(lhs.loops.size(), rhs.loops.size());
    for (std::size_t i = 0; i < lhs.loops.size(); ++i) {
        const feature::FeatureLoop& ll = lhs.loops[i];
        const feature::FeatureLoop& rl = rhs.loops[i];
        EXPECT_EQ(ll.id, rl.id);
        EXPECT_EQ(ll.componentId, rl.componentId);
        EXPECT_EQ(ll.vertices, rl.vertices);
        EXPECT_EQ(ll.closed, rl.closed);
        EXPECT_EQ(ll.circular, rl.circular);
        EXPECT_EQ(ll.primitive, rl.primitive);
        EXPECT_EQ(ll.radius, rl.radius);
        EXPECT_EQ(ll.componentConfidence, rl.componentConfidence);
    }

    ASSERT_EQ(lhs.components.size(), rhs.components.size());
    for (std::size_t i = 0; i < lhs.components.size(); ++i) {
        EXPECT_EQ(lhs.components[i].vertices, rhs.components[i].vertices);
        EXPECT_EQ(lhs.components[i].edgeCount, rhs.components[i].edgeCount);
        EXPECT_EQ(lhs.components[i].junctionVertices, rhs.components[i].junctionVertices);
        EXPECT_EQ(lhs.components[i].endpointVertices, rhs.components[i].endpointVertices);
        EXPECT_EQ(lhs.components[i].confidence, rhs.components[i].confidence);
    }

    ASSERT_EQ(lhs.vertices.size(), rhs.vertices.size());
    for (std::size_t i = 0; i < lhs.vertices.size(); ++i) {
        EXPECT_EQ(lhs.vertices[i].isFeature, rhs.vertices[i].isFeature);
        EXPECT_EQ(lhs.vertices[i].junction, rhs.vertices[i].junction);
        EXPECT_EQ(lhs.vertices[i].loopId, rhs.vertices[i].loopId);
        EXPECT_EQ(lhs.vertices[i].componentId, rhs.vertices[i].componentId);
    }
}

} // 命名空间

TEST(FeatureDetection, PublicScoringApisRejectMalformedMeshes) {
    const Mesh badIndices = makeOutOfRangeIndexMesh();
    const Mesh badCoordinates = makeNonFiniteVertexMesh();

    EXPECT_THROW(feature::computeNormalTensorFeatures(badIndices), std::invalid_argument);
    EXPECT_THROW(
        feature::computeNormalTensorFeatures(badIndices, feature::NormalTensorOptions{}, 0.05), std::invalid_argument
    );
    EXPECT_THROW(feature::computeSmoothCurvatureFeatures(badIndices), std::invalid_argument);
    EXPECT_THROW(
        feature::computeSmoothCurvatureFeatures(badIndices, feature::SmoothCurvatureOptions{}, 0.05),
        std::invalid_argument
    );
    EXPECT_THROW(feature::computeNormalTensorFeatures(badCoordinates), std::invalid_argument);
    EXPECT_THROW(feature::computeSmoothCurvatureFeatures(badCoordinates), std::invalid_argument);

    // 检查该步骤的边界条件，并确保结果保持确定性。
    Mesh vertexOnly;
    vertexOnly.vertices = {Vec3(0.0, 0.0, 0.0)};
    EXPECT_NO_THROW(feature::computeNormalTensorFeatures(vertexOnly));
    EXPECT_NO_THROW(feature::computeSmoothCurvatureFeatures(vertexOnly));
}

TEST(FeatureDetection, RejectsScaleParametersAboveSupportedMaximum) {
    FeatureOptions options = discreteOnlyOptions();
    options.normalTensorScaleCount = 10;
    EXPECT_THROW(feature::validateFeatureOptions(options), std::invalid_argument);

    options = discreteOnlyOptions();
    options.smoothCurvatureScaleCount = 10;
    EXPECT_THROW(feature::validateFeatureOptions(options), std::invalid_argument);
    Mesh triangle;
    triangle.vertices = {Vec3(0.0, 0.0, 0.0), Vec3(1.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0)};
    triangle.faces = {{{0, 1, 2}}};
    EXPECT_THROW(feature::detectFeatureCurves(triangle, options), std::invalid_argument);

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    options = discreteOnlyOptions();
    options.smoothCurvatureMinPersistentScales = 8;
    EXPECT_THROW(feature::validateFeatureOptions(options), std::invalid_argument);

    options = discreteOnlyOptions();
    options.normalTensorScaleCount = 2;
    options.normalTensorMinPersistentScales = 3;
    EXPECT_THROW(feature::validateFeatureOptions(options), std::invalid_argument);

    options = discreteOnlyOptions();
    options.normalTensorSmoothingIterations = 9;
    EXPECT_THROW(feature::validateFeatureOptions(options), std::invalid_argument);

    options = discreteOnlyOptions();
    options.smoothCurvatureBaseNeighborhoodRings = 5;
    EXPECT_THROW(feature::validateFeatureOptions(options), std::invalid_argument);

    options = discreteOnlyOptions();
    options.smoothCurvatureRobustFitIterations = 5;
    EXPECT_THROW(feature::validateFeatureOptions(options), std::invalid_argument);

    // 检查该步骤的边界条件，并确保结果保持确定性。
    options = discreteOnlyOptions();
    options.normalTensorScaleCount = feature::kMaxNormalTensorScaleCount;
    options.normalTensorMinPersistentScales = feature::kMaxNormalTensorScaleCount;
    options.smoothCurvatureScaleCount = feature::kMaxSmoothCurvatureScaleCount;
    options.smoothCurvatureMinPersistentScales = feature::kMaxSmoothCurvatureScaleCount;
    EXPECT_NO_THROW(feature::validateFeatureOptions(options));
}

TEST(FeatureDetection, DetectsKnifeEdgeFoldBeyondNinetyDegrees) {
    const Mesh fin = makeFoldedFinMesh(150.0);

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 40.0;
    const FeatureAnalysis features = feature::detectFeatureCurves(fin, options);

    EXPECT_EQ(1, features.dihedralFeatureEdges);
    EXPECT_EQ(4, features.boundaryFeatureEdges);
    EXPECT_EQ(0, features.inconsistentWindingEdges);
    EXPECT_TRUE(features.vertices[0].isFeature);
    EXPECT_TRUE(features.vertices[1].isFeature);

    // 检查该步骤的边界条件，并确保结果保持确定性。
    FeatureOptions strict = options;
    strict.featureAngleDeg = 160.0;
    EXPECT_EQ(0, feature::detectFeatureCurves(fin, strict).dihedralFeatureEdges);
}

TEST(FeatureDetection, HarmonizesInconsistentWindingOnFlatSurface) {
    const Mesh flat = makeInconsistentWindingFlatMesh();

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 40.0;
    const FeatureAnalysis features = feature::detectFeatureCurves(flat, options);

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    EXPECT_EQ(0, features.dihedralFeatureEdges);
    EXPECT_EQ(4, features.boundaryFeatureEdges);
    EXPECT_EQ(0, features.inconsistentWindingEdges);
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(FeatureDetection, DetectsKnifeEdgeAcrossReversedWindingPatch) {
    Mesh fin = makeFoldedFinMesh(150.0);
    std::swap(fin.faces[1].v[0], fin.faces[1].v[1]); // {1,0,3} -> {0,1,3}

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 40.0;
    const FeatureAnalysis features = feature::detectFeatureCurves(fin, options);

    EXPECT_EQ(1, features.dihedralFeatureEdges);
    EXPECT_EQ(0, features.inconsistentWindingEdges);
    EXPECT_TRUE(features.vertices[0].isFeature);
    EXPECT_TRUE(features.vertices[1].isFeature);
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(FeatureDetection, MoebiusBandKeepsInconsistentWindingDiagnostic) {
    const Mesh band = makeMoebiusBandMesh(16);

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 40.0;
    const FeatureAnalysis features = feature::detectFeatureCurves(band, options);

    EXPECT_EQ(1, features.inconsistentWindingEdges);

    const FeatureAnalysis second = feature::detectFeatureCurves(band, options);
    EXPECT_EQ(features.inconsistentWindingEdges, second.inconsistentWindingEdges);
    EXPECT_EQ(features.dihedralFeatureEdges, second.dihedralFeatureEdges);
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(FeatureDetection, ReversedPatchYieldsIdenticalFeatureEdgeSet) {
    const Mesh original = loadFixtureMesh("feature_fixtures/boss_pocket_plate.obj");
    ASSERT_FALSE(original.empty());

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    const auto edgeInfo = [](const Mesh& mesh) {
        std::unordered_map<std::uint64_t, std::vector<int>> info;
        for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
            for (int e = 0; e < 3; ++e) {
                const int a = mesh.faces[fi].v[e];
                const int b = mesh.faces[fi].v[(e + 1) % 3];
                info[manumesh::common::meshEdgeKey(a, b)].push_back(fi);
            }
        }
        return info;
    };
    const auto info = edgeInfo(original);
    std::vector<char> inPatch(original.faces.size(), 0);
    std::vector<int> queue = {0};
    inPatch[0] = 1;
    int patchSize = 1;
    constexpr int kPatchFaces = 40;
    for (std::size_t head = 0; head < queue.size() && patchSize < kPatchFaces; ++head) {
        const int f = queue[head];
        for (int e = 0; e < 3 && patchSize < kPatchFaces; ++e) {
            const int a = original.faces[f].v[e];
            const int b = original.faces[f].v[(e + 1) % 3];
            for (int g : info.at(manumesh::common::meshEdgeKey(a, b))) {
                if (!inPatch[g] && patchSize < kPatchFaces) {
                    inPatch[g] = 1;
                    ++patchSize;
                    queue.push_back(g);
                }
            }
        }
    }
    ASSERT_EQ(kPatchFaces, patchSize);

    Mesh flipped = original;
    for (int fi = 0; fi < static_cast<int>(flipped.faces.size()); ++fi) {
        if (inPatch[fi]) {
            std::swap(flipped.faces[fi].v[1], flipped.faces[fi].v[2]);
        }
    }

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 25.0;
    options.minFeatureLoopVertices = 4;
    const FeatureAnalysis baseline = feature::detectFeatureCurves(original, options);
    const FeatureAnalysis patched = feature::detectFeatureCurves(flipped, options);

    const auto signedEdgeSet = [](const FeatureAnalysis& features) {
        std::set<std::tuple<int, int, bool, bool, int>> result;
        for (const feature::FeatureGraphEdge& edge : features.graph.edges) {
            result.insert({edge.a, edge.b, edge.boundary, edge.dihedral, edge.signedKind});
        }
        return result;
    };
    EXPECT_EQ(signedEdgeSet(baseline), signedEdgeSet(patched));
    EXPECT_EQ(0, baseline.inconsistentWindingEdges);
    EXPECT_EQ(0, patched.inconsistentWindingEdges);
    EXPECT_EQ(baseline.convexFeatureEdges, patched.convexFeatureEdges);
    EXPECT_EQ(baseline.concaveFeatureEdges, patched.concaveFeatureEdges);
    EXPECT_EQ(baseline.unknownSignedFeatureEdges, patched.unknownSignedFeatureEdges);
}

TEST(FeatureDetection, DegreeOneChainEndpointsAreNotJunctions) {
    const Mesh mesh = makeRectangularBipyramidMesh();

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 120.0;
    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

    ASSERT_EQ(2, features.dihedralFeatureEdges);
    EXPECT_EQ(2, features.featureEdges);
    EXPECT_EQ(0, features.boundaryFeatureEdges);
    EXPECT_TRUE(features.graph.junctionVertices.empty());
    EXPECT_EQ(std::vector<int>({2, 3, 4, 5}), features.graph.endpointVertices);
    for (int id : {2, 3, 4, 5}) {
        EXPECT_TRUE(features.vertices[id].isFeature);
        EXPECT_FALSE(features.vertices[id].junction);
        EXPECT_FALSE(features.graph.vertices[id].junction);
        EXPECT_TRUE(features.graph.vertices[id].endpoint);
    }
    ASSERT_EQ(2u, features.components.size());
    for (const feature::FeatureComponent& component : features.components) {
        EXPECT_EQ(1, component.edgeCount);
        EXPECT_EQ(0, component.junctionVertices);
        EXPECT_EQ(2, component.endpointVertices);
    }
}

TEST(FeatureDetection, DetectFeatureCurvesIsDeterministicAcrossRuns) {
    {
        const Mesh mesh = makeBranchedCircularBoundaryMesh();
        FeatureOptions options = discreteOnlyOptions();
        options.circleFitRelativeThreshold = 0.03;
        options.minFeatureLoopVertices = 12;
        const FeatureAnalysis first = feature::detectFeatureCurves(mesh, options);
        const FeatureAnalysis second = feature::detectFeatureCurves(mesh, options);
        expectIdenticalAnalyses(first, second);
    }
    {
        const Mesh mesh = makeFragmentedCircleWithTensorRidgeMesh();
        FeatureOptions options;
        options.featureAngleDeg = 179.0;
        options.normalTensorFeatureThreshold = 0.06;
        options.normalTensorMinEdgeAlignment = 0.2;
        options.normalTensorSmoothingIterations = 1;
        options.minFeatureLoopVertices = 12;
        options.circleFitRelativeThreshold = 0.04;
        const FeatureAnalysis first = feature::detectFeatureCurves(mesh, options);
        const FeatureAnalysis second = feature::detectFeatureCurves(mesh, options);
        expectIdenticalAnalyses(first, second);
    }
}

namespace {

// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
Mesh makePlaneWithDegenerateTriangle(bool duplicateVertex) {
    Mesh mesh = manumesh::generatePlaneGrid(6, 1.0, false);
    const Vec3 base = mesh.vertices[0];
    const Vec3 along = mesh.vertices[1] - mesh.vertices[0];
    const int apex = static_cast<int>(mesh.vertices.size());
    if (duplicateVertex) {
        mesh.vertices.push_back(base);
    } else {
        const Vec3 ortho(-along.y(), along.x(), 0.0);
        mesh.vertices.push_back(base + 0.5 * along + 1e-30 * ortho);
    }
    mesh.faces.push_back({{0, apex, 1}});
    return mesh;
}

void expectFiniteAnalysis(const FeatureAnalysis& analysis) {
    for (const feature::VertexFeature& vertex : analysis.vertices) {
        EXPECT_TRUE(vertex.tangent.allFinite());
        EXPECT_TRUE(std::isfinite(vertex.confidence));
    }
    for (const feature::FeatureLoop& loop : analysis.loops) {
        EXPECT_TRUE(loop.center.allFinite());
        EXPECT_TRUE(loop.normal.allFinite());
        EXPECT_TRUE(std::isfinite(loop.componentConfidence));
    }
}

void expectDegenerateFaceTolerated(bool duplicateVertex) {
    const Mesh dirty = makePlaneWithDegenerateTriangle(duplicateVertex);
    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 40.0;

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    FeatureAnalysis analysis;
    ASSERT_NO_THROW(analysis = feature::detectFeatureCurves(dirty, options));
    EXPECT_EQ(1, analysis.degenerateFaces);
    expectFiniteAnalysis(analysis);

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    const FeatureAnalysis clean = feature::detectFeatureCurves(manumesh::generatePlaneGrid(6, 1.0, false), options);
    EXPECT_EQ(0, clean.degenerateFaces);
    EXPECT_EQ(clean.dihedralFeatureEdges, analysis.dihedralFeatureEdges);
}

} // 命名空间

TEST(FeatureDetection, ToleratesZeroAreaTriangleFromDuplicateVertex) { expectDegenerateFaceTolerated(true); }

TEST(FeatureDetection, ToleratesCollinearSliverTriangle) { expectDegenerateFaceTolerated(false); }
