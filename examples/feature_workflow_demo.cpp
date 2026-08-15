/**
 * @file examples/feature_workflow_demo.cpp
 * @brief 通过 ManuMesh SDK 示例演示特征工作流示例。
 * @ingroup manumesh_examples
 *
 * @details 示例只使用受支持的公共入口，同时作为特征分析与简化集成流程的可执行文档。
 */

#include "algorithms/analysis/MeshAnalysis.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {

struct QualityGateOptions {
    double targetRatio = 0.45;
    double maxSampledDistance = 1.0;
    int distanceSamples = 128;
};

struct QualityGateReport {
    int inputFaces = 0;
    int outputFaces = 0;
    int detectedFeatureLoops = 0;
    int collapsedEdges = 0;
    double outputMinTriangleQuality = 0.0;
    double maxSampledDistance = 0.0;
};

struct QualityGateResult {
    manumesh::Mesh mesh;
    QualityGateReport report;
    bool accepted = false;
};

QualityGateResult runManufacturingQualityGate(const manumesh::Mesh& input, const QualityGateOptions& gateOptions) {
    if (input.empty()) {
        throw std::invalid_argument("quality gate requires a non-empty mesh");
    }

    manumesh::feature::FeatureOptions featureOptions;
    featureOptions.minFeatureLoopVertices = 6;
    featureOptions.useNormalTensorFeatures = true;

    const manumesh::feature::FeatureAnalysis features =
        manumesh::feature::FeatureDetector(featureOptions).analyze(input);

    manumesh::simplification::SimplifyOptions simplifyOptions;
    simplifyOptions.targetRatio = gateOptions.targetRatio;
    simplifyOptions.useLineQuadrics = true;
    simplifyOptions.preserveFeatureCurves = true;
    simplifyOptions.featureCurveWeight = 0.05;
    simplifyOptions.featureOptionsOverride = featureOptions;
    simplifyOptions.minCircularFeatureLoopVertices = 6;
    simplifyOptions.boundaryWeight = 1.0;

    manumesh::simplification::SimplifyReport simplifyReport;
    // 复用上面已经计算的 FeatureAnalysis，避免简化器在内部再次检测特征；
    // (input, features, report) 重载确保同一网格不会重复运行特征分析。
    const manumesh::Mesh output =
        manumesh::simplification::QEMSimplifier(simplifyOptions).simplify(input, features, &simplifyReport);

    const manumesh::analysis::MeshStats inputStats = manumesh::analysis::computeMeshStats(input);
    const manumesh::analysis::MeshStats outputStats = manumesh::analysis::computeMeshStats(output);
    const manumesh::analysis::DistanceStats distance =
        manumesh::analysis::compareMeshesBySampledDistance(input, output, gateOptions.distanceSamples);

    QualityGateResult result;
    result.mesh = output;
    result.report.inputFaces = inputStats.faces;
    result.report.outputFaces = outputStats.faces;
    result.report.detectedFeatureLoops = static_cast<int>(features.loops.size());
    result.report.collapsedEdges = simplifyReport.collapsedEdges;
    result.report.outputMinTriangleQuality = outputStats.minTriangleQuality;
    result.report.maxSampledDistance = std::max(distance.maxOriginalToSimplified, distance.maxSimplifiedToOriginal);

    result.accepted = !output.empty() && outputStats.faces < inputStats.faces && outputStats.area > 0.0 &&
                      result.report.maxSampledDistance <= gateOptions.maxSampledDistance;
    return result;
}

} // namespace

int main() {
    const manumesh::Mesh input = manumesh::generateCylinderGrid(48, 12, 1.0, 2.0);

    QualityGateOptions options;
    options.targetRatio = 0.45;
    options.maxSampledDistance = 1.0;

    const QualityGateResult result = runManufacturingQualityGate(input, options);

    std::cout << "feature_workflow_demo"
              << " input_faces=" << result.report.inputFaces << " output_faces=" << result.report.outputFaces
              << " feature_loops=" << result.report.detectedFeatureLoops
              << " collapsed_edges=" << result.report.collapsedEdges
              << " min_triangle_quality=" << result.report.outputMinTriangleQuality
              << " max_sampled_distance=" << result.report.maxSampledDistance
              << " accepted=" << (result.accepted ? 1 : 0) << "\n";

    return result.accepted ? 0 : 1;
}
