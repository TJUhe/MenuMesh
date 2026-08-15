/**
 * @file tests/support/TestSupport.h
 * @brief 验证 ManuMesh 测试中的测试支持行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#pragma once

#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/Mesh.h"

#include "core/Filesystem.h"
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace manumesh {
namespace test {

struct CaseLine {
    manumesh::filesystem::path relativePath;
    std::vector<std::string> fields;
};

struct SimplifiedMesh {
    Mesh mesh;
    simplification::SimplifyReport report;
};

struct FeatureLabels {
    std::vector<std::pair<int, int>> edges;
    std::vector<int> junctions;
};

manumesh::filesystem::path dataRoot();
manumesh::filesystem::path externalDataRoot();

std::vector<CaseLine> readCaseLines(const manumesh::filesystem::path& relativeCaseFile);
FeatureLabels readFeatureLabels(const manumesh::filesystem::path& relativeLabelFile);
Mesh loadCaseMesh(const manumesh::filesystem::path& relativePath);
Mesh loadFixtureMesh(const manumesh::filesystem::path& relativePath);
Mesh loadExternalMesh(const manumesh::filesystem::path& relativePath);
Mesh loadExternalStl(const manumesh::filesystem::path& relativePath);

simplification::SimplifyOptions standardOptions(double ratio);
simplification::SimplifyOptions lineOptions(double ratio);
simplification::SimplifyOptions protectedOptions(double ratio);

feature::FeatureOptions circularFeatureOptions(double circleFitRelativeThreshold = 0.05);
int countCircularLoops(const feature::FeatureAnalysis& analysis);
double maxCircularRelativeError(const feature::FeatureAnalysis& analysis);

SimplifiedMesh simplifyWithReport(const Mesh& input, const simplification::SimplifyOptions& options);
void expectReportCountersConsistent(const simplification::SimplifyReport& report);
void expectBudget(const SimplifiedMesh& result, const Mesh& input, double ratio);
int caseFieldInt(const CaseLine& testCase, std::size_t field, int defaultValue);

} // namespace test
} // namespace manumesh
