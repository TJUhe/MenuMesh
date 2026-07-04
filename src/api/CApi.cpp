#include "line_quadrics_qem/api/CApi.h"

#include "line_quadrics_qem/core/MeshGenerators.h"
#include "line_quadrics_qem/simplification/Metrics.h"
#include "line_quadrics_qem/simplification/QEMSimplifier.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

#ifndef LINE_QUADRICS_QEM_VERSION
#define LINE_QUADRICS_QEM_VERSION "0.0.0"
#endif

struct LqContext {
  std::string lastError;
};

struct LqMeshHandle {
  lq::Mesh mesh;
};

namespace {

void clearError(LqContext* context) {
  if (context) {
    context->lastError.clear();
  }
}

LqStatus fail(LqContext* context, LqStatus status, const std::string& message) {
  if (context) {
    context->lastError = message;
  }
  return status;
}

LqStatus translateException(LqContext* context, const std::exception& ex) {
  if (dynamic_cast<const std::bad_alloc*>(&ex) != nullptr) {
    return fail(context, LQ_STATUS_OUT_OF_MEMORY, ex.what());
  }
  if (dynamic_cast<const std::invalid_argument*>(&ex) != nullptr) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT, ex.what());
  }
  return fail(context, LQ_STATUS_ALGORITHM_ERROR, ex.what());
}

LqStatus translateUnknownException(LqContext* context) {
  return fail(context, LQ_STATUS_ALGORITHM_ERROR, "Unknown C++ exception.");
}

bool boolFromInt(int value) {
  return value != 0;
}

bool convertWeightMode(LqWeightMode input, lq::WeightMode& output) {
  switch (input) {
  case LQ_WEIGHT_MODE_UNIFORM:
    output = lq::WeightMode::Uniform;
    return true;
  case LQ_WEIGHT_MODE_DIHEDRAL:
    output = lq::WeightMode::Dihedral;
    return true;
  case LQ_WEIGHT_MODE_HEIGHT:
    output = lq::WeightMode::Height;
    return true;
  case LQ_WEIGHT_MODE_X_BAND:
    output = lq::WeightMode::XBand;
    return true;
  case LQ_WEIGHT_MODE_NORMAL_TENSOR:
    output = lq::WeightMode::NormalTensor;
    return true;
  }
  return false;
}

LqSimplifyTerminationReason
convertTerminationReason(lq::SimplifyTerminationReason input) {
  switch (input) {
  case lq::SimplifyTerminationReason::NotStarted:
    return LQ_SIMPLIFY_TERMINATION_NOT_STARTED;
  case lq::SimplifyTerminationReason::ReachedTarget:
    return LQ_SIMPLIFY_TERMINATION_REACHED_TARGET;
  case lq::SimplifyTerminationReason::AlreadyAtOrBelowTarget:
    return LQ_SIMPLIFY_TERMINATION_ALREADY_AT_OR_BELOW_TARGET;
  case lq::SimplifyTerminationReason::NoCandidates:
    return LQ_SIMPLIFY_TERMINATION_NO_CANDIDATES;
  case lq::SimplifyTerminationReason::RejectionLimit:
    return LQ_SIMPLIFY_TERMINATION_REJECTION_LIMIT;
  }
  return LQ_SIMPLIFY_TERMINATION_NOT_STARTED;
}

void fillReport(const lq::SimplifyReport& source, LqSimplifyReport& target) {
  target.initial_vertices = source.initialVertices;
  target.initial_faces = source.initialFaces;
  target.final_vertices = source.finalVertices;
  target.final_faces = source.finalFaces;
  target.collapsed_edges = source.collapsedEdges;
  target.rejected_collapses = source.rejectedCollapses;
  target.solver_fallbacks = source.solverFallbacks;
  target.queue_rebuilds = source.queueRebuilds;
  target.feature_loops = source.featureLoops;
  target.circular_feature_loops = source.circularFeatureLoops;
  target.feature_vertices = source.featureVertices;
  target.normal_tensor_feature_edges = source.normalTensorFeatureEdges;
  target.feature_rejected_collapses = source.featureRejectedCollapses;
  target.boundary_rejected_collapses = source.boundaryRejectedCollapses;
  target.topology_rejected_collapses = source.topologyRejectedCollapses;
  target.normal_flip_rejected_collapses = source.normalFlipRejectedCollapses;
  target.quality_rejected_collapses = source.qualityRejectedCollapses;
  target.self_intersection_rejected_collapses =
      source.selfIntersectionRejectedCollapses;
  target.curve_budget_rejected_collapses = source.curveBudgetRejectedCollapses;
  target.error_rejected_collapses = source.errorRejectedCollapses;
  target.projected_feature_placements = source.projectedFeaturePlacements;
  target.termination_reason = convertTerminationReason(source.terminationReason);
  target.min_applied_line_weight = source.minAppliedLineWeight;
  target.max_applied_line_weight = source.maxAppliedLineWeight;
}

void fillStats(const lq::MeshStats& source, LqMeshStats& target) {
  target.vertices = source.vertices;
  target.faces = source.faces;
  target.edges = source.edges;
  target.boundary_edges = source.boundaryEdges;
  target.non_manifold_edges = source.nonManifoldEdges;
  target.area = source.area;
  target.mean_triangle_quality = source.meanTriangleQuality;
  target.min_triangle_quality = source.minTriangleQuality;
  target.mean_edge_length = source.meanEdgeLength;
  target.edge_length_cv = source.edgeLengthCv;
}

} // namespace

extern "C" {

const char* lq_version(void) {
  return LINE_QUADRICS_QEM_VERSION;
}

const char* lq_status_message(LqStatus status) {
  switch (status) {
  case LQ_STATUS_OK:
    return "ok";
  case LQ_STATUS_INVALID_ARGUMENT:
    return "invalid argument";
  case LQ_STATUS_BUFFER_TOO_SMALL:
    return "buffer too small";
  case LQ_STATUS_IO_ERROR:
    return "I/O error";
  case LQ_STATUS_ALGORITHM_ERROR:
    return "algorithm error";
  case LQ_STATUS_OUT_OF_MEMORY:
    return "out of memory";
  }
  return "unknown status";
}

LqContext* lq_context_create(void) {
  return new (std::nothrow) LqContext();
}

void lq_context_destroy(LqContext* context) {
  delete context;
}

const char* lq_context_last_error(const LqContext* context) {
  return context ? context->lastError.c_str() : "LqContext is null.";
}

void lq_context_clear_error(LqContext* context) {
  clearError(context);
}

LqMeshHandle* lq_mesh_create(LqContext* context) {
  clearError(context);
  LqMeshHandle* mesh = new (std::nothrow) LqMeshHandle();
  if (!mesh) {
    fail(context, LQ_STATUS_OUT_OF_MEMORY, "Failed to allocate mesh handle.");
  }
  return mesh;
}

void lq_mesh_destroy(LqMeshHandle* mesh) {
  delete mesh;
}

LqStatus lq_mesh_clear(LqContext* context, LqMeshHandle* mesh) {
  clearError(context);
  if (!mesh) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
  }
  mesh->mesh.vertices.clear();
  mesh->mesh.faces.clear();
  return LQ_STATUS_OK;
}

LqStatus lq_mesh_set_data(LqContext* context, LqMeshHandle* mesh,
                          const LqVec3* vertices, size_t vertex_count,
                          const LqFace* faces, size_t face_count) {
  clearError(context);
  if (!mesh) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
  }
  if ((vertex_count > 0 && !vertices) || (face_count > 0 && !faces)) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT,
                "Vertex and face pointers must be valid when counts are non-zero.");
  }
  if (vertex_count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT,
                "Vertex count exceeds the supported int-index range.");
  }

  try {
    lq::Mesh next;
    next.vertices.reserve(vertex_count);
    next.faces.reserve(face_count);
    for (size_t i = 0; i < vertex_count; ++i) {
      next.vertices.emplace_back(vertices[i].x, vertices[i].y, vertices[i].z);
    }
    for (size_t i = 0; i < face_count; ++i) {
      lq::Face face;
      face.v = {faces[i].v[0], faces[i].v[1], faces[i].v[2]};
      next.faces.push_back(face);
    }
    std::string error;
    if (!lq::validateMeshGeometry(next, &error)) {
      return fail(context, LQ_STATUS_INVALID_ARGUMENT,
                  error.empty() ? "Mesh geometry is invalid." : error);
    }
    mesh->mesh = std::move(next);
    return LQ_STATUS_OK;
  } catch (const std::exception& ex) {
    return translateException(context, ex);
  } catch (...) {
    return translateUnknownException(context);
  }
}

LqStatus lq_mesh_get_counts(LqContext* context, const LqMeshHandle* mesh,
                            size_t* vertex_count, size_t* face_count) {
  clearError(context);
  if (!mesh || !vertex_count || !face_count) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT,
                "Mesh and output count pointers must be valid.");
  }
  *vertex_count = mesh->mesh.vertices.size();
  *face_count = mesh->mesh.faces.size();
  return LQ_STATUS_OK;
}

LqStatus lq_mesh_copy_vertices(LqContext* context, const LqMeshHandle* mesh,
                               LqVec3* vertices, size_t vertex_capacity,
                               size_t* vertices_written) {
  clearError(context);
  if (!mesh || !vertices_written) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT,
                "Mesh and vertices_written pointers must be valid.");
  }
  const size_t required = mesh->mesh.vertices.size();
  *vertices_written = required;
  if (vertex_capacity < required) {
    return fail(context, LQ_STATUS_BUFFER_TOO_SMALL,
                "Vertex buffer is smaller than the mesh vertex count.");
  }
  if (required > 0 && !vertices) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT, "Vertex buffer is null.");
  }
  for (size_t i = 0; i < required; ++i) {
    vertices[i].x = mesh->mesh.vertices[i].x();
    vertices[i].y = mesh->mesh.vertices[i].y();
    vertices[i].z = mesh->mesh.vertices[i].z();
  }
  return LQ_STATUS_OK;
}

LqStatus lq_mesh_copy_faces(LqContext* context, const LqMeshHandle* mesh, LqFace* faces,
                            size_t face_capacity, size_t* faces_written) {
  clearError(context);
  if (!mesh || !faces_written) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT,
                "Mesh and faces_written pointers must be valid.");
  }
  const size_t required = mesh->mesh.faces.size();
  *faces_written = required;
  if (face_capacity < required) {
    return fail(context, LQ_STATUS_BUFFER_TOO_SMALL,
                "Face buffer is smaller than the mesh face count.");
  }
  if (required > 0 && !faces) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT, "Face buffer is null.");
  }
  for (size_t i = 0; i < required; ++i) {
    faces[i].v[0] = mesh->mesh.faces[i].v[0];
    faces[i].v[1] = mesh->mesh.faces[i].v[1];
    faces[i].v[2] = mesh->mesh.faces[i].v[2];
  }
  return LQ_STATUS_OK;
}

LqStatus lq_load_mesh(LqContext* context, const char* path, LqMeshHandle* mesh,
                      double weld_relative_epsilon) {
  clearError(context);
  if (!path || !mesh) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT,
                "Path and mesh handle must be valid.");
  }
  if (!std::isfinite(weld_relative_epsilon) || weld_relative_epsilon < 0.0) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT,
                "weld_relative_epsilon must be finite and non-negative.");
  }
  std::string error;
  try {
    lq::Mesh loaded;
    if (!lq::loadMesh(path, loaded, &error, weld_relative_epsilon)) {
      return fail(context, LQ_STATUS_IO_ERROR, error);
    }
    mesh->mesh = std::move(loaded);
    return LQ_STATUS_OK;
  } catch (const std::exception& ex) {
    return translateException(context, ex);
  } catch (...) {
    return translateUnknownException(context);
  }
}

LqStatus lq_save_ascii_stl(LqContext* context, const char* path,
                           const LqMeshHandle* mesh, const char* solid_name) {
  clearError(context);
  if (!path || !mesh) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT,
                "Path and mesh handle must be valid.");
  }
  std::string error;
  try {
    const char* name = solid_name ? solid_name : "mesh";
    if (!lq::saveAsciiStl(path, mesh->mesh, name, &error)) {
      return fail(context, LQ_STATUS_IO_ERROR, error);
    }
    return LQ_STATUS_OK;
  } catch (const std::exception& ex) {
    return translateException(context, ex);
  } catch (...) {
    return translateUnknownException(context);
  }
}

LqStatus lq_generate_mesh(LqContext* context, const char* name, int n,
                          LqMeshHandle* mesh) {
  clearError(context);
  if (!name || !mesh) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT,
                "Generator name and mesh handle must be valid.");
  }
  std::string error;
  try {
    lq::Mesh generated;
    if (!lq::generateMeshByName(name, n, generated, &error)) {
      return fail(context, LQ_STATUS_INVALID_ARGUMENT, error);
    }
    mesh->mesh = std::move(generated);
    return LQ_STATUS_OK;
  } catch (const std::exception& ex) {
    return translateException(context, ex);
  } catch (...) {
    return translateUnknownException(context);
  }
}

void lq_simplify_options_init(LqSimplifyOptions* options) {
  if (!options) {
    return;
  }
  options->target_faces = -1;
  options->target_ratio = 0.25;
  options->use_line_quadrics = 1;
  options->line_weight = 1e-3;
  options->weight_mode = LQ_WEIGHT_MODE_UNIFORM;
  options->feature_boost = 0.05;
  options->feature_angle_deg = 40.0;
  options->adaptive_scale = 0;
  options->adaptive_base_line_weight = 1e-2;
  options->boundary_weight = 0.0;
  options->preserve_boundary = 0;
  options->preserve_feature_curves = 0;
  options->protect_all_feature_edges = 0;
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
}

LqStatus lq_simplify_mesh(LqContext* context, const LqMeshHandle* input,
                          const LqSimplifyOptions* options, LqMeshHandle* output,
                          LqSimplifyReport* report) {
  clearError(context);
  if (!input || !output) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT,
                "Input and output mesh handles must be valid.");
  }
  try {
    lq::SimplifyOptions cppOptions;
    if (options) {
      cppOptions.targetFaces = options->target_faces;
      cppOptions.targetRatio = options->target_ratio;
      cppOptions.useLineQuadrics = boolFromInt(options->use_line_quadrics);
      cppOptions.lineWeight = options->line_weight;
      if (!convertWeightMode(options->weight_mode, cppOptions.weightMode)) {
        return fail(context, LQ_STATUS_INVALID_ARGUMENT,
                    "Unknown simplification weight mode.");
      }
      cppOptions.featureBoost = options->feature_boost;
      cppOptions.featureAngleDeg = options->feature_angle_deg;
      cppOptions.adaptiveScale = boolFromInt(options->adaptive_scale);
      cppOptions.adaptiveBaseLineWeight = options->adaptive_base_line_weight;
      cppOptions.boundaryWeight = options->boundary_weight;
      cppOptions.preserveBoundary = boolFromInt(options->preserve_boundary);
      cppOptions.preserveFeatureCurves = boolFromInt(options->preserve_feature_curves);
      cppOptions.protectAllFeatureEdges =
          boolFromInt(options->protect_all_feature_edges);
      cppOptions.featureCurveWeight = options->feature_curve_weight;
      cppOptions.maxFeatureCurveDeviationRatio =
          options->max_feature_curve_deviation_ratio;
      cppOptions.circleFitRelativeThreshold = options->circle_fit_relative_threshold;
      cppOptions.ellipseFitRelativeThreshold = options->ellipse_fit_relative_threshold;
      cppOptions.nearCircleAxisRatioTolerance =
          options->near_circle_axis_ratio_tolerance;
      cppOptions.minFeatureLoopVertices = options->min_feature_loop_vertices;
      cppOptions.minCircularFeatureLoopVertices =
          options->min_circular_feature_loop_vertices;
      cppOptions.useNormalTensorFeatures =
          boolFromInt(options->use_normal_tensor_features);
      cppOptions.normalTensorFeatureThreshold =
          options->normal_tensor_feature_threshold;
      cppOptions.normalTensorMinEdgeAlignment =
          options->normal_tensor_min_edge_alignment;
      cppOptions.normalTensorSmoothingIterations =
          options->normal_tensor_smoothing_iterations;
      cppOptions.normalTensorScaleCount = options->normal_tensor_scale_count;
      cppOptions.minTriangleQuality = options->min_triangle_quality;
      cppOptions.maxNormalDeviationDeg = options->max_normal_deviation_deg;
      cppOptions.maxLocalError = options->max_local_error;
      cppOptions.maxLocalErrorRatio = options->max_local_error_ratio;
      cppOptions.preventLocalIntersections =
          boolFromInt(options->prevent_local_intersections);
      cppOptions.verbose = boolFromInt(options->verbose);
    }

    lq::SimplifyReport cppReport;
    lq::QEMSimplifier simplifier(cppOptions);
    output->mesh = simplifier.simplify(input->mesh, &cppReport);
    if (report) {
      fillReport(cppReport, *report);
    }
    return LQ_STATUS_OK;
  } catch (const std::exception& ex) {
    return translateException(context, ex);
  } catch (...) {
    return translateUnknownException(context);
  }
}

LqStatus lq_compute_mesh_stats(LqContext* context, const LqMeshHandle* mesh,
                               LqMeshStats* stats) {
  clearError(context);
  if (!mesh || !stats) {
    return fail(context, LQ_STATUS_INVALID_ARGUMENT,
                "Mesh and stats output pointers must be valid.");
  }
  try {
    fillStats(lq::computeMeshStats(mesh->mesh), *stats);
    return LQ_STATUS_OK;
  } catch (const std::exception& ex) {
    return translateException(context, ex);
  } catch (...) {
    return translateUnknownException(context);
  }
}

} // extern "C"
