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

/**
 * @brief One mesh edge carrying the evidence channels collected by detection.
 */
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

/**
 * @brief Per-edge evidence attributes stored once per trace-graph edge.
 *
 * Earlier revisions kept eleven parallel hash maps with identical keys; one
 * struct per key halves memory traffic and lets hot loops fetch every
 * attribute with a single lookup.
 */
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

/**
 * @brief Compact feature graph used by cleanup, tracing, and loop recovery.
 */
struct TraceGraph {
    std::vector<std::vector<int>> adjacency;
    std::vector<char> traceVertex;
    std::unordered_map<std::uint64_t, TraceEdgeAttrs> edgeAttrs;
    std::vector<std::pair<int, int>> graphEdges;
};

/**
 * @brief Evidence counters accumulated while traversing one graph chain.
 */
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

/**
 * @brief Recovery source used to select acceptance policy for a cycle.
 */
enum class RecoveredCycleKind {
    Circular,
    Polygonal,
};

/**
 * @brief Mutable accumulator that centralizes FeatureAnalysis bookkeeping.
 */
class FeatureAnalysisBuilder {
public:
    /** @brief Allocates one public feature record per mesh vertex. */
    explicit FeatureAnalysisBuilder(int vertexCount) { analysis_.vertices.assign(vertexCount, VertexFeature{}); }

    /** @brief Returns the analysis currently being accumulated. */
    FeatureAnalysis& analysis() { return analysis_; }
    /** @brief Returns a read-only view of the accumulated analysis. */
    const FeatureAnalysis& analysis() const { return analysis_; }

    /** @brief Returns the monotonic loop-id counter used by recovery stages. */
    int& nextLoopId() { return nextLoopId_; }

    /** @brief Transfers the completed analysis to the caller. */
    FeatureAnalysis build() { return std::move(analysis_); }

    /** @brief Adds one accepted edge to all matching evidence counters. */
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
