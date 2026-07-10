#pragma once

#include <string>
#include <vector>

namespace manumesh {
struct Mesh;
}

namespace manumesh::feature {
struct FeatureAnalysis;
}

namespace manumesh::debugUtil {

/// Debug drawing intent for color and line-width selection.
///
/// The values are semantic rather than algorithm-specific, so callers can use
/// the same palette when inspecting feature detection, collapse rejection,
/// topology warnings, or before/after simplification snapshots.
enum class UseCase {
    /// Ordinary mesh wireframe.
    Mesh,
    /// Boundary edge or open-border evidence.
    Boundary,
    /// Strong feature edge such as dihedral evidence.
    Feature,
    /// Weak feature evidence, for example normal-tensor-only support.
    WeakFeature,
    /// Recovered or user-visible feature loop.
    FeatureLoop,
    /// Candidate edge being inspected before acceptance/rejection.
    Candidate,
    /// Accepted result edge, commonly used for simplified output.
    Accepted,
    /// Rejected edge or failed collapse candidate.
    Rejected,
    /// Suspicious edge that may need closer inspection.
    Warning,
    /// Definite error evidence such as non-manifold topology.
    Error,
};

/// Optional colored edge overlay drawn on top of the base wireframe.
///
/// Vertex indices are mesh-local. Invalid indices are ignored instead of
/// failing the caller's debug run, because this utility is often placed inside
/// error-handling paths.
struct EdgeOverlay {
    /// First vertex index.
    int a = -1;
    /// Second vertex index.
    int b = -1;
    /// Semantic color/width for this overlay.
    UseCase useCase = UseCase::Feature;
    /// Optional label drawn near the edge midpoint.
    std::string label;
};

#if defined(MANUMESH_ENABLE_DEBUG_UTIL) && !defined(NDEBUG)

/// Write a local interactive HTML wireframe for a mesh.
void showWireframe(const char* tag, const Mesh& mesh, UseCase useCase = UseCase::Mesh);
/// Write a local HTML snapshot with one highlighted edge.
void showEdge(const char* tag, const Mesh& mesh, int a, int b, UseCase useCase, const char* label = nullptr);
/// Write a local HTML snapshot with multiple highlighted edges.
void showEdges(
    const char* tag, const Mesh& mesh, const std::vector<EdgeOverlay>& overlays, UseCase baseUseCase = UseCase::Mesh
);
/// Write a local HTML snapshot of feature graph and recovered loop overlays.
void showFeatures(const char* tag, const Mesh& mesh, const feature::FeatureAnalysis& analysis);
/// Write a local HTML snapshot with before and after meshes side by side.
void showBeforeAfter(const char* tag, const Mesh& before, const Mesh& after);

#endif

} // namespace manumesh::debugUtil

// These macros are intentionally available to all internal C++ sources through
// the CMake force-include hook. They compile to no-ops unless both the CMake
// debug utility option and a non-NDEBUG build are active.
#if defined(MANUMESH_ENABLE_DEBUG_UTIL) && !defined(NDEBUG)

#define MANUMESH_DEBUG_UTIL_WIREFRAME(tag, mesh) ::manumesh::debugUtil::showWireframe((tag), (mesh))

#define MANUMESH_DEBUG_UTIL_WIREFRAME_AS(tag, mesh, useCase)                                                           \
    ::manumesh::debugUtil::showWireframe((tag), (mesh), (useCase))

#define MANUMESH_DEBUG_UTIL_EDGE(tag, mesh, a, b, useCase)                                                             \
    ::manumesh::debugUtil::showEdge((tag), (mesh), (a), (b), (useCase))

#define MANUMESH_DEBUG_UTIL_EDGE_LABEL(tag, mesh, a, b, useCase, label)                                                \
    ::manumesh::debugUtil::showEdge((tag), (mesh), (a), (b), (useCase), (label))

#define MANUMESH_DEBUG_UTIL_EDGES(tag, mesh, overlays) ::manumesh::debugUtil::showEdges((tag), (mesh), (overlays))

#define MANUMESH_DEBUG_UTIL_FEATURES(tag, mesh, analysis) ::manumesh::debugUtil::showFeatures((tag), (mesh), (analysis))

#define MANUMESH_DEBUG_UTIL_BEFORE_AFTER(tag, beforeMesh, afterMesh)                                                   \
    ::manumesh::debugUtil::showBeforeAfter((tag), (beforeMesh), (afterMesh))

#else

#define MANUMESH_DEBUG_UTIL_WIREFRAME(tag, mesh) ((void)0)
#define MANUMESH_DEBUG_UTIL_WIREFRAME_AS(tag, mesh, useCase) ((void)0)
#define MANUMESH_DEBUG_UTIL_EDGE(tag, mesh, a, b, useCase) ((void)0)
#define MANUMESH_DEBUG_UTIL_EDGE_LABEL(tag, mesh, a, b, useCase, label) ((void)0)
#define MANUMESH_DEBUG_UTIL_EDGES(tag, mesh, overlays) ((void)0)
#define MANUMESH_DEBUG_UTIL_FEATURES(tag, mesh, analysis) ((void)0)
#define MANUMESH_DEBUG_UTIL_BEFORE_AFTER(tag, beforeMesh, afterMesh) ((void)0)

#endif
