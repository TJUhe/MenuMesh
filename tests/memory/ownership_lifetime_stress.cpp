/**
 * @file tests/memory/ownership_lifetime_stress.cpp
 * @brief Exercises public ownership boundaries while counting outstanding C++ allocations.
 */

#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "api/CApi.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace {

std::atomic<long long> outstandingAllocations{0};
std::atomic<unsigned long long> totalAllocations{0};
std::atomic<bool> rejectAllocations{false};

class AllocationFailureScope {
public:
    AllocationFailureScope() noexcept { rejectAllocations.store(true, std::memory_order_relaxed); }
    ~AllocationFailureScope() { rejectAllocations.store(false, std::memory_order_relaxed); }

    AllocationFailureScope(const AllocationFailureScope&) = delete;
    AllocationFailureScope& operator=(const AllocationFailureScope&) = delete;
};

struct ContextDeleter {
    void operator()(ManuMeshContext* context) const noexcept { manumesh_context_destroy(context); }
};

struct MeshDeleter {
    void operator()(ManuMeshMeshHandle* mesh) const noexcept { manumesh_mesh_destroy(mesh); }
};

using ContextPtr = std::unique_ptr<ManuMeshContext, ContextDeleter>;
using MeshPtr = std::unique_ptr<ManuMeshMeshHandle, MeshDeleter>;

int runCApiCycle() {
    ContextPtr context(manumesh_context_create());
    if (!context) {
        return 10;
    }

    MeshPtr input(manumesh_mesh_create(context.get()));
    MeshPtr output(manumesh_mesh_create(context.get()));
    if (!input || !output) {
        return 11;
    }
    if (manumesh_generate_mesh(context.get(), "cylinder", 16, input.get()) != MANUMESH_STATUS_OK) {
        return 12;
    }

    ManuMeshFeatureOptions featureOptions;
    manumesh_feature_options_init(&featureOptions);
    featureOptions.use_normal_tensor_features = 0;
    featureOptions.cleanup_feature_graph = 0;
    featureOptions.graph_consolidation_enabled = 0;

    std::size_t requiredEdges = 0;
    ManuMeshStatus status =
        manumesh_detect_feature_edges(context.get(), input.get(), &featureOptions, nullptr, 0, &requiredEdges);
    if (status != MANUMESH_STATUS_BUFFER_TOO_SMALL || requiredEdges == 0) {
        return 13;
    }

    std::vector<ManuMeshFeatureEdge> edges(requiredEdges);
    std::size_t writtenEdges = 0;
    if (requiredEdges > 1) {
        status = manumesh_detect_feature_edges(
            context.get(), input.get(), &featureOptions, edges.data(), requiredEdges - 1, &writtenEdges
        );
        if (status != MANUMESH_STATUS_BUFFER_TOO_SMALL || writtenEdges != requiredEdges) {
            return 14;
        }
    }
    status = manumesh_detect_feature_edges(
        context.get(), input.get(), &featureOptions, edges.data(), edges.size(), &writtenEdges
    );
    if (status != MANUMESH_STATUS_OK || writtenEdges != requiredEdges) {
        return 15;
    }
    std::size_t requiredEdgesV2 = 0;
    status =
        manumesh_detect_feature_edges_v2(context.get(), input.get(), &featureOptions, nullptr, 0, &requiredEdgesV2);
    if (status != MANUMESH_STATUS_BUFFER_TOO_SMALL || requiredEdgesV2 != requiredEdges) {
        return 16;
    }
    std::vector<ManuMeshFeatureEdgeV2> edgesV2(requiredEdgesV2);
    std::size_t writtenEdgesV2 = 0;
    if (requiredEdgesV2 > 1) {
        status = manumesh_detect_feature_edges_v2(
            context.get(), input.get(), &featureOptions, edgesV2.data(), requiredEdgesV2 - 1, &writtenEdgesV2
        );
        if (status != MANUMESH_STATUS_BUFFER_TOO_SMALL || writtenEdgesV2 != requiredEdgesV2) {
            return 17;
        }
    }
    status = manumesh_detect_feature_edges_v2(
        context.get(), input.get(), &featureOptions, edgesV2.data(), edgesV2.size(), &writtenEdgesV2
    );
    if (status != MANUMESH_STATUS_OK || writtenEdgesV2 != requiredEdgesV2) {
        return 18;
    }
    for (std::size_t edgeIndex = 0; edgeIndex < writtenEdgesV2; ++edgeIndex) {
        const ManuMeshFeatureEdgeV2& edge = edgesV2[edgeIndex];
        if (edge.feature_edge_index != edgeIndex ||
            (edge.geometric_constraint != 0) != (edge.input_edge_index != MANUMESH_INVALID_EDGE_INDEX)) {
            return 23;
        }
    }

    ManuMeshSimplifyOptions simplifyOptions;
    ManuMeshSimplifyReport report;
    manumesh_simplify_options_init(&simplifyOptions);
    manumesh_simplify_report_init(&report);
    simplifyOptions.target_ratio = 0.80;
    simplifyOptions.preserve_feature_curves = 1;
    simplifyOptions.feature_options = &featureOptions;

    ManuMeshSimplifyOptions invalidOptions = simplifyOptions;
    invalidOptions.target_ratio = -1.0;
    if (manumesh_simplify_mesh(context.get(), input.get(), &invalidOptions, output.get(), &report) !=
        MANUMESH_STATUS_INVALID_ARGUMENT) {
        return 19;
    }

    if (manumesh_simplify_mesh(context.get(), input.get(), &simplifyOptions, output.get(), &report) !=
        MANUMESH_STATUS_OK) {
        return 20;
    }

    std::size_t outputVertices = 0;
    std::size_t outputFaces = 0;
    if (manumesh_mesh_get_counts(context.get(), output.get(), &outputVertices, &outputFaces) != MANUMESH_STATUS_OK ||
        outputVertices == 0 || outputFaces == 0) {
        return 21;
    }
    if (manumesh_mesh_clear(context.get(), output.get()) != MANUMESH_STATUS_OK ||
        manumesh_mesh_clear(context.get(), input.get()) != MANUMESH_STATUS_OK) {
        return 22;
    }
    return 0;
}

int runCppOwnershipCycle() {
    const manumesh::Mesh mesh = manumesh::generateCylinderGrid(16, 4, 0.65, 1.7);

    manumesh::feature::FeatureOptions featureOptions;
    featureOptions.useNormalTensorFeatures = false;
    featureOptions.cleanupFeatureGraph = false;
    featureOptions.graphConsolidation.enabled = false;

    manumesh::feature::FeatureDetector detector(featureOptions);
    manumesh::feature::FeatureDetector detectorCopy(detector);
    manumesh::feature::FeatureDetector detectorMoved(std::move(detectorCopy));
    detectorCopy.setOptions(featureOptions);
    detectorCopy = detector;
    detectorMoved = std::move(detectorCopy);
    const manumesh::feature::FeatureAnalysis analysis = detectorMoved.analyze(mesh);

    manumesh::simplification::SimplifyOptions simplifyOptions;
    simplifyOptions.targetRatio = 0.80;
    simplifyOptions.preserveFeatureCurves = true;
    simplifyOptions.featureOptionsOverride = featureOptions;

    manumesh::simplification::QEMSimplifier simplifier(simplifyOptions);
    manumesh::simplification::QEMSimplifier simplifierCopy(simplifier);
    manumesh::simplification::QEMSimplifier simplifierMoved(std::move(simplifierCopy));
    simplifierCopy.setOptions(simplifyOptions);
    simplifierCopy = simplifier;
    simplifierMoved = std::move(simplifierCopy);
    const manumesh::Mesh simplified = simplifierMoved.simplify(mesh, analysis);
    if (simplified.vertices.empty() || simplified.faces.empty()) {
        return 23;
    }

    const manumesh::Result<manumesh::MeshTopology> topologyResult = manumesh::MeshTopology::build(mesh);
    if (!topologyResult.ok()) {
        return 24;
    }
    manumesh::MeshTopology topologyCopy(topologyResult.value());
    manumesh::MeshTopology topologyMoved(std::move(topologyCopy));
    topologyCopy = topologyResult.value();
    topologyMoved = std::move(topologyCopy);
    if (topologyMoved.vertexCount() != static_cast<int>(mesh.vertices.size()) || topologyMoved.edgeCount() == 0) {
        return 25;
    }
    return 0;
}

int runCApiAllocationFailureBoundary() {
    ContextPtr context(manumesh_context_create());
    if (!context) {
        return 40;
    }
    MeshPtr mesh(manumesh_mesh_create(context.get()));
    if (!mesh) {
        return 41;
    }

    ManuMeshStatus status = MANUMESH_STATUS_OK;
    bool exceptionEscaped = false;
    {
        AllocationFailureScope reject;
        try {
            status = manumesh_mesh_clear(context.get(), nullptr);
        } catch (...) {
            exceptionEscaped = true;
        }
    }
    if (exceptionEscaped || status != MANUMESH_STATUS_INVALID_ARGUMENT) {
        return 42;
    }
    if (manumesh_context_last_error(context.get())[0] != '\0') {
        return 43;
    }

    ManuMeshFeatureOptions invalidOptions{};
    std::size_t written = 0;
    exceptionEscaped = false;
    {
        AllocationFailureScope reject;
        try {
            status = manumesh_detect_feature_edges(context.get(), mesh.get(), &invalidOptions, nullptr, 0, &written);
        } catch (...) {
            exceptionEscaped = true;
        }
    }
    if (exceptionEscaped || status != MANUMESH_STATUS_OUT_OF_MEMORY || written != 0) {
        return 44;
    }
    return 0;
}

int runOwnershipCycle() {
    const int cApiResult = runCApiCycle();
    return cApiResult == 0 ? runCppOwnershipCycle() : cApiResult;
}

} // namespace

void* operator new(std::size_t size) {
    if (rejectAllocations.load(std::memory_order_relaxed)) {
        throw std::bad_alloc();
    }
    void* memory = std::malloc(size == 0 ? 1 : size);
    if (!memory) {
        throw std::bad_alloc();
    }
    outstandingAllocations.fetch_add(1, std::memory_order_relaxed);
    totalAllocations.fetch_add(1, std::memory_order_relaxed);
    return memory;
}

void* operator new[](std::size_t size) { return ::operator new(size); }

void operator delete(void* memory) noexcept {
    if (memory) {
        outstandingAllocations.fetch_sub(1, std::memory_order_relaxed);
        std::free(memory);
    }
}

void operator delete[](void* memory) noexcept { ::operator delete(memory); }

void operator delete(void* memory, std::size_t) noexcept { ::operator delete(memory); }

void operator delete[](void* memory, std::size_t) noexcept { ::operator delete(memory); }

int main() {
    constexpr int warmupCycles = 3;
    constexpr int measuredCycles = 64;

    const int allocationFailureResult = runCApiAllocationFailureBoundary();
    if (allocationFailureResult != 0) {
        std::fprintf(stderr, "C API allocation-failure boundary failed with code %d\n", allocationFailureResult);
        return allocationFailureResult;
    }

    for (int cycle = 0; cycle < warmupCycles; ++cycle) {
        const int result = runOwnershipCycle();
        if (result != 0) {
            std::fprintf(stderr, "ownership lifetime warmup failed with code %d\n", result);
            return result;
        }
    }

    const long long baseline = outstandingAllocations.load(std::memory_order_relaxed);
    const unsigned long long allocationBaseline = totalAllocations.load(std::memory_order_relaxed);
    for (int cycle = 0; cycle < measuredCycles; ++cycle) {
        const int result = runOwnershipCycle();
        if (result != 0) {
            std::fprintf(stderr, "ownership lifetime cycle %d failed with code %d\n", cycle, result);
            return result;
        }
        const long long current = outstandingAllocations.load(std::memory_order_relaxed);
        if (current != baseline) {
            std::fprintf(
                stderr,
                "outstanding C++ allocations changed after cycle %d: baseline=%lld current=%lld\n",
                cycle,
                baseline,
                current
            );
            return 30;
        }
    }

    const unsigned long long measuredAllocations =
        totalAllocations.load(std::memory_order_relaxed) - allocationBaseline;
    if (measuredAllocations < static_cast<unsigned long long>(measuredCycles)) {
        std::fprintf(
            stderr,
            "allocation counter was not exercised: cycles=%d measured_allocations=%llu\n",
            measuredCycles,
            measuredAllocations
        );
        return 31;
    }

    std::printf(
        "ownership lifetime stress passed: cycles=%d measured_allocations=%llu outstanding_allocations=%lld\n",
        measuredCycles,
        measuredAllocations,
        baseline
    );
    return 0;
}
