#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "common/detail/MathConstants.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace manumesh::feature::detector_detail {

using manumesh::detail::kPi;

struct CandidateEdge {
  int a = -1;
  int b = -1;
  bool boundary = false;
  bool dihedral = false;
  bool normalTensor = false;
  bool nonManifold = false;
  int signedKind = 0;
  double angleRad = 0.0;
};

struct TraceGraph {
  std::vector<std::vector<int>> adjacency;
  std::vector<char> traceVertex;
  std::unordered_map<std::uint64_t, bool> edgeIsBoundary;
  std::unordered_map<std::uint64_t, bool> edgeIsNormalTensor;
  std::unordered_map<std::uint64_t, int> edgeSignedKind;
  std::vector<std::pair<int, int>> graphEdges;
};

struct TraceLoopStats {
  int edgeCount = 0;
  int boundaryEdges = 0;
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
  explicit FeatureAnalysisBuilder(int vertexCount) {
    analysis_.vertices.assign(vertexCount, VertexFeature{});
  }

  FeatureAnalysis& analysis() { return analysis_; }
  const FeatureAnalysis& analysis() const { return analysis_; }

  int& nextLoopId() { return nextLoopId_; }

  FeatureAnalysis build() { return std::move(analysis_); }

  void recordFeatureEdge(const CandidateEdge& edge) {
    ++analysis_.featureEdges;
    if (edge.boundary) ++analysis_.boundaryFeatureEdges;
    if (edge.dihedral) ++analysis_.dihedralFeatureEdges;
    if (edge.normalTensor) ++analysis_.normalTensorFeatureEdges;
    if (edge.nonManifold) ++analysis_.nonManifoldFeatureEdges;
    if (edge.signedKind > 0) ++analysis_.convexFeatureEdges;
    if (edge.signedKind < 0) ++analysis_.concaveFeatureEdges;
    if (edge.dihedral && edge.signedKind == 0) {
      ++analysis_.unknownSignedFeatureEdges;
    }
  }

private:
  FeatureAnalysis analysis_;
  int nextLoopId_ = 0;
};

} // namespace manumesh::feature::detector_detail
