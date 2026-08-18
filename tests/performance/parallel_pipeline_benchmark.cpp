/**
 * @file tests/performance/parallel_pipeline_benchmark.cpp
 * @brief 记录特征检测和 QEM 在不同执行宽度下的可复现结果与耗时。
 *
 * 该用例只报告测量值，不断言固定加速比；运行时间取决于 CPU、内存带宽和 oneTBB
 * 调度器配置。结果指纹用于防止性能路径悄然改变算法输出。
 */

#include "TestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

using manumesh::ExecutionMode;
using manumesh::ExecutionOptions;
using manumesh::Mesh;

class Fingerprint {
public:
    Fingerprint()
        : value_(1469598103934665603ULL) {}

    template <typename T> void add(const T& value) {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&value);
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            value_ ^= static_cast<std::uint64_t>(bytes[i]);
            value_ *= 1099511628211ULL;
        }
    }

    std::uint64_t value() const { return value_; }

private:
    std::uint64_t value_;
};

std::uint64_t meshFingerprint(const Mesh& mesh) {
    Fingerprint hash;
    const std::size_t vertexCount = mesh.vertices.size();
    const std::size_t faceCount = mesh.faces.size();
    hash.add(vertexCount);
    hash.add(faceCount);
    for (const auto& vertex : mesh.vertices) {
        hash.add(vertex.x());
        hash.add(vertex.y());
        hash.add(vertex.z());
    }
    for (const auto& face : mesh.faces) {
        hash.add(face.v[0]);
        hash.add(face.v[1]);
        hash.add(face.v[2]);
    }
    return hash.value();
}

std::uint64_t featureFingerprint(const manumesh::feature::FeatureAnalysis& analysis) {
    Fingerprint hash;
    hash.add(analysis.featureEdges);
    hash.add(analysis.tracedFeatureEdges);
    hash.add(analysis.normalTensorFeatureEdges);
    hash.add(analysis.smoothCurvatureFeatureEdges);
    hash.add(analysis.graph.edges.size());
    for (const auto& edge : analysis.graph.edges) {
        hash.add(edge.a);
        hash.add(edge.b);
        hash.add(edge.boundary);
        hash.add(edge.dihedral);
        hash.add(edge.normalTensor);
        hash.add(edge.smoothCurvature);
        hash.add(edge.nonManifold);
        hash.add(edge.cleanupBridge);
        hash.add(edge.consolidationBridge);
        hash.add(edge.removedByCleanup);
        hash.add(edge.signedKind);
        hash.add(edge.tensorPersistence);
        hash.add(edge.curvaturePersistence);
    }
    hash.add(analysis.loops.size());
    for (const auto& loop : analysis.loops) {
        hash.add(loop.id);
        hash.add(loop.componentId);
        hash.add(loop.edgeCount);
        hash.add(loop.closed);
        hash.add(loop.circular);
        hash.add(loop.vertices.size());
        for (int vertex : loop.vertices) {
            hash.add(vertex);
        }
    }
    return hash.value();
}

ExecutionOptions executionFor(int threadCount) {
    ExecutionOptions options;
    if (threadCount < 0) {
        options.mode = ExecutionMode::Serial;
        return options;
    }
    options.mode = ExecutionMode::Parallel;
    options.maxConcurrency = threadCount;
    options.minItemsPerTask = 256;
    return options;
}

template <typename Function> double timeMs(Function&& function) {
    const auto start = std::chrono::steady_clock::now();
    function();
    const auto stop = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(stop - start).count();
}

} // namespace

TEST(ParallelPipelineBenchmark, ReportsFeatureAndSimplificationScalingWithoutFixedSpeedupAssertion) {
    const Mesh featureMesh = manumesh::generateBumpGrid(256, 2.0);
    manumesh::feature::FeatureOptions featureOptions = manumesh::feature::makeFeatureOptions(
        manumesh::feature::FeatureProfile::NoisyScan
    );
    featureOptions.normalFilter.iterations = 2;
    featureOptions.normalTensorScaleCount = 3;
    featureOptions.normalTensorSmoothingIterations = 1;
    featureOptions.useSmoothCurvatureFeatures = false;

    std::cout << "\nparallel_pipeline,stage,threads,backend,vertices,faces,wall_ms,fingerprint\n";
    const int threadCounts[] = {-1, 1, 2, 4, 8};
    std::uint64_t serialFeatureFingerprint = 0;
    for (int threadCount : threadCounts) {
        const ExecutionOptions execution = executionFor(threadCount);
        manumesh::feature::FeatureAnalysis analysis;
        const double elapsed = timeMs([&]() {
            analysis = manumesh::feature::detectFeatureCurves(featureMesh, featureOptions, execution);
        });
        const std::uint64_t fingerprint = featureFingerprint(analysis);
        if (threadCount < 0) {
            serialFeatureFingerprint = fingerprint;
        } else {
            EXPECT_EQ(serialFeatureFingerprint, fingerprint) << "feature output changed at threads=" << threadCount;
        }
        std::cout << "parallel_pipeline,feature," << (threadCount < 0 ? 0 : threadCount) << ","
                  << manumesh::parallelExecutionBackendName() << "," << featureMesh.vertices.size() << ","
                  << featureMesh.faces.size() << "," << std::fixed << std::setprecision(2) << elapsed << ","
                  << fingerprint << "\n";
    }

    const Mesh simplifyMesh = manumesh::generatePlaneGrid(128, 1.0, false);
    manumesh::simplification::SimplifyOptions simplifyOptions = manumesh::test::lineOptions(0.72);
    simplifyOptions.maxNormalDeviationDeg = 180.0;
    simplifyOptions.minTriangleQuality = 0.0;
    std::uint64_t serialMeshFingerprint = 0;
    for (int threadCount : threadCounts) {
        const ExecutionOptions execution = executionFor(threadCount);
        Mesh output;
        manumesh::simplification::SimplifyReport report;
        const double elapsed = timeMs([&]() {
            output = manumesh::simplification::simplifyMesh(simplifyMesh, simplifyOptions, execution, &report);
        });
        const std::uint64_t fingerprint = meshFingerprint(output);
        if (threadCount < 0) {
            serialMeshFingerprint = fingerprint;
        } else {
            EXPECT_EQ(serialMeshFingerprint, fingerprint) << "simplification output changed at threads=" << threadCount;
        }
        std::cout << "parallel_pipeline,simplification," << (threadCount < 0 ? 0 : threadCount) << ","
                  << manumesh::parallelExecutionBackendName() << "," << simplifyMesh.vertices.size() << ","
                  << simplifyMesh.faces.size() << "," << std::fixed << std::setprecision(2) << elapsed << ","
                  << fingerprint << "\n";
        EXPECT_EQ(report.finalFaces, static_cast<int>(output.faces.size()));
    }
}
