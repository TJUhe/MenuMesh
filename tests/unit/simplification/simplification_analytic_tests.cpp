// Simplification against analytic fixtures with closed-form ground truth.
//
// Assertion bounds are derived from the geometry of QEM edge collapse:
//  - On a uniformly sampled convex surface the quadric optimum stays within
//    the local chord sagitta of the surface, which gives an absolute
//    distance budget from the target edge length after simplification.
//  - Hard primitive-curve protection projects placements onto the fitted
//    circle, and on an exact rim polygon that fitted circle *is* the
//    analytic circle up to round-off, so surviving rim vertices must sit on
//    the true circle to ~1e-6 relative, not merely "nearby".
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

/// Wraps a difference of periodic u coordinates into [0, 0.5].
double circularUvDistance(double a, double b) {
    const double difference = std::abs(a - b);
    const double wrapped = difference - std::floor(difference);
    return std::min(wrapped, 1.0 - wrapped);
}

/// Assigns the exact cylinder parametrization as per-corner UVs:
/// u = theta / (2*pi) circumferential, v = z/height + 1/2 axial. Faces that
/// cross the theta = 0 seam use u = 1 on the wrapped column, which creates a
/// legitimate two-sided UV seam chart exactly like an unwrapped texture atlas.
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

} // namespace

// A sphere is the extreme uniformly convex case: every plane quadric around
// a vertex is tangent to the sphere, so the QEM optimum of any collapse lies
// inside the convex hull of nearby tangent-plane intersections, within the
// local chord sagitta of the surface. After simplifying 9024 faces to ratio
// 0.2 the output edge length is about l = sqrt(8*A/(sqrt(3)*F)) ~= 0.09*r,
// so the per-collapse sagitta is l^2/(8r) ~= 1.1e-3*r; the 1%-of-radius
// budget leaves ~9x margin for cascaded collapses without ever tolerating a
// visibly off-sphere vertex.
TEST(SimplificationAnalytic, SphereVerticesStayOnSphereAndTopologyIsPreserved) {
    const analytic::SphereFixture sphere = analytic::makeUvSphere(48, 96, 1.0);
    const SimplifiedMesh result = simplifyWithReport(sphere.mesh, manumesh::test::standardOptions(0.2));
    manumesh::test::expectBudget(result, sphere.mesh, 0.2);

    double maxRadialDeviation = 0.0;
    for (const Vec3& vertex : result.mesh.vertices) {
        maxRadialDeviation = std::max(maxRadialDeviation, std::abs(vertex.norm() - sphere.radius));
    }
    EXPECT_LT(maxRadialDeviation, 0.01 * sphere.radius);

    // Closed 2-manifold of genus 0 before and after: no boundary or
    // non-manifold edges may appear and V - E + F = 2 must be preserved.
    const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(result.mesh);
    EXPECT_EQ(0, stats.boundaryEdges);
    EXPECT_EQ(0, stats.nonManifoldEdges);
    EXPECT_EQ(2, stats.vertices - stats.edges + stats.faces);
}

// With FeatureProtectionMode::PrimitiveCurves the two rim circles of the
// capped cylinder are hard-protected: any collapse placement touching a rim
// vertex is projected onto the circle fitted at detection time. The input
// rim polygon lies exactly on the analytic circle, so that fitted circle
// equals the true circle up to double round-off, and every surviving rim
// vertex must satisfy |dist_to_axis - r| <= 1e-6*r and |z -+ h/2| <= 1e-6*r
// (round-off tolerance, not a geometric allowance).
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

    // Re-detection on the simplified output must still see two circular rim
    // loops with the analytic radius.
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

// Texture-preserving simplification on a cylinder whose UVs are the exact
// developable parametrization (u = theta/2pi, v = z/h + 1/2). Ground truth:
//  - no surviving UV triangle may flip (all signed UV areas keep the input
//    orientation), and
//  - each surviving corner's UV may deviate from the analytic
//    parametrization of its own 3D position by at most the parameter
//    increment of one local mesh step: an edge of 3D length L spans
//    delta_u = L/(2*pi*r) and delta_v = L/h on the cylinder, so a corner
//    whose UV drifts further than its face's longest edge in parameter
//    space has slipped by more than one texel neighborhood, which the
//    seam-aware interpolation must never do.
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
            // The seam chart stores u = theta/2pi with a different branch
            // (atan2 above uses [-pi, pi], the fixture uses [0, 2pi)), so u
            // is compared as a periodic coordinate.
            EXPECT_LT(circularUvDistance(texcoords.uv[corner].x() + 0.5, analyticU), uTolerance)
                << "face " << face << " corner " << corner;
            EXPECT_NEAR(analyticV, texcoords.uv[corner].y(), vTolerance) << "face " << face << " corner " << corner;
        }
    }
    EXPECT_GT(validFaces, 0);
}

// Torus Hausdorff budget. At ratio 0.35 the simplified mesh of a (R=1,
// r=0.3) torus with 9216 input faces keeps about 3225 faces, i.e. an output
// edge length of l = sqrt(8*A/(sqrt(3)*F)) ~= 0.077 (A = 4*pi^2*R*r ~= 11.8).
// The tightest bending direction is the minor circle, so the chord sagitta
// bound is l^2/(8*r_minor) ~= 2.5e-3, and the 2%-of-minor-radius budget
// (6e-3) leaves ~2.4x margin. Sampled bidirectional distance is a lower
// bound of the true Hausdorff distance on the samples, which is exactly the
// quantity the budget constrains.
TEST(SimplificationAnalytic, TorusStaysWithinSampledHausdorffBudget) {
    const analytic::TorusFixture torus = analytic::makeTorus(96, 48, 1.0, 0.3);
    const SimplifiedMesh result = simplifyWithReport(torus.mesh, manumesh::test::standardOptions(0.35));
    manumesh::test::expectBudget(result, torus.mesh, 0.35);

    const manumesh::analysis::DistanceStats distance =
        manumesh::analysis::compareMeshesBySampledDistance(torus.mesh, result.mesh, 4096);
    const double budget = 0.02 * torus.minorRadius;
    EXPECT_LT(distance.maxOriginalToSimplified, budget);
    EXPECT_LT(distance.maxSimplifiedToOriginal, budget);

    // Genus-1 closed surface: Euler characteristic 0, no boundary, and no
    // non-manifold edges may be created.
    const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(result.mesh);
    EXPECT_EQ(0, stats.boundaryEdges);
    EXPECT_EQ(0, stats.nonManifoldEdges);
    EXPECT_EQ(0, stats.vertices - stats.edges + stats.faces);
}

// End-to-end determinism: detection plus feature-protected simplification on
// the same fixture must be byte-identical across repeated runs (unordered
// containers, float reductions, and priority-queue tiebreaks are all
// required to be deterministic by design). Three runs catch both run-to-run
// and first-vs-warm discrepancies.
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
            // Bitwise equality: determinism means the same doubles, not
            // merely close ones.
            EXPECT_EQ(reference.vertices[vertex].x(), output.vertices[vertex].x());
            EXPECT_EQ(reference.vertices[vertex].y(), output.vertices[vertex].y());
            EXPECT_EQ(reference.vertices[vertex].z(), output.vertices[vertex].z());
        }
        for (std::size_t face = 0; face < reference.faces.size(); ++face) {
            EXPECT_EQ(reference.faces[face].v, output.faces[face].v);
        }
    }
}
