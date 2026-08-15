/**
 * @file tests/unit/common/spatial_index_tests.cpp
 * @brief 验证 ManuMesh 测试中的空间索引测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#include "../../../src/common/detail/SpatialIndex.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <vector>
namespace {

bool containsItem(const std::vector<int>& items, int itemId) {
    return std::find(items.begin(), items.end(), itemId) != items.end();
}

} // 命名空间

TEST(ManuMesh, UniformAabbCandidateGridQueriesOverlappingItems) {
    manumesh::common::UniformAabbCandidateGrid grid;
    grid.reset(manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(10.0, 10.0, 10.0), 1000);
    ASSERT_TRUE(grid.enabled());

    grid.insert(1, manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(1.0, 1.0, 1.0));
    grid.insert(2, manumesh::Vec3(5.0, 5.0, 5.0), manumesh::Vec3(6.0, 6.0, 6.0));

    const std::vector<int> nearOrigin =
        grid.queryCandidates(manumesh::Vec3(0.25, 0.25, 0.25), manumesh::Vec3(1.5, 1.5, 1.5));
    EXPECT_TRUE(containsItem(nearOrigin, 1));
    EXPECT_FALSE(containsItem(nearOrigin, 2));

    const std::vector<int> distant = grid.queryCandidates(manumesh::Vec3(4.5, 4.5, 4.5), manumesh::Vec3(6.5, 6.5, 6.5));
    EXPECT_FALSE(containsItem(distant, 1));
    EXPECT_TRUE(containsItem(distant, 2));
}

TEST(ManuMesh, UniformAabbCandidateGridRemovesAndUpdatesItems) {
    manumesh::common::UniformAabbCandidateGrid grid;
    grid.reset(manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(10.0, 10.0, 10.0), 1000);
    ASSERT_TRUE(grid.enabled());

    grid.insert(1, manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(1.0, 1.0, 1.0));
    grid.insert(2, manumesh::Vec3(5.0, 5.0, 5.0), manumesh::Vec3(6.0, 6.0, 6.0));
    grid.remove(1);
    grid.update(2, manumesh::Vec3(0.25, 0.25, 0.25), manumesh::Vec3(1.25, 1.25, 1.25));

    const std::vector<int> nearOrigin =
        grid.queryCandidates(manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(1.5, 1.5, 1.5));
    EXPECT_FALSE(containsItem(nearOrigin, 1));
    EXPECT_TRUE(containsItem(nearOrigin, 2));

    const std::vector<int> oldLocation =
        grid.queryCandidates(manumesh::Vec3(5.0, 5.0, 5.0), manumesh::Vec3(6.0, 6.0, 6.0));
    EXPECT_FALSE(containsItem(oldLocation, 2));
}

TEST(ManuMesh, UniformAabbCandidateGridKeepsOversizedItemsAsConservativeCandidates) {
    manumesh::common::UniformAabbCandidateGrid grid;
    grid.reset(manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(10.0, 10.0, 10.0), 1000);
    ASSERT_TRUE(grid.enabled());

    grid.insert(7, manumesh::Vec3(-1000.0, -1000.0, -1000.0), manumesh::Vec3(1000.0, 1000.0, 1000.0));
    grid.insert(8, manumesh::Vec3(9.0, 9.0, 9.0), manumesh::Vec3(9.5, 9.5, 9.5));

    const std::vector<int> nearOrigin =
        grid.queryCandidates(manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(0.25, 0.25, 0.25));
    EXPECT_TRUE(containsItem(nearOrigin, 7));
    EXPECT_FALSE(containsItem(nearOrigin, 8));
}

TEST(ManuMesh, UniformAabbCandidateGridIgnoresOperationsWhenDisabled) {
    manumesh::common::UniformAabbCandidateGrid grid;
    grid.reset(manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(1.0, 1.0, 1.0), 0);
    EXPECT_FALSE(grid.enabled());

    grid.insert(1, manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(1.0, 1.0, 1.0));
    EXPECT_TRUE(grid.queryCandidates(manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(1.0, 1.0, 1.0)).empty());
}
