#include "line_quadrics_qem/FeatureDetection.h"
#include "line_quadrics_qem/MeshGenerators.h"
#include "line_quadrics_qem/Metrics.h"
#include "line_quadrics_qem/QEMSimplifier.h"

#include <gtest/gtest.h>

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

TEST(LineQuadricsQem, ReportsFeatureLoopsOnCylinderCreases) {
  const lq::Mesh input = lq::generateCylinderGrid(32, 4, 1.0, 2.0);
  lq::FeatureOptions options;
  options.featureAngleDeg = 30.0;

  const lq::FeatureAnalysis features = lq::detectFeatureCurves(input, options);

  EXPECT_GT(features.featureEdges, 0);
  EXPECT_GT(features.dihedralFeatureEdges, 0);
  EXPECT_FALSE(features.loops.empty());
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
