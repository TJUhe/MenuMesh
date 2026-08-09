/**
 * @file tests/unit/api/c_api_simplify_abi_tests.cpp
 * @brief 验证 ManuMesh 测试中的 C API 简化 ABI 测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#include "CApiTestSupport.h"

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

} // 命名空间

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
    ManuMeshSimplifyOptions options;
    ::manumesh_simplify_options_init(&options);
    EXPECT_EQ(sizeof(options), options.struct_size);

    ManuMeshSimplifyReport report;
    ::manumesh_simplify_report_init(&report);
    EXPECT_EQ(sizeof(report), report.struct_size);

    ManuMeshMeshStats stats;
    ::manumesh_mesh_stats_init(&stats);
    EXPECT_EQ(sizeof(stats), stats.struct_size);

    void (*optionsInitializer)(ManuMeshSimplifyOptions*) = &manumesh_simplify_options_init;
    ManuMeshSimplifyOptions indirectOptions;
    optionsInitializer(&indirectOptions);
    EXPECT_EQ(sizeof(indirectOptions), indirectOptions.struct_size);
}

TEST_F(CApiTest, SizeAwareInitializersRespectMinimumLegacyCurrentAndOversizedCapacities) {
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
