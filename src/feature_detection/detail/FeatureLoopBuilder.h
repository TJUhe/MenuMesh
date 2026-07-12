#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace manumesh::feature::detector_detail {

/// Order-independent identity of a traced cycle: its sorted undirected edge
/// keys. Replaces the earlier string concatenation, which allocated and
/// formatted one text buffer per candidate cycle.
using CycleSignature = std::vector<std::uint64_t>;

struct CycleSignatureHash {
    std::size_t operator()(const CycleSignature& signature) const;
};

using CycleSignatureSet = std::unordered_set<CycleSignature, CycleSignatureHash>;

CycleSignature cycleSignature(const std::vector<int>& vertices);

void assignLoopToVertices(
    const FeatureLoop& loop, const Mesh& mesh, const std::vector<std::vector<int>>& adjacency, FeatureAnalysis& analysis
);

FeatureLoop makeLoopFromStats(std::vector<int> vertices, int loopId, const TraceLoopStats& stats);

void addTracedLoop(
    const Mesh& mesh,
    const FeatureOptions& options,
    const std::vector<std::vector<int>>& adjacency,
    std::vector<int> vertices,
    const TraceLoopStats& stats,
    FeatureAnalysis& analysis,
    int& loopId
);

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
