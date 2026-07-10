#include "../../../src/common/detail/GeometryPredicates.h"

#include "core/Mesh.h"

#include <array>
#include <cmath>
#include <gtest/gtest.h>

TEST(ManuMesh, GeometryPredicatesMeasureTriangleQualityAndDistance) {
    const manumesh::Vec3 a(0.0, 0.0, 0.0);
    const manumesh::Vec3 b(1.0, 0.0, 0.0);
    const manumesh::Vec3 c(0.5, std::sqrt(3.0) / 2.0, 0.0);

    EXPECT_NEAR(1.0, manumesh::detail::triangleQuality(a, b, c), 1e-12);
    EXPECT_EQ(0.0, manumesh::detail::triangleQuality(a, a, b));

    const double aboveDistance =
        manumesh::detail::pointTriangleDistanceSquared(manumesh::Vec3(0.5, 0.25, 2.0), a, b, c);
    EXPECT_NEAR(4.0, aboveDistance, 1e-12);

    const double outsideDistance =
        manumesh::detail::pointTriangleDistanceSquared(manumesh::Vec3(-1.0, 0.0, 0.0), a, b, c);
    EXPECT_NEAR(1.0, outsideDistance, 1e-12);
}

TEST(ManuMesh, GeometryPredicatesMeasureAabbDistanceAndTriangleBounds) {
    const manumesh::Vec3 lo(0.0, 0.0, 0.0);
    const manumesh::Vec3 hi(1.0, 2.0, 3.0);

    EXPECT_EQ(0.0, manumesh::detail::pointAabbDistanceSquared(manumesh::Vec3(0.5, 1.0, 2.0), lo, hi));
    EXPECT_NEAR(5.0, manumesh::detail::pointAabbDistanceSquared(manumesh::Vec3(2.0, -2.0, 0.0), lo, hi), 1e-12);

    const std::array<manumesh::Vec3, 3> tri = {
        manumesh::Vec3(0.0, 1.0, 2.0),
        manumesh::Vec3(2.0, -1.0, 3.0),
        manumesh::Vec3(1.0, 0.0, -2.0),
    };
    const auto [triLo, triHi] = manumesh::detail::triangleAabb(tri, 0.25);
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

    EXPECT_TRUE(manumesh::detail::trianglesIntersect(base, coplanarInside, 1e-12));
    EXPECT_FALSE(manumesh::detail::trianglesIntersect(base, separated, 1e-12));
    EXPECT_TRUE(manumesh::detail::trianglesIntersect(base, piercing, 1e-12));
}
