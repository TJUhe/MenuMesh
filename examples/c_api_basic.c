/**
 * @file examples/c_api_basic.c
 * @brief 演示纯 C 调用方创建、简化并读取网格。
 * @ingroup manumesh_examples
 *
 * @details 示例只使用受支持的公共入口，同时展示 C API 的基本调用流程。
 */

#include "api/CApi.h"

#include <stdio.h>

static int fail_with_status(ManuMeshContext* context, ManuMeshStatus status) {
    fprintf(stderr, "manumesh error: %s", manumesh_status_message(status));
    if (context) {
        const char* detail = manumesh_context_last_error(context);
        if (detail && detail[0] != '\0') {
            fprintf(stderr, " (%s)", detail);
        }
    }
    fprintf(stderr, "\n");
    return 1;
}

int main(void) {
    ManuMeshContext* context = manumesh_context_create();
    if (!context) {
        fprintf(stderr, "failed to allocate manumesh context\n");
        return 1;
    }

    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    if (!input || !output) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 1;
    }

    ManuMeshStatus status = manumesh_generate_mesh(context, "cylinder", 32, input);
    if (status != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        int rc = fail_with_status(context, status);
        manumesh_context_destroy(context);
        return rc;
    }

    size_t input_vertices = 0;
    size_t input_faces = 0;
    status = manumesh_mesh_get_counts(context, input, &input_vertices, &input_faces);
    if (status != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        int rc = fail_with_status(context, status);
        manumesh_context_destroy(context);
        return rc;
    }

    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    options.target_ratio = 0.35;
    options.boundary_weight = 1.0;

    ManuMeshSimplifyReport report;
    manumesh_simplify_report_init(&report);
    status = manumesh_simplify_mesh(context, input, &options, output, &report);
    if (status != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        int rc = fail_with_status(context, status);
        manumesh_context_destroy(context);
        return rc;
    }

    ManuMeshMeshStats stats;
    manumesh_mesh_stats_init(&stats);
    status = manumesh_compute_mesh_stats(context, output, &stats);
    if (status != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        int rc = fail_with_status(context, status);
        manumesh_context_destroy(context);
        return rc;
    }

    printf(
        "manumesh %s: input_faces=%zu simplified_faces=%d "
        "collapsed_edges=%d\n",
        manumesh_version(),
        input_faces,
        stats.faces,
        report.collapsed_edges
    );

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
    manumesh_context_destroy(context);
    return stats.faces < (int)input_faces ? 0 : 1;
}
