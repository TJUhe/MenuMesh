// Feature detection against analytic fixtures with closed-form ground truth.
//
// The assertions here are derived from the differential geometry of the
// fixtures rather than from recorded outputs:
//  - Spheres, cylinders and tori are Dupin cyclides, so the curvature
//    extremality e = grad(kappa) . t vanishes identically on them and the
//    smooth ridge/valley channel must stay silent (Yoshizawa 2005, M021).
//  - A Gaussian ridge sheet has an exact crest curvature |f''(0)| = 2*h*s,
//    giving a quantitative accuracy target for the Monge fit.
//  - The capped cylinder and the chamfer box have exact hard-edge sets and
//    exact rim circles, giving edge-level recall/precision and circle-fit
//    error bounds.
#include "AnalyticFixtures.h"
#include "FeatureDetectionTestSupport.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace manumesh::test::feature_detection {
namespace {

namespace analytic = manumesh::test::analytic;

double maximumPersistentScore(const std::vector<feature::SmoothCurvatureVertex>& values) {
    double maximum = 0.0;
    for (const auto& value : values) {
        maximum = std::max(maximum, value.persistentFeatureScore);
    }
    return maximum;
}

double median(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }
    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle), values.end());
    return values[middle];
}

/// Feature options that expose only the smooth-curvature channel: the
/// dihedral threshold is parked at 180 degrees so no hard edge can fire.
FeatureOptions smoothChannelOptions() {
    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 180.0;
    options.loopTraceAngleDeg = 180.0;
    options.useSmoothCurvatureFeatures = true;
    options.smoothCurvatureFeatureThreshold = 0.008;
    options.smoothCurvatureMinEdgeAlignment = 0.45;
    options.smoothCurvatureMinTangentConsistency = 0.55;
    return options;
}

FeatureOptions rimDetectionOptions(double circleFitRelativeThreshold = 0.05) {
    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 40.0;
    options.minFeatureLoopVertices = 8;
    options.circleFitRelativeThreshold = circleFitRelativeThreshold;
    return options;
}

int countCircular(const FeatureAnalysis& analysis) {
    return static_cast<int>(
        std::count_if(analysis.loops.begin(), analysis.loops.end(), [](const FeatureLoop& loop) {
            return loop.circular;
        })
    );
}

const FeatureLoop* circularLoopNearestTo(const FeatureAnalysis& analysis, const Vec3& center) {
    const FeatureLoop* best = nullptr;
    double bestDistance = 0.0;
    for (const FeatureLoop& loop : analysis.loops) {
        if (!loop.circular) {
            continue;
        }
        const double distance = (loop.center - center).norm();
        if (best == nullptr || distance < bestDistance) {
            best = &loop;
            bestDistance = distance;
        }
    }
    return best;
}

/// Active (non-removed) graph edges as sorted index pairs, sorted overall.
std::vector<std::pair<int, int>> activeGraphEdges(const FeatureAnalysis& analysis) {
    std::vector<std::pair<int, int>> edges;
    for (const feature::FeatureGraphEdge& edge : analysis.graph.edges) {
        if (edge.removedByCleanup || edge.a < 0 || edge.b < 0) {
            continue;
        }
        edges.emplace_back(std::min(edge.a, edge.b), std::max(edge.a, edge.b));
    }
    std::sort(edges.begin(), edges.end());
    return edges;
}

/// Fraction of the given crest vertices carrying persistent ridge evidence
/// that clears the downstream edge gate (persistentScales >= minScales and
/// persistentFeatureScore > threshold, the same predicate
/// smoothCurvatureEdgeCandidate applies per endpoint).
double crestPersistentRecall(
    const std::vector<feature::SmoothCurvatureVertex>& values,
    const std::vector<int>& crestVertices,
    int minScales,
    double threshold
) {
    if (crestVertices.empty()) {
        return 0.0;
    }
    int detected = 0;
    for (int vertex : crestVertices) {
        const feature::SmoothCurvatureVertex& value = values[vertex];
        if (value.persistentScales >= minScales && value.persistentFeatureScore > threshold) {
            ++detected;
        }
    }
    return static_cast<double>(detected) / static_cast<double>(crestVertices.size());
}

} // namespace

// On a sphere every direction is principal and kappa is constant; on a
// cylinder wall the principal curvatures are constant everywhere. Both are
// Dupin cyclides, where the extremality e = grad(kappa) . t is identically
// zero, so the smooth ridge/valley detector must produce no persistent
// evidence and no feature edges. Both fixtures are sampled at the same
// relative density h/r ~= 0.13 (h = edge length, r = curvature radius), where
// the cubic-fit truncation noise in e sits several orders of magnitude below
// the working threshold 0.008; the asserted ceiling 8e-4 is 10x below it.
// The torus, the third cyclide with a definable inner side, is covered by the
// companion test below.
TEST(FeatureDetectionAnalytic, SphereAndCylinderProduceNoSmoothCurvatureFeatures) {
    const analytic::SphereFixture sphere = analytic::makeUvSphere(24, 48, 1.0);
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(48, 12, 1.0, 2.0, false);

    const feature::SmoothCurvatureOptions curvatureOptions{2, 3, 2, 0.55};
    const FeatureOptions graphOptions = smoothChannelOptions();
    struct Case {
        const char* name;
        const Mesh* mesh;
    };
    const Case cases[] = {
        {"sphere", &sphere.mesh},
        {"cylinder", &cylinder.mesh},
    };
    for (const Case& item : cases) {
        SCOPED_TRACE(item.name);
        const auto values = feature::computeSmoothCurvatureFeatures(*item.mesh, curvatureOptions, 0.008);
        EXPECT_LT(maximumPersistentScore(values), 8e-4);

        const FeatureAnalysis analysis = feature::detectFeatureCurves(*item.mesh, graphOptions);
        EXPECT_EQ(0, analysis.smoothCurvatureFeatureEdges);
    }
}

// On a torus (R=1, r=0.3) the inner (saddle) side used to produce a
// persistent spurious valley band even though e == 0 analytically: cubic-fit
// truncation error in e, amplified by the saddle geometry, produced zero
// crossings with max persistent scores of 0.097/0.038/0.026/0.011 at 24/32/
// 36/48 minor segments against the working threshold 0.008 (only ~96
// segments per circle went silent). The fix is the per-crossing cyclideness
// gate of Yoshizawa (M021 Eq.5-6): the interpolated cyclideness
// mean(|e_endpoints|) must exceed a fixed fraction of kappa^2 (dimensionless
// and uniform-scale invariant) for a crossing to count, which vanishes
// identically on Dupin cyclides while true crests sit orders of magnitude
// above it. After the gate the torus is exactly silent at all tested
// densities (measured max persistent score 0.0 for 24-48 segments).
TEST(FeatureDetectionAnalytic, TorusInnerSideProducesNoSmoothCurvatureFeatures) {
    const analytic::TorusFixture torus = analytic::makeTorus(48, 24, 1.0, 0.3);
    const feature::SmoothCurvatureOptions curvatureOptions{2, 3, 2, 0.55};
    const auto values = feature::computeSmoothCurvatureFeatures(torus.mesh, curvatureOptions, 0.008);
    EXPECT_LT(maximumPersistentScore(values), 8e-4);

    const FeatureAnalysis analysis = feature::detectFeatureCurves(torus.mesh, smoothChannelOptions());
    EXPECT_EQ(0, analysis.smoothCurvatureFeatureEdges);
}

// Quantitative accuracy of the Monge fit at a smooth ridge crest. The sheet
// z = h*exp(-s*x^2) has exact crest curvature 2*h*s. With h = 0.35, s = 6 and
// a pinned single fit scale of 2 rings (radius rho ~ 2 * (size/n) = 0.083),
// the dominant error is neighborhood averaging of the curvature profile:
// kappa(x)/kappa(0) = (1 - 2*s*x^2) * exp(-s*x^2) * (1 + f'(x)^2)^(-3/2),
// which drops to ~0.85 at x = rho. The Gaussian fit weight exp(-r^2/(2rho^2))
// halves that bias at the weighted mean, so the expected relative bias is
// ~7-10%; the median bound of 15% leaves margin without hiding regressions.
TEST(FeatureDetectionAnalytic, GaussianRidgeCrestCurvatureMatchesAnalyticProfile) {
    const analytic::GaussianRidgeSheetFixture ridge = analytic::makeGaussianRidgeSheet(48, 2.0, 0.35, 6.0);
    const double crestCurvature = ridge.analyticCrestCurvature();
    ASSERT_GT(crestCurvature, 0.0);

    // Single scale, no robust reweighting: pins the fit radius so the
    // truncation-bias bound above applies to every sample.
    const auto values =
        feature::computeSmoothCurvatureFeatures(ridge.mesh, feature::SmoothCurvatureOptions{2, 1, 0, 0.55}, 1e-6);

    std::vector<double> relativeErrors;
    std::vector<double> tangentAlignments;
    for (int vertex : ridge.interiorCrestVertices()) {
        const feature::SmoothCurvatureVertex& value = values[vertex];
        if (value.signedKind == 0 || value.localScale <= 0.0) {
            continue;
        }
        // principalCurvature is reported in fit-radius-normalized units
        // (coordinates are divided by localScale before fitting), so the
        // metric curvature estimate is principalCurvature / localScale.
        const double estimated = std::abs(value.principalCurvature) / value.localScale;
        relativeErrors.push_back(std::abs(estimated - crestCurvature) / crestCurvature);
        tangentAlignments.push_back(std::abs(value.curveTangent.dot(ridge.crestTangent())));
    }

    // The crest column must actually be detected as a ridge/valley extremum:
    // the zero crossing of e sits on the crest by symmetry.
    EXPECT_GT(static_cast<int>(relativeErrors.size()), static_cast<int>(ridge.interiorCrestVertices().size()) / 2);
    EXPECT_LT(median(relativeErrors), 0.15);
    // The recovered curve tangent must follow the ridge axis: the fixture is
    // translationally invariant along y, so misalignment is pure noise.
    EXPECT_GT(median(tangentAlignments), 0.9);
}

// The capped cylinder has two exact 90-degree rim circles whose vertices lie
// exactly on the analytic circles. A complete, uniformly sampled ring is the
// best case for the algebraic circle fit (it is exact up to round-off), so
// the recovered center/radius/normal must match the ground truth far tighter
// than the generic 2%-of-radius acceptance bound; 1e-7 relative keeps ~7
// orders of magnitude of margin while still allowing double round-off
// accumulation over 64 samples.
TEST(FeatureDetectionAnalytic, CappedCylinderRimsRecoverAsTwoExactCircles) {
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(64, 6, 1.0, 2.0, true);
    const FeatureAnalysis analysis = feature::detectFeatureCurves(cylinder.mesh, rimDetectionOptions());

    ASSERT_EQ(2, countCircular(analysis));
    for (const analytic::GroundTruthCircle& truth : cylinder.groundTruthCircles()) {
        SCOPED_TRACE(truth.center.z());
        const FeatureLoop* loop = circularLoopNearestTo(analysis, truth.center);
        ASSERT_NE(nullptr, loop);
        EXPECT_TRUE(loop->closed);
        EXPECT_EQ(cylinder.segments, loop->edgeCount);
        EXPECT_NEAR(truth.radius, loop->radius, 1e-7 * truth.radius);
        EXPECT_LT((loop->center - truth.center).norm(), 1e-7 * truth.radius);
        // The fitted plane normal is defined up to sign.
        EXPECT_GT(std::abs(loop->normal.dot(truth.normal)), 1.0 - 1e-9);
    }
}

// The chamfer box's hard-edge set is known exactly: 8 vertical crease lines
// (45-degree dihedral, the two parallel replacements of each chamfered box
// edge) plus the two octagonal rims (90-degree dihedral). With a detection
// threshold of 30 degrees both classes clear the threshold with >= 15 degrees
// of margin and the flat faces provide zero competing evidence, so edge-level
// recall and precision must be essentially perfect (>= 0.99; the task-level
// acceptance bound is 0.9). Junction recall must also be perfect: all 16
// octagon corners have feature-graph valence 3. Junction *precision* is
// asserted in the companion test below.
TEST(FeatureDetectionAnalytic, ChamferBoxHardEdgesRecoverWithHighPrecisionAndRecall) {
    const analytic::ChamferBoxFixture box = analytic::makeChamferBox(2.0, 0.3, 6);
    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 30.0;
    options.minFeatureLoopVertices = 8;
    const FeatureAnalysis analysis = feature::detectFeatureCurves(box.mesh, options);

    // Ground-truth junctions: the 16 octagon corners where a vertical crease
    // meets a rim (feature-graph valence 3).
    std::vector<int> junctions;
    for (int corner = 0; corner < 8; ++corner) {
        junctions.push_back(corner);
        junctions.push_back(box.divisions * 8 + corner);
    }
    const feature::FeatureEdgeBenchmark benchmark =
        feature::benchmarkFeatureEdges(analysis, box.groundTruthHardEdges(), junctions);

    EXPECT_GT(benchmark.groundTruthEdges, 0);
    EXPECT_GE(benchmark.edgeRecall, 0.99);
    EXPECT_GE(benchmark.edgePrecision, 0.99);
    EXPECT_GE(benchmark.junctionRecall, 0.99);
}

// Junction precision companion to the recall test above. The edge evidence
// on the chamfer box is exact (64 detected = 64 ground truth), and loop
// recovery must not flood the junction set: circular recovery used to
// stitch 8-vertex "circles" through the corner vertices of *different*
// vertical crease lines (each row of 8 octagon corners is exactly
// concyclic) even though no feature edge connects them, marking all 56
// crease vertices as junctions (precision 16/56 ~= 0.29, measured 2026-07).
// Circular recovery is now gated on evidence connectivity (clusters must be
// linked by trace-graph edges up to a bounded angular gap) and junctions are
// graph branch points (valence > 2) rather than any vertex shared between
// overlapping recovered loops, so the junction set is exactly the 16
// octagon corners.
TEST(FeatureDetectionAnalytic, ChamferBoxJunctionPrecisionIsNotFloodedByCircularRecovery) {
    const analytic::ChamferBoxFixture box = analytic::makeChamferBox(2.0, 0.3, 6);
    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 30.0;
    options.minFeatureLoopVertices = 8;
    const FeatureAnalysis analysis = feature::detectFeatureCurves(box.mesh, options);

    std::vector<int> junctions;
    for (int corner = 0; corner < 8; ++corner) {
        junctions.push_back(corner);
        junctions.push_back(box.divisions * 8 + corner);
    }
    const feature::FeatureEdgeBenchmark benchmark =
        feature::benchmarkFeatureEdges(analysis, box.groundTruthHardEdges(), junctions);
    EXPECT_GE(benchmark.junctionPrecision, 0.99);
}

// Robustness of rim recovery under bounded deterministic noise. The side
// wall uses 20 rings so all edges are near-uniform (~0.1 to 0.14), making
// "0.1x mean edge length" a meaningful per-vertex displacement bound:
// amplitude = 0.014 = 1.4% of the radius per coordinate. Geometry of the
// relaxations relative to the noise-free test:
//  - face normals tilt by at most ~2*sqrt(3)*amplitude/edge ~= 0.34 rad
//    ~= 20 degrees, so noise dihedrals stay well below the 90-degree rims
//    and the 40-degree threshold still separates the two populations;
//  - a rim vertex moves off the true circle by at most sqrt(3)*amplitude
//    ~= 2.4% of the radius, so the circle-fit acceptance is relaxed from
//    0.05 to 0.08 and center/radius must match within 5% (vs 1e-7 clean).
TEST(FeatureDetectionAnalytic, NoisyCappedCylinderStillRecoversBothRimCircles) {
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(64, 20, 1.0, 2.0, true);
    const double amplitude = 0.1 * analytic::meanEdgeLength(cylinder.mesh);
    const Mesh noisy = analytic::withDeterministicNoise(cylinder.mesh, amplitude, 20260712u);

    const FeatureAnalysis analysis = feature::detectFeatureCurves(noisy, rimDetectionOptions(0.08));

    ASSERT_EQ(2, countCircular(analysis));
    for (const analytic::GroundTruthCircle& truth : cylinder.groundTruthCircles()) {
        SCOPED_TRACE(truth.center.z());
        const FeatureLoop* loop = circularLoopNearestTo(analysis, truth.center);
        ASSERT_NE(nullptr, loop);
        EXPECT_NEAR(truth.radius, loop->radius, 0.05 * truth.radius);
        EXPECT_LT((loop->center - truth.center).norm(), 0.05 * truth.radius);
        EXPECT_GT(std::abs(loop->normal.dot(truth.normal)), 0.95);
    }
}

// P1-3 regression: cross-vertex extremality comparisons must convert both
// operands into a common unit before comparing magnitudes. Each vertex fits
// its Monge patch in coordinates divided by its own radius r (average edge
// length x rings), so the fitted extremality is e_hat = r^2 * e_phys; where
// neighboring vertices have different r, raw e_hat values live in different
// units. On this graded sheet the crest column x = 0 borders columns whose
// fit radii differ by up to the density ratio 3, i.e. mixed-unit magnitude
// comparisons are distorted by up to ratio^2 = 9x -- far beyond the 2.5x
// design margin of the cyclideness gate -- and the Ohtake ownership rule
// (crossing belongs to the endpoint with smaller |e|) resolves against the
// crest vertex because its fine-side neighbor's e_hat is deflated by
// (r_fine / r_crest)^2. Expected behavior: the zero of e_phys lies exactly
// on the crest column by symmetry, so with unit-correct comparisons the
// crest column must claim the ridge at the same rate as on a uniform mesh
// of the fine density.
//
// Measured (2026-07, options {2,3,2,0.55}, threshold 0.008, crest rows 54):
//   pre-fix (raw e_hat compared across vertices):  crest recall  2/54
//   post-fix (neighbor e converted to center units): crest recall 53/54
//   uniform control (n=64, same spacing as the fine half): 63/63.
// The asserted bound requires the graded recall to stay within 0.10 of the
// uniform-density control instead of pinning exact counts.
TEST(FeatureDetectionAnalytic, GradedDensityGaussianRidgeCrestSurvivesDensityTransition) {
    const feature::SmoothCurvatureOptions options{2, 3, 2, 0.55};

    // Graded sheet: fine spacing 0.0208, coarse spacing 0.0625, crest at the
    // transition column. Sharp profile (sigma = 0.19 ~ 3 coarse cells) keeps
    // the e zero crossing within one cell of x = 0 on both halves.
    const analytic::GradedGaussianRidgeSheetFixture ridge =
        analytic::makeGradedGaussianRidgeSheet(48, 2.0, 0.50, 14.0, 3);
    const auto values = feature::computeSmoothCurvatureFeatures(ridge.mesh, options, 0.008);
    std::vector<int> crest;
    for (int row = 1; row < ridge.rows; ++row) {
        crest.push_back(ridge.vertexAt(row, ridge.crestColumn()));
    }
    const double gradedRecall = crestPersistentRecall(values, crest, 2, 0.008);

    // Uniform-density control with the same ridge profile and a spacing
    // matching the fine half (2 / 64 = 0.03125 vs 0.0208).
    const analytic::GaussianRidgeSheetFixture uniform = analytic::makeGaussianRidgeSheet(64, 2.0, 0.50, 14.0);
    const auto uniformValues = feature::computeSmoothCurvatureFeatures(uniform.mesh, options, 0.008);
    const double uniformRecall =
        crestPersistentRecall(uniformValues, uniform.interiorCrestVertices(), 2, 0.008);

    EXPECT_GT(uniformRecall, 0.90);
    EXPECT_GE(gradedRecall, uniformRecall - 0.10);
}

// P1-5 regression: the multiscale persistence must be a pure vote count
// (Luo-Zha M009), not gated on support at the coarsest scale. On a dense
// sheet a narrow ridge (sigma = 1/sqrt(2*400) = 0.035 ~ 1.1 edge lengths, so
// the full ridge support ~ 4 sigma ~ 4.5 edges) is smaller than the coarsest
// neighborhood radius baseRings + scaleCount - 1 = 6 rings: the coarsest fit
// averages the ridge away and cannot support the candidate, while the finer
// scales all agree. A coarsest-scale veto therefore zeroed the persistence
// of every crest vertex even though smoothCurvatureMinPersistentScales = 2
// was satisfied several times over.
//
// Measured (2026-07, options {2,5,2,0.55}, threshold 0.008, crest rows 63):
//   pre-fix (coarsest-scale veto):  persistent crest vertices  0/63
//   post-fix (pure vote counting):  persistent crest vertices 63/63.
TEST(FeatureDetectionAnalytic, NarrowRidgeOnDenseSheetSurvivesCoarsestScale) {
    const analytic::GaussianRidgeSheetFixture ridge = analytic::makeGaussianRidgeSheet(64, 2.0, 0.05, 400.0);
    const feature::SmoothCurvatureOptions options{2, 5, 2, 0.55};
    const auto values = feature::computeSmoothCurvatureFeatures(ridge.mesh, options, 0.008);

    const double recall = crestPersistentRecall(values, ridge.interiorCrestVertices(), 2, 0.008);
    EXPECT_GT(recall, 0.90);
}

// Every threshold in the pipeline is dimensionless (normalized by local
// scale), so a uniform rescaling of the mesh must reproduce the *same set*
// of feature edges, not merely the same count. 1e-3/1e3 exercise six orders
// of magnitude in each direction. minFeatureLoopVertices = 12 keeps the
// octagonal rims out of the (scale-free but expensive) circular-recovery
// path so the comparison focuses on evidence and cleanup decisions.
TEST(FeatureDetectionAnalytic, FeatureEdgeSetIsExactlyScaleInvariant) {
    const analytic::ChamferBoxFixture box = analytic::makeChamferBox(2.0, 0.3, 6);
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(48, 6, 1.0, 2.0, true);

    FeatureOptions options;
    options.featureAngleDeg = 30.0;
    options.minFeatureLoopVertices = 12;
    options.useNormalTensorFeatures = true;
    options.useSmoothCurvatureFeatures = true;

    const Mesh* meshes[] = {&box.mesh, &cylinder.mesh};
    const char* names[] = {"chamfer-box", "capped-cylinder"};
    for (int index = 0; index < 2; ++index) {
        SCOPED_TRACE(names[index]);
        const std::vector<std::pair<int, int>> reference =
            activeGraphEdges(feature::detectFeatureCurves(*meshes[index], options));
        EXPECT_FALSE(reference.empty());
        for (double factor : {1e-3, 1e3}) {
            SCOPED_TRACE(factor);
            const std::vector<std::pair<int, int>> scaled = activeGraphEdges(
                feature::detectFeatureCurves(analytic::uniformlyScaled(*meshes[index], factor), options)
            );
            EXPECT_EQ(reference, scaled);
        }
    }
}

} // namespace manumesh::test::feature_detection
