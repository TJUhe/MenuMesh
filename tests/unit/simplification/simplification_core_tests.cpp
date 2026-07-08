#include "SimplificationTestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/PlainSimplifier.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "core/PlainMesh.h"
#include "../../../src/common/detail/MeshQueries.h"

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

using manumesh::test::countCircularLoops;
using manumesh::test::loadExternalMesh;
using manumesh::test::loadExternalStl;
using manumesh::test::SimplifiedMesh;
using manumesh::test::simplifyWithReport;
using namespace manumesh::test::simplification;

namespace simplification = manumesh::simplification;
TEST(ManuMesh, MeshQueriesComputeLocalVertexEdgeScale) {
  manumesh::Mesh mesh;
  mesh.vertices = {
      manumesh::Vec3(0.0, 0.0, 0.0),
      manumesh::Vec3(1.0, 0.0, 0.0),
      manumesh::Vec3(0.0, 2.0, 0.0),
      manumesh::Vec3(10.0, 10.0, 10.0),
  };
  mesh.faces = {{{0, 1, 2}}};

  const std::vector<double> scale =
      manumesh::detail::computeVertexAverageEdgeLength(mesh);

  ASSERT_EQ(4u, scale.size());
  const double hyp = std::sqrt(5.0);
  EXPECT_NEAR((1.0 + 2.0) / 2.0, scale[0], 1e-12);
  EXPECT_NEAR((1.0 + hyp) / 2.0, scale[1], 1e-12);
  EXPECT_NEAR((2.0 + hyp) / 2.0, scale[2], 1e-12);
  EXPECT_NEAR((1.0 + 2.0 + hyp) / 3.0, scale[3], 1e-12);
}

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
