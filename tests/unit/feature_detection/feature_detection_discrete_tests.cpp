#include "FeatureDetectionTestSupport.h"

#include <algorithm>
#include <utility>
#include <gtest/gtest.h>

namespace {

namespace feature = manumesh::feature;

using FeatureAnalysis = feature::FeatureAnalysis;
using FeatureOptions = feature::FeatureOptions;
using Mesh = manumesh::Mesh;
using Vec3 = manumesh::Vec3;
using VertexFeature = feature::VertexFeature;
using manumesh::test::feature_detection::countClosedLoops;
using manumesh::test::feature_detection::discreteOnlyOptions;
using manumesh::test::feature_detection::makeMixedDiscreteEvidenceMesh;

} // namespace

TEST(FeatureDetection, ClassifiesBoundaryEdgesOnOpenTriangle) {
  Mesh mesh;
  mesh.vertices = {
      Vec3(0.0, 0.0, 0.0),
      Vec3(1.0, 0.0, 0.0),
      Vec3(0.0, 1.0, 0.0),
  };
  mesh.faces = {{{0, 1, 2}}};

  const FeatureAnalysis features =
      feature::detectFeatureCurves(mesh, discreteOnlyOptions());

  EXPECT_EQ(3, features.featureEdges);
  EXPECT_EQ(3, features.boundaryFeatureEdges);
  EXPECT_EQ(0, features.dihedralFeatureEdges);
  EXPECT_EQ(0, features.nonManifoldFeatureEdges);
  EXPECT_EQ(1, countClosedLoops(features));
  ASSERT_EQ(mesh.vertices.size(), features.vertices.size());
  for (const VertexFeature& vertex : features.vertices) {
    EXPECT_TRUE(vertex.isFeature);
  }
  ASSERT_EQ(1u, features.components.size());
  EXPECT_GT(features.components.front().confidence, 0.70);
  EXPECT_EQ(1, features.highConfidenceFeatureComponents);
  EXPECT_GT(features.meanFeatureComponentConfidence, 0.70);
  ASSERT_FALSE(features.loops.empty());
  EXPECT_EQ(0, features.loops.front().componentId);
  EXPECT_GT(features.loops.front().componentConfidence, 0.70);
}

TEST(FeatureDetection, BenchmarksDetectedEdgesAgainstGroundTruthLabels) {
  Mesh mesh;
  mesh.vertices = {
      Vec3(0.0, 0.0, 0.0),
      Vec3(1.0, 0.0, 0.0),
      Vec3(0.0, 1.0, 0.0),
  };
  mesh.faces = {{{0, 1, 2}}};

  const FeatureAnalysis features =
      feature::detectFeatureCurves(mesh, discreteOnlyOptions());
  const feature::FeatureEdgeBenchmark benchmark =
      feature::benchmarkFeatureEdges(features, {{0, 1}, {1, 2}});

  EXPECT_EQ(2, benchmark.groundTruthEdges);
  EXPECT_EQ(3, benchmark.detectedEdges);
  EXPECT_EQ(2, benchmark.truePositiveEdges);
  EXPECT_EQ(1, benchmark.falsePositiveEdges);
  EXPECT_EQ(0, benchmark.falseNegativeEdges);
  EXPECT_NEAR(2.0 / 3.0, benchmark.edgePrecision, 1e-12);
  EXPECT_DOUBLE_EQ(1.0, benchmark.edgeRecall);
  EXPECT_GT(benchmark.loopClosureRate, 0.9);
  EXPECT_GT(benchmark.meanComponentConfidence, 0.70);
}

TEST(FeatureDetection, ClassifiesNonManifoldEdgesSeparately) {
  Mesh mesh;
  mesh.vertices = {
      Vec3(0.0, 0.0, 0.0), Vec3(1.0, 0.0, 0.0),  Vec3(0.0, 1.0, 0.0),
      Vec3(0.0, 0.0, 1.0), Vec3(0.0, 0.0, -1.0),
  };
  mesh.faces = {
      {{0, 1, 2}},
      {{1, 0, 3}},
      {{0, 1, 4}},
  };

  const FeatureAnalysis features =
      feature::detectFeatureCurves(mesh, discreteOnlyOptions());

  EXPECT_EQ(1, features.nonManifoldFeatureEdges);
  EXPECT_GT(features.boundaryFeatureEdges, 0);
  EXPECT_GE(features.featureEdges, features.nonManifoldFeatureEdges);
  EXPECT_TRUE(features.vertices[0].isFeature);
  EXPECT_TRUE(features.vertices[1].isFeature);
}

TEST(FeatureDetection, DihedralThresholdControlsCreaseEdges) {
  Mesh mesh;
  mesh.vertices = {
      Vec3(0.0, 0.0, 0.0),
      Vec3(1.0, 0.0, 0.0),
      Vec3(0.0, 1.0, 0.0),
      Vec3(0.0, 0.0, 1.0),
  };
  mesh.faces = {
      {{0, 1, 2}},
      {{1, 0, 3}},
  };

  FeatureOptions strict = discreteOnlyOptions();
  strict.featureAngleDeg = 120.0;
  const FeatureAnalysis strictFeatures = feature::detectFeatureCurves(mesh, strict);

  FeatureOptions permissive = discreteOnlyOptions();
  permissive.featureAngleDeg = 30.0;
  const FeatureAnalysis permissiveFeatures =
      feature::detectFeatureCurves(mesh, permissive);

  EXPECT_EQ(0, strictFeatures.dihedralFeatureEdges);
  EXPECT_EQ(1, permissiveFeatures.dihedralFeatureEdges);
  EXPECT_GT(permissiveFeatures.featureEdges, strictFeatures.featureEdges);
}

TEST(FeatureDetection, ComposesDiscreteEvidenceSourceCounters) {
  const Mesh mesh = makeMixedDiscreteEvidenceMesh();

  FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 30.0;
  const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

  EXPECT_EQ(0, features.normalTensorFeatureEdges);
  EXPECT_EQ(1, features.nonManifoldFeatureEdges);
  EXPECT_EQ(1, features.dihedralFeatureEdges);
  EXPECT_GT(features.boundaryFeatureEdges, 0);
  EXPECT_EQ(features.featureEdges, static_cast<int>(features.graph.edges.size()));
  EXPECT_EQ(features.featureEdges,
            features.boundaryFeatureEdges + features.dihedralFeatureEdges +
                features.normalTensorFeatureEdges + features.nonManifoldFeatureEdges);
}

TEST(FeatureDetection, SplitsBranchedFeatureGraphAndMarksJunctions) {
  Mesh mesh;
  mesh.vertices = {
      Vec3(0.0, 0.0, 0.0),  Vec3(1.0, 0.0, 0.0),  Vec3(0.5, 1.0, 0.0),
      Vec3(-1.0, 0.0, 0.0), Vec3(-0.5, 1.0, 0.0), Vec3(0.0, -1.0, 0.0),
      Vec3(1.0, -1.0, 0.0),
  };
  mesh.faces = {
      {{0, 1, 2}},
      {{0, 3, 4}},
      {{0, 5, 6}},
  };

  const FeatureAnalysis features =
      feature::detectFeatureCurves(mesh, discreteOnlyOptions());

  EXPECT_GT(features.loops.size(), 1u);
  EXPECT_EQ(features.featureEdges, static_cast<int>(features.graph.edges.size()));
  EXPECT_EQ(mesh.vertices.size(), features.graph.vertices.size());
  EXPECT_FALSE(features.graph.junctionVertices.empty());
  ASSERT_LT(0u, features.vertices.size());
  EXPECT_TRUE(features.vertices[0].junction);
  EXPECT_TRUE(features.graph.vertices[0].junction);
}
