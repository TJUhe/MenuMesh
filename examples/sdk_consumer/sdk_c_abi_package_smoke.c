/**
 * @file examples/sdk_consumer/sdk_c_abi_package_smoke.c
 * @brief Verifies that the installed C ABI can be consumed without Eigen.
 */

#include "api/CApi.h"

#include <stddef.h>

int main(void) {
    ManuMeshSimplifyOptions options;
    ManuMeshFeatureOptions feature_options;
    manumesh_simplify_options_init(&options);
    manumesh_feature_options_init(&feature_options);

    if (options.struct_size != sizeof(options) || options.feature_options != NULL ||
        feature_options.struct_size != sizeof(feature_options) || feature_options.abi_version != MANUMESH_ABI_VERSION) {
        return 1;
    }

    ManuMeshContext* context = manumesh_context_create();
    ManuMeshMeshHandle* mesh = context ? manumesh_mesh_create(context) : NULL;
    if (!context || !mesh) {
        manumesh_mesh_destroy(mesh);
        manumesh_context_destroy(context);
        return 2;
    }
    if (manumesh_generate_mesh(context, "plane", 2, mesh) != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(mesh);
        manumesh_context_destroy(context);
        return 3;
    }

    feature_options.use_normal_tensor_features = 0;
    feature_options.cleanup_feature_graph = 0;
    size_t feature_edge_count = 0;
    const ManuMeshStatus status =
        manumesh_detect_feature_edges_v2(context, mesh, &feature_options, NULL, 0, &feature_edge_count);
    manumesh_mesh_destroy(mesh);
    manumesh_context_destroy(context);
    if (status != MANUMESH_STATUS_BUFFER_TOO_SMALL || feature_edge_count == 0) {
        return 4;
    }

    return 0;
}
