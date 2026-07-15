#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "common/detail/MeshQueries.h"

namespace manumesh::feature::detector_detail {

FeatureNormalFilterResult filterFeatureNormalsImpl(
    const Mesh& mesh, const common::MeshEdgeInfoMap& edgeInfo, const FeatureNormalFilterOptions& options
);

} // namespace manumesh::feature::detector_detail
