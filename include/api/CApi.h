#pragma once

#include "Export.h"

#include <stddef.h>

/*
 * ManuMesh stable C ABI (v1).
 *
 * Threading: a ManuMeshContext is NOT thread-safe. Do not share a single
 * context across threads without external synchronization; give each thread its
 * own context instead.
 *
 * Texture coordinates: the v1 ABI does NOT carry per-corner texture
 * coordinates. manumesh_mesh_set_data accepts only vertex positions and face
 * indices, and manumesh_mesh_copy_* returns only positions and indices. UVs
 * that exist on a C++ manumesh::Mesh / PlainMesh (Mesh::faceTexCoords) are not
 * exposed here and are dropped when a mesh crosses this boundary. Use the C++
 * API directly when texture coordinates must be preserved.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ManuMeshContext ManuMeshContext;
typedef struct ManuMeshMeshHandle ManuMeshMeshHandle;

#define MANUMESH_ABI_VERSION 1u

typedef enum ManuMeshStatus {
    MANUMESH_STATUS_OK = 0,
    MANUMESH_STATUS_INVALID_ARGUMENT = 1,
    MANUMESH_STATUS_BUFFER_TOO_SMALL = 2,
    MANUMESH_STATUS_IO_ERROR = 3,
    MANUMESH_STATUS_ALGORITHM_ERROR = 4,
    MANUMESH_STATUS_OUT_OF_MEMORY = 5
} ManuMeshStatus;

typedef enum ManuMeshWeightMode {
    MANUMESH_WEIGHT_MODE_UNIFORM = 0,
    MANUMESH_WEIGHT_MODE_DIHEDRAL = 1,
    MANUMESH_WEIGHT_MODE_HEIGHT = 2,
    MANUMESH_WEIGHT_MODE_X_BAND = 3,
    MANUMESH_WEIGHT_MODE_NORMAL_TENSOR = 4
} ManuMeshWeightMode;

typedef enum ManuMeshSimplifyTerminationReason {
    MANUMESH_SIMPLIFY_TERMINATION_NOT_STARTED = 0,
    MANUMESH_SIMPLIFY_TERMINATION_REACHED_TARGET = 1,
    MANUMESH_SIMPLIFY_TERMINATION_ALREADY_AT_OR_BELOW_TARGET = 2,
    MANUMESH_SIMPLIFY_TERMINATION_NO_CANDIDATES = 3,
    MANUMESH_SIMPLIFY_TERMINATION_REJECTION_LIMIT = 4
} ManuMeshSimplifyTerminationReason;

typedef enum ManuMeshFeatureProtectionMode {
    /* Disable hard feature-curve protection. */
    MANUMESH_FEATURE_PROTECTION_NONE = 0,
    /* Hard-protect circular and near-circular loops only. */
    MANUMESH_FEATURE_PROTECTION_CIRCULAR_ONLY = 1,
    /* Hard-protect fitted primitive loops: circle, near-circle, and ellipse. */
    MANUMESH_FEATURE_PROTECTION_PRIMITIVE_CURVES = 2,
    /* Strict behavior: hard-protect every detected feature edge. */
    MANUMESH_FEATURE_PROTECTION_ALL_FEATURE_EDGES = 3
} ManuMeshFeatureProtectionMode;

typedef struct ManuMeshVec3 {
    double x;
    double y;
    double z;
} ManuMeshVec3;

typedef struct ManuMeshFace {
    int v[3];
} ManuMeshFace;

typedef struct ManuMeshSimplifyOptions {
    /*
   * Must be set by manumesh_simplify_options_init before calling
   * manumesh_simplify_mesh.
   * The library accepts older trailing struct sizes for
   * the same ABI version;
   * fields not present in struct_size keep the library
   * default value.
   */
    size_t struct_size;
    unsigned int abi_version;
    /* Target selection. target_faces > 0 overrides target_ratio. */
    int target_faces;
    double target_ratio;
    /* QEM and line-quadric candidate-ranking costs. */
    int use_line_quadrics;
    double line_weight;
    ManuMeshWeightMode weight_mode;
    double feature_boost;
    double feature_angle_deg;
    int adaptive_scale;
    double adaptive_base_line_weight;
    /* Boundary and feature policies. Hard filters run after QEM ranking. */
    double boundary_weight;
    int preserve_boundary;
    int preserve_feature_curves;
    double feature_curve_weight;
    double max_feature_curve_deviation_ratio;
    /* Feature detection thresholds used before simplification. */
    double circle_fit_relative_threshold;
    double ellipse_fit_relative_threshold;
    double near_circle_axis_ratio_tolerance;
    int min_feature_loop_vertices;
    int min_circular_feature_loop_vertices;
    /* Normal-tensor weak-feature evidence. */
    int use_normal_tensor_features;
    double normal_tensor_feature_threshold;
    double normal_tensor_min_edge_alignment;
    int normal_tensor_smoothing_iterations;
    int normal_tensor_scale_count;
    /* Hard legality filters. Zero local-error budgets disable those tests. */
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
    /* Optional fixed-topology quality-improvement passes after simplification. */
    int quality_refinement_iterations;
} ManuMeshSimplifyOptions;

typedef struct ManuMeshSimplifyReport {
    /*
   * Must be initialized by manumesh_simplify_report_init before calling
   * manumesh_simplify_mesh, unless the caller deliberately provides an older
   * trailing struct_size with the current MANUMESH_ABI_VERSION.
   */
    size_t struct_size;
    unsigned int abi_version;
    int initial_vertices;
    int initial_faces;
    int final_vertices;
    int final_faces;
    int collapsed_edges;
    int rejected_collapses;
    /* Current collapse candidates whose placement solve used fallback candidates. */
    int solver_fallbacks;
    int queue_rebuilds;
    /* Feature-detection summary from the input mesh. */
    int feature_loops;
    int circular_feature_loops;
    int feature_vertices;
    int normal_tensor_feature_edges;
    /* First hard filter that rejected each current collapse candidate. */
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
    /* Feature trace diagnostics. Added at the tail to preserve old field offsets. */
    int traced_feature_edges;
    int untraced_feature_edges;
    /* Normal-tensor scale diagnostics. Added at the tail for ABI compatibility. */
    int normal_tensor_scored_vertices;
    double max_normal_tensor_persistent_score;
    double mean_normal_tensor_local_scale;
    double mean_normal_tensor_persistence;
    /* Feature component confidence and cleanup diagnostics. */
    int feature_components;
    int weak_feature_components;
    int high_confidence_feature_components;
    int graph_cleanup_bridged_gaps;
    int graph_cleanup_removed_spurs;
    int graph_cleanup_merged_junctions;
    double mean_feature_component_confidence;
    double min_feature_component_confidence;
    /* Fixed-topology second-round quality-refinement diagnostics. */
    int quality_refinement_iterations_completed;
    int quality_refinement_attempted_moves;
    int quality_refinement_accepted_moves;
    /* Input faces tolerated as degenerate (zero area / repeated vertex).
   * Added at the tail to preserve old field offsets. */
    int degenerate_input_faces;
} ManuMeshSimplifyReport;

typedef struct ManuMeshMeshStats {
    /*
   * Must be initialized by manumesh_mesh_stats_init before calling
   * manumesh_compute_mesh_stats, unless the caller deliberately provides an
   * older trailing struct_size with the current MANUMESH_ABI_VERSION.
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

MANUMESH_API const char* manumesh_version(void);
MANUMESH_API const char* manumesh_status_message(ManuMeshStatus status);

MANUMESH_API ManuMeshContext* manumesh_context_create(void);
MANUMESH_API void manumesh_context_destroy(ManuMeshContext* context);
/*
 * Returns the last error message recorded on this context, or an empty string.
 *
 * Lifetime: the returned pointer refers to storage owned by the context and is
 * only guaranteed valid until the next ManuMesh API call made with the same
 * context (any call may clear or replace the message) or until the context is
 * destroyed. Copy the string if it must outlive that window. Contexts are not
 * thread-safe; see the header comment.
 */
MANUMESH_API const char* manumesh_context_last_error(const ManuMeshContext* context);
MANUMESH_API void manumesh_context_clear_error(ManuMeshContext* context);

MANUMESH_API ManuMeshMeshHandle* manumesh_mesh_create(ManuMeshContext* context);
MANUMESH_API void manumesh_mesh_destroy(ManuMeshMeshHandle* mesh);
MANUMESH_API ManuMeshStatus manumesh_mesh_clear(ManuMeshContext* context, ManuMeshMeshHandle* mesh);
MANUMESH_API ManuMeshStatus manumesh_mesh_set_data(
    ManuMeshContext* context,
    ManuMeshMeshHandle* mesh,
    const ManuMeshVec3* vertices,
    size_t vertex_count,
    const ManuMeshFace* faces,
    size_t face_count
);
/*
 * Either vertex_count or face_count may be null to request just one count;
 * passing both as null is an invalid argument.
 */
MANUMESH_API ManuMeshStatus manumesh_mesh_get_counts(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, size_t* vertex_count, size_t* face_count
);
MANUMESH_API ManuMeshStatus manumesh_mesh_copy_vertices(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshVec3* vertices,
    size_t vertex_capacity,
    size_t* vertices_written
);
MANUMESH_API ManuMeshStatus manumesh_mesh_copy_faces(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshFace* faces,
    size_t face_capacity,
    size_t* faces_written
);

MANUMESH_API ManuMeshStatus
manumesh_load_mesh(ManuMeshContext* context, const char* path, ManuMeshMeshHandle* mesh, double merge_relative_epsilon);
MANUMESH_API ManuMeshStatus manumesh_save_ascii_stl(
    ManuMeshContext* context, const char* path, const ManuMeshMeshHandle* mesh, const char* solid_name
);
MANUMESH_API ManuMeshStatus
manumesh_generate_mesh(ManuMeshContext* context, const char* name, int n, ManuMeshMeshHandle* mesh);

MANUMESH_API void manumesh_simplify_options_init(ManuMeshSimplifyOptions* options);
MANUMESH_API void manumesh_simplify_report_init(ManuMeshSimplifyReport* report);
MANUMESH_API void manumesh_mesh_stats_init(ManuMeshMeshStats* stats);
MANUMESH_API ManuMeshStatus manumesh_simplify_mesh(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* input,
    const ManuMeshSimplifyOptions* options,
    ManuMeshMeshHandle* output,
    ManuMeshSimplifyReport* report
);
MANUMESH_API ManuMeshStatus
manumesh_compute_mesh_stats(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshMeshStats* stats);

#ifdef __cplusplus
}
#endif
