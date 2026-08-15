/**
 * @file src/feature_detection/detail/FeatureDebugInstrumentation.h
 * @brief Adapts feature-analysis diagnostics to the generic debug overlay sink.
 * @ingroup manumesh_feature_detection
 */

#pragma once

#if defined(MANUMESH_ENABLE_DEBUG_UTIL) && !defined(NDEBUG)
#include "algorithms/feature_detection/FeatureTypes.h"
#include "core/Mesh.h"
#include "debugUtil/debugUtil.h"

#include <vector>
#else
namespace manumesh {
struct Mesh;
}

namespace manumesh {
namespace feature {
struct FeatureAnalysis;
} // namespace feature
} // namespace manumesh
#endif

namespace manumesh {
namespace feature {
namespace detector_detail {

#if defined(MANUMESH_ENABLE_DEBUG_UTIL) && !defined(NDEBUG)

inline bool validFeatureDebugEdge(const Mesh& mesh, int a, int b) {
    return a >= 0 && b >= 0 && a < static_cast<int>(mesh.vertices.size()) &&
           b < static_cast<int>(mesh.vertices.size()) && a != b;
}

inline debugUtil::UseCase featureDebugUseCase(const FeatureGraphEdge& edge) {
    if (edge.removedByCleanup) {
        return debugUtil::UseCase::Warning;
    }
    if (edge.nonManifold) {
        return debugUtil::UseCase::Error;
    }
    if (edge.boundary) {
        return debugUtil::UseCase::Boundary;
    }
    if (edge.normalTensor && !edge.dihedral) {
        return debugUtil::UseCase::WeakFeature;
    }
    if (edge.cleanupBridge) {
        return debugUtil::UseCase::Candidate;
    }
    return debugUtil::UseCase::Feature;
}

inline void showFeatureDebugSnapshot(const char* tag, const Mesh& mesh, const FeatureAnalysis& analysis) {
    if (!debugUtil::enabled()) {
        return;
    }

    std::vector<debugUtil::EdgeOverlay> overlays;
    overlays.reserve(analysis.graph.edges.size() + analysis.loops.size() * 8);

    for (const FeatureGraphEdge& edge : analysis.graph.edges) {
        if (validFeatureDebugEdge(mesh, edge.a, edge.b)) {
            overlays.push_back({edge.a, edge.b, featureDebugUseCase(edge), {}});
        }
    }

    for (const FeatureLoop& loop : analysis.loops) {
        if (loop.vertices.size() < 2) {
            continue;
        }
        for (std::size_t i = 0; i + 1 < loop.vertices.size(); ++i) {
            const int a = loop.vertices[i];
            const int b = loop.vertices[i + 1];
            if (validFeatureDebugEdge(mesh, a, b)) {
                overlays.push_back({a, b, debugUtil::UseCase::FeatureLoop, {}});
            }
        }
        if (loop.closed && loop.vertices.size() > 2) {
            const int a = loop.vertices.back();
            const int b = loop.vertices.front();
            if (validFeatureDebugEdge(mesh, a, b)) {
                overlays.push_back({a, b, debugUtil::UseCase::FeatureLoop, {}});
            }
        }
    }

    debugUtil::showEdges(tag, mesh, overlays);
}

#else

inline void showFeatureDebugSnapshot(const char* tag, const Mesh& mesh, const FeatureAnalysis& analysis) {
    (void)tag;
    (void)mesh;
    (void)analysis;
}

#endif

} // namespace detector_detail
} // namespace feature
} // namespace manumesh

#define MANUMESH_DEBUG_UTIL_FEATURES(tag, mesh, analysis)                                                              \
    ::manumesh::feature::detector_detail::showFeatureDebugSnapshot((tag), (mesh), (analysis))
