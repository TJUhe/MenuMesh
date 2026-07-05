#pragma once

#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"
#include "line_quadrics_qem/core/Mesh.h"
#include "line_quadrics_qem/features/FeatureDetection.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace lq::test {

struct CaseLine {
  std::filesystem::path relativePath;
  std::vector<std::string> fields;
};

struct SimplifiedMesh {
  Mesh mesh;
  SimplifyReport report;
};

std::filesystem::path dataRoot();
std::filesystem::path externalDataRoot();

std::vector<CaseLine> readCaseLines(const std::filesystem::path& relativeCaseFile);
Mesh loadCaseMesh(const std::filesystem::path& relativePath);
Mesh loadFixtureMesh(const std::filesystem::path& relativePath);
Mesh loadExternalMesh(const std::filesystem::path& relativePath);
Mesh loadExternalStl(const std::filesystem::path& relativePath);

FeatureOptions circularFeatureOptions(double circleFitRelativeThreshold = 0.05);
int countCircularLoops(const FeatureAnalysis& analysis);
double maxCircularRelativeError(const FeatureAnalysis& analysis);

SimplifyOptions standardOptions(double ratio);
SimplifyOptions lineOptions(double ratio);
SimplifyOptions protectedOptions(double ratio);

SimplifiedMesh simplifyWithReport(const Mesh& input, const SimplifyOptions& options);
void expectBudget(const SimplifiedMesh& result, const Mesh& input, double ratio);
int caseFieldInt(const CaseLine& testCase, std::size_t field, int defaultValue);

} // namespace lq::test
