/*
 * Intentionally has no include guard. Define MANUMESH_C_API_SYMBOL before
 * including this file to verify every exported C ABI declaration.
 */

MANUMESH_C_API_SYMBOL(manumesh_version, const char*, (void));
MANUMESH_C_API_SYMBOL(manumesh_status_message, const char*, (ManuMeshStatus));
MANUMESH_C_API_SYMBOL(manumesh_context_create, ManuMeshContext*, (void));
MANUMESH_C_API_SYMBOL(manumesh_context_destroy, void, (ManuMeshContext*));
MANUMESH_C_API_SYMBOL(manumesh_context_last_error, const char*, (const ManuMeshContext*));
MANUMESH_C_API_SYMBOL(manumesh_context_clear_error, void, (ManuMeshContext*));
MANUMESH_C_API_SYMBOL(
    manumesh_execution_options_init_with_size,
    ManuMeshStatus,
    (ManuMeshExecutionOptions*, size_t)
);
MANUMESH_C_API_SYMBOL(manumesh_execution_options_init, void, (ManuMeshExecutionOptions*));
MANUMESH_C_API_SYMBOL(
    manumesh_context_set_execution_options,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshExecutionOptions*)
);
MANUMESH_C_API_SYMBOL(manumesh_parallel_execution_available, int, (void));
MANUMESH_C_API_SYMBOL(manumesh_parallel_execution_backend, const char*, (void));
MANUMESH_C_API_SYMBOL(manumesh_mesh_create, ManuMeshMeshHandle*, (ManuMeshContext*));
MANUMESH_C_API_SYMBOL(manumesh_mesh_destroy, void, (ManuMeshMeshHandle*));
MANUMESH_C_API_SYMBOL(manumesh_mesh_clear, ManuMeshStatus, (ManuMeshContext*, ManuMeshMeshHandle*));
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_set_data,
    ManuMeshStatus,
    (ManuMeshContext*, ManuMeshMeshHandle*, const ManuMeshVec3*, size_t, const ManuMeshFace*, size_t)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_set_data_with_texcoords,
    ManuMeshStatus,
    (ManuMeshContext*,
     ManuMeshMeshHandle*,
     const ManuMeshVec3*,
     size_t,
     const ManuMeshFace*,
     size_t,
     const ManuMeshFaceTexCoords*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_get_counts,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, size_t*, size_t*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_copy_vertices,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshVec3*, size_t, size_t*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_copy_faces,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshFace*, size_t, size_t*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_copy,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshMeshHandle*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_get_vertex,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, size_t, ManuMeshVec3*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_set_vertex,
    ManuMeshStatus,
    (ManuMeshContext*, ManuMeshMeshHandle*, size_t, ManuMeshVec3)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_get_face,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, size_t, ManuMeshFace*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_set_face,
    ManuMeshStatus,
    (ManuMeshContext*, ManuMeshMeshHandle*, size_t, ManuMeshFace)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_get_bounds,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshBounds*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_translate,
    ManuMeshStatus,
    (ManuMeshContext*, ManuMeshMeshHandle*, ManuMeshVec3)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_transform,
    ManuMeshStatus,
    (ManuMeshContext*, ManuMeshMeshHandle*, const double*)
);
MANUMESH_C_API_SYMBOL(manumesh_mesh_compact, ManuMeshStatus, (ManuMeshContext*, ManuMeshMeshHandle*));
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_validate,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, int, size_t*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_copy_face_areas,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, double*, size_t, size_t*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_get_surface_area,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, double*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_get_signed_volume,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, double*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_get_surface_centroid,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshVec3*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_copy_face_centroids,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshVec3*, size_t, size_t*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_copy_face_normals,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshVec3*, size_t, size_t*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_copy_vertex_normals,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshVec3*, size_t, size_t*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_copy_unique_edges,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshEdge*, size_t, size_t*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_get_topology_summary,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshTopologySummary*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_has_texture_coordinates,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, int*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_get_face_texcoords,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, size_t, ManuMeshFaceTexCoords*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_set_face_texcoords,
    ManuMeshStatus,
    (ManuMeshContext*, ManuMeshMeshHandle*, size_t, const ManuMeshFaceTexCoords*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_copy_face_texcoords,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshFaceTexCoords*, size_t, size_t*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_reverse_winding,
    ManuMeshStatus,
    (ManuMeshContext*, ManuMeshMeshHandle*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_remove_degenerate_faces,
    ManuMeshStatus,
    (ManuMeshContext*, ManuMeshMeshHandle*, size_t*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_append,
    ManuMeshStatus,
    (ManuMeshContext*, ManuMeshMeshHandle*, const ManuMeshMeshHandle*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_load_mesh,
    ManuMeshStatus,
    (ManuMeshContext*, const char*, ManuMeshMeshHandle*, double)
);
MANUMESH_C_API_SYMBOL(
    manumesh_save_ascii_stl,
    ManuMeshStatus,
    (ManuMeshContext*, const char*, const ManuMeshMeshHandle*, const char*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_save_binary_stl,
    ManuMeshStatus,
    (ManuMeshContext*, const char*, const ManuMeshMeshHandle*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_save_obj,
    ManuMeshStatus,
    (ManuMeshContext*, const char*, const ManuMeshMeshHandle*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_generate_mesh,
    ManuMeshStatus,
    (ManuMeshContext*, const char*, int, ManuMeshMeshHandle*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_feature_options_init_with_size,
    ManuMeshStatus,
    (ManuMeshFeatureOptions*, size_t)
);
MANUMESH_C_API_SYMBOL(manumesh_feature_options_init, void, (ManuMeshFeatureOptions*));
MANUMESH_C_API_SYMBOL(
    manumesh_detect_feature_edges,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, const ManuMeshFeatureOptions*, ManuMeshFeatureEdge*, size_t, size_t*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_detect_feature_edges_v2,
    ManuMeshStatus,
    (ManuMeshContext*,
     const ManuMeshMeshHandle*,
     const ManuMeshFeatureOptions*,
     ManuMeshFeatureEdgeV2*,
     size_t,
     size_t*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_simplify_options_init_with_size,
    ManuMeshStatus,
    (ManuMeshSimplifyOptions*, size_t)
);
MANUMESH_C_API_SYMBOL(
    manumesh_simplify_report_init_with_size,
    ManuMeshStatus,
    (ManuMeshSimplifyReport*, size_t)
);
MANUMESH_C_API_SYMBOL(
    manumesh_mesh_stats_init_with_size,
    ManuMeshStatus,
    (ManuMeshMeshStats*, size_t)
);
MANUMESH_C_API_SYMBOL(manumesh_simplify_options_init, void, (ManuMeshSimplifyOptions*));
MANUMESH_C_API_SYMBOL(manumesh_simplify_report_init, void, (ManuMeshSimplifyReport*));
MANUMESH_C_API_SYMBOL(manumesh_mesh_stats_init, void, (ManuMeshMeshStats*));
MANUMESH_C_API_SYMBOL(
    manumesh_simplify_mesh_with_report_size,
    ManuMeshStatus,
    (ManuMeshContext*,
     const ManuMeshMeshHandle*,
     const ManuMeshSimplifyOptions*,
     ManuMeshMeshHandle*,
     ManuMeshSimplifyReport*,
     size_t)
);
MANUMESH_C_API_SYMBOL(
    manumesh_compute_mesh_stats_with_size,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshMeshStats*, size_t)
);
MANUMESH_C_API_SYMBOL(
    manumesh_simplify_mesh,
    ManuMeshStatus,
    (ManuMeshContext*,
     const ManuMeshMeshHandle*,
     const ManuMeshSimplifyOptions*,
     ManuMeshMeshHandle*,
     ManuMeshSimplifyReport*)
);
MANUMESH_C_API_SYMBOL(
    manumesh_compute_mesh_stats,
    ManuMeshStatus,
    (ManuMeshContext*, const ManuMeshMeshHandle*, ManuMeshMeshStats*)
);
