/**
 * @file tests/support/feature_options_header_compile.cpp
 * @brief Verifies that the feature option contract is independently compilable.
 */

#include "algorithms/feature_detection/FeatureOptions.h"

#include <type_traits>

static_assert(
    std::is_default_constructible<manumesh::feature::FeatureOptions>::value,
    "FeatureOptions must be default constructible"
);
static_assert(
    std::is_default_constructible<manumesh::feature::NormalTensorOptions>::value,
    "NormalTensorOptions must be default constructible"
);
static_assert(
    std::is_default_constructible<manumesh::feature::SmoothCurvatureOptions>::value,
    "SmoothCurvatureOptions must be default constructible"
);
static_assert(manumesh::feature::kMaxNormalTensorScaleCount == 8, "normal tensor scale cap changed");

void featureOptionsHeaderCompileCheck() {
    manumesh::feature::FeatureOptions options;
    options.normalFilter.enabled = true;
    options.graphConsolidation.enabled = true;
    options.surfacePatches.enabled = true;
}
