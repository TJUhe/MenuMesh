#pragma once

#include "line_quadrics_qem/Export.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LqContext LqContext;
typedef struct LqMeshHandle LqMeshHandle;

typedef enum LqStatus {
  LQ_STATUS_OK = 0,
  LQ_STATUS_INVALID_ARGUMENT = 1,
  LQ_STATUS_BUFFER_TOO_SMALL = 2,
  LQ_STATUS_IO_ERROR = 3,
  LQ_STATUS_ALGORITHM_ERROR = 4,
  LQ_STATUS_OUT_OF_MEMORY = 5
} LqStatus;

typedef enum LqWeightMode {
  LQ_WEIGHT_MODE_UNIFORM = 0,
  LQ_WEIGHT_MODE_DIHEDRAL = 1,
  LQ_WEIGHT_MODE_HEIGHT = 2,
  LQ_WEIGHT_MODE_X_BAND = 3,
  LQ_WEIGHT_MODE_NORMAL_TENSOR = 4
} LqWeightMode;

typedef enum LqSimplifyTerminationReason {
  LQ_SIMPLIFY_TERMINATION_NOT_STARTED = 0,
  LQ_SIMPLIFY_TERMINATION_REACHED_TARGET = 1,
  LQ_SIMPLIFY_TERMINATION_ALREADY_AT_OR_BELOW_TARGET = 2,
  LQ_SIMPLIFY_TERMINATION_NO_CANDIDATES = 3,
  LQ_SIMPLIFY_TERMINATION_REJECTION_LIMIT = 4
} LqSimplifyTerminationReason;

typedef enum LqFeatureProtectionMode {
  LQ_FEATURE_PROTECTION_NONE = 0,
  LQ_FEATURE_PROTECTION_CIRCULAR_ONLY = 1,
  LQ_FEATURE_PROTECTION_PRIMITIVE_CURVES = 2,
  LQ_FEATURE_PROTECTION_ALL_FEATURE_EDGES = 3
} LqFeatureProtectionMode;

typedef struct LqVec3 {
  double x;
  double y;
  double z;
} LqVec3;

typedef struct LqFace {
  int v[3];
} LqFace;

typedef struct LqSimplifyOptions {
  int target_faces;
  double target_ratio;
  int use_line_quadrics;
  double line_weight;
  LqWeightMode weight_mode;
  double feature_boost;
  double feature_angle_deg;
  int adaptive_scale;
  double adaptive_base_line_weight;
  double boundary_weight;
  int preserve_boundary;
  int preserve_feature_curves;
  int protect_all_feature_edges;
  double feature_curve_weight;
  double max_feature_curve_deviation_ratio;
  double circle_fit_relative_threshold;
  double ellipse_fit_relative_threshold;
  double near_circle_axis_ratio_tolerance;
  int min_feature_loop_vertices;
  int min_circular_feature_loop_vertices;
  int use_normal_tensor_features;
  double normal_tensor_feature_threshold;
  double normal_tensor_min_edge_alignment;
  int normal_tensor_smoothing_iterations;
  int normal_tensor_scale_count;
  double min_triangle_quality;
  double max_normal_deviation_deg;
  double max_local_error;
  double max_local_error_ratio;
  int prevent_local_intersections;
  int verbose;
  LqFeatureProtectionMode feature_protection_mode;
} LqSimplifyOptions;

typedef struct LqSimplifyReport {
  int initial_vertices;
  int initial_faces;
  int final_vertices;
  int final_faces;
  int collapsed_edges;
  int rejected_collapses;
  /* Current collapse candidates whose placement solve used fallback candidates. */
  int solver_fallbacks;
  int queue_rebuilds;
  int feature_loops;
  int circular_feature_loops;
  int feature_vertices;
  int normal_tensor_feature_edges;
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
  LqSimplifyTerminationReason termination_reason;
  double min_applied_line_weight;
  double max_applied_line_weight;
} LqSimplifyReport;

typedef struct LqMeshStats {
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
} LqMeshStats;

LQ_API const char* lq_version(void);
LQ_API const char* lq_status_message(LqStatus status);

LQ_API LqContext* lq_context_create(void);
LQ_API void lq_context_destroy(LqContext* context);
LQ_API const char* lq_context_last_error(const LqContext* context);
LQ_API void lq_context_clear_error(LqContext* context);

LQ_API LqMeshHandle* lq_mesh_create(LqContext* context);
LQ_API void lq_mesh_destroy(LqMeshHandle* mesh);
LQ_API LqStatus lq_mesh_clear(LqContext* context, LqMeshHandle* mesh);
LQ_API LqStatus lq_mesh_set_data(LqContext* context, LqMeshHandle* mesh,
                                 const LqVec3* vertices, size_t vertex_count,
                                 const LqFace* faces, size_t face_count);
LQ_API LqStatus lq_mesh_get_counts(LqContext* context, const LqMeshHandle* mesh,
                                   size_t* vertex_count, size_t* face_count);
LQ_API LqStatus lq_mesh_copy_vertices(LqContext* context, const LqMeshHandle* mesh,
                                      LqVec3* vertices, size_t vertex_capacity,
                                      size_t* vertices_written);
LQ_API LqStatus lq_mesh_copy_faces(LqContext* context, const LqMeshHandle* mesh,
                                   LqFace* faces, size_t face_capacity,
                                   size_t* faces_written);

LQ_API LqStatus lq_load_mesh(LqContext* context, const char* path, LqMeshHandle* mesh,
                             double merge_relative_epsilon);
LQ_API LqStatus lq_save_ascii_stl(LqContext* context, const char* path,
                                  const LqMeshHandle* mesh, const char* solid_name);
LQ_API LqStatus lq_generate_mesh(LqContext* context, const char* name, int n,
                                 LqMeshHandle* mesh);

LQ_API void lq_simplify_options_init(LqSimplifyOptions* options);
LQ_API LqStatus lq_simplify_mesh(LqContext* context, const LqMeshHandle* input,
                                 const LqSimplifyOptions* options, LqMeshHandle* output,
                                 LqSimplifyReport* report);
LQ_API LqStatus lq_compute_mesh_stats(LqContext* context, const LqMeshHandle* mesh,
                                      LqMeshStats* stats);

#ifdef __cplusplus
}
#endif
