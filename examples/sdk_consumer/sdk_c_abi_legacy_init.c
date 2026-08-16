/**
 * @file examples/sdk_consumer/sdk_c_abi_legacy_init.c
 * @brief 验证旧版 C ABI 结构初始化仍可由已安装 SDK 使用。
 * @ingroup manumesh_examples
 *
 * @details 示例只使用已安装 SDK 的旧版 C ABI 初始化入口，作为兼容性集成文档。
 */

#define MANUMESH_DISABLE_SIZE_AWARE_ALIASES
#include "api/CApi.h"

#include <stddef.h>
#include <string.h>

typedef struct LegacyV1MeshStatsLayout {
    size_t struct_size;
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
} LegacyV1MeshStatsLayout;

static const size_t kLegacyV1FeatureOptionsSize =
    offsetof(ManuMeshFeatureOptions, graph_consolidation_min_alignment) +
    sizeof(((ManuMeshFeatureOptions*)0)->graph_consolidation_min_alignment);

int main(void) {
    ManuMeshFeatureOptions feature_options;
    manumesh_feature_options_init(&feature_options);
    if (feature_options.struct_size != kLegacyV1FeatureOptionsSize) {
        return 8;
    }

    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    if (options.struct_size != offsetof(ManuMeshSimplifyOptions, loop_trace_angle_deg)) {
        return 1;
    }

    ManuMeshSimplifyReport initialized_report;
    manumesh_simplify_report_init(&initialized_report);
    if (initialized_report.struct_size != offsetof(ManuMeshSimplifyReport, traced_feature_edges)) {
        return 2;
    }

    ManuMeshMeshStats initialized_stats;
    manumesh_mesh_stats_init(&initialized_stats);
    if (initialized_stats.struct_size != sizeof(LegacyV1MeshStatsLayout)) {
        return 3;
    }

    ManuMeshContext* context = manumesh_context_create();
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    if (!context || !input || !output) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 4;
    }
    if (manumesh_generate_mesh(context, "plane", 8, input) != MANUMESH_STATUS_OK) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 5;
    }

    options.target_ratio = 0.75;
    ManuMeshSimplifyReport report;
    memset(&report, 0xA5, sizeof(report));
    if (manumesh_simplify_mesh(context, input, &options, output, &report) != MANUMESH_STATUS_OK ||
        report.struct_size != offsetof(ManuMeshSimplifyReport, traced_feature_edges)) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 6;
    }

    ManuMeshMeshStats stats;
    memset(&stats, 0xA5, sizeof(stats));
    if (manumesh_compute_mesh_stats(context, output, &stats) != MANUMESH_STATUS_OK ||
        stats.struct_size != sizeof(LegacyV1MeshStatsLayout) || stats.faces <= 0) {
        manumesh_mesh_destroy(output);
        manumesh_mesh_destroy(input);
        manumesh_context_destroy(context);
        return 7;
    }

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
    manumesh_context_destroy(context);
    return 0;
}
