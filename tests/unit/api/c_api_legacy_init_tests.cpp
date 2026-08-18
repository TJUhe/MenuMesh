/**
 * @file tests/unit/api/c_api_legacy_init_tests.cpp
 * @brief 验证旧版 C ABI 初始化函数只写入首个发布布局。
 * @ingroup manumesh_tests
 */

#define MANUMESH_DISABLE_SIZE_AWARE_ALIASES
#include "api/CApi.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>

namespace {
constexpr unsigned char kInitializerSentinel = 0xA5;

struct LegacyV1FeatureOptionsLayout {
    std::size_t struct_size;
    unsigned int abi_version;
    double feature_angle_deg;
    double loop_trace_angle_deg;
    double circle_fit_relative_threshold;
    double ellipse_fit_relative_threshold;
    double near_circle_axis_ratio_tolerance;
    int min_feature_loop_vertices;
    int use_normal_tensor_features;
    double normal_tensor_feature_threshold;
    double normal_tensor_min_edge_alignment;
    int normal_tensor_smoothing_iterations;
    int normal_tensor_scale_count;
    int normal_tensor_min_persistent_scales;
    unsigned char reserved_feature_evidence[60];
    int cleanup_feature_graph;
    double feature_graph_gap_length_ratio;
    int feature_graph_max_weak_spur_edges;
    double feature_graph_min_weak_spur_strength;
    double feature_component_min_confidence;
    int normal_filter_enabled;
    int normal_filter_iterations;
    double normal_filter_angle_sigma_deg;
    double normal_filter_preserve_angle_deg;
    double normal_filter_relaxation;
    int graph_consolidation_enabled;
    double graph_consolidation_gap_length_ratio;
    double graph_consolidation_min_alignment;
};

struct LegacyV1SimplifyOptionsLayout {
    std::size_t struct_size;
    unsigned int abi_version;
    int target_faces;
    double target_ratio;
    int use_line_quadrics;
    double line_weight;
    ManuMeshWeightMode weight_mode;
    double feature_boost;
    double feature_angle_deg;
    int adaptive_scale;
    double adaptive_base_line_weight;
    double boundary_weight;
    int preserve_boundary;
    int preserve_feature_curves;
    double feature_curve_weight;
    double max_feature_curve_deviation_ratio;
    double circle_fit_relative_threshold;
    double ellipse_fit_relative_threshold;
    double near_circle_axis_ratio_tolerance;
    int min_feature_loop_vertices;
    int min_circular_feature_loop_vertices;
    int use_normal_tensor_features;
    double normal_tensor_feature_threshold;
    double normal_tensor_min_edge_alignment;
    int normal_tensor_smoothing_iterations;
    int normal_tensor_scale_count;
    double min_triangle_quality;
    double max_normal_deviation_deg;
    double max_local_error;
    double max_local_error_ratio;
    int prevent_local_intersections;
    int verbose;
    ManuMeshFeatureProtectionMode feature_protection_mode;
};

struct LegacyV1SimplifyReportLayout {
    std::size_t struct_size;
    unsigned int abi_version;
    int initial_vertices;
    int initial_faces;
    int final_vertices;
    int final_faces;
    int collapsed_edges;
    int rejected_collapses;
    int solver_fallbacks;
    int queue_rebuilds;
    int feature_loops;
    int circular_feature_loops;
    int feature_vertices;
    int normal_tensor_feature_edges;
    int feature_rejected_collapses;
    int primitive_feature_rejected_collapses;
    int generic_feature_rejected_collapses;
    int boundary_rejected_collapses;
    int topology_rejected_collapses;
    int normal_flip_rejected_collapses;
    int quality_rejected_collapses;
    int self_intersection_rejected_collapses;
    int curve_budget_rejected_collapses;
    int error_rejected_collapses;
    int projected_feature_placements;
    ManuMeshSimplifyTerminationReason termination_reason;
    double min_applied_line_weight;
    double max_applied_line_weight;
};

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
    offsetof(LegacyV1FeatureOptionsLayout, graph_consolidation_min_alignment) ==
        offsetof(ManuMeshFeatureOptions, graph_consolidation_min_alignment),
    "ManuMeshFeatureOptions v1 final field offset changed"
);
static_assert(
    sizeof(LegacyV1FeatureOptionsLayout) ==
        offsetof(ManuMeshFeatureOptions, graph_consolidation_min_alignment) + sizeof(double),
    "ManuMeshFeatureOptions v1 prefix size changed"
);
static_assert(
    sizeof(LegacyV1SimplifyOptionsLayout) == offsetof(ManuMeshSimplifyOptions, loop_trace_angle_deg),
    "ManuMeshSimplifyOptions v1 prefix size changed"
);
static_assert(
    sizeof(LegacyV1SimplifyReportLayout) == offsetof(ManuMeshSimplifyReport, traced_feature_edges),
    "ManuMeshSimplifyReport v1 prefix size changed"
);

constexpr std::size_t kLegacyV1FeatureOptionsSize = sizeof(LegacyV1FeatureOptionsLayout);
constexpr std::size_t kLegacyV1SimplifyOptionsSize = sizeof(LegacyV1SimplifyOptionsLayout);
constexpr std::size_t kLegacyV1SimplifyReportSize = sizeof(LegacyV1SimplifyReportLayout);
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

template <typename T> struct GuardedLegacyStorage {
    T object;
    std::array<unsigned char, 16> guard{};

    void fill(unsigned char byte) {
        std::memset(&object, byte, sizeof(object));
        guard.fill(byte);
    }
};

template <typename T> void expectSentinelFrom(const GuardedAbiStorage<T>& storage, std::size_t offset) {
    const auto* objectBytes = reinterpret_cast<const unsigned char*>(&storage.object);
    for (std::size_t i = offset; i < sizeof(T); ++i) {
        EXPECT_EQ(kInitializerSentinel, objectBytes[i]) << "object byte " << i << " was overwritten";
    }
    for (std::size_t i = 0; i < storage.guard.size(); ++i) {
        EXPECT_EQ(kInitializerSentinel, storage.guard[i]) << "guard byte " << i << " was overwritten";
    }
}

} // namespace

template <typename T> void expectLegacyInitializerIsBounded(void (*initializer)(T*), std::size_t expectedWriteSize) {
    GuardedAbiStorage<T> storage;
    storage.fill(kInitializerSentinel);

    initializer(storage.value());

    EXPECT_EQ(expectedWriteSize, storage.value()->struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, storage.value()->abi_version);
    expectSentinelFrom(storage, expectedWriteSize);
}

template <typename T>
void expectLegacyInitializerMatchesSizeAwarePrefix(
    void (*legacyInitializer)(T*), ManuMeshStatus (*sizeAwareInitializer)(T*, std::size_t), std::size_t legacySize
) {
    GuardedAbiStorage<T> legacyStorage;
    GuardedAbiStorage<T> sizeAwareStorage;
    legacyStorage.fill(kInitializerSentinel);
    sizeAwareStorage.fill(kInitializerSentinel);

    legacyInitializer(legacyStorage.value());
    ASSERT_EQ(MANUMESH_STATUS_OK, sizeAwareInitializer(sizeAwareStorage.value(), legacySize));

    EXPECT_EQ(0, std::memcmp(&legacyStorage.object, &sizeAwareStorage.object, legacySize));
}

template <typename Legacy, typename Public>
void expectLegacyInitializerAcceptsActualFirstPublishedStorage(void (*initializer)(Public*)) {
    GuardedLegacyStorage<Legacy> storage;
    storage.fill(kInitializerSentinel);

    initializer(reinterpret_cast<Public*>(&storage.object));

    EXPECT_EQ(sizeof(Legacy), storage.object.struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, storage.object.abi_version);
    for (std::size_t i = 0; i < storage.guard.size(); ++i) {
        EXPECT_EQ(kInitializerSentinel, storage.guard[i]) << "guard byte " << i << " was overwritten";
    }
}
TEST(CApiLegacyInit, SymbolsStayWithinFirstPublishedV1Layouts) {
    expectLegacyInitializerIsBounded(&manumesh_feature_options_init, kLegacyV1FeatureOptionsSize);
    expectLegacyInitializerIsBounded(&manumesh_simplify_options_init, kLegacyV1SimplifyOptionsSize);
    expectLegacyInitializerIsBounded(&manumesh_simplify_report_init, kLegacyV1SimplifyReportSize);
    expectLegacyInitializerIsBounded(&manumesh_mesh_stats_init, kLegacyV1MeshStatsSize);

    expectLegacyInitializerMatchesSizeAwarePrefix(
        &manumesh_feature_options_init, &manumesh_feature_options_init_with_size, kLegacyV1FeatureOptionsSize
    );
    expectLegacyInitializerMatchesSizeAwarePrefix(
        &manumesh_simplify_options_init, &manumesh_simplify_options_init_with_size, kLegacyV1SimplifyOptionsSize
    );
    expectLegacyInitializerMatchesSizeAwarePrefix(
        &manumesh_simplify_report_init, &manumesh_simplify_report_init_with_size, kLegacyV1SimplifyReportSize
    );
    expectLegacyInitializerMatchesSizeAwarePrefix(
        &manumesh_mesh_stats_init, &manumesh_mesh_stats_init_with_size, kLegacyV1MeshStatsSize
    );

    expectLegacyInitializerAcceptsActualFirstPublishedStorage<LegacyV1FeatureOptionsLayout>(
        &manumesh_feature_options_init
    );
    expectLegacyInitializerAcceptsActualFirstPublishedStorage<LegacyV1SimplifyOptionsLayout>(
        &manumesh_simplify_options_init
    );

    ManuMeshContext* context = manumesh_context_create();
    ASSERT_NE(nullptr, context);
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    ASSERT_NE(nullptr, input);
    ASSERT_NE(nullptr, output);
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

    GuardedLegacyStorage<LegacyV1SimplifyOptionsLayout> optionsStorage;
    optionsStorage.fill(kInitializerSentinel);
    auto* options = reinterpret_cast<ManuMeshSimplifyOptions*>(&optionsStorage.object);
    manumesh_simplify_options_init(options);
    optionsStorage.object.target_ratio = 0.75;

    GuardedAbiStorage<ManuMeshSimplifyReport> reportStorage;
    reportStorage.fill(kInitializerSentinel);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_simplify_mesh(context, input, options, output, reportStorage.value()));
    EXPECT_EQ(kLegacyV1SimplifyReportSize, reportStorage.value()->struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, reportStorage.value()->abi_version);
    EXPECT_GT(reportStorage.value()->initial_faces, reportStorage.value()->final_faces);
    expectSentinelFrom(reportStorage, kLegacyV1SimplifyReportSize);
    for (std::size_t i = 0; i < optionsStorage.guard.size(); ++i) {
        EXPECT_EQ(kInitializerSentinel, optionsStorage.guard[i]) << "options guard byte " << i << " was overwritten";
    }

    GuardedAbiStorage<ManuMeshMeshStats> statsStorage;
    statsStorage.fill(kInitializerSentinel);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_compute_mesh_stats(context, output, statsStorage.value()));
    EXPECT_EQ(kLegacyV1MeshStatsSize, statsStorage.value()->struct_size);
    EXPECT_EQ(MANUMESH_ABI_VERSION, statsStorage.value()->abi_version);
    EXPECT_GT(statsStorage.value()->vertices, 0);
    expectSentinelFrom(statsStorage, kLegacyV1MeshStatsSize);

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
    manumesh_context_destroy(context);
}
