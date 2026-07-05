#include "TestSupport.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <utility>

namespace lq::test {
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

std::vector<std::filesystem::path> expandPattern(const std::string& pattern) {
  const std::string marker = "/**/";
  const std::size_t markerPos = pattern.find(marker);
  const std::size_t starPos = pattern.find_last_of('*');
  if (markerPos == std::string::npos || starPos == std::string::npos) {
    return {std::filesystem::path(pattern)};
  }

  const std::filesystem::path root = dataRoot() / pattern.substr(0, markerPos);
  const std::string suffix = pattern.substr(starPos + 1);

  std::vector<std::filesystem::path> paths;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::filesystem::path relative =
        std::filesystem::relative(entry.path(), dataRoot());
    const std::string text = relative.generic_string();
    if (text.size() >= suffix.size() &&
        text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0) {
      paths.push_back(relative);
    }
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

} // namespace

std::filesystem::path dataRoot() {
#ifdef LQ_TEST_DATA_DIR
  return std::filesystem::path(LQ_TEST_DATA_DIR);
#else
  return std::filesystem::path(__FILE__).parent_path().parent_path() / "data";
#endif
}

std::filesystem::path externalDataRoot() {
#ifdef LQ_TEST_EXTERNAL_DATA_DIR
  return std::filesystem::path(LQ_TEST_EXTERNAL_DATA_DIR);
#else
  return dataRoot() / "external";
#endif
}

std::vector<CaseLine> readCaseLines(const std::filesystem::path& relativeCaseFile) {
  const std::filesystem::path path = dataRoot() / "qem_test" / relativeCaseFile;
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

    for (const std::filesystem::path& expanded : expandPattern(words.front())) {
      CaseLine testCase;
      testCase.relativePath = expanded;
      testCase.fields.assign(words.begin() + 1, words.end());
      cases.push_back(std::move(testCase));
    }
  }
  EXPECT_FALSE(cases.empty()) << "No qem_test cases in " << path.string();
  return cases;
}

Mesh loadCaseMesh(const std::filesystem::path& relativePath) {
  Mesh mesh;
  std::string error;
  const std::filesystem::path path = dataRoot() / relativePath;
  if (!loadMesh(path.string(), mesh, &error)) {
    ADD_FAILURE() << "Failed to load " << path.string() << ": " << error;
  }
  return mesh;
}

Mesh loadFixtureMesh(const std::filesystem::path& relativePath) {
  Mesh mesh;
  std::string error;
  const std::filesystem::path path = dataRoot() / relativePath;
  if (!loadMesh(path.string(), mesh, &error)) {
    ADD_FAILURE() << "Failed to load fixture " << path.string() << ": " << error;
  }
  return mesh;
}

Mesh loadExternalMesh(const std::filesystem::path& relativePath) {
  Mesh mesh;
  std::string error;
  const std::filesystem::path path = externalDataRoot() / relativePath;
  if (!loadMesh(path.string(), mesh, &error)) {
    ADD_FAILURE() << "Failed to load " << path.string() << ": " << error;
  }
  return mesh;
}

Mesh loadExternalStl(const std::filesystem::path& relativePath) {
  Mesh mesh;
  std::string error;
  const std::filesystem::path path = externalDataRoot() / relativePath;
  if (!loadStl(path.string(), mesh, &error)) {
    ADD_FAILURE() << "Failed to load " << path.string() << ": " << error;
  }
  return mesh;
}

FeatureOptions circularFeatureOptions(double circleFitRelativeThreshold) {
  FeatureOptions options;
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = circleFitRelativeThreshold;
  options.minFeatureLoopVertices = 8;
  return options;
}

int countCircularLoops(const FeatureAnalysis& analysis) {
  return static_cast<int>(
      std::count_if(analysis.loops.begin(), analysis.loops.end(),
                    [](const FeatureLoop& loop) { return loop.circular; }));
}

double maxCircularRelativeError(const FeatureAnalysis& analysis) {
  double maxError = 0.0;
  for (const FeatureLoop& loop : analysis.loops) {
    if (!loop.circular || loop.radius <= 1e-12) {
      continue;
    }
    const double relative =
        (loop.rmsRadialError + loop.rmsPlaneError) / std::max(1e-12, loop.radius);
    maxError = std::max(maxError, relative);
  }
  return maxError;
}

SimplifyOptions standardOptions(double ratio) {
  SimplifyOptions options;
  options.targetRatio = ratio;
  options.useLineQuadrics = false;
  options.lineWeight = 0.0;
  return options;
}

SimplifyOptions lineOptions(double ratio) {
  SimplifyOptions options;
  options.targetRatio = ratio;
  options.useLineQuadrics = true;
  options.lineWeight = 1e-3;
  options.weightMode = WeightMode::Dihedral;
  options.featureBoost = 0.08;
  options.featureAngleDeg = 25.0;
  return options;
}

SimplifyOptions protectedOptions(double ratio) {
  SimplifyOptions options = lineOptions(ratio);
  options.preserveFeatureCurves = true;
  options.protectAllFeatureEdges = true;
  options.featureCurveWeight = 0.08;
  options.circleFitRelativeThreshold = 0.05;
  options.minFeatureLoopVertices = 8;
  return options;
}

SimplifiedMesh simplifyWithReport(const Mesh& input, const SimplifyOptions& options) {
  SimplifiedMesh result;
  QEMSimplifier simplifier(options);
  result.mesh = simplifier.simplify(input, &result.report);
  expectReportCountersConsistent(result.report);
  return result;
}

void expectReportCountersConsistent(const SimplifyReport& report) {
  const int rejectionTotal =
      report.featureRejectedCollapses + report.boundaryRejectedCollapses +
      report.topologyRejectedCollapses + report.normalFlipRejectedCollapses +
      report.qualityRejectedCollapses + report.selfIntersectionRejectedCollapses +
      report.curveBudgetRejectedCollapses + report.errorRejectedCollapses;
  EXPECT_EQ(report.rejectedCollapses, rejectionTotal);

  const int featureSubtypeTotal =
      report.primitiveFeatureRejectedCollapses + report.genericFeatureRejectedCollapses;
  EXPECT_EQ(report.featureRejectedCollapses, featureSubtypeTotal);
}

void expectBudget(const SimplifiedMesh& result, const Mesh& input, double ratio) {
  expectReportCountersConsistent(result.report);
  EXPECT_FALSE(result.mesh.empty());
  EXPECT_EQ(result.report.initialFaces, static_cast<int>(input.faces.size()));
  EXPECT_EQ(result.report.finalFaces, static_cast<int>(result.mesh.faces.size()));
  EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
  EXPECT_LE(result.report.finalFaces,
            static_cast<int>(std::llround(input.faces.size() * ratio)) + 2);
  EXPECT_GT(result.report.collapsedEdges, 0);
  EXPECT_LE(result.report.solverFallbacks,
            result.report.collapsedEdges + result.report.rejectedCollapses);
}

int caseFieldInt(const CaseLine& testCase, std::size_t field, int defaultValue) {
  if (field >= testCase.fields.size()) {
    return defaultValue;
  }
  return std::stoi(testCase.fields[field]);
}

} // namespace lq::test
