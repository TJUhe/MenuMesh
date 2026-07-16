/**
 * @file include/api/CApi.h
 * @brief Declares capi facilities for ManuMesh's C-ABI module.
 * @ingroup manumesh_c_api
 *
 * @details The C boundary validates pointers and capacities, translates failures to status codes, and never permits a C++ exception to cross the ABI.
 */

#pragma once

#include "Export.h"

#include <stddef.h>

/**
 * @addtogroup manumesh_c_api
 * @{
 *
 * @brief ManuMesh stable C ABI version 1.
 *
 * @par Threading
 * A ManuMeshContext is not thread-safe. Do not share a single
 * context across threads without external synchronization; give each thread its
 * own context instead.
 *
 * @par Texture coordinates
 * The v1 ABI does not carry per-corner texture
 * coordinates. manumesh_mesh_set_data accepts only vertex positions and face
 * indices, and manumesh_mesh_copy_* returns only positions and indices. UVs
 * that exist on a C++ manumesh::Mesh / PlainMesh (Mesh::faceTexCoords) are not
 * exposed here and are dropped when a mesh crosses this boundary. Use the C++
 * API directly when texture coordinates must be preserved.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ManuMeshContext ManuMeshContext;       ///< Opaque error and operation context.
typedef struct ManuMeshMeshHandle ManuMeshMeshHandle; ///< Opaque owned triangle-mesh handle.

#define MANUMESH_ABI_VERSION 1u

/** @brief Stable status returned by fallible C ABI functions. */
typedef enum ManuMeshStatus {
    MANUMESH_STATUS_OK = 0,               ///< Operation completed successfully.
    MANUMESH_STATUS_INVALID_ARGUMENT = 1, ///< Pointer, value, mesh, or ABI contract is invalid.
    MANUMESH_STATUS_BUFFER_TOO_SMALL = 2, ///< Caller-owned output storage lacks capacity.
    MANUMESH_STATUS_IO_ERROR = 3,         ///< File parsing, creation, read, or write failed.
    MANUMESH_STATUS_ALGORITHM_ERROR = 4,  ///< Geometry algorithm could not complete.
    MANUMESH_STATUS_OUT_OF_MEMORY = 5     ///< Allocation failed.
} ManuMeshStatus;

/** @brief Spatial policy used to scale line-quadric ranking costs. */
typedef enum ManuMeshWeightMode {
    MANUMESH_WEIGHT_MODE_UNIFORM = 0,
    MANUMESH_WEIGHT_MODE_DIHEDRAL = 1,
    MANUMESH_WEIGHT_MODE_HEIGHT = 2,
    MANUMESH_WEIGHT_MODE_X_BAND = 3,
    MANUMESH_WEIGHT_MODE_NORMAL_TENSOR = 4
} ManuMeshWeightMode;

/** @brief Why an otherwise successful simplification run stopped. */
typedef enum ManuMeshSimplifyTerminationReason {
    MANUMESH_SIMPLIFY_TERMINATION_NOT_STARTED = 0,
    MANUMESH_SIMPLIFY_TERMINATION_REACHED_TARGET = 1,
    MANUMESH_SIMPLIFY_TERMINATION_ALREADY_AT_OR_BELOW_TARGET = 2,
    MANUMESH_SIMPLIFY_TERMINATION_NO_CANDIDATES = 3,
    MANUMESH_SIMPLIFY_TERMINATION_REJECTION_LIMIT = 4
} ManuMeshSimplifyTerminationReason;

/** @brief Hard feature policy applied after QEM candidate ranking. */
typedef enum ManuMeshFeatureProtectionMode {
    /** Disable hard feature-curve protection. */
    MANUMESH_FEATURE_PROTECTION_NONE = 0,
    /** Hard-protect circular and near-circular loops only. */
    MANUMESH_FEATURE_PROTECTION_CIRCULAR_ONLY = 1,
    /** Hard-protect fitted circle, near-circle, and ellipse loops. */
    MANUMESH_FEATURE_PROTECTION_PRIMITIVE_CURVES = 2,
    /** Strict behavior: hard-protect every detected feature edge. */
    MANUMESH_FEATURE_PROTECTION_ALL_FEATURE_EDGES = 3
} ManuMeshFeatureProtectionMode;

/** @brief Plain double-precision position in model units. */
typedef struct ManuMeshVec3 {
    double x; ///< X coordinate.
    double y; ///< Y coordinate.
    double z; ///< Z coordinate.
} ManuMeshVec3;

/** @brief Triangle containing three zero-based vertex indices. */
typedef struct ManuMeshFace {
    int v[3]; ///< Counter-clockwise indices into the supplied vertex array.
} ManuMeshFace;

/** @brief Size-versioned simplification options for ABI-compatible extension. */
typedef struct ManuMeshSimplifyOptions {
    /**
   * Must be set by manumesh_simplify_options_init before calling
   * manumesh_simplify_mesh.
   * The library accepts older trailing struct sizes for
   * the same ABI version;
   * fields not present in struct_size keep the library
   * default value.
   */
    size_t struct_size;
    unsigned int abi_version;
    /** Target selection. `target_faces > 0` overrides `target_ratio`. */
    int target_faces;
    double target_ratio;
    /** QEM and line-quadric candidate-ranking costs. */
    int use_line_quadrics;
    double line_weight;
    ManuMeshWeightMode weight_mode;
    double feature_boost;
    double feature_angle_deg;
    int adaptive_scale;
    double adaptive_base_line_weight;
    /** Boundary and feature policies. Hard filters run after QEM ranking. */
    double boundary_weight;
    int preserve_boundary;
    int preserve_feature_curves;
    double feature_curve_weight;
    double max_feature_curve_deviation_ratio;
    /** Feature detection thresholds used before simplification. */
    double circle_fit_relative_threshold;
    double ellipse_fit_relative_threshold;
    double near_circle_axis_ratio_tolerance;
    int min_feature_loop_vertices;
    int min_circular_feature_loop_vertices;
    /** Normal-tensor weak-feature evidence. */
    int use_normal_tensor_features;
    double normal_tensor_feature_threshold;
    double normal_tensor_min_edge_alignment;
    int normal_tensor_smoothing_iterations;
    int normal_tensor_scale_count;
    /** Hard legality filters. Zero local-error budgets disable those tests. */
    double min_triangle_quality;
    double max_normal_deviation_deg;
    double max_local_error;
    double max_local_error_ratio;
    int prevent_local_intersections;
    int verbose;
    ManuMeshFeatureProtectionMode feature_protection_mode;
    double loop_trace_angle_deg;
    int normal_tensor_min_persistent_scales;
    int cleanup_feature_graph;
    double feature_graph_gap_length_ratio;
    int feature_graph_max_weak_spur_edges;
    double feature_component_min_confidence;
    /** Optional fixed-topology quality-improvement passes after simplification. */
    int quality_refinement_iterations;
    /** Optional deterministic smooth ridge/valley evidence. Tail extension. */
    int use_smooth_curvature_features;
    double smooth_curvature_feature_threshold;
    double smooth_curvature_min_edge_alignment;
    double smooth_curvature_min_tangent_consistency;
    int smooth_curvature_base_neighborhood_rings;
    int smooth_curvature_scale_count;
    int smooth_curvature_min_persistent_scales;
    int smooth_curvature_robust_fit_iterations;
    /** Integrated weak-spur strength threshold; zero keeps edge-count cleanup. */
    double feature_graph_min_weak_spur_strength;
    /** Optional noisy-input preprocessing and graph recovery. Tail extension. */
    int use_feature_normal_filter;
    int feature_normal_filter_iterations;
    double feature_normal_filter_angle_sigma_deg;
    double feature_normal_filter_preserve_angle_deg;
    double feature_normal_filter_relaxation;
    int smooth_curvature_use_stable_scale_selection;
    double smooth_curvature_min_scale_stability;
    int consolidate_feature_graph;
    double feature_graph_consolidation_gap_length_ratio;
    double feature_graph_consolidation_min_alignment;
} ManuMeshSimplifyOptions;

/** @brief Size-versioned diagnostics from one simplification run. */
typedef struct ManuMeshSimplifyReport {
    /**
   * Output storage for simplification diagnostics. Current source calls to
   * manumesh_simplify_mesh pass sizeof(ManuMeshSimplifyReport) through the
   * size-aware inline wrapper, so this object need not be pre-initialized.
   * Call manumesh_simplify_report_init when an initialized standalone value is
   * useful before the simplify call.
   */
    size_t struct_size;
    unsigned int abi_version;
    int initial_vertices;
    int initial_faces;
    int final_vertices;
    int final_faces;
    int collapsed_edges;
    int rejected_collapses;
    /** Current candidates whose placement solve used endpoint/midpoint fallback. */
    int solver_fallbacks;
    int queue_rebuilds;
    /** Feature-detection summary from the input mesh. */
    int feature_loops;
    int circular_feature_loops;
    int feature_vertices;
    int normal_tensor_feature_edges;
    /** First hard filter that rejected each current collapse candidate. */
    int feature_rejected_collapses;
    int primitive_feature_rejected_collapses;
    int generic_feature_rejected_collapses;
    int boundary_rejected_collapses;
    int topology_rejected_collapses;
    int normal_flip_rejected_collapses;
    int quality_rejected_collapses;
    int self_intersection_rejected_collapses;
    int curve_budget_rejected_collapses;
    int error_rejected_collapses;
    int projected_feature_placements;
    ManuMeshSimplifyTerminationReason termination_reason;
    double min_applied_line_weight;
    double max_applied_line_weight;
    /** Feature trace diagnostics. Added at the tail to preserve old offsets. */
    int traced_feature_edges;
    int untraced_feature_edges;
    /** Normal-tensor scale diagnostics. Added at the tail for ABI compatibility. */
    int normal_tensor_scored_vertices;
    double max_normal_tensor_persistent_score;
    double mean_normal_tensor_local_scale;
    double mean_normal_tensor_persistence;
    /** Feature component confidence and cleanup diagnostics. */
    int feature_components;
    int weak_feature_components;
    int high_confidence_feature_components;
    int graph_cleanup_bridged_gaps;
    int graph_cleanup_removed_spurs;
    int graph_cleanup_merged_junctions;
    double mean_feature_component_confidence;
    double min_feature_component_confidence;
    /** Fixed-topology second-round quality-refinement diagnostics. */
    int quality_refinement_iterations_completed;
    int quality_refinement_attempted_moves;
    int quality_refinement_accepted_moves;
    /** Input faces tolerated as degenerate (zero area or repeated vertex).
   * Added at the tail to preserve old field offsets. */
    int degenerate_input_faces;
    /** Smooth-curvature and feature-recovery diagnostics. Tail extension. */
    int smooth_curvature_feature_edges;
    int smooth_curvature_scored_vertices;
    double max_smooth_curvature_persistent_score;
    double mean_smooth_curvature_local_scale;
    double mean_smooth_curvature_persistence;
    int inconsistent_winding_edges;
    int graph_cleanup_skipped_by_cap;
    int circular_recovery_truncated;
    /** Noisy-input preprocessing, stable-scale, and graph-recovery diagnostics. */
    int feature_normal_filter_iterations_completed;
    int feature_normal_filter_changed_faces;
    int feature_normal_filter_preserved_edges;
    double mean_feature_normal_filter_angular_change_deg;
    double max_feature_normal_filter_angular_change_deg;
    double mean_feature_normal_filter_edge_indicator;
    double mean_smooth_curvature_scale_stability;
    int graph_consolidation_bridges;
    int graph_consolidation_skipped_by_cap;
    int junction_branch_pairs;
    int ambiguous_feature_junctions;
} ManuMeshSimplifyReport;

/** @brief Size-versioned geometric and topological mesh statistics. */
typedef struct ManuMeshMeshStats {
    /**
   * Output storage for mesh statistics. Current source calls to
   * manumesh_compute_mesh_stats pass sizeof(ManuMeshMeshStats) through the
   * size-aware inline wrapper, so this object need not be pre-initialized.
   * Call manumesh_mesh_stats_init when an initialized standalone value is
   * useful before the statistics call.
   */
    size_t struct_size;
    unsigned int abi_version;
    int vertices;
    int faces;
    int edges;
    int boundary_edges;
    int non_manifold_edges;
    double area;
    double mean_triangle_quality;
    double min_triangle_quality;
    double mean_edge_length;
    double edge_length_cv;
} ManuMeshMeshStats;

/** @return Static null-terminated semantic version string. */
MANUMESH_API const char* manumesh_version(void);
/**
 * @param[in] status Stable status value.
 * @return Static null-terminated English status name.
 */
MANUMESH_API const char* manumesh_status_message(ManuMeshStatus status);

/** @return New context, or NULL when allocation fails. */
MANUMESH_API ManuMeshContext* manumesh_context_create(void);
/** @param[in] context Context to destroy; NULL is accepted. */
MANUMESH_API void manumesh_context_destroy(ManuMeshContext* context);
/**
 * Returns the last error message recorded on this context, or an empty string.
 *
 * Lifetime: the returned pointer refers to storage owned by the context and is
 * only guaranteed valid until the next ManuMesh API call made with the same
 * context (any call may clear or replace the message) or until the context is
 * destroyed. Copy the string if it must outlive that window. Contexts are not
 * thread-safe; see the header comment.
 */
MANUMESH_API const char* manumesh_context_last_error(const ManuMeshContext* context);
/** @param[in,out] context Context whose error string is cleared; must be non-null. */
MANUMESH_API void manumesh_context_clear_error(ManuMeshContext* context);

/**
 * @param[in,out] context Context that records allocation errors.
 * @return New empty mesh handle, or NULL on invalid context/allocation failure.
 */
MANUMESH_API ManuMeshMeshHandle* manumesh_mesh_create(ManuMeshContext* context);
/** @param[in] mesh Mesh handle to destroy; NULL is accepted. */
MANUMESH_API void manumesh_mesh_destroy(ManuMeshMeshHandle* mesh);
/**
 * @param[in,out] context Error context.
 * @param[in,out] mesh Mesh whose geometry is cleared.
 * @retval MANUMESH_STATUS_OK Mesh is empty.
 * @retval MANUMESH_STATUS_INVALID_ARGUMENT A required pointer is NULL.
 */
MANUMESH_API ManuMeshStatus manumesh_mesh_clear(ManuMeshContext* context, ManuMeshMeshHandle* mesh);
/**
 * @brief Replaces a mesh with caller-owned position and triangle arrays.
 * @param[in,out] context Error context.
 * @param[in,out] mesh Destination mesh handle.
 * @param[in] vertices Array containing `vertex_count` positions.
 * @param[in] vertex_count Number of positions.
 * @param[in] faces Array containing `face_count` triangles.
 * @param[in] face_count Number of triangles.
 * @return Status; invalid indices and non-finite values are rejected.
 * @note Input arrays are copied and may be released after return.
 */
MANUMESH_API ManuMeshStatus manumesh_mesh_set_data(
    ManuMeshContext* context,
    ManuMeshMeshHandle* mesh,
    const ManuMeshVec3* vertices,
    size_t vertex_count,
    const ManuMeshFace* faces,
    size_t face_count
);
/**
 * Either vertex_count or face_count may be null to request just one count;
 * passing both as null is an invalid argument.
 * @param[in,out] context Error context.
 * @param[in] mesh Source mesh.
 * @param[out] vertex_count Optional vertex-count destination.
 * @param[out] face_count Optional face-count destination.
 * @return Status code.
 */
MANUMESH_API ManuMeshStatus manumesh_mesh_get_counts(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, size_t* vertex_count, size_t* face_count
);
/**
 * @brief Copies vertices into caller-owned storage.
 * @param[in,out] context Error context.
 * @param[in] mesh Source mesh.
 * @param[out] vertices Output array, or NULL only when capacity is zero.
 * @param[in] vertex_capacity Number of array elements available.
 * @param[out] vertices_written Required/written element count.
 * @retval MANUMESH_STATUS_BUFFER_TOO_SMALL Capacity is below the required count.
 */
MANUMESH_API ManuMeshStatus manumesh_mesh_copy_vertices(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshVec3* vertices,
    size_t vertex_capacity,
    size_t* vertices_written
);
/** Same contract as manumesh_mesh_copy_vertices(), for triangle indices. */
MANUMESH_API ManuMeshStatus manumesh_mesh_copy_faces(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshFace* faces,
    size_t face_capacity,
    size_t* faces_written
);

/**
 * @brief Loads STL or OBJ geometry selected by file extension.
 * @param[in,out] context Error context.
 * @param[in] path Null-terminated input path.
 * @param[in,out] mesh Destination mesh, replaced on success.
 * @param[in] merge_relative_epsilon Relative STL coincident-vertex merge tolerance.
 * @return I/O, validation, allocation, or success status.
 */
MANUMESH_API ManuMeshStatus
manumesh_load_mesh(ManuMeshContext* context, const char* path, ManuMeshMeshHandle* mesh, double merge_relative_epsilon);
/**
 * @brief Writes a strictly valid mesh as ASCII STL.
 * @param[in,out] context Error context.
 * @param[in] path Null-terminated destination path.
 * @param[in] mesh Source mesh.
 * @param[in] solid_name Optional STL solid label; NULL selects the default.
 * @return I/O, validation, allocation, or success status.
 */
MANUMESH_API ManuMeshStatus manumesh_save_ascii_stl(
    ManuMeshContext* context, const char* path, const ManuMeshMeshHandle* mesh, const char* solid_name
);
/**
 * @brief Writes a strictly valid mesh as standard little-endian binary STL.
 * @param[in,out] context Error context.
 * @param[in] path Null-terminated destination path.
 * @param[in] mesh Source mesh.
 * @return I/O, validation, allocation, or success status.
 */
MANUMESH_API ManuMeshStatus
manumesh_save_binary_stl(ManuMeshContext* context, const char* path, const ManuMeshMeshHandle* mesh);

/**
 * @brief Generates one built-in analytic/test mesh by stable name.
 * @param[in,out] context Error context.
 * @param[in] name Null-terminated generator name.
 * @param[in] n Resolution parameter interpreted by the selected generator.
 * @param[in,out] mesh Destination mesh, replaced on success.
 * @return Invalid-argument, allocation, or success status.
 */
MANUMESH_API ManuMeshStatus
manumesh_generate_mesh(ManuMeshContext* context, const char* name, int n, ManuMeshMeshHandle* mesh);

/**
 * Size-aware initializers. The library writes at most struct_capacity bytes,
 * records the initialized size in struct_size, and ignores unknown future
 * tail bytes when struct_capacity is larger than the library's current type.
 */
MANUMESH_API ManuMeshStatus
manumesh_simplify_options_init_with_size(ManuMeshSimplifyOptions* options, size_t struct_capacity);
/** @copydoc manumesh_simplify_options_init_with_size */
MANUMESH_API ManuMeshStatus
manumesh_simplify_report_init_with_size(ManuMeshSimplifyReport* report, size_t struct_capacity);
/** @copydoc manumesh_simplify_options_init_with_size */
MANUMESH_API ManuMeshStatus manumesh_mesh_stats_init_with_size(ManuMeshMeshStats* stats, size_t struct_capacity);

/**
 * Legacy ABI v1 symbols retained for already-built callers. They initialize
 * only the original v1 layout published with these symbols. New source code
 * includes the compatibility macros below and transparently calls the
 * size-aware entry points with the current public struct size.
 */
MANUMESH_API void manumesh_simplify_options_init(ManuMeshSimplifyOptions* options);
/** Initializes the original ABI-v1 prefix of a simplification report. */
MANUMESH_API void manumesh_simplify_report_init(ManuMeshSimplifyReport* report);
/** Initializes the original ABI-v1 prefix of a mesh-statistics structure. */
MANUMESH_API void manumesh_mesh_stats_init(ManuMeshMeshStats* stats);

/**
 * Capacity-aware output entry points. A non-null output buffer must include
 * the complete abi_version field. The library writes at most the supplied
 * capacity and at most its current structure size; larger unknown tails are
 * left untouched. report may be null because simplification diagnostics are
 * optional. stats is required by manumesh_compute_mesh_stats_with_size.
 */
/**
 * @brief Simplifies a mesh with capacity-bounded diagnostic output.
 * @param[in,out] context Error context.
 * @param[in] input Source mesh; may alias neither `output` nor destroyed storage.
 * @param[in] options Initialized, ABI-compatible options.
 * @param[in,out] output Destination mesh, replaced only on success.
 * @param[out] report Optional prefix-compatible report buffer.
 * @param[in] report_capacity Writable bytes at `report`; ignored when report is NULL.
 * @return Validation, algorithm, allocation, or success status.
 */
MANUMESH_API ManuMeshStatus manumesh_simplify_mesh_with_report_size(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* input,
    const ManuMeshSimplifyOptions* options,
    ManuMeshMeshHandle* output,
    ManuMeshSimplifyReport* report,
    size_t report_capacity
);
/**
 * @brief Computes mesh statistics into a capacity-bounded output structure.
 * @param[in,out] context Error context.
 * @param[in] mesh Source mesh.
 * @param[out] stats Required prefix-compatible output buffer.
 * @param[in] stats_capacity Writable bytes at `stats`.
 * @return Validation, capacity, allocation, or success status.
 */
MANUMESH_API ManuMeshStatus manumesh_compute_mesh_stats_with_size(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshMeshStats* stats, size_t stats_capacity
);

/**
 * Legacy ABI v1 output symbols retained for already-built callers. They never
 * inspect output memory and write only the first published v1 report/stats
 * layouts. Current source calls are redirected to the capacity-aware entries.
 */
/** @brief Legacy ABI-v1 simplification symbol; new source is macro-routed to the size-aware entry. */
MANUMESH_API ManuMeshStatus manumesh_simplify_mesh(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* input,
    const ManuMeshSimplifyOptions* options,
    ManuMeshMeshHandle* output,
    ManuMeshSimplifyReport* report
);
/** @brief Legacy ABI-v1 statistics symbol; new source is macro-routed to the size-aware entry. */
MANUMESH_API ManuMeshStatus
manumesh_compute_mesh_stats(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshMeshStats* stats);

#if !defined(MANUMESH_C_API_IMPLEMENTATION) && !defined(MANUMESH_DISABLE_SIZE_AWARE_ALIASES) &&                        \
    !defined(MANUMESH_DISABLE_SIZE_AWARE_INIT_MACROS)
#if defined(_MSC_VER) && !defined(__cplusplus)
#define MANUMESH_C_API_INLINE __inline
#else
#define MANUMESH_C_API_INLINE inline
#endif

static MANUMESH_C_API_INLINE void manumesh_detail_simplify_options_init_current(ManuMeshSimplifyOptions* options) {
    (void)manumesh_simplify_options_init_with_size(options, sizeof(ManuMeshSimplifyOptions));
}

static MANUMESH_C_API_INLINE void manumesh_detail_simplify_report_init_current(ManuMeshSimplifyReport* report) {
    (void)manumesh_simplify_report_init_with_size(report, sizeof(ManuMeshSimplifyReport));
}

static MANUMESH_C_API_INLINE void manumesh_detail_mesh_stats_init_current(ManuMeshMeshStats* stats) {
    (void)manumesh_mesh_stats_init_with_size(stats, sizeof(ManuMeshMeshStats));
}

static MANUMESH_C_API_INLINE ManuMeshStatus manumesh_detail_simplify_mesh_current(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* input,
    const ManuMeshSimplifyOptions* options,
    ManuMeshMeshHandle* output,
    ManuMeshSimplifyReport* report
) {
    return manumesh_simplify_mesh_with_report_size(
        context, input, options, output, report, sizeof(ManuMeshSimplifyReport)
    );
}

static MANUMESH_C_API_INLINE ManuMeshStatus manumesh_detail_compute_mesh_stats_current(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshMeshStats* stats
) {
    return manumesh_compute_mesh_stats_with_size(context, mesh, stats, sizeof(ManuMeshMeshStats));
}

#undef MANUMESH_C_API_INLINE

#define manumesh_simplify_options_init manumesh_detail_simplify_options_init_current
#define manumesh_simplify_report_init manumesh_detail_simplify_report_init_current
#define manumesh_mesh_stats_init manumesh_detail_mesh_stats_init_current
#define manumesh_simplify_mesh manumesh_detail_simplify_mesh_current
#define manumesh_compute_mesh_stats manumesh_detail_compute_mesh_stats_current
#endif

#ifdef __cplusplus
}
#endif

/** @} */
