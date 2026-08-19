/**
 * @file tests/unit/simplification/simplification_placement_tests.cpp
 * @brief 验证 QEM 退化求解、边界投影、曲线索引和自适应权重。
 * @ingroup manumesh_tests
 */

#include "AnalyticFixtures.h"
#include "SimplificationTestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MathUtils.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "simplification/detail/FeatureConstraints.h"
#include "simplification/detail/Placement.h"
#include "simplification/detail/Quadrics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <vector>
namespace {

using manumesh::Mesh;
using manumesh::Vec3;
using manumesh::mesh_edit::DynamicTopology;
using manumesh::simplification::BoundaryCollapseDecision;
using manumesh::simplification::FaceState;
using manumesh::simplification::FeatureCurveConstraint;
using manumesh::simplification::FeatureCurveKind;
using manumesh::simplification::SolveResult;
using manumesh::simplification::VertexState;

double evaluateCost(const manumesh::Mat4& q, const Vec3& p) { return manumesh::simplification::evaluateQuadric(q, p); }

struct BoundaryStrip {
    std::vector<VertexState> vertices;
    std::vector<FaceState> faces;
    DynamicTopology topology;

    explicit BoundaryStrip(const std::array<Vec3, 4>& chain)
        : topology(makeState(chain), 6) {}

    static constexpr int kKeep = 1;
    static constexpr int kRemove = 2;

private:
    std::vector<FaceState> makeState(const std::array<Vec3, 4>& chain) {
        const Vec3 interiorA(0.0, chain[1].y() - 1.0, 0.0);
        const Vec3 interiorB(1.5, chain[1].y() - 1.0, 0.0);
        const std::array<Vec3, 6> points = {chain[0], chain[1], chain[2], chain[3], interiorA, interiorB};
        vertices.resize(points.size());
        for (std::size_t i = 0; i < points.size(); ++i) {
            vertices[i].p = points[i];
            vertices[i].isBoundary = true;
        }
        vertices[4].isBoundary = true;
        vertices[5].isBoundary = true;
        faces.resize(4);
        faces[0].v = {0, 4, 1};
        faces[1].v = {1, 4, 5};
        faces[2].v = {1, 5, 2};
        faces[3].v = {2, 5, 3};
        return faces;
    }
};

double boundaryAreaObjective(const std::array<Vec3, 4>& chain, const Vec3& v) {
    const Vec3 e1 = (chain[0] - chain[1]) + (chain[1] - chain[2]) + (chain[2] - chain[3]);
    const Vec3 e2 = chain[1].cross(chain[0]) + chain[2].cross(chain[1]) + chain[3].cross(chain[2]);
    return (e2 - v.cross(e1)).squaredNorm();
}

Vec3 clampToSegmentReference(const Vec3& position, const Vec3& a, const Vec3& b) {
    const Vec3 edge = b - a;
    const double len2 = edge.squaredNorm();
    if (len2 <= 1e-30) {
        return 0.5 * (a + b);
    }
    const double t = manumesh::clampValue((position - a).dot(edge) / len2, 0.0, 1.0);
    return a + t * edge;
}

double distanceToSegments(const Vec3& point, const std::vector<std::array<Vec3, 2>>& segments) {
    double best2 = std::numeric_limits<double>::infinity();
    for (const std::array<Vec3, 2>& segment : segments) {
        const Vec3 closest = clampToSegmentReference(point, segment[0], segment[1]);
        best2 = std::min(best2, (point - closest).squaredNorm());
    }
    return std::sqrt(best2);
}

std::vector<std::array<Vec3, 2>> collectBoundarySegments(const Mesh& mesh, double* maxSegmentLength) {
    std::vector<std::array<Vec3, 2>> segments;
    const manumesh::Result<manumesh::MeshTopology> topologyResult = manumesh::MeshTopology::build(mesh);
    if (!topologyResult.ok()) {
        return segments;
    }
    for (const manumesh::TopologyEdge& edge : topologyResult.value().edges()) {
        if (!edge.boundary()) {
            continue;
        }
        const Vec3& a = mesh.vertices[edge.vertices[0]];
        const Vec3& b = mesh.vertices[edge.vertices[1]];
        segments.push_back({a, b});
        if (maxSegmentLength) {
            *maxSegmentLength = std::max(*maxSegmentLength, (b - a).norm());
        }
    }
    return segments;
}

} // 命名空间

TEST(PlacementSolve, RankTwoQuadricUsesAlongEdgeOptimum) {
    const manumesh::Mat4 q = manumesh::simplification::planeQuadric(Vec3(0.0, 0.0, 1.0), Vec3::Zero()) +
                             manumesh::simplification::planeQuadric(Vec3(0.0, 1.0, 0.0), Vec3::Zero());
    const Vec3 a(0.0, 1.0, 0.0);
    const Vec3 b(1.0, -3.0, 0.0);

    const std::vector<SolveResult> placements = manumesh::simplification::solvePlacementCandidates(q, a, b);

    ASSERT_FALSE(placements.empty());
    EXPECT_TRUE(placements.front().usedFallback);
    const double costA = evaluateCost(q, a);
    const double costB = evaluateCost(q, b);
    const double costMid = evaluateCost(q, 0.5 * (a + b));
    EXPECT_LT(placements.front().cost, costA);
    EXPECT_LT(placements.front().cost, costB);
    EXPECT_LT(placements.front().cost, costMid);
    EXPECT_NEAR(placements.front().position.x(), 0.25, 1e-12);
    EXPECT_NEAR(placements.front().position.y(), 0.0, 1e-12);
    EXPECT_NEAR(placements.front().position.z(), 0.0, 1e-12);
    EXPECT_NEAR(placements.front().cost, 0.0, 1e-18);
}

TEST(PlacementSolve, RankTwoAlongEdgeOptimumIsScaleInvariant) {
    for (const double scale : {1e-3, 1.0, 1e+3}) {
        const manumesh::Mat4 q = manumesh::simplification::planeQuadric(Vec3(0.0, 0.0, 1.0), Vec3::Zero()) +
                                 manumesh::simplification::planeQuadric(Vec3(0.0, 1.0, 0.0), Vec3::Zero());
        const Vec3 a = scale * Vec3(0.0, 1.0, 0.0);
        const Vec3 b = scale * Vec3(1.0, -3.0, 0.0);
        const std::vector<SolveResult> placements = manumesh::simplification::solvePlacementCandidates(q, a, b);
        ASSERT_FALSE(placements.empty());
        EXPECT_NEAR(placements.front().position.x(), 0.25 * scale, 1e-12 * scale) << "scale " << scale;
        EXPECT_NEAR(placements.front().position.y(), 0.0, 1e-12 * scale) << "scale " << scale;
    }
}

TEST(PlacementSolve, DegenerateFacePointQuadricsScaleWithAreaWeightedQem) {
    auto makeMesh = [](double scale) {
        Mesh mesh;
        mesh.vertices = {
            scale * Vec3(0.0, 0.0, 0.0),
            scale * Vec3(1.0, 0.0, 0.0),
            scale * Vec3(0.0, 1.0, 0.0),
            scale * Vec3(2.0, 0.0, 0.0),
            scale * Vec3(3.0, 0.0, 0.0),
            scale * Vec3(4.0, 0.0, 0.0),
        };
        mesh.faces = {{{0, 1, 2}}, {{3, 4, 5}}};
        return mesh;
    };

    double referenceNormalizedCost = -1.0;
    for (double scale : {1e-3, 1.0, 1e3}) {
        SCOPED_TRACE(testing::Message() << "scale=" << scale);
        const Mesh mesh = makeMesh(scale);
        manumesh::simplification::SimplifyOptions options;
        options.useLineQuadrics = false;
        manumesh::simplification::SimplifyReport report;
        const manumesh::simplification::InitialQuadrics initial =
            manumesh::simplification::InitialQuadricBuilder(options).build(
                mesh, manumesh::simplification::FeatureGuidance{}, report
            );
        const Vec3 probe = mesh.vertices[3] + scale * Vec3(0.0, 1.0, 0.0);
        const double normalizedCost =
            manumesh::simplification::evaluateQuadric(initial.quadrics[3], probe) / std::pow(scale, 4.0);
        if (referenceNormalizedCost < 0.0) {
            referenceNormalizedCost = normalizedCost;
        } else {
            EXPECT_NEAR(referenceNormalizedCost, normalizedCost, 1e-12 * referenceNormalizedCost);
        }
    }
}

TEST(BoundaryPlacement, StraightChainProjectsOntoBoundaryLine) {
    const std::array<Vec3, 4> chain = {
        Vec3(-1.0, 1.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        Vec3(1.0, 1.0, 0.0),
        Vec3(2.0, 1.0, 0.0),
    };
    BoundaryStrip strip(chain);
    const BoundaryCollapseDecision decision{true, true};

    Vec3 position(0.4, 1.25, 0.1);
    const bool projected = manumesh::simplification::projectBoundaryPlacement(
        {{BoundaryStrip::kKeep, BoundaryStrip::kRemove}, decision, strip.vertices, strip.faces, strip.topology},
        position
    );

    EXPECT_TRUE(projected);
    EXPECT_NEAR(position.x(), 0.4, 1e-12);
    EXPECT_NEAR(position.y(), 1.0, 1e-12);
    EXPECT_NEAR(position.z(), 0.0, 1e-12);
}

TEST(BoundaryPlacement, CorneredChainReducesDirectedAreaChangeVersusClamp) {
    const std::array<Vec3, 4> chain = {
        Vec3(-1.0, 0.3, 0.0),
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(2.0, 0.3, 0.0),
    };
    BoundaryStrip strip(chain);
    const BoundaryCollapseDecision decision{true, true};

    const Vec3 qemPosition(0.5, -0.05, 0.0);
    Vec3 position = qemPosition;
    const bool projected = manumesh::simplification::projectBoundaryPlacement(
        {{BoundaryStrip::kKeep, BoundaryStrip::kRemove}, decision, strip.vertices, strip.faces, strip.topology},
        position
    );
    ASSERT_TRUE(projected);

    const Vec3 clamped = clampToSegmentReference(qemPosition, chain[1], chain[2]);
    const double ltObjective = boundaryAreaObjective(chain, position);
    const double clampObjective = boundaryAreaObjective(chain, clamped);
    EXPECT_LT(ltObjective, clampObjective);
    EXPECT_NEAR(position.y(), -0.1, 1e-12);
    EXPECT_NEAR(position.z(), 0.0, 1e-12);
    EXPECT_GE(position.x(), 0.0);
    EXPECT_LE(position.x(), 1.0);
}

TEST(BoundaryPlacement, PreserveBoundarySimplifyKeepsBoundaryDriftTight) {
    const Mesh input = manumesh::generateHolePlaneGrid(16, 2.0, 0.35);
    double maxSegmentLength = 0.0;
    const std::vector<std::array<Vec3, 2>> inputBoundary = collectBoundarySegments(input, &maxSegmentLength);
    ASSERT_FALSE(inputBoundary.empty());
    ASSERT_GT(maxSegmentLength, 0.0);

    manumesh::simplification::SimplifyOptions options;
    options.targetRatio = 0.4;
    options.preserveBoundary = true;
    const Mesh output = manumesh::simplification::simplifyMesh(input, options);
    ASSERT_FALSE(output.empty());

    double drift = 0.0;
    const std::vector<std::array<Vec3, 2>> outputBoundary = collectBoundarySegments(output, nullptr);
    ASSERT_FALSE(outputBoundary.empty());
    for (const std::array<Vec3, 2>& segment : outputBoundary) {
        drift = std::max(drift, distanceToSegments(segment[0], inputBoundary));
        drift = std::max(drift, distanceToSegments(segment[1], inputBoundary));
    }
    EXPECT_LE(drift, 1.0 * maxSegmentLength);
}

TEST(FeatureCurves, SegmentIndexMatchesLinearScan) {
    FeatureCurveConstraint linear;
    linear.valid = true;
    linear.closed = true;
    linear.primitive = FeatureCurveKind::PolygonalLoop;
    const int sampleCount = 200;
    for (int i = 0; i < sampleCount; ++i) {
        const double angle = 2.0 * 3.14159265358979323846 * static_cast<double>(i) / sampleCount;
        const double radius = 2.0 + 0.35 * std::sin(7.0 * angle);
        linear.samples.push_back(Vec3(radius * std::cos(angle), radius * std::sin(angle), 0.2 * std::sin(3.0 * angle)));
    }
    FeatureCurveConstraint indexed = linear;
    manumesh::simplification::buildPolylineSegmentIndex(indexed);
    ASSERT_TRUE(indexed.segmentIndex.built());
    ASSERT_FALSE(linear.segmentIndex.built());

    for (int i = 0; i < 64; ++i) {
        const double angle = 0.37 + 0.11 * static_cast<double>(i);
        const Vec3 query(2.6 * std::cos(angle), 2.6 * std::sin(angle), 0.3 * std::sin(angle * 2.0));
        double linearDist2 = 0.0;
        double indexedDist2 = 0.0;
        const Vec3 linearPoint = manumesh::simplification::closestPointOnFeatureCurve(linear, query, linearDist2);
        const Vec3 indexedPoint = manumesh::simplification::closestPointOnFeatureCurve(indexed, query, indexedDist2);
        EXPECT_NEAR(linearDist2, indexedDist2, 1e-15) << "query " << i;
        EXPECT_NEAR((linearPoint - indexedPoint).norm(), 0.0, 1e-9) << "query " << i;
    }
}

TEST(FeatureCurves, EllipseProjectionSatisfiesGlobalClosestPointConditions) {
    manumesh::simplification::FeaturePrimitiveFit fit;
    fit.ellipseCenter = Vec3(0.25, -0.5, 0.75);
    fit.ellipseNormal = Vec3(0.0, 0.0, 2.0);
    fit.ellipseMajorAxis = Vec3(3.0, 0.0, 0.0);
    fit.ellipseMinorAxis = Vec3(0.0, -4.0, 0.0);
    fit.ellipseMajorRadius = 3.0;
    fit.ellipseMinorRadius = 0.8;
    VertexState feature;
    feature.p = fit.ellipseCenter + fit.ellipseMajorRadius * Vec3(1.0, 0.0, 0.0);

    const Vec3 major = fit.ellipseMajorAxis.normalized();
    const Vec3 minor = fit.ellipseMinorAxis.normalized();
    const Vec3 normal = fit.ellipseNormal.normalized();
    const std::array<Vec3, 5> queries = {{
        fit.ellipseCenter + Vec3(4.1, -1.3, 2.0),
        fit.ellipseCenter + Vec3(-2.2, 0.45, -1.0),
        fit.ellipseCenter + Vec3(0.55, -0.2, 0.3),
        fit.ellipseCenter + Vec3(-0.4, -0.1, 0.0),
        fit.ellipseCenter,
    }};
    const double pi = std::acos(-1.0);
    constexpr int kReferenceSamples = 16384;

    for (const Vec3& query : queries) {
        const Vec3 projected = manumesh::simplification::projectToEllipse(query, feature, fit);
        const Vec3 local = projected - fit.ellipseCenter;
        const double ellipseX = local.dot(major) / fit.ellipseMajorRadius;
        const double ellipseY = local.dot(minor) / fit.ellipseMinorRadius;
        EXPECT_NEAR(1.0, ellipseX * ellipseX + ellipseY * ellipseY, 1e-12);
        EXPECT_NEAR(0.0, local.dot(normal), 1e-12);

        const Vec3 tangent = -fit.ellipseMajorRadius * ellipseY * major + fit.ellipseMinorRadius * ellipseX * minor;
        const double orthogonalityScale = std::max(1.0, (query - projected).norm() * tangent.norm());
        EXPECT_NEAR(0.0, (query - projected).dot(tangent), 1e-10 * orthogonalityScale);

        double sampledDistanceSquared = std::numeric_limits<double>::infinity();
        for (int sample = 0; sample < kReferenceSamples; ++sample) {
            const double theta = 2.0 * pi * static_cast<double>(sample) / static_cast<double>(kReferenceSamples);
            const Vec3 candidate = fit.ellipseCenter + fit.ellipseMajorRadius * std::cos(theta) * major +
                                   fit.ellipseMinorRadius * std::sin(theta) * minor;
            sampledDistanceSquared = std::min(sampledDistanceSquared, (query - candidate).squaredNorm());
        }
        EXPECT_LE((query - projected).squaredNorm(), sampledDistanceSquared + 1e-11);
    }
}

TEST(InitialQuadrics, BoundaryAccumulationIsStableAcrossFaceOrder) {
    Mesh input;
    input.vertices.push_back(Vec3::Zero());
    constexpr int kTriangleCount = 12;
    const double pi = std::acos(-1.0);
    for (int triangle = 0; triangle < kTriangleCount; ++triangle) {
        const double angle = 2.0 * pi * static_cast<double>(triangle) / static_cast<double>(kTriangleCount);
        const double nextAngle = angle + 0.19;
        input.vertices.push_back(Vec3(std::cos(angle), std::sin(angle), 0.0));
        input.vertices.push_back(Vec3(1.7 * std::cos(nextAngle), 1.7 * std::sin(nextAngle), 0.0));
        input.faces.push_back(manumesh::Face{{0, 2 * triangle + 1, 2 * triangle + 2}});
    }
    Mesh reversed = input;
    std::reverse(reversed.faces.begin(), reversed.faces.end());

    manumesh::simplification::SimplifyOptions options;
    options.boundaryWeight = 0.37;
    manumesh::simplification::FeatureGuidance guidance;
    manumesh::simplification::InitialQuadrics forwardQuadrics;
    manumesh::simplification::InitialQuadrics reversedQuadrics;
    manumesh::simplification::SimplifyReport forwardReport;
    manumesh::simplification::SimplifyReport reversedReport;
    manumesh::simplification::computeInitialQuadrics(input, options, guidance, forwardQuadrics, forwardReport);
    manumesh::simplification::computeInitialQuadrics(reversed, options, guidance, reversedQuadrics, reversedReport);

    ASSERT_EQ(forwardQuadrics.quadrics.size(), reversedQuadrics.quadrics.size());
    manumesh::Mat4 forwardBoundaryTerms = forwardQuadrics.quadrics.front();
    manumesh::Mat4 reversedBoundaryTerms = reversedQuadrics.quadrics.front();
    // Coplanar face quadrics only contribute to the z-z entry. Removing that
    // entry isolates the many boundary terms accumulated at the shared vertex.
    forwardBoundaryTerms.row(2).setZero();
    forwardBoundaryTerms.col(2).setZero();
    reversedBoundaryTerms.row(2).setZero();
    reversedBoundaryTerms.col(2).setZero();
    EXPECT_TRUE(forwardBoundaryTerms.isApprox(reversedBoundaryTerms, 0.0));
}

TEST(FeatureBoost, AdaptiveScaleKeepsQuadricsCleanAndFillsPriorityScales) {
    const Mesh input = manumesh::generateRidgeGrid(16, 2.0, 0.6);
    manumesh::simplification::SimplifyOptions boosted;
    boosted.useLineQuadrics = true;
    boosted.lineWeight = 1e-3;
    boosted.weightMode = manumesh::simplification::WeightMode::NormalTensor;
    boosted.normalTensorFeatureThreshold = 0.04;
    boosted.adaptiveScale = true;
    boosted.adaptiveBaseLineWeight = 1e-2;
    boosted.featureBoost = 0.5;
    manumesh::simplification::SimplifyOptions unboosted = boosted;
    unboosted.featureBoost = 0.0;

    manumesh::simplification::FeatureGuidance guidance;
    manumesh::simplification::SimplifyReport boostedReport;
    manumesh::simplification::SimplifyReport unboostedReport;
    manumesh::simplification::InitialQuadrics boostedQ;
    manumesh::simplification::InitialQuadrics unboostedQ;
    manumesh::simplification::computeInitialQuadrics(input, boosted, guidance, boostedQ, boostedReport);
    manumesh::simplification::computeInitialQuadrics(input, unboosted, guidance, unboostedQ, unboostedReport);

    ASSERT_EQ(boostedQ.quadrics.size(), unboostedQ.quadrics.size());
    for (std::size_t i = 0; i < boostedQ.quadrics.size(); ++i) {
        EXPECT_TRUE(boostedQ.quadrics[i].isApprox(unboostedQ.quadrics[i], 0.0))
            << "quadric " << i << " was distorted by featureBoost";
    }

    ASSERT_EQ(boostedQ.priorityScales.size(), input.vertices.size());
    double maxScale = 0.0;
    for (double scale : boostedQ.priorityScales) {
        EXPECT_GE(scale, 1.0);
        maxScale = std::max(maxScale, scale);
    }
    EXPECT_GT(maxScale, 1.0);
    for (double scale : unboostedQ.priorityScales) {
        EXPECT_DOUBLE_EQ(1.0, scale);
    }
    EXPECT_DOUBLE_EQ(boostedReport.minAppliedLineWeight, boostedReport.maxAppliedLineWeight);
    EXPECT_DOUBLE_EQ(boostedReport.minAppliedLineWeight, unboostedReport.minAppliedLineWeight);
}

TEST(FeatureBoost, SmoothCurvatureAdaptiveScaleUsesPersistentVertexWeights) {
    const Mesh input = manumesh::generateBumpGrid(24, 2.0);

    manumesh::simplification::SimplifyOptions options;
    options.useLineQuadrics = true;
    options.lineWeight = 1e-3;
    options.adaptiveScale = true;
    options.adaptiveBaseLineWeight = 0.02;
    options.weightMode = manumesh::simplification::WeightMode::SmoothCurvature;
    options.featureBoost = 0.5;
    options.featureAngleDeg = 180.0;
    options.loopTraceAngleDeg = 180.0;
    options.useNormalTensorFeatures = false;
    options.useSmoothCurvatureFeatures = true;
    options.smoothCurvatureFeatureThreshold = 0.008;
    options.smoothCurvatureMinEdgeAlignment = 0.45;
    options.smoothCurvatureMinTangentConsistency = 0.55;
    options.smoothCurvatureBaseNeighborhoodRings = 2;
    options.smoothCurvatureScaleCount = 3;
    options.smoothCurvatureMinPersistentScales = 2;
    options.smoothCurvatureRobustFitIterations = 2;

    const manumesh::simplification::FeatureWeightScores scores =
        manumesh::simplification::computeFeatureWeightScores(input, options);
    ASSERT_EQ(input.vertices.size(), scores.values.size());
    EXPECT_GT(scores.smoothCurvatureScoredVertices, 0);
    EXPECT_GT(scores.maxSmoothCurvaturePersistentScore, options.smoothCurvatureFeatureThreshold);
    EXPECT_GT(scores.meanSmoothCurvatureScaleStability, 0.0);

    manumesh::simplification::SimplifyOptions unboosted = options;
    unboosted.featureBoost = 0.0;
    manumesh::simplification::FeatureGuidance guidance;
    manumesh::simplification::InitialQuadrics boostedQuadrics;
    manumesh::simplification::InitialQuadrics unboostedQuadrics;
    manumesh::simplification::SimplifyReport boostedReport;
    manumesh::simplification::SimplifyReport unboostedReport;
    manumesh::simplification::computeInitialQuadrics(input, options, guidance, boostedQuadrics, boostedReport);
    manumesh::simplification::computeInitialQuadrics(input, unboosted, guidance, unboostedQuadrics, unboostedReport);

    ASSERT_EQ(input.vertices.size(), boostedQuadrics.priorityScales.size());
    ASSERT_EQ(boostedQuadrics.quadrics.size(), unboostedQuadrics.quadrics.size());
    bool foundBoostedVertex = false;
    for (std::size_t vertex = 0; vertex < input.vertices.size(); ++vertex) {
        EXPECT_TRUE(boostedQuadrics.quadrics[vertex].isApprox(unboostedQuadrics.quadrics[vertex], 0.0))
            << "quadric " << vertex << " changed under adaptive smooth-curvature boost";
        EXPECT_NEAR(1.0 + options.featureBoost * scores.values[vertex], boostedQuadrics.priorityScales[vertex], 1e-12)
            << "vertex=" << vertex;
        EXPECT_DOUBLE_EQ(1.0, unboostedQuadrics.priorityScales[vertex]);
        foundBoostedVertex = foundBoostedVertex || boostedQuadrics.priorityScales[vertex] > 1.0;
    }
    EXPECT_TRUE(foundBoostedVertex);
    EXPECT_DOUBLE_EQ(options.adaptiveBaseLineWeight, boostedReport.minAppliedLineWeight);
    EXPECT_DOUBLE_EQ(options.adaptiveBaseLineWeight, boostedReport.maxAppliedLineWeight);
    EXPECT_EQ(scores.smoothCurvatureScoredVertices, boostedReport.smoothCurvatureScoredVertices);
    EXPECT_DOUBLE_EQ(scores.maxSmoothCurvaturePersistentScore, boostedReport.maxSmoothCurvaturePersistentScore);
    EXPECT_DOUBLE_EQ(scores.meanSmoothCurvatureScaleStability, boostedReport.meanSmoothCurvatureScaleStability);
}

TEST(FeatureBoost, SmoothCurvatureWeightModeReusesPrecomputedVertexWeights) {
    const Mesh input = manumesh::generateBumpGrid(24, 2.0);
    manumesh::feature::FeatureOptions detectionOptions;
    detectionOptions.featureAngleDeg = 180.0;
    detectionOptions.loopTraceAngleDeg = 180.0;
    detectionOptions.useNormalTensorFeatures = false;
    detectionOptions.useSmoothCurvatureFeatures = true;
    detectionOptions.smoothCurvatureFeatureThreshold = 0.008;
    detectionOptions.smoothCurvatureMinEdgeAlignment = 0.45;
    detectionOptions.smoothCurvatureMinTangentConsistency = 0.55;
    detectionOptions.smoothCurvatureBaseNeighborhoodRings = 2;
    detectionOptions.smoothCurvatureScaleCount = 3;
    detectionOptions.smoothCurvatureMinPersistentScales = 2;
    detectionOptions.smoothCurvatureRobustFitIterations = 2;
    const manumesh::feature::FeatureAnalysis analysis = manumesh::feature::detectFeatureCurves(input, detectionOptions);

    ASSERT_EQ(input.vertices.size(), analysis.smoothCurvatureVertexWeights.size());
    ASSERT_GT(
        *std::max_element(analysis.smoothCurvatureVertexWeights.begin(), analysis.smoothCurvatureVertexWeights.end()),
        0.0
    );

    manumesh::simplification::SimplifyOptions options;
    options.useLineQuadrics = true;
    options.lineWeight = 1e-3;
    options.adaptiveScale = true;
    options.adaptiveBaseLineWeight = 0.02;
    options.weightMode = manumesh::simplification::WeightMode::SmoothCurvature;
    options.featureBoost = 0.5;
    manumesh::feature::FeatureOptions conflictingOptions = detectionOptions;
    conflictingOptions.smoothCurvatureFeatureThreshold = 1.0;
    conflictingOptions.smoothCurvatureScaleCount = 1;
    conflictingOptions.smoothCurvatureMinPersistentScales = 1;
    options.featureOptionsOverride = conflictingOptions;

    const manumesh::simplification::FeatureWeightScores recomputed =
        manumesh::simplification::computeFeatureWeightScores(input, options);
    ASSERT_EQ(input.vertices.size(), recomputed.values.size());
    EXPECT_LT(*std::max_element(recomputed.values.begin(), recomputed.values.end()), 1e-12);

    const manumesh::simplification::FeatureWeightScores reused =
        manumesh::simplification::computeFeatureWeightScores(input, options, &analysis);
    EXPECT_EQ(analysis.smoothCurvatureVertexWeights, reused.values);
    EXPECT_EQ(analysis.smoothCurvatureScoredVertices, reused.smoothCurvatureScoredVertices);
    EXPECT_DOUBLE_EQ(analysis.maxSmoothCurvaturePersistentScore, reused.maxSmoothCurvaturePersistentScore);
    EXPECT_DOUBLE_EQ(analysis.meanSmoothCurvatureLocalScale, reused.meanSmoothCurvatureLocalScale);
    EXPECT_DOUBLE_EQ(analysis.meanSmoothCurvaturePersistence, reused.meanSmoothCurvaturePersistence);
    EXPECT_DOUBLE_EQ(analysis.meanSmoothCurvatureScaleStability, reused.meanSmoothCurvatureScaleStability);

    manumesh::simplification::FeatureGuidance guidance;
    manumesh::simplification::InitialQuadrics initial;
    manumesh::simplification::SimplifyReport report;
    manumesh::simplification::computeInitialQuadrics(input, options, guidance, &analysis, initial, report);
    ASSERT_EQ(input.vertices.size(), initial.priorityScales.size());
    for (std::size_t vertex = 0; vertex < input.vertices.size(); ++vertex) {
        EXPECT_NEAR(
            1.0 + options.featureBoost * analysis.smoothCurvatureVertexWeights[vertex],
            initial.priorityScales[vertex],
            1e-12
        ) << "vertex="
          << vertex;
    }
    EXPECT_EQ(analysis.smoothCurvatureScoredVertices, report.smoothCurvatureScoredVertices);
    EXPECT_DOUBLE_EQ(analysis.meanSmoothCurvatureScaleStability, report.meanSmoothCurvatureScaleStability);
}

TEST(FeatureBoost, SmoothCurvatureWeightModeUsesResolvedNormalFilter) {
    const manumesh::test::analytic::GaussianRidgeSheetFixture ridge =
        manumesh::test::analytic::makeGaussianRidgeSheet(48, 2.0, 0.35, 6.0);
    const Mesh input = manumesh::test::analytic::withDeterministicNoise(
        ridge.mesh, 0.04 * manumesh::test::analytic::meanEdgeLength(ridge.mesh), 20260819u
    );

    manumesh::feature::FeatureOptions featureOptions;
    featureOptions.featureAngleDeg = 180.0;
    featureOptions.loopTraceAngleDeg = 180.0;
    featureOptions.useNormalTensorFeatures = false;
    featureOptions.useSmoothCurvatureFeatures = true;
    featureOptions.smoothCurvatureFeatureThreshold = 0.008;
    featureOptions.smoothCurvatureMinEdgeAlignment = 0.45;
    featureOptions.smoothCurvatureMinTangentConsistency = 0.55;
    featureOptions.smoothCurvatureBaseNeighborhoodRings = 2;
    featureOptions.smoothCurvatureScaleCount = 3;
    featureOptions.smoothCurvatureMinPersistentScales = 2;
    featureOptions.smoothCurvatureRobustFitIterations = 2;
    featureOptions.normalFilter.enabled = true;
    featureOptions.normalFilter.iterations = 4;
    featureOptions.normalFilter.angleSigmaDeg = 18.0;
    featureOptions.normalFilter.preserveAngleDeg = 55.0;

    const manumesh::feature::FeatureAnalysis analysis = manumesh::feature::detectFeatureCurves(input, featureOptions);
    ASSERT_EQ(input.vertices.size(), analysis.smoothCurvatureVertexWeights.size());
    EXPECT_GT(analysis.normalFilter.changedFaces, 0);

    manumesh::simplification::SimplifyOptions options;
    options.weightMode = manumesh::simplification::WeightMode::SmoothCurvature;
    options.featureOptionsOverride = featureOptions;
    const manumesh::simplification::FeatureWeightScores scores =
        manumesh::simplification::computeFeatureWeightScores(input, options);

    EXPECT_EQ(analysis.smoothCurvatureVertexWeights, scores.values);
    EXPECT_EQ(analysis.smoothCurvatureScoredVertices, scores.smoothCurvatureScoredVertices);
    EXPECT_DOUBLE_EQ(analysis.maxSmoothCurvaturePersistentScore, scores.maxSmoothCurvaturePersistentScore);
    EXPECT_DOUBLE_EQ(analysis.meanSmoothCurvatureLocalScale, scores.meanSmoothCurvatureLocalScale);
    EXPECT_DOUBLE_EQ(analysis.meanSmoothCurvaturePersistence, scores.meanSmoothCurvaturePersistence);
    EXPECT_DOUBLE_EQ(analysis.meanSmoothCurvatureScaleStability, scores.meanSmoothCurvatureScaleStability);
}

TEST(FeatureBoost, AdaptiveScaleUsesItsOwnBaseLineWeight) {
    const Mesh input = manumesh::generateRidgeGrid(8, 2.0, 0.6);
    manumesh::simplification::SimplifyOptions zeroLegacyWeight;
    zeroLegacyWeight.useLineQuadrics = true;
    zeroLegacyWeight.lineWeight = 0.0;
    zeroLegacyWeight.adaptiveScale = true;
    zeroLegacyWeight.adaptiveBaseLineWeight = 0.02;

    manumesh::simplification::SimplifyOptions nonzeroLegacyWeight = zeroLegacyWeight;
    nonzeroLegacyWeight.lineWeight = 1e-3;

    manumesh::simplification::FeatureGuidance guidance;
    manumesh::simplification::InitialQuadrics zeroLegacyQuadrics;
    manumesh::simplification::InitialQuadrics nonzeroLegacyQuadrics;
    manumesh::simplification::SimplifyReport zeroLegacyReport;
    manumesh::simplification::SimplifyReport nonzeroLegacyReport;
    manumesh::simplification::computeInitialQuadrics(
        input, zeroLegacyWeight, guidance, zeroLegacyQuadrics, zeroLegacyReport
    );
    manumesh::simplification::computeInitialQuadrics(
        input, nonzeroLegacyWeight, guidance, nonzeroLegacyQuadrics, nonzeroLegacyReport
    );

    ASSERT_EQ(zeroLegacyQuadrics.quadrics.size(), nonzeroLegacyQuadrics.quadrics.size());
    for (std::size_t i = 0; i < zeroLegacyQuadrics.quadrics.size(); ++i) {
        EXPECT_TRUE(zeroLegacyQuadrics.quadrics[i].isApprox(nonzeroLegacyQuadrics.quadrics[i], 0.0));
    }
    EXPECT_DOUBLE_EQ(0.02, zeroLegacyReport.minAppliedLineWeight);
    EXPECT_DOUBLE_EQ(0.02, zeroLegacyReport.maxAppliedLineWeight);
    EXPECT_EQ(input.vertices.size(), zeroLegacyQuadrics.priorityScales.size());
}
