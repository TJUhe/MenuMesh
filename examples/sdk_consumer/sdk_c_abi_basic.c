/**
 * @file examples/sdk_consumer/sdk_c_abi_basic.c
 * @brief Demonstrates sdk c abi basic through the ManuMesh SDK examples.
 * @ingroup manumesh_examples
 *
 * @details The example intentionally uses only supported public entry points and doubles as executable integration documentation.
 */

#include "api/CApi.h"

#include <stddef.h>
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
        options.quality_refinement_iterations != 0) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 4;
    }
    options.target_ratio = 0.35;

    ManuMeshSimplifyReport report;
    memset(&report, 0xA5, sizeof(report));
    ManuMeshStatus status = manumesh_simplify_mesh(context, input, &options, output, &report);
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
