#include "line_quadrics_qem/core/Mesh.h"
#include "line_quadrics_qem/simplification/Metrics.h"
#include "line_quadrics_qem/simplification/QEMSimplifier.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct PerformanceCase {
  std::string name;
  std::filesystem::path relativePath;
};

std::filesystem::path externalDataDir() {
  return std::filesystem::path(__FILE__).parent_path() / "data" / "external";
}

std::vector<PerformanceCase> discoverLargeMeshCases() {
  std::vector<PerformanceCase> cases;
  const std::filesystem::path root = externalDataDir();

  const std::vector<std::string> rootLargeFiles = {
      "casting_aimshape_2014.stl",
      "fandisk_2014.stl",
      "nasa_cubesat_middle.stl",
      "nasa_mars2020_wheel.stl",
  };

  for (const std::string& fileName : rootLargeFiles) {
    const std::filesystem::path path = root / fileName;
    if (std::filesystem::exists(path)) {
      cases.push_back({path.stem().string(), fileName});
    }
  }

  const std::filesystem::path largeDir = root / "large";
  if (std::filesystem::exists(largeDir)) {
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(largeDir)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".stl") {
        continue;
      }
      cases.push_back({"large/" + entry.path().stem().string(),
                       std::filesystem::path("large") / entry.path().filename()});
    }
  }

  std::sort(cases.begin(), cases.end(),
            [](const PerformanceCase& a, const PerformanceCase& b) {
              return a.relativePath.generic_string() < b.relativePath.generic_string();
            });
  return cases;
}

lq::SimplifyOptions performanceOptions() {
  lq::SimplifyOptions options;
  options.targetRatio = 0.90;
  options.useLineQuadrics = true;
  options.lineWeight = 1e-3;
  options.weightMode = lq::WeightMode::Dihedral;
  options.featureBoost = 0.08;
  options.featureAngleDeg = 25.0;
  return options;
}

double elapsedMs(std::chrono::steady_clock::time_point start,
                 std::chrono::steady_clock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

std::string propertyPrefix(std::string name) {
  for (char& ch : name) {
    const unsigned char value = static_cast<unsigned char>(ch);
    if (!std::isalnum(value)) {
      ch = '_';
    }
  }
  return name;
}

} // namespace

TEST(LineQuadricsQemPerformance, LargeExternalMeshesSimplifyAtNinetyPercent) {
  const std::vector<PerformanceCase> cases = discoverLargeMeshCases();
  ASSERT_GE(cases.size(), 14u);

  std::cout << "\nlarge mesh performance, ratio=0.90\n";
  std::cout << "model,vertices,input_faces,output_faces,load_ms,simplify_ms,"
               "faces_per_second,non_manifold_delta,boundary_delta\n";

  for (const PerformanceCase& testCase : cases) {
    SCOPED_TRACE(testCase.relativePath.generic_string());

    lq::Mesh input;
    std::string error;
    const std::filesystem::path path = externalDataDir() / testCase.relativePath;

    const auto loadStart = std::chrono::steady_clock::now();
    ASSERT_TRUE(lq::loadMesh(path.string(), input, &error)) << error;
    const auto loadEnd = std::chrono::steady_clock::now();
    ASSERT_FALSE(input.empty());

    const lq::MeshStats inputStats = lq::computeMeshStats(input);
    lq::SimplifyReport report;
    const lq::SimplifyOptions options = performanceOptions();

    const auto simplifyStart = std::chrono::steady_clock::now();
    const lq::Mesh output = lq::simplifyMesh(input, options, &report);
    const auto simplifyEnd = std::chrono::steady_clock::now();
    ASSERT_FALSE(output.empty());

    const lq::MeshStats outputStats = lq::computeMeshStats(output);
    const double loadMilliseconds = elapsedMs(loadStart, loadEnd);
    const double simplifyMilliseconds = elapsedMs(simplifyStart, simplifyEnd);
    const double facesPerSecond =
        simplifyMilliseconds > 0.0
            ? static_cast<double>(input.faces.size()) * 1000.0 / simplifyMilliseconds
            : 0.0;

    EXPECT_EQ(report.initialFaces, static_cast<int>(input.faces.size()));
    EXPECT_EQ(report.finalFaces, static_cast<int>(output.faces.size()));
    EXPECT_LE(report.finalFaces,
              static_cast<int>(std::llround(input.faces.size() * options.targetRatio)) +
                  2);
    EXPECT_GT(report.collapsedEdges, 0);
    EXPECT_LE(outputStats.nonManifoldEdges, inputStats.nonManifoldEdges)
        << "performance cases should not add non-manifold edges";
    EXPECT_LT(simplifyMilliseconds, 30000.0)
        << "large mesh simplification exceeded the generous per-model budget";

    const std::string prefix = propertyPrefix(testCase.name);
    RecordProperty(prefix + "_load_ms", loadMilliseconds);
    RecordProperty(prefix + "_simplify_ms", simplifyMilliseconds);
    RecordProperty(prefix + "_faces_per_second", facesPerSecond);

    std::cout << testCase.name << "," << input.vertices.size() << ","
              << input.faces.size() << "," << output.faces.size() << "," << std::fixed
              << std::setprecision(2) << loadMilliseconds << "," << simplifyMilliseconds
              << "," << facesPerSecond << ","
              << (outputStats.nonManifoldEdges - inputStats.nonManifoldEdges) << ","
              << (outputStats.boundaryEdges - inputStats.boundaryEdges) << "\n";
  }
}
