/**
 * @file examples/sdk_consumer/sdk_c_abi_basic.c
 * @brief 验证已安装 SDK 的稳定 C ABI 基本调用路径。
 * @ingroup manumesh_examples
 *
 * @details 示例只使用已安装 SDK 的公共 C ABI 入口，作为可执行的集成文档。
 */

#include "api/CApi.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    ManuMeshContext* context = manumesh_context_create();
    if (!context) {
        return 1;
    }

    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    if (!input || !output) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 2;
    }

    if (manumesh_generate_mesh(context, "cylinder", 32, input) != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 3;
    }

    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    if (options.struct_size != sizeof(options) || options.loop_trace_angle_deg != -1.0 ||
        options.quality_refinement_iterations != 0 || options.feature_options != NULL) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 4;
    }
    options.target_ratio = 0.35;
    options.preserve_feature_curves = 1;

    ManuMeshFeatureOptions feature_options;
    manumesh_feature_options_init(&feature_options);
    if (feature_options.struct_size != sizeof(feature_options) || feature_options.abi_version != MANUMESH_ABI_VERSION) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 9;
    }
    feature_options.feature_angle_deg = 35.0;
    feature_options.use_normal_tensor_features = 0;
    feature_options.use_smooth_curvature_features = 0;
    feature_options.cleanup_feature_graph = 0;
    options.feature_options = &feature_options;

    size_t input_vertex_count = 0;
    ManuMeshStatus status = manumesh_mesh_get_counts(context, input, &input_vertex_count, NULL);
    if (status != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 10;
    }

    size_t feature_edge_count = 0;
    status = manumesh_detect_feature_edges_v2(context, input, &feature_options, NULL, 0, &feature_edge_count);
    if (status != MANUMESH_STATUS_BUFFER_TOO_SMALL || feature_edge_count == 0 ||
        feature_edge_count > (size_t)-1 / sizeof(ManuMeshFeatureEdgeV2)) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 11;
    }

    ManuMeshFeatureEdgeV2* feature_edges =
        (ManuMeshFeatureEdgeV2*)malloc(feature_edge_count * sizeof(ManuMeshFeatureEdgeV2));
    if (!feature_edges) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 12;
    }

    size_t feature_edges_written = 0;
    status = manumesh_detect_feature_edges_v2(
        context, input, &feature_options, feature_edges, feature_edge_count, &feature_edges_written
    );
    if (status != MANUMESH_STATUS_OK || feature_edges_written != feature_edge_count) {
        free(feature_edges);
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 13;
    }
    for (size_t edge_index = 0; edge_index < feature_edges_written; ++edge_index) {
        const ManuMeshFeatureEdgeV2* edge = &feature_edges[edge_index];
        if (edge->a < 0 || edge->b < 0 || edge->a == edge->b || (size_t)edge->a >= input_vertex_count ||
            (size_t)edge->b >= input_vertex_count || edge->feature_edge_index != (uint64_t)edge_index ||
            (edge->geometric_constraint && edge->input_edge_index == MANUMESH_INVALID_EDGE_INDEX) ||
            (!edge->geometric_constraint && edge->input_edge_index != MANUMESH_INVALID_EDGE_INDEX)) {
            free(feature_edges);
            manumesh_mesh_destroy(output);
            manumesh_mesh_destroy(input);
            manumesh_context_destroy(context);
            return 14;
        }
    }
    free(feature_edges);

    ManuMeshSimplifyReport report;
    memset(&report, 0xA5, sizeof(report));
    status = manumesh_simplify_mesh(context, input, &options, output, &report);
    if (status != MANUMESH_STATUS_OK || report.struct_size != sizeof(report)) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 5;
    }

    size_t vertex_count = 0;
    size_t face_count = 0;
    status = manumesh_mesh_get_counts(context, output, &vertex_count, &face_count);
    if (status != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 6;
    }

    ManuMeshMeshStats stats;
    memset(&stats, 0xA5, sizeof(stats));
    status = manumesh_compute_mesh_stats(context, output, &stats);
    if (status != MANUMESH_STATUS_OK || stats.struct_size != sizeof(stats) || stats.faces <= 0) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 7;
    }

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
    manumesh_context_destroy(context);

    return face_count > 0 && (size_t)report.final_faces == face_count ? 0 : 8;
}
