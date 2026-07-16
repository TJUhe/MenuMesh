/**
 * @file src/feature_detection/detail/FeatureLoopBuilder.h
 * @brief Declares feature loop builder facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace manumesh::feature::detector_detail {

/**
 * @brief Order-independent identity of a traced cycle: its sorted undirected edge
 * keys. Replaces the earlier string concatenation, which allocated and
 * formatted one text buffer per candidate cycle.
 */
using CycleSignature = std::vector<std::uint64_t>;

/**
 * @brief Hashes canonical cycle signatures for duplicate suppression.
 */
struct CycleSignatureHash {
    /**
     * @return Stable hash of a canonical sorted edge-key sequence.
     */
    std::size_t operator()(const CycleSignature& signature) const;
};

using CycleSignatureSet = std::unordered_set<CycleSignature, CycleSignatureHash>;

/**
 * @return Canonical order-independent undirected-edge signature of a cycle.
 */
CycleSignature cycleSignature(const std::vector<int>& vertices);

/**
 * @brief Writes loop ownership, primitive projection data, and tangents to vertices.
 */
void assignLoopToVertices(
    const FeatureLoop& loop, const Mesh& mesh, const std::vector<std::vector<int>>& adjacency, FeatureAnalysis& analysis
);

/**
 * @brief Constructs a public loop record from traced vertices and evidence counts.
 */
FeatureLoop makeLoopFromStats(std::vector<int> vertices, int loopId, const TraceLoopStats& stats);

/**
 * @brief Fits, validates, records, and assigns one directly traced chain or loop.
 */
void addTracedLoop(
    const Mesh& mesh,
    const FeatureOptions& options,
    const std::vector<std::vector<int>>& adjacency,
    std::vector<int> vertices,
    const TraceLoopStats& stats,
    FeatureAnalysis& analysis,
    int& loopId
);

/**
 * @brief Deduplicates and conditionally materializes one recovered cycle.
 * @return true only when a new accepted loop was appended.
 */
bool addRecoveredCycle(
    RecoveredCycleKind kind,
    std::vector<int> vertices,
    CycleSignatureSet& seenCycles,
    const Mesh& mesh,
    const FeatureOptions& options,
    const TraceGraph& trace,
    FeatureAnalysis& analysis,
    int& loopId
);

} // namespace manumesh::feature::detector_detail
