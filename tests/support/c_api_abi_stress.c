/*
 * Compile the public C ABI with non-default MSVC calling convention and
 * packing flags.  The header must restore its documented pack-8 layout and
 * every exported function must remain callable as __cdecl.
 */

#pragma pack(push, 1)
#include "api/CApi.h"

typedef struct ManuMeshPostHeaderPackProbe {
    char tag;
    double value;
} ManuMeshPostHeaderPackProbe;
#pragma pack(pop)

#include <stddef.h>

#define MANUMESH_C_ABI_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]

MANUMESH_C_ABI_ASSERT(manumesh_vec2_size, sizeof(ManuMeshVec2) == 16);
MANUMESH_C_ABI_ASSERT(manumesh_vec3_size, sizeof(ManuMeshVec3) == 24);
MANUMESH_C_ABI_ASSERT(manumesh_face_size, sizeof(ManuMeshFace) == 12);
MANUMESH_C_ABI_ASSERT(manumesh_texcoord_size, sizeof(ManuMeshFaceTexCoords) == 56);
MANUMESH_C_ABI_ASSERT(manumesh_texcoord_valid_offset, offsetof(ManuMeshFaceTexCoords, valid) == 48);
MANUMESH_C_ABI_ASSERT(manumesh_edge_size, sizeof(ManuMeshEdge) == 24);
MANUMESH_C_ABI_ASSERT(manumesh_edge_face_count_offset, offsetof(ManuMeshEdge, face_count) == 8);
MANUMESH_C_ABI_ASSERT(manumesh_bounds_size, sizeof(ManuMeshBounds) == 56);
MANUMESH_C_ABI_ASSERT(manumesh_bounds_valid_offset, offsetof(ManuMeshBounds, valid) == 48);
MANUMESH_C_ABI_ASSERT(manumesh_topology_summary_size, sizeof(ManuMeshTopologySummary) == 40);
MANUMESH_C_ABI_ASSERT(
    manumesh_topology_summary_closed_offset, offsetof(ManuMeshTopologySummary, closed_manifold) == 32
);
MANUMESH_C_ABI_ASSERT(
    manumesh_feature_options_double_alignment, offsetof(ManuMeshFeatureOptions, feature_angle_deg) == 16
);
MANUMESH_C_ABI_ASSERT(
    manumesh_feature_edge_v2_u64_alignment, offsetof(ManuMeshFeatureEdgeV2, feature_edge_index) == 48
);
MANUMESH_C_ABI_ASSERT(
    manumesh_simplify_options_pointer_alignment, (offsetof(ManuMeshSimplifyOptions, feature_options) % 8) == 0
);
MANUMESH_C_ABI_ASSERT(
    manumesh_simplify_options_texture_tail,
    offsetof(ManuMeshSimplifyOptions, preserve_texture) > offsetof(ManuMeshSimplifyOptions, feature_options)
);
MANUMESH_C_ABI_ASSERT(
    manumesh_simplify_options_texture_double_alignment,
    (offsetof(ManuMeshSimplifyOptions, texture_weight) % 8) == 0 &&
        (offsetof(ManuMeshSimplifyOptions, texture_seam_tolerance) % 8) == 0 &&
        (offsetof(ManuMeshSimplifyOptions, min_texture_area_ratio) % 8) == 0
);
MANUMESH_C_ABI_ASSERT(
    manumesh_simplify_report_double_alignment, (offsetof(ManuMeshSimplifyReport, min_applied_line_weight) % 8) == 0
);
MANUMESH_C_ABI_ASSERT(
    manumesh_simplify_report_texture_tail,
    offsetof(ManuMeshSimplifyReport, texture_rejected_collapses) >
        offsetof(ManuMeshSimplifyReport, quality_refinement_skipped_for_texture)
);
MANUMESH_C_ABI_ASSERT(manumesh_header_restores_pack, offsetof(ManuMeshPostHeaderPackProbe, value) == 1);
MANUMESH_C_ABI_ASSERT(manumesh_header_restores_pack_size, sizeof(ManuMeshPostHeaderPackProbe) == 9);

#if defined(MANUMESH_EXPECT_DLL_IMPORT) && !defined(MANUMESH_USING_DLL)
#error "ManuMesh::c_api must propagate MANUMESH_USING_DLL for shared builds"
#endif

#if defined(MANUMESH_EXPECT_STATIC_API) && defined(MANUMESH_USING_DLL)
#error "ManuMesh::c_api must not propagate MANUMESH_USING_DLL for static builds"
#endif

#if defined(MANUMESH_BUILDING_DLL)
#error "ManuMesh::c_api must never expose the library-only MANUMESH_BUILDING_DLL definition"
#endif

static int manumesh_verify_cdecl_aliases(void) {
#define MANUMESH_C_API_ALIAS(symbol, result, parameters)                                                               \
    do {                                                                                                               \
        typedef result(MANUMESH_CDECL * symbol##_cdecl_pointer) parameters;                                            \
        volatile symbol##_cdecl_pointer pointer = &(symbol);                                                           \
        if (pointer == NULL) {                                                                                         \
            return 0;                                                                                                  \
        }                                                                                                              \
    } while (0)
    MANUMESH_C_API_ALIAS(manumesh_feature_options_init, void, (ManuMeshFeatureOptions*));
    MANUMESH_C_API_ALIAS(manumesh_simplify_options_init, void, (ManuMeshSimplifyOptions*));
    MANUMESH_C_API_ALIAS(manumesh_simplify_report_init, void, (ManuMeshSimplifyReport*));
    MANUMESH_C_API_ALIAS(manumesh_mesh_stats_init, void, (ManuMeshMeshStats*));
    MANUMESH_C_API_ALIAS(
        manumesh_simplify_mesh,
        ManuMeshStatus,
        (ManuMeshContext*,
         const ManuMeshMeshHandle*,
         const ManuMeshSimplifyOptions*,
         ManuMeshMeshHandle*,
         ManuMeshSimplifyReport*)
    );
    MANUMESH_C_API_ALIAS(
        manumesh_compute_mesh_stats, ManuMeshStatus, (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshMeshStats*)
    );
#undef MANUMESH_C_API_ALIAS
    return 1;
}

#undef manumesh_feature_options_init
#undef manumesh_simplify_options_init
#undef manumesh_simplify_report_init
#undef manumesh_mesh_stats_init
#undef manumesh_simplify_mesh
#undef manumesh_compute_mesh_stats

static int manumesh_verify_cdecl_exports(void) {
#define MANUMESH_C_API_SYMBOL(symbol, result, parameters)                                                              \
    do {                                                                                                               \
        typedef result(MANUMESH_CDECL * symbol##_cdecl_pointer) parameters;                                            \
        volatile symbol##_cdecl_pointer pointer = &(symbol);                                                           \
        if (pointer == NULL) {                                                                                         \
            return 0;                                                                                                  \
        }                                                                                                              \
    } while (0)
#include "c_api_abi_exports.inc"
#undef MANUMESH_C_API_SYMBOL
    return 1;
}

int MANUMESH_CDECL main(void) {
    if (!manumesh_verify_cdecl_aliases()) {
        return 1;
    }
    if (!manumesh_verify_cdecl_exports()) {
        return 2;
    }

    ManuMeshMeshHandle* mesh = manumesh_mesh_create(NULL);
    if (!mesh) {
        return 3;
    }

    const ManuMeshVec3 vertices[3] = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    const ManuMeshFace faces[1] = {{{0, 1, 2}}};
    if (manumesh_mesh_set_data(NULL, mesh, vertices, 3, faces, 1) != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(mesh);
        return 4;
    }

    size_t vertex_count = 0;
    size_t face_count = 0;
    if (manumesh_mesh_get_counts(NULL, mesh, &vertex_count, &face_count) != MANUMESH_STATUS_OK || vertex_count != 3 ||
        face_count != 1) {
        manumesh_mesh_destroy(mesh);
        return 5;
    }

    ManuMeshVec3 copied[3];
    size_t written = 0;
    if (manumesh_mesh_copy_vertices(NULL, mesh, copied, 3, &written) != MANUMESH_STATUS_OK || written != 3) {
        manumesh_mesh_destroy(mesh);
        return 6;
    }

    const ManuMeshVec3 offset = {1.0, 2.0, 3.0};
    if (manumesh_mesh_translate(NULL, mesh, offset) != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(mesh);
        return 7;
    }
    manumesh_mesh_destroy(mesh);
    return 0;
}
