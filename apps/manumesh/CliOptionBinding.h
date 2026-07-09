#pragma once

#include "CliArguments.h"
#include "algorithms/feature_detection/FeatureTypes.h"
#include "algorithms/simplification/SimplificationTypes.h"

namespace manumesh::cli {

simplification::SimplifyOptions parseSimplifyOptions(const Args& args);
feature::FeatureOptions parseFeatureOptions(const Args& args);

} // namespace manumesh::cli
