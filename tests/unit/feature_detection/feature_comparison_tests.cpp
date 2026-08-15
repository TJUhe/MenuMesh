/**
 * @file tests/unit/feature_detection/feature_comparison_tests.cpp
 * @brief 验证 ManuMesh 测试中的特征比较测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#include "algorithms/feature_detection/FeatureComparison.h"
#include "algorithms/feature_detection/FeatureDetector.h"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <vector>
namespace {

using manumesh::Mesh;
using manumesh::Vec3;
using manumesh::feature::FeatureAnalysis;
using manumesh::feature::FeatureLoop;
using manumesh::feature::LoopMatch;
using manumesh::feature::LoopMatchOptions;
using manumesh::feature::LoopMatchReport;
using manumesh::feature::LoopMatchStatus;
using manumesh::feature::matchCircularLoops;

constexpr double kTestPi = 3.141592653589793238462643383279502884;

/// 说明该辅助函数的输入、输出和边界条件。
/// 说明该辅助函数的输入、输出和边界条件。
FeatureLoop appendCircleLoop(Mesh& mesh, int id, const Vec3& center, double radius, int count) {
    FeatureLoop loop;
    loop.id = id;
    loop.closed = true;
    loop.circular = true;
    loop.primitive = manumesh::feature::FeaturePrimitiveType::Circle;
    loop.center = center;
    loop.normal = Vec3(0.0, 0.0, 1.0);
    loop.radius = radius;
    for (int i = 0; i < count; ++i) {
        const double angle = 2.0 * kTestPi * static_cast<double>(i) / static_cast<double>(count);
        loop.vertices.push_back(static_cast<int>(mesh.vertices.size()));
        mesh.vertices.push_back(center + Vec3(radius * std::cos(angle), radius * std::sin(angle), 0.0));
    }
    loop.edgeCount = count;
    return loop;
}

} // namespace

void bindAnalysisToMesh(FeatureAnalysis& analysis, Mesh& mesh) {
    for (const FeatureLoop& loop : analysis.loops) {
        const int centerVertex = static_cast<int>(mesh.vertices.size());
        mesh.vertices.push_back(loop.center);
        for (std::size_t index = 0; index < loop.vertices.size(); ++index) {
            const int a = loop.vertices[index];
            const int b = loop.vertices[(index + 1u) % loop.vertices.size()];
            mesh.faces.push_back({{{a, b, centerVertex}}});
        }
    }

    analysis.vertices.assign(mesh.vertices.size(), manumesh::feature::VertexFeature{});
    analysis.graph.vertices.assign(mesh.vertices.size(), manumesh::feature::FeatureGraphVertex{});
    analysis.components.clear();
    analysis.graph.edges.clear();
    for (std::size_t loopIndex = 0; loopIndex < analysis.loops.size(); ++loopIndex) {
        FeatureLoop& loop = analysis.loops[loopIndex];
        loop.componentId = static_cast<int>(loopIndex);
        manumesh::feature::FeatureComponent component;
        component.id = static_cast<int>(loopIndex);
        component.vertices = loop.vertices;
        component.edgeCount = loop.edgeCount;
        component.boundaryEdges = loop.edgeCount;
        component.strongEvidenceEdges = loop.edgeCount;
        component.cycleRank = 1;
        component.closed = true;
        component.closureRate = 1.0;
        component.strongEvidenceRatio = 1.0;
        analysis.components.push_back(component);

        for (int vertexId : loop.vertices) {
            manumesh::feature::VertexFeature& vertex = analysis.vertices[static_cast<std::size_t>(vertexId)];
            manumesh::feature::FeatureGraphVertex& graphVertex =
                analysis.graph.vertices[static_cast<std::size_t>(vertexId)];
            if (!vertex.isFeature) {
                vertex.isFeature = true;
                vertex.loopId = loop.id;
                vertex.componentId = loop.componentId;
                vertex.circular = loop.circular;
                vertex.primitive = loop.primitive;
                vertex.circleCenter = loop.center;
                vertex.circleNormal = loop.normal;
                vertex.circleRadius = loop.radius;
                Vec3 radial = mesh.vertices[static_cast<std::size_t>(vertexId)] - loop.center;
                radial -= loop.normal * radial.dot(loop.normal);
                vertex.tangent = loop.normal.cross(radial).normalized();
            }
            graphVertex.loopIds.push_back(loop.id);
        }
        for (std::size_t index = 0; index < loop.vertices.size(); ++index) {
            const int a = loop.vertices[index];
            const int b = loop.vertices[(index + 1u) % loop.vertices.size()];
            manumesh::feature::FeatureGraphEdge edge;
            edge.a = a;
            edge.b = b;
            edge.boundary = true;
            const int edgeId = static_cast<int>(analysis.graph.edges.size());
            analysis.graph.edges.push_back(edge);
            analysis.graph.vertices[static_cast<std::size_t>(a)].incidentEdges.push_back(edgeId);
            analysis.graph.vertices[static_cast<std::size_t>(b)].incidentEdges.push_back(edgeId);
        }
    }
    for (std::size_t vertexId = 0; vertexId < analysis.graph.vertices.size(); ++vertexId) {
        manumesh::feature::FeatureGraphVertex& graphVertex = analysis.graph.vertices[vertexId];
        for (int edgeId : graphVertex.incidentEdges) {
            const manumesh::feature::FeatureGraphEdge& edge = analysis.graph.edges[static_cast<std::size_t>(edgeId)];
            const int owner = static_cast<int>(vertexId);
            const int neighbor = edge.a == owner ? edge.b : edge.a;
            const Vec3 tangent =
                (mesh.vertices[static_cast<std::size_t>(neighbor)] - mesh.vertices[vertexId]).normalized();
            graphVertex.branches.push_back({edgeId, neighbor, tangent, edge.signedKind});
        }
    }
    analysis.featureEdges = static_cast<int>(analysis.graph.edges.size());
    analysis.tracedFeatureEdges = analysis.featureEdges;
    analysis.boundaryFeatureEdges = analysis.featureEdges;
    analysis.source = manumesh::feature::featureAnalysisSource(mesh);
}
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(FeatureComparison, IdenticalLoopsMatchStronglyWithZeroErrors) {
    Mesh original;
    Mesh simplified;
    FeatureAnalysis originalFeatures;
    FeatureAnalysis simplifiedFeatures;
    originalFeatures.loops.push_back(appendCircleLoop(original, 0, Vec3(0.0, 0.0, 0.0), 1.0, 12));
    originalFeatures.loops.push_back(appendCircleLoop(original, 1, Vec3(5.0, 0.0, 0.0), 0.5, 10));
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 0, Vec3(0.0, 0.0, 0.0), 1.0, 8));
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 1, Vec3(5.0, 0.0, 0.0), 0.5, 8));
    bindAnalysisToMesh(simplifiedFeatures, simplified);

    const LoopMatchReport report = matchCircularLoops(originalFeatures, simplifiedFeatures, simplified);

    EXPECT_EQ(2, report.originalCircularLoops);
    EXPECT_EQ(2, report.simplifiedCircularLoops);
    EXPECT_EQ(2, report.matchedLoops);
    EXPECT_EQ(0, report.missingLoops);
    ASSERT_EQ(2u, report.matches.size());
    for (int i = 0; i < 2; ++i) {
        const LoopMatch& match = report.matches[i];
        EXPECT_EQ(LoopMatchStatus::Matched, match.status);
        EXPECT_EQ(i, match.originalLoopId);
        EXPECT_EQ(i, match.simplifiedLoopIndex);
        EXPECT_EQ(8, match.simplifiedVertices);
        EXPECT_NEAR(0.0, match.centerError, 1e-12);
        EXPECT_NEAR(0.0, match.radiusError, 1e-12);
        EXPECT_NEAR(0.0, match.normalAngleDeg, 1e-9);
        EXPECT_NEAR(0.0, match.directional.radialMax, 1e-12);
        EXPECT_NEAR(0.0, match.directional.planeMax, 1e-12);
        EXPECT_EQ(8, match.directional.samples);
    }
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(FeatureComparison, RadiusDriftInsidePlausibleBandIsWeakMatch) {
    Mesh original;
    Mesh simplified;
    FeatureAnalysis originalFeatures;
    FeatureAnalysis simplifiedFeatures;
    originalFeatures.loops.push_back(appendCircleLoop(original, 0, Vec3(0.0, 0.0, 0.0), 1.0, 12));
    // 检查该步骤的边界条件，并确保结果保持确定性。
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 0, Vec3(0.0, 0.0, 0.0), 1.15, 12));
    bindAnalysisToMesh(simplifiedFeatures, simplified);

    LoopMatchOptions options;
    options.referenceDiagonal = 10.0;
    const LoopMatchReport report = matchCircularLoops(originalFeatures, simplifiedFeatures, simplified, options);

    EXPECT_EQ(1, report.matchedLoops);
    EXPECT_EQ(0, report.missingLoops);
    ASSERT_EQ(1u, report.matches.size());
    const LoopMatch& match = report.matches.front();
    EXPECT_EQ(LoopMatchStatus::WeakMatch, match.status);
    EXPECT_EQ("weak_match", manumesh::feature::toString(match.status));
    EXPECT_NEAR(0.15, match.radiusError, 1e-12);
    EXPECT_NEAR(0.15, match.directional.radialMax, 1e-12);
    EXPECT_EQ(0, static_cast<int>(std::lround(match.normalAngleDeg)));

    // 检查该步骤的边界条件，并确保结果保持确定性。
    LoopMatchOptions strict = options;
    strict.plausibleRadiusErrorRel = 0.10;
    const LoopMatchReport strictReport = matchCircularLoops(originalFeatures, simplifiedFeatures, simplified, strict);
    ASSERT_EQ(1u, strictReport.matches.size());
    EXPECT_EQ(LoopMatchStatus::Missing, strictReport.matches.front().status);
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(FeatureComparison, VanishedLoopIsMissingAndSimplifiedLoopsAreConsumedOnce) {
    Mesh original;
    Mesh simplified;
    FeatureAnalysis originalFeatures;
    FeatureAnalysis simplifiedFeatures;
    originalFeatures.loops.push_back(appendCircleLoop(original, 0, Vec3(0.0, 0.0, 0.0), 1.0, 12));
    originalFeatures.loops.push_back(appendCircleLoop(original, 1, Vec3(0.2, 0.0, 0.0), 1.0, 12));
    // 检查该步骤的边界条件，并确保结果保持确定性。
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 0, Vec3(0.0, 0.0, 0.0), 1.0, 8));
    bindAnalysisToMesh(simplifiedFeatures, simplified);

    LoopMatchOptions options;
    options.referenceDiagonal = 10.0;
    const LoopMatchReport report = matchCircularLoops(originalFeatures, simplifiedFeatures, simplified, options);

    EXPECT_EQ(2, report.originalCircularLoops);
    EXPECT_EQ(1, report.simplifiedCircularLoops);
    EXPECT_EQ(1, report.matchedLoops);
    EXPECT_EQ(1, report.missingLoops);
    ASSERT_EQ(2u, report.matches.size());
    EXPECT_EQ(LoopMatchStatus::Matched, report.matches[0].status);
    EXPECT_EQ(0, report.matches[0].simplifiedLoopIndex);

    const LoopMatch& missing = report.matches[1];
    EXPECT_EQ(LoopMatchStatus::Missing, missing.status);
    EXPECT_EQ(-1, missing.simplifiedLoopIndex);
    EXPECT_EQ(0, missing.simplifiedVertices);
    EXPECT_EQ(0.0, missing.centerError);
    EXPECT_EQ(0.0, missing.radiusError);
    EXPECT_EQ(0.0, missing.normalAngleDeg);
    EXPECT_EQ(0, missing.directional.samples);
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(FeatureComparison, ChoosesBestPlausibleCandidateAfterThresholdFiltering) {
    Mesh original;
    Mesh simplified;
    FeatureAnalysis originalFeatures;
    FeatureAnalysis simplifiedFeatures;
    originalFeatures.loops.push_back(appendCircleLoop(original, 0, Vec3(0.0, 0.0, 0.0), 1.0, 12));

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 0, Vec3(0.81, 0.0, 0.0), 1.0, 8));
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 1, Vec3(0.7, 0.0, 0.0), 1.02, 8));
    bindAnalysisToMesh(simplifiedFeatures, simplified);

    LoopMatchOptions options;
    options.referenceDiagonal = 10.0;
    const LoopMatchReport report = matchCircularLoops(originalFeatures, simplifiedFeatures, simplified, options);

    EXPECT_EQ(1, report.matchedLoops);
    EXPECT_EQ(0, report.missingLoops);
    ASSERT_EQ(1u, report.matches.size());
    const LoopMatch& match = report.matches.front();
    EXPECT_EQ(LoopMatchStatus::WeakMatch, match.status);
    EXPECT_EQ(1, match.simplifiedLoopIndex);
    EXPECT_NEAR(0.7, match.centerError, 1e-12);
    EXPECT_NEAR(0.02, match.radiusError, 1e-12);
}

TEST(FeatureComparison, RejectsInvalidLoopMatchOptionsBeforeMatching) {
    auto expectInvalid = [](const LoopMatchOptions& options) {
        const manumesh::Status status = manumesh::feature::validateLoopMatchOptions(options);
        EXPECT_FALSE(status.ok());
        EXPECT_EQ(manumesh::StatusCode::InvalidArgument, status.code());
    };

    LoopMatchOptions options;
    options.plausibleCenterErrorRatio = std::numeric_limits<double>::quiet_NaN();
    expectInvalid(options);

    options = LoopMatchOptions{};
    options.matchedRadiusErrorRel = -0.01;
    expectInvalid(options);

    options = LoopMatchOptions{};
    options.plausibleNormalAngleDeg = 181.0;
    expectInvalid(options);

    options = LoopMatchOptions{};
    options.referenceDiagonal = std::numeric_limits<double>::infinity();
    expectInvalid(options);

    options = LoopMatchOptions{};
    options.matchedCenterErrorRatio = options.plausibleCenterErrorRatio + 0.01;
    expectInvalid(options);

    FeatureAnalysis analysis;
    Mesh mesh;
    EXPECT_THROW(matchCircularLoops(analysis, analysis, mesh, options), std::invalid_argument);
}

// 命名空间
TEST(FeatureComparison, NonCircularLoopsAreIgnored) {
    Mesh original;
    Mesh simplified;
    FeatureAnalysis originalFeatures;
    FeatureAnalysis simplifiedFeatures;
    FeatureLoop polygonal = appendCircleLoop(original, 0, Vec3(0.0, 0.0, 0.0), 1.0, 6);
    polygonal.circular = false;
    polygonal.primitive = manumesh::feature::FeaturePrimitiveType::PolygonalLoop;
    originalFeatures.loops.push_back(polygonal);
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 0, Vec3(0.0, 0.0, 0.0), 1.0, 6));
    bindAnalysisToMesh(simplifiedFeatures, simplified);

    const LoopMatchReport report = matchCircularLoops(originalFeatures, simplifiedFeatures, simplified);

    EXPECT_EQ(0, report.originalCircularLoops);
    EXPECT_EQ(1, report.simplifiedCircularLoops);
    EXPECT_TRUE(report.matches.empty());
    EXPECT_EQ(0, report.matchedLoops);
    EXPECT_EQ(0, report.missingLoops);
}
