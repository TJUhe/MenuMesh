/**
 * @file tests/unit/api/c_api_feature_edges_tests.cpp
 * @brief Verifies standalone feature-edge detection through the C ABI.
 */

#include "CApiTestSupport.h"

#include <cstddef>
#include <limits>
#include <vector>

namespace {

ManuMeshMeshHandle* makeSingleTriangle(ManuMeshContext* context) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    if (!mesh) {
        return nullptr;
    }
    const ManuMeshVec3 vertices[] = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
    };
    const ManuMeshFace faces[] = {{{0, 1, 2}}};
    if (manumesh_mesh_set_data(context, mesh, vertices, 3, faces, 1) != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(mesh);
        return nullptr;
    }
    return mesh;
}

} // namespace

TEST_F(CApiTest, InitializesStandaloneFeatureOptions) {
    ManuMeshFeatureOptions options{};
    EXPECT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_feature_options_init_with_size(&options, sizeof(options))
    );
    EXPECT_EQ(sizeof(options), options.struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, options.abi_version);
    EXPECT_DOUBLE_EQ(40.0, options.feature_angle_deg);
    EXPECT_EQ(1, options.use_normal_tensor_features);
    EXPECT_EQ(0, options.use_smooth_curvature_features);
    EXPECT_EQ(1, options.cleanup_feature_graph);
}

TEST_F(CApiTest, DetectsAndCopiesBoundaryFeatureEdges) {
    ManuMeshMeshHandle* mesh = makeSingleTriangle(context);
    ASSERT_NE(nullptr, mesh);

    ManuMeshFeatureOptions options{};
    manumesh_feature_options_init(&options);
    options.use_normal_tensor_features = 0;
    options.cleanup_feature_graph = 0;

    size_t required = 0;
    EXPECT_EQ(
        MANUMESH_STATUS_BUFFER_TOO_SMALL,
        manumesh_detect_feature_edges(context, mesh, &options, nullptr, 0, &required)
    );
    ASSERT_EQ(3u, required);

    std::vector<ManuMeshFeatureEdge> edges(required);
    size_t written = 0;
    EXPECT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_detect_feature_edges(context, mesh, &options, edges.data(), edges.size(), &written)
    );
    ASSERT_EQ(edges.size(), written);
    for (const ManuMeshFeatureEdge& edge : edges) {
        EXPECT_GE(edge.a, 0);
        EXPECT_LT(edge.a, 3);
        EXPECT_GE(edge.b, 0);
        EXPECT_LT(edge.b, 3);
        EXPECT_NE(edge.a, edge.b);
        EXPECT_EQ(1, edge.boundary);
        EXPECT_EQ(0, edge.removed_by_cleanup);
    }

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, ReportsRequiredFeatureEdgeCapacity) {
    ManuMeshMeshHandle* mesh = makeSingleTriangle(context);
    ASSERT_NE(nullptr, mesh);

    ManuMeshFeatureEdge oneEdge{};
    size_t written = 0;
    EXPECT_EQ(
        MANUMESH_STATUS_BUFFER_TOO_SMALL,
        manumesh_detect_feature_edges(context, mesh, nullptr, &oneEdge, 1, &written)
    );
    EXPECT_EQ(3u, written);

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, RejectsInvalidFeatureOptionsAndBuffers) {
    ManuMeshMeshHandle* mesh = makeSingleTriangle(context);
    ASSERT_NE(nullptr, mesh);

    ManuMeshFeatureOptions uninitialized{};
    size_t written = 99;
    EXPECT_EQ(
        MANUMESH_STATUS_INVALID_ARGUMENT,
        manumesh_detect_feature_edges(context, mesh, &uninitialized, nullptr, 0, &written)
    );
    EXPECT_EQ(0u, written);

    ManuMeshFeatureOptions options{};
    manumesh_feature_options_init(&options);
    options.feature_angle_deg = std::numeric_limits<double>::quiet_NaN();
    written = 99;
    EXPECT_EQ(
        MANUMESH_STATUS_INVALID_ARGUMENT,
        manumesh_detect_feature_edges(context, mesh, &options, nullptr, 0, &written)
    );
    EXPECT_EQ(0u, written);

    manumesh_feature_options_init(&options);
    written = 99;
    EXPECT_EQ(
        MANUMESH_STATUS_INVALID_ARGUMENT,
        manumesh_detect_feature_edges(context, mesh, &options, nullptr, 1, &written)
    );
    EXPECT_EQ(0u, written);

    manumesh_mesh_destroy(mesh);
}
