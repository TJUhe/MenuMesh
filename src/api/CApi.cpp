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

/** @brief 保存最新诊断消息的不透明 C API 上下文。*/
struct ManuMeshContext {
    std::string lastError;
};

/** @brief 持有一个可变 C++ Mesh 值的不透明 C API 句柄。*/
struct ManuMeshMeshHandle {
    manumesh::Mesh mesh;
};

namespace {

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

constexpr std::size_t kLegacyV1SimplifyOptionsSize = sizeof(LegacyV1SimplifyOptionsLayout);
constexpr std::size_t kLegacyV1SimplifyReportSize = sizeof(LegacyV1SimplifyReportLayout);
constexpr std::size_t kLegacyV1MeshStatsSize = sizeof(LegacyV1MeshStatsLayout);

constexpr std::size_t kFeatureOptionsMinimumSize =
    offsetof(ManuMeshFeatureOptions, abi_version) + sizeof(unsigned int);

bool featureFieldPresent(const ManuMeshFeatureOptions& options, std::size_t offset, std::size_t size) {
    return options.struct_size >= offset &&
           size <= options.struct_size - offset;
}

template <typename T>
bool readFeatureField(
    const ManuMeshFeatureOptions& source,
    std::size_t offset,
    T& target
) {
    if (!featureFieldPresent(source, offset, sizeof(T))) {
        return false;
    }
    std::memcpy(&target, reinterpret_cast<const unsigned char*>(&source) + offset, sizeof(T));
    return true;
}

bool featureOptionsInitialized(const ManuMeshFeatureOptions& options) {
    return options.struct_size >= kFeatureOptionsMinimumSize &&
           options.abi_version == MANUMESH_ABI_VERSION;
}

manumesh::feature::FeatureOptions readFeatureOptions(const ManuMeshFeatureOptions* source) {
    manumesh::feature::FeatureOptions target;
    if (!source) {
        return target;
    }
    if (!featureOptionsInitialized(*source)) {
        throw std::invalid_argument(
            "ManuMeshFeatureOptions must be initialized with manumesh_feature_options_init."
        );
    }
#define READ_FEATURE(field, member)                                                                                  \
    do {                                                                                                             \
        using FieldType = decltype(target.member);                                                                   \
        FieldType value{};                                                                                            \
        if (readFeatureField(*source, offsetof(ManuMeshFeatureOptions, field), value)) {                           \
            target.member = value;                                                                                   \
        }                                                                                                            \
    } while (false)
#define READ_FEATURE_BOOL(field, member)                                                                              \
    do {                                                                                                             \
        int value = 0;                                                                                                \
        if (readFeatureField(*source, offsetof(ManuMeshFeatureOptions, field), value)) {                           \
            target.member = value != 0;                                                                              \
        }                                                                                                            \
    } while (false)
    READ_FEATURE(feature_angle_deg, featureAngleDeg);
    READ_FEATURE(loop_trace_angle_deg, loopTraceAngleDeg);
    READ_FEATURE(circle_fit_relative_threshold, circleFitRelativeThreshold);
    READ_FEATURE(ellipse_fit_relative_threshold, ellipseFitRelativeThreshold);
    READ_FEATURE(near_circle_axis_ratio_tolerance, nearCircleAxisRatioTolerance);
    READ_FEATURE(min_feature_loop_vertices, minFeatureLoopVertices);
    READ_FEATURE_BOOL(use_normal_tensor_features, useNormalTensorFeatures);
    READ_FEATURE(normal_tensor_feature_threshold, normalTensorFeatureThreshold);
    READ_FEATURE(normal_tensor_min_edge_alignment, normalTensorMinEdgeAlignment);
    READ_FEATURE(normal_tensor_smoothing_iterations, normalTensorSmoothingIterations);
    READ_FEATURE(normal_tensor_scale_count, normalTensorScaleCount);
    READ_FEATURE(normal_tensor_min_persistent_scales, normalTensorMinPersistentScales);
    READ_FEATURE_BOOL(use_smooth_curvature_features, useSmoothCurvatureFeatures);
    READ_FEATURE(smooth_curvature_feature_threshold, smoothCurvatureFeatureThreshold);
    READ_FEATURE(smooth_curvature_min_edge_alignment, smoothCurvatureMinEdgeAlignment);
    READ_FEATURE(smooth_curvature_min_tangent_consistency, smoothCurvatureMinTangentConsistency);
    READ_FEATURE(smooth_curvature_base_neighborhood_rings, smoothCurvatureBaseNeighborhoodRings);
    READ_FEATURE(smooth_curvature_scale_count, smoothCurvatureScaleCount);
    READ_FEATURE(smooth_curvature_min_persistent_scales, smoothCurvatureMinPersistentScales);
    READ_FEATURE(smooth_curvature_robust_fit_iterations, smoothCurvatureRobustFitIterations);
    READ_FEATURE_BOOL(smooth_curvature_use_stable_scale_selection, smoothCurvatureUseStableScaleSelection);
    READ_FEATURE(smooth_curvature_min_scale_stability, smoothCurvatureMinScaleStability);
    READ_FEATURE_BOOL(cleanup_feature_graph, cleanupFeatureGraph);
    READ_FEATURE(feature_graph_gap_length_ratio, featureGraphGapLengthRatio);
    READ_FEATURE(feature_graph_max_weak_spur_edges, featureGraphMaxWeakSpurEdges);
    READ_FEATURE(feature_graph_min_weak_spur_strength, featureGraphMinWeakSpurStrength);
    READ_FEATURE(feature_component_min_confidence, featureComponentMinConfidence);
    READ_FEATURE_BOOL(normal_filter_enabled, normalFilter.enabled);
    READ_FEATURE(normal_filter_iterations, normalFilter.iterations);
    READ_FEATURE(normal_filter_angle_sigma_deg, normalFilter.angleSigmaDeg);
    READ_FEATURE(normal_filter_preserve_angle_deg, normalFilter.preserveAngleDeg);
    READ_FEATURE(normal_filter_relaxation, normalFilter.relaxation);
    READ_FEATURE_BOOL(graph_consolidation_enabled, graphConsolidation.enabled);
    READ_FEATURE(graph_consolidation_gap_length_ratio, graphConsolidation.maxGapLengthRatio);
    READ_FEATURE(graph_consolidation_min_alignment, graphConsolidation.minAlignment);
#undef READ_FEATURE
#undef READ_FEATURE_BOOL
    return target;
}

void writeFeatureOptions(ManuMeshFeatureOptions* options, std::size_t writeSize) {
    std::memset(options, 0, writeSize);
    auto write = [options, writeSize](std::size_t offset, const auto& value) {
        if (offset <= writeSize && sizeof(value) <= writeSize - offset) {
            std::memcpy(reinterpret_cast<unsigned char*>(options) + offset, &value, sizeof(value));
        }
    };
    write(offsetof(ManuMeshFeatureOptions, struct_size), writeSize);
    const unsigned int version = MANUMESH_ABI_VERSION;
    write(offsetof(ManuMeshFeatureOptions, abi_version), version);
#define WRITE_FEATURE(field, value) write(offsetof(ManuMeshFeatureOptions, field), value)
    WRITE_FEATURE(feature_angle_deg, 40.0);
    WRITE_FEATURE(loop_trace_angle_deg, -1.0);
    WRITE_FEATURE(circle_fit_relative_threshold, 0.05);
    WRITE_FEATURE(ellipse_fit_relative_threshold, 0.05);
    WRITE_FEATURE(near_circle_axis_ratio_tolerance, 0.08);
    WRITE_FEATURE(min_feature_loop_vertices, 8);
    WRITE_FEATURE(use_normal_tensor_features, 1);
    WRITE_FEATURE(normal_tensor_feature_threshold, 0.16);
    WRITE_FEATURE(normal_tensor_min_edge_alignment, 0.45);
    WRITE_FEATURE(normal_tensor_smoothing_iterations, 0);
    WRITE_FEATURE(normal_tensor_scale_count, 1);
    WRITE_FEATURE(normal_tensor_min_persistent_scales, 1);
    WRITE_FEATURE(use_smooth_curvature_features, 0);
    WRITE_FEATURE(smooth_curvature_feature_threshold, 0.015);
    WRITE_FEATURE(smooth_curvature_min_edge_alignment, 0.55);
    WRITE_FEATURE(smooth_curvature_min_tangent_consistency, 0.65);
    WRITE_FEATURE(smooth_curvature_base_neighborhood_rings, 2);
    WRITE_FEATURE(smooth_curvature_scale_count, 3);
    WRITE_FEATURE(smooth_curvature_min_persistent_scales, 2);
    WRITE_FEATURE(smooth_curvature_robust_fit_iterations, 2);
    WRITE_FEATURE(smooth_curvature_use_stable_scale_selection, 0);
    WRITE_FEATURE(smooth_curvature_min_scale_stability, 0.0);
    WRITE_FEATURE(cleanup_feature_graph, 1);
    WRITE_FEATURE(feature_graph_gap_length_ratio, 1.25);
    WRITE_FEATURE(feature_graph_max_weak_spur_edges, 2);
    WRITE_FEATURE(feature_graph_min_weak_spur_strength, 0.0);
    WRITE_FEATURE(feature_component_min_confidence, 0.35);
    WRITE_FEATURE(normal_filter_enabled, 0);
    WRITE_FEATURE(normal_filter_iterations, 4);
    WRITE_FEATURE(normal_filter_angle_sigma_deg, 20.0);
    WRITE_FEATURE(normal_filter_preserve_angle_deg, 50.0);
    WRITE_FEATURE(normal_filter_relaxation, 0.8);
    WRITE_FEATURE(graph_consolidation_enabled, 0);
    WRITE_FEATURE(graph_consolidation_gap_length_ratio, 3.0);
    WRITE_FEATURE(graph_consolidation_min_alignment, 0.75);
#undef WRITE_FEATURE
}

void clearError(ManuMeshContext* context) {
    if (context) {
        context->lastError.clear();
    }
}

ManuMeshStatus fail(ManuMeshContext* context, ManuMeshStatus status, const std::string& message) {
    if (context) {
        // 字符串赋值可能分配内存；不得让内存不足或其他异常穿过 C ABI。
        // 失败时回退为空消息；clear() 不释放资源且不会抛出异常。
        try {
            context->lastError = message;
        } catch (...) {
            context->lastError.clear();
        }
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

} // 命名空间

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

ManuMeshContext* manumesh_context_create(void) { return new (std::nothrow) ManuMeshContext(); }

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
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, error.empty() ? "Mesh geometry is invalid." : error);
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
            return fail(context, MANUMESH_STATUS_IO_ERROR, error);
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
            return fail(context, MANUMESH_STATUS_IO_ERROR, error);
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

ManuMeshStatus manumesh_feature_options_init_with_size(
    ManuMeshFeatureOptions* options, size_t struct_capacity
) {
    if (!options || struct_capacity < kFeatureOptionsMinimumSize) {
        return MANUMESH_STATUS_INVALID_ARGUMENT;
    }
    try {
        const size_t writeSize = std::min(struct_capacity, sizeof(ManuMeshFeatureOptions));
        writeFeatureOptions(options, writeSize);
        return MANUMESH_STATUS_OK;
    } catch (const std::exception&) {
        return MANUMESH_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        return MANUMESH_STATUS_ALGORITHM_ERROR;
    }
}

void manumesh_feature_options_init(ManuMeshFeatureOptions* options) {
    (void)manumesh_feature_options_init_with_size(options, sizeof(ManuMeshFeatureOptions));
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
        const manumesh::feature::FeatureOptions cppOptions = readFeatureOptions(options);
        const manumesh::feature::FeatureAnalysis analysis =
            manumesh::feature::detectFeatureCurves(mesh->mesh, cppOptions);

        // Filter first, then copy directly into the caller's buffer. This keeps the
        // two-call API allocation-free on the successful copy path.
        size_t required = 0;
        for (const manumesh::feature::FeatureGraphEdge& source : analysis.graph.edges) {
            if (source.removedByCleanup || source.a == source.b || source.a < 0 || source.b < 0 ||
                source.a >= static_cast<int>(mesh->mesh.vertices.size()) ||
                source.b >= static_cast<int>(mesh->mesh.vertices.size())) {
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
            if (source.removedByCleanup || source.a == source.b || source.a < 0 || source.b < 0 ||
                source.a >= static_cast<int>(mesh->mesh.vertices.size()) ||
                source.b >= static_cast<int>(mesh->mesh.vertices.size())) {
                continue;
            }
            ManuMeshFeatureEdge& target = edges[outputIndex++];
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
                return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, conversionError);
            }
        }
        if (report) {
            std::string outputError;
            if (!manumesh::api::validateSimplifyReportOutput(report, report_capacity, outputError)) {
                return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, outputError);
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
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, outputError);
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
