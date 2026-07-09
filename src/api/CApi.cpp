#include "api/CApi.h"

#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

#ifndef MANUMESH_VERSION
#define MANUMESH_VERSION "0.0.0"
#endif

struct ManuMeshContext {
  std::string lastError;
};

struct ManuMeshMeshHandle {
  manumesh::Mesh mesh;
};

namespace {

void clearError(ManuMeshContext* context) {
  if (context) {
    context->lastError.clear();
  }
}

ManuMeshStatus fail(ManuMeshContext* context, ManuMeshStatus status,
                    const std::string& message) {
  if (context) {
    context->lastError = message;
  }
  return status;
}

ManuMeshStatus translateException(ManuMeshContext* context, const std::exception& ex) {
  if (dynamic_cast<const std::bad_alloc*>(&ex) != nullptr) {
    return fail(context, MANUMESH_STATUS_OUT_OF_MEMORY, ex.what());
  }
  if (dynamic_cast<const std::invalid_argument*>(&ex) != nullptr) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, ex.what());
  }
  return fail(context, MANUMESH_STATUS_ALGORITHM_ERROR, ex.what());
}

ManuMeshStatus translateUnknownException(ManuMeshContext* context) {
  return fail(context, MANUMESH_STATUS_ALGORITHM_ERROR, "Unknown C++ exception.");
}

bool boolFromInt(int value) {
  return value != 0;
}

template <typename T> void initializeAbiStruct(T& value) {
  value = T{};
  value.struct_size = sizeof(T);
  value.abi_version = MANUMESH_ABI_VERSION;
}

template <typename T> bool abiStructLooksInitialized(const T& value) {
  constexpr std::size_t kMinimumInitializedSize =
      offsetof(T, abi_version) + sizeof(value.abi_version);
  return value.struct_size >= kMinimumInitializedSize &&
         value.abi_version == MANUMESH_ABI_VERSION;
}

bool abiFieldPresent(std::size_t structSize, std::size_t fieldOffset,
                     std::size_t fieldSize) {
  return fieldOffset <= std::numeric_limits<std::size_t>::max() - fieldSize &&
         structSize >= fieldOffset + fieldSize;
}

template <typename T> std::size_t outputWriteSizeOrCurrent(const T& value) {
  std::size_t structSize = 0;
  unsigned int abiVersion = 0;
  std::memcpy(&structSize,
              reinterpret_cast<const unsigned char*>(&value) + offsetof(T, struct_size),
              sizeof(structSize));
  std::memcpy(&abiVersion,
              reinterpret_cast<const unsigned char*>(&value) + offsetof(T, abi_version),
              sizeof(abiVersion));

  constexpr std::size_t kMinimumInitializedSize =
      offsetof(T, abi_version) + sizeof(value.abi_version);
  if (structSize >= kMinimumInitializedSize && structSize <= sizeof(T) &&
      abiVersion == MANUMESH_ABI_VERSION) {
    return structSize;
  }
  return sizeof(T);
}

template <typename T> void initializeOutputAbiStruct(T& value, std::size_t writeSize) {
  std::memset(&value, 0, writeSize);
  if (abiFieldPresent(writeSize, offsetof(T, struct_size), sizeof(value.struct_size))) {
    value.struct_size = writeSize;
  }
  if (abiFieldPresent(writeSize, offsetof(T, abi_version), sizeof(value.abi_version))) {
    value.abi_version = MANUMESH_ABI_VERSION;
  }
}

#define MANUMESH_SIMPLIFY_FIELD_PRESENT(options, field)                                \
  abiFieldPresent((options).struct_size, offsetof(ManuMeshSimplifyOptions, field),     \
                  sizeof((options).field))

#define MANUMESH_REPORT_FIELD_PRESENT(size, field)                                     \
  abiFieldPresent((size), offsetof(ManuMeshSimplifyReport, field),                     \
                  sizeof(ManuMeshSimplifyReport{}.field))

#define MANUMESH_STATS_FIELD_PRESENT(size, field)                                      \
  abiFieldPresent((size), offsetof(ManuMeshMeshStats, field),                          \
                  sizeof(ManuMeshMeshStats{}.field))

#define MANUMESH_SET_REPORT_FIELD(target, size, field, value)                          \
  do {                                                                                 \
    if (MANUMESH_REPORT_FIELD_PRESENT((size), field)) {                                \
      (target).field = (value);                                                        \
    }                                                                                  \
  } while (false)

#define MANUMESH_SET_STATS_FIELD(target, size, field, value)                           \
  do {                                                                                 \
    if (MANUMESH_STATS_FIELD_PRESENT((size), field)) {                                 \
      (target).field = (value);                                                        \
    }                                                                                  \
  } while (false)

bool convertWeightMode(ManuMeshWeightMode input,
                       manumesh::simplification::WeightMode& output) {
  switch (input) {
  case MANUMESH_WEIGHT_MODE_UNIFORM:
    output = manumesh::simplification::WeightMode::Uniform;
    return true;
  case MANUMESH_WEIGHT_MODE_DIHEDRAL:
    output = manumesh::simplification::WeightMode::Dihedral;
    return true;
  case MANUMESH_WEIGHT_MODE_HEIGHT:
    output = manumesh::simplification::WeightMode::Height;
    return true;
  case MANUMESH_WEIGHT_MODE_X_BAND:
    output = manumesh::simplification::WeightMode::XBand;
    return true;
  case MANUMESH_WEIGHT_MODE_NORMAL_TENSOR:
    output = manumesh::simplification::WeightMode::NormalTensor;
    return true;
  }
  return false;
}

bool convertFeatureProtectionMode(
    ManuMeshFeatureProtectionMode input,
    manumesh::simplification::FeatureProtectionMode& output) {
  switch (input) {
  case MANUMESH_FEATURE_PROTECTION_NONE:
    output = manumesh::simplification::FeatureProtectionMode::None;
    return true;
  case MANUMESH_FEATURE_PROTECTION_CIRCULAR_ONLY:
    output = manumesh::simplification::FeatureProtectionMode::CircularOnly;
    return true;
  case MANUMESH_FEATURE_PROTECTION_PRIMITIVE_CURVES:
    output = manumesh::simplification::FeatureProtectionMode::PrimitiveCurves;
    return true;
  case MANUMESH_FEATURE_PROTECTION_ALL_FEATURE_EDGES:
    output = manumesh::simplification::FeatureProtectionMode::AllFeatureEdges;
    return true;
  }
  return false;
}

ManuMeshSimplifyTerminationReason
convertTerminationReason(manumesh::simplification::SimplifyTerminationReason input) {
  switch (input) {
  case manumesh::simplification::SimplifyTerminationReason::NotStarted:
    return MANUMESH_SIMPLIFY_TERMINATION_NOT_STARTED;
  case manumesh::simplification::SimplifyTerminationReason::ReachedTarget:
    return MANUMESH_SIMPLIFY_TERMINATION_REACHED_TARGET;
  case manumesh::simplification::SimplifyTerminationReason::AlreadyAtOrBelowTarget:
    return MANUMESH_SIMPLIFY_TERMINATION_ALREADY_AT_OR_BELOW_TARGET;
  case manumesh::simplification::SimplifyTerminationReason::NoCandidates:
    return MANUMESH_SIMPLIFY_TERMINATION_NO_CANDIDATES;
  case manumesh::simplification::SimplifyTerminationReason::RejectionLimit:
    return MANUMESH_SIMPLIFY_TERMINATION_REJECTION_LIMIT;
  }
  return MANUMESH_SIMPLIFY_TERMINATION_NOT_STARTED;
}

void fillReport(const manumesh::simplification::SimplifyReport& source,
                ManuMeshSimplifyReport& target) {
  const std::size_t writeSize = outputWriteSizeOrCurrent(target);
  initializeOutputAbiStruct(target, writeSize);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, initial_vertices,
                            source.initialVertices);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, initial_faces, source.initialFaces);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, final_vertices, source.finalVertices);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, final_faces, source.finalFaces);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, collapsed_edges, source.collapsedEdges);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, rejected_collapses,
                            source.rejectedCollapses);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, solver_fallbacks,
                            source.solverFallbacks);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, queue_rebuilds, source.queueRebuilds);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, feature_loops, source.featureLoops);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, circular_feature_loops,
                            source.circularFeatureLoops);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, feature_vertices,
                            source.featureVertices);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, normal_tensor_feature_edges,
                            source.normalTensorFeatureEdges);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, feature_rejected_collapses,
                            source.featureRejectedCollapses);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, primitive_feature_rejected_collapses,
                            source.primitiveFeatureRejectedCollapses);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, generic_feature_rejected_collapses,
                            source.genericFeatureRejectedCollapses);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, boundary_rejected_collapses,
                            source.boundaryRejectedCollapses);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, topology_rejected_collapses,
                            source.topologyRejectedCollapses);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, normal_flip_rejected_collapses,
                            source.normalFlipRejectedCollapses);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, quality_rejected_collapses,
                            source.qualityRejectedCollapses);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, self_intersection_rejected_collapses,
                            source.selfIntersectionRejectedCollapses);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, curve_budget_rejected_collapses,
                            source.curveBudgetRejectedCollapses);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, error_rejected_collapses,
                            source.errorRejectedCollapses);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, projected_feature_placements,
                            source.projectedFeaturePlacements);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, termination_reason,
                            convertTerminationReason(source.terminationReason));
  MANUMESH_SET_REPORT_FIELD(target, writeSize, min_applied_line_weight,
                            source.minAppliedLineWeight);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, max_applied_line_weight,
                            source.maxAppliedLineWeight);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, traced_feature_edges,
                            source.tracedFeatureEdges);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, untraced_feature_edges,
                            source.untracedFeatureEdges);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, normal_tensor_scored_vertices,
                            source.normalTensorScoredVertices);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, max_normal_tensor_persistent_score,
                            source.maxNormalTensorPersistentScore);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, mean_normal_tensor_local_scale,
                            source.meanNormalTensorLocalScale);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, mean_normal_tensor_persistence,
                            source.meanNormalTensorPersistence);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, feature_components,
                            source.featureComponents);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, weak_feature_components,
                            source.weakFeatureComponents);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, high_confidence_feature_components,
                            source.highConfidenceFeatureComponents);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, graph_cleanup_bridged_gaps,
                            source.graphCleanupBridgedGaps);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, graph_cleanup_removed_spurs,
                            source.graphCleanupRemovedSpurs);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, graph_cleanup_merged_junctions,
                            source.graphCleanupMergedJunctions);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, mean_feature_component_confidence,
                            source.meanFeatureComponentConfidence);
  MANUMESH_SET_REPORT_FIELD(target, writeSize, min_feature_component_confidence,
                            source.minFeatureComponentConfidence);
}

void fillStats(const manumesh::simplification::MeshStats& source,
               ManuMeshMeshStats& target) {
  const std::size_t writeSize = outputWriteSizeOrCurrent(target);
  initializeOutputAbiStruct(target, writeSize);
  MANUMESH_SET_STATS_FIELD(target, writeSize, vertices, source.vertices);
  MANUMESH_SET_STATS_FIELD(target, writeSize, faces, source.faces);
  MANUMESH_SET_STATS_FIELD(target, writeSize, edges, source.edges);
  MANUMESH_SET_STATS_FIELD(target, writeSize, boundary_edges, source.boundaryEdges);
  MANUMESH_SET_STATS_FIELD(target, writeSize, non_manifold_edges,
                           source.nonManifoldEdges);
  MANUMESH_SET_STATS_FIELD(target, writeSize, area, source.area);
  MANUMESH_SET_STATS_FIELD(target, writeSize, mean_triangle_quality,
                           source.meanTriangleQuality);
  MANUMESH_SET_STATS_FIELD(target, writeSize, min_triangle_quality,
                           source.minTriangleQuality);
  MANUMESH_SET_STATS_FIELD(target, writeSize, mean_edge_length, source.meanEdgeLength);
  MANUMESH_SET_STATS_FIELD(target, writeSize, edge_length_cv, source.edgeLengthCv);
}

} // namespace

extern "C" {

const char* manumesh_version(void) {
  return MANUMESH_VERSION;
}

const char* manumesh_status_message(ManuMeshStatus status) {
  switch (status) {
  case MANUMESH_STATUS_OK:
    return "ok";
  case MANUMESH_STATUS_INVALID_ARGUMENT:
    return "invalid argument";
  case MANUMESH_STATUS_BUFFER_TOO_SMALL:
    return "buffer too small";
  case MANUMESH_STATUS_IO_ERROR:
    return "I/O error";
  case MANUMESH_STATUS_ALGORITHM_ERROR:
    return "algorithm error";
  case MANUMESH_STATUS_OUT_OF_MEMORY:
    return "out of memory";
  }
  return "unknown status";
}

ManuMeshContext* manumesh_context_create(void) {
  return new (std::nothrow) ManuMeshContext();
}

void manumesh_context_destroy(ManuMeshContext* context) {
  delete context;
}

const char* manumesh_context_last_error(const ManuMeshContext* context) {
  return context ? context->lastError.c_str() : "ManuMeshContext is null.";
}

void manumesh_context_clear_error(ManuMeshContext* context) {
  clearError(context);
}

ManuMeshMeshHandle* manumesh_mesh_create(ManuMeshContext* context) {
  clearError(context);
  ManuMeshMeshHandle* mesh = new (std::nothrow) ManuMeshMeshHandle();
  if (!mesh) {
    fail(context, MANUMESH_STATUS_OUT_OF_MEMORY, "Failed to allocate mesh handle.");
  }
  return mesh;
}

void manumesh_mesh_destroy(ManuMeshMeshHandle* mesh) {
  delete mesh;
}

ManuMeshStatus manumesh_mesh_clear(ManuMeshContext* context, ManuMeshMeshHandle* mesh) {
  clearError(context);
  if (!mesh) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
  }
  mesh->mesh.vertices.clear();
  mesh->mesh.faces.clear();
  return MANUMESH_STATUS_OK;
}

ManuMeshStatus manumesh_mesh_set_data(ManuMeshContext* context,
                                      ManuMeshMeshHandle* mesh,
                                      const ManuMeshVec3* vertices, size_t vertex_count,
                                      const ManuMeshFace* faces, size_t face_count) {
  clearError(context);
  if (!mesh) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
  }
  if ((vertex_count > 0 && !vertices) || (face_count > 0 && !faces)) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                "Vertex and face pointers must be valid when counts are non-zero.");
  }
  if (vertex_count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                "Vertex count exceeds the supported int-index range.");
  }
  if (face_count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                "Face count exceeds the supported int-index range.");
  }

  try {
    manumesh::Mesh next;
    next.vertices.reserve(vertex_count);
    next.faces.reserve(face_count);
    for (size_t i = 0; i < vertex_count; ++i) {
      next.vertices.emplace_back(vertices[i].x, vertices[i].y, vertices[i].z);
    }
    for (size_t i = 0; i < face_count; ++i) {
      manumesh::Face face;
      face.v = {faces[i].v[0], faces[i].v[1], faces[i].v[2]};
      next.faces.push_back(face);
    }
    std::string error;
    if (!manumesh::validateMeshGeometry(next, &error)) {
      return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                  error.empty() ? "Mesh geometry is invalid." : error);
    }
    mesh->mesh = std::move(next);
    return MANUMESH_STATUS_OK;
  } catch (const std::exception& ex) {
    return translateException(context, ex);
  } catch (...) {
    return translateUnknownException(context);
  }
}

ManuMeshStatus manumesh_mesh_get_counts(ManuMeshContext* context,
                                        const ManuMeshMeshHandle* mesh,
                                        size_t* vertex_count, size_t* face_count) {
  clearError(context);
  if (!mesh || !vertex_count || !face_count) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                "Mesh and output count pointers must be valid.");
  }
  *vertex_count = mesh->mesh.vertices.size();
  *face_count = mesh->mesh.faces.size();
  return MANUMESH_STATUS_OK;
}

ManuMeshStatus manumesh_mesh_copy_vertices(ManuMeshContext* context,
                                           const ManuMeshMeshHandle* mesh,
                                           ManuMeshVec3* vertices,
                                           size_t vertex_capacity,
                                           size_t* vertices_written) {
  clearError(context);
  if (!mesh || !vertices_written) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                "Mesh and vertices_written pointers must be valid.");
  }
  const size_t required = mesh->mesh.vertices.size();
  *vertices_written = required;
  if (vertex_capacity < required) {
    return fail(context, MANUMESH_STATUS_BUFFER_TOO_SMALL,
                "Vertex buffer is smaller than the mesh vertex count.");
  }
  if (required > 0 && !vertices) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Vertex buffer is null.");
  }
  for (size_t i = 0; i < required; ++i) {
    vertices[i].x = mesh->mesh.vertices[i].x();
    vertices[i].y = mesh->mesh.vertices[i].y();
    vertices[i].z = mesh->mesh.vertices[i].z();
  }
  return MANUMESH_STATUS_OK;
}

ManuMeshStatus manumesh_mesh_copy_faces(ManuMeshContext* context,
                                        const ManuMeshMeshHandle* mesh,
                                        ManuMeshFace* faces, size_t face_capacity,
                                        size_t* faces_written) {
  clearError(context);
  if (!mesh || !faces_written) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                "Mesh and faces_written pointers must be valid.");
  }
  const size_t required = mesh->mesh.faces.size();
  *faces_written = required;
  if (face_capacity < required) {
    return fail(context, MANUMESH_STATUS_BUFFER_TOO_SMALL,
                "Face buffer is smaller than the mesh face count.");
  }
  if (required > 0 && !faces) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Face buffer is null.");
  }
  for (size_t i = 0; i < required; ++i) {
    faces[i].v[0] = mesh->mesh.faces[i].v[0];
    faces[i].v[1] = mesh->mesh.faces[i].v[1];
    faces[i].v[2] = mesh->mesh.faces[i].v[2];
  }
  return MANUMESH_STATUS_OK;
}

ManuMeshStatus manumesh_load_mesh(ManuMeshContext* context, const char* path,
                                  ManuMeshMeshHandle* mesh,
                                  double merge_relative_epsilon) {
  clearError(context);
  if (!path || !mesh) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                "Path and mesh handle must be valid.");
  }
  if (!std::isfinite(merge_relative_epsilon) || merge_relative_epsilon < 0.0) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                "merge_relative_epsilon must be finite and non-negative.");
  }
  std::string error;
  try {
    manumesh::Mesh loaded;
    if (!manumesh::loadMesh(path, loaded, &error, merge_relative_epsilon)) {
      return fail(context, MANUMESH_STATUS_IO_ERROR, error);
    }
    mesh->mesh = std::move(loaded);
    return MANUMESH_STATUS_OK;
  } catch (const std::exception& ex) {
    return translateException(context, ex);
  } catch (...) {
    return translateUnknownException(context);
  }
}

ManuMeshStatus manumesh_save_ascii_stl(ManuMeshContext* context, const char* path,
                                       const ManuMeshMeshHandle* mesh,
                                       const char* solid_name) {
  clearError(context);
  if (!path || !mesh) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                "Path and mesh handle must be valid.");
  }
  std::string error;
  try {
    const char* name = solid_name ? solid_name : "mesh";
    if (!manumesh::saveAsciiStl(path, mesh->mesh, name, &error)) {
      return fail(context, MANUMESH_STATUS_IO_ERROR, error);
    }
    return MANUMESH_STATUS_OK;
  } catch (const std::exception& ex) {
    return translateException(context, ex);
  } catch (...) {
    return translateUnknownException(context);
  }
}

ManuMeshStatus manumesh_generate_mesh(ManuMeshContext* context, const char* name, int n,
                                      ManuMeshMeshHandle* mesh) {
  clearError(context);
  if (!name || !mesh) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                "Generator name and mesh handle must be valid.");
  }
  std::string error;
  try {
    manumesh::Mesh generated;
    if (!manumesh::generateMeshByName(name, n, generated, &error)) {
      return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, error);
    }
    mesh->mesh = std::move(generated);
    return MANUMESH_STATUS_OK;
  } catch (const std::exception& ex) {
    return translateException(context, ex);
  } catch (...) {
    return translateUnknownException(context);
  }
}

void manumesh_simplify_options_init(ManuMeshSimplifyOptions* options) {
  if (!options) {
    return;
  }
  initializeAbiStruct(*options);
  options->target_faces = -1;
  options->target_ratio = 0.25;
  options->use_line_quadrics = 1;
  options->line_weight = 1e-3;
  options->weight_mode = MANUMESH_WEIGHT_MODE_UNIFORM;
  options->feature_boost = 0.05;
  options->feature_angle_deg = 40.0;
  options->loop_trace_angle_deg = -1.0;
  options->adaptive_scale = 0;
  options->adaptive_base_line_weight = 1e-2;
  options->boundary_weight = 0.0;
  options->preserve_boundary = 0;
  options->preserve_feature_curves = 0;
  options->feature_curve_weight = 0.05;
  options->max_feature_curve_deviation_ratio = 0.0;
  options->circle_fit_relative_threshold = 0.05;
  options->ellipse_fit_relative_threshold = 0.05;
  options->near_circle_axis_ratio_tolerance = 0.08;
  options->min_feature_loop_vertices = 16;
  options->min_circular_feature_loop_vertices = 6;
  options->use_normal_tensor_features = 1;
  options->normal_tensor_feature_threshold = 0.16;
  options->normal_tensor_min_edge_alignment = 0.45;
  options->normal_tensor_smoothing_iterations = 0;
  options->normal_tensor_scale_count = 1;
  options->min_triangle_quality = 0.0;
  options->max_normal_deviation_deg = 90.0;
  options->max_local_error = 0.0;
  options->max_local_error_ratio = 0.0;
  options->prevent_local_intersections = 0;
  options->verbose = 0;
  options->feature_protection_mode = MANUMESH_FEATURE_PROTECTION_PRIMITIVE_CURVES;
  options->normal_tensor_min_persistent_scales = 1;
  options->cleanup_feature_graph = 1;
  options->feature_graph_gap_length_ratio = 1.25;
  options->feature_graph_max_weak_spur_edges = 2;
  options->feature_component_min_confidence = 0.35;
}

void manumesh_simplify_report_init(ManuMeshSimplifyReport* report) {
  if (!report) {
    return;
  }
  initializeAbiStruct(*report);
}

void manumesh_mesh_stats_init(ManuMeshMeshStats* stats) {
  if (!stats) {
    return;
  }
  initializeAbiStruct(*stats);
}

ManuMeshStatus manumesh_simplify_mesh(ManuMeshContext* context,
                                      const ManuMeshMeshHandle* input,
                                      const ManuMeshSimplifyOptions* options,
                                      ManuMeshMeshHandle* output,
                                      ManuMeshSimplifyReport* report) {
  clearError(context);
  if (!input || !output) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                "Input and output mesh handles must be valid.");
  }
  try {
    manumesh::simplification::SimplifyOptions cppOptions;
    if (options) {
      if (!abiStructLooksInitialized(*options)) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                    "ManuMeshSimplifyOptions must be initialized with "
                    "manumesh_simplify_options_init for this ABI version.");
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, target_faces)) {
        cppOptions.targetFaces = options->target_faces;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, target_ratio)) {
        cppOptions.targetRatio = options->target_ratio;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, use_line_quadrics)) {
        cppOptions.useLineQuadrics = boolFromInt(options->use_line_quadrics);
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, line_weight)) {
        cppOptions.lineWeight = options->line_weight;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, weight_mode) &&
          !convertWeightMode(options->weight_mode, cppOptions.weightMode)) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                    "Unknown simplification weight mode.");
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, feature_boost)) {
        cppOptions.featureBoost = options->feature_boost;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, feature_angle_deg)) {
        cppOptions.featureAngleDeg = options->feature_angle_deg;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, loop_trace_angle_deg)) {
        cppOptions.loopTraceAngleDeg = options->loop_trace_angle_deg;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, adaptive_scale)) {
        cppOptions.adaptiveScale = boolFromInt(options->adaptive_scale);
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, adaptive_base_line_weight)) {
        cppOptions.adaptiveBaseLineWeight = options->adaptive_base_line_weight;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, boundary_weight)) {
        cppOptions.boundaryWeight = options->boundary_weight;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, preserve_boundary)) {
        cppOptions.preserveBoundary = boolFromInt(options->preserve_boundary);
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, preserve_feature_curves)) {
        cppOptions.preserveFeatureCurves =
            boolFromInt(options->preserve_feature_curves);
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, feature_protection_mode) &&
          !convertFeatureProtectionMode(options->feature_protection_mode,
                                        cppOptions.featureProtectionMode)) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                    "Unknown feature protection mode.");
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, feature_curve_weight)) {
        cppOptions.featureCurveWeight = options->feature_curve_weight;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options,
                                          max_feature_curve_deviation_ratio)) {
        cppOptions.maxFeatureCurveDeviationRatio =
            options->max_feature_curve_deviation_ratio;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, circle_fit_relative_threshold)) {
        cppOptions.circleFitRelativeThreshold = options->circle_fit_relative_threshold;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, ellipse_fit_relative_threshold)) {
        cppOptions.ellipseFitRelativeThreshold =
            options->ellipse_fit_relative_threshold;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, near_circle_axis_ratio_tolerance)) {
        cppOptions.nearCircleAxisRatioTolerance =
            options->near_circle_axis_ratio_tolerance;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, min_feature_loop_vertices)) {
        cppOptions.minFeatureLoopVertices = options->min_feature_loop_vertices;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options,
                                          min_circular_feature_loop_vertices)) {
        cppOptions.minCircularFeatureLoopVertices =
            options->min_circular_feature_loop_vertices;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, use_normal_tensor_features)) {
        cppOptions.useNormalTensorFeatures =
            boolFromInt(options->use_normal_tensor_features);
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, normal_tensor_feature_threshold)) {
        cppOptions.normalTensorFeatureThreshold =
            options->normal_tensor_feature_threshold;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, normal_tensor_min_edge_alignment)) {
        cppOptions.normalTensorMinEdgeAlignment =
            options->normal_tensor_min_edge_alignment;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options,
                                          normal_tensor_smoothing_iterations)) {
        cppOptions.normalTensorSmoothingIterations =
            options->normal_tensor_smoothing_iterations;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, normal_tensor_scale_count)) {
        cppOptions.normalTensorScaleCount = options->normal_tensor_scale_count;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options,
                                          normal_tensor_min_persistent_scales)) {
        cppOptions.normalTensorMinPersistentScales =
            options->normal_tensor_min_persistent_scales;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, cleanup_feature_graph)) {
        cppOptions.cleanupFeatureGraph = boolFromInt(options->cleanup_feature_graph);
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, feature_graph_gap_length_ratio)) {
        cppOptions.featureGraphGapLengthRatio = options->feature_graph_gap_length_ratio;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options,
                                          feature_graph_max_weak_spur_edges)) {
        cppOptions.featureGraphMaxWeakSpurEdges =
            options->feature_graph_max_weak_spur_edges;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, feature_component_min_confidence)) {
        cppOptions.featureComponentMinConfidence =
            options->feature_component_min_confidence;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, min_triangle_quality)) {
        cppOptions.minTriangleQuality = options->min_triangle_quality;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, max_normal_deviation_deg)) {
        cppOptions.maxNormalDeviationDeg = options->max_normal_deviation_deg;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, max_local_error)) {
        cppOptions.maxLocalError = options->max_local_error;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, max_local_error_ratio)) {
        cppOptions.maxLocalErrorRatio = options->max_local_error_ratio;
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, prevent_local_intersections)) {
        cppOptions.preventLocalIntersections =
            boolFromInt(options->prevent_local_intersections);
      }
      if (MANUMESH_SIMPLIFY_FIELD_PRESENT(*options, verbose)) {
        cppOptions.verbose = boolFromInt(options->verbose);
      }
    }

    manumesh::simplification::SimplifyReport cppReport;
    manumesh::simplification::QEMSimplifier simplifier(cppOptions);
    output->mesh = simplifier.simplify(input->mesh, &cppReport);
    if (report) {
      fillReport(cppReport, *report);
    }
    return MANUMESH_STATUS_OK;
  } catch (const std::exception& ex) {
    return translateException(context, ex);
  } catch (...) {
    return translateUnknownException(context);
  }
}

ManuMeshStatus manumesh_compute_mesh_stats(ManuMeshContext* context,
                                           const ManuMeshMeshHandle* mesh,
                                           ManuMeshMeshStats* stats) {
  clearError(context);
  if (!mesh || !stats) {
    return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT,
                "Mesh and stats output pointers must be valid.");
  }
  try {
    fillStats(manumesh::simplification::computeMeshStats(mesh->mesh), *stats);
    return MANUMESH_STATUS_OK;
  } catch (const std::exception& ex) {
    return translateException(context, ex);
  } catch (...) {
    return translateUnknownException(context);
  }
}

} // extern "C"
