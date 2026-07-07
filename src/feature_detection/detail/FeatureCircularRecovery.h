#pragma once

#include "FeatureDetectionTypes.h"
#include "manumesh/algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

void recoverCircularVertexClusters(const Mesh& mesh, const FeatureOptions& options,
                                   const std::vector<char>& traceVertex,
                                   const std::vector<std::vector<int>>& adjacency,
                                   FeatureAnalysis& analysis, int& loopId);

} // namespace manumesh::feature::detector_detail
