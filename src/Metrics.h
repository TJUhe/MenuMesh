#pragma once

#include "Mesh.h"

#include <string>

namespace lq {

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

struct DistanceStats {
  double meanOriginalToSimplified = 0.0;
  double maxOriginalToSimplified = 0.0;
  double meanSimplifiedToOriginal = 0.0;
  double maxSimplifiedToOriginal = 0.0;
};

MeshStats computeMeshStats(const Mesh& mesh);
DistanceStats compareMeshesBySampledDistance(const Mesh& original,
                                             const Mesh& simplified,
                                             int maxSamples);

std::string statsHeaderCsv();
std::string statsRowCsv(const std::string& label, const MeshStats& stats,
                        const DistanceStats* distance = nullptr);

}  // namespace lq
