/**
 * @file tests/unit/apps/cli_option_binding_tests.cpp
 * @brief 验证 CLI preset 与显式用户参数的合并语义。
 * @ingroup manumesh_tests
 */

#include "CliCsv.h"
#include "CliOptionBinding.h"
#include "core/Filesystem.h"

#include <gtest/gtest.h>

#include <fstream>
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

TEST(CliOptionBinding, IndustrialSafePreservesStricterLocalErrorRatio) {
    const manumesh::simplification::SimplifyOptions options = parseIndustrialSafeRatio("0.005");

    EXPECT_DOUBLE_EQ(0.005, options.maxLocalErrorRatio);
}

TEST(CliOptionBinding, IndustrialSafeTightensLooserLocalErrorRatio) {
    const manumesh::simplification::SimplifyOptions options = parseIndustrialSafeRatio("0.05");

    EXPECT_DOUBLE_EQ(0.02, options.maxLocalErrorRatio);
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
