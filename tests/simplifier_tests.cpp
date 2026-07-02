#include "line_quadrics_qem/core/MeshGenerators.h"
#include "line_quadrics_qem/core/MeshTopology.h"
#include "line_quadrics_qem/features/FeatureDetection.h"
#include "line_quadrics_qem/simplification/Metrics.h"
#include "line_quadrics_qem/simplification/QEMSimplifier.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int countCircularLoops(const lq::FeatureAnalysis& analysis) {
  return static_cast<int>(
      std::count_if(analysis.loops.begin(), analysis.loops.end(),
                    [](const lq::FeatureLoop& loop) { return loop.circular; }));
}

lq::FeatureOptions circularFeatureOptions() {
  lq::FeatureOptions options;
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = 0.04;
  options.minFeatureLoopVertices = 8;
  return options;
}

lq::SimplifyOptions standardQemOptions(double ratio) {
  lq::SimplifyOptions options;
  options.targetRatio = ratio;
  options.useLineQuadrics = false;
  options.lineWeight = 0.0;
  return options;
}

lq::SimplifyOptions paperLineQuadricsOptions(double ratio) {
  lq::SimplifyOptions options;
  options.targetRatio = ratio;
  options.useLineQuadrics = true;
  options.lineWeight = 1e-3;
  options.weightMode = lq::WeightMode::Dihedral;
  options.featureBoost = 0.08;
  options.featureAngleDeg = 25.0;
  return options;
}

lq::SimplifyOptions featureCurveOptions(double ratio) {
  lq::SimplifyOptions options = paperLineQuadricsOptions(ratio);
  options.preserveFeatureCurves = true;
  options.featureCurveWeight = 0.12;
  options.circleFitRelativeThreshold = 0.04;
  options.minFeatureLoopVertices = 8;
  return options;
}

lq::SimplifyOptions protectedIndustrialFeatureOptions(double ratio) {
  lq::SimplifyOptions options = paperLineQuadricsOptions(ratio);
  options.preserveFeatureCurves = true;
  options.protectAllFeatureEdges = true;
  options.featureCurveWeight = 0.08;
  options.circleFitRelativeThreshold = 0.05;
  options.minFeatureLoopVertices = 8;
  return options;
}

struct SimplifiedMesh {
  lq::Mesh mesh;
  lq::SimplifyReport report;
};

SimplifiedMesh simplifyWithReport(const lq::Mesh& input,
                                  const lq::SimplifyOptions& options) {
  SimplifiedMesh result;
  result.mesh = lq::simplifyMesh(input, options, &result.report);
  return result;
}

void expectBudgetedSimplification(const SimplifiedMesh& result, const lq::Mesh& input,
                                  double ratio) {
  EXPECT_FALSE(result.mesh.empty());
  EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
  EXPECT_EQ(result.report.initialFaces, static_cast<int>(input.faces.size()));
  EXPECT_EQ(result.report.finalFaces, static_cast<int>(result.mesh.faces.size()));
  EXPECT_LE(result.report.finalFaces,
            static_cast<int>(std::llround(input.faces.size() * ratio)) + 2);
  EXPECT_GT(result.report.collapsedEdges, 0);
}

std::filesystem::path externalDataDir() {
#ifdef LQ_TEST_EXTERNAL_DATA_DIR
  return std::filesystem::path(LQ_TEST_EXTERNAL_DATA_DIR);
#else
  return std::filesystem::path(__FILE__).parent_path() / "data" / "external";
#endif
}

lq::Mesh loadExternalStl(const std::string& fileName) {
  lq::Mesh mesh;
  std::string error;
  const std::filesystem::path path = externalDataDir() / fileName;
  if (!lq::loadStl(path.string(), mesh, &error)) {
    ADD_FAILURE() << "Failed to load " << path.string() << ": " << error;
  }
  return mesh;
}

lq::Mesh loadExternalMesh(const std::string& fileName) {
  lq::Mesh mesh;
  std::string error;
  const std::filesystem::path path = externalDataDir() / fileName;
  if (!lq::loadMesh(path.string(), mesh, &error)) {
    ADD_FAILURE() << "Failed to load " << path.string() << ": " << error;
  }
  return mesh;
}

std::vector<std::filesystem::path> discoverExternalStlFixtures() {
  std::vector<std::filesystem::path> paths;
  const std::filesystem::path root = externalDataDir();
  if (!std::filesystem::exists(root)) {
    return paths;
  }

  for (const std::filesystem::directory_entry& entry :
       std::filesystem::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".stl") {
      continue;
    }
    paths.push_back(std::filesystem::relative(entry.path(), root));
  }

  std::sort(paths.begin(), paths.end());
  return paths;
}

bool isBinaryStlFile(const std::filesystem::path& path, std::uint32_t& triangleCount) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  if (size < 84) {
    return false;
  }
  in.seekg(80, std::ios::beg);
  std::uint32_t count = 0;
  in.read(reinterpret_cast<char*>(&count), sizeof(count));
  if (!in) {
    return false;
  }
  triangleCount = count;
  return size == static_cast<std::streamoff>(84 + 50ull * triangleCount);
}

} // namespace

TEST(LineQuadricsQem, SimplifiesGeneratedGridToRequestedBudget) {
  const lq::Mesh input = lq::generatePlaneGrid(12, 1.0, false);
  ASSERT_FALSE(input.empty());

  lq::SimplifyOptions options;
  options.targetRatio = 0.5;
  options.lineWeight = 1e-3;

  lq::SimplifyReport report;
  const lq::Mesh output = lq::simplifyMesh(input, options, &report);

  EXPECT_FALSE(output.empty());
  EXPECT_LT(output.faces.size(), input.faces.size());
  EXPECT_LE(output.faces.size(),
            static_cast<std::size_t>(input.faces.size() * 0.5 + 2));
  EXPECT_EQ(report.initialFaces, static_cast<int>(input.faces.size()));
  EXPECT_EQ(report.finalFaces, static_cast<int>(output.faces.size()));
  EXPECT_GT(report.collapsedEdges, 0);
}

TEST(LineQuadricsQem, BuiltInGeneratorsCoverDemoAndIndustrialModels) {
  const std::vector<std::string> names = {
      "plane",       "clustered-plane", "hole-plane",    "ridge",
      "noisy-plane", "sine-terrain",    "terrace",       "bump",
      "cylinder",    "torus",           "cube",          "thin-fin",
      "flange",      "stepped-shaft",   "pipe-coupling", "pulley",
  };

  for (const std::string& name : names) {
    lq::Mesh mesh;
    std::string error;
    EXPECT_TRUE(lq::generateMeshByName(name, 24, mesh, &error))
        << name << ": " << error;
    EXPECT_FALSE(mesh.empty()) << name;

    const lq::MeshStats stats = lq::computeMeshStats(mesh);
    EXPECT_EQ(stats.vertices, static_cast<int>(mesh.vertices.size())) << name;
    EXPECT_EQ(stats.faces, static_cast<int>(mesh.faces.size())) << name;
    EXPECT_GT(stats.edges, 0) << name;
    EXPECT_GT(stats.area, 0.0) << name;
    EXPECT_GE(stats.meanTriangleQuality, 0.0) << name;
  }

  lq::Mesh mesh;
  std::string error;
  EXPECT_FALSE(lq::generateMeshByName("not-a-generator", 16, mesh, &error));
  EXPECT_FALSE(error.empty());
}

TEST(LineQuadricsQem, ExternalStlFixtureSetIsDownloadedBinaryData) {
  const std::vector<std::filesystem::path> paths = discoverExternalStlFixtures();
  ASSERT_GE(paths.size(), 100u);

  for (const std::filesystem::path& relativePath : paths) {
    SCOPED_TRACE(relativePath.generic_string());
    std::uint32_t triangleCount = 0;
    EXPECT_TRUE(isBinaryStlFile(externalDataDir() / relativePath, triangleCount));
    EXPECT_GT(triangleCount, 0u);
  }
}

TEST(LineQuadricsQem, WeightModesRoundTripAndRejectUnknownValues) {
  EXPECT_EQ(lq::WeightMode::Uniform, lq::parseWeightMode("uniform"));
  EXPECT_EQ(lq::WeightMode::Dihedral, lq::parseWeightMode("dihedral"));
  EXPECT_EQ(lq::WeightMode::Height, lq::parseWeightMode("height"));
  EXPECT_EQ(lq::WeightMode::XBand, lq::parseWeightMode("xband"));

  EXPECT_EQ("uniform", lq::toString(lq::WeightMode::Uniform));
  EXPECT_EQ("dihedral", lq::toString(lq::WeightMode::Dihedral));
  EXPECT_EQ("height", lq::toString(lq::WeightMode::Height));
  EXPECT_EQ("xband", lq::toString(lq::WeightMode::XBand));

  EXPECT_THROW(lq::parseWeightMode("paper"), std::invalid_argument);
}

TEST(LineQuadricsQem, TargetFaceCountOverridesRatioBudget) {
  const lq::Mesh input = lq::generatePlaneGrid(14, 2.0, false);
  ASSERT_FALSE(input.empty());

  lq::SimplifyOptions options;
  options.targetRatio = 0.95;
  options.targetFaces = 48;
  options.lineWeight = 1e-3;

  lq::SimplifyReport report;
  const lq::Mesh output = lq::simplifyMesh(input, options, &report);

  EXPECT_FALSE(output.empty());
  EXPECT_LT(output.faces.size(), input.faces.size());
  EXPECT_LE(report.finalFaces, options.targetFaces + 2);
  EXPECT_EQ(report.finalFaces, static_cast<int>(output.faces.size()));
}

TEST(LineQuadricsQem, ReportsFeatureLoopsOnCylinderCreases) {
  const lq::Mesh input = lq::generateCylinderGrid(32, 4, 1.0, 2.0);
  lq::FeatureOptions options;
  options.featureAngleDeg = 30.0;

  const lq::FeatureAnalysis features = lq::detectFeatureCurves(input, options);

  EXPECT_GT(features.featureEdges, 0);
  EXPECT_GT(features.dihedralFeatureEdges, 0);
  EXPECT_FALSE(features.loops.empty());
}

TEST(LineQuadricsQem, MeasuresCircularFeatureLoopAgainstDetectedCircle) {
  const lq::Mesh input = lq::generateCylinderGrid(32, 4, 1.0, 2.0);
  const lq::FeatureAnalysis features =
      lq::detectFeatureCurves(input, circularFeatureOptions());
  ASSERT_GT(countCircularLoops(features), 0);

  const auto loopIt =
      std::find_if(features.loops.begin(), features.loops.end(),
                   [](const lq::FeatureLoop& loop) { return loop.circular; });
  ASSERT_NE(loopIt, features.loops.end());

  const lq::DirectionalCurveError error = lq::measureLoopAgainstCircle(
      input, *loopIt, loopIt->center, loopIt->normal, loopIt->radius);

  EXPECT_EQ(error.samples, static_cast<int>(loopIt->vertices.size()));
  EXPECT_NEAR(error.radialRms, loopIt->rmsRadialError, 1e-10);
  EXPECT_NEAR(error.planeRms, loopIt->rmsPlaneError, 1e-10);
  EXPECT_LT(error.radialMax, 1e-10);
  EXPECT_LT(error.planeMax, 1e-10);
}

TEST(LineQuadricsQem, StandardAndPaperLineQuadricsHaveDistinctDiagnostics) {
  const lq::Mesh input = lq::generateFlangedBossGrid(48);
  ASSERT_FALSE(input.empty());

  const SimplifiedMesh standard = simplifyWithReport(input, standardQemOptions(0.20));
  const SimplifiedMesh line = simplifyWithReport(input, paperLineQuadricsOptions(0.20));

  ASSERT_FALSE(standard.mesh.empty());
  ASSERT_FALSE(line.mesh.empty());

  EXPECT_EQ(0.0, standard.report.minAppliedLineWeight);
  EXPECT_EQ(0.0, standard.report.maxAppliedLineWeight);
  EXPECT_GE(line.report.minAppliedLineWeight, 1e-3);
  EXPECT_GT(line.report.maxAppliedLineWeight, line.report.minAppliedLineWeight);

  EXPECT_EQ(0, standard.report.featureLoops);
  EXPECT_EQ(0, line.report.featureLoops);
  EXPECT_LT(standard.report.finalFaces, standard.report.initialFaces);
  EXPECT_LT(line.report.finalFaces, line.report.initialFaces);
  EXPECT_LE(std::abs(standard.report.finalFaces - line.report.finalFaces), 2);
}

TEST(LineQuadricsQem, FeatureCurveExtensionReportsProtectedCircularFeatures) {
  const lq::Mesh input = lq::generateFlangedBossGrid(48);
  ASSERT_FALSE(input.empty());

  const SimplifiedMesh curve = simplifyWithReport(input, featureCurveOptions(0.20));

  EXPECT_FALSE(curve.mesh.empty());
  EXPECT_LT(curve.report.finalFaces, curve.report.initialFaces);
  EXPECT_GT(curve.report.featureLoops, 0);
  EXPECT_GT(curve.report.circularFeatureLoops, 0);
  EXPECT_GT(curve.report.featureVertices, 0);
  EXPECT_GT(curve.report.featureRejectedCollapses, 0);
  EXPECT_GT(curve.report.projectedFeaturePlacements, 0);
  EXPECT_GE(curve.report.minAppliedLineWeight, 1e-3);
  EXPECT_GT(curve.report.maxAppliedLineWeight, curve.report.minAppliedLineWeight);
}

TEST(LineQuadricsQem, CurveExtensionPreservesAtLeastPaperStyleCircularLoopCount) {
  const lq::Mesh input = lq::generateFlangedBossGrid(48);
  ASSERT_FALSE(input.empty());

  const SimplifiedMesh line = simplifyWithReport(input, paperLineQuadricsOptions(0.20));
  const SimplifiedMesh curve = simplifyWithReport(input, featureCurveOptions(0.20));

  const lq::FeatureOptions featureOptions = circularFeatureOptions();
  const lq::FeatureAnalysis lineFeatures =
      lq::detectFeatureCurves(line.mesh, featureOptions);
  const lq::FeatureAnalysis curveFeatures =
      lq::detectFeatureCurves(curve.mesh, featureOptions);

  EXPECT_GE(countCircularLoops(curveFeatures), countCircularLoops(lineFeatures));
  EXPECT_GT(curve.report.featureRejectedCollapses,
            line.report.featureRejectedCollapses);
  EXPECT_GT(curve.report.projectedFeaturePlacements,
            line.report.projectedFeaturePlacements);
}

TEST(LineQuadricsQem, ExternalNasaIndustrialMeshesExposeRichFeatureTopology) {
  struct Case {
    std::string fileName;
    int minFaces = 0;
    int minFeatureEdges = 0;
    int minCircularLoops = 0;
  };

  const std::array<Case, 3> cases = {{
      {"nasa_antenna_azimuth_track.stl", 3000, 1000, 4},
      {"nasa_cubesat_middle.stl", 28000, 8000, 20},
      {"nasa_mars2020_wheel.stl", 45000, 10000, 4},
  }};

  for (const Case& testCase : cases) {
    SCOPED_TRACE(testCase.fileName);
    const lq::Mesh mesh = loadExternalStl(testCase.fileName);
    ASSERT_FALSE(mesh.empty());

    const lq::MeshStats stats = lq::computeMeshStats(mesh);
    EXPECT_GE(stats.faces, testCase.minFaces);
    EXPECT_GT(stats.edges, 0);
    EXPECT_GT(stats.area, 0.0);
    EXPECT_EQ(stats.nonManifoldEdges, 0);

    lq::FeatureOptions featureOptions = circularFeatureOptions();
    featureOptions.featureAngleDeg = 30.0;
    featureOptions.circleFitRelativeThreshold = 0.05;
    const lq::FeatureAnalysis features = lq::detectFeatureCurves(mesh, featureOptions);
    EXPECT_GE(features.featureEdges, testCase.minFeatureEdges);
    EXPECT_GE(countCircularLoops(features), testCase.minCircularLoops);
    EXPECT_GT(features.dihedralFeatureEdges, 0);
  }
}

TEST(LineQuadricsQem, ExternalDownloadedMeshesCompareIndustrialSimplificationModes) {
  struct Case {
    std::string fileName;
    bool expectsCircularProjection = false;
  };

  const std::array<Case, 3> cases = {{
      {"nasa_antenna_azimuth_track.stl", true},
      {"thingi10k/thingi10k_108336_projekt_muse_z_system.stl", false},
      {"thingi10k/thingi10k_318045_moko_mini_pulley.stl", true},
  }};

  constexpr double ratio = 0.45;
  for (const Case& testCase : cases) {
    SCOPED_TRACE(testCase.fileName);
    const lq::Mesh input = loadExternalStl(testCase.fileName);
    ASSERT_FALSE(input.empty());

    const SimplifiedMesh standard =
        simplifyWithReport(input, standardQemOptions(ratio));
    const SimplifiedMesh line =
        simplifyWithReport(input, paperLineQuadricsOptions(ratio));
    const SimplifiedMesh protectedResult =
        simplifyWithReport(input, protectedIndustrialFeatureOptions(ratio));

    expectBudgetedSimplification(standard, input, ratio);
    expectBudgetedSimplification(line, input, ratio);
    EXPECT_FALSE(protectedResult.mesh.empty());
    EXPECT_LT(protectedResult.report.finalFaces, protectedResult.report.initialFaces);
    EXPECT_EQ(protectedResult.report.finalFaces,
              static_cast<int>(protectedResult.mesh.faces.size()));

    EXPECT_EQ(0.0, standard.report.maxAppliedLineWeight);
    EXPECT_GE(line.report.maxAppliedLineWeight, line.report.minAppliedLineWeight);
    EXPECT_GT(protectedResult.report.featureLoops, 0);
    EXPECT_GT(protectedResult.report.featureVertices, 0);
    EXPECT_GT(protectedResult.report.featureRejectedCollapses, 0);
    EXPECT_GE(protectedResult.report.maxAppliedLineWeight,
              protectedResult.report.minAppliedLineWeight);

    if (testCase.expectsCircularProjection) {
      EXPECT_GT(protectedResult.report.circularFeatureLoops, 0);
      EXPECT_GT(protectedResult.report.projectedFeaturePlacements, 0);
    }
  }
}

TEST(LineQuadricsQem, Public2014CastingModelKeepsClosedTopologyAfterLineSimplify) {
  const lq::Mesh input = loadExternalMesh("casting_aimshape_2014.stl");
  ASSERT_FALSE(input.empty());

  const lq::MeshStats inputStats = lq::computeMeshStats(input);
  ASSERT_EQ(inputStats.boundaryEdges, 0);
  ASSERT_EQ(inputStats.nonManifoldEdges, 0);

  lq::SimplifyOptions options = paperLineQuadricsOptions(0.25);
  lq::SimplifyReport report;
  const lq::Mesh output = lq::simplifyMesh(input, options, &report);
  const lq::MeshStats outputStats = lq::computeMeshStats(output);

  EXPECT_FALSE(output.empty());
  EXPECT_LE(report.finalFaces,
            static_cast<int>(std::llround(input.faces.size() * 0.25)) + 2);
  EXPECT_EQ(outputStats.boundaryEdges, 0);
  EXPECT_EQ(outputStats.nonManifoldEdges, 0);
}

TEST(LineQuadricsQem, ComplexLathePartsComparePaperLineAndCurveExtension) {
  struct Case {
    std::string name;
    std::function<lq::Mesh()> generate;
    double ratio = 0.22;
  };

  const std::array<Case, 3> cases = {{
      {"stepped shaft", [] { return lq::generateSteppedShaftGrid(64); }, 0.22},
      {"pipe coupling", [] { return lq::generatePipeCouplingGrid(72); }, 0.22},
      {"pulley groove", [] { return lq::generatePulleyGrid(72); }, 0.22},
  }};

  for (const Case& testCase : cases) {
    SCOPED_TRACE(testCase.name);
    const lq::Mesh input = testCase.generate();
    ASSERT_FALSE(input.empty());

    const lq::FeatureAnalysis originalFeatures =
        lq::detectFeatureCurves(input, circularFeatureOptions());
    EXPECT_GT(originalFeatures.featureEdges, 0);
    EXPECT_GT(countCircularLoops(originalFeatures), 0);

    const SimplifiedMesh standard =
        simplifyWithReport(input, standardQemOptions(testCase.ratio));
    const SimplifiedMesh line =
        simplifyWithReport(input, paperLineQuadricsOptions(testCase.ratio));
    const SimplifiedMesh curve =
        simplifyWithReport(input, featureCurveOptions(testCase.ratio));

    expectBudgetedSimplification(standard, input, testCase.ratio);
    expectBudgetedSimplification(line, input, testCase.ratio);
    expectBudgetedSimplification(curve, input, testCase.ratio);

    EXPECT_EQ(0, standard.report.featureLoops);
    EXPECT_EQ(0, line.report.featureLoops);
    EXPECT_GT(curve.report.featureLoops, 0);
    EXPECT_GT(curve.report.circularFeatureLoops, 0);
    EXPECT_GT(curve.report.projectedFeaturePlacements, 0);

    EXPECT_EQ(0.0, standard.report.maxAppliedLineWeight);
    EXPECT_GT(line.report.maxAppliedLineWeight, line.report.minAppliedLineWeight);
    EXPECT_GT(curve.report.maxAppliedLineWeight, curve.report.minAppliedLineWeight);

    const lq::FeatureAnalysis curveFeatures =
        lq::detectFeatureCurves(curve.mesh, circularFeatureOptions());
    EXPECT_GT(curveFeatures.featureEdges, 0);
    EXPECT_GT(countCircularLoops(curveFeatures), 0);
  }
}

TEST(LineQuadricsQem, HolePlateExercisesBoundaryFeatureProtection) {
  const lq::Mesh input = lq::generateHolePlaneGrid(36, 2.0, 0.34);
  ASSERT_FALSE(input.empty());

  const lq::FeatureAnalysis originalFeatures =
      lq::detectFeatureCurves(input, circularFeatureOptions());
  EXPECT_GT(originalFeatures.boundaryFeatureEdges, 0);

  const SimplifiedMesh standard = simplifyWithReport(input, standardQemOptions(0.35));
  const SimplifiedMesh line = simplifyWithReport(input, paperLineQuadricsOptions(0.35));

  lq::SimplifyOptions protectedOptions = paperLineQuadricsOptions(0.35);
  protectedOptions.preserveFeatureCurves = true;
  protectedOptions.protectAllFeatureEdges = true;
  protectedOptions.featureCurveWeight = 0.08;
  protectedOptions.minFeatureLoopVertices = 8;
  const SimplifiedMesh protectedResult = simplifyWithReport(input, protectedOptions);

  expectBudgetedSimplification(standard, input, 0.35);
  expectBudgetedSimplification(line, input, 0.35);
  expectBudgetedSimplification(protectedResult, input, 0.35);

  EXPECT_EQ(0, standard.report.featureLoops);
  EXPECT_EQ(0, line.report.featureLoops);
  EXPECT_GT(protectedResult.report.featureLoops, 0);
  EXPECT_GT(protectedResult.report.featureVertices, 0);
  EXPECT_GT(protectedResult.report.featureRejectedCollapses, 0);

  const lq::FeatureAnalysis standardFeatures =
      lq::detectFeatureCurves(standard.mesh, circularFeatureOptions());
  const lq::FeatureAnalysis protectedFeatures =
      lq::detectFeatureCurves(protectedResult.mesh, circularFeatureOptions());
  EXPECT_GE(protectedFeatures.boundaryFeatureEdges,
            standardFeatures.boundaryFeatureEdges);
}

TEST(LineQuadricsQem, ThinFinExercisesNonCircularHardFeatureProtection) {
  const lq::Mesh input = lq::generateThinFinGrid(32, 2.0);
  ASSERT_FALSE(input.empty());

  const lq::FeatureAnalysis features =
      lq::detectFeatureCurves(input, circularFeatureOptions());
  EXPECT_GT(features.featureEdges, 0);
  EXPECT_GT(features.boundaryFeatureEdges + features.dihedralFeatureEdges, 0);

  lq::SimplifyOptions protectedOptions = paperLineQuadricsOptions(0.30);
  protectedOptions.preserveFeatureCurves = true;
  protectedOptions.protectAllFeatureEdges = true;
  protectedOptions.featureCurveWeight = 0.08;
  protectedOptions.minFeatureLoopVertices = 8;

  const SimplifiedMesh standard = simplifyWithReport(input, standardQemOptions(0.30));
  const SimplifiedMesh protectedResult = simplifyWithReport(input, protectedOptions);

  expectBudgetedSimplification(standard, input, 0.30);
  expectBudgetedSimplification(protectedResult, input, 0.30);

  EXPECT_EQ(0, standard.report.featureRejectedCollapses);
  EXPECT_GT(protectedResult.report.featureLoops, 0);
  EXPECT_GT(protectedResult.report.featureVertices, 0);
  EXPECT_GT(protectedResult.report.featureRejectedCollapses, 0);
  EXPECT_GE(protectedResult.report.maxAppliedLineWeight, 1e-3);
}

TEST(LineQuadricsQem, SmoothAndCreasedComplexSurfacesTriggerDifferentWeightModes) {
  struct Case {
    std::string name;
    lq::Mesh mesh;
    lq::WeightMode mode;
  };

  const std::array<Case, 3> cases = {{
      {"smooth torus with uniform line quadrics",
       lq::generateTorusGrid(40, 16, 0.7, 0.23), lq::WeightMode::Uniform},
      {"sine terrain with height weighting", lq::generateSineTerrainGrid(24, 2.0),
       lq::WeightMode::Height},
      {"terraced terrain with dihedral weighting", lq::generateTerraceGrid(24, 2.0),
       lq::WeightMode::Dihedral},
  }};

  for (const Case& testCase : cases) {
    SCOPED_TRACE(testCase.name);
    ASSERT_FALSE(testCase.mesh.empty());

    lq::SimplifyOptions options = paperLineQuadricsOptions(0.35);
    options.weightMode = testCase.mode;
    options.featureBoost = testCase.mode == lq::WeightMode::Uniform ? 0.0 : 0.08;

    const SimplifiedMesh result = simplifyWithReport(testCase.mesh, options);
    expectBudgetedSimplification(result, testCase.mesh, 0.35);

    EXPECT_GE(result.report.minAppliedLineWeight, 1e-3);
    if (testCase.mode == lq::WeightMode::Uniform) {
      EXPECT_NEAR(result.report.minAppliedLineWeight,
                  result.report.maxAppliedLineWeight, 1e-12);
    } else {
      EXPECT_GT(result.report.maxAppliedLineWeight, result.report.minAppliedLineWeight);
    }

    const lq::DistanceStats distance =
        lq::compareMeshesBySampledDistance(testCase.mesh, result.mesh, 48);
    EXPECT_GE(distance.maxOriginalToSimplified, distance.meanOriginalToSimplified);
    EXPECT_GE(distance.maxSimplifiedToOriginal, distance.meanSimplifiedToOriginal);
  }
}

TEST(LineQuadricsQem, ComputesMeshStatsForGeneratedCube) {
  const lq::Mesh input = lq::generateCubeGrid(4, 1.0);
  const lq::MeshStats stats = lq::computeMeshStats(input);

  EXPECT_EQ(stats.vertices, static_cast<int>(input.vertices.size()));
  EXPECT_EQ(stats.faces, static_cast<int>(input.faces.size()));
  EXPECT_GT(stats.edges, 0);
  EXPECT_GT(stats.area, 0.0);
  EXPECT_GT(stats.meanTriangleQuality, 0.0);
}

TEST(LineQuadricsQem, MeshTopologyCachesBoundaryAndNonManifoldEdges) {
  lq::Mesh mesh;
  mesh.vertices = {
      lq::Vec3(0.0, 0.0, 0.0), lq::Vec3(1.0, 0.0, 0.0),  lq::Vec3(0.0, 1.0, 0.0),
      lq::Vec3(0.0, 0.0, 1.0), lq::Vec3(0.0, 0.0, -1.0),
  };
  mesh.faces = {
      {{0, 1, 2}},
      {{1, 0, 3}},
      {{0, 1, 4}},
  };

  const lq::Result<lq::MeshTopology> topologyResult = lq::MeshTopology::build(mesh);
  ASSERT_TRUE(topologyResult.ok()) << topologyResult.status().message();
  const lq::MeshTopology& topology = topologyResult.value();

  EXPECT_EQ(topology.vertexCount(), 5);
  EXPECT_EQ(topology.faceCount(), 3);
  EXPECT_EQ(topology.edgeCount(), 7);
  EXPECT_EQ(topology.boundaryEdgeCount(), 6);
  EXPECT_EQ(topology.nonManifoldEdgeCount(), 1);

  const lq::MeshStats stats = lq::computeMeshStats(mesh);
  EXPECT_EQ(stats.edges, topology.edgeCount());
  EXPECT_EQ(stats.boundaryEdges, topology.boundaryEdgeCount());
  EXPECT_EQ(stats.nonManifoldEdges, topology.nonManifoldEdgeCount());
}

TEST(LineQuadricsQem, MeshTopologyRejectsInvalidFaces) {
  lq::Mesh mesh;
  mesh.vertices = {
      lq::Vec3(0.0, 0.0, 0.0),
      lq::Vec3(1.0, 0.0, 0.0),
      lq::Vec3(0.0, 1.0, 0.0),
  };
  mesh.faces = {{{0, 1, 5}}};

  const lq::Result<lq::MeshTopology> topologyResult = lq::MeshTopology::build(mesh);
  EXPECT_FALSE(topologyResult.ok());
  EXPECT_EQ(topologyResult.status().code(), lq::StatusCode::InvalidArgument);
}

TEST(LineQuadricsQem, MeshDistanceIsZeroForIdenticalMeshAndFiniteAfterSimplify) {
  const lq::Mesh input = lq::generateSineTerrainGrid(12, 2.0);
  ASSERT_FALSE(input.empty());

  const lq::DistanceStats identical =
      lq::compareMeshesBySampledDistance(input, input, 32);
  EXPECT_NEAR(identical.meanOriginalToSimplified, 0.0, 1e-12);
  EXPECT_NEAR(identical.maxOriginalToSimplified, 0.0, 1e-12);
  EXPECT_NEAR(identical.meanSimplifiedToOriginal, 0.0, 1e-12);
  EXPECT_NEAR(identical.maxSimplifiedToOriginal, 0.0, 1e-12);

  const SimplifiedMesh simplified =
      simplifyWithReport(input, paperLineQuadricsOptions(0.35));
  const lq::DistanceStats distance =
      lq::compareMeshesBySampledDistance(input, simplified.mesh, 32);
  EXPECT_GE(distance.meanOriginalToSimplified, 0.0);
  EXPECT_GE(distance.maxOriginalToSimplified, distance.meanOriginalToSimplified);
  EXPECT_GE(distance.meanSimplifiedToOriginal, 0.0);
  EXPECT_GE(distance.maxSimplifiedToOriginal, distance.meanSimplifiedToOriginal);
}

TEST(LineQuadricsQem, ExternalBinaryStlLoadKeepsGeometryUsable) {
  const lq::Mesh loaded =
      loadExternalStl("thingi10k/thingi10k_108336_projekt_muse_z_system.stl");
  ASSERT_FALSE(loaded.empty());

  const lq::MeshStats stats = lq::computeMeshStats(loaded);
  EXPECT_EQ(stats.faces, static_cast<int>(loaded.faces.size()));
  EXPECT_GT(stats.vertices, 0);
  EXPECT_GT(stats.edges, 0);
  EXPECT_GT(stats.area, 0.0);
  EXPECT_EQ(stats.nonManifoldEdges, 0);
}
