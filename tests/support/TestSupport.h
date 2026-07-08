#pragma once

#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/Mesh.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace manumesh::test {

struct CaseLine {
  std::filesystem::path relativePath;
  std::vector<std::string> fields;
};

struct SimplifiedMesh {
  Mesh mesh;
  simplification::SimplifyReport report;
};

std::filesystem::path dataRoot();
std::filesystem::path externalDataRoot();

std::vector<CaseLine> readCaseLines(const std::filesystem::path& relativeCaseFile);
Mesh loadCaseMesh(const std::filesystem::path& relativePath);
Mesh loadFixtureMesh(const std::filesystem::path& relativePath);
Mesh loadExternalMesh(const std::filesystem::path& relativePath);
Mesh loadExternalStl(const std::filesystem::path& relativePath);

simplification::SimplifyOptions standardOptions(double ratio);
simplification::SimplifyOptions lineOptions(double ratio);
simplification::SimplifyOptions protectedOptions(double ratio);

feature::FeatureOptions
circularFeatureOptions(double circleFitRelativeThreshold = 0.05);
int countCircularLoops(const feature::FeatureAnalysis& analysis);
double maxCircularRelativeError(const feature::FeatureAnalysis& analysis);

SimplifiedMesh simplifyWithReport(const Mesh& input,
                                  const simplification::SimplifyOptions& options);
void expectReportCountersConsistent(const simplification::SimplifyReport& report);
void expectBudget(const SimplifiedMesh& result, const Mesh& input, double ratio);
int caseFieldInt(const CaseLine& testCase, std::size_t field, int defaultValue);

} // namespace manumesh::test
