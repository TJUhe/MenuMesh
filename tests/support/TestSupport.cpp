/**
 * @file tests/support/TestSupport.cpp
 * @brief 验证 ManuMesh 测试中的测试支持行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#include "TestSupport.h"

#include "io/MeshIo.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <utility>

namespace manumesh {
namespace test {
namespace {

std::vector<std::string> splitWords(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> words;
    std::string word;
    while (in >> word) {
        words.push_back(word);
    }
    return words;
}

manumesh::filesystem::path relativeTo(const manumesh::filesystem::path& path, const manumesh::filesystem::path& base) {
    manumesh::filesystem::path::const_iterator pathIt = path.begin();
    manumesh::filesystem::path::const_iterator baseIt = base.begin();
    while (pathIt != path.end() && baseIt != base.end() && *pathIt == *baseIt) {
        ++pathIt;
        ++baseIt;
    }
    if (baseIt != base.end()) {
        return path;
    }

    manumesh::filesystem::path relative;
    for (; pathIt != path.end(); ++pathIt) {
        relative /= *pathIt;
    }
    return relative;
}

std::vector<manumesh::filesystem::path> expandPattern(const std::string& pattern) {
    const std::string marker = "/**/";
    const std::size_t markerPos = pattern.find(marker);
    const std::size_t starPos = pattern.find_last_of('*');
    if (markerPos == std::string::npos || starPos == std::string::npos) {
        return {manumesh::filesystem::path(pattern)};
    }

    const manumesh::filesystem::path root = dataRoot() / pattern.substr(0, markerPos);
    const std::string suffix = pattern.substr(starPos + 1);

    std::vector<manumesh::filesystem::path> paths;
    for (const manumesh::filesystem::directory_entry& entry :
         manumesh::filesystem::recursive_directory_iterator(root)) {
        if (!manumesh::filesystem::is_regular_file(entry.path())) {
            continue;
        }
        const manumesh::filesystem::path relative = relativeTo(entry.path(), dataRoot());
        const std::string text = relative.generic_string();
        if (text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0) {
            paths.push_back(relative);
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

} // 命名空间

manumesh::filesystem::path dataRoot() {
#ifdef MANUMESH_TEST_DATA_DIR
    return manumesh::filesystem::path(MANUMESH_TEST_DATA_DIR);
#else
    return manumesh::filesystem::path(__FILE__).parent_path().parent_path() / "data";
#endif
}

manumesh::filesystem::path externalDataRoot() {
#ifdef MANUMESH_TEST_EXTERNAL_DATA_DIR
    return manumesh::filesystem::path(MANUMESH_TEST_EXTERNAL_DATA_DIR);
#else
    return dataRoot() / "external";
#endif
}

std::vector<CaseLine> readCaseLines(const manumesh::filesystem::path& relativeCaseFile) {
    const manumesh::filesystem::path path = dataRoot() / "qem_test" / relativeCaseFile;
    std::ifstream in(path);
    EXPECT_TRUE(in) << "Failed to open qem_test case file: " << path.string();

    std::vector<CaseLine> cases;
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        const std::vector<std::string> words = splitWords(line);
        if (words.empty()) {
            continue;
        }

        for (const manumesh::filesystem::path& expanded : expandPattern(words.front())) {
            CaseLine testCase;
            testCase.relativePath = expanded;
            testCase.fields.assign(words.begin() + 1, words.end());
            cases.push_back(std::move(testCase));
        }
    }
    EXPECT_FALSE(cases.empty()) << "No qem_test cases in " << path.string();
    return cases;
}

FeatureLabels readFeatureLabels(const manumesh::filesystem::path& relativeLabelFile) {
    const manumesh::filesystem::path path = dataRoot() / relativeLabelFile;
    std::ifstream in(path);
    EXPECT_TRUE(in) << "Failed to open feature label file: " << path.string();

    FeatureLabels labels;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::replace(line.begin(), line.end(), ',', ' ');
        const std::vector<std::string> fields = splitWords(line);
        if (fields.size() < 2 || fields[0] == "a") {
            continue;
        }
        if (fields[0] == "junction") {
            labels.junctions.push_back(std::stoi(fields[1]));
        } else {
            labels.edges.emplace_back(std::stoi(fields[0]), std::stoi(fields[1]));
        }
    }
    EXPECT_FALSE(labels.edges.empty()) << "No edge labels in " << path.string();
    return labels;
}

Mesh loadCaseMesh(const manumesh::filesystem::path& relativePath) {
    Mesh mesh;
    std::string error;
    const manumesh::filesystem::path path = dataRoot() / relativePath;
    if (!loadMesh(path.string(), mesh, &error)) {
        ADD_FAILURE() << "Failed to load " << path.string() << ": " << error;
    }
    return mesh;
}

Mesh loadFixtureMesh(const manumesh::filesystem::path& relativePath) {
    Mesh mesh;
    std::string error;
    const manumesh::filesystem::path path = dataRoot() / relativePath;
    if (!loadMesh(path.string(), mesh, &error)) {
        ADD_FAILURE() << "Failed to load fixture " << path.string() << ": " << error;
    }
    return mesh;
}

Mesh loadExternalMesh(const manumesh::filesystem::path& relativePath) {
    Mesh mesh;
    std::string error;
    const manumesh::filesystem::path path = externalDataRoot() / relativePath;
    if (!loadMesh(path.string(), mesh, &error)) {
        ADD_FAILURE() << "Failed to load " << path.string() << ": " << error;
    }
    return mesh;
}

Mesh loadExternalStl(const manumesh::filesystem::path& relativePath) {
    Mesh mesh;
    std::string error;
    const manumesh::filesystem::path path = externalDataRoot() / relativePath;
    if (!loadStl(path.string(), mesh, &error)) {
        ADD_FAILURE() << "Failed to load " << path.string() << ": " << error;
    }
    return mesh;
}

feature::FeatureOptions circularFeatureOptions(double circleFitRelativeThreshold) {
    feature::FeatureOptions options;
    options.featureAngleDeg = 25.0;
    options.circleFitRelativeThreshold = circleFitRelativeThreshold;
    options.minFeatureLoopVertices = 8;
    return options;
}

int countCircularLoops(const feature::FeatureAnalysis& analysis) {
    return static_cast<int>(
        std::count_if(analysis.loops.begin(), analysis.loops.end(), [](const feature::FeatureLoop& loop) {
            return loop.circular;
        })
    );
}

double maxCircularRelativeError(const feature::FeatureAnalysis& analysis) {
    double maxError = 0.0;
    for (const feature::FeatureLoop& loop : analysis.loops) {
        if (!loop.circular || loop.radius <= 1e-12) {
            continue;
        }
        const double relative = (loop.rmsRadialError + loop.rmsPlaneError) / std::max(1e-12, loop.radius);
        maxError = std::max(maxError, relative);
    }
    return maxError;
}

simplification::SimplifyOptions standardOptions(double ratio) {
    simplification::SimplifyOptions options;
    options.targetRatio = ratio;
    options.useLineQuadrics = false;
    options.lineWeight = 0.0;
    return options;
}

simplification::SimplifyOptions lineOptions(double ratio) {
    simplification::SimplifyOptions options;
    options.targetRatio = ratio;
    options.useLineQuadrics = true;
    options.lineWeight = 1e-3;
    options.weightMode = simplification::WeightMode::Dihedral;
    options.featureBoost = 0.08;
    options.featureAngleDeg = 25.0;
    return options;
}

simplification::SimplifyOptions protectedOptions(double ratio) {
    simplification::SimplifyOptions options = lineOptions(ratio);
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::AllFeatureEdges;
    options.featureCurveWeight = 0.08;
    options.circleFitRelativeThreshold = 0.05;
    options.minFeatureLoopVertices = 8;
    return options;
}

SimplifiedMesh simplifyWithReport(const Mesh& input, const simplification::SimplifyOptions& options) {
    SimplifiedMesh result;
    simplification::QEMSimplifier simplifier(options);
    result.mesh = simplifier.simplify(input, &result.report);
    expectReportCountersConsistent(result.report);
    return result;
}

void expectReportCountersConsistent(const simplification::SimplifyReport& report) {
    const int rejectionTotal = report.featureRejectedCollapses + report.boundaryRejectedCollapses +
                               report.topologyRejectedCollapses + report.normalFlipRejectedCollapses +
                               report.qualityRejectedCollapses + report.selfIntersectionRejectedCollapses +
                               report.curveBudgetRejectedCollapses + report.errorRejectedCollapses;
    EXPECT_EQ(report.rejectedCollapses, rejectionTotal);

    const int featureSubtypeTotal = report.primitiveFeatureRejectedCollapses + report.genericFeatureRejectedCollapses;
    EXPECT_EQ(report.featureRejectedCollapses, featureSubtypeTotal);
}

void expectBudget(const SimplifiedMesh& result, const Mesh& input, double ratio) {
    expectReportCountersConsistent(result.report);
    EXPECT_FALSE(result.mesh.empty());
    EXPECT_EQ(result.report.initialFaces, static_cast<int>(input.faces.size()));
    EXPECT_EQ(result.report.finalFaces, static_cast<int>(result.mesh.faces.size()));
    EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
    EXPECT_LE(result.report.finalFaces, static_cast<int>(std::llround(input.faces.size() * ratio)) + 2);
    EXPECT_GT(result.report.collapsedEdges, 0);
    EXPECT_LE(result.report.solverFallbacks, result.report.collapsedEdges + result.report.rejectedCollapses);
}

int caseFieldInt(const CaseLine& testCase, std::size_t field, int defaultValue) {
    if (field >= testCase.fields.size()) {
        return defaultValue;
    }
    return std::stoi(testCase.fields[field]);
}

} // namespace test
} // namespace manumesh
