/**
 * @file tests/unit/apps/cli_option_binding_tests.cpp
 * @brief 验证 CLI preset 与显式用户参数的合并语义。
 * @ingroup manumesh_tests
 */

#include "CliOptionBinding.h"

#include <gtest/gtest.h>

namespace {

manumesh::simplification::SimplifyOptions parseIndustrialSafeRatio(const char* ratio) {
    manumesh::cli::Args args;
    args.values.push_back("--industrial-safe");
    if (ratio != nullptr) {
        args.values.push_back("--max-local-error-ratio");
        args.values.push_back(ratio);
    }
    return manumesh::cli::parseSimplifyOptions(args);
}

} // namespace

TEST(CliOptionBinding, IndustrialSafeProvidesLocalErrorRatioWhenUnset) {
    const manumesh::simplification::SimplifyOptions options = parseIndustrialSafeRatio(nullptr);

    EXPECT_DOUBLE_EQ(0.02, options.maxLocalErrorRatio);
}

TEST(CliOptionBinding, IndustrialSafePreservesStricterLocalErrorRatio) {
    const manumesh::simplification::SimplifyOptions options = parseIndustrialSafeRatio("0.005");

    EXPECT_DOUBLE_EQ(0.005, options.maxLocalErrorRatio);
}

TEST(CliOptionBinding, IndustrialSafeTightensLooserLocalErrorRatio) {
    const manumesh::simplification::SimplifyOptions options = parseIndustrialSafeRatio("0.05");

    EXPECT_DOUBLE_EQ(0.02, options.maxLocalErrorRatio);
}
