/**
 * @file src/api/CApi.cpp
 * @brief 实现 ManuMesh 的C ABI 模块的C API功能。
 * @ingroup manumesh_c_api
 *
 * @details C 边界负责校验指针和容量，将失败转换为状态码，并且不允许 C++ 异常穿过 ABI。
 */

#include "api/CApi.h"

#include "algorithms/analysis/MeshAnalysis.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "api/detail/CApiConverters.h"
#include "core/MeshGenerators.h"
#include "io/MeshIo.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
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
    manumesh::Mesh mesh;
};

namespace {
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
static_assert(
    sizeof(LegacyV1SimplifyOptionsLayout) == offsetof(ManuMeshSimplifyOptions, loop_trace_angle_deg),
    "ManuMeshSimplifyOptions v1 prefix layout changed"
);
static_assert(
    offsetof(LegacyV1SimplifyOptionsLayout, feature_protection_mode) ==
        offsetof(ManuMeshSimplifyOptions, feature_protection_mode),
    "ManuMeshSimplifyOptions v1 field layout changed"
);
static_assert(
    offsetof(ManuMeshSimplifyOptions, feature_options) + sizeof(((ManuMeshSimplifyOptions*)nullptr)->feature_options) ==
        sizeof(ManuMeshSimplifyOptions),
    "ManuMeshSimplifyOptions feature_options must remain the final ABI field"
);
static_assert(
    sizeof(LegacyV1SimplifyReportLayout) == offsetof(ManuMeshSimplifyReport, traced_feature_edges),
    "ManuMeshSimplifyReport v1 prefix layout changed"
);
static_assert(
    offsetof(LegacyV1SimplifyReportLayout, max_applied_line_weight) ==
        offsetof(ManuMeshSimplifyReport, max_applied_line_weight),
    "ManuMeshSimplifyReport v1 field layout changed"
);
static_assert(
    offsetof(LegacyV1MeshStatsLayout, edge_length_cv) == offsetof(ManuMeshMeshStats, edge_length_cv),
    "ManuMeshMeshStats v1 field layout changed"
);

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
    mesh->mesh.vertices.clear();
    mesh->mesh.faces.clear();
    mesh->mesh.faceTexCoords.clear();
    return MANUMESH_STATUS_OK;
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
        mesh->mesh = std::move(next);
        return MANUMESH_STATUS_OK;
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
    if (vertex_count) {
        *vertex_count = mesh->mesh.vertices.size();
    }
    if (face_count) {
        *face_count = mesh->mesh.faces.size();
    }
    return MANUMESH_STATUS_OK;
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
    const size_t required = mesh->mesh.vertices.size();
    *vertices_written = required;
    if (vertex_capacity < required) {
        return fail(context, MANUMESH_STATUS_BUFFER_TOO_SMALL, "Vertex buffer is smaller than the mesh vertex count.");
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
    const size_t required = mesh->mesh.faces.size();
    *faces_written = required;
    if (face_capacity < required) {
        return fail(context, MANUMESH_STATUS_BUFFER_TOO_SMALL, "Face buffer is smaller than the mesh face count.");
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
        manumesh::Mesh loaded;
        if (!manumesh::loadMesh(path, loaded, &error, merge_relative_epsilon)) {
            return fail(context, MANUMESH_STATUS_IO_ERROR, error.c_str());
        }
        mesh->mesh = std::move(loaded);
        return MANUMESH_STATUS_OK;
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
        const char* name = solid_name ? solid_name : "mesh";
        if (!manumesh::saveAsciiStl(path, mesh->mesh, name, &error)) {
            return fail(context, MANUMESH_STATUS_IO_ERROR, error.c_str());
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
        if (!manumesh::saveBinaryStl(path, mesh->mesh, &error)) {
            return fail(context, MANUMESH_STATUS_IO_ERROR, error.c_str());
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
        mesh->mesh = std::move(generated);
        return MANUMESH_STATUS_OK;
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus manumesh_feature_options_init_with_size(ManuMeshFeatureOptions* options, size_t struct_capacity) {
    return manumesh::api::initializeFeatureOptions(options, struct_capacity);
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
        manumesh::feature::FeatureOptions cppOptions;
        std::string conversionError;
        if (!manumesh::api::readFeatureOptions(options, cppOptions, conversionError)) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, conversionError.c_str());
        }
        const manumesh::feature::FeatureAnalysis analysis =
            manumesh::feature::detectFeatureCurves(mesh->mesh, cppOptions);

        // Filter first, then copy directly into the caller's buffer. This keeps the
        // two-call API allocation-free on the successful copy path.
        size_t required = 0;
        for (const manumesh::feature::FeatureGraphEdge& source : analysis.graph.edges) {
            if (!isExportableFeatureEdge(source, mesh->mesh.vertices.size())) {
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
            if (!isExportableFeatureEdge(source, mesh->mesh.vertices.size())) {
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
        manumesh::feature::FeatureOptions cppOptions;
        std::string conversionError;
        if (!manumesh::api::readFeatureOptions(options, cppOptions, conversionError)) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, conversionError.c_str());
        }
        const manumesh::feature::FeatureAnalysis analysis =
            manumesh::feature::detectFeatureCurves(mesh->mesh, cppOptions);
        const std::vector<IndexedFeatureEdge> indexed = buildIndexedFeatureEdges(mesh->mesh, analysis);

        *edges_written = indexed.size();
        if (edge_capacity < indexed.size()) {
            return fail(context, MANUMESH_STATUS_BUFFER_TOO_SMALL, "Feature edge buffer is too small.");
        }
        for (std::size_t i = 0; i < indexed.size(); ++i) {
            copyFeatureEdgeV2(indexed[i], static_cast<std::uint64_t>(i), edges[i]);
        }
        return MANUMESH_STATUS_OK;
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus manumesh_simplify_options_init_with_size(ManuMeshSimplifyOptions* options, size_t struct_capacity) {
    return manumesh::api::initializeSimplifyOptions(options, struct_capacity);
}

ManuMeshStatus manumesh_simplify_report_init_with_size(ManuMeshSimplifyReport* report, size_t struct_capacity) {
    return manumesh::api::initializeSimplifyReport(report, struct_capacity);
}

ManuMeshStatus manumesh_mesh_stats_init_with_size(ManuMeshMeshStats* stats, size_t struct_capacity) {
    return manumesh::api::initializeMeshStats(stats, struct_capacity);
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

        manumesh::simplification::SimplifyReport cppReport;
        manumesh::simplification::QEMSimplifier simplifier(cppOptions);
        output->mesh = simplifier.simplify(input->mesh, &cppReport);
        if (report) {
            const ManuMeshStatus reportStatus = manumesh::api::fillSimplifyReport(cppReport, report, report_capacity);
            if (reportStatus != MANUMESH_STATUS_OK) {
                return fail(context, reportStatus, "Failed to initialize the simplify report output buffer.");
            }
        }
        return MANUMESH_STATUS_OK;
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
        return manumesh::api::fillMeshStats(manumesh::analysis::computeMeshStats(mesh->mesh), stats, stats_capacity);
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
