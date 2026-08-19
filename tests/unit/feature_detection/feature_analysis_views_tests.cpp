/**
 * @file tests/unit/feature_detection/feature_analysis_views_tests.cpp
 * @brief Verifies the non-owning FeatureAnalysis read-only view contracts.
 * @ingroup manumesh_tests
 */

#include "algorithms/feature_detection/FeatureAnalysisViews.h"
#include "algorithms/feature_detection/FeatureDetector.h"

#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

namespace {
namespace feature = manumesh::feature;

using Analysis = feature::FeatureAnalysis;
using Curves = feature::FeatureCurveView;
using Diagnostics = feature::FeatureDiagnosticsView;
using Evidence = feature::FeatureEvidenceView;
using Segmentation = feature::FeatureSegmentationView;

template <typename T> class CanViewFeatureEvidence {
    template <typename U>
    static auto test(int) -> decltype(feature::viewFeatureEvidence(std::declval<U>()), std::true_type{});
    template <typename> static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T> class CanViewFeatureCurves {
    template <typename U>
    static auto test(int) -> decltype(feature::viewFeatureCurves(std::declval<U>()), std::true_type{});
    template <typename> static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T> class CanViewFeatureSegmentation {
    template <typename U>
    static auto test(int) -> decltype(feature::viewFeatureSegmentation(std::declval<U>()), std::true_type{});
    template <typename> static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T> class CanViewFeatureDiagnostics {
    template <typename U>
    static auto test(int) -> decltype(feature::viewFeatureDiagnostics(std::declval<U>()), std::true_type{});
    template <typename> static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

static_assert(
    std::is_same<
        decltype(std::declval<const Evidence&>().graphEdges()),
        const std::vector<feature::FeatureGraphEdge>&>::value,
    "Feature evidence must remain read-only"
);
static_assert(
    std::is_same<decltype(std::declval<const Curves&>().loops()), const std::vector<feature::FeatureLoop>&>::value,
    "Feature curves must remain read-only"
);
static_assert(
    std::is_same<decltype(std::declval<const Segmentation&>().patches()), const std::vector<feature::FeaturePatch>&>::
        value,
    "Feature segmentation must remain read-only"
);
static_assert(
    std::is_same<
        decltype(std::declval<const Diagnostics&>().normalFilter()),
        const feature::FeatureNormalFilterReport&>::value,
    "Feature diagnostics must remain read-only"
);
static_assert(std::is_constructible<Evidence, Analysis&>::value, "Evidence views must accept analysis lvalues");
static_assert(std::is_constructible<Curves, const Analysis&>::value, "Curve views must accept const analysis lvalues");
static_assert(std::is_constructible<Segmentation, Analysis&>::value, "Segmentation views must accept analysis lvalues");
static_assert(
    std::is_constructible<Diagnostics, const Analysis&>::value, "Diagnostics views must accept const analysis lvalues"
);
static_assert(!std::is_constructible<Evidence, Analysis&&>::value, "Evidence views must reject analysis temporaries");
static_assert(
    !std::is_constructible<Curves, const Analysis&&>::value, "Curve views must reject const analysis temporaries"
);
static_assert(
    !std::is_constructible<Segmentation, Analysis&&>::value, "Segmentation views must reject analysis temporaries"
);
static_assert(
    !std::is_constructible<Diagnostics, const Analysis&&>::value,
    "Diagnostics views must reject const analysis temporaries"
);
static_assert(CanViewFeatureEvidence<Analysis&>::value, "Evidence helpers must accept analysis lvalues");
static_assert(CanViewFeatureCurves<const Analysis&>::value, "Curve helpers must accept const analysis lvalues");
static_assert(CanViewFeatureSegmentation<Analysis&>::value, "Segmentation helpers must accept analysis lvalues");
static_assert(
    CanViewFeatureDiagnostics<const Analysis&>::value, "Diagnostics helpers must accept const analysis lvalues"
);
static_assert(!CanViewFeatureEvidence<Analysis&&>::value, "Evidence helpers must reject analysis temporaries");
static_assert(!CanViewFeatureCurves<const Analysis&&>::value, "Curve helpers must reject const analysis temporaries");
static_assert(!CanViewFeatureSegmentation<Analysis&&>::value, "Segmentation helpers must reject analysis temporaries");
static_assert(
    !CanViewFeatureDiagnostics<const Analysis&&>::value, "Diagnostics helpers must reject const analysis temporaries"
);

TEST(FeatureAnalysisViews, AliasOriginalStorageWithoutCopyingResults) {
    Analysis analysis;
    analysis.vertices.resize(2);
    analysis.loops.resize(1);
    analysis.components.resize(1);
    analysis.graph.edges.resize(1);
    analysis.graph.vertices.resize(2);
    analysis.normalTensorVertexWeights = {0.25, 0.75};
    analysis.smoothCurvatureVertexWeights = {0.125, 0.875};
    analysis.facePatchIds = {0, 1};
    analysis.patches.resize(2);
    analysis.patchAdjacencies.resize(1);

    const Evidence evidence = feature::viewFeatureEvidence(analysis);
    const Curves curves = feature::viewFeatureCurves(analysis);
    const Segmentation segmentation = feature::viewFeatureSegmentation(analysis);

    EXPECT_EQ(&analysis.graph.edges, &evidence.graphEdges());
    EXPECT_EQ(&analysis.normalTensorVertexWeights, &evidence.normalTensorVertexWeights());
    EXPECT_EQ(&analysis.smoothCurvatureVertexWeights, &evidence.smoothCurvatureVertexWeights());
    EXPECT_EQ(&analysis.vertices, &curves.vertices());
    EXPECT_EQ(&analysis.loops, &curves.loops());
    EXPECT_EQ(&analysis.components, &curves.components());
    EXPECT_EQ(&analysis.graph, &curves.graph());
    EXPECT_EQ(&analysis.facePatchIds, &segmentation.facePatchIds());
    EXPECT_EQ(&analysis.patches, &segmentation.patches());
    EXPECT_EQ(&analysis.patchAdjacencies, &segmentation.patchAdjacencies());
}

TEST(FeatureAnalysisViews, KeepEvidenceAndDiagnosticsContractsSeparate) {
    Analysis analysis;
    analysis.featureEdges = 9;
    analysis.boundaryFeatureEdges = 2;
    analysis.dihedralFeatureEdges = 3;
    analysis.normalTensorFeatureEdges = 4;
    analysis.tracedFeatureEdges = 7;
    analysis.graphCleanupBridgedGaps = 1;
    analysis.meanFeatureComponentConfidence = 0.625;
    analysis.normalFilter.iterationsCompleted = 3;
    analysis.closedSurfacePatches = 2;
    analysis.segmentationIgnoredRecoveryEdges = 5;
    analysis.source.geometryFingerprint = 42;

    const Evidence evidence = feature::viewFeatureEvidence(analysis);
    const Diagnostics diagnostics = feature::viewFeatureDiagnostics(analysis);
    const Segmentation segmentation = feature::viewFeatureSegmentation(analysis);

    EXPECT_EQ(9, evidence.featureEdgeCount());
    EXPECT_EQ(2, evidence.boundaryFeatureEdgeCount());
    EXPECT_EQ(3, evidence.dihedralFeatureEdgeCount());
    EXPECT_EQ(4, evidence.normalTensorFeatureEdgeCount());
    EXPECT_EQ(7, diagnostics.tracedFeatureEdgeCount());
    EXPECT_EQ(1, diagnostics.graphCleanupBridgedGapCount());
    EXPECT_DOUBLE_EQ(0.625, diagnostics.meanFeatureComponentConfidence());
    EXPECT_EQ(3, diagnostics.normalFilter().iterationsCompleted);
    EXPECT_EQ(42u, diagnostics.source().geometryFingerprint);
    EXPECT_EQ(2, segmentation.closedSurfacePatchCount());
    EXPECT_EQ(5, segmentation.ignoredRecoveryEdgeCount());

    analysis.featureEdges = 10;
    analysis.meanFeatureComponentConfidence = 0.75;
    EXPECT_EQ(10, evidence.featureEdgeCount());
    EXPECT_DOUBLE_EQ(0.75, diagnostics.meanFeatureComponentConfidence());
}

} // namespace
