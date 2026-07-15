#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

void buildFeaturePatches(const Mesh& mesh, FeatureAnalysis& analysis, const SurfacePatchOptions& options);

} // namespace manumesh::feature::detector_detail
