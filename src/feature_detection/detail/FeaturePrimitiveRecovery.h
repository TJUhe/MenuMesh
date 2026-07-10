#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

void recoverPrimitiveComponents(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
);

} // namespace manumesh::feature::detector_detail
