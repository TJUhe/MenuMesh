#include "api/CApi.h"

#include "algorithms/analysis/MeshAnalysis.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "api/detail/CApiConverters.h"
#include "core/MeshGenerators.h"
#include "io/MeshIo.h"

#include <algorithm>
#include <cmath>
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

void manumesh_simplify_options_init(ManuMeshSimplifyOptions* options) {
    if (!options) {
        return;
    }
    manumesh::api::initializeSimplifyOptions(*options);
}

void manumesh_simplify_report_init(ManuMeshSimplifyReport* report) {
    if (!report) {
        return;
    }
    manumesh::api::initializeSimplifyReport(*report);
}

void manumesh_mesh_stats_init(ManuMeshMeshStats* stats) {
    if (!stats) {
        return;
    }
    manumesh::api::initializeMeshStats(*stats);
}

ManuMeshStatus manumesh_simplify_mesh(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* input,
    const ManuMeshSimplifyOptions* options,
    ManuMeshMeshHandle* output,
    ManuMeshSimplifyReport* report
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
            if (!manumesh::api::validateSimplifyReportOutput(*report, outputError)) {
                return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, outputError);
            }
        }

        manumesh::simplification::SimplifyReport cppReport;
        manumesh::simplification::QEMSimplifier simplifier(cppOptions);
        output->mesh = simplifier.simplify(input->mesh, &cppReport);
        if (report) {
            manumesh::api::fillSimplifyReport(cppReport, *report);
        }
        return MANUMESH_STATUS_OK;
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

ManuMeshStatus
manumesh_compute_mesh_stats(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshMeshStats* stats) {
    clearError(context);
    if (!mesh || !stats) {
        return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, "Mesh and stats output pointers must be valid.");
    }
    try {
        std::string outputError;
        if (!manumesh::api::validateMeshStatsOutput(*stats, outputError)) {
            return fail(context, MANUMESH_STATUS_INVALID_ARGUMENT, outputError);
        }
        manumesh::api::fillMeshStats(manumesh::analysis::computeMeshStats(mesh->mesh), *stats);
        return MANUMESH_STATUS_OK;
    } catch (const std::exception& ex) {
        return translateException(context, ex);
    } catch (...) {
        return translateUnknownException(context);
    }
}

} // extern "C"
