#include "FeatureDetectionTestSupport.h"
#include "TestSupport.h"

#include <algorithm>
#include <gtest/gtest.h>

namespace {

namespace feature = manumesh::feature;

using FeatureAnalysis = feature::FeatureAnalysis;
using FeatureOptions = feature::FeatureOptions;
using Mesh = manumesh::Mesh;
using VertexFeature = feature::VertexFeature;
using manumesh::test::loadExternalStl;
using manumesh::test::feature_detection::countClosedLoops;
using manumesh::test::feature_detection::discreteOnlyOptions;

} // namespace

TEST(FeatureDetection, LargeExternalThingi10kMeshProducesNontrivialFeatureGraph) {
  const Mesh mesh = loadExternalStl(
      "thingi10k/thingi10k_105382_measuring_cup_with_handle_and_spout.stl");
  ASSERT_FALSE(mesh.empty());
  ASSERT_GT(mesh.faces.size(), 10000u);

  FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = 0.08;
  options.minFeatureLoopVertices = 8;
  const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

  const int featureVertices = static_cast<int>(
      std::count_if(features.vertices.begin(), features.vertices.end(),
                    [](const VertexFeature& vertex) { return vertex.isFeature; }));

  EXPECT_EQ(mesh.vertices.size(), features.vertices.size());
  EXPECT_EQ(features.featureEdges, static_cast<int>(features.graph.edges.size()));
  EXPECT_GT(featureVertices, 100);
  EXPECT_GT(features.featureEdges, 100);
  EXPECT_GT(features.loops.size(), 5u);
  EXPECT_GT(countClosedLoops(features), 0);
  EXPECT_GT(features.boundaryFeatureEdges + features.dihedralFeatureEdges +
                features.nonManifoldFeatureEdges,
            100);
  EXPECT_GT(features.graph.junctionVertices.size(), 0u);
}
