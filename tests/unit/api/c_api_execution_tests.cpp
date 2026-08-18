/**
 * @file tests/unit/api/c_api_execution_tests.cpp
 * @brief 验证 C ABI 上下文级执行策略及其算法结果契约。
 */

#include "CApiTestSupport.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace {

ManuMeshExecutionOptions makeParallelOptions() {
    ManuMeshExecutionOptions options{};
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_execution_options_init_with_size(&options, sizeof(options)));
    options.mode = MANUMESH_EXECUTION_MODE_PARALLEL;
    options.max_concurrency = 4;
    options.min_items_per_task = 32;
    return options;
}

void expectMeshGeometryEqual(
    ManuMeshContext* context, const ManuMeshMeshHandle* first, const ManuMeshMeshHandle* second
) {
    size_t firstVertices = 0;
    size_t firstFaces = 0;
    size_t secondVertices = 0;
    size_t secondFaces = 0;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, first, &firstVertices, &firstFaces));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, second, &secondVertices, &secondFaces));
    ASSERT_EQ(firstVertices, secondVertices);
    ASSERT_EQ(firstFaces, secondFaces);

    std::vector<ManuMeshVec3> firstVertexData(firstVertices);
    std::vector<ManuMeshVec3> secondVertexData(secondVertices);
    std::vector<ManuMeshFace> firstFaceData(firstFaces);
    std::vector<ManuMeshFace> secondFaceData(secondFaces);
    size_t written = 0;
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_mesh_copy_vertices(context, first, firstVertexData.data(), firstVertexData.size(), &written)
    );
    ASSERT_EQ(firstVertexData.size(), written);
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_mesh_copy_vertices(context, second, secondVertexData.data(), secondVertexData.size(), &written)
    );
    ASSERT_EQ(secondVertexData.size(), written);
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_mesh_copy_faces(context, first, firstFaceData.data(), firstFaceData.size(), &written)
    );
    ASSERT_EQ(firstFaceData.size(), written);
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_mesh_copy_faces(context, second, secondFaceData.data(), secondFaceData.size(), &written)
    );
    ASSERT_EQ(secondFaceData.size(), written);

    for (size_t i = 0; i < firstVertexData.size(); ++i) {
        EXPECT_DOUBLE_EQ(firstVertexData[i].x, secondVertexData[i].x) << "vertex=" << i;
        EXPECT_DOUBLE_EQ(firstVertexData[i].y, secondVertexData[i].y) << "vertex=" << i;
        EXPECT_DOUBLE_EQ(firstVertexData[i].z, secondVertexData[i].z) << "vertex=" << i;
    }
    for (size_t i = 0; i < firstFaceData.size(); ++i) {
        EXPECT_EQ(firstFaceData[i].v[0], secondFaceData[i].v[0]) << "face=" << i;
        EXPECT_EQ(firstFaceData[i].v[1], secondFaceData[i].v[1]) << "face=" << i;
        EXPECT_EQ(firstFaceData[i].v[2], secondFaceData[i].v[2]) << "face=" << i;
    }
}

} // namespace

TEST_F(CApiTest, InitializesExecutionOptionsAndReportsBackendCapability) {
    ManuMeshExecutionOptions options{};
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_execution_options_init_with_size(&options, sizeof(options)));
    EXPECT_EQ(sizeof(options), options.struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, options.abi_version);
    EXPECT_EQ(MANUMESH_EXECUTION_MODE_SERIAL, options.mode);
    EXPECT_EQ(0, options.max_concurrency);
    EXPECT_GT(options.min_items_per_task, 0u);

    const int available = manumesh_parallel_execution_available();
    EXPECT_TRUE(available == 0 || available == 1);
    const char* backend = manumesh_parallel_execution_backend();
    ASSERT_NE(nullptr, backend);
    if (available) {
        EXPECT_STREQ("oneTBB", backend);
    } else {
        EXPECT_STREQ("serial", backend);
    }
}

TEST_F(CApiTest, RejectsInvalidExecutionOptionsWithoutChangingContext) {
    ManuMeshExecutionOptions options{};
    EXPECT_EQ(
        MANUMESH_STATUS_INVALID_ARGUMENT,
        manumesh_context_set_execution_options(context, &options)
    );
    EXPECT_NE(std::string::npos, std::string(manumesh_context_last_error(context)).find("initialized"));

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_execution_options_init_with_size(&options, sizeof(options)));
    options.mode = static_cast<ManuMeshExecutionMode>(99);
    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_context_set_execution_options(context, &options));
    EXPECT_NE(std::string::npos, std::string(manumesh_context_last_error(context)).find("mode"));

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_execution_options_init_with_size(&options, sizeof(options)));
    options.max_concurrency = -1;
    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_context_set_execution_options(context, &options));

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_execution_options_init_with_size(&options, sizeof(options)));
    options.min_items_per_task = 0;
    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_context_set_execution_options(context, &options));

    // A valid update remains usable after all rejected updates.
    options = makeParallelOptions();
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_context_set_execution_options(context, &options));
    EXPECT_TRUE(manumesh_context_last_error(context)[0] == '\0');
}

TEST_F(CApiTest, ContextExecutionOptionsKeepFeatureAndSimplificationResultsEquivalent) {
    ManuMeshContext* parallelContext = manumesh_context_create();
    ASSERT_NE(nullptr, parallelContext);

    ManuMeshExecutionOptions parallelOptions = makeParallelOptions();
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_context_set_execution_options(parallelContext, &parallelOptions));

    ManuMeshMeshHandle* serialInput = manumesh_mesh_create(context);
    ManuMeshMeshHandle* parallelInput = manumesh_mesh_create(parallelContext);
    ManuMeshMeshHandle* serialOutput = manumesh_mesh_create(context);
    ManuMeshMeshHandle* parallelOutput = manumesh_mesh_create(parallelContext);
    ASSERT_NE(nullptr, serialInput);
    ASSERT_NE(nullptr, parallelInput);
    ASSERT_NE(nullptr, serialOutput);
    ASSERT_NE(nullptr, parallelOutput);
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "bump", 32, serialInput));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_copy(parallelContext, serialInput, parallelInput));

    ManuMeshFeatureOptions featureOptions{};
    manumesh_feature_options_init(&featureOptions);
    featureOptions.normal_filter_enabled = 1;
    featureOptions.normal_filter_iterations = 2;
    featureOptions.normal_tensor_scale_count = 3;
    featureOptions.normal_tensor_smoothing_iterations = 1;

    size_t serialRequired = 0;
    size_t parallelRequired = 0;
    EXPECT_EQ(
        MANUMESH_STATUS_BUFFER_TOO_SMALL,
        manumesh_detect_feature_edges(context, serialInput, &featureOptions, nullptr, 0, &serialRequired)
    );
    EXPECT_EQ(
        MANUMESH_STATUS_BUFFER_TOO_SMALL,
        manumesh_detect_feature_edges(
            parallelContext, parallelInput, &featureOptions, nullptr, 0, &parallelRequired
        )
    );
    ASSERT_EQ(serialRequired, parallelRequired);
    std::vector<ManuMeshFeatureEdge> serialEdges(serialRequired);
    std::vector<ManuMeshFeatureEdge> parallelEdges(parallelRequired);
    size_t written = 0;
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_detect_feature_edges(
            context, serialInput, &featureOptions, serialEdges.data(), serialEdges.size(), &written
        )
    );
    ASSERT_EQ(serialEdges.size(), written);
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_detect_feature_edges(
            parallelContext, parallelInput, &featureOptions, parallelEdges.data(), parallelEdges.size(), &written
        )
    );
    ASSERT_EQ(parallelEdges.size(), written);
    for (size_t i = 0; i < serialEdges.size(); ++i) {
        EXPECT_EQ(0, std::memcmp(&serialEdges[i], &parallelEdges[i], sizeof(ManuMeshFeatureEdge)))
            << "feature edge=" << i;
    }

    ManuMeshSimplifyOptions simplifyOptions{};
    manumesh_simplify_options_init(&simplifyOptions);
    simplifyOptions.target_ratio = 0.70;
    simplifyOptions.use_normal_tensor_features = 1;
    simplifyOptions.normal_tensor_scale_count = 3;
    simplifyOptions.normal_tensor_smoothing_iterations = 1;
    ManuMeshSimplifyReport serialReport{};
    ManuMeshSimplifyReport parallelReport{};
    manumesh_simplify_report_init(&serialReport);
    manumesh_simplify_report_init(&parallelReport);
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_simplify_mesh(context, serialInput, &simplifyOptions, serialOutput, &serialReport)
    );
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_simplify_mesh(parallelContext, parallelInput, &simplifyOptions, parallelOutput, &parallelReport)
    );
    EXPECT_EQ(serialReport.final_faces, parallelReport.final_faces);
    EXPECT_EQ(serialReport.final_vertices, parallelReport.final_vertices);
    EXPECT_EQ(serialReport.collapsed_edges, parallelReport.collapsed_edges);
    EXPECT_EQ(serialReport.rejected_collapses, parallelReport.rejected_collapses);
    expectMeshGeometryEqual(context, serialOutput, parallelOutput);

    manumesh_mesh_destroy(parallelOutput);
    manumesh_mesh_destroy(serialOutput);
    manumesh_mesh_destroy(parallelInput);
    manumesh_mesh_destroy(serialInput);
    manumesh_context_destroy(parallelContext);
}
