/**
 * @file src/feature_detection/detail/FeatureDetectionTypes.h
 * @brief Declares feature detection types facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "common/detail/MathConstants.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace manumesh::feature::detector_detail {

using manumesh::common::kPi;

struct CandidateEdge {
    int a = -1;
    int b = -1;
    bool boundary = false;
    bool dihedral = false;
    bool normalTensor = false;
    bool smoothCurvature = false;
    bool nonManifold = false;
    bool cleanupBridge = false;
    bool consolidationBridge = false;
    int signedKind = 0;
    double angleRad = 0.0;
    double tensorPersistentScore = 0.0;
    int tensorPersistentScales = 0;
    double curvaturePersistentScore = 0.0;
    int curvaturePersistentScales = 0;
};

/// Per-edge evidence attributes stored once per trace-graph edge.
///
/// Earlier revisions kept eleven parallel hash maps with identical keys; one
/// struct per key halves memory traffic and lets hot loops fetch every
/// attribute with a single lookup.
struct TraceEdgeAttrs {
    bool boundary = false;
    bool dihedral = false;
    bool normalTensor = false;
    bool smoothCurvature = false;
    bool nonManifold = false;
    bool cleanupBridge = false;
    bool consolidationBridge = false;
    int signedKind = 0;
    double tensorPersistence = 0.0;
    int tensorPersistentScales = 0;
    double curvaturePersistence = 0.0;
    int curvaturePersistentScales = 0;
};

struct TraceGraph {
    std::vector<std::vector<int>> adjacency;
    std::vector<char> traceVertex;
    std::unordered_map<std::uint64_t, TraceEdgeAttrs> edgeAttrs;
    std::vector<std::pair<int, int>> graphEdges;
};

struct TraceLoopStats {
    int edgeCount = 0;
    int boundaryEdges = 0;
    int dihedralEdges = 0;
    int normalTensorEdges = 0;
    int smoothCurvatureEdges = 0;
    int nonManifoldEdges = 0;
    int cleanupBridgeEdges = 0;
    int convexEdges = 0;
    int concaveEdges = 0;
    int unknownSignedEdges = 0;
    bool closed = false;
};

enum class RecoveredCycleKind {
    Circular,
    Polygonal,
};

class FeatureAnalysisBuilder {
public:
    explicit FeatureAnalysisBuilder(int vertexCount) { analysis_.vertices.assign(vertexCount, VertexFeature{}); }

    FeatureAnalysis& analysis() { return analysis_; }
    const FeatureAnalysis& analysis() const { return analysis_; }

    int& nextLoopId() { return nextLoopId_; }

    FeatureAnalysis build() { return std::move(analysis_); }

    void recordFeatureEdge(const CandidateEdge& edge) {
        ++analysis_.featureEdges;
        if (edge.boundary)
            ++analysis_.boundaryFeatureEdges;
        if (edge.dihedral)
            ++analysis_.dihedralFeatureEdges;
        if (edge.normalTensor)
            ++analysis_.normalTensorFeatureEdges;
        if (edge.smoothCurvature)
            ++analysis_.smoothCurvatureFeatureEdges;
        if (edge.nonManifold)
            ++analysis_.nonManifoldFeatureEdges;
        if (edge.signedKind > 0)
            ++analysis_.convexFeatureEdges;
        if (edge.signedKind < 0)
            ++analysis_.concaveFeatureEdges;
        if (edge.dihedral && edge.signedKind == 0) {
            ++analysis_.unknownSignedFeatureEdges;
        }
    }

private:
    FeatureAnalysis analysis_;
    int nextLoopId_ = 0;
};

} // namespace manumesh::feature::detector_detail
