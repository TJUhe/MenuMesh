/**
 * @file tests/unit/feature_detection/feature_detection_smooth_curvature_tests.cpp
 * @brief Verifies feature detection smooth curvature tests behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

#include "FeatureDetectionTestSupport.h"

#include "core/MeshGenerators.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

namespace manumesh::test::feature_detection {
namespace {

int persistentVertexCount(const std::vector<feature::SmoothCurvatureVertex>& values, int minimumScales) {
    return static_cast<int>(std::count_if(values.begin(), values.end(), [&](const auto& value) {
        return value.persistentScales >= minimumScales && value.persistentFeatureScore > 0.0;
    }));
}

double maximumPersistentScore(const std::vector<feature::SmoothCurvatureVertex>& values) {
    double maximum = 0.0;
    for (const auto& value : values) {
        maximum = std::max(maximum, value.persistentFeatureScore);
    }
    return maximum;
}

Mesh uniformlyScaled(Mesh mesh, double scale) {
    for (Vec3& vertex : mesh.vertices) {
        vertex *= scale;
    }
    return mesh;
}

Mesh makeGaussianRidgeGrid(int subdivisions, double size, double height, double sharpness) {
    Mesh mesh = generatePlaneGrid(subdivisions, size, false);
    for (Vec3& vertex : mesh.vertices) {
        vertex.z() = height * std::exp(-sharpness * vertex.x() * vertex.x());
    }
    return mesh;
}

FeatureOptions smoothFeatureOptions() {
    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 180.0;
    options.loopTraceAngleDeg = 180.0;
    options.useSmoothCurvatureFeatures = true;
    options.smoothCurvatureFeatureThreshold = 0.008;
    options.smoothCurvatureMinEdgeAlignment = 0.45;
    options.smoothCurvatureMinTangentConsistency = 0.55;
    options.smoothCurvatureBaseNeighborhoodRings = 2;
    options.smoothCurvatureScaleCount = 3;
    options.smoothCurvatureMinPersistentScales = 2;
    options.smoothCurvatureRobustFitIterations = 2;
    return options;
}

} // namespace

TEST(FeatureDetection, SmoothCurvatureRejectsExactPlaneAndFindsSmoothBump) {
    const feature::SmoothCurvatureOptions options{2, 3, 2, 0.55};
    const auto plane = feature::computeSmoothCurvatureFeatures(generatePlaneGrid(24, 2.0, false), options, 0.008);
    const auto bump = feature::computeSmoothCurvatureFeatures(generateBumpGrid(32, 2.0), options, 0.008);

    EXPECT_LT(maximumPersistentScore(plane), 1e-10);
    EXPECT_GT(maximumPersistentScore(bump), 0.008);
    EXPECT_GT(persistentVertexCount(bump, 2), 8);

    const FeatureOptions graphOptions = smoothFeatureOptions();
    const FeatureAnalysis bumpGraph = feature::detectFeatureCurves(generateBumpGrid(32, 2.0), graphOptions);
    const FeatureAnalysis noisyGraph =
        feature::detectFeatureCurves(generateNoisyPlaneGrid(32, 2.0, 0.008), graphOptions);
    EXPECT_GT(bumpGraph.smoothCurvatureFeatureEdges, 0);
    EXPECT_LT(noisyGraph.smoothCurvatureFeatureEdges, bumpGraph.smoothCurvatureFeatureEdges);
    // Tighter separation requirement than the raw comparison above: the
    // noise response must stay at most 3/4 of the structured response. The
    // noisy plane (deterministic mt19937 seed 42 in generateNoisyPlaneGrid)
    // currently measures 60 smooth-curvature edges versus 112 on the bump
    // (ratio 0.54); the 0.75 bound keeps ~1.4x margin to that measurement
    // while still failing if the detector degenerates to near-equal noise
    // and signal counts, which the plain EXPECT_LT would let through.
    EXPECT_LE(4 * noisyGraph.smoothCurvatureFeatureEdges, 3 * bumpGraph.smoothCurvatureFeatureEdges);
}

TEST(FeatureDetection, SmoothCurvatureEvidenceIsInvariantUnderUniformScaling) {
    const Mesh bump = generateBumpGrid(28, 2.0);
    const Mesh scaled = uniformlyScaled(bump, 7.5);
    const feature::SmoothCurvatureOptions options{2, 3, 2, 0.55};
    const auto original = feature::computeSmoothCurvatureFeatures(bump, options, 0.008);
    const auto enlarged = feature::computeSmoothCurvatureFeatures(scaled, options, 0.008);

    EXPECT_EQ(persistentVertexCount(original, 2), persistentVertexCount(enlarged, 2));
    EXPECT_NEAR(maximumPersistentScore(original), maximumPersistentScore(enlarged), 1e-8);
}

TEST(FeatureDetection, SmoothCurvaturePersistenceSuppressesSingleScaleCandidates) {
    // Precondition: the bump grid must carry vertices whose candidate is
    // supported by exactly 2 of the 3 scales while clearing the score
    // threshold. Those vertices pass the minPersistentScales = 2 gate but are
    // exactly the ones minPersistentScales = 3 removes.
    const Mesh bump = generateBumpGrid(28, 2.0);
    const feature::SmoothCurvatureOptions curvatureOptions{2, 3, 2, 0.55};
    const auto values = feature::computeSmoothCurvatureFeatures(bump, curvatureOptions, 0.008);
    const int twoScaleCandidates = static_cast<int>(std::count_if(values.begin(), values.end(), [](const auto& value) {
        return value.persistentScales == 2 && value.persistentFeatureScore >= 0.008;
    }));
    ASSERT_GT(twoScaleCandidates, 0);

    FeatureOptions permissive = smoothFeatureOptions();
    permissive.smoothCurvatureMinPersistentScales = 2;
    const FeatureAnalysis permissiveResult = feature::detectFeatureCurves(bump, permissive);

    FeatureOptions strict = permissive;
    strict.smoothCurvatureMinPersistentScales = 3;
    const FeatureAnalysis strictResult = feature::detectFeatureCurves(bump, strict);

    // Both settings keep the dome ring (fully persistent evidence), but the
    // stricter gate must actually drop the partially persistent edges: the
    // count decreases strictly, not merely non-strictly, because the
    // two-scale candidates asserted above lose every incident
    // smooth-curvature edge under minPersistentScales = 3.
    EXPECT_GT(strictResult.smoothCurvatureFeatureEdges, 0);
    EXPECT_LT(strictResult.smoothCurvatureFeatureEdges, permissiveResult.smoothCurvatureFeatureEdges);
}

TEST(FeatureDetection, SmoothCurvatureEdgesRemainDistinctInFeatureGraph) {
    const FeatureAnalysis analysis = feature::detectFeatureCurves(generateBumpGrid(32, 2.0), smoothFeatureOptions());

    EXPECT_GT(analysis.smoothCurvatureScoredVertices, 0);
    EXPECT_GT(analysis.maxSmoothCurvaturePersistentScore, 0.008);
    EXPECT_GT(analysis.meanSmoothCurvatureLocalScale, 0.0);
    EXPECT_GT(analysis.smoothCurvatureFeatureEdges, 0);
    EXPECT_EQ(
        analysis.smoothCurvatureFeatureEdges,
        static_cast<int>(std::count_if(analysis.graph.edges.begin(), analysis.graph.edges.end(), [](const auto& edge) {
            return edge.smoothCurvature;
        }))
    );
}

TEST(FeatureDetection, SmoothCurvatureGraphRecoversLabeledGaussianRidge) {
    constexpr int subdivisions = 32;
    const Mesh mesh = makeGaussianRidgeGrid(subdivisions, 2.0, 0.24, 24.0);
    FeatureOptions options = smoothFeatureOptions();
    options.smoothCurvatureFeatureThreshold = 0.015;
    options.smoothCurvatureMinEdgeAlignment = 0.85;
    options.smoothCurvatureMinTangentConsistency = 0.70;
    const FeatureAnalysis analysis = feature::detectFeatureCurves(mesh, options);

    constexpr double size = 2.0;
    const double spacing = size / static_cast<double>(subdivisions);
    // Ground-truth curve locations for the Gaussian ridge z = h e^{-s x^2}
    // (h = 0.24, s = 24). In the small-slope regime the cross-section
    // curvature is kappa ~ z'' = h (4 s^2 x^2 - 2 s) e^{-s x^2}, and ridge /
    // valley lines are the extrema of kappa along x:
    //   dkappa/dx = 4 h s^2 x (3 - 2 s x^2) e^{-s x^2} = 0
    // at x = 0 (the crest, kappa minimum) and x = +/- sqrt(3 / (2 s))
    // (the flanking shoulder lines, kappa maxima). With s = 24 that is
    // sqrt(3 / 48) = sqrt(1 / 16) = 0.25 exactly.
    const std::array<double, 3> featureX = {-0.25, 0.0, 0.25};

    int detected = 0;
    int truePositive = 0;
    std::unordered_set<int> coveredSegments;
    for (const feature::FeatureGraphEdge& edge : analysis.graph.edges) {
        if (!edge.smoothCurvature || edge.removedByCleanup) {
            continue;
        }
        ++detected;
        const Vec3 midpoint = 0.5 * (mesh.vertices[edge.a] + mesh.vertices[edge.b]);
        int nearestCurve = 0;
        double nearestDistance = std::abs(midpoint.x() - featureX[0]);
        for (int curve = 1; curve < static_cast<int>(featureX.size()); ++curve) {
            const double distance = std::abs(midpoint.x() - featureX[curve]);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestCurve = curve;
            }
        }
        if (nearestDistance <= 1.1 * spacing) {
            ++truePositive;
            const int ySegment = static_cast<int>(std::lround((midpoint.y() + 0.5 * size) / spacing - 0.5));
            if (ySegment >= 1 && ySegment + 1 < subdivisions) {
                coveredSegments.insert(nearestCurve * subdivisions + ySegment);
            }
        }
    }
    const double precision = detected > 0 ? static_cast<double>(truePositive) / static_cast<double>(detected) : 0.0;
    const int expectedSegments = static_cast<int>(featureX.size()) * (subdivisions - 2);
    const double recall = static_cast<double>(coveredSegments.size()) / static_cast<double>(expectedSegments);

    EXPECT_GT(precision, 0.70);
    EXPECT_GT(recall, 0.55);
}

TEST(FeatureDetection, RejectsInvalidSmoothCurvatureOptions) {
    FeatureOptions options = smoothFeatureOptions();
    options.smoothCurvatureMinTangentConsistency = 1.1;
    EXPECT_THROW(feature::validateFeatureOptions(options), std::invalid_argument);

    options = smoothFeatureOptions();
    options.smoothCurvatureScaleCount = 0;
    EXPECT_THROW(feature::validateFeatureOptions(options), std::invalid_argument);

    options = smoothFeatureOptions();
    options.smoothCurvatureRobustFitIterations = -1;
    EXPECT_THROW(feature::validateFeatureOptions(options), std::invalid_argument);
}

} // namespace manumesh::test::feature_detection
