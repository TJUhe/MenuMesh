#include "../../../src/common/detail/GeometryPredicates.h"

#include "core/Mesh.h"

#include <array>
#include <cmath>
#include <gtest/gtest.h>

TEST(ManuMesh, GeometryPredicatesMeasureTriangleQualityAndDistance) {
    const manumesh::Vec3 a(0.0, 0.0, 0.0);
    const manumesh::Vec3 b(1.0, 0.0, 0.0);
    const manumesh::Vec3 c(0.5, std::sqrt(3.0) / 2.0, 0.0);

    EXPECT_NEAR(1.0, manumesh::common::triangleQuality(a, b, c), 1e-12);
    EXPECT_EQ(0.0, manumesh::common::triangleQuality(a, a, b));

    const double aboveDistance =
        manumesh::common::pointTriangleDistanceSquared(manumesh::Vec3(0.5, 0.25, 2.0), a, b, c);
    EXPECT_NEAR(4.0, aboveDistance, 1e-12);

    const double outsideDistance =
        manumesh::common::pointTriangleDistanceSquared(manumesh::Vec3(-1.0, 0.0, 0.0), a, b, c);
    EXPECT_NEAR(1.0, outsideDistance, 1e-12);
}

TEST(ManuMesh, GeometryPredicatesMeasureAabbDistanceAndTriangleBounds) {
    const manumesh::Vec3 lo(0.0, 0.0, 0.0);
    const manumesh::Vec3 hi(1.0, 2.0, 3.0);

    EXPECT_EQ(0.0, manumesh::common::pointAabbDistanceSquared(manumesh::Vec3(0.5, 1.0, 2.0), lo, hi));
    EXPECT_NEAR(5.0, manumesh::common::pointAabbDistanceSquared(manumesh::Vec3(2.0, -2.0, 0.0), lo, hi), 1e-12);

    const std::array<manumesh::Vec3, 3> tri = {
        manumesh::Vec3(0.0, 1.0, 2.0),
        manumesh::Vec3(2.0, -1.0, 3.0),
        manumesh::Vec3(1.0, 0.0, -2.0),
    };
    const auto [triLo, triHi] = manumesh::common::triangleAabb(tri, 0.25);
    EXPECT_TRUE(triLo.isApprox(manumesh::Vec3(-0.25, -1.25, -2.25)));
    EXPECT_TRUE(triHi.isApprox(manumesh::Vec3(2.25, 1.25, 3.25)));
}

TEST(ManuMesh, GeometryPredicatesDetectTriangleIntersection) {
    const std::array<manumesh::Vec3, 3> base = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    const std::array<manumesh::Vec3, 3> coplanarInside = {
        manumesh::Vec3(0.25, 0.25, 0.0),
        manumesh::Vec3(0.5, 0.25, 0.0),
        manumesh::Vec3(0.25, 0.5, 0.0),
    };
    const std::array<manumesh::Vec3, 3> separated = {
        manumesh::Vec3(2.0, 2.0, 0.0),
        manumesh::Vec3(3.0, 2.0, 0.0),
        manumesh::Vec3(2.0, 3.0, 0.0),
    };
    const std::array<manumesh::Vec3, 3> piercing = {
        manumesh::Vec3(0.25, 0.25, -1.0),
        manumesh::Vec3(0.25, 0.25, 1.0),
        manumesh::Vec3(0.75, 0.25, 0.0),
    };

    EXPECT_TRUE(manumesh::common::trianglesIntersect(base, coplanarInside, 1e-12));
    EXPECT_FALSE(manumesh::common::trianglesIntersect(base, separated, 1e-12));
    EXPECT_TRUE(manumesh::common::trianglesIntersect(base, piercing, 1e-12));
}

namespace {

std::array<manumesh::Vec3, 3> scaleTriangle(const std::array<manumesh::Vec3, 3>& tri, double scale) {
    return {scale * tri[0], scale * tri[1], scale * tri[2]};
}

} // namespace

// The intersection predicate takes a RELATIVE tolerance and normalizes by the
// local geometric scale, so the same intersecting / non-intersecting
// configuration must produce identical decisions at 1e-3 and 1e+3 scale.
TEST(ManuMesh, GeometryPredicatesTriangleIntersectionIsScaleInvariant) {
    const std::array<manumesh::Vec3, 3> base = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    const std::array<manumesh::Vec3, 3> coplanarInside = {
        manumesh::Vec3(0.25, 0.25, 0.0),
        manumesh::Vec3(0.5, 0.25, 0.0),
        manumesh::Vec3(0.25, 0.5, 0.0),
    };
    const std::array<manumesh::Vec3, 3> separated = {
        manumesh::Vec3(2.0, 2.0, 0.0),
        manumesh::Vec3(3.0, 2.0, 0.0),
        manumesh::Vec3(2.0, 3.0, 0.0),
    };
    const std::array<manumesh::Vec3, 3> piercing = {
        manumesh::Vec3(0.25, 0.25, -1.0),
        manumesh::Vec3(0.25, 0.25, 1.0),
        manumesh::Vec3(0.75, 0.25, 0.0),
    };
    // Near-touching but strictly separated parallel triangle: a gap of 1e-6
    // of the local scale stays separated at every scale.
    const std::array<manumesh::Vec3, 3> hovering = {
        manumesh::Vec3(0.1, 0.1, 1e-6),
        manumesh::Vec3(0.9, 0.1, 1e-6),
        manumesh::Vec3(0.1, 0.9, 1e-6),
    };

    constexpr double kRelativeEps = 1e-9;
    for (const double scale : {1e-3, 1.0, 1e+3}) {
        SCOPED_TRACE(testing::Message() << "scale=" << scale);
        const auto scaledBase = scaleTriangle(base, scale);
        EXPECT_TRUE(
            manumesh::common::trianglesIntersect(scaledBase, scaleTriangle(coplanarInside, scale), kRelativeEps)
        );
        EXPECT_FALSE(manumesh::common::trianglesIntersect(scaledBase, scaleTriangle(separated, scale), kRelativeEps));
        EXPECT_TRUE(manumesh::common::trianglesIntersect(scaledBase, scaleTriangle(piercing, scale), kRelativeEps));
        EXPECT_FALSE(manumesh::common::trianglesIntersect(scaledBase, scaleTriangle(hovering, scale), kRelativeEps));
    }
}

// Collinear-overlap and inside-point decisions in the coplanar path mix
// length- and area-dimensioned comparisons; both must scale consistently.
TEST(ManuMesh, GeometryPredicatesCoplanarOverlapIsScaleInvariant) {
    const std::array<manumesh::Vec3, 3> base = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(2.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 2.0, 0.0),
    };
    // Coplanar triangle overlapping the base across an edge.
    const std::array<manumesh::Vec3, 3> crossing = {
        manumesh::Vec3(1.0, -0.5, 0.0),
        manumesh::Vec3(1.0, 0.5, 0.0),
        manumesh::Vec3(2.5, 0.5, 0.0),
    };
    // Coplanar triangle fully outside the base: no overlap.
    const std::array<manumesh::Vec3, 3> disjointCoplanar = {
        manumesh::Vec3(3.0, 0.0, 0.0),
        manumesh::Vec3(4.0, 0.0, 0.0),
        manumesh::Vec3(3.0, 1.0, 0.0),
    };

    constexpr double kRelativeEps = 1e-9;
    for (const double scale : {1e-3, 1.0, 1e+3}) {
        SCOPED_TRACE(testing::Message() << "scale=" << scale);
        const auto scaledBase = scaleTriangle(base, scale);
        EXPECT_TRUE(manumesh::common::trianglesIntersect(scaledBase, scaleTriangle(crossing, scale), kRelativeEps));
        EXPECT_FALSE(
            manumesh::common::trianglesIntersect(scaledBase, scaleTriangle(disjointCoplanar, scale), kRelativeEps)
        );
    }
}

TEST(ManuMesh, GeometryPredicatesAllowOnlyDeclaredSharedTopologyContact) {
    const std::array<int, 3> baseIds{{0, 1, 2}};
    const std::array<manumesh::Vec3, 3> base = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    constexpr double kRelativeEps = 1e-9;

    const std::array<int, 3> vertexTouchIds{{0, 3, 4}};
    const std::array<manumesh::Vec3, 3> vertexTouch = {
        base[0],
        manumesh::Vec3(-1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, -1.0, 0.0),
    };
    EXPECT_FALSE(
        manumesh::common::trianglesIntersectBeyondSharedTopology(
            baseIds, base, vertexTouchIds, vertexTouch, kRelativeEps
        )
    );

    const std::array<manumesh::Vec3, 3> vertexOverlap = {
        base[0],
        manumesh::Vec3(0.8, 0.1, 0.0),
        manumesh::Vec3(0.1, 0.8, 0.0),
    };
    EXPECT_TRUE(
        manumesh::common::trianglesIntersectBeyondSharedTopology(
            baseIds, base, vertexTouchIds, vertexOverlap, kRelativeEps
        )
    );

    const std::array<int, 3> edgeNeighborIds{{0, 1, 3}};
    const std::array<manumesh::Vec3, 3> edgeNeighbor = {
        base[0],
        base[1],
        manumesh::Vec3(0.0, -1.0, 0.0),
    };
    EXPECT_FALSE(
        manumesh::common::trianglesIntersectBeyondSharedTopology(
            baseIds, base, edgeNeighborIds, edgeNeighbor, kRelativeEps
        )
    );

    const std::array<manumesh::Vec3, 3> edgeOverlap = {
        base[0],
        base[1],
        manumesh::Vec3(0.25, 0.5, 0.0),
    };
    EXPECT_TRUE(
        manumesh::common::trianglesIntersectBeyondSharedTopology(
            baseIds, base, edgeNeighborIds, edgeOverlap, kRelativeEps
        )
    );

    const std::array<manumesh::Vec3, 3> foldedNeighbor = {
        base[0],
        base[1],
        manumesh::Vec3(0.25, 0.0, 0.75),
    };
    EXPECT_FALSE(
        manumesh::common::trianglesIntersectBeyondSharedTopology(
            baseIds, base, edgeNeighborIds, foldedNeighbor, kRelativeEps
        )
    );
}
