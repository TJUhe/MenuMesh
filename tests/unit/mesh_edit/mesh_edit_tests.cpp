/**
 * @file tests/unit/mesh_edit/mesh_edit_tests.cpp
 * @brief 验证动态拓扑缓存和活动网格压缩后的稳定重映射。
 * @ingroup manumesh_tests
 */

#include "mesh_edit/detail/DynamicTopology.h"
#include "mesh_edit/detail/MeshCompaction.h"

#include <gtest/gtest.h>
namespace {

namespace mesh_edit = manumesh::mesh_edit;

TEST(MeshEdit, CompactsActiveFacesAndReturnsStableRemaps) {
    const std::vector<manumesh::Vec3> positions = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {1.0, 1.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.5, 1.5, 0.0},
    };
    const std::vector<char> activeVertices = {1, 1, 0, 1, 1};
    const std::vector<mesh_edit::EditableFace> faces = {
        {{{0, 1, 3}}, true},
        {{{1, 2, 3}}, true},
        {{{0, 3, 4}}, false},
        {{{1, 3, 4}}, true},
        {{{1, 3, 9}}, true},
        {{{1, 1, 4}}, true},
    };

    const mesh_edit::MeshCompactionResult result = mesh_edit::compactActiveMesh(positions, activeVertices, faces);

    ASSERT_EQ(4u, result.mesh.vertices.size());
    ASSERT_EQ(2u, result.mesh.faces.size());
    EXPECT_EQ((std::vector<int>{0, 1, -1, 2, 3}), result.oldToNewVertices);
    EXPECT_EQ((std::vector<int>{0, -1, -1, 1, -1, -1}), result.oldToNewFaces);
    EXPECT_EQ((std::array<int, 3>{0, 1, 2}), result.mesh.faces[0].v);
    EXPECT_EQ((std::array<int, 3>{1, 2, 3}), result.mesh.faces[1].v);
}

TEST(MeshEdit, DynamicTopologyTracksAdjacencyAndDuplicateFaces) {
    std::vector<mesh_edit::EditableFace> faces = {
        {{{0, 1, 2}}, true},
        {{{0, 2, 3}}, true},
        {{{0, 1, 2}}, false},
    };
    mesh_edit::DynamicTopology topology(faces, 4);

    EXPECT_TRUE(mesh_edit::areAdjacent(0, 2, faces, topology));
    EXPECT_FALSE(mesh_edit::areAdjacent(1, 3, faces, topology));
    EXPECT_EQ(2, mesh_edit::activeIncidentFaceCountForEdge(0, 2, faces, topology));
    EXPECT_EQ(5u, mesh_edit::collectActiveEdges(faces).size());

    faces[2].active = true;
    topology.addFace(2, faces[2]);
    EXPECT_TRUE(topology.hasDuplicateFace(2, faces[2]));
    topology.removeFace(0, faces[0]);
    faces[0].active = false;
    EXPECT_FALSE(topology.hasDuplicateFace(2, faces[2]));
}

} // 命名空间
