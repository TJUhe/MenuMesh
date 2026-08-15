/**
 * @file tests/unit/api/c_api_simplify_abi_tests.cpp
 * @brief 验证 ManuMesh 测试中的 C API 简化 ABI 测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#include "CApiTestSupport.h"
#include "api/detail/CApiConverters.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>

using manumesh::test::dataRoot;
namespace {

constexpr unsigned char kInitializerSentinel = 0xA5;

struct LegacyV1MeshStatsLayout {
    std::size_t struct_size;
    unsigned int abi_version;
    int vertices;
    int faces;
    int edges;
    int boundary_edges;
    int non_manifold_edges;
    double area;
    double mean_triangle_quality;
    double min_triangle_quality;
    double mean_edge_length;
    double edge_length_cv;
};

static_assert(
    offsetof(LegacyV1MeshStatsLayout, edge_length_cv) == offsetof(ManuMeshMeshStats, edge_length_cv),
    "ManuMeshMeshStats v1 field layout changed"
);

constexpr std::size_t kLegacyV1FeatureOptionsSize =
    offsetof(ManuMeshFeatureOptions, graph_consolidation_min_alignment) + sizeof(double);
constexpr std::size_t kLegacyV1SimplifyOptionsSize = offsetof(ManuMeshSimplifyOptions, loop_trace_angle_deg);
constexpr std::size_t kLegacyV1SimplifyReportSize = offsetof(ManuMeshSimplifyReport, traced_feature_edges);
constexpr std::size_t kLegacyV1MeshStatsSize = sizeof(LegacyV1MeshStatsLayout);

template <typename T> struct GuardedAbiStorage {
    T object;
    std::array<unsigned char, 16> guard{};

    T* value() { return &object; }

    void fill(unsigned char byte) {
        std::memset(&object, byte, sizeof(object));
        guard.fill(byte);
    }
};

template <typename T> constexpr std::size_t minimumAbiStructSize() {
    return offsetof(T, abi_version) + sizeof(unsigned int);
}

} // namespace

// Keep this list in ManuMeshFeatureOptions order. It drives exhaustive mapping,
// prefix-size, and composed-override checks for every public payload field.
#define MANUMESH_C_ABI_FEATURE_FIELDS(BOOL_FIELD, INT_FIELD, DOUBLE_FIELD)                                             \
    DOUBLE_FIELD(feature_angle_deg, featureAngleDeg, feature_angle_deg, featureAngleDeg, 41.5)                         \
    DOUBLE_FIELD(loop_trace_angle_deg, loopTraceAngleDeg, loop_trace_angle_deg, loopTraceAngleDeg, 39.5)               \
    DOUBLE_FIELD(                                                                                                      \
        circle_fit_relative_threshold,                                                                                 \
        circleFitRelativeThreshold,                                                                                    \
        circle_fit_relative_threshold,                                                                                 \
        circleFitRelativeThreshold,                                                                                    \
        0.041                                                                                                          \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        ellipse_fit_relative_threshold,                                                                                \
        ellipseFitRelativeThreshold,                                                                                   \
        ellipse_fit_relative_threshold,                                                                                \
        ellipseFitRelativeThreshold,                                                                                   \
        0.052                                                                                                          \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        near_circle_axis_ratio_tolerance,                                                                              \
        nearCircleAxisRatioTolerance,                                                                                  \
        near_circle_axis_ratio_tolerance,                                                                              \
        nearCircleAxisRatioTolerance,                                                                                  \
        0.071                                                                                                          \
    )                                                                                                                  \
    INT_FIELD(                                                                                                         \
        min_feature_loop_vertices, minFeatureLoopVertices, min_feature_loop_vertices, minFeatureLoopVertices, 11       \
    )                                                                                                                  \
    BOOL_FIELD(                                                                                                        \
        use_normal_tensor_features, useNormalTensorFeatures, use_normal_tensor_features, useNormalTensorFeatures, 0    \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        normal_tensor_feature_threshold,                                                                               \
        normalTensorFeatureThreshold,                                                                                  \
        normal_tensor_feature_threshold,                                                                               \
        normalTensorFeatureThreshold,                                                                                  \
        0.21                                                                                                           \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        normal_tensor_min_edge_alignment,                                                                              \
        normalTensorMinEdgeAlignment,                                                                                  \
        normal_tensor_min_edge_alignment,                                                                              \
        normalTensorMinEdgeAlignment,                                                                                  \
        0.52                                                                                                           \
    )                                                                                                                  \
    INT_FIELD(                                                                                                         \
        normal_tensor_smoothing_iterations,                                                                            \
        normalTensorSmoothingIterations,                                                                               \
        normal_tensor_smoothing_iterations,                                                                            \
        normalTensorSmoothingIterations,                                                                               \
        2                                                                                                              \
    )                                                                                                                  \
    INT_FIELD(normal_tensor_scale_count, normalTensorScaleCount, normal_tensor_scale_count, normalTensorScaleCount, 4) \
    INT_FIELD(                                                                                                         \
        normal_tensor_min_persistent_scales,                                                                           \
        normalTensorMinPersistentScales,                                                                               \
        normal_tensor_min_persistent_scales,                                                                           \
        normalTensorMinPersistentScales,                                                                               \
        3                                                                                                              \
    )                                                                                                                  \
    BOOL_FIELD(                                                                                                        \
        use_smooth_curvature_features,                                                                                 \
        useSmoothCurvatureFeatures,                                                                                    \
        use_smooth_curvature_features,                                                                                 \
        useSmoothCurvatureFeatures,                                                                                    \
        1                                                                                                              \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        smooth_curvature_feature_threshold,                                                                            \
        smoothCurvatureFeatureThreshold,                                                                               \
        smooth_curvature_feature_threshold,                                                                            \
        smoothCurvatureFeatureThreshold,                                                                               \
        0.021                                                                                                          \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        smooth_curvature_min_edge_alignment,                                                                           \
        smoothCurvatureMinEdgeAlignment,                                                                               \
        smooth_curvature_min_edge_alignment,                                                                           \
        smoothCurvatureMinEdgeAlignment,                                                                               \
        0.61                                                                                                           \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        smooth_curvature_min_tangent_consistency,                                                                      \
        smoothCurvatureMinTangentConsistency,                                                                          \
        smooth_curvature_min_tangent_consistency,                                                                      \
        smoothCurvatureMinTangentConsistency,                                                                          \
        0.72                                                                                                           \
    )                                                                                                                  \
    INT_FIELD(                                                                                                         \
        smooth_curvature_base_neighborhood_rings,                                                                      \
        smoothCurvatureBaseNeighborhoodRings,                                                                          \
        smooth_curvature_base_neighborhood_rings,                                                                      \
        smoothCurvatureBaseNeighborhoodRings,                                                                          \
        3                                                                                                              \
    )                                                                                                                  \
    INT_FIELD(                                                                                                         \
        smooth_curvature_scale_count,                                                                                  \
        smoothCurvatureScaleCount,                                                                                     \
        smooth_curvature_scale_count,                                                                                  \
        smoothCurvatureScaleCount,                                                                                     \
        5                                                                                                              \
    )                                                                                                                  \
    INT_FIELD(                                                                                                         \
        smooth_curvature_min_persistent_scales,                                                                        \
        smoothCurvatureMinPersistentScales,                                                                            \
        smooth_curvature_min_persistent_scales,                                                                        \
        smoothCurvatureMinPersistentScales,                                                                            \
        4                                                                                                              \
    )                                                                                                                  \
    INT_FIELD(                                                                                                         \
        smooth_curvature_robust_fit_iterations,                                                                        \
        smoothCurvatureRobustFitIterations,                                                                            \
        smooth_curvature_robust_fit_iterations,                                                                        \
        smoothCurvatureRobustFitIterations,                                                                            \
        3                                                                                                              \
    )                                                                                                                  \
    BOOL_FIELD(                                                                                                        \
        smooth_curvature_use_stable_scale_selection,                                                                   \
        smoothCurvatureUseStableScaleSelection,                                                                        \
        smooth_curvature_use_stable_scale_selection,                                                                   \
        smoothCurvatureUseStableScaleSelection,                                                                        \
        1                                                                                                              \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        smooth_curvature_min_scale_stability,                                                                          \
        smoothCurvatureMinScaleStability,                                                                              \
        smooth_curvature_min_scale_stability,                                                                          \
        smoothCurvatureMinScaleStability,                                                                              \
        0.44                                                                                                           \
    )                                                                                                                  \
    BOOL_FIELD(cleanup_feature_graph, cleanupFeatureGraph, cleanup_feature_graph, cleanupFeatureGraph, 0)              \
    DOUBLE_FIELD(                                                                                                      \
        feature_graph_gap_length_ratio,                                                                                \
        featureGraphGapLengthRatio,                                                                                    \
        feature_graph_gap_length_ratio,                                                                                \
        featureGraphGapLengthRatio,                                                                                    \
        1.75                                                                                                           \
    )                                                                                                                  \
    INT_FIELD(                                                                                                         \
        feature_graph_max_weak_spur_edges,                                                                             \
        featureGraphMaxWeakSpurEdges,                                                                                  \
        feature_graph_max_weak_spur_edges,                                                                             \
        featureGraphMaxWeakSpurEdges,                                                                                  \
        5                                                                                                              \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        feature_graph_min_weak_spur_strength,                                                                          \
        featureGraphMinWeakSpurStrength,                                                                               \
        feature_graph_min_weak_spur_strength,                                                                          \
        featureGraphMinWeakSpurStrength,                                                                               \
        0.12                                                                                                           \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        feature_component_min_confidence,                                                                              \
        featureComponentMinConfidence,                                                                                 \
        feature_component_min_confidence,                                                                              \
        featureComponentMinConfidence,                                                                                 \
        0.63                                                                                                           \
    )                                                                                                                  \
    BOOL_FIELD(normal_filter_enabled, normalFilter.enabled, use_feature_normal_filter, useFeatureNormalFilter, 1)      \
    INT_FIELD(                                                                                                         \
        normal_filter_iterations,                                                                                      \
        normalFilter.iterations,                                                                                       \
        feature_normal_filter_iterations,                                                                              \
        featureNormalFilterIterations,                                                                                 \
        6                                                                                                              \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        normal_filter_angle_sigma_deg,                                                                                 \
        normalFilter.angleSigmaDeg,                                                                                    \
        feature_normal_filter_angle_sigma_deg,                                                                         \
        featureNormalFilterAngleSigmaDeg,                                                                              \
        17.5                                                                                                           \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        normal_filter_preserve_angle_deg,                                                                              \
        normalFilter.preserveAngleDeg,                                                                                 \
        feature_normal_filter_preserve_angle_deg,                                                                      \
        featureNormalFilterPreserveAngleDeg,                                                                           \
        48.5                                                                                                           \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        normal_filter_relaxation,                                                                                      \
        normalFilter.relaxation,                                                                                       \
        feature_normal_filter_relaxation,                                                                              \
        featureNormalFilterRelaxation,                                                                                 \
        0.65                                                                                                           \
    )                                                                                                                  \
    BOOL_FIELD(                                                                                                        \
        graph_consolidation_enabled, graphConsolidation.enabled, consolidate_feature_graph, consolidateFeatureGraph, 1 \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        graph_consolidation_gap_length_ratio,                                                                          \
        graphConsolidation.maxGapLengthRatio,                                                                          \
        feature_graph_consolidation_gap_length_ratio,                                                                  \
        featureGraphConsolidationGapLengthRatio,                                                                       \
        2.5                                                                                                            \
    )                                                                                                                  \
    DOUBLE_FIELD(                                                                                                      \
        graph_consolidation_min_alignment,                                                                             \
        graphConsolidation.minAlignment,                                                                               \
        feature_graph_consolidation_min_alignment,                                                                     \
        featureGraphConsolidationMinAlignment,                                                                         \
        0.82                                                                                                           \
    )

#define MANUMESH_COUNT_FEATURE_FIELD(...) +1
constexpr std::size_t kFeaturePayloadFieldCount = 0 MANUMESH_C_ABI_FEATURE_FIELDS(
    MANUMESH_COUNT_FEATURE_FIELD, MANUMESH_COUNT_FEATURE_FIELD, MANUMESH_COUNT_FEATURE_FIELD
);
#undef MANUMESH_COUNT_FEATURE_FIELD
static_assert(kFeaturePayloadFieldCount == 35, "Update the exhaustive C ABI feature field audit.");
static_assert(
    offsetof(ManuMeshFeatureOptions, graph_consolidation_min_alignment) +
            sizeof(((ManuMeshFeatureOptions*)nullptr)->graph_consolidation_min_alignment) ==
        sizeof(ManuMeshFeatureOptions),
    "ManuMeshFeatureOptions gained payload fields; extend the exhaustive C ABI feature field audit."
);

ManuMeshFeatureOptions makeDistinctFeatureOptions() {
    ManuMeshFeatureOptions options;
    manumesh_feature_options_init(&options);
#define MANUMESH_SET_FEATURE_FIELD(cField, featureMember, legacyField, legacyMember, value) options.cField = value;
    MANUMESH_C_ABI_FEATURE_FIELDS(MANUMESH_SET_FEATURE_FIELD, MANUMESH_SET_FEATURE_FIELD, MANUMESH_SET_FEATURE_FIELD)
#undef MANUMESH_SET_FEATURE_FIELD
    return options;
}

bool featureFieldPresent(std::size_t structSize, std::size_t fieldOffset, std::size_t fieldSize) {
    return fieldOffset <= std::numeric_limits<std::size_t>::max() - fieldSize && structSize >= fieldOffset + fieldSize;
}

std::vector<std::size_t> featureOptionPrefixSizes(const ManuMeshFeatureOptions& options) {
    std::vector<std::size_t> sizes{minimumAbiStructSize<ManuMeshFeatureOptions>()};
    sizes.reserve(kFeaturePayloadFieldCount + 2);
#define MANUMESH_ADD_FEATURE_PREFIX(cField, featureMember, legacyField, legacyMember, value)                           \
    sizes.push_back(offsetof(ManuMeshFeatureOptions, cField) + sizeof(options.cField));
    MANUMESH_C_ABI_FEATURE_FIELDS(MANUMESH_ADD_FEATURE_PREFIX, MANUMESH_ADD_FEATURE_PREFIX, MANUMESH_ADD_FEATURE_PREFIX)
#undef MANUMESH_ADD_FEATURE_PREFIX
    sizes.push_back(sizeof(ManuMeshFeatureOptions) + 16);
    return sizes;
}

void expectFeatureOptionsForCapacity(
    const manumesh::feature::FeatureOptions& actual, const ManuMeshFeatureOptions& source, std::size_t structSize
) {
    const manumesh::feature::FeatureOptions defaults;
#define MANUMESH_EXPECT_FEATURE_BOOL(cField, featureMember, legacyField, legacyMember, value)                          \
    do {                                                                                                               \
        const bool expected =                                                                                          \
            featureFieldPresent(structSize, offsetof(ManuMeshFeatureOptions, cField), sizeof(source.cField))           \
                ? (value != 0)                                                                                         \
                : defaults.featureMember;                                                                              \
        EXPECT_EQ(expected, actual.featureMember) << #cField << " at struct_size=" << structSize;                      \
    } while (false);
#define MANUMESH_EXPECT_FEATURE_INT(cField, featureMember, legacyField, legacyMember, value)                           \
    do {                                                                                                               \
        const int expected =                                                                                           \
            featureFieldPresent(structSize, offsetof(ManuMeshFeatureOptions, cField), sizeof(source.cField))           \
                ? value                                                                                                \
                : defaults.featureMember;                                                                              \
        EXPECT_EQ(expected, actual.featureMember) << #cField << " at struct_size=" << structSize;                      \
    } while (false);
#define MANUMESH_EXPECT_FEATURE_DOUBLE(cField, featureMember, legacyField, legacyMember, value)                        \
    do {                                                                                                               \
        const double expected =                                                                                        \
            featureFieldPresent(structSize, offsetof(ManuMeshFeatureOptions, cField), sizeof(source.cField))           \
                ? value                                                                                                \
                : defaults.featureMember;                                                                              \
        EXPECT_DOUBLE_EQ(expected, actual.featureMember) << #cField << " at struct_size=" << structSize;               \
    } while (false);
    MANUMESH_C_ABI_FEATURE_FIELDS(
        MANUMESH_EXPECT_FEATURE_BOOL, MANUMESH_EXPECT_FEATURE_INT, MANUMESH_EXPECT_FEATURE_DOUBLE
    )
#undef MANUMESH_EXPECT_FEATURE_BOOL
#undef MANUMESH_EXPECT_FEATURE_INT
#undef MANUMESH_EXPECT_FEATURE_DOUBLE
    EXPECT_EQ(defaults.surfacePatches.enabled, actual.surfacePatches.enabled);
    EXPECT_EQ(defaults.surfacePatches.includeWeakEvidence, actual.surfacePatches.includeWeakEvidence);
}

void poisonLegacyFeatureFields(ManuMeshSimplifyOptions& options) {
#define MANUMESH_POISON_FEATURE_BOOL(cField, featureMember, legacyField, legacyMember, value)                          \
    options.legacyField = value;
#define MANUMESH_POISON_FEATURE_INT(cField, featureMember, legacyField, legacyMember, value) options.legacyField = -777;
#define MANUMESH_POISON_FEATURE_DOUBLE(cField, featureMember, legacyField, legacyMember, value)                        \
    options.legacyField = std::numeric_limits<double>::quiet_NaN();
    MANUMESH_C_ABI_FEATURE_FIELDS(
        MANUMESH_POISON_FEATURE_BOOL, MANUMESH_POISON_FEATURE_INT, MANUMESH_POISON_FEATURE_DOUBLE
    )
#undef MANUMESH_POISON_FEATURE_BOOL
#undef MANUMESH_POISON_FEATURE_INT
#undef MANUMESH_POISON_FEATURE_DOUBLE
}

void expectLegacyFeatureFieldsRemainDefault(const manumesh::simplification::SimplifyOptions& actual) {
    const manumesh::simplification::SimplifyOptions defaults{};
#define MANUMESH_EXPECT_LEGACY_BOOL(cField, featureMember, legacyField, legacyMember, value)                           \
    EXPECT_EQ(defaults.legacyMember, actual.legacyMember) << #legacyField;
#define MANUMESH_EXPECT_LEGACY_INT(cField, featureMember, legacyField, legacyMember, value)                            \
    EXPECT_EQ(defaults.legacyMember, actual.legacyMember) << #legacyField;
#define MANUMESH_EXPECT_LEGACY_DOUBLE(cField, featureMember, legacyField, legacyMember, value)                         \
    EXPECT_DOUBLE_EQ(defaults.legacyMember, actual.legacyMember) << #legacyField;
    MANUMESH_C_ABI_FEATURE_FIELDS(
        MANUMESH_EXPECT_LEGACY_BOOL, MANUMESH_EXPECT_LEGACY_INT, MANUMESH_EXPECT_LEGACY_DOUBLE
    )
#undef MANUMESH_EXPECT_LEGACY_BOOL
#undef MANUMESH_EXPECT_LEGACY_INT
#undef MANUMESH_EXPECT_LEGACY_DOUBLE
}

#undef MANUMESH_C_ABI_FEATURE_FIELDS

template <typename T> void expectSentinelFrom(const GuardedAbiStorage<T>& storage, std::size_t offset) {
    const auto* objectBytes = reinterpret_cast<const unsigned char*>(&storage.object);
    for (std::size_t i = offset; i < sizeof(T); ++i) {
        EXPECT_EQ(kInitializerSentinel, objectBytes[i]) << "object byte " << i << " was overwritten";
    }
    for (std::size_t i = 0; i < storage.guard.size(); ++i) {
        EXPECT_EQ(kInitializerSentinel, storage.guard[i]) << "guard byte " << i << " was overwritten";
    }
}

template <typename T> void expectAllSentinel(const GuardedAbiStorage<T>& storage) { expectSentinelFrom(storage, 0); }

template <typename T>
void expectSizeAwareInitializerIsBounded(
    ManuMeshStatus (*initializer)(T*, std::size_t), std::size_t capacity, std::size_t expectedWriteSize
) {
    GuardedAbiStorage<T> storage;
    storage.fill(kInitializerSentinel);
    ASSERT_LE(capacity, sizeof(T) + storage.guard.size());

    EXPECT_EQ(MANUMESH_STATUS_OK, initializer(storage.value(), capacity));
    EXPECT_EQ(expectedWriteSize, storage.value()->struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, storage.value()->abi_version);
    expectSentinelFrom(storage, expectedWriteSize);
}

template <typename T>
void expectSizeAwareInitializerRejectsInvalidCapacity(ManuMeshStatus (*initializer)(T*, std::size_t)) {
    GuardedAbiStorage<T> storage;
    storage.fill(kInitializerSentinel);

    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, initializer(storage.value(), minimumAbiStructSize<T>() - 1));
    expectAllSentinel(storage);
    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, initializer(nullptr, sizeof(T)));
}
TEST(CApiFeatureOptionsConverter, MapsEveryPayloadFieldAcrossEverySizeAwarePrefix) {
    const ManuMeshFeatureOptions distinct = makeDistinctFeatureOptions();
    const std::vector<std::size_t> prefixSizes = featureOptionPrefixSizes(distinct);
    ASSERT_EQ(kFeaturePayloadFieldCount + 2, prefixSizes.size());
    for (std::size_t index = 1; index < prefixSizes.size(); ++index) {
        ASSERT_GT(prefixSizes[index], prefixSizes[index - 1]);
    }

    for (const std::size_t structSize : prefixSizes) {
        ManuMeshFeatureOptions source = distinct;
        source.struct_size = structSize;
        manumesh::feature::FeatureOptions actual;
        std::string error;

        ASSERT_TRUE(manumesh::api::readFeatureOptions(&source, actual, error))
            << "struct_size=" << structSize << ": " << error;
        expectFeatureOptionsForCapacity(actual, source, structSize);
    }
}

TEST(CApiFeatureOptionsConverter, SuccessfulReadClearsStaleErrorText) {
    manumesh::feature::FeatureOptions actual;
    std::string error = "stale error";

    ASSERT_TRUE(manumesh::api::readFeatureOptions(nullptr, actual, error));
    EXPECT_TRUE(error.empty());
}

TEST(CApiFeatureOptionsConverter, ComposedOverrideOwnsEveryFlatFieldAcrossEverySizeAwarePrefix) {
    const ManuMeshFeatureOptions distinct = makeDistinctFeatureOptions();
    const std::vector<std::size_t> prefixSizes = featureOptionPrefixSizes(distinct);

    for (const std::size_t structSize : prefixSizes) {
        ManuMeshFeatureOptions featureOptions = distinct;
        featureOptions.struct_size = structSize;

        ManuMeshSimplifyOptions source;
        manumesh_simplify_options_init(&source);
        poisonLegacyFeatureFields(source);
        source.feature_options = &featureOptions;

        manumesh::simplification::SimplifyOptions actual;
        std::string error;
        ASSERT_TRUE(manumesh::api::readSimplifyOptions(source, actual, error))
            << "struct_size=" << structSize << ": " << error;
        ASSERT_TRUE(actual.featureOptionsOverride.has_value()) << "struct_size=" << structSize;
        expectFeatureOptionsForCapacity(*actual.featureOptionsOverride, featureOptions, structSize);
        expectLegacyFeatureFieldsRemainDefault(actual);
    }
}

TEST(CApiFeatureOptionsConverter, CopiesBorrowedOverrideAndClearsItWhenTheTargetIsReused) {
    ManuMeshFeatureOptions featureOptions = makeDistinctFeatureOptions();
    const double copiedFeatureAngle = featureOptions.feature_angle_deg;

    ManuMeshSimplifyOptions source;
    manumesh_simplify_options_init(&source);
    source.feature_options = &featureOptions;

    manumesh::simplification::SimplifyOptions actual;
    std::string error = "stale error";
    ASSERT_TRUE(manumesh::api::readSimplifyOptions(source, actual, error)) << error;
    ASSERT_TRUE(actual.featureOptionsOverride.has_value());
    EXPECT_DOUBLE_EQ(copiedFeatureAngle, actual.featureOptionsOverride->featureAngleDeg);
    EXPECT_TRUE(error.empty());

    featureOptions.feature_angle_deg = copiedFeatureAngle + 10.0;
    EXPECT_DOUBLE_EQ(copiedFeatureAngle, actual.featureOptionsOverride->featureAngleDeg);

    ManuMeshSimplifyOptions sourceWithoutOverride;
    manumesh_simplify_options_init(&sourceWithoutOverride);
    sourceWithoutOverride.target_ratio = 0.6;
    actual.preserveTexture = true;
    ASSERT_TRUE(manumesh::api::readSimplifyOptions(sourceWithoutOverride, actual, error)) << error;
    EXPECT_FALSE(actual.featureOptionsOverride.has_value());
    EXPECT_FALSE(actual.preserveTexture);
    EXPECT_DOUBLE_EQ(0.6, actual.targetRatio);
}

TEST_F(CApiTest, MapsInvalidSimplifyOptionsToInvalidArgumentStatus) {
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    options.target_ratio = 0.0;

    ManuMeshSimplifyReport report;
    manumesh_simplify_report_init(&report);
    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_simplify_mesh(context, input, &options, output, &report));
    EXPECT_NE('\0', manumesh_context_last_error(context)[0]);

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, RejectsUninitializedSimplifyOptionsAbiStruct) {
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

    ManuMeshSimplifyOptions options{};
    options.target_ratio = 0.5;

    ManuMeshSimplifyReport report;
    manumesh_simplify_report_init(&report);
    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_simplify_mesh(context, input, &options, output, &report));
    EXPECT_NE('\0', manumesh_context_last_error(context)[0]);

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, AcceptsOlderTrailingSimplifyOptionsAbiStruct) {
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    options.target_ratio = 0.75;
    options.feature_protection_mode = static_cast<ManuMeshFeatureProtectionMode>(999);
    options.struct_size = offsetof(ManuMeshSimplifyOptions, feature_protection_mode);

    ManuMeshSimplifyReport report;
    manumesh_simplify_report_init(&report);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_simplify_mesh(context, input, &options, output, &report));
    EXPECT_GT(report.initial_faces, report.final_faces);
    EXPECT_EQ(MANUMESH_SIMPLIFY_TERMINATION_REACHED_TARGET, report.termination_reason);

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, ComposedFeatureOptionsOverrideLegacyFields) {
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

    ManuMeshFeatureOptions featureOptions;
    manumesh_feature_options_init(&featureOptions);
    featureOptions.feature_angle_deg = 45.0;

    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    options.target_ratio = 0.75;
    options.feature_angle_deg = std::numeric_limits<double>::quiet_NaN();
    options.normal_tensor_feature_threshold = std::numeric_limits<double>::quiet_NaN();
    options.smooth_curvature_feature_threshold = std::numeric_limits<double>::quiet_NaN();
    options.feature_normal_filter_relaxation = std::numeric_limits<double>::quiet_NaN();
    options.feature_graph_consolidation_gap_length_ratio = std::numeric_limits<double>::quiet_NaN();
    options.feature_options = &featureOptions;

    ManuMeshSimplifyReport report;
    manumesh_simplify_report_init(&report);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_simplify_mesh(context, input, &options, output, &report));

    featureOptions.feature_angle_deg = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_simplify_mesh(context, input, &options, output, &report));

    ManuMeshFeatureOptions uninitialized{};
    options.feature_options = &uninitialized;
    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_simplify_mesh(context, input, &options, output, &report));

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, ComposedOlderFeatureOptionsUseDefaultsForAbsentTailFields) {
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

    ManuMeshFeatureOptions featureOptions;
    manumesh_feature_options_init(&featureOptions);
    featureOptions.feature_angle_deg = 45.0;
    featureOptions.normal_tensor_feature_threshold = std::numeric_limits<double>::quiet_NaN();
    featureOptions.struct_size = offsetof(ManuMeshFeatureOptions, use_normal_tensor_features);

    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    options.target_ratio = 0.75;
    options.feature_options = &featureOptions;

    ManuMeshSimplifyReport report;
    manumesh_simplify_report_init(&report);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_simplify_mesh(context, input, &options, output, &report));

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, OlderSimplifyOptionsSizeIgnoresComposedFeaturePointer) {
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

    ManuMeshFeatureOptions uninitialized{};
    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    options.target_ratio = 0.75;
    options.feature_options = &uninitialized;
    options.struct_size = offsetof(ManuMeshSimplifyOptions, feature_options);

    ManuMeshSimplifyReport report;
    manumesh_simplify_report_init(&report);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_simplify_mesh(context, input, &options, output, &report));

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, IgnoresAbsentQualityRefinementTailFieldInOlderOptionsStruct) {
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    options.target_ratio = 0.75;
    options.quality_refinement_iterations = -1;
    options.struct_size = offsetof(ManuMeshSimplifyOptions, quality_refinement_iterations);

    ManuMeshSimplifyReport report;
    manumesh_simplify_report_init(&report);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_simplify_mesh(context, input, &options, output, &report));
    EXPECT_EQ(0, report.quality_refinement_iterations_completed);
    EXPECT_EQ(0, report.quality_refinement_attempted_moves);
    EXPECT_EQ(0, report.quality_refinement_accepted_moves);

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, IgnoresAbsentSmoothCurvatureTailFieldsInOlderOptionsStruct) {
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "bump", 20, input));

    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    options.target_ratio = 0.90;
    options.preserve_feature_curves = 1;
    options.use_smooth_curvature_features = 1;
    options.smooth_curvature_feature_threshold = -1.0;
    options.feature_graph_min_weak_spur_strength = -1.0;
    options.struct_size = offsetof(ManuMeshSimplifyOptions, use_smooth_curvature_features);

    ManuMeshSimplifyReport report;
    manumesh_simplify_report_init(&report);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_simplify_mesh(context, input, &options, output, &report));
    EXPECT_EQ(0, report.smooth_curvature_feature_edges);
    EXPECT_EQ(0, report.smooth_curvature_scored_vertices);

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, MapsQualityRefinementTailOptionAndReportFields) {
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    options.target_ratio = 1.0;
    options.preserve_boundary = 1;
    options.quality_refinement_iterations = 2;

    ManuMeshSimplifyReport report;
    manumesh_simplify_report_init(&report);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_simplify_mesh(context, input, &options, output, &report));
    EXPECT_GE(report.quality_refinement_iterations_completed, 1);
    EXPECT_LE(report.quality_refinement_iterations_completed, 2);
    EXPECT_GE(report.quality_refinement_attempted_moves, report.quality_refinement_accepted_moves);
    EXPECT_EQ(0, report.quality_refinement_skipped_for_texture);

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, SourceCompatibleSimplifyInitializesUninitializedCurrentReport) {
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    options.target_ratio = 0.75;

    ManuMeshSimplifyReport report;
    std::memset(&report, kInitializerSentinel, sizeof(report));
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_simplify_mesh(context, input, &options, output, &report));
    EXPECT_EQ(sizeof(report), report.struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, report.abi_version);
    EXPECT_GT(report.initial_faces, report.final_faces);

    EXPECT_EQ(
        MANUMESH_STATUS_OK, manumesh_simplify_mesh_with_report_size(context, input, &options, output, nullptr, 0)
    );

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, DoesNotWritePastCallerSizedSimplifyReport) {
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    options.target_ratio = 0.75;

    constexpr std::size_t kOlderReportSize = offsetof(ManuMeshSimplifyReport, traced_feature_edges);
    GuardedAbiStorage<ManuMeshSimplifyReport> storage;
    storage.fill(kInitializerSentinel);
    ManuMeshSimplifyReport* report = storage.value();

    EXPECT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_simplify_mesh_with_report_size(context, input, &options, output, report, kOlderReportSize)
    );
    EXPECT_EQ(kOlderReportSize, report->struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, report->abi_version);
    EXPECT_GT(report->initial_faces, report->final_faces);
    EXPECT_EQ(MANUMESH_SIMPLIFY_TERMINATION_REACHED_TARGET, report->termination_reason);
    expectSentinelFrom(storage, kOlderReportSize);

    GuardedAbiStorage<ManuMeshSimplifyReport> tooSmallStorage;
    tooSmallStorage.fill(kInitializerSentinel);
    const std::size_t tooSmallCapacity = minimumAbiStructSize<ManuMeshSimplifyReport>() - 1;
    EXPECT_EQ(
        MANUMESH_STATUS_INVALID_ARGUMENT,
        manumesh_simplify_mesh_with_report_size(
            context, input, &options, output, tooSmallStorage.value(), tooSmallCapacity
        )
    );
    expectAllSentinel(tooSmallStorage);

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, DoesNotWritePastCallerSizedMeshStats) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "cube", 4, mesh));

    constexpr std::size_t kOlderStatsSize = offsetof(ManuMeshMeshStats, mean_triangle_quality);
    GuardedAbiStorage<ManuMeshMeshStats> storage;
    storage.fill(kInitializerSentinel);
    ManuMeshMeshStats* stats = storage.value();

    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_compute_mesh_stats_with_size(context, mesh, stats, kOlderStatsSize));
    EXPECT_EQ(kOlderStatsSize, stats->struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, stats->abi_version);
    EXPECT_GT(stats->vertices, 0);
    EXPECT_GT(stats->faces, 0);
    expectSentinelFrom(storage, kOlderStatsSize);

    GuardedAbiStorage<ManuMeshMeshStats> tooSmallStorage;
    tooSmallStorage.fill(kInitializerSentinel);
    const std::size_t tooSmallCapacity = minimumAbiStructSize<ManuMeshMeshStats>() - 1;
    EXPECT_EQ(
        MANUMESH_STATUS_INVALID_ARGUMENT,
        manumesh_compute_mesh_stats_with_size(context, mesh, tooSmallStorage.value(), tooSmallCapacity)
    );
    expectAllSentinel(tooSmallStorage);
    EXPECT_EQ(
        MANUMESH_STATUS_INVALID_ARGUMENT,
        manumesh_compute_mesh_stats_with_size(context, mesh, nullptr, sizeof(ManuMeshMeshStats))
    );

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, SourceCompatibleMeshStatsInitializesUninitializedCurrentOutput) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "cube", 4, mesh));

    ManuMeshMeshStats stats;
    std::memset(&stats, kInitializerSentinel, sizeof(stats));
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_compute_mesh_stats(context, mesh, &stats));
    EXPECT_EQ(sizeof(stats), stats.struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, stats.abi_version);
    EXPECT_GT(stats.vertices, 0);
    EXPECT_GT(stats.faces, 0);

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, SourceCompatibilityInitializersSupportGlobalQualification) {
    ManuMeshFeatureOptions featureOptions;
    ::manumesh_feature_options_init(&featureOptions);
    EXPECT_EQ(sizeof(featureOptions), featureOptions.struct_size);

    ManuMeshSimplifyOptions options;
    ::manumesh_simplify_options_init(&options);
    EXPECT_EQ(sizeof(options), options.struct_size);

    ManuMeshSimplifyReport report;
    ::manumesh_simplify_report_init(&report);
    EXPECT_EQ(sizeof(report), report.struct_size);

    ManuMeshMeshStats stats;
    ::manumesh_mesh_stats_init(&stats);
    EXPECT_EQ(sizeof(stats), stats.struct_size);

    void (*featureOptionsInitializer)(ManuMeshFeatureOptions*) = &manumesh_feature_options_init;
    ManuMeshFeatureOptions indirectFeatureOptions;
    featureOptionsInitializer(&indirectFeatureOptions);
    EXPECT_EQ(sizeof(indirectFeatureOptions), indirectFeatureOptions.struct_size);

    void (*optionsInitializer)(ManuMeshSimplifyOptions*) = &manumesh_simplify_options_init;
    ManuMeshSimplifyOptions indirectOptions;
    optionsInitializer(&indirectOptions);
    EXPECT_EQ(sizeof(indirectOptions), indirectOptions.struct_size);
}

TEST_F(CApiTest, SizeAwareInitializersRespectMinimumLegacyCurrentAndOversizedCapacities) {
    expectSizeAwareInitializerIsBounded(
        &manumesh_feature_options_init_with_size,
        minimumAbiStructSize<ManuMeshFeatureOptions>(),
        minimumAbiStructSize<ManuMeshFeatureOptions>()
    );
    expectSizeAwareInitializerIsBounded(
        &manumesh_feature_options_init_with_size, kLegacyV1FeatureOptionsSize, kLegacyV1FeatureOptionsSize
    );
    expectSizeAwareInitializerIsBounded(
        &manumesh_feature_options_init_with_size, sizeof(ManuMeshFeatureOptions), sizeof(ManuMeshFeatureOptions)
    );
    expectSizeAwareInitializerIsBounded(
        &manumesh_feature_options_init_with_size, sizeof(ManuMeshFeatureOptions) + 16, sizeof(ManuMeshFeatureOptions)
    );

    expectSizeAwareInitializerIsBounded(
        &manumesh_simplify_options_init_with_size,
        minimumAbiStructSize<ManuMeshSimplifyOptions>(),
        minimumAbiStructSize<ManuMeshSimplifyOptions>()
    );
    expectSizeAwareInitializerIsBounded(
        &manumesh_simplify_options_init_with_size, kLegacyV1SimplifyOptionsSize, kLegacyV1SimplifyOptionsSize
    );
    expectSizeAwareInitializerIsBounded(
        &manumesh_simplify_options_init_with_size, sizeof(ManuMeshSimplifyOptions), sizeof(ManuMeshSimplifyOptions)
    );
    expectSizeAwareInitializerIsBounded(
        &manumesh_simplify_options_init_with_size, sizeof(ManuMeshSimplifyOptions) + 16, sizeof(ManuMeshSimplifyOptions)
    );

    expectSizeAwareInitializerIsBounded(
        &manumesh_simplify_report_init_with_size,
        minimumAbiStructSize<ManuMeshSimplifyReport>(),
        minimumAbiStructSize<ManuMeshSimplifyReport>()
    );
    expectSizeAwareInitializerIsBounded(
        &manumesh_simplify_report_init_with_size, kLegacyV1SimplifyReportSize, kLegacyV1SimplifyReportSize
    );
    expectSizeAwareInitializerIsBounded(
        &manumesh_simplify_report_init_with_size, sizeof(ManuMeshSimplifyReport), sizeof(ManuMeshSimplifyReport)
    );
    expectSizeAwareInitializerIsBounded(
        &manumesh_simplify_report_init_with_size, sizeof(ManuMeshSimplifyReport) + 16, sizeof(ManuMeshSimplifyReport)
    );

    expectSizeAwareInitializerIsBounded(
        &manumesh_mesh_stats_init_with_size,
        minimumAbiStructSize<ManuMeshMeshStats>(),
        minimumAbiStructSize<ManuMeshMeshStats>()
    );
    expectSizeAwareInitializerIsBounded(
        &manumesh_mesh_stats_init_with_size, kLegacyV1MeshStatsSize, kLegacyV1MeshStatsSize
    );
    expectSizeAwareInitializerIsBounded(
        &manumesh_mesh_stats_init_with_size, sizeof(ManuMeshMeshStats) + 16, sizeof(ManuMeshMeshStats)
    );
}

TEST_F(CApiTest, SizeAwareInitializersRejectTooSmallAndNullBuffersWithoutWriting) {
    expectSizeAwareInitializerRejectsInvalidCapacity(&manumesh_feature_options_init_with_size);
    expectSizeAwareInitializerRejectsInvalidCapacity(&manumesh_simplify_options_init_with_size);
    expectSizeAwareInitializerRejectsInvalidCapacity(&manumesh_simplify_report_init_with_size);
    expectSizeAwareInitializerRejectsInvalidCapacity(&manumesh_mesh_stats_init_with_size);
}

TEST_F(CApiTest, InitializesPrimitiveFitOptions) {
    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);

    EXPECT_EQ(sizeof(ManuMeshSimplifyOptions), options.struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, options.abi_version);
    EXPECT_DOUBLE_EQ(0.05, options.circle_fit_relative_threshold);
    EXPECT_DOUBLE_EQ(0.05, options.ellipse_fit_relative_threshold);
    EXPECT_DOUBLE_EQ(0.08, options.near_circle_axis_ratio_tolerance);
    EXPECT_DOUBLE_EQ(-1.0, options.loop_trace_angle_deg);
    EXPECT_DOUBLE_EQ(0.0, options.max_feature_curve_deviation_ratio);
    EXPECT_EQ(6, options.min_circular_feature_loop_vertices);
    EXPECT_EQ(nullptr, options.feature_options);
    EXPECT_EQ(0, options.preserve_boundary);
    EXPECT_DOUBLE_EQ(0.0, options.min_triangle_quality);
    EXPECT_DOUBLE_EQ(90.0, options.max_normal_deviation_deg);
    EXPECT_EQ(1, options.normal_tensor_scale_count);
    EXPECT_EQ(1, options.normal_tensor_min_persistent_scales);
    EXPECT_EQ(1, options.cleanup_feature_graph);
    EXPECT_DOUBLE_EQ(1.25, options.feature_graph_gap_length_ratio);
    EXPECT_EQ(2, options.feature_graph_max_weak_spur_edges);
    EXPECT_DOUBLE_EQ(0.35, options.feature_component_min_confidence);
    EXPECT_EQ(0, options.quality_refinement_iterations);
    EXPECT_EQ(0, options.use_smooth_curvature_features);
    EXPECT_DOUBLE_EQ(0.015, options.smooth_curvature_feature_threshold);
    EXPECT_DOUBLE_EQ(0.55, options.smooth_curvature_min_edge_alignment);
    EXPECT_DOUBLE_EQ(0.65, options.smooth_curvature_min_tangent_consistency);
    EXPECT_EQ(2, options.smooth_curvature_base_neighborhood_rings);
    EXPECT_EQ(3, options.smooth_curvature_scale_count);
    EXPECT_EQ(2, options.smooth_curvature_min_persistent_scales);
    EXPECT_EQ(2, options.smooth_curvature_robust_fit_iterations);
    EXPECT_DOUBLE_EQ(0.0, options.feature_graph_min_weak_spur_strength);
    EXPECT_EQ(0, options.use_feature_normal_filter);
    EXPECT_EQ(4, options.feature_normal_filter_iterations);
    EXPECT_DOUBLE_EQ(20.0, options.feature_normal_filter_angle_sigma_deg);
    EXPECT_DOUBLE_EQ(50.0, options.feature_normal_filter_preserve_angle_deg);
    EXPECT_DOUBLE_EQ(0.8, options.feature_normal_filter_relaxation);
    EXPECT_EQ(0, options.smooth_curvature_use_stable_scale_selection);
    EXPECT_DOUBLE_EQ(0.0, options.smooth_curvature_min_scale_stability);
    EXPECT_EQ(0, options.consolidate_feature_graph);
    EXPECT_DOUBLE_EQ(3.0, options.feature_graph_consolidation_gap_length_ratio);
    EXPECT_DOUBLE_EQ(0.75, options.feature_graph_consolidation_min_alignment);
    EXPECT_DOUBLE_EQ(0.0, options.max_local_error);
    EXPECT_DOUBLE_EQ(0.0, options.max_local_error_ratio);
    EXPECT_EQ(0, options.prevent_local_intersections);
    EXPECT_EQ(MANUMESH_FEATURE_PROTECTION_PRIMITIVE_CURVES, options.feature_protection_mode);

    ManuMeshSimplifyReport report;
    manumesh_simplify_report_init(&report);
    EXPECT_EQ(sizeof(ManuMeshSimplifyReport), report.struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, report.abi_version);

    ManuMeshMeshStats stats;
    manumesh_mesh_stats_init(&stats);
    EXPECT_EQ(sizeof(ManuMeshMeshStats), stats.struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, stats.abi_version);
}
