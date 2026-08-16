/**
 * @file src/api/CApiConverters.cpp
 * @brief 在带大小的 C ABI 结构与内部 C++ 类型之间转换。
 * @ingroup manumesh_c_api
 *
 * @details 转换器复用 C ABI 的容量检查和有限数值约定，将内部结果安全写入公开结构体。
 */

#include "api/detail/CApiConverters.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>

namespace manumesh {
namespace api {
namespace {

bool boolFromInt(int value) { return value != 0; }

/**
 * @brief 复制有限 double 选项，遵循 CLI 的严格数值约定。
 */
bool readFiniteDouble(double value, const char* fieldName, double& target, std::string& error) {
    if (!std::isfinite(value)) {
        error = std::string("ManuMeshSimplifyOptions.") + fieldName + " must be a finite number.";
        return false;
    }
    target = value;
    return true;
}

bool abiFieldPresent(std::size_t structSize, std::size_t fieldOffset, std::size_t fieldSize) {
    return fieldOffset <= std::numeric_limits<std::size_t>::max() - fieldSize && structSize >= fieldOffset + fieldSize;
}

template <typename T, typename Field>
void writeAbiField(T* value, std::size_t writeSize, std::size_t fieldOffset, const Field& fieldValue) {
    if (!abiFieldPresent(writeSize, fieldOffset, sizeof(Field))) {
        return;
    }
    std::memcpy(reinterpret_cast<unsigned char*>(value) + fieldOffset, &fieldValue, sizeof(Field));
}

template <typename T, typename Field> bool readAbiField(const T& value, std::size_t fieldOffset, Field& fieldValue) {
    if (!abiFieldPresent(value.struct_size, fieldOffset, sizeof(Field))) {
        return false;
    }
    std::memcpy(&fieldValue, reinterpret_cast<const unsigned char*>(&value) + fieldOffset, sizeof(Field));
    return true;
}

template <typename T> ManuMeshStatus initializeAbiBuffer(T* value, std::size_t structCapacity, std::size_t& writeSize) {
    constexpr std::size_t kMinimumInitializedSize = offsetof(T, abi_version) + sizeof(unsigned int);
    if (!value || structCapacity < kMinimumInitializedSize) {
        return MANUMESH_STATUS_INVALID_ARGUMENT;
    }

    writeSize = structCapacity < sizeof(T) ? structCapacity : sizeof(T);
    std::memset(static_cast<void*>(value), 0, writeSize);
    writeAbiField(value, writeSize, offsetof(T, struct_size), writeSize);
    const unsigned int abiVersion = MANUMESH_ABI_VERSION;
    writeAbiField(value, writeSize, offsetof(T, abi_version), abiVersion);
    return MANUMESH_STATUS_OK;
}

template <typename T> bool abiStructLooksInitialized(const T& value) {
    constexpr std::size_t kMinimumInitializedSize = offsetof(T, abi_version) + sizeof(value.abi_version);
    return value.struct_size >= kMinimumInitializedSize && value.abi_version == MANUMESH_ABI_VERSION;
}

template <typename T>
bool validateOutputCapacity(
    const T* value, std::size_t structCapacity, bool allowNull, const char* typeName, std::string& error
) {
    if (!value) {
        if (allowNull) {
            return true;
        }
        error = std::string(typeName) + " output pointer must be valid.";
        return false;
    }

    constexpr std::size_t kMinimumInitializedSize = offsetof(T, abi_version) + sizeof(unsigned int);
    if (structCapacity >= kMinimumInitializedSize) {
        return true;
    }
    error = std::string(typeName) + " output capacity must include the complete abi_version field.";
    return false;
}

#define MANUMESH_SIMPLIFY_FIELD_PRESENT(options, field)                                                                \
    abiFieldPresent((options).struct_size, offsetof(ManuMeshSimplifyOptions, field), sizeof((options).field))

#define MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(options, field, hasOverride)                                             \
    (!(hasOverride) && MANUMESH_SIMPLIFY_FIELD_PRESENT(options, field))

#define MANUMESH_REPORT_FIELD_PRESENT(size, field)                                                                     \
    abiFieldPresent((size), offsetof(ManuMeshSimplifyReport, field), sizeof(ManuMeshSimplifyReport{}.field))

#define MANUMESH_STATS_FIELD_PRESENT(size, field)                                                                      \
    abiFieldPresent((size), offsetof(ManuMeshMeshStats, field), sizeof(ManuMeshMeshStats{}.field))

#define MANUMESH_SET_REPORT_FIELD(target, size, field, value)                                                          \
    do {                                                                                                               \
        if (MANUMESH_REPORT_FIELD_PRESENT((size), field)) {                                                            \
            (target).field = (value);                                                                                  \
        }                                                                                                              \
    } while (false)

#define MANUMESH_SET_STATS_FIELD(target, size, field, value)                                                           \
    do {                                                                                                               \
        if (MANUMESH_STATS_FIELD_PRESENT((size), field)) {                                                             \
            (target).field = (value);                                                                                  \
        }                                                                                                              \
    } while (false)

bool convertWeightMode(ManuMeshWeightMode input, simplification::WeightMode& output) {
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

bool convertFeatureProtectionMode(ManuMeshFeatureProtectionMode input, simplification::FeatureProtectionMode& output) {
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

ManuMeshSimplifyTerminationReason convertTerminationReason(simplification::SimplifyTerminationReason input) {
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

ManuMeshStatus initializeFeatureOptions(ManuMeshFeatureOptions* options, std::size_t structCapacity) {
    std::size_t writeSize = 0;
    const ManuMeshStatus status = initializeAbiBuffer(options, structCapacity, writeSize);
    if (status != MANUMESH_STATUS_OK) {
        return status;
    }

    const feature::FeatureOptions defaults;
#define MANUMESH_INITIALIZE_FEATURE_OPTION(field, value)                                                               \
    writeAbiField(options, writeSize, offsetof(ManuMeshFeatureOptions, field), value)
    MANUMESH_INITIALIZE_FEATURE_OPTION(feature_angle_deg, defaults.featureAngleDeg);
    MANUMESH_INITIALIZE_FEATURE_OPTION(loop_trace_angle_deg, defaults.loopTraceAngleDeg);
    MANUMESH_INITIALIZE_FEATURE_OPTION(circle_fit_relative_threshold, defaults.circleFitRelativeThreshold);
    MANUMESH_INITIALIZE_FEATURE_OPTION(ellipse_fit_relative_threshold, defaults.ellipseFitRelativeThreshold);
    MANUMESH_INITIALIZE_FEATURE_OPTION(near_circle_axis_ratio_tolerance, defaults.nearCircleAxisRatioTolerance);
    MANUMESH_INITIALIZE_FEATURE_OPTION(min_feature_loop_vertices, defaults.minFeatureLoopVertices);
    MANUMESH_INITIALIZE_FEATURE_OPTION(use_normal_tensor_features, defaults.useNormalTensorFeatures ? 1 : 0);
    MANUMESH_INITIALIZE_FEATURE_OPTION(normal_tensor_feature_threshold, defaults.normalTensorFeatureThreshold);
    MANUMESH_INITIALIZE_FEATURE_OPTION(normal_tensor_min_edge_alignment, defaults.normalTensorMinEdgeAlignment);
    MANUMESH_INITIALIZE_FEATURE_OPTION(normal_tensor_smoothing_iterations, defaults.normalTensorSmoothingIterations);
    MANUMESH_INITIALIZE_FEATURE_OPTION(normal_tensor_scale_count, defaults.normalTensorScaleCount);
    MANUMESH_INITIALIZE_FEATURE_OPTION(normal_tensor_min_persistent_scales, defaults.normalTensorMinPersistentScales);
    MANUMESH_INITIALIZE_FEATURE_OPTION(use_smooth_curvature_features, defaults.useSmoothCurvatureFeatures ? 1 : 0);
    MANUMESH_INITIALIZE_FEATURE_OPTION(smooth_curvature_feature_threshold, defaults.smoothCurvatureFeatureThreshold);
    MANUMESH_INITIALIZE_FEATURE_OPTION(smooth_curvature_min_edge_alignment, defaults.smoothCurvatureMinEdgeAlignment);
    MANUMESH_INITIALIZE_FEATURE_OPTION(
        smooth_curvature_min_tangent_consistency, defaults.smoothCurvatureMinTangentConsistency
    );
    MANUMESH_INITIALIZE_FEATURE_OPTION(
        smooth_curvature_base_neighborhood_rings, defaults.smoothCurvatureBaseNeighborhoodRings
    );
    MANUMESH_INITIALIZE_FEATURE_OPTION(smooth_curvature_scale_count, defaults.smoothCurvatureScaleCount);
    MANUMESH_INITIALIZE_FEATURE_OPTION(
        smooth_curvature_min_persistent_scales, defaults.smoothCurvatureMinPersistentScales
    );
    MANUMESH_INITIALIZE_FEATURE_OPTION(
        smooth_curvature_robust_fit_iterations, defaults.smoothCurvatureRobustFitIterations
    );
    MANUMESH_INITIALIZE_FEATURE_OPTION(
        smooth_curvature_use_stable_scale_selection, defaults.smoothCurvatureUseStableScaleSelection ? 1 : 0
    );
    MANUMESH_INITIALIZE_FEATURE_OPTION(smooth_curvature_min_scale_stability, defaults.smoothCurvatureMinScaleStability);
    MANUMESH_INITIALIZE_FEATURE_OPTION(cleanup_feature_graph, defaults.cleanupFeatureGraph ? 1 : 0);
    MANUMESH_INITIALIZE_FEATURE_OPTION(feature_graph_gap_length_ratio, defaults.featureGraphGapLengthRatio);
    MANUMESH_INITIALIZE_FEATURE_OPTION(feature_graph_max_weak_spur_edges, defaults.featureGraphMaxWeakSpurEdges);
    MANUMESH_INITIALIZE_FEATURE_OPTION(feature_graph_min_weak_spur_strength, defaults.featureGraphMinWeakSpurStrength);
    MANUMESH_INITIALIZE_FEATURE_OPTION(feature_component_min_confidence, defaults.featureComponentMinConfidence);
    MANUMESH_INITIALIZE_FEATURE_OPTION(normal_filter_enabled, defaults.normalFilter.enabled ? 1 : 0);
    MANUMESH_INITIALIZE_FEATURE_OPTION(normal_filter_iterations, defaults.normalFilter.iterations);
    MANUMESH_INITIALIZE_FEATURE_OPTION(normal_filter_angle_sigma_deg, defaults.normalFilter.angleSigmaDeg);
    MANUMESH_INITIALIZE_FEATURE_OPTION(normal_filter_preserve_angle_deg, defaults.normalFilter.preserveAngleDeg);
    MANUMESH_INITIALIZE_FEATURE_OPTION(normal_filter_relaxation, defaults.normalFilter.relaxation);
    MANUMESH_INITIALIZE_FEATURE_OPTION(graph_consolidation_enabled, defaults.graphConsolidation.enabled ? 1 : 0);
    MANUMESH_INITIALIZE_FEATURE_OPTION(
        graph_consolidation_gap_length_ratio, defaults.graphConsolidation.maxGapLengthRatio
    );
    MANUMESH_INITIALIZE_FEATURE_OPTION(graph_consolidation_min_alignment, defaults.graphConsolidation.minAlignment);
#undef MANUMESH_INITIALIZE_FEATURE_OPTION
    return MANUMESH_STATUS_OK;
}

bool readFeatureOptions(const ManuMeshFeatureOptions* source, feature::FeatureOptions& target, std::string& error) {
    target = feature::FeatureOptions{};
    error.clear();
    if (source == nullptr) {
        return true;
    }
    if (!abiStructLooksInitialized(*source)) {
        error = "ManuMeshFeatureOptions must be initialized with "
                "manumesh_feature_options_init for this ABI version.";
        return false;
    }

#define MANUMESH_READ_FEATURE_OPTION(field, member)                                                                    \
    do {                                                                                                               \
        using FieldType = decltype(target.member);                                                                     \
        FieldType value{};                                                                                             \
        if (readAbiField(*source, offsetof(ManuMeshFeatureOptions, field), value)) {                                   \
            target.member = value;                                                                                     \
        }                                                                                                              \
    } while (false)
#define MANUMESH_READ_FEATURE_BOOL(field, member)                                                                      \
    do {                                                                                                               \
        int value = 0;                                                                                                 \
        if (readAbiField(*source, offsetof(ManuMeshFeatureOptions, field), value)) {                                   \
            target.member = boolFromInt(value);                                                                        \
        }                                                                                                              \
    } while (false)
    MANUMESH_READ_FEATURE_OPTION(feature_angle_deg, featureAngleDeg);
    MANUMESH_READ_FEATURE_OPTION(loop_trace_angle_deg, loopTraceAngleDeg);
    MANUMESH_READ_FEATURE_OPTION(circle_fit_relative_threshold, circleFitRelativeThreshold);
    MANUMESH_READ_FEATURE_OPTION(ellipse_fit_relative_threshold, ellipseFitRelativeThreshold);
    MANUMESH_READ_FEATURE_OPTION(near_circle_axis_ratio_tolerance, nearCircleAxisRatioTolerance);
    MANUMESH_READ_FEATURE_OPTION(min_feature_loop_vertices, minFeatureLoopVertices);
    MANUMESH_READ_FEATURE_BOOL(use_normal_tensor_features, useNormalTensorFeatures);
    MANUMESH_READ_FEATURE_OPTION(normal_tensor_feature_threshold, normalTensorFeatureThreshold);
    MANUMESH_READ_FEATURE_OPTION(normal_tensor_min_edge_alignment, normalTensorMinEdgeAlignment);
    MANUMESH_READ_FEATURE_OPTION(normal_tensor_smoothing_iterations, normalTensorSmoothingIterations);
    MANUMESH_READ_FEATURE_OPTION(normal_tensor_scale_count, normalTensorScaleCount);
    MANUMESH_READ_FEATURE_OPTION(normal_tensor_min_persistent_scales, normalTensorMinPersistentScales);
    MANUMESH_READ_FEATURE_BOOL(use_smooth_curvature_features, useSmoothCurvatureFeatures);
    MANUMESH_READ_FEATURE_OPTION(smooth_curvature_feature_threshold, smoothCurvatureFeatureThreshold);
    MANUMESH_READ_FEATURE_OPTION(smooth_curvature_min_edge_alignment, smoothCurvatureMinEdgeAlignment);
    MANUMESH_READ_FEATURE_OPTION(smooth_curvature_min_tangent_consistency, smoothCurvatureMinTangentConsistency);
    MANUMESH_READ_FEATURE_OPTION(smooth_curvature_base_neighborhood_rings, smoothCurvatureBaseNeighborhoodRings);
    MANUMESH_READ_FEATURE_OPTION(smooth_curvature_scale_count, smoothCurvatureScaleCount);
    MANUMESH_READ_FEATURE_OPTION(smooth_curvature_min_persistent_scales, smoothCurvatureMinPersistentScales);
    MANUMESH_READ_FEATURE_OPTION(smooth_curvature_robust_fit_iterations, smoothCurvatureRobustFitIterations);
    MANUMESH_READ_FEATURE_BOOL(smooth_curvature_use_stable_scale_selection, smoothCurvatureUseStableScaleSelection);
    MANUMESH_READ_FEATURE_OPTION(smooth_curvature_min_scale_stability, smoothCurvatureMinScaleStability);
    MANUMESH_READ_FEATURE_BOOL(cleanup_feature_graph, cleanupFeatureGraph);
    MANUMESH_READ_FEATURE_OPTION(feature_graph_gap_length_ratio, featureGraphGapLengthRatio);
    MANUMESH_READ_FEATURE_OPTION(feature_graph_max_weak_spur_edges, featureGraphMaxWeakSpurEdges);
    MANUMESH_READ_FEATURE_OPTION(feature_graph_min_weak_spur_strength, featureGraphMinWeakSpurStrength);
    MANUMESH_READ_FEATURE_OPTION(feature_component_min_confidence, featureComponentMinConfidence);
    MANUMESH_READ_FEATURE_BOOL(normal_filter_enabled, normalFilter.enabled);
    MANUMESH_READ_FEATURE_OPTION(normal_filter_iterations, normalFilter.iterations);
    MANUMESH_READ_FEATURE_OPTION(normal_filter_angle_sigma_deg, normalFilter.angleSigmaDeg);
    MANUMESH_READ_FEATURE_OPTION(normal_filter_preserve_angle_deg, normalFilter.preserveAngleDeg);
    MANUMESH_READ_FEATURE_OPTION(normal_filter_relaxation, normalFilter.relaxation);
    MANUMESH_READ_FEATURE_BOOL(graph_consolidation_enabled, graphConsolidation.enabled);
    MANUMESH_READ_FEATURE_OPTION(graph_consolidation_gap_length_ratio, graphConsolidation.maxGapLengthRatio);
    MANUMESH_READ_FEATURE_OPTION(graph_consolidation_min_alignment, graphConsolidation.minAlignment);
#undef MANUMESH_READ_FEATURE_OPTION
#undef MANUMESH_READ_FEATURE_BOOL
    return true;
}

ManuMeshStatus initializeSimplifyOptions(ManuMeshSimplifyOptions* options, std::size_t structCapacity) {
    std::size_t writeSize = 0;
    const ManuMeshStatus status = initializeAbiBuffer(options, structCapacity, writeSize);
    if (status != MANUMESH_STATUS_OK) {
        return status;
    }

#define MANUMESH_INITIALIZE_OPTION(field, value)                                                                       \
    writeAbiField(options, writeSize, offsetof(ManuMeshSimplifyOptions, field), value)

    MANUMESH_INITIALIZE_OPTION(target_faces, -1);
    MANUMESH_INITIALIZE_OPTION(target_ratio, 0.25);
    MANUMESH_INITIALIZE_OPTION(use_line_quadrics, 1);
    MANUMESH_INITIALIZE_OPTION(line_weight, 1e-3);
    MANUMESH_INITIALIZE_OPTION(weight_mode, MANUMESH_WEIGHT_MODE_UNIFORM);
    MANUMESH_INITIALIZE_OPTION(feature_boost, 0.05);
    MANUMESH_INITIALIZE_OPTION(feature_angle_deg, 40.0);
    MANUMESH_INITIALIZE_OPTION(loop_trace_angle_deg, -1.0);
    MANUMESH_INITIALIZE_OPTION(adaptive_scale, 0);
    MANUMESH_INITIALIZE_OPTION(adaptive_base_line_weight, 1e-2);
    MANUMESH_INITIALIZE_OPTION(boundary_weight, 0.0);
    MANUMESH_INITIALIZE_OPTION(preserve_boundary, 0);
    MANUMESH_INITIALIZE_OPTION(preserve_feature_curves, 0);
    MANUMESH_INITIALIZE_OPTION(feature_curve_weight, 0.05);
    MANUMESH_INITIALIZE_OPTION(max_feature_curve_deviation_ratio, 0.0);
    MANUMESH_INITIALIZE_OPTION(circle_fit_relative_threshold, 0.05);
    MANUMESH_INITIALIZE_OPTION(ellipse_fit_relative_threshold, 0.05);
    MANUMESH_INITIALIZE_OPTION(near_circle_axis_ratio_tolerance, 0.08);
    MANUMESH_INITIALIZE_OPTION(min_feature_loop_vertices, 16);
    MANUMESH_INITIALIZE_OPTION(min_circular_feature_loop_vertices, 6);
    MANUMESH_INITIALIZE_OPTION(use_normal_tensor_features, 1);
    MANUMESH_INITIALIZE_OPTION(normal_tensor_feature_threshold, 0.16);
    MANUMESH_INITIALIZE_OPTION(normal_tensor_min_edge_alignment, 0.45);
    MANUMESH_INITIALIZE_OPTION(normal_tensor_smoothing_iterations, 0);
    MANUMESH_INITIALIZE_OPTION(normal_tensor_scale_count, 1);
    MANUMESH_INITIALIZE_OPTION(min_triangle_quality, 0.0);
    MANUMESH_INITIALIZE_OPTION(max_normal_deviation_deg, 90.0);
    MANUMESH_INITIALIZE_OPTION(max_local_error, 0.0);
    MANUMESH_INITIALIZE_OPTION(max_local_error_ratio, 0.0);
    MANUMESH_INITIALIZE_OPTION(prevent_local_intersections, 0);
    MANUMESH_INITIALIZE_OPTION(verbose, 0);
    MANUMESH_INITIALIZE_OPTION(feature_protection_mode, MANUMESH_FEATURE_PROTECTION_PRIMITIVE_CURVES);
    MANUMESH_INITIALIZE_OPTION(normal_tensor_min_persistent_scales, 1);
    MANUMESH_INITIALIZE_OPTION(cleanup_feature_graph, 1);
    MANUMESH_INITIALIZE_OPTION(feature_graph_gap_length_ratio, 1.25);
    MANUMESH_INITIALIZE_OPTION(feature_graph_max_weak_spur_edges, 2);
    MANUMESH_INITIALIZE_OPTION(feature_component_min_confidence, 0.35);
    MANUMESH_INITIALIZE_OPTION(quality_refinement_iterations, 0);
    MANUMESH_INITIALIZE_OPTION(use_smooth_curvature_features, 0);
    MANUMESH_INITIALIZE_OPTION(smooth_curvature_feature_threshold, 0.015);
    MANUMESH_INITIALIZE_OPTION(smooth_curvature_min_edge_alignment, 0.55);
    MANUMESH_INITIALIZE_OPTION(smooth_curvature_min_tangent_consistency, 0.65);
    MANUMESH_INITIALIZE_OPTION(smooth_curvature_base_neighborhood_rings, 2);
    MANUMESH_INITIALIZE_OPTION(smooth_curvature_scale_count, 3);
    MANUMESH_INITIALIZE_OPTION(smooth_curvature_min_persistent_scales, 2);
    MANUMESH_INITIALIZE_OPTION(smooth_curvature_robust_fit_iterations, 2);
    MANUMESH_INITIALIZE_OPTION(feature_graph_min_weak_spur_strength, 0.0);
    MANUMESH_INITIALIZE_OPTION(use_feature_normal_filter, 0);
    MANUMESH_INITIALIZE_OPTION(feature_normal_filter_iterations, 4);
    MANUMESH_INITIALIZE_OPTION(feature_normal_filter_angle_sigma_deg, 20.0);
    MANUMESH_INITIALIZE_OPTION(feature_normal_filter_preserve_angle_deg, 50.0);
    MANUMESH_INITIALIZE_OPTION(feature_normal_filter_relaxation, 0.8);
    MANUMESH_INITIALIZE_OPTION(smooth_curvature_use_stable_scale_selection, 0);
    MANUMESH_INITIALIZE_OPTION(smooth_curvature_min_scale_stability, 0.0);
    MANUMESH_INITIALIZE_OPTION(consolidate_feature_graph, 0);
    MANUMESH_INITIALIZE_OPTION(feature_graph_consolidation_gap_length_ratio, 3.0);
    MANUMESH_INITIALIZE_OPTION(feature_graph_consolidation_min_alignment, 0.75);
    MANUMESH_INITIALIZE_OPTION(feature_options, static_cast<const ManuMeshFeatureOptions*>(nullptr));

#undef MANUMESH_INITIALIZE_OPTION

    return MANUMESH_STATUS_OK;
}

ManuMeshStatus initializeSimplifyReport(ManuMeshSimplifyReport* report, std::size_t structCapacity) {
    std::size_t writeSize = 0;
    return initializeAbiBuffer(report, structCapacity, writeSize);
}

ManuMeshStatus initializeMeshStats(ManuMeshMeshStats* stats, std::size_t structCapacity) {
    std::size_t writeSize = 0;
    return initializeAbiBuffer(stats, structCapacity, writeSize);
}

namespace {

bool readSimplifyTargetAndCostOptions(
    const ManuMeshSimplifyOptions& source, simplification::SimplifyOptions& target, std::string& error
) {
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, target_faces)) {
        target.targetFaces = source.target_faces;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, target_ratio) &&
        !readFiniteDouble(source.target_ratio, "target_ratio", target.targetRatio, error)) {
        return false;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, use_line_quadrics)) {
        target.useLineQuadrics = boolFromInt(source.use_line_quadrics);
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, line_weight) &&
        !readFiniteDouble(source.line_weight, "line_weight", target.lineWeight, error)) {
        return false;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, weight_mode) &&
        !convertWeightMode(source.weight_mode, target.weightMode)) {
        error = "Unknown simplification weight mode.";
        return false;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, feature_boost) &&
        !readFiniteDouble(source.feature_boost, "feature_boost", target.featureBoost, error)) {
        return false;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, adaptive_scale)) {
        target.adaptiveScale = boolFromInt(source.adaptive_scale);
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, adaptive_base_line_weight) &&
        !readFiniteDouble(
            source.adaptive_base_line_weight, "adaptive_base_line_weight", target.adaptiveBaseLineWeight, error
        )) {
        return false;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, boundary_weight) &&
        !readFiniteDouble(source.boundary_weight, "boundary_weight", target.boundaryWeight, error)) {
        return false;
    }
    return true;
}

bool readSimplifyFeatureFields(
    const ManuMeshSimplifyOptions& source,
    bool hasFeatureOptionsOverride,
    simplification::SimplifyOptions& target,
    std::string& error
) {
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, feature_angle_deg, hasFeatureOptionsOverride) &&
        !readFiniteDouble(source.feature_angle_deg, "feature_angle_deg", target.featureAngleDeg, error)) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, loop_trace_angle_deg, hasFeatureOptionsOverride) &&
        !readFiniteDouble(source.loop_trace_angle_deg, "loop_trace_angle_deg", target.loopTraceAngleDeg, error)) {
        return false;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, preserve_feature_curves)) {
        target.preserveFeatureCurves = boolFromInt(source.preserve_feature_curves);
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, feature_protection_mode) &&
        !convertFeatureProtectionMode(source.feature_protection_mode, target.featureProtectionMode)) {
        error = "Unknown feature protection mode.";
        return false;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, feature_curve_weight) &&
        !readFiniteDouble(source.feature_curve_weight, "feature_curve_weight", target.featureCurveWeight, error)) {
        return false;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, max_feature_curve_deviation_ratio) &&
        !readFiniteDouble(
            source.max_feature_curve_deviation_ratio,
            "max_feature_curve_deviation_ratio",
            target.maxFeatureCurveDeviationRatio,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, circle_fit_relative_threshold, hasFeatureOptionsOverride) &&
        !readFiniteDouble(
            source.circle_fit_relative_threshold,
            "circle_fit_relative_threshold",
            target.circleFitRelativeThreshold,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, ellipse_fit_relative_threshold, hasFeatureOptionsOverride) &&
        !readFiniteDouble(
            source.ellipse_fit_relative_threshold,
            "ellipse_fit_relative_threshold",
            target.ellipseFitRelativeThreshold,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, near_circle_axis_ratio_tolerance, hasFeatureOptionsOverride) &&
        !readFiniteDouble(
            source.near_circle_axis_ratio_tolerance,
            "near_circle_axis_ratio_tolerance",
            target.nearCircleAxisRatioTolerance,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, min_feature_loop_vertices, hasFeatureOptionsOverride)) {
        target.minFeatureLoopVertices = source.min_feature_loop_vertices;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, min_circular_feature_loop_vertices)) {
        target.minCircularFeatureLoopVertices = source.min_circular_feature_loop_vertices;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, use_normal_tensor_features, hasFeatureOptionsOverride)) {
        target.useNormalTensorFeatures = boolFromInt(source.use_normal_tensor_features);
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, normal_tensor_feature_threshold, hasFeatureOptionsOverride) &&
        !readFiniteDouble(
            source.normal_tensor_feature_threshold,
            "normal_tensor_feature_threshold",
            target.normalTensorFeatureThreshold,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, normal_tensor_min_edge_alignment, hasFeatureOptionsOverride) &&
        !readFiniteDouble(
            source.normal_tensor_min_edge_alignment,
            "normal_tensor_min_edge_alignment",
            target.normalTensorMinEdgeAlignment,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, normal_tensor_smoothing_iterations, hasFeatureOptionsOverride)) {
        target.normalTensorSmoothingIterations = source.normal_tensor_smoothing_iterations;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, normal_tensor_scale_count, hasFeatureOptionsOverride)) {
        target.normalTensorScaleCount = source.normal_tensor_scale_count;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, normal_tensor_min_persistent_scales, hasFeatureOptionsOverride)) {
        target.normalTensorMinPersistentScales = source.normal_tensor_min_persistent_scales;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, use_smooth_curvature_features, hasFeatureOptionsOverride)) {
        target.useSmoothCurvatureFeatures = boolFromInt(source.use_smooth_curvature_features);
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, smooth_curvature_feature_threshold, hasFeatureOptionsOverride) &&
        !readFiniteDouble(
            source.smooth_curvature_feature_threshold,
            "smooth_curvature_feature_threshold",
            target.smoothCurvatureFeatureThreshold,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, smooth_curvature_min_edge_alignment, hasFeatureOptionsOverride) &&
        !readFiniteDouble(
            source.smooth_curvature_min_edge_alignment,
            "smooth_curvature_min_edge_alignment",
            target.smoothCurvatureMinEdgeAlignment,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(
            source, smooth_curvature_min_tangent_consistency, hasFeatureOptionsOverride
        ) &&
        !readFiniteDouble(
            source.smooth_curvature_min_tangent_consistency,
            "smooth_curvature_min_tangent_consistency",
            target.smoothCurvatureMinTangentConsistency,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(
            source, smooth_curvature_base_neighborhood_rings, hasFeatureOptionsOverride
        )) {
        target.smoothCurvatureBaseNeighborhoodRings = source.smooth_curvature_base_neighborhood_rings;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, smooth_curvature_scale_count, hasFeatureOptionsOverride)) {
        target.smoothCurvatureScaleCount = source.smooth_curvature_scale_count;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(
            source, smooth_curvature_min_persistent_scales, hasFeatureOptionsOverride
        )) {
        target.smoothCurvatureMinPersistentScales = source.smooth_curvature_min_persistent_scales;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(
            source, smooth_curvature_robust_fit_iterations, hasFeatureOptionsOverride
        )) {
        target.smoothCurvatureRobustFitIterations = source.smooth_curvature_robust_fit_iterations;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, cleanup_feature_graph, hasFeatureOptionsOverride)) {
        target.cleanupFeatureGraph = boolFromInt(source.cleanup_feature_graph);
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, feature_graph_gap_length_ratio, hasFeatureOptionsOverride) &&
        !readFiniteDouble(
            source.feature_graph_gap_length_ratio,
            "feature_graph_gap_length_ratio",
            target.featureGraphGapLengthRatio,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, feature_graph_max_weak_spur_edges, hasFeatureOptionsOverride)) {
        target.featureGraphMaxWeakSpurEdges = source.feature_graph_max_weak_spur_edges;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(
            source, feature_graph_min_weak_spur_strength, hasFeatureOptionsOverride
        ) &&
        !readFiniteDouble(
            source.feature_graph_min_weak_spur_strength,
            "feature_graph_min_weak_spur_strength",
            target.featureGraphMinWeakSpurStrength,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, feature_component_min_confidence, hasFeatureOptionsOverride) &&
        !readFiniteDouble(
            source.feature_component_min_confidence,
            "feature_component_min_confidence",
            target.featureComponentMinConfidence,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, use_feature_normal_filter, hasFeatureOptionsOverride)) {
        target.useFeatureNormalFilter = boolFromInt(source.use_feature_normal_filter);
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, feature_normal_filter_iterations, hasFeatureOptionsOverride)) {
        target.featureNormalFilterIterations = source.feature_normal_filter_iterations;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(
            source, feature_normal_filter_angle_sigma_deg, hasFeatureOptionsOverride
        ) &&
        !readFiniteDouble(
            source.feature_normal_filter_angle_sigma_deg,
            "feature_normal_filter_angle_sigma_deg",
            target.featureNormalFilterAngleSigmaDeg,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(
            source, feature_normal_filter_preserve_angle_deg, hasFeatureOptionsOverride
        ) &&
        !readFiniteDouble(
            source.feature_normal_filter_preserve_angle_deg,
            "feature_normal_filter_preserve_angle_deg",
            target.featureNormalFilterPreserveAngleDeg,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, feature_normal_filter_relaxation, hasFeatureOptionsOverride) &&
        !readFiniteDouble(
            source.feature_normal_filter_relaxation,
            "feature_normal_filter_relaxation",
            target.featureNormalFilterRelaxation,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(
            source, smooth_curvature_use_stable_scale_selection, hasFeatureOptionsOverride
        )) {
        target.smoothCurvatureUseStableScaleSelection = boolFromInt(source.smooth_curvature_use_stable_scale_selection);
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(
            source, smooth_curvature_min_scale_stability, hasFeatureOptionsOverride
        ) &&
        !readFiniteDouble(
            source.smooth_curvature_min_scale_stability,
            "smooth_curvature_min_scale_stability",
            target.smoothCurvatureMinScaleStability,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(source, consolidate_feature_graph, hasFeatureOptionsOverride)) {
        target.consolidateFeatureGraph = boolFromInt(source.consolidate_feature_graph);
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(
            source, feature_graph_consolidation_gap_length_ratio, hasFeatureOptionsOverride
        ) &&
        !readFiniteDouble(
            source.feature_graph_consolidation_gap_length_ratio,
            "feature_graph_consolidation_gap_length_ratio",
            target.featureGraphConsolidationGapLengthRatio,
            error
        )) {
        return false;
    }
    if (MANUMESH_LEGACY_FEATURE_FIELD_PRESENT(
            source, feature_graph_consolidation_min_alignment, hasFeatureOptionsOverride
        ) &&
        !readFiniteDouble(
            source.feature_graph_consolidation_min_alignment,
            "feature_graph_consolidation_min_alignment",
            target.featureGraphConsolidationMinAlignment,
            error
        )) {
        return false;
    }
    return true;
}

bool readSimplifyQualityFields(
    const ManuMeshSimplifyOptions& source, simplification::SimplifyOptions& target, std::string& error
) {
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, preserve_boundary)) {
        target.preserveBoundary = boolFromInt(source.preserve_boundary);
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, quality_refinement_iterations)) {
        target.qualityRefinementIterations = source.quality_refinement_iterations;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, min_triangle_quality) &&
        !readFiniteDouble(source.min_triangle_quality, "min_triangle_quality", target.minTriangleQuality, error)) {
        return false;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, max_normal_deviation_deg) &&
        !readFiniteDouble(
            source.max_normal_deviation_deg, "max_normal_deviation_deg", target.maxNormalDeviationDeg, error
        )) {
        return false;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, max_local_error) &&
        !readFiniteDouble(source.max_local_error, "max_local_error", target.maxLocalError, error)) {
        return false;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, max_local_error_ratio) &&
        !readFiniteDouble(source.max_local_error_ratio, "max_local_error_ratio", target.maxLocalErrorRatio, error)) {
        return false;
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, prevent_local_intersections)) {
        target.preventLocalIntersections = boolFromInt(source.prevent_local_intersections);
    }
    if (MANUMESH_SIMPLIFY_FIELD_PRESENT(source, verbose)) {
        target.verbose = boolFromInt(source.verbose);
    }
    return true;
}

} // namespace

bool readSimplifyOptions(
    const ManuMeshSimplifyOptions& source, simplification::SimplifyOptions& target, std::string& error
) {
    target = simplification::SimplifyOptions{};
    error.clear();
    if (!abiStructLooksInitialized(source)) {
        error = "ManuMeshSimplifyOptions must be initialized with "
                "manumesh_simplify_options_init for this ABI version.";
        return false;
    }

    const bool hasFeatureOptionsOverride =
        MANUMESH_SIMPLIFY_FIELD_PRESENT(source, feature_options) && source.feature_options != nullptr;
    if (hasFeatureOptionsOverride) {
        feature::FeatureOptions featureOptions;
        if (!readFeatureOptions(source.feature_options, featureOptions, error)) {
            return false;
        }
        target.featureOptionsOverride = featureOptions;
    }

    return readSimplifyTargetAndCostOptions(source, target, error) &&
           readSimplifyFeatureFields(source, hasFeatureOptionsOverride, target, error) &&
           readSimplifyQualityFields(source, target, error);
}

bool validateSimplifyReportOutput(
    const ManuMeshSimplifyReport* target, std::size_t structCapacity, std::string& error
) {
    return validateOutputCapacity(target, structCapacity, true, "ManuMeshSimplifyReport", error);
}

bool validateMeshStatsOutput(const ManuMeshMeshStats* target, std::size_t structCapacity, std::string& error) {
    return validateOutputCapacity(target, structCapacity, false, "ManuMeshMeshStats", error);
}

ManuMeshStatus fillSimplifyReport(
    const simplification::SimplifyReport& source, ManuMeshSimplifyReport* output, std::size_t structCapacity
) {
    std::size_t writeSize = 0;
    const ManuMeshStatus status = initializeAbiBuffer(output, structCapacity, writeSize);
    if (status != MANUMESH_STATUS_OK) {
        return status;
    }
    ManuMeshSimplifyReport& target = *output;
    MANUMESH_SET_REPORT_FIELD(target, writeSize, initial_vertices, source.initialVertices);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, initial_faces, source.initialFaces);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, final_vertices, source.finalVertices);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, final_faces, source.finalFaces);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, collapsed_edges, source.collapsedEdges);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, rejected_collapses, source.rejectedCollapses);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, solver_fallbacks, source.solverFallbacks);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, queue_rebuilds, source.queueRebuilds);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, feature_loops, source.featureLoops);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, circular_feature_loops, source.circularFeatureLoops);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, feature_vertices, source.featureVertices);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, normal_tensor_feature_edges, source.normalTensorFeatureEdges);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, feature_rejected_collapses, source.featureRejectedCollapses);
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, primitive_feature_rejected_collapses, source.primitiveFeatureRejectedCollapses
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, generic_feature_rejected_collapses, source.genericFeatureRejectedCollapses
    );
    MANUMESH_SET_REPORT_FIELD(target, writeSize, boundary_rejected_collapses, source.boundaryRejectedCollapses);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, topology_rejected_collapses, source.topologyRejectedCollapses);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, normal_flip_rejected_collapses, source.normalFlipRejectedCollapses);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, quality_rejected_collapses, source.qualityRejectedCollapses);
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, self_intersection_rejected_collapses, source.selfIntersectionRejectedCollapses
    );
    MANUMESH_SET_REPORT_FIELD(target, writeSize, curve_budget_rejected_collapses, source.curveBudgetRejectedCollapses);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, error_rejected_collapses, source.errorRejectedCollapses);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, projected_feature_placements, source.projectedFeaturePlacements);
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, termination_reason, convertTerminationReason(source.terminationReason)
    );
    MANUMESH_SET_REPORT_FIELD(target, writeSize, min_applied_line_weight, source.minAppliedLineWeight);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, max_applied_line_weight, source.maxAppliedLineWeight);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, traced_feature_edges, source.tracedFeatureEdges);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, untraced_feature_edges, source.untracedFeatureEdges);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, normal_tensor_scored_vertices, source.normalTensorScoredVertices);
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, max_normal_tensor_persistent_score, source.maxNormalTensorPersistentScore
    );
    MANUMESH_SET_REPORT_FIELD(target, writeSize, mean_normal_tensor_local_scale, source.meanNormalTensorLocalScale);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, mean_normal_tensor_persistence, source.meanNormalTensorPersistence);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, feature_components, source.featureComponents);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, weak_feature_components, source.weakFeatureComponents);
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, high_confidence_feature_components, source.highConfidenceFeatureComponents
    );
    MANUMESH_SET_REPORT_FIELD(target, writeSize, graph_cleanup_bridged_gaps, source.graphCleanupBridgedGaps);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, graph_cleanup_removed_spurs, source.graphCleanupRemovedSpurs);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, graph_cleanup_merged_junctions, source.graphCleanupMergedJunctions);
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, mean_feature_component_confidence, source.meanFeatureComponentConfidence
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, min_feature_component_confidence, source.minFeatureComponentConfidence
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, quality_refinement_iterations_completed, source.qualityRefinementIterationsCompleted
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, quality_refinement_attempted_moves, source.qualityRefinementAttemptedMoves
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, quality_refinement_accepted_moves, source.qualityRefinementAcceptedMoves
    );
    MANUMESH_SET_REPORT_FIELD(target, writeSize, degenerate_input_faces, source.degenerateInputFaces);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, smooth_curvature_feature_edges, source.smoothCurvatureFeatureEdges);
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, smooth_curvature_scored_vertices, source.smoothCurvatureScoredVertices
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, max_smooth_curvature_persistent_score, source.maxSmoothCurvaturePersistentScore
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, mean_smooth_curvature_local_scale, source.meanSmoothCurvatureLocalScale
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, mean_smooth_curvature_persistence, source.meanSmoothCurvaturePersistence
    );
    MANUMESH_SET_REPORT_FIELD(target, writeSize, inconsistent_winding_edges, source.inconsistentWindingEdges);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, graph_cleanup_skipped_by_cap, source.graphCleanupSkippedByCap);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, circular_recovery_truncated, source.circularRecoveryTruncated);
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, feature_normal_filter_iterations_completed, source.featureNormalFilterIterationsCompleted
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, feature_normal_filter_changed_faces, source.featureNormalFilterChangedFaces
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, feature_normal_filter_preserved_edges, source.featureNormalFilterPreservedEdges
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, mean_feature_normal_filter_angular_change_deg, source.meanFeatureNormalFilterAngularChangeDeg
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, max_feature_normal_filter_angular_change_deg, source.maxFeatureNormalFilterAngularChangeDeg
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, mean_feature_normal_filter_edge_indicator, source.meanFeatureNormalFilterEdgeIndicator
    );
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, mean_smooth_curvature_scale_stability, source.meanSmoothCurvatureScaleStability
    );
    MANUMESH_SET_REPORT_FIELD(target, writeSize, graph_consolidation_bridges, source.graphConsolidationBridges);
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, graph_consolidation_skipped_by_cap, source.graphConsolidationSkippedByCap
    );
    MANUMESH_SET_REPORT_FIELD(target, writeSize, junction_branch_pairs, source.junctionBranchPairs);
    MANUMESH_SET_REPORT_FIELD(target, writeSize, ambiguous_feature_junctions, source.ambiguousFeatureJunctions);
    MANUMESH_SET_REPORT_FIELD(
        target, writeSize, quality_refinement_skipped_for_texture, source.qualityRefinementSkippedForTexture ? 1 : 0
    );
    return MANUMESH_STATUS_OK;
}

ManuMeshStatus fillMeshStats(const analysis::MeshStats& source, ManuMeshMeshStats* output, std::size_t structCapacity) {
    std::size_t writeSize = 0;
    const ManuMeshStatus status = initializeAbiBuffer(output, structCapacity, writeSize);
    if (status != MANUMESH_STATUS_OK) {
        return status;
    }
    ManuMeshMeshStats& target = *output;
    MANUMESH_SET_STATS_FIELD(target, writeSize, vertices, source.vertices);
    MANUMESH_SET_STATS_FIELD(target, writeSize, faces, source.faces);
    MANUMESH_SET_STATS_FIELD(target, writeSize, edges, source.edges);
    MANUMESH_SET_STATS_FIELD(target, writeSize, boundary_edges, source.boundaryEdges);
    MANUMESH_SET_STATS_FIELD(target, writeSize, non_manifold_edges, source.nonManifoldEdges);
    MANUMESH_SET_STATS_FIELD(target, writeSize, area, source.area);
    MANUMESH_SET_STATS_FIELD(target, writeSize, mean_triangle_quality, source.meanTriangleQuality);
    MANUMESH_SET_STATS_FIELD(target, writeSize, min_triangle_quality, source.minTriangleQuality);
    MANUMESH_SET_STATS_FIELD(target, writeSize, mean_edge_length, source.meanEdgeLength);
    MANUMESH_SET_STATS_FIELD(target, writeSize, edge_length_cv, source.edgeLengthCv);
    return MANUMESH_STATUS_OK;
}

} // namespace api
} // namespace manumesh
