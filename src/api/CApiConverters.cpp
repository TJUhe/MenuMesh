#include "api/detail/CApiConverters.h"

#include <cstddef>
#include <cstring>
#include <limits>

namespace manumesh::api {
namespace {

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
                       simplification::WeightMode& output) {
  switch (input) {
  case MANUMESH_WEIGHT_MODE_UNIFORM:
    output = simplification::WeightMode::Uniform;
    return true;
  case MANUMESH_WEIGHT_MODE_DIHEDRAL:
    output = simplification::WeightMode::Dihedral;
    return true;
  case MANUMESH_WEIGHT_MODE_HEIGHT:
    output = simplification::WeightMode::Height;
    return true;
  case MANUMESH_WEIGHT_MODE_X_BAND:
    output = simplification::WeightMode::XBand;
    return true;
  case MANUMESH_WEIGHT_MODE_NORMAL_TENSOR:
    output = simplification::WeightMode::NormalTensor;
    return true;
  }
  return false;
}

bool convertFeatureProtectionMode(ManuMeshFeatureProtectionMode input,
                                  simplification::FeatureProtectionMode& output) {
  switch (input) {
  case MANUMESH_FEATURE_PROTECTION_NONE:
    output = simplification::FeatureProtectionMode::None;
    return true;
  case MANUMESH_FEATURE_PROTECTION_CIRCULAR_ONLY:
    output = simplification::FeatureProtectionMode::CircularOnly;
    return true;
  case MANUMESH_FEATURE_PROTECTION_PRIMITIVE_CURVES:
    output = simplification::FeatureProtectionMode::PrimitiveCurves;
    return true;
  case MANUMESH_FEATURE_PROTECTION_ALL_FEATURE_EDGES:
    output = simplification::FeatureProtectionMode::AllFeatureEdges;
    return true;
  }
  return false;
}

ManuMeshSimplifyTerminationReason
convertTerminationReason(simplification::SimplifyTerminationReason input) {
  switch (input) {
  case simplification::SimplifyTerminationReason::NotStarted:
    return MANUMESH_SIMPLIFY_TERMINATION_NOT_STARTED;
  case simplification::SimplifyTerminationReason::ReachedTarget:
    return MANUMESH_SIMPLIFY_TERMINATION_REACHED_TARGET;
  case simplification::SimplifyTerminationReason::AlreadyAtOrBelowTarget:
    return MANUMESH_SIMPLIFY_TERMINATION_ALREADY_AT_OR_BELOW_TARGET;
  case simplification::SimplifyTerminationReason::NoCandidates:
    return MANUMESH_SIMPLIFY_TERMINATION_NO_CANDIDATES;
  case simplification::SimplifyTerminationReason::RejectionLimit:
    return MANUMESH_SIMPLIFY_TERMINATION_REJECTION_LIMIT;
  }
  return MANUMESH_SIMPLIFY_TERMINATION_NOT_STARTED;
}

} // namespace

void initializeSimplifyOptions(ManuMeshSimplifyOptions& options) {
  initializeAbiStruct(options);
  options.target_faces = -1;
  options.target_ratio = 0.25;
  options.use_line_quadrics = 1;
  options.line_weight = 1e-3;
  options.weight_mode = MANUMESH_WEIGHT_MODE_UNIFORM;
  options.feature_boost = 0.05;
  options.feature_angle_deg = 40.0;
  options.loop_trace_angle_deg = -1.0;
  options.adaptive_scale = 0;
  options.adaptive_base_line_weight = 1e-2;
  options.boundary_weight = 0.0;
  options.preserve_boundary = 0;
  options.preserve_feature_curves = 0;
  options.feature_curve_weight = 0.05;
  options.max_feature_curve_deviation_ratio = 0.0;
  options.circle_fit_relative_threshold = 0.05;
  options.ellipse_fit_relative_threshold = 0.05;
  options.near_circle_axis_ratio_tolerance = 0.08;
  options.min_feature_loop_vertices = 16;
  options.min_circular_feature_loop_vertices = 6;
  options.use_normal_tensor_features = 1;
  options.normal_tensor_feature_threshold = 0.16;
  options.normal_tensor_min_edge_alignment = 0.45;
  options.normal_tensor_smoothing_iterations = 0;
  options.normal_tensor_scale_count = 1;
  options.min_triangle_quality = 0.0;
  options.max_normal_deviation_deg = 90.0;
  options.max_local_error = 0.0;
  options.max_local_error_ratio = 0.0;
  options.prevent_local_intersections = 0;
  options.verbose = 0;
  options.feature_protection_mode = MANUMESH_FEATURE_PROTECTION_PRIMITIVE_CURVES;
  options.normal_tensor_min_persistent_scales = 1;
  options.cleanup_feature_graph = 1;
  options.feature_graph_gap_length_ratio = 1.25;
  options.feature_graph_max_weak_spur_edges = 2;
  options.feature_component_min_confidence = 0.35;
}

void initializeSimplifyReport(ManuMeshSimplifyReport& report) {
  initializeAbiStruct(report);
}

void initializeMeshStats(ManuMeshMeshStats& stats) {
  initializeAbiStruct(stats);
}

bool readSimplifyOptions(const ManuMeshSimplifyOptions& source,
                         simplification::SimplifyOptions& target,
                         std::string& error) {
  if (!abiStructLooksInitialized(source)) {
    error = "ManuMeshSimplifyOptions must be initialized with "
            "manumesh_simplify_options_init for this ABI version.";
    return false;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, target_faces)) {
    target.targetFaces = source.target_faces;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, target_ratio)) {
    target.targetRatio = source.target_ratio;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, use_line_quadrics)) {
    target.useLineQuadrics = boolFromInt(source.use_line_quadrics);
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, line_weight)) {
    target.lineWeight = source.line_weight;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, weight_mode) &&
      !convertWeightMode(source.weight_mode, target.weightMode)) {
    error = "Unknown simplification weight mode.";
    return false;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, feature_boost)) {
    target.featureBoost = source.feature_boost;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, feature_angle_deg)) {
    target.featureAngleDeg = source.feature_angle_deg;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, loop_trace_angle_deg)) {
    target.loopTraceAngleDeg = source.loop_trace_angle_deg;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, adaptive_scale)) {
    target.adaptiveScale = boolFromInt(source.adaptive_scale);
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, adaptive_base_line_weight)) {
    target.adaptiveBaseLineWeight = source.adaptive_base_line_weight;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, boundary_weight)) {
    target.boundaryWeight = source.boundary_weight;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, preserve_boundary)) {
    target.preserveBoundary = boolFromInt(source.preserve_boundary);
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, preserve_feature_curves)) {
    target.preserveFeatureCurves = boolFromInt(source.preserve_feature_curves);
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, feature_protection_mode) &&
      !convertFeatureProtectionMode(source.feature_protection_mode,
                                    target.featureProtectionMode)) {
    error = "Unknown feature protection mode.";
    return false;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, feature_curve_weight)) {
    target.featureCurveWeight = source.feature_curve_weight;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, max_feature_curve_deviation_ratio)) {
    target.maxFeatureCurveDeviationRatio = source.max_feature_curve_deviation_ratio;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, circle_fit_relative_threshold)) {
    target.circleFitRelativeThreshold = source.circle_fit_relative_threshold;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, ellipse_fit_relative_threshold)) {
    target.ellipseFitRelativeThreshold = source.ellipse_fit_relative_threshold;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, near_circle_axis_ratio_tolerance)) {
    target.nearCircleAxisRatioTolerance = source.near_circle_axis_ratio_tolerance;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, min_feature_loop_vertices)) {
    target.minFeatureLoopVertices = source.min_feature_loop_vertices;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, min_circular_feature_loop_vertices)) {
    target.minCircularFeatureLoopVertices = source.min_circular_feature_loop_vertices;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, use_normal_tensor_features)) {
    target.useNormalTensorFeatures = boolFromInt(source.use_normal_tensor_features);
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, normal_tensor_feature_threshold)) {
    target.normalTensorFeatureThreshold = source.normal_tensor_feature_threshold;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, normal_tensor_min_edge_alignment)) {
    target.normalTensorMinEdgeAlignment = source.normal_tensor_min_edge_alignment;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, normal_tensor_smoothing_iterations)) {
    target.normalTensorSmoothingIterations =
        source.normal_tensor_smoothing_iterations;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, normal_tensor_scale_count)) {
    target.normalTensorScaleCount = source.normal_tensor_scale_count;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, normal_tensor_min_persistent_scales)) {
    target.normalTensorMinPersistentScales =
        source.normal_tensor_min_persistent_scales;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, cleanup_feature_graph)) {
    target.cleanupFeatureGraph = boolFromInt(source.cleanup_feature_graph);
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, feature_graph_gap_length_ratio)) {
    target.featureGraphGapLengthRatio = source.feature_graph_gap_length_ratio;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, feature_graph_max_weak_spur_edges)) {
    target.featureGraphMaxWeakSpurEdges = source.feature_graph_max_weak_spur_edges;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, feature_component_min_confidence)) {
    target.featureComponentMinConfidence = source.feature_component_min_confidence;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, min_triangle_quality)) {
    target.minTriangleQuality = source.min_triangle_quality;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, max_normal_deviation_deg)) {
    target.maxNormalDeviationDeg = source.max_normal_deviation_deg;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, max_local_error)) {
    target.maxLocalError = source.max_local_error;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, max_local_error_ratio)) {
    target.maxLocalErrorRatio = source.max_local_error_ratio;
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, prevent_local_intersections)) {
    target.preventLocalIntersections = boolFromInt(source.prevent_local_intersections);
  }
  if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, verbose)) {
    target.verbose = boolFromInt(source.verbose);
  }
  return true;
}

void fillSimplifyReport(const simplification::SimplifyReport& source,
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

void fillMeshStats(const simplification::MeshStats& source,
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

} // namespace manumesh::api
