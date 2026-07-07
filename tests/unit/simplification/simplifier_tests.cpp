#include "TestSupport.h"
#include "manumesh/algorithms/feature_detection/FeatureDetector.h"
#include "manumesh/algorithms/simplification/Metrics.h"
#include "manumesh/algorithms/simplification/PlainSimplifier.h"
#include "manumesh/algorithms/simplification/QEMSimplifier.h"
#include "manumesh/core/MeshGenerators.h"
#include "manumesh/core/MeshTopology.h"
#include "manumesh/core/PlainMesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using manumesh::test::countCircularLoops;
using manumesh::test::loadExternalMesh;
using manumesh::test::loadExternalStl;
using manumesh::test::SimplifiedMesh;
using manumesh::test::simplifyWithReport;

namespace simplification = manumesh::simplification;

int countBoundaryVertices(const manumesh::Mesh& mesh) {
  const manumesh::Result<manumesh::MeshTopology> topologyResult =
      manumesh::MeshTopology::build(mesh);
  if (!topologyResult.ok()) {
    return -1;
  }
  std::vector<char> boundary(mesh.vertices.size(), 0);
  for (const manumesh::TopologyEdge& edge : topologyResult.value().edges()) {
    if (!edge.boundary()) {
      continue;
    }
    boundary[edge.vertices[0]] = 1;
    boundary[edge.vertices[1]] = 1;
  }
  return static_cast<int>(std::count(boundary.begin(), boundary.end(), 1));
}

int countBoundaryComponents(const manumesh::Mesh& mesh) {
  const manumesh::Result<manumesh::MeshTopology> topologyResult =
      manumesh::MeshTopology::build(mesh);
  if (!topologyResult.ok()) {
    return -1;
  }

  std::vector<std::vector<int>> adjacency(mesh.vertices.size());
  for (const manumesh::TopologyEdge& edge : topologyResult.value().edges()) {
    if (!edge.boundary()) {
      continue;
    }
    adjacency[edge.vertices[0]].push_back(edge.vertices[1]);
    adjacency[edge.vertices[1]].push_back(edge.vertices[0]);
  }

  int components = 0;
  std::vector<char> visited(mesh.vertices.size(), 0);
  for (int seed = 0; seed < static_cast<int>(adjacency.size()); ++seed) {
    if (adjacency[seed].empty() || visited[seed]) {
      continue;
    }
    ++components;
    std::vector<int> stack{seed};
    visited[seed] = 1;
    while (!stack.empty()) {
      const int v = stack.back();
      stack.pop_back();
      for (int neighbor : adjacency[v]) {
        if (!visited[neighbor]) {
          visited[neighbor] = 1;
          stack.push_back(neighbor);
        }
      }
    }
  }
  return components;
}

manumesh::feature::FeatureOptions circularFeatureOptions() {
  return manumesh::test::circularFeatureOptions(0.04);
}

manumesh::simplification::SimplifyOptions standardQemOptions(double ratio) {
  return manumesh::test::standardOptions(ratio);
}

manumesh::simplification::SimplifyOptions paperLineQuadricsOptions(double ratio) {
  return manumesh::test::lineOptions(ratio);
}

manumesh::simplification::SimplifyOptions
protectedIndustrialFeatureOptions(double ratio) {
  return manumesh::test::protectedOptions(ratio);
}

void expectBudgetedSimplification(const SimplifiedMesh& result,
                                  const manumesh::Mesh& input, double ratio) {
  manumesh::test::expectBudget(result, input, ratio);
}

manumesh::Mesh makeLocalIntersectionGuardMesh() {
  manumesh::Mesh mesh;
  mesh.vertices = {
      manumesh::Vec3(0.0, 0.0, 0.0),    manumesh::Vec3(0.1, 0.0, 0.0),
      manumesh::Vec3(0.1, 1.0, 0.0),    manumesh::Vec3(0.2, -1.0, 0.0),
      manumesh::Vec3(0.12, -0.3, -1.0), manumesh::Vec3(0.12, 0.3, 1.0),
      manumesh::Vec3(0.12, 0.9, -1.0),
  };
  mesh.faces = {
      manumesh::Face{{0, 1, 2}},
      manumesh::Face{{0, 3, 1}},
      manumesh::Face{{1, 2, 3}},
      manumesh::Face{{4, 5, 6}},
  };
  return mesh;
}

manumesh::Mesh makeSpatialIntersectionGuardMeshWithFarFaces() {
  manumesh::Mesh mesh = makeLocalIntersectionGuardMesh();
  for (int i = 0; i < 96; ++i) {
    const int base = static_cast<int>(mesh.vertices.size());
    const double x = 100.0 + 3.0 * static_cast<double>(i);
    mesh.vertices.push_back(manumesh::Vec3(x, 100.0, 0.0));
    mesh.vertices.push_back(manumesh::Vec3(x + 1.4, 100.0, 0.0));
    mesh.vertices.push_back(manumesh::Vec3(x, 101.2, 0.0));
    mesh.faces.push_back(manumesh::Face{{base, base + 1, base + 2}});
  }
  return mesh;
}

manumesh::Mesh makeCoplanarOverlapGuardMesh() {
  manumesh::Mesh mesh;
  mesh.vertices = {
      manumesh::Vec3(0.0, 0.0, 0.0),  manumesh::Vec3(2.0, 0.0, 0.0),
      manumesh::Vec3(0.0, -0.2, 0.0), manumesh::Vec3(1.0, 0.0, 0.0),
      manumesh::Vec3(0.0, 1.0, 0.0),  manumesh::Vec3(0.2, 0.2, 0.0),
      manumesh::Vec3(0.7, 0.2, 0.0),  manumesh::Vec3(0.2, 0.7, 0.0),
  };
  mesh.faces = {
      manumesh::Face{{0, 1, 2}},
      manumesh::Face{{1, 3, 4}},
      manumesh::Face{{5, 6, 7}},
  };
  return mesh;
}

manumesh::Mesh makeCoplanarSeparatedGuardMesh() {
  manumesh::Mesh mesh = makeCoplanarOverlapGuardMesh();
  mesh.vertices[5] = manumesh::Vec3(2.2, 2.2, 0.0);
  mesh.vertices[6] = manumesh::Vec3(2.7, 2.2, 0.0);
  mesh.vertices[7] = manumesh::Vec3(2.2, 2.7, 0.0);
  return mesh;
}

manumesh::Mesh makePolygonalFeatureChordMesh() {
  manumesh::Mesh mesh;
  mesh.vertices = {
      manumesh::Vec3(0.0, 0.0, 0.0),  manumesh::Vec3(3.0, 0.0, 0.0),
      manumesh::Vec3(2.5, 1.0, 0.0),  manumesh::Vec3(1.2, 2.2, 0.0),
      manumesh::Vec3(-0.4, 1.3, 0.0),
  };
  mesh.faces = {
      manumesh::Face{{0, 1, 3}},
      manumesh::Face{{1, 2, 3}},
      manumesh::Face{{0, 3, 4}},
  };
  return mesh;
}

manumesh::Mesh makePlacementFallbackMesh() {
  manumesh::Mesh mesh;
  mesh.vertices = {
      manumesh::Vec3(0.0, 0.0, 0.0),
      manumesh::Vec3(1.0, 0.0, 0.0),
      manumesh::Vec3(0.0, 0.9, 0.0),
      manumesh::Vec3(1.0, 0.1, 0.0),
  };
  mesh.faces = {
      manumesh::Face{{0, 1, 2}},
      manumesh::Face{{1, 3, 2}},
  };
  return mesh;
}

} // namespace

TEST(ManuMesh, BuiltInGeneratorsCoverDemoAndIndustrialModels) {
  const std::vector<std::string> names = {
      "plane",         "clustered-plane", "hole-plane", "ridge",
      "noisy-plane",   "sine-terrain",    "terrace",    "bump",
      "cylinder",      "torus",           "cube",       "thin-fin",
      "stepped-shaft", "pipe-coupling",   "pulley",
  };

  for (const std::string& name : names) {
    manumesh::Mesh mesh;
    std::string error;
    EXPECT_TRUE(manumesh::generateMeshByName(name, 24, mesh, &error))
        << name << ": " << error;
    EXPECT_FALSE(mesh.empty()) << name;

    const manumesh::simplification::MeshStats stats =
        manumesh::simplification::computeMeshStats(mesh);
    EXPECT_EQ(stats.vertices, static_cast<int>(mesh.vertices.size())) << name;
    EXPECT_EQ(stats.faces, static_cast<int>(mesh.faces.size())) << name;
    EXPECT_GT(stats.edges, 0) << name;
    EXPECT_GT(stats.area, 0.0) << name;
    EXPECT_GE(stats.meanTriangleQuality, 0.0) << name;
  }

  manumesh::Mesh mesh;
  std::string error;
  EXPECT_FALSE(manumesh::generateMeshByName("not-a-generator", 16, mesh, &error));
  EXPECT_FALSE(error.empty());

  error.clear();
  EXPECT_FALSE(manumesh::generateMeshByName("flange", 24, mesh, &error));
  EXPECT_FALSE(error.empty());
}

TEST(ManuMesh, ExternalFinishedFlangeFixtureLoadsWithFeatures) {
  const manumesh::Mesh mesh = loadExternalStl("openfoam_flange.stl");
  ASSERT_FALSE(mesh.empty());
  EXPECT_GT(mesh.faces.size(), 1000u);

  manumesh::feature::FeatureOptions options =
      manumesh::test::circularFeatureOptions(0.08);
  options.featureAngleDeg = 25.0;
  const manumesh::feature::FeatureAnalysis analysis =
      manumesh::feature::detectFeatureCurves(mesh, options);
  EXPECT_GT(analysis.featureEdges, 0);
  EXPECT_GT(analysis.graph.edges.size(), 0u);
  EXPECT_GT(analysis.loops.size(), 0u);
}

TEST(ManuMesh, WeightModesRoundTripAndRejectUnknownValues) {
  EXPECT_EQ(manumesh::simplification::WeightMode::Uniform,
            manumesh::simplification::parseWeightMode("uniform"));
  EXPECT_EQ(manumesh::simplification::WeightMode::Dihedral,
            manumesh::simplification::parseWeightMode("dihedral"));
  EXPECT_EQ(manumesh::simplification::WeightMode::NormalTensor,
            manumesh::simplification::parseWeightMode("normal-tensor"));
  EXPECT_EQ(manumesh::simplification::WeightMode::Height,
            manumesh::simplification::parseWeightMode("height"));
  EXPECT_EQ(manumesh::simplification::WeightMode::XBand,
            manumesh::simplification::parseWeightMode("xband"));

  EXPECT_EQ("uniform", manumesh::simplification::toString(
                           manumesh::simplification::WeightMode::Uniform));
  EXPECT_EQ("dihedral", manumesh::simplification::toString(
                            manumesh::simplification::WeightMode::Dihedral));
  EXPECT_EQ("normal-tensor", manumesh::simplification::toString(
                                 manumesh::simplification::WeightMode::NormalTensor));
  EXPECT_EQ("height", manumesh::simplification::toString(
                          manumesh::simplification::WeightMode::Height));
  EXPECT_EQ("xband", manumesh::simplification::toString(
                         manumesh::simplification::WeightMode::XBand));

  EXPECT_THROW(manumesh::simplification::parseWeightMode("paper"),
               std::invalid_argument);
}

TEST(ManuMesh, SimplificationNamespaceApiIsProjectScoped) {
  static_assert(std::is_same_v<manumesh::simplification::SimplifyOptions,
                               simplification::SimplifyOptions>);
  static_assert(std::is_same_v<manumesh::simplification::SimplifyReport,
                               simplification::SimplifyReport>);
  static_assert(std::is_same_v<manumesh::simplification::QEMSimplifier,
                               simplification::QEMSimplifier>);

  const manumesh::Mesh input = manumesh::generatePlaneGrid(4, 1.0, false);
  simplification::SimplifyOptions options = standardQemOptions(0.5);
  options.targetFaces = 8;

  simplification::SimplifyReport report;
  const manumesh::Mesh output = simplification::simplifyMesh(input, options, &report);
  EXPECT_LE(output.faces.size(), static_cast<std::size_t>(options.targetFaces));
  EXPECT_EQ(output.faces.size(), static_cast<std::size_t>(report.finalFaces));
  EXPECT_EQ("uniform", simplification::toString(simplification::WeightMode::Uniform));

  const manumesh::PlainMesh plainOutput =
      simplification::simplifyPlainMesh(manumesh::toPlainMesh(input), options, nullptr);
  EXPECT_EQ(output.faces.size(), plainOutput.faces.size());
}

TEST(ManuMesh, NormalTensorScoresSeparatePlaneFromRidge) {
  const manumesh::Mesh plane = manumesh::generatePlaneGrid(24, 2.0, false);
  const manumesh::Mesh ridge = manumesh::generateRidgeGrid(24, 2.0, 0.5);

  const std::vector<manumesh::feature::NormalTensorVertex> planeTensor =
      manumesh::feature::computeNormalTensorFeatures(plane);
  const std::vector<manumesh::feature::NormalTensorVertex> ridgeTensor =
      manumesh::feature::computeNormalTensorFeatures(ridge);

  double planeMax = 0.0;
  double ridgeMax = 0.0;
  for (const manumesh::feature::NormalTensorVertex& vertex : planeTensor) {
    planeMax = std::max(planeMax, vertex.featureScore);
  }
  for (const manumesh::feature::NormalTensorVertex& vertex : ridgeTensor) {
    ridgeMax = std::max(ridgeMax, vertex.featureScore);
  }

  EXPECT_LT(planeMax, 1e-8);
  EXPECT_GT(ridgeMax, 0.08);
}

TEST(ManuMesh, NormalTensorAddsFeatureEdgesWhenDihedralThresholdIsStrict) {
  const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);
  manumesh::feature::FeatureOptions options;
  options.featureAngleDeg = 179.0;
  options.normalTensorFeatureThreshold = 0.06;
  options.normalTensorMinEdgeAlignment = 0.2;

  const manumesh::feature::FeatureAnalysis features =
      manumesh::feature::detectFeatureCurves(input, options);

  EXPECT_EQ(0, features.dihedralFeatureEdges);
  EXPECT_GT(features.normalTensorFeatureEdges, 0);
  EXPECT_GT(features.maxNormalTensorFeatureScore, 0.06);
}

TEST(ManuMesh, NormalTensorWeightModeAppliesSpatiallyVaryingWeights) {
  const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);
  manumesh::simplification::SimplifyOptions options = paperLineQuadricsOptions(0.70);
  options.weightMode = manumesh::simplification::WeightMode::NormalTensor;
  options.normalTensorSmoothingIterations = 1;

  const SimplifiedMesh result = simplifyWithReport(input, options);

  expectBudgetedSimplification(result, input, 0.70);
  EXPECT_GT(result.report.maxAppliedLineWeight, result.report.minAppliedLineWeight);
}

TEST(ManuMesh, StrictTriangleQualityRejectsPoorCollapsePlacements) {
  const manumesh::Mesh input = manumesh::generatePlaneGrid(4, 1.0, false);

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
  options.minTriangleQuality = 0.95;
  options.maxNormalDeviationDeg = 180.0;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  EXPECT_FALSE(result.mesh.empty());
  EXPECT_GT(result.report.qualityRejectedCollapses, 0);
  EXPECT_EQ(result.report.rejectedCollapses,
            result.report.topologyRejectedCollapses +
                result.report.normalFlipRejectedCollapses +
                result.report.qualityRejectedCollapses +
                result.report.boundaryRejectedCollapses +
                result.report.selfIntersectionRejectedCollapses +
                result.report.curveBudgetRejectedCollapses +
                result.report.errorRejectedCollapses +
                result.report.featureRejectedCollapses);
}

TEST(ManuMesh, TriesEndpointPlacementWhenBestPlacementFailsLegality) {
  const manumesh::Mesh input = makePlacementFallbackMesh();

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.5);
  options.targetFaces = 1;
  options.minTriangleQuality = 0.35;
  options.maxNormalDeviationDeg = 180.0;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::ReachedTarget,
            result.report.terminationReason);
  EXPECT_EQ(1, result.report.finalFaces);
  EXPECT_EQ(0, result.report.rejectedCollapses);
}

TEST(ManuMesh, StrictNormalDeviationRejectsFoldoverRisk) {
  const manumesh::Mesh input = manumesh::generateCubeGrid(3, 1.0);

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
  options.minTriangleQuality = 0.0;
  options.maxNormalDeviationDeg = 0.0;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  EXPECT_FALSE(result.mesh.empty());
  EXPECT_GT(result.report.normalFlipRejectedCollapses, 0);
  EXPECT_EQ(result.report.rejectedCollapses,
            result.report.topologyRejectedCollapses +
                result.report.normalFlipRejectedCollapses +
                result.report.qualityRejectedCollapses +
                result.report.boundaryRejectedCollapses +
                result.report.selfIntersectionRejectedCollapses +
                result.report.curveBudgetRejectedCollapses +
                result.report.errorRejectedCollapses +
                result.report.featureRejectedCollapses);
}

TEST(ManuMesh, StrictLocalErrorRejectsLargeVertexDrift) {
  const manumesh::Mesh input = manumesh::generatePlaneGrid(3, 2.0, false);

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
  options.maxNormalDeviationDeg = 180.0;
  options.maxLocalErrorRatio = 1e-12;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  EXPECT_FALSE(result.mesh.empty());
  EXPECT_GT(result.report.errorRejectedCollapses, 0);
  EXPECT_EQ(result.report.rejectedCollapses,
            result.report.topologyRejectedCollapses +
                result.report.normalFlipRejectedCollapses +
                result.report.qualityRejectedCollapses +
                result.report.boundaryRejectedCollapses +
                result.report.selfIntersectionRejectedCollapses +
                result.report.curveBudgetRejectedCollapses +
                result.report.errorRejectedCollapses +
                result.report.featureRejectedCollapses);
}

TEST(ManuMesh, SimplifiesOpenBoundaryEdgesWhenTopologyIsPreserved) {
  const manumesh::Mesh input = manumesh::generatePlaneGrid(8, 2.0, false);
  const manumesh::simplification::MeshStats inputStats =
      manumesh::simplification::computeMeshStats(input);
  ASSERT_GT(inputStats.boundaryEdges, 0);
  ASSERT_EQ(1, countBoundaryComponents(input));

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.08);
  options.preserveBoundary = true;
  const SimplifiedMesh result = simplifyWithReport(input, options);
  const manumesh::simplification::MeshStats outputStats =
      manumesh::simplification::computeMeshStats(result.mesh);

  EXPECT_FALSE(result.mesh.empty());
  EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
  EXPECT_LT(outputStats.boundaryEdges, inputStats.boundaryEdges);
  EXPECT_LT(countBoundaryVertices(result.mesh), countBoundaryVertices(input));
  EXPECT_EQ(1, countBoundaryComponents(result.mesh));
  EXPECT_EQ(0, outputStats.nonManifoldEdges);
  EXPECT_GT(result.report.boundaryRejectedCollapses, 0);
}

TEST(ManuMesh, KeepsSeparateBoundaryLoopsWhenBoundaryEdgesCollapse) {
  const manumesh::Mesh input = manumesh::generateHolePlaneGrid(16, 2.0, 0.35);
  const manumesh::simplification::MeshStats inputStats =
      manumesh::simplification::computeMeshStats(input);
  ASSERT_GT(inputStats.boundaryEdges, 0);
  ASSERT_GE(countBoundaryComponents(input), 2);

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.15);
  options.preserveBoundary = true;
  const SimplifiedMesh result = simplifyWithReport(input, options);
  const manumesh::simplification::MeshStats outputStats =
      manumesh::simplification::computeMeshStats(result.mesh);

  EXPECT_FALSE(result.mesh.empty());
  EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
  EXPECT_EQ(countBoundaryComponents(input), countBoundaryComponents(result.mesh));
  EXPECT_LE(outputStats.boundaryEdges, inputStats.boundaryEdges);
  EXPECT_EQ(0, outputStats.nonManifoldEdges);
  EXPECT_GT(result.report.boundaryRejectedCollapses, 0);
}

TEST(ManuMesh, LocalIntersectionGuardRejectsIntersectingCollapse) {
  const manumesh::Mesh input = makeLocalIntersectionGuardMesh();

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
  options.targetFaces = 1;
  options.preventLocalIntersections = true;
  options.maxNormalDeviationDeg = 180.0;
  options.minTriangleQuality = 0.0;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  EXPECT_FALSE(result.mesh.empty());
  EXPECT_GT(result.report.selfIntersectionRejectedCollapses, 0);
  EXPECT_EQ(result.report.rejectedCollapses,
            result.report.topologyRejectedCollapses +
                result.report.normalFlipRejectedCollapses +
                result.report.qualityRejectedCollapses +
                result.report.boundaryRejectedCollapses +
                result.report.selfIntersectionRejectedCollapses +
                result.report.curveBudgetRejectedCollapses +
                result.report.errorRejectedCollapses +
                result.report.featureRejectedCollapses);
}

TEST(ManuMesh, LocalIntersectionGuardFindsIndexedDistantCandidates) {
  const manumesh::Mesh input = makeSpatialIntersectionGuardMeshWithFarFaces();

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.98);
  options.targetFaces = static_cast<int>(input.faces.size()) - 1;
  options.preventLocalIntersections = true;
  options.maxNormalDeviationDeg = 180.0;
  options.minTriangleQuality = 0.0;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  EXPECT_FALSE(result.mesh.empty());
  EXPECT_GT(result.report.selfIntersectionRejectedCollapses, 0);
  EXPECT_EQ(result.report.rejectedCollapses,
            result.report.topologyRejectedCollapses +
                result.report.normalFlipRejectedCollapses +
                result.report.qualityRejectedCollapses +
                result.report.boundaryRejectedCollapses +
                result.report.selfIntersectionRejectedCollapses +
                result.report.curveBudgetRejectedCollapses +
                result.report.errorRejectedCollapses +
                result.report.featureRejectedCollapses);
}

TEST(ManuMesh, LocalIntersectionGuardUsesFallbackPlacementForCoplanarOverlap) {
  const manumesh::Mesh input = makeCoplanarOverlapGuardMesh();

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
  options.targetFaces = 1;
  options.preventLocalIntersections = true;
  options.maxNormalDeviationDeg = 180.0;
  options.minTriangleQuality = 0.0;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  EXPECT_FALSE(result.mesh.empty());
  EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::ReachedTarget,
            result.report.terminationReason);
  EXPECT_EQ(1, result.report.finalFaces);
  EXPECT_EQ(0, result.report.selfIntersectionRejectedCollapses);
}

TEST(ManuMesh, LocalIntersectionGuardAllowsCoplanarSeparatedTriangles) {
  const manumesh::Mesh input = makeCoplanarSeparatedGuardMesh();

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
  options.targetFaces = 1;
  options.preventLocalIntersections = true;
  options.maxNormalDeviationDeg = 180.0;
  options.minTriangleQuality = 0.0;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  EXPECT_FALSE(result.mesh.empty());
  EXPECT_EQ(0, result.report.selfIntersectionRejectedCollapses);
}

TEST(ManuMesh, LocalIntersectionGuardAllowsSharedCoplanarEdges) {
  const manumesh::Mesh input = manumesh::generatePlaneGrid(3, 1.0, false);

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.55);
  options.preventLocalIntersections = true;
  options.maxNormalDeviationDeg = 180.0;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  EXPECT_FALSE(result.mesh.empty());
  EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
  EXPECT_EQ(0, result.report.selfIntersectionRejectedCollapses);
}

TEST(ManuMesh, StrictPolygonalFeatureProtectionRejectsChordPlacement) {
  const manumesh::Mesh input = makePolygonalFeatureChordMesh();

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
  options.targetFaces = 1;
  options.preserveFeatureCurves = true;
  options.featureProtectionMode =
      manumesh::simplification::FeatureProtectionMode::AllFeatureEdges;
  options.useNormalTensorFeatures = false;
  options.featureAngleDeg = 179.0;
  options.circleFitRelativeThreshold = 0.0;
  options.ellipseFitRelativeThreshold = 0.0;
  options.minFeatureLoopVertices = 3;
  options.maxFeatureCurveDeviationRatio = 1e-9;
  options.maxNormalDeviationDeg = 180.0;
  options.minTriangleQuality = 0.0;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  EXPECT_FALSE(result.mesh.empty());
  EXPECT_GT(result.report.featureLoops, 0);
  EXPECT_GT(result.report.featureRejectedCollapses, 0);
  EXPECT_GT(result.report.genericFeatureRejectedCollapses, 0);
  EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::RejectionLimit,
            result.report.terminationReason);
  EXPECT_EQ(result.report.rejectedCollapses,
            result.report.topologyRejectedCollapses +
                result.report.normalFlipRejectedCollapses +
                result.report.qualityRejectedCollapses +
                result.report.boundaryRejectedCollapses +
                result.report.selfIntersectionRejectedCollapses +
                result.report.curveBudgetRejectedCollapses +
                result.report.errorRejectedCollapses +
                result.report.featureRejectedCollapses);
}

TEST(ManuMesh, PrimitiveModeKeepsPolygonalFeaturesSoftByDefault) {
  const manumesh::Mesh input = manumesh::generateCubeGrid(4, 1.0);

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.35);
  options.preserveFeatureCurves = true;
  options.featureProtectionMode =
      manumesh::simplification::FeatureProtectionMode::PrimitiveCurves;
  options.useNormalTensorFeatures = false;
  options.featureAngleDeg = 25.0;
  options.minFeatureLoopVertices = 4;
  options.maxNormalDeviationDeg = 180.0;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  EXPECT_FALSE(result.mesh.empty());
  EXPECT_GT(result.report.featureLoops, 0);
  EXPECT_GT(result.report.featureVertices, 0);
  EXPECT_EQ(0, result.report.genericFeatureRejectedCollapses);
}

TEST(ManuMesh, PlainMeshRoundTripsWithoutEigenInExchangeType) {
  manumesh::PlainMesh plain;
  plain.vertices = {
      {0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0},
  };
  plain.faces = {{{{0, 1, 2}}}};

  const manumesh::Mesh mesh = manumesh::toMesh(plain);
  ASSERT_EQ(3u, mesh.vertices.size());
  ASSERT_EQ(1u, mesh.faces.size());

  const manumesh::PlainMesh roundTrip = manumesh::toPlainMesh(mesh);
  EXPECT_EQ(plain.vertices.size(), roundTrip.vertices.size());
  EXPECT_EQ(plain.faces.size(), roundTrip.faces.size());
  EXPECT_DOUBLE_EQ(1.0, roundTrip.vertices[1].x);
  EXPECT_EQ(2, roundTrip.faces[0].v[2]);
}

TEST(ManuMesh, SimplifiesPlainMeshThroughEigenFreeEntryPoint) {
  const manumesh::PlainMesh input =
      manumesh::toPlainMesh(manumesh::generatePlaneGrid(8, 1.0, false));

  manumesh::simplification::SimplifyOptions options = standardQemOptions(0.50);
  options.maxNormalDeviationDeg = 180.0;

  manumesh::simplification::SimplifyReport report;
  const manumesh::PlainMesh output =
      manumesh::simplification::simplifyPlainMesh(input, options, &report);

  EXPECT_FALSE(output.faces.empty());
  EXPECT_EQ(report.initialFaces, static_cast<int>(input.faces.size()));
  EXPECT_EQ(report.finalFaces, static_cast<int>(output.faces.size()));
  EXPECT_LT(report.finalFaces, report.initialFaces);
  EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::ReachedTarget,
            report.terminationReason);
}

TEST(ManuMesh, QEMSimplifierObjectStoresOptionsAndLatestReport) {
  const manumesh::Mesh input = manumesh::generateCylinderGrid(24, 6, 1.0, 2.0);

  manumesh::simplification::SimplifyOptions options = paperLineQuadricsOptions(0.50);
  manumesh::simplification::QEMSimplifier simplifier(options);

  manumesh::simplification::SimplifyReport copiedReport;
  const manumesh::Mesh output = simplifier.simplify(input, &copiedReport);

  EXPECT_FALSE(output.empty());
  EXPECT_EQ(options.targetRatio, simplifier.options().targetRatio);
  EXPECT_EQ(copiedReport.finalFaces, simplifier.report().finalFaces);
  EXPECT_EQ(copiedReport.collapsedEdges, simplifier.report().collapsedEdges);
  EXPECT_LT(simplifier.report().finalFaces, simplifier.report().initialFaces);
  EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::ReachedTarget,
            simplifier.report().terminationReason);
}

TEST(ManuMesh, QEMSimplifierCopiesPimplStateIndependently) {
  manumesh::simplification::SimplifyOptions options = paperLineQuadricsOptions(0.60);
  manumesh::simplification::QEMSimplifier original(options);

  const manumesh::Mesh input = manumesh::generatePlaneGrid(8, 1.0, false);
  const manumesh::Mesh output = original.simplify(input);
  ASSERT_FALSE(output.empty());

  manumesh::simplification::QEMSimplifier copied = original;
  EXPECT_EQ(original.options().targetRatio, copied.options().targetRatio);
  EXPECT_EQ(original.report().finalFaces, copied.report().finalFaces);

  manumesh::simplification::QEMSimplifier moved = std::move(copied);
  EXPECT_EQ(original.report().finalFaces, moved.report().finalFaces);

  manumesh::simplification::SimplifyOptions movedFromOptions;
  movedFromOptions.targetRatio = 0.75;
  copied.setOptions(movedFromOptions);
  EXPECT_DOUBLE_EQ(0.75, copied.options().targetRatio);

  manumesh::simplification::SimplifyOptions copiedOptions = copied.options();
  copiedOptions.targetRatio = 0.25;
  copied.setOptions(copiedOptions);

  EXPECT_DOUBLE_EQ(0.60, original.options().targetRatio);
  EXPECT_DOUBLE_EQ(0.25, copied.options().targetRatio);
}

TEST(ManuMesh, ReportsFeatureLoopsOnCylinderCreases) {
  const manumesh::Mesh input = manumesh::generateCylinderGrid(32, 4, 1.0, 2.0);
  manumesh::feature::FeatureOptions options;
  options.featureAngleDeg = 30.0;

  const manumesh::feature::FeatureAnalysis features =
      manumesh::feature::detectFeatureCurves(input, options);

  EXPECT_GT(features.featureEdges, 0);
  EXPECT_GT(features.dihedralFeatureEdges, 0);
  EXPECT_FALSE(features.loops.empty());
}

TEST(ManuMesh, MeasuresCircularFeatureLoopAgainstDetectedCircle) {
  const manumesh::Mesh input = manumesh::generateCylinderGrid(32, 4, 1.0, 2.0);
  const manumesh::feature::FeatureAnalysis features =
      manumesh::feature::detectFeatureCurves(input, circularFeatureOptions());
  ASSERT_GT(countCircularLoops(features), 0);

  const auto loopIt = std::find_if(
      features.loops.begin(), features.loops.end(),
      [](const manumesh::feature::FeatureLoop& loop) { return loop.circular; });
  ASSERT_NE(loopIt, features.loops.end());

  const manumesh::feature::DirectionalCurveError error =
      manumesh::feature::measureLoopAgainstCircle(input, *loopIt, loopIt->center,
                                                  loopIt->normal, loopIt->radius);

  EXPECT_EQ(error.samples, static_cast<int>(loopIt->vertices.size()));
  EXPECT_NEAR(error.radialRms, loopIt->rmsRadialError, 1e-10);
  EXPECT_NEAR(error.planeRms, loopIt->rmsPlaneError, 1e-10);
  EXPECT_LT(error.radialMax, 1e-10);
  EXPECT_LT(error.planeMax, 1e-10);
}

TEST(ManuMesh, ExternalNasaIndustrialMeshesExposeRichFeatureTopology) {
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
    const manumesh::Mesh mesh = loadExternalStl(testCase.fileName);
    ASSERT_FALSE(mesh.empty());

    const manumesh::simplification::MeshStats stats =
        manumesh::simplification::computeMeshStats(mesh);
    EXPECT_GE(stats.faces, testCase.minFaces);
    EXPECT_GT(stats.edges, 0);
    EXPECT_GT(stats.area, 0.0);
    EXPECT_EQ(stats.nonManifoldEdges, 0);

    manumesh::feature::FeatureOptions featureOptions = circularFeatureOptions();
    featureOptions.featureAngleDeg = 30.0;
    featureOptions.circleFitRelativeThreshold = 0.05;
    const manumesh::feature::FeatureAnalysis features =
        manumesh::feature::detectFeatureCurves(mesh, featureOptions);
    EXPECT_GE(features.featureEdges, testCase.minFeatureEdges);
    EXPECT_GE(countCircularLoops(features), testCase.minCircularLoops);
    EXPECT_GT(features.dihedralFeatureEdges, 0);
  }
}

TEST(ManuMesh, ExternalDownloadedMeshesCompareIndustrialSimplificationModes) {
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
    const manumesh::Mesh input = loadExternalStl(testCase.fileName);
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

TEST(ManuMesh, Public2014CastingModelKeepsClosedTopologyAfterLineSimplify) {
  const manumesh::Mesh input = loadExternalMesh("casting_aimshape_2014.stl");
  ASSERT_FALSE(input.empty());

  const manumesh::simplification::MeshStats inputStats =
      manumesh::simplification::computeMeshStats(input);
  ASSERT_EQ(inputStats.boundaryEdges, 0);
  ASSERT_EQ(inputStats.nonManifoldEdges, 0);

  manumesh::simplification::SimplifyOptions options = paperLineQuadricsOptions(0.25);
  manumesh::simplification::SimplifyReport report;
  manumesh::simplification::QEMSimplifier simplifier(options);
  const manumesh::Mesh output = simplifier.simplify(input, &report);
  const manumesh::simplification::MeshStats outputStats =
      manumesh::simplification::computeMeshStats(output);

  EXPECT_FALSE(output.empty());
  EXPECT_LE(report.finalFaces,
            static_cast<int>(std::llround(input.faces.size() * 0.25)) + 2);
  EXPECT_EQ(outputStats.boundaryEdges, 0);
  EXPECT_EQ(outputStats.nonManifoldEdges, 0);
}

TEST(ManuMesh, ComputesMeshStatsForGeneratedCube) {
  const manumesh::Mesh input = manumesh::generateCubeGrid(4, 1.0);
  const manumesh::simplification::MeshStats stats =
      manumesh::simplification::computeMeshStats(input);

  EXPECT_EQ(stats.vertices, static_cast<int>(input.vertices.size()));
  EXPECT_EQ(stats.faces, static_cast<int>(input.faces.size()));
  EXPECT_GT(stats.edges, 0);
  EXPECT_GT(stats.area, 0.0);
  EXPECT_GT(stats.meanTriangleQuality, 0.0);
}

TEST(ManuMesh, MeshTopologyCachesBoundaryAndNonManifoldEdges) {
  manumesh::Mesh mesh;
  mesh.vertices = {
      manumesh::Vec3(0.0, 0.0, 0.0),  manumesh::Vec3(1.0, 0.0, 0.0),
      manumesh::Vec3(0.0, 1.0, 0.0),  manumesh::Vec3(0.0, 0.0, 1.0),
      manumesh::Vec3(0.0, 0.0, -1.0),
  };
  mesh.faces = {
      {{0, 1, 2}},
      {{1, 0, 3}},
      {{0, 1, 4}},
  };

  const manumesh::Result<manumesh::MeshTopology> topologyResult =
      manumesh::MeshTopology::build(mesh);
  ASSERT_TRUE(topologyResult.ok()) << topologyResult.status().message();
  const manumesh::MeshTopology& topology = topologyResult.value();

  EXPECT_EQ(topology.vertexCount(), 5);
  EXPECT_EQ(topology.faceCount(), 3);
  EXPECT_EQ(topology.edgeCount(), 7);
  EXPECT_EQ(topology.boundaryEdgeCount(), 6);
  EXPECT_EQ(topology.nonManifoldEdgeCount(), 1);

  const manumesh::simplification::MeshStats stats =
      manumesh::simplification::computeMeshStats(mesh);
  EXPECT_EQ(stats.edges, topology.edgeCount());
  EXPECT_EQ(stats.boundaryEdges, topology.boundaryEdgeCount());
  EXPECT_EQ(stats.nonManifoldEdges, topology.nonManifoldEdgeCount());
}

TEST(ManuMesh, MeshTopologyCopiesAndMovesPimplCache) {
  const manumesh::Mesh mesh = manumesh::generatePlaneGrid(4, 1.0, false);
  const manumesh::Result<manumesh::MeshTopology> topologyResult =
      manumesh::MeshTopology::build(mesh);
  ASSERT_TRUE(topologyResult.ok()) << topologyResult.status().message();

  manumesh::MeshTopology copied = topologyResult.value();
  EXPECT_EQ(topologyResult.value().vertexCount(), copied.vertexCount());
  EXPECT_EQ(topologyResult.value().edgeCount(), copied.edgeCount());
  EXPECT_EQ(topologyResult.value().boundaryEdgeCount(), copied.boundaryEdgeCount());

  manumesh::MeshTopology moved = std::move(copied);
  EXPECT_EQ(static_cast<int>(mesh.vertices.size()), moved.vertexCount());
  EXPECT_GT(moved.edgeCount(), 0);
  EXPECT_GT(moved.boundaryEdgeCount(), 0);
  EXPECT_EQ(0, copied.vertexCount());
}

TEST(ManuMesh, MeshTopologyRejectsInvalidFaces) {
  manumesh::Mesh mesh;
  mesh.vertices = {
      manumesh::Vec3(0.0, 0.0, 0.0),
      manumesh::Vec3(1.0, 0.0, 0.0),
      manumesh::Vec3(0.0, 1.0, 0.0),
  };
  mesh.faces = {{{0, 1, 5}}};

  const manumesh::Result<manumesh::MeshTopology> topologyResult =
      manumesh::MeshTopology::build(mesh);
  EXPECT_FALSE(topologyResult.ok());
  EXPECT_EQ(topologyResult.status().code(), manumesh::StatusCode::InvalidArgument);
}

TEST(ManuMesh, MeshUtilitiesRejectMalformedInputWithoutThrowing) {
  const std::filesystem::path objPath =
      std::filesystem::temp_directory_path() / "line_quadrics_bad_face.obj";
  {
    std::ofstream out(objPath);
    out << "v 0 0 0\n";
    out << "v 1 0 0\n";
    out << "v 0 1 0\n";
    out << "f nope 2 3\n";
  }

  manumesh::Mesh mesh;
  std::string error;
  EXPECT_FALSE(manumesh::loadObj(objPath.string(), mesh, &error));
  EXPECT_FALSE(error.empty());
  std::filesystem::remove(objPath);

  manumesh::Mesh invalid;
  invalid.vertices = {manumesh::Vec3(0.0, 0.0, 0.0)};
  invalid.faces = {{{0, 1, 2}}};
  error.clear();
  const std::filesystem::path stlPath =
      std::filesystem::temp_directory_path() / "line_quadrics_invalid.stl";
  EXPECT_FALSE(manumesh::saveAsciiStl(stlPath.string(), invalid, "invalid", &error));
  EXPECT_FALSE(error.empty());

  manumesh::Mesh nonFinite;
  nonFinite.vertices = {
      manumesh::Vec3(0.0, 0.0, 0.0),
      manumesh::Vec3(std::numeric_limits<double>::infinity(), 0.0, 0.0),
      manumesh::Vec3(0.0, 1.0, 0.0),
  };
  nonFinite.faces = {{{0, 1, 2}}};
  error.clear();
  EXPECT_FALSE(manumesh::validateMeshGeometry(nonFinite, &error));
  EXPECT_FALSE(error.empty());
}

TEST(ManuMesh, SimplifierRejectsInvalidOptionsAndMeshes) {
  const manumesh::Mesh input = manumesh::generatePlaneGrid(4, 1.0, false);

  manumesh::simplification::SimplifyOptions options;
  options.targetRatio = 0.0;
  EXPECT_THROW(manumesh::simplification::simplifyMesh(input, options),
               std::invalid_argument);

  manumesh::Mesh invalid;
  invalid.vertices = {
      manumesh::Vec3(0.0, 0.0, 0.0),
      manumesh::Vec3(1.0, 0.0, 0.0),
      manumesh::Vec3(0.0, 1.0, 0.0),
  };
  invalid.faces = {{{0, 1, 5}}};
  EXPECT_THROW(manumesh::simplification::simplifyMesh(
                   invalid, manumesh::simplification::SimplifyOptions{}),
               std::invalid_argument);

  manumesh::Mesh nonFinite = input;
  nonFinite.vertices[0].x() = std::numeric_limits<double>::quiet_NaN();
  EXPECT_THROW(manumesh::simplification::simplifyMesh(
                   nonFinite, manumesh::simplification::SimplifyOptions{}),
               std::invalid_argument);
}

TEST(ManuMesh, MeshDistanceIsZeroForIdenticalMeshAndFiniteAfterSimplify) {
  const manumesh::Mesh input =
      loadExternalStl("thingi10k/thingi10k_108336_projekt_muse_z_system.stl");
  ASSERT_FALSE(input.empty());

  const manumesh::simplification::DistanceStats identical =
      manumesh::simplification::compareMeshesBySampledDistance(input, input, 32);
  EXPECT_NEAR(identical.meanOriginalToSimplified, 0.0, 1e-12);
  EXPECT_NEAR(identical.maxOriginalToSimplified, 0.0, 1e-12);
  EXPECT_NEAR(identical.meanSimplifiedToOriginal, 0.0, 1e-12);
  EXPECT_NEAR(identical.maxSimplifiedToOriginal, 0.0, 1e-12);

  const SimplifiedMesh simplified =
      simplifyWithReport(input, paperLineQuadricsOptions(0.35));
  const manumesh::simplification::DistanceStats distance =
      manumesh::simplification::compareMeshesBySampledDistance(input, simplified.mesh,
                                                               32);
  EXPECT_GE(distance.meanOriginalToSimplified, 0.0);
  EXPECT_GE(distance.maxOriginalToSimplified, distance.meanOriginalToSimplified);
  EXPECT_GE(distance.meanSimplifiedToOriginal, 0.0);
  EXPECT_GE(distance.maxSimplifiedToOriginal, distance.meanSimplifiedToOriginal);
}

TEST(ManuMesh, ExternalBinaryStlLoadKeepsGeometryUsable) {
  const manumesh::Mesh loaded =
      loadExternalStl("thingi10k/thingi10k_108336_projekt_muse_z_system.stl");
  ASSERT_FALSE(loaded.empty());

  const manumesh::simplification::MeshStats stats =
      manumesh::simplification::computeMeshStats(loaded);
  EXPECT_EQ(stats.faces, static_cast<int>(loaded.faces.size()));
  EXPECT_GT(stats.vertices, 0);
  EXPECT_GT(stats.edges, 0);
  EXPECT_GT(stats.area, 0.0);
  EXPECT_EQ(stats.nonManifoldEdges, 0);
}
