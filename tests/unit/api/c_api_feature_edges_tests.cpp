/**
 * @file tests/unit/api/c_api_feature_edges_tests.cpp
 * @brief Verifies standalone feature-edge detection through the C ABI.
 */

#include "CApiTestSupport.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <set>
#include <utility>
#include <vector>

namespace {
struct FeatureOptionsInitialPrefixLayout {
    std::size_t struct_size;
    unsigned int abi_version;
    double feature_angle_deg;
    double loop_trace_angle_deg;
    double circle_fit_relative_threshold;
    double ellipse_fit_relative_threshold;
    double near_circle_axis_ratio_tolerance;
    int min_feature_loop_vertices;
    int use_normal_tensor_features;
};

static_assert(
    sizeof(FeatureOptionsInitialPrefixLayout) == offsetof(ManuMeshFeatureOptions, normal_tensor_feature_threshold),
    "ManuMeshFeatureOptions initial prefix layout changed"
);

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
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_feature_options_init_with_size(&options, sizeof(options)));
    EXPECT_EQ(sizeof(options), options.struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, options.abi_version);
    EXPECT_DOUBLE_EQ(40.0, options.feature_angle_deg);
    EXPECT_EQ(1, options.use_normal_tensor_features);
    EXPECT_EQ(1, options.cleanup_feature_graph);
}

TEST_F(CApiTest, SizeAwareFeatureInitializerAndReaderSupportActualShortPrefixStorage) {
    struct ShortFeatureOptionsStorage {
        FeatureOptionsInitialPrefixLayout options;
        std::array<unsigned char, sizeof(ManuMeshFeatureOptions)> guard;
    } storage;
    std::memset(&storage, 0xFF, sizeof(storage));

    auto* options = reinterpret_cast<ManuMeshFeatureOptions*>(&storage.options);
    ASSERT_EQ(
        MANUMESH_STATUS_OK, manumesh_feature_options_init_with_size(options, sizeof(FeatureOptionsInitialPrefixLayout))
    );
    EXPECT_EQ(sizeof(FeatureOptionsInitialPrefixLayout), storage.options.struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, storage.options.abi_version);
    EXPECT_DOUBLE_EQ(40.0, storage.options.feature_angle_deg);
    storage.options.use_normal_tensor_features = 0;
    for (std::size_t i = 0; i < storage.guard.size(); ++i) {
        EXPECT_EQ(0xFF, storage.guard[i]) << "initializer overwrote guard byte " << i;
    }

    ManuMeshMeshHandle* mesh = makeSingleTriangle(context);
    ASSERT_NE(nullptr, mesh);
    std::size_t required = 0;
    EXPECT_EQ(
        MANUMESH_STATUS_BUFFER_TOO_SMALL, manumesh_detect_feature_edges(context, mesh, options, nullptr, 0, &required)
    );
    EXPECT_EQ(3u, required);
    for (std::size_t i = 0; i < storage.guard.size(); ++i) {
        EXPECT_EQ(0xFF, storage.guard[i]) << "feature reader overwrote guard byte " << i;
    }
    manumesh_mesh_destroy(mesh);
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
        MANUMESH_STATUS_BUFFER_TOO_SMALL, manumesh_detect_feature_edges(context, mesh, &options, nullptr, 0, &required)
    );
    ASSERT_EQ(3u, required);

    std::vector<ManuMeshFeatureEdge> edges(required);
    size_t written = 0;
    EXPECT_EQ(
        MANUMESH_STATUS_OK, manumesh_detect_feature_edges(context, mesh, &options, edges.data(), edges.size(), &written)
    );
    ASSERT_EQ(edges.size(), written);
    std::set<std::pair<int, int>> endpointPairs;
    for (const ManuMeshFeatureEdge& edge : edges) {
        EXPECT_GE(edge.a, 0);
        EXPECT_LT(edge.a, 3);
        EXPECT_GE(edge.b, 0);
        EXPECT_LT(edge.b, 3);
        EXPECT_NE(edge.a, edge.b);
        EXPECT_EQ(1, edge.boundary);
        EXPECT_EQ(0, edge.dihedral);
        EXPECT_EQ(0, edge.normal_tensor);
        EXPECT_EQ(0, edge.reserved_source);
        EXPECT_EQ(0, edge.removed_by_cleanup);
        endpointPairs.insert(std::make_pair(std::min(edge.a, edge.b), std::max(edge.a, edge.b)));
    }
    const std::set<std::pair<int, int>> expectedPairs = {
        std::make_pair(0, 1),
        std::make_pair(0, 2),
        std::make_pair(1, 2),
    };
    EXPECT_EQ(expectedPairs, endpointPairs);

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, FeatureEdgeV2ReportsStableFeatureAndInputEdgeIndices) {
    ManuMeshMeshHandle* mesh = makeSingleTriangle(context);
    ASSERT_NE(nullptr, mesh);

    ManuMeshFeatureOptions options{};
    manumesh_feature_options_init(&options);
    options.use_normal_tensor_features = 0;
    options.cleanup_feature_graph = 0;

    size_t required = 0;
    EXPECT_EQ(
        MANUMESH_STATUS_BUFFER_TOO_SMALL,
        manumesh_detect_feature_edges_v2(context, mesh, &options, nullptr, 0, &required)
    );
    ASSERT_EQ(3u, required);

    std::vector<ManuMeshFeatureEdgeV2> edges(required);
    size_t written = 0;
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_detect_feature_edges_v2(context, mesh, &options, edges.data(), edges.size(), &written)
    );
    ASSERT_EQ(edges.size(), written);
    const std::array<std::pair<int, int>, 3> expected = {
        std::make_pair(0, 1),
        std::make_pair(0, 2),
        std::make_pair(1, 2),
    };
    for (std::size_t i = 0; i < edges.size(); ++i) {
        EXPECT_EQ(expected[i].first, edges[i].a);
        EXPECT_EQ(expected[i].second, edges[i].b);
        EXPECT_EQ(static_cast<std::uint64_t>(i), edges[i].feature_edge_index);
        EXPECT_EQ(static_cast<std::uint64_t>(i), edges[i].input_edge_index);
        EXPECT_EQ(0, edges[i].synthetic);
        EXPECT_EQ(1, edges[i].geometric_constraint);
    }

    std::vector<ManuMeshFeatureEdgeV2> repeated(required);
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_detect_feature_edges_v2(context, mesh, &options, repeated.data(), repeated.size(), &written)
    );
    EXPECT_EQ(0, std::memcmp(edges.data(), repeated.data(), edges.size() * sizeof(edges[0])));
    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, FeatureEdgeV2CapacityFailureDoesNotPartiallyWrite) {
    ManuMeshMeshHandle* mesh = makeSingleTriangle(context);
    ASSERT_NE(nullptr, mesh);

    ManuMeshFeatureEdgeV2 edge;
    std::memset(&edge, 0xA5, sizeof(edge));
    const ManuMeshFeatureEdgeV2 sentinel = edge;
    size_t written = 0;
    EXPECT_EQ(
        MANUMESH_STATUS_BUFFER_TOO_SMALL, manumesh_detect_feature_edges_v2(context, mesh, nullptr, &edge, 1, &written)
    );
    EXPECT_EQ(3u, written);
    EXPECT_EQ(0, std::memcmp(&sentinel, &edge, sizeof(edge)));
    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, ReportsRequiredFeatureEdgeCapacityWithoutPartialWritesAndClearsErrorOnRetry) {
    ManuMeshMeshHandle* mesh = makeSingleTriangle(context);
    ASSERT_NE(nullptr, mesh);

    ManuMeshFeatureEdge oneEdge;
    std::memset(&oneEdge, 0xA5, sizeof(oneEdge));
    const ManuMeshFeatureEdge sentinel = oneEdge;
    size_t written = 0;
    EXPECT_EQ(
        MANUMESH_STATUS_BUFFER_TOO_SMALL, manumesh_detect_feature_edges(context, mesh, nullptr, &oneEdge, 1, &written)
    );
    EXPECT_EQ(3u, written);
    EXPECT_EQ(0, std::memcmp(&sentinel, &oneEdge, sizeof(oneEdge)));
    EXPECT_STRNE("", manumesh_context_last_error(context));

    std::vector<ManuMeshFeatureEdge> edges(written);
    EXPECT_EQ(
        MANUMESH_STATUS_OK, manumesh_detect_feature_edges(context, mesh, nullptr, edges.data(), edges.size(), &written)
    );
    EXPECT_EQ(edges.size(), written);
    EXPECT_STREQ("", manumesh_context_last_error(context));

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, EmptyFeatureResultReturnsOkAndDoesNotTouchCallerStorage) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(nullptr, mesh);

    size_t written = 99;
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_detect_feature_edges(context, mesh, nullptr, nullptr, 0, &written));
    EXPECT_EQ(0u, written);

    ManuMeshFeatureEdge edge;
    std::memset(&edge, 0x5A, sizeof(edge));
    const ManuMeshFeatureEdge sentinel = edge;
    written = 99;
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_detect_feature_edges(context, mesh, nullptr, &edge, 1, &written));
    EXPECT_EQ(0u, written);
    EXPECT_EQ(0, std::memcmp(&sentinel, &edge, sizeof(edge)));

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
        MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_detect_feature_edges(context, mesh, &options, nullptr, 0, &written)
    );
    EXPECT_EQ(0u, written);

    manumesh_feature_options_init(&options);
    written = 99;
    EXPECT_EQ(
        MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_detect_feature_edges(context, mesh, &options, nullptr, 1, &written)
    );
    EXPECT_EQ(0u, written);

    written = 99;
    EXPECT_EQ(
        MANUMESH_STATUS_INVALID_ARGUMENT,
        manumesh_detect_feature_edges(context, nullptr, &options, nullptr, 0, &written)
    );
    EXPECT_EQ(0u, written);

    manumesh_mesh_destroy(mesh);
}
