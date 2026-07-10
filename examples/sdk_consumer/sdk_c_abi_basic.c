#include "api/CApi.h"

#include <stddef.h>

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
    options.target_ratio = 0.35;

    ManuMeshSimplifyReport report;
    manumesh_simplify_report_init(&report);
    ManuMeshStatus status = manumesh_simplify_mesh(context, input, &options, output, &report);
    if (status != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 4;
    }

    size_t vertex_count = 0;
    size_t face_count = 0;
    status = manumesh_mesh_get_counts(context, output, &vertex_count, &face_count);
    if (status != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 5;
    }

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
    manumesh_context_destroy(context);

    return face_count > 0 && (size_t)report.final_faces == face_count ? 0 : 6;
}
