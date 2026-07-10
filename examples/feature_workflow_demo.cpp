#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
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
    simplifyOptions.minFeatureLoopVertices = 6;
    simplifyOptions.minCircularFeatureLoopVertices = 6;
    simplifyOptions.boundaryWeight = 1.0;

    manumesh::simplification::SimplifyReport simplifyReport;
    const manumesh::Mesh output =
        manumesh::simplification::QEMSimplifier(simplifyOptions).simplify(input, &simplifyReport);

    const manumesh::simplification::MeshStats inputStats = manumesh::simplification::computeMeshStats(input);
    const manumesh::simplification::MeshStats outputStats = manumesh::simplification::computeMeshStats(output);
    const manumesh::simplification::DistanceStats distance =
        manumesh::simplification::compareMeshesBySampledDistance(input, output, gateOptions.distanceSamples);

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
