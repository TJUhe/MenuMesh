/**
 * @file tests/support/AnalyticFixtures.h
 * @brief Verifies analytic fixtures behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

#pragma once

#include "core/Mesh.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace manumesh::test::analytic {

/// Deterministic triangle-mesh fixtures with closed-form ground truth.
///
/// Every generator is pure and reproducible (no random device, no clock).
/// Sign convention for the analytic curvatures: a normal section that bends
/// away from the outward surface normal has positive curvature, so a sphere
/// of radius r reports kappaMax = kappaMin = 1/r, the side wall of a cylinder
/// reports kappaMax = 1/r (circumferential) and kappaMin = 0 (axial), and a
/// torus reports kappaMax/kappaMin from {1/r, cos(theta)/(R + r cos(theta))}.

/// Analytic principal curvatures and principal frame at one vertex.
struct PrincipalCurvatures {
    double kappaMax = 0.0;
    double kappaMin = 0.0;
    Vec3 directionMax = Vec3(1.0, 0.0, 0.0);
    Vec3 directionMin = Vec3(0.0, 1.0, 0.0);
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
};

/// One analytic ground-truth feature circle (e.g. a cylinder rim).
struct GroundTruthCircle {
    Vec3 center = Vec3::Zero();
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    double radius = 0.0;
};

/// Longitude/latitude sphere. Vertex 0 is the north pole, vertex
/// vertexCount-1 the south pole; latitude row k (1..rings-1) stores
/// `segments` vertices contiguously.
struct SphereFixture {
    Mesh mesh;
    double radius = 1.0;
    int rings = 0;
    int segments = 0;

    bool isPole(int vertex) const;
    /// Latitude row index in [0, rings]; poles map to 0 and rings.
    int ringIndex(int vertex) const;
    /// True for vertices at least `margin` latitude rows away from each pole.
    bool isInterior(int vertex, int margin) const;
    PrincipalCurvatures analyticPrincipalCurvatures(int vertex) const;
};
SphereFixture makeUvSphere(int rings, int segments, double radius);

/// Right circular cylinder around the z axis, z in [-height/2, height/2].
/// Side vertices are stored row-major from bottom (row 0) to top (row
/// `rings`), `segments` vertices per row; when capped, the two cap centers
/// follow (bottom first). The capped version has exact 90-degree creases
/// along the two rim circles.
struct CylinderFixture {
    Mesh mesh;
    double radius = 1.0;
    double height = 1.0;
    int segments = 0;
    int rings = 0;
    bool capped = false;

    bool isRimVertex(int vertex) const;
    /// Side-wall vertices strictly between the two rims.
    bool isSideInterior(int vertex) const;
    /// Valid for side-wall vertices (including rims).
    PrincipalCurvatures analyticPrincipalCurvatures(int vertex) const;
    /// The two rim circles (bottom then top). Meaningful ground-truth feature
    /// curves only for the capped variant.
    std::vector<GroundTruthCircle> groundTruthCircles() const;
    /// Rim edges as sorted vertex-index pairs (capped variant's hard edges).
    std::vector<std::pair<int, int>> groundTruthFeatureEdges() const;
};
CylinderFixture makeCylinder(int segments, int rings, double radius, double height, bool capped);

/// Torus around the z axis: P(u, v) = ((R + r cos v) cos u,
/// (R + r cos v) sin u, r sin v). Vertices are stored as majorSegments rows
/// of minorSegments vertices (major index i, minor index j -> i * minor + j).
struct TorusFixture {
    Mesh mesh;
    double majorRadius = 1.0;
    double minorRadius = 0.25;
    int majorSegments = 0;
    int minorSegments = 0;

    /// Minor angle v in [0, 2*pi) of one vertex.
    double minorAngle(int vertex) const;
    /// True on the inner half of the tube (cos v < 0).
    bool isInnerSide(int vertex) const;
    PrincipalCurvatures analyticPrincipalCurvatures(int vertex) const;
};
TorusFixture makeTorus(int majorSegments, int minorSegments, double majorRadius, double minorRadius);

/// Axis-aligned box of edge length `size` whose four vertical edges are
/// 45-degree chamfered with leg length `chamfer` (an octagonal prism with
/// flat octagon caps). Each of the four original vertical box edges is
/// replaced by a chamfer facet bounded by two parallel vertical crease
/// lines, so the ground-truth hard-edge set is the 8 vertical crease lines
/// (dihedral 45 degrees) plus the two octagonal rims (dihedral 90 degrees).
struct ChamferBoxFixture {
    Mesh mesh;
    double size = 1.0;
    double chamfer = 0.1;
    int divisions = 1;

    /// Every ground-truth hard edge as a sorted vertex-index pair.
    std::vector<std::pair<int, int>> groundTruthHardEdges() const;
};
ChamferBoxFixture makeChamferBox(double size, double chamfer, int divisions);

/// Open sheet z = height * exp(-sharpness * x^2) over [-size/2, size/2]^2
/// with a single smooth ridge along the y axis. The profile curvature at the
/// crest is f''(0) = -2 * height * sharpness, so the ridge-crossing principal
/// curvature magnitude is analyticCrestCurvature() and the ridge tangent is
/// the y axis.
struct GaussianRidgeSheetFixture {
    Mesh mesh;
    int subdivisions = 0;
    double size = 2.0;
    double height = 0.25;
    double sharpness = 8.0;

    /// Vertices on the crest line x == 0, excluding the two boundary rows.
    std::vector<int> interiorCrestVertices() const;
    /// |f''(0)| = 2 * height * sharpness (f'(0) = 0, so this is the exact
    /// principal curvature magnitude across the ridge at the crest).
    double analyticCrestCurvature() const;
    Vec3 crestTangent() const { return Vec3(0.0, 1.0, 0.0); }
};
GaussianRidgeSheetFixture makeGaussianRidgeSheet(int subdivisions, double size, double height, double sharpness);

/// Gaussian ridge sheet with the same analytic profile as
/// GaussianRidgeSheetFixture (z = h * exp(-s * x^2), crest line along the y
/// axis at x = 0) but graded column density: the columns at x < 0 are
/// `densityRatio` times denser than the columns at x > 0, with one column
/// exactly on the crest x = 0. The crest therefore lies in the
/// density-transition band: the per-vertex fit radius (average incident edge
/// length) differs by up to `densityRatio` between the two endpoints of
/// every ridge-crossing edge, which is exactly the configuration where
/// cross-vertex extremality comparisons must convert units.
struct GradedGaussianRidgeSheetFixture {
    Mesh mesh;
    int fineColumns = 0;   ///< Column intervals covering x in [-size/2, 0].
    int coarseColumns = 0; ///< Column intervals covering x in [0, size/2].
    int rows = 0;          ///< Row intervals covering y in [-size/2, size/2].
    double size = 2.0;
    double height = 0.25;
    double sharpness = 8.0;

    double fineSpacing() const { return 0.5 * size / fineColumns; }
    double coarseSpacing() const { return 0.5 * size / coarseColumns; }
    /// Vertex index at (row, col), matching the row-major layout.
    int vertexAt(int row, int col) const { return row * (fineColumns + coarseColumns + 1) + col; }
    /// Column index of the crest column x = 0.
    int crestColumn() const { return fineColumns; }
    /// |f''(0)| = 2 * height * sharpness.
    double analyticCrestCurvature() const { return 2.0 * height * sharpness; }
    Vec3 crestTangent() const { return Vec3(0.0, 1.0, 0.0); }
};
GradedGaussianRidgeSheetFixture
makeGradedGaussianRidgeSheet(int fineColumns, double size, double height, double sharpness, int densityRatio);

/// Returns a copy of the mesh with every vertex displaced by a reproducible
/// pseudo-random offset in [-amplitude, amplitude]^3 from a fixed-seed
/// 64-bit LCG (no std::random_device, no time). The same (mesh, amplitude,
/// seed) triple always yields bit-identical output.
Mesh withDeterministicNoise(const Mesh& mesh, double amplitude, std::uint64_t seed);

/// Returns a copy of the mesh with every vertex multiplied by `factor`.
Mesh uniformlyScaled(const Mesh& mesh, double factor);

/// Mean length over the unique undirected edges of the mesh.
double meanEdgeLength(const Mesh& mesh);

} // namespace manumesh::test::analytic
