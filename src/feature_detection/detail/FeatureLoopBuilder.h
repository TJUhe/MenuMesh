#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace manumesh::feature::detector_detail {

std::string cycleSignature(const std::vector<int>& vertices);

void assignLoopToVertices(const FeatureLoop& loop, const Mesh& mesh,
                          const std::vector<std::vector<int>>& adjacency,
                          FeatureAnalysis& analysis);

FeatureLoop makeLoopFromStats(std::vector<int> vertices, int loopId,
                              const TraceLoopStats& stats);

void addTracedLoop(const Mesh& mesh, const FeatureOptions& options,
                   const std::vector<std::vector<int>>& adjacency,
                   std::vector<int> vertices, const TraceLoopStats& stats,
                   FeatureAnalysis& analysis, int& loopId);

bool addRecoveredCycle(RecoveredCycleKind kind, std::vector<int> vertices,
                       std::unordered_set<std::string>& seenCycles, const Mesh& mesh,
                       const FeatureOptions& options, const TraceGraph& trace,
                       FeatureAnalysis& analysis, int& loopId);

} // namespace manumesh::feature::detector_detail
