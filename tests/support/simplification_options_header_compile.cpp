/**
 * @file tests/support/simplification_options_header_compile.cpp
 * @brief Verifies that simplification options compile without feature result types.
 */

#include "algorithms/simplification/SimplificationTypes.h"

#include <type_traits>

static_assert(
    std::is_default_constructible<manumesh::simplification::SimplifyOptions>::value,
    "SimplifyOptions must be default constructible"
);
static_assert(
    std::is_same<
        decltype(manumesh::simplification::SimplifyOptions::featureOptionsOverride),
        manumesh::Optional<manumesh::feature::FeatureOptions>>::value,
    "featureOptionsOverride must use the C++14 optional type"
);

void simplificationOptionsHeaderCompileCheck() {
    manumesh::simplification::SimplifyOptions options;
    options.featureOptionsOverride = manumesh::feature::FeatureOptions{};
}
