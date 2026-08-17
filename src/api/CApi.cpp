/**
 * @file src/api/CApi.cpp
 * @brief 实现稳定 C ABI 的句柄、错误边界和算法入口。
 * @ingroup manumesh_c_api
 *
 * @details C 边界负责校验指针和容量，将失败转换为状态码，并且不允许 C++ 异常穿过 ABI。
 */

#include "api/CApi.h"

#include "algorithms/analysis/MeshAnalysis.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "api/detail/CApiConverters.h"
#include "core/Filesystem.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "core/Tolerances.h"
#include "io/MeshIo.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef MANUMESH_VERSION
#define MANUMESH_VERSION "0.0.0"
#endif

/** @brief 保存最新诊断消息的不透明 C API 上下文。*/
struct ManuMeshContext {
    std::string lastError;
};

/** @brief 持有一个可变 C++ Mesh 值的不透明 C API 句柄。*/
struct ManuMeshMeshHandle {
    mutable std::mutex mutex;
    manumesh::Mesh mesh;
};

namespace {

bool hasSupportedMeshExtension(const char* path) {
    std::string extension = manumesh::filesystem::u8path(path).extension().u8string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension == ".stl" || extension == ".obj";
}

/** @brief 独立特征选项无容量初始化符号首次发布时的完整 ABI-v1 布局。 */
struct LegacyV1FeatureOptionsLayout {
    std::size_t struct_size;
    unsigned int abi_version;
    double feature_angle_deg;
    double loop_trace_angle_deg;
    double circle_fit_relative_threshold;
    double ellipse_fit_relative_threshold;
    double near_circle_axis_ratio_tolerance;
    int min_feature_loop_vertices;
    int use_normal_tensor_features;
    double normal_tensor_feature_threshold;
    double normal_tensor_min_edge_alignment;
    int normal_tensor_smoothing_iterations;
    int normal_tensor_scale_count;
    int normal_tensor_min_persistent_scales;
    int use_smooth_curvature_features;
    double smooth_curvature_feature_threshold;
    double smooth_curvature_min_edge_alignment;
    double smooth_curvature_min_tangent_consistency;
    int smooth_curvature_base_neighborhood_rings;
    int smooth_curvature_scale_count;
    int smooth_curvature_min_persistent_scales;
    int smooth_curvature_robust_fit_iterations;
    int smooth_curvature_use_stable_scale_selection;
    double smooth_curvature_min_scale_stability;
    int cleanup_feature_graph;
    double feature_graph_gap_length_ratio;
    int feature_graph_max_weak_spur_edges;
    double feature_graph_min_weak_spur_strength;
    double feature_component_min_confidence;
    int normal_filter_enabled;
    int normal_filter_iterations;
    double normal_filter_angle_sigma_deg;
    double normal_filter_preserve_angle_deg;
    double normal_filter_relaxation;
    int graph_consolidation_enabled;
    double graph_consolidation_gap_length_ratio;
    double graph_consolidation_min_alignment;
};

/** @brief 特征边数组元素首次发布时的固定 ABI-v1 布局。 */
struct LegacyV1FeatureEdgeLayout {
    int a;
    int b;
    int boundary;
    int dihedral;
    int normal_tensor;
    int smooth_curvature;
    int non_manifold;
    int cleanup_bridge;
    int consolidation_bridge;
    int removed_by_cleanup;
    int signed_kind;
};

/** @brief ABI-v2 特征边的固定布局镜像。 */
struct FeatureEdgeV2Layout {
    int a;
    int b;
    int boundary;
    int dihedral;
    int normal_tensor;
    int smooth_curvature;
    int non_manifold;
    int cleanup_bridge;
    int consolidation_bridge;
    int removed_by_cleanup;
    int signed_kind;
    std::uint64_t feature_edge_index;
    std::uint64_t input_edge_index;
    int synthetic;
    int geometric_constraint;
};

/** @brief 用于兼容旧版选项结构的首发版本前缀布局。*/
struct LegacyV1SimplifyOptionsLayout {
    std::size_t struct_size;
    unsigned int abi_version;
    int target_faces;
    double target_ratio;
    int use_line_quadrics;
    double line_weight;
    ManuMeshWeightMode weight_mode;
    double feature_boost;
    double feature_angle_deg;
    int adaptive_scale;
    double adaptive_base_line_weight;
    double boundary_weight;
    int preserve_boundary;
    int preserve_feature_curves;
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
    ManuMeshFeatureProtectionMode feature_protection_mode;
};

/** @brief 用于有界写入简化报告的首发版本前缀布局。*/
struct LegacyV1SimplifyReportLayout {
    std::size_t struct_size;
    unsigned int abi_version;
    int initial_vertices;
    int initial_faces;
    int final_vertices;
    int final_faces;
    int collapsed_edges;
    int rejected_collapses;
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
    ManuMeshSimplifyTerminationReason termination_reason;
    double min_applied_line_weight;
    double max_applied_line_weight;
};

/** @brief 用于有界写入网格统计的首发版本前缀布局。*/
struct LegacyV1MeshStatsLayout {
    std::size_t struct_size;
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
};

#define MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(field)                                                                \
    static_assert(                                                                                                     \
        offsetof(LegacyV1FeatureOptionsLayout, field) == offsetof(ManuMeshFeatureOptions, field),                      \
        "ManuMeshFeatureOptions v1 field layout changed: " #field                                                      \
    )
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(struct_size);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(abi_version);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(feature_angle_deg);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(loop_trace_angle_deg);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(circle_fit_relative_threshold);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(ellipse_fit_relative_threshold);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(near_circle_axis_ratio_tolerance);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(min_feature_loop_vertices);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(use_normal_tensor_features);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(normal_tensor_feature_threshold);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(normal_tensor_min_edge_alignment);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(normal_tensor_smoothing_iterations);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(normal_tensor_scale_count);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(normal_tensor_min_persistent_scales);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(use_smooth_curvature_features);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(smooth_curvature_feature_threshold);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(smooth_curvature_min_edge_alignment);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(smooth_curvature_min_tangent_consistency);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(smooth_curvature_base_neighborhood_rings);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(smooth_curvature_scale_count);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(smooth_curvature_min_persistent_scales);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(smooth_curvature_robust_fit_iterations);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(smooth_curvature_use_stable_scale_selection);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(smooth_curvature_min_scale_stability);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(cleanup_feature_graph);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(feature_graph_gap_length_ratio);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(feature_graph_max_weak_spur_edges);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(feature_graph_min_weak_spur_strength);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(feature_component_min_confidence);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(normal_filter_enabled);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(normal_filter_iterations);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(normal_filter_angle_sigma_deg);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(normal_filter_preserve_angle_deg);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(normal_filter_relaxation);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(graph_consolidation_enabled);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(graph_consolidation_gap_length_ratio);
MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD(graph_consolidation_min_alignment);
#undef MANUMESH_ASSERT_FEATURE_OPTIONS_V1_FIELD

#define MANUMESH_ASSERT_FEATURE_EDGE_V1_FIELD(field)                                                                   \
    static_assert(                                                                                                     \
        offsetof(LegacyV1FeatureEdgeLayout, field) == offsetof(ManuMeshFeatureEdge, field),                            \
        "ManuMeshFeatureEdge v1 field layout changed: " #field                                                         \
    )
MANUMESH_ASSERT_FEATURE_EDGE_V1_FIELD(a);
MANUMESH_ASSERT_FEATURE_EDGE_V1_FIELD(b);
MANUMESH_ASSERT_FEATURE_EDGE_V1_FIELD(boundary);
MANUMESH_ASSERT_FEATURE_EDGE_V1_FIELD(dihedral);
MANUMESH_ASSERT_FEATURE_EDGE_V1_FIELD(normal_tensor);
MANUMESH_ASSERT_FEATURE_EDGE_V1_FIELD(smooth_curvature);
MANUMESH_ASSERT_FEATURE_EDGE_V1_FIELD(non_manifold);
MANUMESH_ASSERT_FEATURE_EDGE_V1_FIELD(cleanup_bridge);
MANUMESH_ASSERT_FEATURE_EDGE_V1_FIELD(consolidation_bridge);
MANUMESH_ASSERT_FEATURE_EDGE_V1_FIELD(removed_by_cleanup);
MANUMESH_ASSERT_FEATURE_EDGE_V1_FIELD(signed_kind);
#undef MANUMESH_ASSERT_FEATURE_EDGE_V1_FIELD

static_assert(
    sizeof(LegacyV1FeatureOptionsLayout) ==
        offsetof(ManuMeshFeatureOptions, graph_consolidation_min_alignment) + sizeof(double),
    "ManuMeshFeatureOptions v1 prefix size changed"
);
static_assert(
    sizeof(LegacyV1FeatureOptionsLayout) <= sizeof(ManuMeshFeatureOptions),
    "ManuMeshFeatureOptions is smaller than its first published ABI-v1 layout"
);
static_assert(
    sizeof(LegacyV1FeatureEdgeLayout) == sizeof(ManuMeshFeatureEdge),
    "ManuMeshFeatureEdge v1 array element size changed"
);
static_assert(sizeof(FeatureEdgeV2Layout) == sizeof(ManuMeshFeatureEdgeV2), "ManuMeshFeatureEdgeV2 layout changed");
static_assert(sizeof(ManuMeshVec2) == 16, "ManuMeshVec2 ABI size changed");
static_assert(sizeof(ManuMeshVec3) == 24, "ManuMeshVec3 ABI size changed");
static_assert(sizeof(ManuMeshFace) == 12, "ManuMeshFace ABI size changed");
static_assert(sizeof(ManuMeshFaceTexCoords) == 56, "ManuMeshFaceTexCoords ABI size changed");
static_assert(offsetof(ManuMeshFaceTexCoords, valid) == 48, "ManuMeshFaceTexCoords layout changed");
static_assert(sizeof(ManuMeshEdge) == 24, "ManuMeshEdge ABI size changed");
static_assert(offsetof(ManuMeshEdge, face_count) == 8, "ManuMeshEdge layout changed");
static_assert(sizeof(ManuMeshBounds) == 56, "ManuMeshBounds ABI size changed");
static_assert(offsetof(ManuMeshBounds, valid) == 48, "ManuMeshBounds layout changed");
static_assert(sizeof(ManuMeshTopologySummary) == 40, "ManuMeshTopologySummary ABI size changed");
static_assert(
    offsetof(ManuMeshTopologySummary, closed_manifold) == 32, "ManuMeshTopologySummary closed_manifold layout changed"
);
#define MANUMESH_ASSERT_FEATURE_EDGE_V2_PREFIX_FIELD(field)                                                            \
    static_assert(                                                                                                     \
        offsetof(ManuMeshFeatureEdgeV2, field) == offsetof(ManuMeshFeatureEdge, field),                                \
        "ManuMeshFeatureEdgeV2 must preserve the v1 prefix field offset: " #field                                      \
    )
MANUMESH_ASSERT_FEATURE_EDGE_V2_PREFIX_FIELD(a);
MANUMESH_ASSERT_FEATURE_EDGE_V2_PREFIX_FIELD(b);
MANUMESH_ASSERT_FEATURE_EDGE_V2_PREFIX_FIELD(boundary);
MANUMESH_ASSERT_FEATURE_EDGE_V2_PREFIX_FIELD(dihedral);
MANUMESH_ASSERT_FEATURE_EDGE_V2_PREFIX_FIELD(normal_tensor);
MANUMESH_ASSERT_FEATURE_EDGE_V2_PREFIX_FIELD(smooth_curvature);
MANUMESH_ASSERT_FEATURE_EDGE_V2_PREFIX_FIELD(non_manifold);
MANUMESH_ASSERT_FEATURE_EDGE_V2_PREFIX_FIELD(cleanup_bridge);
MANUMESH_ASSERT_FEATURE_EDGE_V2_PREFIX_FIELD(consolidation_bridge);
MANUMESH_ASSERT_FEATURE_EDGE_V2_PREFIX_FIELD(removed_by_cleanup);
MANUMESH_ASSERT_FEATURE_EDGE_V2_PREFIX_FIELD(signed_kind);
#undef MANUMESH_ASSERT_FEATURE_EDGE_V2_PREFIX_FIELD
static_assert(
    offsetof(ManuMeshFeatureEdgeV2, feature_edge_index) == offsetof(FeatureEdgeV2Layout, feature_edge_index),
    "ManuMeshFeatureEdgeV2 feature_edge_index offset changed"
);
static_assert(
    offsetof(ManuMeshFeatureEdgeV2, input_edge_index) == offsetof(FeatureEdgeV2Layout, input_edge_index),
    "ManuMeshFeatureEdgeV2 input_edge_index offset changed"
);
#define MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(field)                                                               \
    static_assert(                                                                                                     \
        offsetof(LegacyV1SimplifyOptionsLayout, field) == offsetof(ManuMeshSimplifyOptions, field),                    \
        "ManuMeshSimplifyOptions v1 field layout changed: " #field                                                     \
    )
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(struct_size);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(abi_version);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(target_faces);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(target_ratio);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(use_line_quadrics);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(line_weight);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(weight_mode);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(feature_boost);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(feature_angle_deg);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(adaptive_scale);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(adaptive_base_line_weight);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(boundary_weight);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(preserve_boundary);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(preserve_feature_curves);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(feature_curve_weight);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(max_feature_curve_deviation_ratio);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(circle_fit_relative_threshold);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(ellipse_fit_relative_threshold);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(near_circle_axis_ratio_tolerance);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(min_feature_loop_vertices);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(min_circular_feature_loop_vertices);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(use_normal_tensor_features);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(normal_tensor_feature_threshold);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(normal_tensor_min_edge_alignment);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(normal_tensor_smoothing_iterations);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(normal_tensor_scale_count);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(min_triangle_quality);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(max_normal_deviation_deg);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(max_local_error);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(max_local_error_ratio);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(prevent_local_intersections);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(verbose);
MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD(feature_protection_mode);
#undef MANUMESH_ASSERT_SIMPLIFY_OPTIONS_V1_FIELD

static_assert(
    sizeof(LegacyV1SimplifyOptionsLayout) == offsetof(ManuMeshSimplifyOptions, loop_trace_angle_deg),
    "ManuMeshSimplifyOptions v1 prefix layout changed"
);
static_assert(
    offsetof(ManuMeshSimplifyOptions, min_texture_area_ratio) +
            sizeof(((ManuMeshSimplifyOptions*)nullptr)->min_texture_area_ratio) ==
        sizeof(ManuMeshSimplifyOptions),
    "ManuMeshSimplifyOptions min_texture_area_ratio must remain the final ABI field"
);

#define MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(field)                                                                \
    static_assert(                                                                                                     \
        offsetof(LegacyV1SimplifyReportLayout, field) == offsetof(ManuMeshSimplifyReport, field),                      \
        "ManuMeshSimplifyReport v1 field layout changed: " #field                                                      \
    )
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(struct_size);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(abi_version);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(initial_vertices);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(initial_faces);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(final_vertices);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(final_faces);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(collapsed_edges);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(rejected_collapses);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(solver_fallbacks);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(queue_rebuilds);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(feature_loops);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(circular_feature_loops);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(feature_vertices);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(normal_tensor_feature_edges);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(feature_rejected_collapses);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(primitive_feature_rejected_collapses);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(generic_feature_rejected_collapses);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(boundary_rejected_collapses);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(topology_rejected_collapses);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(normal_flip_rejected_collapses);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(quality_rejected_collapses);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(self_intersection_rejected_collapses);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(curve_budget_rejected_collapses);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(error_rejected_collapses);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(projected_feature_placements);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(termination_reason);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(min_applied_line_weight);
MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD(max_applied_line_weight);
#undef MANUMESH_ASSERT_SIMPLIFY_REPORT_V1_FIELD

static_assert(
    sizeof(LegacyV1SimplifyReportLayout) == offsetof(ManuMeshSimplifyReport, traced_feature_edges),
    "ManuMeshSimplifyReport v1 prefix layout changed"
);

#define MANUMESH_ASSERT_MESH_STATS_V1_FIELD(field)                                                                     \
    static_assert(                                                                                                     \
        offsetof(LegacyV1MeshStatsLayout, field) == offsetof(ManuMeshMeshStats, field),                                \
        "ManuMeshMeshStats v1 field layout changed: " #field                                                           \
    )
MANUMESH_ASSERT_MESH_STATS_V1_FIELD(struct_size);
MANUMESH_ASSERT_MESH_STATS_V1_FIELD(abi_version);
MANUMESH_ASSERT_MESH_STATS_V1_FIELD(vertices);
MANUMESH_ASSERT_MESH_STATS_V1_FIELD(faces);
MANUMESH_ASSERT_MESH_STATS_V1_FIELD(edges);
MANUMESH_ASSERT_MESH_STATS_V1_FIELD(boundary_edges);
MANUMESH_ASSERT_MESH_STATS_V1_FIELD(non_manifold_edges);
MANUMESH_ASSERT_MESH_STATS_V1_FIELD(area);
MANUMESH_ASSERT_MESH_STATS_V1_FIELD(mean_triangle_quality);
MANUMESH_ASSERT_MESH_STATS_V1_FIELD(min_triangle_quality);
MANUMESH_ASSERT_MESH_STATS_V1_FIELD(mean_edge_length);
MANUMESH_ASSERT_MESH_STATS_V1_FIELD(edge_length_cv);
#undef MANUMESH_ASSERT_MESH_STATS_V1_FIELD

constexpr std::size_t kLegacyV1FeatureOptionsSize = sizeof(LegacyV1FeatureOptionsLayout);
constexpr std::size_t kLegacyV1SimplifyOptionsSize = sizeof(LegacyV1SimplifyOptionsLayout);
constexpr std::size_t kLegacyV1SimplifyReportSize = sizeof(LegacyV1SimplifyReportLayout);
constexpr std::size_t kLegacyV1MeshStatsSize = sizeof(LegacyV1MeshStatsLayout);

bool isExportableFeatureEdge(const manumesh::feature::FeatureGraphEdge& edge, std::size_t vertexCount) {
    return !edge.removedByCleanup && edge.a >= 0 && edge.b >= 0 && edge.a != edge.b &&
           static_cast<std::size_t>(edge.a) < vertexCount && static_cast<std::size_t>(edge.b) < vertexCount;
}

void copyFeatureEdge(const manumesh::feature::FeatureGraphEdge& source, ManuMeshFeatureEdge& target) {
    target.a = source.a;
    target.b = source.b;
    target.boundary = source.boundary ? 1 : 0;
    target.dihedral = source.dihedral ? 1 : 0;
    target.normal_tensor = source.normalTensor ? 1 : 0;
    target.smooth_curvature = source.smoothCurvature ? 1 : 0;
    target.non_manifold = source.nonManifold ? 1 : 0;
    target.cleanup_bridge = source.cleanupBridge ? 1 : 0;
    target.consolidation_bridge = source.consolidationBridge ? 1 : 0;
    target.removed_by_cleanup = source.removedByCleanup ? 1 : 0;
    target.signed_kind = source.signedKind;
}

struct IndexedFeatureEdge {
    const manumesh::feature::FeatureGraphEdge* source = nullptr;
    int first = -1;
    int second = -1;
    std::uint64_t inputEdgeIndex = MANUMESH_INVALID_EDGE_INDEX;
    std::size_t graphEdgeIndex = 0;
};

std::vector<IndexedFeatureEdge>
buildIndexedFeatureEdges(const manumesh::Mesh& mesh, const manumesh::feature::FeatureAnalysis& analysis) {
    const std::vector<std::pair<int, int>> inputEdges = manumesh::uniqueEdges(mesh);
    std::vector<IndexedFeatureEdge> indexed;
    indexed.reserve(analysis.graph.edges.size());
    for (std::size_t graphEdgeIndex = 0; graphEdgeIndex < analysis.graph.edges.size(); ++graphEdgeIndex) {
        const manumesh::feature::FeatureGraphEdge& source = analysis.graph.edges[graphEdgeIndex];
        if (!isExportableFeatureEdge(source, mesh.vertices.size())) {
            continue;
        }
        IndexedFeatureEdge edge;
        edge.source = &source;
        edge.graphEdgeIndex = graphEdgeIndex;
        edge.first = std::min(source.a, source.b);
        edge.second = std::max(source.a, source.b);
        const std::pair<int, int> key(edge.first, edge.second);
        const auto inputIt = std::lower_bound(inputEdges.begin(), inputEdges.end(), key);
        if (inputIt != inputEdges.end() && *inputIt == key) {
            edge.inputEdgeIndex = static_cast<std::uint64_t>(inputIt - inputEdges.begin());
        }
        indexed.push_back(edge);
    }
    std::sort(indexed.begin(), indexed.end(), [](const IndexedFeatureEdge& lhs, const IndexedFeatureEdge& rhs) {
        const manumesh::feature::FeatureGraphEdge& a = *lhs.source;
        const manumesh::feature::FeatureGraphEdge& b = *rhs.source;
        return std::tie(
                   lhs.first,
                   lhs.second,
                   a.cleanupBridge,
                   a.consolidationBridge,
                   a.boundary,
                   a.dihedral,
                   a.normalTensor,
                   a.smoothCurvature,
                   a.nonManifold,
                   a.signedKind,
                   lhs.graphEdgeIndex
               ) <
               std::tie(
                   rhs.first,
                   rhs.second,
                   b.cleanupBridge,
                   b.consolidationBridge,
                   b.boundary,
                   b.dihedral,
                   b.normalTensor,
                   b.smoothCurvature,
                   b.nonManifold,
                   b.signedKind,
                   rhs.graphEdgeIndex
               );
    });
    return indexed;
}

void copyFeatureEdgeV2(const IndexedFeatureEdge& indexed, std::uint64_t outputIndex, ManuMeshFeatureEdgeV2& target) {
    const manumesh::feature::FeatureGraphEdge& source = *indexed.source;
    target.a = indexed.first;
    target.b = indexed.second;
    target.boundary = source.boundary ? 1 : 0;
    target.dihedral = source.dihedral ? 1 : 0;
    target.normal_tensor = source.normalTensor ? 1 : 0;
    target.smooth_curvature = source.smoothCurvature ? 1 : 0;
    target.non_manifold = source.nonManifold ? 1 : 0;
    target.cleanup_bridge = source.cleanupBridge ? 1 : 0;
    target.consolidation_bridge = source.consolidationBridge ? 1 : 0;
    target.removed_by_cleanup = source.removedByCleanup ? 1 : 0;
    target.signed_kind = source.signedKind;
    target.feature_edge_index = outputIndex;
    target.input_edge_index = indexed.inputEdgeIndex;
    target.synthetic = source.cleanupBridge || source.consolidationBridge ? 1 : 0;
    target.geometric_constraint = indexed.inputEdgeIndex != MANUMESH_INVALID_EDGE_INDEX ? 1 : 0;
}

void clearError(ManuMeshContext* context) noexcept {
    if (context) {
        context->lastError.clear();
    }
}

ManuMeshStatus fail(ManuMeshContext* context, ManuMeshStatus status, const char* message) noexcept {
    if (context) {
        // 字符串赋值可能分配内存；不得让内存不足或其他异常穿过 C ABI。
        // 失败时回退为空消息；clear() 不释放资源且不会抛出异常。
        try {
            context->lastError.assign(message ? message : "");
        } catch (...) {
            context->lastError.clear();
        }
    }
    return status;
}

ManuMeshStatus translateException(ManuMeshContext* context, const std::exception& ex) noexcept {
    if (dynamic_cast<const std::bad_alloc*>(&ex) != nullptr) {
        return fail(context, MANUMESH_STATUS_OUT_OF_MEMORY, ex.what());
    }
    if (dynamic_cast<const std::invalid_argument*>(&ex) != nullptr) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, ex.what());
    }
    return fail(context, MANUMESH_STATUS_ALGORITHM_ERROR, ex.what());
}

ManuMeshStatus translateUnknownException(ManuMeshContext* context) noexcept {
    return fail(context, MANUMESH_STATUS_ALGORITHM_ERROR, "Unknown C++ exception.");
}

template <typename Handle, typename Function>
ManuMeshStatus withMeshLock(ManuMeshContext* context, Handle* handle, Function&& function) noexcept {
    try {
        std::lock_guard<std::mutex> lock(handle->mutex);
        return function(handle->mesh);
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

template <typename Function>
ManuMeshStatus withTwoMeshLocks(
    ManuMeshContext* context, const ManuMeshMeshHandle* source, ManuMeshMeshHandle* destination, Function&& function
) noexcept {
    try {
        std::unique_lock<std::mutex> sourceLock(source->mutex, std::defer_lock);
        std::unique_lock<std::mutex> destinationLock(destination->mutex, std::defer_lock);
        std::lock(sourceLock, destinationLock);
        return function(source->mesh, destination->mesh);
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus
snapshotMesh(ManuMeshContext* context, const ManuMeshMeshHandle* handle, manumesh::Mesh& snapshot) noexcept {
    if (!handle) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
    }
    return withMeshLock(context, handle, [&](const manumesh::Mesh& mesh) {
        snapshot = mesh;
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus commitMesh(ManuMeshContext* context, ManuMeshMeshHandle* handle, manumesh::Mesh&& candidate) noexcept {
    if (!handle) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
    }
    return withMeshLock(context, handle, [&](manumesh::Mesh& mesh) {
        mesh = std::move(candidate);
        return MANUMESH_STATUS_OK;
    });
}

bool finiteVec3(const manumesh::Vec3& value) {
    return std::isfinite(value.x()) && std::isfinite(value.y()) && std::isfinite(value.z());
}

bool finiteCVec3(const ManuMeshVec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool finiteTexcoords(const ManuMeshFaceTexCoords& texcoords) {
    if (!texcoords.valid) {
        return true;
    }
    for (const ManuMeshVec2& uv : texcoords.uv) {
        if (!std::isfinite(uv.u) || !std::isfinite(uv.v)) {
            return false;
        }
    }
    return true;
}

bool isBinaryStlRepresentabilityError(const std::string& error) {
    return error == "Binary STL supports at most UINT32_MAX triangles." ||
           error == "Mesh face becomes degenerate after binary STL float32 conversion." ||
           error.find("outside the binary STL float32 range.") != std::string::npos;
}

// MeshIo 在写文件前会执行严格校验；C API 只需根据诊断前缀恢复原有状态码。
bool isMeshValidationError(const std::string& error) {
    return error.rfind("Mesh ", 0) == 0 || error.rfind("Per-corner ", 0) == 0 || error.rfind("Vertex count ", 0) == 0 ||
           error.rfind("Face count ", 0) == 0;
}

ManuMeshVec3 toCVec3(const manumesh::Vec3& value) { return ManuMeshVec3{value.x(), value.y(), value.z()}; }

ManuMeshFaceTexCoords zeroTexcoords() {
    ManuMeshFaceTexCoords result{};
    result.valid = 0;
    return result;
}

ManuMeshFaceTexCoords toCTexcoords(const manumesh::FaceTexCoords& value) {
    ManuMeshFaceTexCoords result{};
    result.valid = value.valid ? 1 : 0;
    if (!value.valid) {
        return result;
    }
    for (int corner = 0; corner < 3; ++corner) {
        result.uv[corner].u = value.uv[corner].x();
        result.uv[corner].v = value.uv[corner].y();
    }
    return result;
}

manumesh::FaceTexCoords toCppTexcoords(const ManuMeshFaceTexCoords& value) {
    manumesh::FaceTexCoords result;
    for (manumesh::Vec2& uv : result.uv) {
        uv.setZero();
    }
    result.valid = value.valid != 0;
    if (!result.valid) {
        return result;
    }
    for (int corner = 0; corner < 3; ++corner) {
        result.uv[corner] = manumesh::Vec2(value.uv[corner].u, value.uv[corner].v);
    }
    return result;
}

manumesh::FaceTexCoords zeroCppTexcoords() {
    manumesh::FaceTexCoords result;
    result.valid = false;
    for (manumesh::Vec2& uv : result.uv) {
        uv.setZero();
    }
    return result;
}

template <typename T>
ManuMeshStatus preflightOutputBuffer(
    ManuMeshContext* context,
    T* output,
    std::size_t capacity,
    std::size_t required,
    std::size_t* written,
    const char* tooSmallMessage,
    const char* nullMessage
) {
    if (capacity < required) {
        *written = required;
        return fail(context, MANUMESH_STATUS_BUFFER_TOO_SMALL, tooSmallMessage);
    }
    if (required > 0 && !output) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, nullMessage);
    }
    return MANUMESH_STATUS_OK;
}

template <typename T>
ManuMeshStatus copyOutputVector(
    ManuMeshContext* context,
    const std::vector<T>& values,
    T* output,
    std::size_t capacity,
    std::size_t* written,
    const char* tooSmallMessage,
    const char* nullMessage
) {
    const ManuMeshStatus preflightStatus =
        preflightOutputBuffer(context, output, capacity, values.size(), written, tooSmallMessage, nullMessage);
    if (preflightStatus != MANUMESH_STATUS_OK) {
        return preflightStatus;
    }
    if (!values.empty()) {
        std::copy(values.begin(), values.end(), output);
    }
    *written = values.size();
    return MANUMESH_STATUS_OK;
}

ManuMeshStatus
validateReadableMesh(ManuMeshContext* context, const manumesh::Mesh& mesh, bool strict, std::string& error) {
    const bool valid =
        strict ? manumesh::validateMeshGeometry(mesh, &error) : manumesh::validateMeshGeometryLenient(mesh, &error);
    if (!valid) {
        return fail(context, MANUMESH_STATUS_INVALID_MESH, error.empty() ? "Mesh geometry is invalid." : error.c_str());
    }
    return MANUMESH_STATUS_OK;
}

bool faceIsDegenerate(const manumesh::Mesh& mesh, const manumesh::Face& face) {
    if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
        return true;
    }
    return manumesh::triangleArea(mesh.vertices[face.v[0]], mesh.vertices[face.v[1]], mesh.vertices[face.v[2]]) <=
           manumesh::kMinTriangleArea;
}

bool appendMeshValue(const manumesh::Mesh& source, manumesh::Mesh& destination, std::string& error) {
    if (!manumesh::validateMeshGeometryLenient(destination, &error)) {
        error = "Destination mesh is invalid: " + error;
        return false;
    }
    if (!manumesh::validateMeshGeometryLenient(source, &error)) {
        error = "Source mesh is invalid: " + error;
        return false;
    }
    const std::size_t maxInt = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (source.vertices.size() > maxInt || destination.vertices.size() > maxInt ||
        destination.vertices.size() > maxInt - source.vertices.size()) {
        error = "Combined vertex count exceeds the supported int-index range.";
        return false;
    }
    if (source.faces.size() > maxInt || destination.faces.size() > maxInt ||
        destination.faces.size() > maxInt - source.faces.size()) {
        error = "Combined face count exceeds the supported int range.";
        return false;
    }

    const int vertexOffset = static_cast<int>(destination.vertices.size());
    const std::size_t originalFaceCount = destination.faces.size();
    destination.vertices.insert(destination.vertices.end(), source.vertices.begin(), source.vertices.end());
    destination.faces.reserve(originalFaceCount + source.faces.size());
    for (const manumesh::Face& sourceFace : source.faces) {
        manumesh::Face face = sourceFace;
        for (int& id : face.v) {
            id += vertexOffset;
        }
        destination.faces.push_back(face);
    }

    if (!destination.faceTexCoords.empty() || !source.faceTexCoords.empty()) {
        if (destination.faceTexCoords.empty()) {
            destination.faceTexCoords.resize(originalFaceCount, zeroCppTexcoords());
        }
        if (source.faceTexCoords.empty()) {
            destination.faceTexCoords.resize(destination.faces.size(), zeroCppTexcoords());
        } else {
            destination.faceTexCoords.reserve(destination.faces.size());
            for (const manumesh::FaceTexCoords& sourceTexcoords : source.faceTexCoords) {
                destination.faceTexCoords.push_back(sourceTexcoords.valid ? sourceTexcoords : zeroCppTexcoords());
            }
        }
    }
    return true;
}

} // namespace

extern "C" {

const char* manumesh_version(void) { return MANUMESH_VERSION; }

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
    case MANUMESH_STATUS_INVALID_MESH:
        return "invalid mesh";
    case MANUMESH_STATUS_UNSUPPORTED_FORMAT:
        return "unsupported format";
    }
    return "unknown status";
}

ManuMeshContext* manumesh_context_create(void) {
    try {
        return new (std::nothrow) ManuMeshContext();
    } catch (...) {
        return nullptr;
    }
}

void manumesh_context_destroy(ManuMeshContext* context) { delete context; }

const char* manumesh_context_last_error(const ManuMeshContext* context) {
    return context ? context->lastError.c_str() : "ManuMeshContext is null.";
}

void manumesh_context_clear_error(ManuMeshContext* context) { clearError(context); }

ManuMeshMeshHandle* manumesh_mesh_create(ManuMeshContext* context) {
    clearError(context);
    try {
        ManuMeshMeshHandle* mesh = new (std::nothrow) ManuMeshMeshHandle();
        if (!mesh) {
            fail(context, MANUMESH_STATUS_OUT_OF_MEMORY, "Failed to allocate mesh handle.");
        }
        return mesh;
    } catch (const std::exception& ex) {
        translateException(context, ex);
        return nullptr;
    } catch (...) {
        translateUnknownException(context);
        return nullptr;
    }
}

void manumesh_mesh_destroy(ManuMeshMeshHandle* mesh) { delete mesh; }

ManuMeshStatus manumesh_mesh_clear(ManuMeshContext* context, ManuMeshMeshHandle* mesh) {
    clearError(context);
    if (!mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
    }
    return withMeshLock(context, mesh, [](manumesh::Mesh& value) {
        value = manumesh::Mesh{};
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_set_data(
    ManuMeshContext* context,
    ManuMeshMeshHandle* mesh,
    const ManuMeshVec3* vertices,
    size_t vertex_count,
    const ManuMeshFace* faces,
    size_t face_count
) {
    clearError(context);
    if (!mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
    }
    if ((vertex_count > 0 && !vertices) || (face_count > 0 && !faces)) {
        return fail(
            context,
            MANUMESH_STATUS_INVALID_ARGUMENT,
            "Vertex and face pointers must be valid when counts are non-zero."
        );
    }
    if (vertex_count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Vertex count exceeds the supported int-index range.");
    }
    if (face_count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Face count exceeds the supported int-index range.");
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
        // C ABI 边界采用宽松校验：允许零面积面，由分析和简化阶段处理并报告数量；
        // 仅拒绝任何算法都无法处理的输入。
        if (!manumesh::validateMeshGeometryLenient(next, &error)) {
            return fail(
                context, MANUMESH_STATUS_INVALID_ARGUMENT, error.empty() ? "Mesh geometry is invalid." : error.c_str()
            );
        }
        return commitMesh(context, mesh, std::move(next));
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus manumesh_mesh_set_data_with_texcoords(
    ManuMeshContext* context,
    ManuMeshMeshHandle* mesh,
    const ManuMeshVec3* vertices,
    size_t vertex_count,
    const ManuMeshFace* faces,
    size_t face_count,
    const ManuMeshFaceTexCoords* face_texcoords
) {
    clearError(context);
    if (!mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
    }
    if ((vertex_count > 0 && !vertices) || (face_count > 0 && !faces)) {
        return fail(
            context,
            MANUMESH_STATUS_INVALID_ARGUMENT,
            "Vertex and face pointers must be valid when counts are non-zero."
        );
    }
    if (vertex_count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Vertex count exceeds the supported int-index range.");
    }
    if (face_count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Face count exceeds the supported int-index range.");
    }

    try {
        manumesh::Mesh next;
        next.vertices.reserve(vertex_count);
        next.faces.reserve(face_count);
        if (face_texcoords) {
            next.faceTexCoords.reserve(face_count);
        }
        for (size_t i = 0; i < vertex_count; ++i) {
            next.vertices.emplace_back(vertices[i].x, vertices[i].y, vertices[i].z);
        }
        for (size_t i = 0; i < face_count; ++i) {
            manumesh::Face face;
            face.v = {faces[i].v[0], faces[i].v[1], faces[i].v[2]};
            next.faces.push_back(face);
            if (face_texcoords) {
                if (!finiteTexcoords(face_texcoords[i])) {
                    return fail(
                        context,
                        MANUMESH_STATUS_INVALID_ARGUMENT,
                        "Texture coordinate arrays must contain only finite values."
                    );
                }
                next.faceTexCoords.push_back(toCppTexcoords(face_texcoords[i]));
            }
        }
        std::string error;
        if (!manumesh::validateMeshGeometryLenient(next, &error)) {
            return fail(
                context, MANUMESH_STATUS_INVALID_ARGUMENT, error.empty() ? "Mesh geometry is invalid." : error.c_str()
            );
        }
        return commitMesh(context, mesh, std::move(next));
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus manumesh_mesh_get_counts(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, size_t* vertex_count, size_t* face_count
) {
    clearError(context);
    if (!mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh handle must be valid.");
    }
    // 两个输出参数都允许为空，调用方可以只请求其中一个计数。
    if (!vertex_count && !face_count) {
        return fail(
            context, MANUMESH_STATUS_INVALID_ARGUMENT, "At least one of vertex_count or face_count must be valid."
        );
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        if (vertex_count) {
            *vertex_count = value.vertices.size();
        }
        if (face_count) {
            *face_count = value.faces.size();
        }
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_copy_vertices(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshVec3* vertices,
    size_t vertex_capacity,
    size_t* vertices_written
) {
    clearError(context);
    if (!mesh || !vertices_written) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and vertices_written pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        const size_t required = value.vertices.size();
        *vertices_written = required;
        if (vertex_capacity < required) {
            return fail(
                context, MANUMESH_STATUS_BUFFER_TOO_SMALL, "Vertex buffer is smaller than the mesh vertex count."
            );
        }
        if (required > 0 && !vertices) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Vertex buffer is null.");
        }
        for (size_t i = 0; i < required; ++i) {
            vertices[i] = toCVec3(value.vertices[i]);
        }
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_copy_faces(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshFace* faces,
    size_t face_capacity,
    size_t* faces_written
) {
    clearError(context);
    if (!mesh || !faces_written) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and faces_written pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        const size_t required = value.faces.size();
        *faces_written = required;
        if (face_capacity < required) {
            return fail(context, MANUMESH_STATUS_BUFFER_TOO_SMALL, "Face buffer is smaller than the mesh face count.");
        }
        if (required > 0 && !faces) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Face buffer is null.");
        }
        for (size_t i = 0; i < required; ++i) {
            faces[i].v[0] = value.faces[i].v[0];
            faces[i].v[1] = value.faces[i].v[1];
            faces[i].v[2] = value.faces[i].v[2];
        }
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus
manumesh_mesh_copy(ManuMeshContext* context, const ManuMeshMeshHandle* source, ManuMeshMeshHandle* destination) {
    clearError(context);
    if (!source || !destination) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Source and destination mesh handles must be valid.");
    }
    if (source == destination) {
        return withMeshLock(context, destination, [](manumesh::Mesh&) {
            return MANUMESH_STATUS_OK;
        });
    }
    return withTwoMeshLocks(
        context, source, destination, [](const manumesh::Mesh& sourceValue, manumesh::Mesh& destinationValue) {
            manumesh::Mesh candidate = sourceValue;
            destinationValue = std::move(candidate);
            return MANUMESH_STATUS_OK;
        }
    );
}

ManuMeshStatus manumesh_mesh_get_vertex(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, size_t vertex_index, ManuMeshVec3* vertex
) {
    clearError(context);
    if (!mesh || !vertex) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and vertex output pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        if (vertex_index >= value.vertices.size()) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Vertex index is out of range.");
        }
        *vertex = toCVec3(value.vertices[vertex_index]);
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus
manumesh_mesh_set_vertex(ManuMeshContext* context, ManuMeshMeshHandle* mesh, size_t vertex_index, ManuMeshVec3 vertex) {
    clearError(context);
    if (!mesh || !finiteCVec3(vertex)) {
        return fail(
            context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh must be valid and vertex coordinates must be finite."
        );
    }
    return withMeshLock(context, mesh, [&](manumesh::Mesh& value) {
        if (vertex_index >= value.vertices.size()) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Vertex index is out of range.");
        }
        manumesh::Mesh candidate = value;
        candidate.vertices[vertex_index] = manumesh::Vec3(vertex.x, vertex.y, vertex.z);
        std::string error;
        if (!manumesh::validateMeshGeometryLenient(candidate, &error)) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, error.c_str());
        }
        value = std::move(candidate);
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_get_face(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, size_t face_index, ManuMeshFace* face
) {
    clearError(context);
    if (!mesh || !face) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and face output pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        if (face_index >= value.faces.size()) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Face index is out of range.");
        }
        const manumesh::Face& source = value.faces[face_index];
        ManuMeshFace result = {{source.v[0], source.v[1], source.v[2]}};
        *face = result;
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus
manumesh_mesh_set_face(ManuMeshContext* context, ManuMeshMeshHandle* mesh, size_t face_index, ManuMeshFace face) {
    clearError(context);
    if (!mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
    }
    return withMeshLock(context, mesh, [&](manumesh::Mesh& value) {
        if (face_index >= value.faces.size()) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Face index is out of range.");
        }
        manumesh::Mesh candidate = value;
        candidate.faces[face_index].v = {face.v[0], face.v[1], face.v[2]};
        std::string error;
        if (!manumesh::validateMeshGeometryLenient(candidate, &error)) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, error.c_str());
        }
        value = std::move(candidate);
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus
manumesh_mesh_get_bounds(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshBounds* bounds) {
    clearError(context);
    if (!mesh || !bounds) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and bounds output pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        ManuMeshBounds result{};
        if (!value.vertices.empty()) {
            manumesh::Vec3 minimum = value.vertices.front();
            manumesh::Vec3 maximum = minimum;
            for (const manumesh::Vec3& vertex : value.vertices) {
                if (!finiteVec3(vertex)) {
                    return fail(context, MANUMESH_STATUS_INVALID_MESH, "Mesh contains a non-finite vertex coordinate.");
                }
                minimum = minimum.cwiseMin(vertex);
                maximum = maximum.cwiseMax(vertex);
            }
            result.min = toCVec3(minimum);
            result.max = toCVec3(maximum);
            result.valid = 1;
        }
        *bounds = result;
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_translate(ManuMeshContext* context, ManuMeshMeshHandle* mesh, ManuMeshVec3 offset) {
    clearError(context);
    if (!mesh || !finiteCVec3(offset)) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh must be valid and translation must be finite.");
    }
    return withMeshLock(context, mesh, [&](manumesh::Mesh& value) {
        manumesh::Mesh candidate = value;
        const manumesh::Vec3 delta(offset.x, offset.y, offset.z);
        for (manumesh::Vec3& vertex : candidate.vertices) {
            vertex += delta;
            if (!finiteVec3(vertex)) {
                return fail(context, MANUMESH_STATUS_INVALID_MESH, "Translation produced a non-finite vertex.");
            }
        }
        std::string error;
        if (!manumesh::validateMeshGeometryLenient(candidate, &error)) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, error.c_str());
        }
        value = std::move(candidate);
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus
manumesh_mesh_transform(ManuMeshContext* context, ManuMeshMeshHandle* mesh, const double matrix_row_major[16]) {
    clearError(context);
    if (!mesh || !matrix_row_major) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and transform matrix must be valid.");
    }
    std::array<double, 16> matrix{};
    for (std::size_t i = 0; i < matrix.size(); ++i) {
        if (!std::isfinite(matrix_row_major[i])) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Transform matrix must contain finite values.");
        }
        matrix[i] = matrix_row_major[i];
    }
    return withMeshLock(context, mesh, [&](manumesh::Mesh& value) {
        manumesh::Mesh candidate = value;
        for (manumesh::Vec3& vertex : candidate.vertices) {
            const double x = vertex.x();
            const double y = vertex.y();
            const double z = vertex.z();
            const double w = matrix[12] * x + matrix[13] * y + matrix[14] * z + matrix[15];
            if (!std::isfinite(w) || w == 0.0) {
                return fail(context, MANUMESH_STATUS_INVALID_MESH, "Transform produced an invalid homogeneous w.");
            }
            const manumesh::Vec3 transformed(
                (matrix[0] * x + matrix[1] * y + matrix[2] * z + matrix[3]) / w,
                (matrix[4] * x + matrix[5] * y + matrix[6] * z + matrix[7]) / w,
                (matrix[8] * x + matrix[9] * y + matrix[10] * z + matrix[11]) / w
            );
            if (!finiteVec3(transformed)) {
                return fail(context, MANUMESH_STATUS_INVALID_MESH, "Transform produced a non-finite vertex.");
            }
            vertex = transformed;
        }
        std::string error;
        if (!manumesh::validateMeshGeometryLenient(candidate, &error)) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, error.c_str());
        }
        value = std::move(candidate);
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_compact(ManuMeshContext* context, ManuMeshMeshHandle* mesh) {
    clearError(context);
    if (!mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
    }
    return withMeshLock(context, mesh, [&](manumesh::Mesh& value) {
        manumesh::Mesh candidate = value;
        candidate.removeUnusedVertices();
        std::string error;
        if (!manumesh::validateMeshGeometryLenient(candidate, &error)) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, error.c_str());
        }
        value = std::move(candidate);
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_validate(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, int strict, size_t* degenerate_face_count
) {
    clearError(context);
    if (degenerate_face_count) {
        *degenerate_face_count = 0;
    }
    if (!mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        std::string error;
        if (!manumesh::validateMeshIndices(value, &error)) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, error.c_str());
        }
        if (degenerate_face_count) {
            *degenerate_face_count = static_cast<size_t>(manumesh::countDegenerateFaces(value));
        }
        return validateReadableMesh(context, value, strict != 0, error);
    });
}

ManuMeshStatus manumesh_mesh_copy_face_areas(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, double* areas, size_t area_capacity, size_t* areas_written
) {
    clearError(context);
    if (areas_written) {
        *areas_written = 0;
    }
    if (!mesh || !areas_written) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and areas_written pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        std::string error;
        const ManuMeshStatus status = validateReadableMesh(context, value, false, error);
        if (status != MANUMESH_STATUS_OK) {
            return status;
        }
        const ManuMeshStatus preflightStatus = preflightOutputBuffer(
            context,
            areas,
            area_capacity,
            value.faces.size(),
            areas_written,
            "Face area buffer is too small.",
            "Face area buffer is null."
        );
        if (preflightStatus != MANUMESH_STATUS_OK) {
            return preflightStatus;
        }
        for (std::size_t faceIndex = 0; faceIndex < value.faces.size(); ++faceIndex) {
            const manumesh::Face& face = value.faces[faceIndex];
            const double area =
                manumesh::triangleArea(value.vertices[face.v[0]], value.vertices[face.v[1]], value.vertices[face.v[2]]);
            if (!std::isfinite(area)) {
                return fail(context, MANUMESH_STATUS_INVALID_MESH, "Face area computation is not finite.");
            }
            // 与 computeFaceAreas 保持一致：低于公共退化阈值的面返回零面积。
            areas[faceIndex] = area > manumesh::kMinTriangleArea ? area : 0.0;
        }
        *areas_written = value.faces.size();
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus
manumesh_mesh_get_surface_area(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, double* surface_area) {
    clearError(context);
    if (!mesh || !surface_area) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and surface_area pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        std::string error;
        const ManuMeshStatus status = validateReadableMesh(context, value, false, error);
        if (status != MANUMESH_STATUS_OK) {
            return status;
        }
        // 直接累加，避免 computeSurfaceArea 为了返回逐面结果而分配临时数组。
        long double sum = 0.0L;
        for (const manumesh::Face& face : value.faces) {
            const double area =
                manumesh::triangleArea(value.vertices[face.v[0]], value.vertices[face.v[1]], value.vertices[face.v[2]]);
            if (!std::isfinite(area)) {
                return fail(context, MANUMESH_STATUS_INVALID_MESH, "Mesh surface area is not finite.");
            }
            if (area > manumesh::kMinTriangleArea) {
                sum += static_cast<long double>(area);
            }
        }
        const double result = static_cast<double>(sum);
        if (!std::isfinite(result)) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, "Mesh surface area is not finite.");
        }
        *surface_area = result;
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus
manumesh_mesh_get_signed_volume(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, double* signed_volume) {
    clearError(context);
    if (!mesh || !signed_volume) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and signed_volume pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        std::string error;
        const ManuMeshStatus validationStatus = validateReadableMesh(context, value, true, error);
        if (validationStatus != MANUMESH_STATUS_OK) {
            return validationStatus;
        }
        const manumesh::Result<manumesh::MeshTopologySummary> topologyResult = manumesh::summarizeMeshTopology(value);
        if (!topologyResult.ok()) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, topologyResult.status().message().c_str());
        }
        const manumesh::MeshTopologySummary& topology = topologyResult.value();
        if (!topology.closedManifold) {
            return fail(
                context, MANUMESH_STATUS_INVALID_MESH, "Signed volume requires a non-empty closed manifold mesh."
            );
        }
        if (!topology.consistentlyOriented) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, "Signed volume requires consistently oriented faces.");
        }
        const double volume = manumesh::computeSignedVolume(value);
        if (!std::isfinite(volume)) {
            return fail(
                context, MANUMESH_STATUS_INVALID_MESH, "Mesh signed volume is not representable as a finite double."
            );
        }
        *signed_volume = volume;
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_get_surface_centroid(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshVec3* surface_centroid
) {
    clearError(context);
    if (!mesh || !surface_centroid) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and surface_centroid pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        std::string error;
        const ManuMeshStatus status = validateReadableMesh(context, value, false, error);
        if (status != MANUMESH_STATUS_OK) {
            return status;
        }
        const manumesh::Vec3 result = manumesh::computeSurfaceCentroid(value);
        if (!finiteVec3(result)) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, "Mesh surface centroid is not finite.");
        }
        *surface_centroid = toCVec3(result);
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_copy_face_centroids(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshVec3* centroids,
    size_t centroid_capacity,
    size_t* centroids_written
) {
    clearError(context);
    if (centroids_written) {
        *centroids_written = 0;
    }
    if (!mesh || !centroids_written) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and centroids_written pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        std::string error;
        const ManuMeshStatus status = validateReadableMesh(context, value, false, error);
        if (status != MANUMESH_STATUS_OK) {
            return status;
        }
        const ManuMeshStatus preflightStatus = preflightOutputBuffer(
            context,
            centroids,
            centroid_capacity,
            value.faces.size(),
            centroids_written,
            "Face centroid buffer is too small.",
            "Face centroid buffer is null."
        );
        if (preflightStatus != MANUMESH_STATUS_OK) {
            return preflightStatus;
        }
        for (std::size_t faceIndex = 0; faceIndex < value.faces.size(); ++faceIndex) {
            const manumesh::Face& face = value.faces[faceIndex];
            const manumesh::Vec3 centroid =
                (value.vertices[face.v[0]] + value.vertices[face.v[1]] + value.vertices[face.v[2]]) / 3.0;
            if (!finiteVec3(centroid)) {
                return fail(context, MANUMESH_STATUS_INVALID_MESH, "Face centroid computation is not finite.");
            }
            centroids[faceIndex] = toCVec3(centroid);
        }
        *centroids_written = value.faces.size();
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_copy_face_normals(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshVec3* normals,
    size_t normal_capacity,
    size_t* normals_written
) {
    clearError(context);
    if (normals_written) {
        *normals_written = 0;
    }
    if (!mesh || !normals_written) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and normals_written pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        std::string error;
        const ManuMeshStatus status = validateReadableMesh(context, value, false, error);
        if (status != MANUMESH_STATUS_OK) {
            return status;
        }
        const ManuMeshStatus preflightStatus = preflightOutputBuffer(
            context,
            normals,
            normal_capacity,
            value.faces.size(),
            normals_written,
            "Face normal buffer is too small.",
            "Face normal buffer is null."
        );
        if (preflightStatus != MANUMESH_STATUS_OK) {
            return preflightStatus;
        }
        for (std::size_t faceIndex = 0; faceIndex < value.faces.size(); ++faceIndex) {
            const manumesh::Face& face = value.faces[faceIndex];
            const manumesh::Vec3 normal = manumesh::triangleNormal(
                value.vertices[face.v[0]], value.vertices[face.v[1]], value.vertices[face.v[2]]
            );
            if (!finiteVec3(normal)) {
                return fail(context, MANUMESH_STATUS_INVALID_MESH, "Face normal computation is not finite.");
            }
            normals[faceIndex] = toCVec3(normal);
        }
        *normals_written = value.faces.size();
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_copy_vertex_normals(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshVec3* normals,
    size_t normal_capacity,
    size_t* normals_written
) {
    clearError(context);
    if (normals_written) {
        *normals_written = 0;
    }
    if (!mesh || !normals_written) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and normals_written pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        std::string error;
        const ManuMeshStatus status = validateReadableMesh(context, value, false, error);
        if (status != MANUMESH_STATUS_OK) {
            return status;
        }
        const ManuMeshStatus preflightStatus = preflightOutputBuffer(
            context,
            normals,
            normal_capacity,
            value.vertices.size(),
            normals_written,
            "Vertex normal buffer is too small.",
            "Vertex normal buffer is null."
        );
        if (preflightStatus != MANUMESH_STATUS_OK) {
            return preflightStatus;
        }
        const std::vector<manumesh::Vec3> cppNormals = manumesh::computeVertexNormals(value);
        for (std::size_t i = 0; i < cppNormals.size(); ++i) {
            if (!finiteVec3(cppNormals[i])) {
                return fail(context, MANUMESH_STATUS_INVALID_MESH, "Vertex normal computation is not finite.");
            }
            normals[i] = toCVec3(cppNormals[i]);
        }
        *normals_written = cppNormals.size();
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_copy_unique_edges(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshEdge* edges,
    size_t edge_capacity,
    size_t* edges_written
) {
    clearError(context);
    if (edges_written) {
        *edges_written = 0;
    }
    if (!mesh || !edges_written) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and edges_written pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        std::string error;
        const ManuMeshStatus status = validateReadableMesh(context, value, false, error);
        if (status != MANUMESH_STATUS_OK) {
            return status;
        }
        const manumesh::Result<manumesh::MeshTopology> topologyResult = manumesh::MeshTopology::build(value);
        if (!topologyResult.ok()) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, topologyResult.status().message().c_str());
        }
        const std::vector<manumesh::TopologyEdge>& topologyEdges = topologyResult.value().edges();
        const ManuMeshStatus preflightStatus = preflightOutputBuffer(
            context,
            edges,
            edge_capacity,
            topologyEdges.size(),
            edges_written,
            "Unique edge buffer is too small.",
            "Unique edge buffer is null."
        );
        if (preflightStatus != MANUMESH_STATUS_OK) {
            return preflightStatus;
        }
        std::vector<ManuMeshEdge> result;
        result.reserve(topologyEdges.size());
        for (const manumesh::TopologyEdge& edge : topologyEdges) {
            result.push_back(
                ManuMeshEdge{
                    edge.vertices[0],
                    edge.vertices[1],
                    edge.faces.size(),
                    edge.boundary() ? 1 : 0,
                    edge.nonManifold() ? 1 : 0,
                }
            );
        }
        std::sort(result.begin(), result.end(), [](const ManuMeshEdge& lhs, const ManuMeshEdge& rhs) {
            return std::tie(lhs.a, lhs.b) < std::tie(rhs.a, rhs.b);
        });
        return copyOutputVector(
            context,
            result,
            edges,
            edge_capacity,
            edges_written,
            "Unique edge buffer is too small.",
            "Unique edge buffer is null."
        );
    });
}

ManuMeshStatus manumesh_mesh_get_topology_summary(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshTopologySummary* summary
) {
    clearError(context);
    if (!mesh || !summary) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and topology summary pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        const manumesh::Result<manumesh::MeshTopologySummary> topologyResult = manumesh::summarizeMeshTopology(value);
        if (!topologyResult.ok()) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, topologyResult.status().message().c_str());
        }
        const manumesh::MeshTopologySummary& source = topologyResult.value();
        const ManuMeshTopologySummary result{
            source.connectedFaceComponents,
            source.uniqueEdges,
            source.boundaryEdges,
            source.nonManifoldEdges,
            source.closedManifold ? 1 : 0,
            source.consistentlyOriented ? 1 : 0,
        };
        *summary = result;
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus
manumesh_mesh_has_texture_coordinates(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, int* has_texcoords) {
    clearError(context);
    if (!mesh || !has_texcoords) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and has_texcoords pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        *has_texcoords = value.hasTextureCoordinates() ? 1 : 0;
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_get_face_texcoords(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, size_t face_index, ManuMeshFaceTexCoords* texcoords
) {
    clearError(context);
    if (!mesh || !texcoords) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and texcoords output pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        if (face_index >= value.faces.size()) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Face index is out of range.");
        }
        ManuMeshFaceTexCoords result = zeroTexcoords();
        if (value.faceTexCoords.size() == value.faces.size()) {
            result = toCTexcoords(value.faceTexCoords[face_index]);
        } else if (!value.faceTexCoords.empty()) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, "Texture coordinates are not aligned with mesh faces.");
        }
        *texcoords = result;
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_set_face_texcoords(
    ManuMeshContext* context, ManuMeshMeshHandle* mesh, size_t face_index, const ManuMeshFaceTexCoords* texcoords
) {
    clearError(context);
    if (!mesh || !texcoords || !finiteTexcoords(*texcoords)) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and finite texture coordinates must be valid.");
    }
    return withMeshLock(context, mesh, [&](manumesh::Mesh& value) {
        if (face_index >= value.faces.size()) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Face index is out of range.");
        }
        manumesh::Mesh candidate = value;
        if (candidate.faceTexCoords.empty()) {
            candidate.faceTexCoords.resize(candidate.faces.size());
        } else if (candidate.faceTexCoords.size() != candidate.faces.size()) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, "Texture coordinates are not aligned with mesh faces.");
        }
        candidate.faceTexCoords[face_index] = toCppTexcoords(*texcoords);
        std::string error;
        if (!manumesh::validateMeshGeometryLenient(candidate, &error)) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, error.c_str());
        }
        value = std::move(candidate);
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus manumesh_mesh_copy_face_texcoords(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshFaceTexCoords* texcoords,
    size_t texcoord_capacity,
    size_t* texcoords_written
) {
    clearError(context);
    if (texcoords_written) {
        *texcoords_written = 0;
    }
    if (!mesh || !texcoords_written) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and texcoords_written pointers must be valid.");
    }
    return withMeshLock(context, mesh, [&](const manumesh::Mesh& value) {
        if (!value.faceTexCoords.empty() && value.faceTexCoords.size() != value.faces.size()) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, "Texture coordinates are not aligned with mesh faces.");
        }
        for (const manumesh::FaceTexCoords& source : value.faceTexCoords) {
            if (source.valid) {
                for (const manumesh::Vec2& uv : source.uv) {
                    if (!std::isfinite(uv.x()) || !std::isfinite(uv.y())) {
                        return fail(context, MANUMESH_STATUS_INVALID_MESH, "Texture coordinates are not finite.");
                    }
                }
            }
        }
        const ManuMeshStatus preflightStatus = preflightOutputBuffer(
            context,
            texcoords,
            texcoord_capacity,
            value.faces.size(),
            texcoords_written,
            "Texture coordinate buffer is too small.",
            "Texture coordinate buffer is null."
        );
        if (preflightStatus != MANUMESH_STATUS_OK) {
            return preflightStatus;
        }
        std::vector<ManuMeshFaceTexCoords> result(value.faces.size(), zeroTexcoords());
        for (std::size_t i = 0; i < value.faceTexCoords.size(); ++i) {
            result[i] = toCTexcoords(value.faceTexCoords[i]);
        }
        return copyOutputVector(
            context,
            result,
            texcoords,
            texcoord_capacity,
            texcoords_written,
            "Texture coordinate buffer is too small.",
            "Texture coordinate buffer is null."
        );
    });
}

ManuMeshStatus manumesh_mesh_reverse_winding(ManuMeshContext* context, ManuMeshMeshHandle* mesh) {
    clearError(context);
    if (!mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
    }
    return withMeshLock(context, mesh, [&](manumesh::Mesh& value) {
        if (!value.faceTexCoords.empty() && value.faceTexCoords.size() != value.faces.size()) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, "Texture coordinates are not aligned with mesh faces.");
        }
        manumesh::Mesh candidate = value;
        manumesh::reverseFaceWindings(candidate);
        value = std::move(candidate);
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus
manumesh_mesh_remove_degenerate_faces(ManuMeshContext* context, ManuMeshMeshHandle* mesh, size_t* removed_face_count) {
    clearError(context);
    if (removed_face_count) {
        *removed_face_count = 0;
    }
    if (!mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh handle is null.");
    }
    return withMeshLock(context, mesh, [&](manumesh::Mesh& value) {
        std::string error;
        if (!manumesh::validateMeshIndices(value, &error)) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, error.c_str());
        }
        for (const manumesh::Vec3& vertex : value.vertices) {
            if (!finiteVec3(vertex)) {
                return fail(context, MANUMESH_STATUS_INVALID_MESH, "Mesh contains a non-finite vertex coordinate.");
            }
        }
        if (!value.faceTexCoords.empty() && value.faceTexCoords.size() != value.faces.size()) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, "Texture coordinates are not aligned with mesh faces.");
        }
        manumesh::Mesh candidate;
        candidate.vertices = value.vertices;
        candidate.faces.reserve(value.faces.size());
        if (!value.faceTexCoords.empty()) {
            candidate.faceTexCoords.reserve(value.faceTexCoords.size());
        }
        std::size_t removed = 0;
        for (std::size_t i = 0; i < value.faces.size(); ++i) {
            if (faceIsDegenerate(value, value.faces[i])) {
                ++removed;
                continue;
            }
            candidate.faces.push_back(value.faces[i]);
            if (!value.faceTexCoords.empty()) {
                candidate.faceTexCoords.push_back(value.faceTexCoords[i]);
            }
        }
        candidate.removeUnusedVertices();
        if (!manumesh::validateMeshGeometryLenient(candidate, &error)) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, error.c_str());
        }
        value = std::move(candidate);
        if (removed_face_count) {
            *removed_face_count = removed;
        }
        return MANUMESH_STATUS_OK;
    });
}

ManuMeshStatus
manumesh_mesh_append(ManuMeshContext* context, ManuMeshMeshHandle* destination, const ManuMeshMeshHandle* source) {
    clearError(context);
    if (!destination || !source) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Destination and source mesh handles must be valid.");
    }
    if (destination == source) {
        return withMeshLock(context, destination, [&](manumesh::Mesh& value) {
            manumesh::Mesh sourceCopy = value;
            manumesh::Mesh candidate = value;
            std::string error;
            if (!appendMeshValue(sourceCopy, candidate, error)) {
                return fail(context, MANUMESH_STATUS_INVALID_MESH, error.c_str());
            }
            value = std::move(candidate);
            return MANUMESH_STATUS_OK;
        });
    }
    return withTwoMeshLocks(
        context, source, destination, [&](const manumesh::Mesh& sourceValue, manumesh::Mesh& destinationValue) {
            manumesh::Mesh candidate = destinationValue;
            std::string error;
            if (!appendMeshValue(sourceValue, candidate, error)) {
                return fail(context, MANUMESH_STATUS_INVALID_MESH, error.c_str());
            }
            destinationValue = std::move(candidate);
            return MANUMESH_STATUS_OK;
        }
    );
}

ManuMeshStatus manumesh_load_mesh(
    ManuMeshContext* context, const char* path, ManuMeshMeshHandle* mesh, double merge_relative_epsilon
) {
    clearError(context);
    if (!path || !mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Path and mesh handle must be valid.");
    }
    if (!std::isfinite(merge_relative_epsilon) || merge_relative_epsilon < 0.0) {
        return fail(
            context, MANUMESH_STATUS_INVALID_ARGUMENT, "merge_relative_epsilon must be finite and non-negative."
        );
    }
    std::string error;
    try {
        if (!hasSupportedMeshExtension(path)) {
            return fail(context, MANUMESH_STATUS_UNSUPPORTED_FORMAT, "Unsupported mesh extension. Use .stl or .obj.");
        }
        manumesh::Mesh loaded;
        if (!manumesh::loadMesh(path, loaded, &error, merge_relative_epsilon)) {
            const ManuMeshStatus status =
                error == "Mesh load ran out of memory." ? MANUMESH_STATUS_OUT_OF_MEMORY : MANUMESH_STATUS_IO_ERROR;
            return fail(context, status, error.c_str());
        }
        return commitMesh(context, mesh, std::move(loaded));
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus manumesh_save_ascii_stl(
    ManuMeshContext* context, const char* path, const ManuMeshMeshHandle* mesh, const char* solid_name
) {
    clearError(context);
    if (!path || !mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Path and mesh handle must be valid.");
    }
    std::string error;
    try {
        manumesh::Mesh snapshot;
        const ManuMeshStatus snapshotStatus = snapshotMesh(context, mesh, snapshot);
        if (snapshotStatus != MANUMESH_STATUS_OK) {
            return snapshotStatus;
        }
        const char* name = solid_name ? solid_name : "mesh";
        if (!manumesh::saveAsciiStl(path, snapshot, name, &error)) {
            const ManuMeshStatus status =
                isMeshValidationError(error) ? MANUMESH_STATUS_INVALID_MESH : MANUMESH_STATUS_IO_ERROR;
            return fail(context, status, error.c_str());
        }
        return MANUMESH_STATUS_OK;
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus manumesh_save_binary_stl(ManuMeshContext* context, const char* path, const ManuMeshMeshHandle* mesh) {
    clearError(context);
    if (!path || !mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Path and mesh handle must be valid.");
    }
    std::string error;
    try {
        manumesh::Mesh snapshot;
        const ManuMeshStatus snapshotStatus = snapshotMesh(context, mesh, snapshot);
        if (snapshotStatus != MANUMESH_STATUS_OK) {
            return snapshotStatus;
        }
        if (!manumesh::saveBinaryStl(path, snapshot, &error)) {
            const ManuMeshStatus status = isBinaryStlRepresentabilityError(error) || isMeshValidationError(error)
                                              ? MANUMESH_STATUS_INVALID_MESH
                                              : MANUMESH_STATUS_IO_ERROR;
            return fail(context, status, error.c_str());
        }
        return MANUMESH_STATUS_OK;
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus manumesh_save_obj(ManuMeshContext* context, const char* path, const ManuMeshMeshHandle* mesh) {
    clearError(context);
    if (!path || !mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Path and mesh handle must be valid.");
    }
    std::string error;
    try {
        manumesh::Mesh snapshot;
        const ManuMeshStatus snapshotStatus = snapshotMesh(context, mesh, snapshot);
        if (snapshotStatus != MANUMESH_STATUS_OK) {
            return snapshotStatus;
        }
        if (!manumesh::saveObj(path, snapshot, &error)) {
            const ManuMeshStatus status =
                isMeshValidationError(error) ? MANUMESH_STATUS_INVALID_MESH : MANUMESH_STATUS_IO_ERROR;
            return fail(context, status, error.c_str());
        }
        return MANUMESH_STATUS_OK;
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus manumesh_generate_mesh(ManuMeshContext* context, const char* name, int n, ManuMeshMeshHandle* mesh) {
    clearError(context);
    if (!name || !mesh) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Generator name and mesh handle must be valid.");
    }
    std::string error;
    try {
        manumesh::Mesh generated;
        if (!manumesh::generateMeshByName(name, n, generated, &error)) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, error.c_str());
        }
        return commitMesh(context, mesh, std::move(generated));
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus manumesh_feature_options_init_with_size(ManuMeshFeatureOptions* options, size_t struct_capacity) {
    try {
        return manumesh::api::initializeFeatureOptions(options, struct_capacity);
    } catch (const std::exception& ex) {
        return translateException(nullptr, ex);
    } catch (...) {
        return translateUnknownException(nullptr);
    }
}

void manumesh_feature_options_init(ManuMeshFeatureOptions* options) {
    (void)manumesh_feature_options_init_with_size(options, kLegacyV1FeatureOptionsSize);
}

ManuMeshStatus manumesh_detect_feature_edges(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    const ManuMeshFeatureOptions* options,
    ManuMeshFeatureEdge* edges,
    size_t edge_capacity,
    size_t* edges_written
) {
    clearError(context);
    if (!mesh || !edges_written) {
        if (edges_written) {
            *edges_written = 0;
        }
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and edges_written pointers must be valid.");
    }
    *edges_written = 0;
    if (edge_capacity > 0 && !edges) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Edge buffer is null for a non-zero capacity.");
    }
    try {
        manumesh::Mesh snapshot;
        const ManuMeshStatus snapshotStatus = snapshotMesh(context, mesh, snapshot);
        if (snapshotStatus != MANUMESH_STATUS_OK) {
            return snapshotStatus;
        }
        manumesh::feature::FeatureOptions cppOptions;
        std::string conversionError;
        if (!manumesh::api::readFeatureOptions(options, cppOptions, conversionError)) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, conversionError.c_str());
        }
        const manumesh::feature::FeatureAnalysis analysis =
            manumesh::feature::detectFeatureCurves(snapshot, cppOptions);

        // Filter first, then copy directly into the caller's buffer. This keeps the
        // two-call API allocation-free on the successful copy path.
        size_t required = 0;
        for (const manumesh::feature::FeatureGraphEdge& source : analysis.graph.edges) {
            if (!isExportableFeatureEdge(source, snapshot.vertices.size())) {
                continue;
            }
            ++required;
        }

        *edges_written = required;
        if (edge_capacity < required) {
            return fail(context, MANUMESH_STATUS_BUFFER_TOO_SMALL, "Feature edge buffer is too small.");
        }
        size_t outputIndex = 0;
        for (const manumesh::feature::FeatureGraphEdge& source : analysis.graph.edges) {
            if (!isExportableFeatureEdge(source, snapshot.vertices.size())) {
                continue;
            }
            copyFeatureEdge(source, edges[outputIndex++]);
        }
        return MANUMESH_STATUS_OK;
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus manumesh_detect_feature_edges_v2(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    const ManuMeshFeatureOptions* options,
    ManuMeshFeatureEdgeV2* edges,
    size_t edge_capacity,
    size_t* edges_written
) {
    clearError(context);
    if (!mesh || !edges_written) {
        if (edges_written) {
            *edges_written = 0;
        }
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and edges_written pointers must be valid.");
    }
    *edges_written = 0;
    if (edge_capacity > 0 && !edges) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Edge buffer is null for a non-zero capacity.");
    }
    try {
        manumesh::Mesh snapshot;
        const ManuMeshStatus snapshotStatus = snapshotMesh(context, mesh, snapshot);
        if (snapshotStatus != MANUMESH_STATUS_OK) {
            return snapshotStatus;
        }
        manumesh::feature::FeatureOptions cppOptions;
        std::string conversionError;
        if (!manumesh::api::readFeatureOptions(options, cppOptions, conversionError)) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, conversionError.c_str());
        }
        const manumesh::feature::FeatureAnalysis analysis =
            manumesh::feature::detectFeatureCurves(snapshot, cppOptions);
        std::size_t required = 0;
        for (const manumesh::feature::FeatureGraphEdge& source : analysis.graph.edges) {
            if (isExportableFeatureEdge(source, snapshot.vertices.size())) {
                ++required;
            }
        }
        if (edge_capacity < required) {
            *edges_written = required;
            return fail(context, MANUMESH_STATUS_BUFFER_TOO_SMALL, "Feature edge buffer is too small.");
        }
        const std::vector<IndexedFeatureEdge> indexed = buildIndexedFeatureEdges(snapshot, analysis);
        if (indexed.size() != required) {
            return fail(context, MANUMESH_STATUS_ALGORITHM_ERROR, "Feature edge export count changed during indexing.");
        }
        for (std::size_t i = 0; i < indexed.size(); ++i) {
            copyFeatureEdgeV2(indexed[i], static_cast<std::uint64_t>(i), edges[i]);
        }
        *edges_written = indexed.size();
        return MANUMESH_STATUS_OK;
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus manumesh_simplify_options_init_with_size(ManuMeshSimplifyOptions* options, size_t struct_capacity) {
    try {
        return manumesh::api::initializeSimplifyOptions(options, struct_capacity);
    } catch (const std::exception& ex) {
        return translateException(nullptr, ex);
    } catch (...) {
        return translateUnknownException(nullptr);
    }
}

ManuMeshStatus manumesh_simplify_report_init_with_size(ManuMeshSimplifyReport* report, size_t struct_capacity) {
    try {
        return manumesh::api::initializeSimplifyReport(report, struct_capacity);
    } catch (const std::exception& ex) {
        return translateException(nullptr, ex);
    } catch (...) {
        return translateUnknownException(nullptr);
    }
}

ManuMeshStatus manumesh_mesh_stats_init_with_size(ManuMeshMeshStats* stats, size_t struct_capacity) {
    try {
        return manumesh::api::initializeMeshStats(stats, struct_capacity);
    } catch (const std::exception& ex) {
        return translateException(nullptr, ex);
    } catch (...) {
        return translateUnknownException(nullptr);
    }
}

void manumesh_simplify_options_init(ManuMeshSimplifyOptions* options) {
    (void)manumesh_simplify_options_init_with_size(options, kLegacyV1SimplifyOptionsSize);
}

void manumesh_simplify_report_init(ManuMeshSimplifyReport* report) {
    (void)manumesh_simplify_report_init_with_size(report, kLegacyV1SimplifyReportSize);
}

void manumesh_mesh_stats_init(ManuMeshMeshStats* stats) {
    (void)manumesh_mesh_stats_init_with_size(stats, kLegacyV1MeshStatsSize);
}

ManuMeshStatus manumesh_simplify_mesh_with_report_size(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* input,
    const ManuMeshSimplifyOptions* options,
    ManuMeshMeshHandle* output,
    ManuMeshSimplifyReport* report,
    size_t report_capacity
) {
    clearError(context);
    if (!input || !output) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Input and output mesh handles must be valid.");
    }
    try {
        manumesh::simplification::SimplifyOptions cppOptions;
        if (options) {
            std::string conversionError;
            if (!manumesh::api::readSimplifyOptions(*options, cppOptions, conversionError)) {
                return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, conversionError.c_str());
            }
        }
        if (report) {
            std::string outputError;
            if (!manumesh::api::validateSimplifyReportOutput(report, report_capacity, outputError)) {
                return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, outputError.c_str());
            }
        }

        const auto simplifyAndCommit = [&](const manumesh::Mesh& source, manumesh::Mesh& destination) {
            manumesh::simplification::SimplifyReport cppReport;
            manumesh::simplification::QEMSimplifier simplifier(cppOptions);
            manumesh::Mesh simplified = simplifier.simplify(source, &cppReport);
            if (report) {
                const ManuMeshStatus reportStatus =
                    manumesh::api::fillSimplifyReport(cppReport, report, report_capacity);
                if (reportStatus != MANUMESH_STATUS_OK) {
                    return fail(context, reportStatus, "Failed to initialize the simplify report output buffer.");
                }
            }
            destination = std::move(simplified);
            return MANUMESH_STATUS_OK;
        };

        // Keep the source snapshot and output commit in one critical section. Otherwise a
        // concurrent writer can successfully update the output while simplification runs,
        // only to have that update silently overwritten by the later commit.
        if (input == output) {
            return withMeshLock(context, output, [&](manumesh::Mesh& mesh) {
                return simplifyAndCommit(mesh, mesh);
            });
        }
        return withTwoMeshLocks(context, input, output, [&](const manumesh::Mesh& source, manumesh::Mesh& destination) {
            return simplifyAndCommit(source, destination);
        });
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus manumesh_simplify_mesh(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* input,
    const ManuMeshSimplifyOptions* options,
    ManuMeshMeshHandle* output,
    ManuMeshSimplifyReport* report
) {
    const size_t reportCapacity = report ? kLegacyV1SimplifyReportSize : 0;
    return manumesh_simplify_mesh_with_report_size(context, input, options, output, report, reportCapacity);
}

ManuMeshStatus manumesh_compute_mesh_stats_with_size(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshMeshStats* stats, size_t stats_capacity
) {
    clearError(context);
    if (!mesh || !stats) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and stats output pointers must be valid.");
    }
    try {
        std::string outputError;
        if (!manumesh::api::validateMeshStatsOutput(stats, stats_capacity, outputError)) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, outputError.c_str());
        }
        manumesh::Mesh snapshot;
        const ManuMeshStatus snapshotStatus = snapshotMesh(context, mesh, snapshot);
        if (snapshotStatus != MANUMESH_STATUS_OK) {
            return snapshotStatus;
        }
        const manumesh::analysis::MeshStats result = manumesh::analysis::computeMeshStats(snapshot);
        if (!std::isfinite(result.area) || !std::isfinite(result.meanTriangleQuality) ||
            !std::isfinite(result.minTriangleQuality) || !std::isfinite(result.meanEdgeLength) ||
            !std::isfinite(result.edgeLengthCv)) {
            return fail(context, MANUMESH_STATUS_INVALID_MESH, "Mesh statistics contain a non-finite value.");
        }
        return manumesh::api::fillMeshStats(result, stats, stats_capacity);
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus
manumesh_compute_mesh_stats(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshMeshStats* stats) {
    return manumesh_compute_mesh_stats_with_size(context, mesh, stats, kLegacyV1MeshStatsSize);
}

} // C 链接块
