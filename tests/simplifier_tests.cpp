#include "line_quadrics_qem/core/MeshGenerators.h"
#include "line_quadrics_qem/core/MeshTopology.h"
#include "line_quadrics_qem/features/FeatureDetection.h"
#include "line_quadrics_qem/simplification/Metrics.h"
#include "line_quadrics_qem/simplification/QEMSimplifier.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
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
  lq::QEMSimplifier simplifier(options);
  result.mesh = simplifier.simplify(input, &result.report);
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

} // namespace

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

TEST(LineQuadricsQem, WeightModesRoundTripAndRejectUnknownValues) {
  EXPECT_EQ(lq::WeightMode::Uniform, lq::parseWeightMode("uniform"));
  EXPECT_EQ(lq::WeightMode::Dihedral, lq::parseWeightMode("dihedral"));
  EXPECT_EQ(lq::WeightMode::NormalTensor, lq::parseWeightMode("normal-tensor"));
  EXPECT_EQ(lq::WeightMode::Height, lq::parseWeightMode("height"));
  EXPECT_EQ(lq::WeightMode::XBand, lq::parseWeightMode("xband"));

  EXPECT_EQ("uniform", lq::toString(lq::WeightMode::Uniform));
  EXPECT_EQ("dihedral", lq::toString(lq::WeightMode::Dihedral));
  EXPECT_EQ("normal-tensor", lq::toString(lq::WeightMode::NormalTensor));
  EXPECT_EQ("height", lq::toString(lq::WeightMode::Height));
  EXPECT_EQ("xband", lq::toString(lq::WeightMode::XBand));

  EXPECT_THROW(lq::parseWeightMode("paper"), std::invalid_argument);
}

TEST(LineQuadricsQem, NormalTensorScoresSeparatePlaneFromRidge) {
  const lq::Mesh plane = lq::generatePlaneGrid(24, 2.0, false);
  const lq::Mesh ridge = lq::generateRidgeGrid(24, 2.0, 0.5);

  const std::vector<lq::NormalTensorVertex> planeTensor =
      lq::computeNormalTensorFeatures(plane);
  const std::vector<lq::NormalTensorVertex> ridgeTensor =
      lq::computeNormalTensorFeatures(ridge);

  double planeMax = 0.0;
  double ridgeMax = 0.0;
  for (const lq::NormalTensorVertex& vertex : planeTensor) {
    planeMax = std::max(planeMax, vertex.featureScore);
  }
  for (const lq::NormalTensorVertex& vertex : ridgeTensor) {
    ridgeMax = std::max(ridgeMax, vertex.featureScore);
  }

  EXPECT_LT(planeMax, 1e-8);
  EXPECT_GT(ridgeMax, 0.08);
}

TEST(LineQuadricsQem, NormalTensorAddsFeatureEdgesWhenDihedralThresholdIsStrict) {
  const lq::Mesh input = lq::generateRidgeGrid(32, 2.0, 0.6);
  lq::FeatureOptions options;
  options.featureAngleDeg = 179.0;
  options.normalTensorFeatureThreshold = 0.06;
  options.normalTensorMinEdgeAlignment = 0.2;

  const lq::FeatureAnalysis features = lq::detectFeatureCurves(input, options);

  EXPECT_EQ(0, features.dihedralFeatureEdges);
  EXPECT_GT(features.normalTensorFeatureEdges, 0);
  EXPECT_GT(features.maxNormalTensorFeatureScore, 0.06);
}

TEST(LineQuadricsQem, NormalTensorWeightModeAppliesSpatiallyVaryingWeights) {
  const lq::Mesh input = lq::generateRidgeGrid(32, 2.0, 0.6);
  lq::SimplifyOptions options = paperLineQuadricsOptions(0.70);
  options.weightMode = lq::WeightMode::NormalTensor;
  options.normalTensorSmoothingIterations = 1;

  const SimplifiedMesh result = simplifyWithReport(input, options);

  expectBudgetedSimplification(result, input, 0.70);
  EXPECT_GT(result.report.maxAppliedLineWeight, result.report.minAppliedLineWeight);
}

TEST(LineQuadricsQem, QEMSimplifierObjectStoresOptionsAndLatestReport) {
  const lq::Mesh input = lq::generateCylinderGrid(24, 6, 1.0, 2.0);

  lq::SimplifyOptions options = paperLineQuadricsOptions(0.50);
  lq::QEMSimplifier simplifier(options);

  lq::SimplifyReport copiedReport;
  const lq::Mesh output = simplifier.simplify(input, &copiedReport);

  EXPECT_FALSE(output.empty());
  EXPECT_EQ(options.targetRatio, simplifier.options().targetRatio);
  EXPECT_EQ(copiedReport.finalFaces, simplifier.report().finalFaces);
  EXPECT_EQ(copiedReport.collapsedEdges, simplifier.report().collapsedEdges);
  EXPECT_LT(simplifier.report().finalFaces, simplifier.report().initialFaces);
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
  lq::QEMSimplifier simplifier(options);
  const lq::Mesh output = simplifier.simplify(input, &report);
  const lq::MeshStats outputStats = lq::computeMeshStats(output);

  EXPECT_FALSE(output.empty());
  EXPECT_LE(report.finalFaces,
            static_cast<int>(std::llround(input.faces.size() * 0.25)) + 2);
  EXPECT_EQ(outputStats.boundaryEdges, 0);
  EXPECT_EQ(outputStats.nonManifoldEdges, 0);
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

TEST(LineQuadricsQem, MeshUtilitiesRejectMalformedInputWithoutThrowing) {
  const std::filesystem::path objPath =
      std::filesystem::temp_directory_path() / "line_quadrics_bad_face.obj";
  {
    std::ofstream out(objPath);
    out << "v 0 0 0\n";
    out << "v 1 0 0\n";
    out << "v 0 1 0\n";
    out << "f nope 2 3\n";
  }

  lq::Mesh mesh;
  std::string error;
  EXPECT_FALSE(lq::loadObj(objPath.string(), mesh, &error));
  EXPECT_FALSE(error.empty());
  std::filesystem::remove(objPath);

  lq::Mesh invalid;
  invalid.vertices = {lq::Vec3(0.0, 0.0, 0.0)};
  invalid.faces = {{{0, 1, 2}}};
  error.clear();
  const std::filesystem::path stlPath =
      std::filesystem::temp_directory_path() / "line_quadrics_invalid.stl";
  EXPECT_FALSE(lq::saveAsciiStl(stlPath.string(), invalid, "invalid", &error));
  EXPECT_FALSE(error.empty());
}

TEST(LineQuadricsQem, SimplifierRejectsInvalidOptionsAndMeshes) {
  const lq::Mesh input = lq::generatePlaneGrid(4, 1.0, false);

  lq::SimplifyOptions options;
  options.targetRatio = 0.0;
  EXPECT_THROW(lq::simplifyMesh(input, options), std::invalid_argument);

  lq::Mesh invalid;
  invalid.vertices = {
      lq::Vec3(0.0, 0.0, 0.0),
      lq::Vec3(1.0, 0.0, 0.0),
      lq::Vec3(0.0, 1.0, 0.0),
  };
  invalid.faces = {{{0, 1, 5}}};
  EXPECT_THROW(lq::simplifyMesh(invalid, lq::SimplifyOptions{}), std::invalid_argument);
}

TEST(LineQuadricsQem, FeatureDetectionSplitsBranchedFeatureGraphIntoChains) {
  lq::Mesh mesh;
  mesh.vertices = {
      lq::Vec3(0.0, 0.0, 0.0),  lq::Vec3(1.0, 0.0, 0.0),  lq::Vec3(0.5, 1.0, 0.0),
      lq::Vec3(-1.0, 0.0, 0.0), lq::Vec3(-0.5, 1.0, 0.0), lq::Vec3(0.0, -1.0, 0.0),
      lq::Vec3(1.0, -1.0, 0.0),
  };
  mesh.faces = {
      {{0, 1, 2}},
      {{0, 3, 4}},
      {{0, 5, 6}},
  };

  lq::FeatureOptions options;
  options.useNormalTensorFeatures = false;
  const lq::FeatureAnalysis features = lq::detectFeatureCurves(mesh, options);

  EXPECT_GT(features.loops.size(), 1u);
  ASSERT_LT(0u, features.vertices.size());
  EXPECT_TRUE(features.vertices[0].junction);
}

TEST(LineQuadricsQem, MeshDistanceIsZeroForIdenticalMeshAndFiniteAfterSimplify) {
  const lq::Mesh input =
      loadExternalStl("thingi10k/thingi10k_108336_projekt_muse_z_system.stl");
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
