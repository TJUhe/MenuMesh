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

enum class UseCase {
  Mesh,
  Boundary,
  Feature,
  WeakFeature,
  FeatureLoop,
  Candidate,
  Accepted,
  Rejected,
  Warning,
  Error,
};

struct EdgeOverlay {
  int a = -1;
  int b = -1;
  UseCase useCase = UseCase::Feature;
  std::string label;
};

#if defined(MANUMESH_ENABLE_DEBUG_UTIL) && !defined(NDEBUG)

void showWireframe(const char* tag, const Mesh& mesh, UseCase useCase = UseCase::Mesh);
void showEdge(const char* tag, const Mesh& mesh, int a, int b, UseCase useCase,
              const char* label = nullptr);
void showEdges(const char* tag, const Mesh& mesh,
               const std::vector<EdgeOverlay>& overlays,
               UseCase baseUseCase = UseCase::Mesh);
void showFeatures(const char* tag, const Mesh& mesh,
                  const feature::FeatureAnalysis& analysis);
void showBeforeAfter(const char* tag, const Mesh& before, const Mesh& after);

#endif

} // namespace manumesh::debugUtil

#if defined(MANUMESH_ENABLE_DEBUG_UTIL) && !defined(NDEBUG)

#define MANUMESH_DEBUG_UTIL_WIREFRAME(tag, mesh)                                       \
  ::manumesh::debugUtil::showWireframe((tag), (mesh))

#define MANUMESH_DEBUG_UTIL_WIREFRAME_AS(tag, mesh, useCase)                           \
  ::manumesh::debugUtil::showWireframe((tag), (mesh), (useCase))

#define MANUMESH_DEBUG_UTIL_EDGE(tag, mesh, a, b, useCase)                             \
  ::manumesh::debugUtil::showEdge((tag), (mesh), (a), (b), (useCase))

#define MANUMESH_DEBUG_UTIL_EDGE_LABEL(tag, mesh, a, b, useCase, label)                \
  ::manumesh::debugUtil::showEdge((tag), (mesh), (a), (b), (useCase), (label))

#define MANUMESH_DEBUG_UTIL_EDGES(tag, mesh, overlays)                                 \
  ::manumesh::debugUtil::showEdges((tag), (mesh), (overlays))

#define MANUMESH_DEBUG_UTIL_FEATURES(tag, mesh, analysis)                              \
  ::manumesh::debugUtil::showFeatures((tag), (mesh), (analysis))

#define MANUMESH_DEBUG_UTIL_BEFORE_AFTER(tag, beforeMesh, afterMesh)                   \
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
