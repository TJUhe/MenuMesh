/**
 * @file tests/unit/apps/cli_option_binding_tests.cpp
 * @brief 验证 CLI preset 与显式用户参数的合并语义。
 * @ingroup manumesh_tests
 */

#include "CliCsv.h"
#include "CliArguments.h"
#include "CliOptionBinding.h"
#include "core/Filesystem.h"

#include <gtest/gtest.h>

#include <fstream>
#include <initializer_list>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>

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

manumesh::cli::Args makeArgs(std::initializer_list<const char*> values) {
    manumesh::cli::Args args;
    for (const char* value : values) {
        args.values.emplace_back(value);
    }
    return args;
}

class CommaDecimalPunct : public std::numpunct<char> {
protected:
    char do_decimal_point() const override { return ','; }
};

class GlobalLocaleRestore {
public:
    GlobalLocaleRestore()
        : previous_(std::locale()) {}
    ~GlobalLocaleRestore() { std::locale::global(previous_); }

private:
    std::locale previous_;
};

} // namespace

TEST(CliOptionBinding, IndustrialSafeProvidesLocalErrorRatioWhenUnset) {
    const manumesh::simplification::SimplifyOptions options = parseIndustrialSafeRatio(nullptr);

    EXPECT_DOUBLE_EQ(0.02, options.maxLocalErrorRatio);
}

TEST(CliArguments, EmptyValueTokenIsNotTreatedAsAnOmittedOption) {
    const manumesh::cli::Args args = makeArgs({"--memory-mib", ""});
    EXPECT_THROW(manumesh::cli::validateArgsForCommand("large-validate", args), std::invalid_argument);
}

TEST(CliOptionBinding, IndustrialSafePreservesStricterLocalErrorRatio) {
    const manumesh::simplification::SimplifyOptions options = parseIndustrialSafeRatio("0.005");

    EXPECT_DOUBLE_EQ(0.005, options.maxLocalErrorRatio);
}

TEST(CliOptionBinding, IndustrialSafeTightensLooserLocalErrorRatio) {
    const manumesh::simplification::SimplifyOptions options = parseIndustrialSafeRatio("0.05");

    EXPECT_DOUBLE_EQ(0.02, options.maxLocalErrorRatio);
}

TEST(CliOptionBinding, IndustrialSafeCanonicalConfigUsesRatioWhenUnset) {
    const manumesh::simplification::SimplifyConfig config =
        manumesh::cli::parseSimplifyConfig(makeArgs({"--industrial-safe"}));

    EXPECT_EQ(manumesh::simplification::SimplifyErrorLimit::Kind::BoundingBoxRatio, config.quality.localError.kind());
    EXPECT_DOUBLE_EQ(0.02, config.quality.localError.value());
}

TEST(CliOptionBinding, IndustrialSafeCanonicalConfigUsesSafeRatioForExplicitZero) {
    const manumesh::simplification::SimplifyConfig config = manumesh::cli::parseSimplifyConfig(
        makeArgs({"--industrial-safe", "--max-local-error-ratio", "0"})
    );

    EXPECT_EQ(manumesh::simplification::SimplifyErrorLimit::Kind::BoundingBoxRatio, config.quality.localError.kind());
    EXPECT_DOUBLE_EQ(0.02, config.quality.localError.value());
}

TEST(CliOptionBinding, IndustrialSafeCanonicalConfigPreservesExplicitAbsoluteError) {
    const manumesh::simplification::SimplifyConfig config =
        manumesh::cli::parseSimplifyConfig(makeArgs({"--industrial-safe", "--max-local-error", "0.01"}));

    EXPECT_EQ(manumesh::simplification::SimplifyErrorLimit::Kind::Absolute, config.quality.localError.kind());
    EXPECT_DOUBLE_EQ(0.01, config.quality.localError.value());
}

TEST(CliOptionBinding, ProfilesSelectExpectedFeatureEvidence) {
    const manumesh::feature::FeatureOptions cad = manumesh::cli::parseFeatureOptions(makeArgs({"--profile", "cad"}));
    EXPECT_FALSE(cad.useNormalTensorFeatures);
    EXPECT_FALSE(cad.useSmoothCurvatureFeatures);

    const manumesh::feature::FeatureOptions scan =
        manumesh::cli::parseFeatureOptions(makeArgs({"--profile", "scan"}));
    EXPECT_TRUE(scan.useNormalTensorFeatures);
    EXPECT_TRUE(scan.normalFilter.enabled);
    EXPECT_EQ(3, scan.normalTensorScaleCount);
    EXPECT_EQ(2, scan.normalTensorMinPersistentScales);

    const manumesh::feature::FeatureOptions smooth =
        manumesh::cli::parseFeatureOptions(makeArgs({"--profile", "smooth"}));
    EXPECT_FALSE(smooth.useNormalTensorFeatures);
    EXPECT_TRUE(smooth.useSmoothCurvatureFeatures);
}

TEST(CliOptionBinding, ExecutionDefaultsToSerialAndBindsExplicitThreadLimit) {
    const manumesh::ExecutionOptions serial = manumesh::cli::parseExecutionOptions(makeArgs({}));
    EXPECT_EQ(manumesh::ExecutionMode::Serial, serial.mode);
    EXPECT_EQ(0, serial.maxConcurrency);

    const manumesh::ExecutionOptions oneThread =
        manumesh::cli::parseExecutionOptions(makeArgs({"--threads", "1"}));
    EXPECT_EQ(manumesh::ExecutionMode::Parallel, oneThread.mode);
    EXPECT_EQ(1, oneThread.maxConcurrency);
}

TEST(CliOptionBinding, RejectsNegativeThreadLimit) {
    EXPECT_THROW(
        static_cast<void>(manumesh::cli::parseExecutionOptions(makeArgs({"--threads", "-1"}))),
        std::invalid_argument
    );
}

TEST(CliOptionBinding, ExplicitFeatureFlagsOverrideProfile) {
    const manumesh::feature::FeatureOptions options = manumesh::cli::parseFeatureOptions(
        makeArgs(
            {"--profile", "smooth", "--no-smooth-curvature-features", "--normal-tensor-features",
             "--normal-tensor-scales", "4"}
        )
    );

    EXPECT_FALSE(options.useSmoothCurvatureFeatures);
    EXPECT_TRUE(options.useNormalTensorFeatures);
    EXPECT_EQ(4, options.normalTensorScaleCount);
}

TEST(CliOptionBinding, SimplifyProfileBuildsCanonicalConfigAndOverride) {
    const manumesh::cli::Args args = makeArgs(
        {"--profile", "cad", "--ratio", "0.5", "--feature-protection-mode", "all-feature-edges"}
    );
    const manumesh::simplification::SimplifyConfig config = manumesh::cli::parseSimplifyConfig(args);
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(args);

    EXPECT_EQ(manumesh::simplification::SimplifyTarget::Kind::Ratio, config.target.kind());
    EXPECT_DOUBLE_EQ(0.5, config.target.ratio());
    EXPECT_TRUE(config.features.enabled);
    EXPECT_EQ(manumesh::simplification::WeightMode::Dihedral, config.cost.weightMode);
    EXPECT_EQ(manumesh::simplification::FeatureProtectionMode::AllFeatureEdges, config.features.protectionMode);
    ASSERT_TRUE(options.featureOptionsOverride.has_value());
    EXPECT_FALSE(options.featureOptionsOverride->useNormalTensorFeatures);
    EXPECT_EQ(8, options.featureOptionsOverride->minFeatureLoopVertices);
}

TEST(CliOptionBinding, ExplicitSimplifyDisableOverridesProfileProtection) {
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(
        makeArgs({"--profile", "scan", "--no-preserve-feature-curves"})
    );

    EXPECT_FALSE(options.preserveFeatureCurves);
    ASSERT_TRUE(options.featureOptionsOverride.has_value());
    EXPECT_TRUE(options.featureOptionsOverride->normalFilter.enabled);
}

TEST(CliOptionBinding, WarnsWhenFeatureCurveProtectionFlagsConflict) {
    const manumesh::cli::Args args =
        makeArgs({"--preserve-feature-curves", "--no-preserve-feature-curves"});
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(args);
    std::ostringstream warnings;
    manumesh::cli::emitOptionWarnings(
        args,
        *options.featureOptionsOverride,
        true,
        warnings
    );

    EXPECT_FALSE(options.preserveFeatureCurves);
    EXPECT_NE(std::string::npos, warnings.str().find("both feature-curve protection enable and disable flags"));
}

TEST(CliOptionBinding, ZeroLineWeightDisablesLineQuadrics) {
    const manumesh::simplification::SimplifyOptions uniform = manumesh::cli::parseSimplifyOptions(
        makeArgs({"--method", "line", "--line-weight", "0"})
    );
    EXPECT_FALSE(uniform.useLineQuadrics);
    EXPECT_DOUBLE_EQ(0.0, uniform.lineWeight);

    const manumesh::simplification::SimplifyOptions adaptive = manumesh::cli::parseSimplifyOptions(
        makeArgs({"--method", "line", "--adaptive-scale", "--adaptive-base-line-weight", "0"})
    );
    EXPECT_FALSE(adaptive.useLineQuadrics);
    EXPECT_FALSE(adaptive.adaptiveScale);
    EXPECT_NE(
        std::string::npos,
        manumesh::cli::formatResolvedSimplifyOptions(makeArgs({"--method", "line", "--line-weight", "0"}), uniform)
            .find("line_quadrics: enabled=off")
    );
}

TEST(CliOptionBinding, ReportsAmbiguousAndInactiveSettings) {
    const manumesh::cli::Args args = makeArgs(
        {"--target-faces", "100", "--ratio", "0.5", "--max-local-error", "0.1", "--max-local-error-ratio",
         "0.02", "--method", "standard", "--line-weight", "0.01", "--smooth-curvature-scales", "4",
         "--feature-component-min-confidence", "0.7"}
    );
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(args);
    ASSERT_TRUE(options.featureOptionsOverride.has_value());
    std::ostringstream warnings;
    manumesh::cli::emitOptionWarnings(args, *options.featureOptionsOverride, true, warnings);

    EXPECT_NE(std::string::npos, warnings.str().find("--target-faces takes precedence"));
    EXPECT_NE(std::string::npos, warnings.str().find("both local-error units"));
    EXPECT_NE(std::string::npos, warnings.str().find("line-quadric weight settings"));
    EXPECT_NE(std::string::npos, warnings.str().find("smooth-curvature values are ignored"));
    EXPECT_NE(std::string::npos, warnings.str().find("only classifies report counters"));
}

TEST(CliOptionBinding, ResolvedConfigShowsProfileAndEffectiveValues) {
    const manumesh::cli::Args args = makeArgs(
        {"--profile", "scan", "--normal-tensor-scales", "4", "--print-resolved-config"}
    );
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(args);
    const std::string text = manumesh::cli::formatResolvedSimplifyOptions(args, options);

    EXPECT_NE(std::string::npos, text.find("resolved_simplify_config profile=scan"));
    EXPECT_NE(std::string::npos, text.find("feature_protection: enabled=on mode=primitive-curves"));
    EXPECT_NE(std::string::npos, text.find("normal_tensor: enabled=on"));
    EXPECT_NE(std::string::npos, text.find("scales=4"));
}

TEST(CliOptionBinding, WarnsWhenProfileDisablesAnAdvancedChannel) {
    const manumesh::cli::Args args = makeArgs(
        {"--profile", "smooth", "--normal-tensor-scales", "3", "--no-preserve-feature-curves",
         "--feature-protection-mode", "all-feature-edges"}
    );
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(args);
    ASSERT_TRUE(options.featureOptionsOverride.has_value());
    std::ostringstream warnings;
    manumesh::cli::emitOptionWarnings(args, *options.featureOptionsOverride, true, warnings);

    EXPECT_NE(std::string::npos, warnings.str().find("normal-tensor values are ignored"));
    EXPECT_NE(std::string::npos, warnings.str().find("feature-protection settings require"));
}

TEST(CliOptionBinding, DoesNotWarnWhenNormalTensorParametersDriveSimplificationWeighting) {
    const manumesh::cli::Args args =
        makeArgs({"--profile", "cad", "--weight-mode", "normal-tensor", "--normal-tensor-scales", "4"});
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(args);
    ASSERT_TRUE(options.featureOptionsOverride.has_value());
    std::ostringstream warnings;
    manumesh::cli::emitOptionWarnings(args, *options.featureOptionsOverride, true, warnings);

    EXPECT_EQ(std::string::npos, warnings.str().find("normal-tensor values are ignored"));
}

TEST(CliOptionBinding, WarnsWhenStandardQemCannotUseNormalTensorWeighting) {
    const manumesh::cli::Args args = makeArgs(
        {"--profile", "cad", "--method", "standard", "--weight-mode", "normal-tensor", "--feature-boost", "0.2",
         "--normal-tensor-scales", "4"}
    );
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(args);
    ASSERT_TRUE(options.featureOptionsOverride.has_value());
    std::ostringstream warnings;
    manumesh::cli::emitOptionWarnings(args, *options.featureOptionsOverride, true, warnings);

    EXPECT_NE(std::string::npos, warnings.str().find("normal-tensor values are ignored"));
    EXPECT_NE(std::string::npos, warnings.str().find("including --weight-mode and --feature-boost"));
}

TEST(CliOptionBinding, DoesNotWarnForProfileNormalTensorWeightingWhenDetectionIsDisabled) {
    const manumesh::cli::Args args = makeArgs(
        {"--profile", "scan", "--no-normal-tensor-features", "--normal-tensor-scales", "4"}
    );
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(args);
    ASSERT_TRUE(options.featureOptionsOverride.has_value());
    std::ostringstream warnings;
    manumesh::cli::emitOptionWarnings(args, *options.featureOptionsOverride, true, warnings);

    EXPECT_EQ(std::string::npos, warnings.str().find("normal-tensor values are ignored"));
}

TEST(CliOptionBinding, WarnsWhenFeatureBoostIsCombinedWithUniformWeighting) {
    const manumesh::cli::Args args = makeArgs({"--weight-mode", "uniform", "--feature-boost", "0.2"});
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(args);
    ASSERT_TRUE(options.featureOptionsOverride.has_value());
    std::ostringstream warnings;
    manumesh::cli::emitOptionWarnings(args, *options.featureOptionsOverride, true, warnings);

    EXPECT_NE(std::string::npos, warnings.str().find("--feature-boost is ignored when the effective --weight-mode is uniform"));
}

TEST(CliOptionBinding, RejectsDuplicateSingleValueOptions) {
    const manumesh::cli::Args args = makeArgs({"--ratio", "0.8", "--ratio", "0.2"});

    EXPECT_THROW(manumesh::cli::parseSimplifyOptions(args), std::invalid_argument);
    EXPECT_THROW(manumesh::cli::validateArgsForCommand("simplify", args), std::invalid_argument);
}

TEST(CliOptionBinding, RejectsNonPositiveDistanceSampleCounts) {
    EXPECT_THROW(
        manumesh::cli::getIntArg(makeArgs({"--samples", "0"}), "--samples", 3000),
        std::invalid_argument
    );
    EXPECT_THROW(
        manumesh::cli::getIntArg(makeArgs({"--samples", "-1"}), "--samples", 3000),
        std::invalid_argument
    );
}

TEST(CliOptionBinding, WarnsWhenStandardQemDisablesProfileWeighting) {
    const manumesh::cli::Args args = makeArgs({"--profile", "scan", "--method", "standard"});
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(args);
    ASSERT_TRUE(options.featureOptionsOverride.has_value());
    std::ostringstream warnings;
    manumesh::cli::emitOptionWarnings(args, *options.featureOptionsOverride, true, warnings);

    EXPECT_NE(std::string::npos, warnings.str().find("line-quadric weight settings"));
    EXPECT_NE(
        std::string::npos,
        warnings.str().find("Feature detection is unaffected; feature protection follows its separate setting")
    );
}

TEST(CliOptionBinding, WarnsOnceWhenSmoothStableScaleSettingsHaveNoActiveSmoothChannel) {
    const manumesh::cli::Args args = makeArgs(
        {"--profile", "cad", "--smooth-curvature-stable-scale", "--smooth-curvature-min-scale-stability", "0.9"}
    );
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(args);
    ASSERT_TRUE(options.featureOptionsOverride.has_value());
    std::ostringstream warnings;
    manumesh::cli::emitOptionWarnings(args, *options.featureOptionsOverride, true, warnings);

    EXPECT_NE(std::string::npos, warnings.str().find("smooth-curvature values are ignored"));
    EXPECT_EQ(
        std::string::npos,
        warnings.str().find("--smooth-curvature-min-scale-stability requires --smooth-curvature-stable-scale")
    );
}

TEST(CliOptionBinding, WarnsWhenActiveSmoothChannelOmitsStableScaleSelection) {
    const manumesh::cli::Args args =
        makeArgs({"--smooth-curvature-features", "--smooth-curvature-min-scale-stability", "0.9"});
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(args);
    ASSERT_TRUE(options.featureOptionsOverride.has_value());
    std::ostringstream warnings;
    manumesh::cli::emitOptionWarnings(args, *options.featureOptionsOverride, true, warnings);

    EXPECT_NE(
        std::string::npos,
        warnings.str().find("--smooth-curvature-min-scale-stability requires --smooth-curvature-stable-scale")
    );
}

TEST(CliOptionBinding, MakesSimplificationFeatureLoopSafetyFloorVisible) {
    const manumesh::cli::Args args = makeArgs({"--preserve-feature-curves", "--min-feature-loop-vertices", "3"});
    const manumesh::simplification::SimplifyOptions options = manumesh::cli::parseSimplifyOptions(args);
    ASSERT_TRUE(options.featureOptionsOverride.has_value());
    std::ostringstream warnings;
    manumesh::cli::emitOptionWarnings(args, *options.featureOptionsOverride, true, warnings);

    EXPECT_NE(std::string::npos, warnings.str().find("safety floor at 5"));
    EXPECT_NE(std::string::npos, manumesh::cli::formatResolvedSimplifyOptions(args, options).find("feature_loop_safety_floor=5"));
}

TEST(CliCsv, RejectsMalformedQuotedFields) {
    EXPECT_THROW(manumesh::cli::splitCsvLine("one,\"unterminated"), std::invalid_argument);
    EXPECT_THROW(manumesh::cli::splitCsvLine("one,ab\"cd\""), std::invalid_argument);
    EXPECT_THROW(manumesh::cli::splitCsvLine("one,\"quoted\"tail"), std::invalid_argument);
    EXPECT_THROW(manumesh::cli::splitCsvLine("one,two\nthree"), std::invalid_argument);
    EXPECT_THROW(manumesh::cli::splitCsvLine("one,two\rthree"), std::invalid_argument);

    const std::vector<std::string> quotedNewline = manumesh::cli::splitCsvLine("one,\"two\nthree\"");
    ASSERT_EQ(2u, quotedNewline.size());
    EXPECT_EQ("two\nthree", quotedNewline[1]);
}

TEST(CliCsv, EscapesStatsLabels) {
    manumesh::analysis::MeshStats stats;
    const std::string row = manumesh::cli::statsRowCsv("case, \"A\"", stats);
    EXPECT_EQ(0u, row.find("\"case, \"\"A\"\"\""));
}

TEST(CliCsv, ReadsQuotedMultilineFirstRecord) {
    const manumesh::filesystem::path path =
        manumesh::filesystem::temp_directory_path() / "manumesh_cli_multiline_first_record.csv";
    {
        std::ofstream out(path);
        ASSERT_TRUE(out);
        out << "label,value\n";
        out << manumesh::cli::quoteCsv("first line\nsecond line") << ",42\n";
    }

    const std::map<std::string, std::string> row = manumesh::cli::readFirstCsvRow(path);
    manumesh::filesystem::remove(path);

    ASSERT_EQ(2u, row.size());
    EXPECT_EQ("first line\nsecond line", row.at("label"));
    EXPECT_EQ("42", row.at("value"));
}

TEST(CliCsv, NormalizesCrLfRecordSeparators) {
    std::istringstream input("label,value\r\ncase,42\r\n");
    std::string header;
    std::string value;

    ASSERT_TRUE(manumesh::cli::readCsvRecord(input, header));
    ASSERT_TRUE(manumesh::cli::readCsvRecord(input, value));
    EXPECT_EQ((std::vector<std::string>{"label", "value"}), manumesh::cli::splitCsvLine(header));
    EXPECT_EQ((std::vector<std::string>{"case", "42"}), manumesh::cli::splitCsvLine(value));
}

TEST(CliCsv, StatsRowsUseClassicNumericLocale) {
    GlobalLocaleRestore restore;
    std::locale::global(std::locale(std::locale(), new CommaDecimalPunct()));

    manumesh::analysis::MeshStats stats;
    stats.area = 1.5;
    const std::string row = manumesh::cli::statsRowCsv("case", stats);

    EXPECT_NE(std::string::npos, row.find(",1.5,"));
    EXPECT_EQ(std::string::npos, row.find(",1,5,"));
}

TEST(CliCsv, AtomicOutputPreservesPriorFileUntilCommit) {
    const manumesh::filesystem::path path =
        manumesh::filesystem::temp_directory_path() / "manumesh_cli_atomic_csv_output_test.csv";
    {
        std::ofstream original(path, std::ios::out | std::ios::trunc);
        ASSERT_TRUE(original);
        original << "previous\n";
    }

    {
        manumesh::cli::AtomicCsvOutput output(path);
        output.stream() << "replacement\n";
        // Destruction without commit must leave the previous complete file in place.
    }
    {
        std::ifstream input(path);
        std::string content;
        std::getline(input, content);
        EXPECT_EQ("previous", content);
    }

    {
        manumesh::cli::AtomicCsvOutput output(path);
        output.stream() << "replacement\n";
        output.commit();
    }
    {
        std::ifstream input(path);
        std::string content;
        std::getline(input, content);
        EXPECT_EQ("replacement", content);
    }
    manumesh::filesystem::remove(path);
}

TEST(CliArguments, RejectsEmptySweepLists) {
    EXPECT_THROW(manumesh::cli::parseWeights("", "--weights"), std::invalid_argument);
    EXPECT_THROW(manumesh::cli::parseWeights("", "--ratios"), std::invalid_argument);
    EXPECT_THROW(manumesh::cli::parseFaceCounts("", "--faces"), std::invalid_argument);
}
