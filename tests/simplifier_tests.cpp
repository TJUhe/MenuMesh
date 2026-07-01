#include "line_quadrics_qem/FeatureDetection.h"
#include "line_quadrics_qem/MeshGenerators.h"
#include "line_quadrics_qem/Metrics.h"
#include "line_quadrics_qem/QEMSimplifier.h"

#include <algorithm>
#include <filesystem>
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

TEST(LineQuadricsQem, ComputesMeshStatsForGeneratedCube) {
  const lq::Mesh input = lq::generateCubeGrid(4, 1.0);
  const lq::MeshStats stats = lq::computeMeshStats(input);

  EXPECT_EQ(stats.vertices, static_cast<int>(input.vertices.size()));
  EXPECT_EQ(stats.faces, static_cast<int>(input.faces.size()));
  EXPECT_GT(stats.edges, 0);
  EXPECT_GT(stats.area, 0.0);
  EXPECT_GT(stats.meanTriangleQuality, 0.0);
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

TEST(LineQuadricsQem, AsciiStlRoundTripKeepsGeometryUsable) {
  const lq::Mesh input = lq::generateCubeGrid(2, 1.0);
  ASSERT_FALSE(input.empty());

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "line_quadrics_qem_roundtrip.stl";
  std::string error;
  ASSERT_TRUE(lq::saveAsciiStl(path.string(), input, "roundtrip", &error)) << error;

  lq::Mesh loaded;
  ASSERT_TRUE(lq::loadStl(path.string(), loaded, &error)) << error;
  std::filesystem::remove(path);

  EXPECT_FALSE(loaded.empty());
  EXPECT_EQ(loaded.faces.size(), input.faces.size());

  const lq::MeshStats inputStats = lq::computeMeshStats(input);
  const lq::MeshStats loadedStats = lq::computeMeshStats(loaded);
  EXPECT_NEAR(loadedStats.area, inputStats.area, 1e-9);
  EXPECT_EQ(loadedStats.nonManifoldEdges, 0);
}
