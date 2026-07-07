#pragma once

#include "manumesh/Export.h"
#include "manumesh/core/Mesh.h"

#include <string>

namespace manumesh::simplification {

/// Basic geometric and topological mesh quality metrics.
struct MeshStats {
  int vertices = 0;
  int faces = 0;
  int edges = 0;
  int boundaryEdges = 0;
  int nonManifoldEdges = 0;
  double area = 0.0;
  double meanTriangleQuality = 0.0;
  double minTriangleQuality = 0.0;
  double meanEdgeLength = 0.0;
  double edgeLengthCv = 0.0;
};

/// Symmetric sampled distance summary between two meshes.
struct DistanceStats {
  double meanOriginalToSimplified = 0.0;
  double maxOriginalToSimplified = 0.0;
  double meanSimplifiedToOriginal = 0.0;
  double maxSimplifiedToOriginal = 0.0;
};

/// Computes mesh quality and topology statistics.
MANUMESH_API MeshStats computeMeshStats(const Mesh& mesh);
/// Estimates bidirectional mesh distance using deterministic vertex samples.
MANUMESH_API DistanceStats compareMeshesBySampledDistance(const Mesh& original,
                                                          const Mesh& simplified,
                                                          int maxSamples);

/// CSV header for mesh statistics rows.
MANUMESH_API std::string statsHeaderCsv();
/// CSV row for a labeled mesh-statistics record.
MANUMESH_API std::string statsRowCsv(const std::string& label, const MeshStats& stats,
                                     const DistanceStats* distance = nullptr);

} // namespace manumesh::simplification
