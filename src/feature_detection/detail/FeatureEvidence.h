#pragma once

#include "FeatureDetectionTypes.h"
#include "manumesh/algorithms/feature_detection/FeatureTypes.h"

#include <vector>

namespace manumesh::feature::detector_detail {

std::vector<CandidateEdge> collectFeatureEdges(const Mesh& mesh,
                                               const FeatureOptions& options,
                                               FeatureAnalysisBuilder& builder);

} // namespace manumesh::feature::detector_detail
