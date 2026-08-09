/**
 * @file tests/unit/simplification/simplification_analytic_tests.cpp
 * @brief 验证 ManuMesh 测试中的简化 解析测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

// 检查该步骤的边界条件，并确保结果保持确定性。
//
// 检查该步骤的边界条件，并确保结果保持确定性。
//  该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//  该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
#include "AnalyticFixtures.h"
#include "SimplificationTestSupport.h"
#include "algorithms/analysis/MeshAnalysis.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MathConstants.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

namespace analytic = manumesh::test::analytic;
namespace feature = manumesh::feature;
namespace simplification = manumesh::simplification;

using manumesh::kPi;
using manumesh::Mesh;
using manumesh::Vec2;
using manumesh::Vec3;
using manumesh::test::SimplifiedMesh;
using manumesh::test::simplifyWithReport;

feature::FeatureOptions rimFeatureOptions() {
    feature::FeatureOptions options;
    options.featureAngleDeg = 40.0;
    options.minFeatureLoopVertices = 8;
    options.useNormalTensorFeatures = false;
    return options;
}

int countCircularLoops(const feature::FeatureAnalysis& analysis) {
    return static_cast<int>(
        std::count_if(analysis.loops.begin(), analysis.loops.end(), [](const feature::FeatureLoop& loop) {
            return loop.circular;
        })
    );
}

/// 说明该辅助函数的输入、输出和边界条件。
double circularUvDistance(double a, double b) {
    const double difference = std::abs(a - b);
    const double wrapped = difference - std::floor(difference);
    return std::min(wrapped, 1.0 - wrapped);
}

/// 说明该辅助函数的输入、输出和边界条件。
/// 说明该辅助函数的输入、输出和边界条件。
/// 说明该辅助函数的输入、输出和边界条件。
/// 说明该辅助函数的输入、输出和边界条件。
void assignCylinderUvs(analytic::CylinderFixture& fixture) {
    Mesh& mesh = fixture.mesh;
    mesh.faceTexCoords.resize(mesh.faces.size());
    for (std::size_t face = 0; face < mesh.faces.size(); ++face) {
        int minColumn = fixture.segments;
        int maxColumn = 0;
        for (int corner = 0; corner < 3; ++corner) {
            const int column = mesh.faces[face].v[corner] % fixture.segments;
            minColumn = std::min(minColumn, column);
            maxColumn = std::max(maxColumn, column);
        }
        const bool wraps = maxColumn - minColumn > fixture.segments / 2;
        mesh.faceTexCoords[face].valid = true;
        for (int corner = 0; corner < 3; ++corner) {
            const int vertex = mesh.faces[face].v[corner];
            const int column = vertex % fixture.segments;
            const int row = vertex / fixture.segments;
            double u = static_cast<double>(column) / fixture.segments;
            if (wraps && column == 0) {
                u = 1.0;
            }
            const double v = static_cast<double>(row) / fixture.rings;
            mesh.faceTexCoords[face].uv[corner] = Vec2(u, v);
        }
    }
}

double signedUvArea(const manumesh::FaceTexCoords& texcoords) {
    const Vec2 e1 = texcoords.uv[1] - texcoords.uv[0];
    const Vec2 e2 = texcoords.uv[2] - texcoords.uv[0];
    return 0.5 * (e1.x() * e2.y() - e1.y() * e2.x());
}

} // 命名空间

// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(SimplificationAnalytic, SphereVerticesStayOnSphereAndTopologyIsPreserved) {
    const analytic::SphereFixture sphere = analytic::makeUvSphere(48, 96, 1.0);
    const SimplifiedMesh result = simplifyWithReport(sphere.mesh, manumesh::test::standardOptions(0.2));
    manumesh::test::expectBudget(result, sphere.mesh, 0.2);

    double maxRadialDeviation = 0.0;
    for (const Vec3& vertex : result.mesh.vertices) {
        maxRadialDeviation = std::max(maxRadialDeviation, std::abs(vertex.norm() - sphere.radius));
    }
    EXPECT_LT(maxRadialDeviation, 0.01 * sphere.radius);

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(result.mesh);
    EXPECT_EQ(0, stats.boundaryEdges);
    EXPECT_EQ(0, stats.nonManifoldEdges);
    EXPECT_EQ(2, stats.vertices - stats.edges + stats.faces);
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(SimplificationAnalytic, PrimitiveProtectionKeepsCylinderRimsExactlyCircular) {
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(64, 8, 1.0, 2.0, true);

    simplification::SimplifyOptions options;
    options.targetRatio = 0.3;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::PrimitiveCurves;
    options.featureAngleDeg = 40.0;
    options.minFeatureLoopVertices = 8;
    const SimplifiedMesh result = simplifyWithReport(cylinder.mesh, options);
    manumesh::test::expectBudget(result, cylinder.mesh, 0.3);

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    const feature::FeatureAnalysis reDetected = feature::detectFeatureCurves(result.mesh, rimFeatureOptions());
    ASSERT_EQ(2, countCircularLoops(reDetected));

    const double radius = cylinder.radius;
    const double halfHeight = 0.5 * cylinder.height;
    for (const feature::FeatureLoop& loop : reDetected.loops) {
        if (!loop.circular) {
            continue;
        }
        EXPECT_NEAR(radius, loop.radius, 1e-6 * radius);
        for (int vertex : loop.vertices) {
            const Vec3& p = result.mesh.vertices[vertex];
            const double axisDistance = std::hypot(p.x(), p.y());
            EXPECT_NEAR(radius, axisDistance, 1e-6 * radius);
            EXPECT_NEAR(halfHeight, std::abs(p.z()), 1e-6 * radius);
        }
    }
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
//  该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//  该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
TEST(SimplificationAnalytic, TexturedCylinderKeepsUvAlignedWithAnalyticParametrization) {
    analytic::CylinderFixture cylinder = analytic::makeCylinder(48, 6, 1.0, 2.0, false);
    assignCylinderUvs(cylinder);
    for (const manumesh::FaceTexCoords& texcoords : cylinder.mesh.faceTexCoords) {
        ASSERT_GT(signedUvArea(texcoords), 0.0);
    }

    simplification::SimplifyOptions options;
    options.targetRatio = 0.5;
    options.preserveTexture = true;
    const SimplifiedMesh result = simplifyWithReport(cylinder.mesh, options);
    manumesh::test::expectBudget(result, cylinder.mesh, 0.5);

    ASSERT_EQ(result.mesh.faces.size(), result.mesh.faceTexCoords.size());
    EXPECT_EQ(0, result.report.textureApplyFailures);

    const double circumference = 2.0 * kPi * cylinder.radius;
    int validFaces = 0;
    for (std::size_t face = 0; face < result.mesh.faces.size(); ++face) {
        const manumesh::FaceTexCoords& texcoords = result.mesh.faceTexCoords[face];
        if (!texcoords.valid) {
            continue;
        }
        ++validFaces;
        EXPECT_GT(signedUvArea(texcoords), 0.0) << "flipped UV face " << face;

        double longestEdge = 0.0;
        for (int corner = 0; corner < 3; ++corner) {
            const Vec3& a = result.mesh.vertices[result.mesh.faces[face].v[corner]];
            const Vec3& b = result.mesh.vertices[result.mesh.faces[face].v[(corner + 1) % 3]];
            longestEdge = std::max(longestEdge, (a - b).norm());
        }
        const double uTolerance = longestEdge / circumference + 1e-9;
        const double vTolerance = longestEdge / cylinder.height + 1e-9;
        for (int corner = 0; corner < 3; ++corner) {
            const Vec3& p = result.mesh.vertices[result.mesh.faces[face].v[corner]];
            const double analyticU = std::atan2(p.y(), p.x()) / (2.0 * kPi) + 0.5;
            const double analyticV = p.z() / cylinder.height + 0.5;
            // 检查该步骤的边界条件，并确保结果保持确定性。
            // 检查该步骤的边界条件，并确保结果保持确定性。
            // 检查该步骤的边界条件，并确保结果保持确定性。
            EXPECT_LT(circularUvDistance(texcoords.uv[corner].x() + 0.5, analyticU), uTolerance)
                << "face " << face << " corner " << corner;
            EXPECT_NEAR(analyticV, texcoords.uv[corner].y(), vTolerance) << "face " << face << " corner " << corner;
        }
    }
    EXPECT_GT(validFaces, 0);
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(SimplificationAnalytic, TorusStaysWithinSampledHausdorffBudget) {
    const analytic::TorusFixture torus = analytic::makeTorus(96, 48, 1.0, 0.3);
    const SimplifiedMesh result = simplifyWithReport(torus.mesh, manumesh::test::standardOptions(0.35));
    manumesh::test::expectBudget(result, torus.mesh, 0.35);

    const manumesh::analysis::DistanceStats distance =
        manumesh::analysis::compareMeshesBySampledDistance(torus.mesh, result.mesh, 4096);
    const double budget = 0.02 * torus.minorRadius;
    EXPECT_LT(distance.maxOriginalToSimplified, budget);
    EXPECT_LT(distance.maxSimplifiedToOriginal, budget);

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(result.mesh);
    EXPECT_EQ(0, stats.boundaryEdges);
    EXPECT_EQ(0, stats.nonManifoldEdges);
    EXPECT_EQ(0, stats.vertices - stats.edges + stats.faces);
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(SimplificationAnalytic, DetectAndSimplifyPipelineIsByteStableAcrossRuns) {
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(48, 6, 1.0, 2.0, true);

    simplification::SimplifyOptions options;
    options.targetRatio = 0.4;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::PrimitiveCurves;
    options.featureAngleDeg = 40.0;
    options.minFeatureLoopVertices = 8;

    Mesh reference;
    for (int run = 0; run < 3; ++run) {
        SCOPED_TRACE(run);
        const feature::FeatureAnalysis features = feature::detectFeatureCurves(cylinder.mesh, rimFeatureOptions());
        simplification::QEMSimplifier simplifier(options);
        const Mesh output = simplifier.simplify(cylinder.mesh, features);
        if (run == 0) {
            reference = output;
            EXPECT_FALSE(reference.empty());
            continue;
        }
        ASSERT_EQ(reference.vertices.size(), output.vertices.size());
        ASSERT_EQ(reference.faces.size(), output.faces.size());
        for (std::size_t vertex = 0; vertex < reference.vertices.size(); ++vertex) {
            // 检查该步骤的边界条件，并确保结果保持确定性。
            // 检查该步骤的边界条件，并确保结果保持确定性。
            EXPECT_EQ(reference.vertices[vertex].x(), output.vertices[vertex].x());
            EXPECT_EQ(reference.vertices[vertex].y(), output.vertices[vertex].y());
            EXPECT_EQ(reference.vertices[vertex].z(), output.vertices[vertex].z());
        }
        for (std::size_t face = 0; face < reference.faces.size(); ++face) {
            EXPECT_EQ(reference.faces[face].v, output.faces[face].v);
        }
    }
}
