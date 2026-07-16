/**
 * @file src/api/CApi.cpp
 * @brief Implements capi facilities for ManuMesh's C-ABI module.
 * @ingroup manumesh_c_api
 *
 * @details The C boundary validates pointers and capacities, translates failures to status codes, and never permits a C++ exception to cross the ABI.
 */

#include "api/CApi.h"

#include "algorithms/analysis/MeshAnalysis.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "api/detail/CApiConverters.h"
#include "core/MeshGenerators.h"
#include "io/MeshIo.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

#ifndef MANUMESH_VERSION
#define MANUMESH_VERSION "0.0.0"
#endif

/** @brief Opaque C API context that owns the latest diagnostic message. */
struct ManuMeshContext {
    std::string lastError;
};

/** @brief Opaque C API handle owning one mutable C++ Mesh value. */
struct ManuMeshMeshHandle {
    manumesh::Mesh mesh;
};

namespace {

/** @brief Exact first-release prefix used to accept older options structs. */
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

/** @brief Exact first-release prefix used for bounded report writes. */
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

/** @brief Exact first-release prefix used for bounded mesh-statistics writes. */
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

void clearError(ManuMeshContext* context) {
    if (context) {
        context->lastError.clear();
    }
}

ManuMeshStatus fail(ManuMeshContext* context, ManuMeshStatus status, const std::string& message) {
    if (context) {
        // The string assignment may allocate; never let an OOM (or any other)
        // exception escape across the C ABI boundary. On failure, fall back to
        // an empty message: clear() releases nothing and cannot throw.
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
        // Lenient validation at the ABI boundary: zero-area faces are
        // tolerated (analysis and simplification handle them and report the
        // count); only inputs no algorithm can process are rejected.
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
    // Either output may be null so callers can request just one count.
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

} // extern "C"
