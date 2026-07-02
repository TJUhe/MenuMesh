#include "line_quadrics_qem/core/Mesh.h"
#include "line_quadrics_qem/features/FeatureDetection.h"
#include "line_quadrics_qem/simplification/Metrics.h"
#include "line_quadrics_qem/simplification/QEMSimplifier.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct CaseLine {
  std::filesystem::path relativePath;
  std::vector<std::string> fields;
};

struct SimplifiedMesh {
  lq::Mesh mesh;
  lq::SimplifyReport report;
};

std::filesystem::path dataRoot() {
#ifdef LQ_TEST_DATA_DIR
  return std::filesystem::path(LQ_TEST_DATA_DIR);
#else
  return std::filesystem::path(__FILE__).parent_path() / "data";
#endif
}

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
  std::string suffix = pattern.substr(starPos + 1);

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

lq::Mesh loadCaseMesh(const std::filesystem::path& relativePath) {
  lq::Mesh mesh;
  std::string error;
  const std::filesystem::path path = dataRoot() / relativePath;
  if (!lq::loadMesh(path.string(), mesh, &error)) {
    ADD_FAILURE() << "Failed to load " << path.string() << ": " << error;
  }
  return mesh;
}

lq::FeatureOptions circularFeatureOptions() {
  lq::FeatureOptions options;
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = 0.05;
  options.minFeatureLoopVertices = 8;
  return options;
}

int countCircularLoops(const lq::FeatureAnalysis& analysis) {
  return static_cast<int>(
      std::count_if(analysis.loops.begin(), analysis.loops.end(),
                    [](const lq::FeatureLoop& loop) { return loop.circular; }));
}

double maxCircularRelativeError(const lq::FeatureAnalysis& analysis) {
  double maxError = 0.0;
  for (const lq::FeatureLoop& loop : analysis.loops) {
    if (!loop.circular || loop.radius <= 1e-12) {
      continue;
    }
    const double relative =
        (loop.rmsRadialError + loop.rmsPlaneError) / std::max(1e-12, loop.radius);
    maxError = std::max(maxError, relative);
  }
  return maxError;
}

lq::SimplifyOptions lineOptions(double ratio) {
  lq::SimplifyOptions options;
  options.targetRatio = ratio;
  options.useLineQuadrics = true;
  options.lineWeight = 1e-3;
  options.weightMode = lq::WeightMode::Dihedral;
  options.featureBoost = 0.08;
  options.featureAngleDeg = 25.0;
  return options;
}

lq::SimplifyOptions protectedOptions(double ratio) {
  lq::SimplifyOptions options = lineOptions(ratio);
  options.preserveFeatureCurves = true;
  options.protectAllFeatureEdges = true;
  options.featureCurveWeight = 0.08;
  options.circleFitRelativeThreshold = 0.05;
  options.minFeatureLoopVertices = 8;
  return options;
}

SimplifiedMesh simplifyWithReport(const lq::Mesh& input,
                                  const lq::SimplifyOptions& options) {
  SimplifiedMesh result;
  lq::QEMSimplifier simplifier(options);
  result.mesh = simplifier.simplify(input, &result.report);
  return result;
}

double elapsedMs(std::chrono::steady_clock::time_point start,
                 std::chrono::steady_clock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

void expectBudget(const SimplifiedMesh& result, const lq::Mesh& input, double ratio) {
  EXPECT_FALSE(result.mesh.empty());
  EXPECT_EQ(result.report.initialFaces, static_cast<int>(input.faces.size()));
  EXPECT_EQ(result.report.finalFaces, static_cast<int>(result.mesh.faces.size()));
  EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
  EXPECT_LE(result.report.finalFaces,
            static_cast<int>(std::llround(input.faces.size() * ratio)) + 2);
  EXPECT_GT(result.report.collapsedEdges, 0);
}

int caseFieldInt(const CaseLine& testCase, std::size_t field, int defaultValue) {
  if (field >= testCase.fields.size()) {
    return defaultValue;
  }
  return std::stoi(testCase.fields[field]);
}

} // namespace

TEST(LineQuadricsQemDataset, AllQemTestStlInputsSimplifyAtSmokeBudget) {
  const std::vector<CaseLine> cases = readCaseLines("all_stl/cases.txt");
  ASSERT_GE(cases.size(), 100u);

  std::cout << "\nqem_test all_stl smoke simplify, ratio=0.90\n";
  std::cout << "model,vertices,input_faces,output_faces,load_ms,simplify_ms\n";

  for (const CaseLine& testCase : cases) {
    SCOPED_TRACE(testCase.relativePath.generic_string());

    const auto loadStart = std::chrono::steady_clock::now();
    const lq::Mesh input = loadCaseMesh(testCase.relativePath);
    const auto loadEnd = std::chrono::steady_clock::now();
    ASSERT_FALSE(input.empty());

    const lq::SimplifyOptions options = lineOptions(0.90);
    const auto simplifyStart = std::chrono::steady_clock::now();
    const SimplifiedMesh result = simplifyWithReport(input, options);
    const auto simplifyEnd = std::chrono::steady_clock::now();

    expectBudget(result, input, 0.90);
    const lq::MeshStats inputStats = lq::computeMeshStats(input);
    const lq::MeshStats outputStats = lq::computeMeshStats(result.mesh);
    EXPECT_LE(outputStats.nonManifoldEdges, inputStats.nonManifoldEdges)
        << "simplification should not add non-manifold edges";

    std::cout << testCase.relativePath.generic_string() << "," << input.vertices.size()
              << "," << input.faces.size() << "," << result.mesh.faces.size() << ","
              << std::fixed << std::setprecision(2) << elapsedMs(loadStart, loadEnd)
              << "," << elapsedMs(simplifyStart, simplifyEnd) << "\n";
  }
}

TEST(LineQuadricsQemDataset, CircularHoleCasesHaveAuditableFeaturePreservation) {
  for (const CaseLine& testCase : readCaseLines("circular_holes/cases.txt")) {
    SCOPED_TRACE(testCase.relativePath.generic_string());
    const lq::Mesh input = loadCaseMesh(testCase.relativePath);
    ASSERT_FALSE(input.empty());

    const int minCircularLoops = caseFieldInt(testCase, 0, 1);
    const int minFeatureEdges = caseFieldInt(testCase, 1, 1);
    const lq::FeatureAnalysis originalFeatures =
        lq::detectFeatureCurves(input, circularFeatureOptions());
    ASSERT_GE(originalFeatures.featureEdges, minFeatureEdges);
    ASSERT_GE(countCircularLoops(originalFeatures), minCircularLoops);

    const SimplifiedMesh line = simplifyWithReport(input, lineOptions(0.80));
    const SimplifiedMesh protectedResult =
        simplifyWithReport(input, protectedOptions(0.80));

    expectBudget(line, input, 0.80);
    expectBudget(protectedResult, input, 0.80);
    EXPECT_GT(protectedResult.report.featureLoops, 0);
    EXPECT_GT(protectedResult.report.circularFeatureLoops, 0);
    EXPECT_GT(protectedResult.report.projectedFeaturePlacements, 0);
    EXPECT_GT(protectedResult.report.featureRejectedCollapses, 0);

    const lq::FeatureAnalysis lineFeatures =
        lq::detectFeatureCurves(line.mesh, circularFeatureOptions());
    const lq::FeatureAnalysis protectedFeatures =
        lq::detectFeatureCurves(protectedResult.mesh, circularFeatureOptions());
    const int protectedCircularLoops = countCircularLoops(protectedFeatures);
    EXPECT_GE(protectedCircularLoops, std::max(1, minCircularLoops / 2));
    EXPECT_GE(protectedCircularLoops + 2, countCircularLoops(lineFeatures));
    EXPECT_LT(maxCircularRelativeError(protectedFeatures), 0.08);
  }
}

TEST(LineQuadricsQemDataset, ClosedTopologyCasesStayClosedAfterLineSimplify) {
  for (const CaseLine& testCase : readCaseLines("closed_topology/cases.txt")) {
    SCOPED_TRACE(testCase.relativePath.generic_string());
    const lq::Mesh input = loadCaseMesh(testCase.relativePath);
    ASSERT_FALSE(input.empty());

    const lq::MeshStats inputStats = lq::computeMeshStats(input);
    ASSERT_EQ(inputStats.boundaryEdges, 0);
    ASSERT_EQ(inputStats.nonManifoldEdges, 0);

    const SimplifiedMesh result = simplifyWithReport(input, lineOptions(0.80));
    expectBudget(result, input, 0.80);

    const lq::MeshStats outputStats = lq::computeMeshStats(result.mesh);
    EXPECT_EQ(outputStats.boundaryEdges, 0);
    EXPECT_EQ(outputStats.nonManifoldEdges, 0);
  }
}

TEST(LineQuadricsQemDataset, NonCircularHardFeatureCasesExerciseProtection) {
  for (const CaseLine& testCase : readCaseLines("non_circular_features/cases.txt")) {
    SCOPED_TRACE(testCase.relativePath.generic_string());
    const lq::Mesh input = loadCaseMesh(testCase.relativePath);
    ASSERT_FALSE(input.empty());

    const lq::FeatureAnalysis originalFeatures =
        lq::detectFeatureCurves(input, circularFeatureOptions());
    ASSERT_GT(originalFeatures.featureEdges, 0);

    const SimplifiedMesh protectedResult =
        simplifyWithReport(input, protectedOptions(0.85));
    expectBudget(protectedResult, input, 0.85);
    EXPECT_GT(protectedResult.report.featureLoops, 0);
    EXPECT_GT(protectedResult.report.featureVertices, 0);
    EXPECT_GT(protectedResult.report.featureRejectedCollapses, 0);
  }
}
