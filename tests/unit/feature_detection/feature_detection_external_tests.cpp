/**
 * @file tests/unit/feature_detection/feature_detection_external_tests.cpp
 * @brief 验证 ManuMesh 测试中的特征检测 外部测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

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

} // 命名空间

TEST(FeatureDetection, LargeExternalThingi10kMeshProducesNontrivialFeatureGraph) {
    const Mesh mesh = loadExternalStl("thingi10k/thingi10k_105382_measuring_cup_with_handle_and_spout.stl");
    ASSERT_FALSE(mesh.empty());
    ASSERT_GT(mesh.faces.size(), 10000u);

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 25.0;
    options.circleFitRelativeThreshold = 0.08;
    options.minFeatureLoopVertices = 8;
    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

    const int featureVertices = static_cast<int>(
        std::count_if(features.vertices.begin(), features.vertices.end(), [](const VertexFeature& vertex) {
            return vertex.isFeature;
        })
    );
    const int cleanupBridgeEdges = static_cast<int>(std::count_if(
        features.graph.edges.begin(), features.graph.edges.end(), [](const feature::FeatureGraphEdge& edge) {
            return edge.cleanupBridge;
        }
    ));

    EXPECT_EQ(mesh.vertices.size(), features.vertices.size());
    EXPECT_EQ(features.featureEdges + cleanupBridgeEdges, static_cast<int>(features.graph.edges.size()));
    EXPECT_GT(featureVertices, 100);
    EXPECT_GT(features.featureEdges, 100);
    EXPECT_GT(features.loops.size(), 5u);
    EXPECT_GT(countClosedLoops(features), 0);
    EXPECT_GT(features.boundaryFeatureEdges + features.dihedralFeatureEdges + features.nonManifoldFeatureEdges, 100);
    EXPECT_GT(features.graph.junctionVertices.size(), 0u);
}
