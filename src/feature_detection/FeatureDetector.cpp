#include "algorithms/feature_detection/FeatureDetector.h"

#include "common/detail/MeshQueries.h"
#include "detail/FeatureDetectionTypes.h"
#include "detail/FeatureEvidence.h"
#include "detail/FeatureGraph.h"
#include "detail/FeatureGraphCleanup.h"
#include "detail/FeatureLoopRecovery.h"
#include "detail/PrimitiveFit.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace manumesh::feature {
namespace {

using detector_detail::CandidateEdge;
using detector_detail::FeatureAnalysisBuilder;
using detector_detail::TraceGraph;

struct FeatureDetectionContext {
    FeatureDetectionContext(const Mesh& inputMesh, const FeatureOptions& inputOptions)
        : mesh(inputMesh),
          options(inputOptions),
          builder(static_cast<int>(inputMesh.vertices.size())) {}

    FeatureAnalysis& analysis() { return builder.analysis(); }

    const Mesh& mesh;
    const FeatureOptions& options;
    FeatureAnalysisBuilder builder;
    std::vector<CandidateEdge> featureEdges;
    TraceGraph trace;
};

void requireFiniteNonNegative(double value, const char* name) {
    if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(std::string(name) + " must be finite and non-negative.");
    }
}

void validateFeatureOptionsImpl(const FeatureOptions& options) {
    if (!std::isfinite(options.featureAngleDeg) || options.featureAngleDeg < 0.0 || options.featureAngleDeg > 180.0) {
        throw std::invalid_argument("featureAngleDeg must be finite and in [0, 180].");
    }
    if (!std::isfinite(options.loopTraceAngleDeg) ||
        (options.loopTraceAngleDeg >= 0.0 && options.loopTraceAngleDeg > 180.0)) {
        throw std::invalid_argument("loopTraceAngleDeg must be negative or finite and in [0, 180].");
    }
    requireFiniteNonNegative(options.circleFitRelativeThreshold, "circleFitRelativeThreshold");
    requireFiniteNonNegative(options.ellipseFitRelativeThreshold, "ellipseFitRelativeThreshold");
    requireFiniteNonNegative(options.nearCircleAxisRatioTolerance, "nearCircleAxisRatioTolerance");
    requireFiniteNonNegative(options.normalTensorFeatureThreshold, "normalTensorFeatureThreshold");
    if (!std::isfinite(options.normalTensorMinEdgeAlignment) || options.normalTensorMinEdgeAlignment < 0.0 ||
        options.normalTensorMinEdgeAlignment > 1.0) {
        throw std::invalid_argument("normalTensorMinEdgeAlignment must be finite and in [0, 1].");
    }
    if (options.minFeatureLoopVertices < 3) {
        throw std::invalid_argument("minFeatureLoopVertices must be at least 3.");
    }
    if (options.normalTensorSmoothingIterations < 0) {
        throw std::invalid_argument("normalTensorSmoothingIterations must be non-negative.");
    }
    if (options.normalTensorScaleCount < 1) {
        throw std::invalid_argument("normalTensorScaleCount must be positive.");
    }
    if (options.normalTensorMinPersistentScales < 1) {
        throw std::invalid_argument("normalTensorMinPersistentScales must be positive.");
    }
    requireFiniteNonNegative(options.featureGraphGapLengthRatio, "featureGraphGapLengthRatio");
    if (options.featureGraphMaxWeakSpurEdges < 0) {
        throw std::invalid_argument("featureGraphMaxWeakSpurEdges must be non-negative.");
    }
    if (!std::isfinite(options.featureComponentMinConfidence) || options.featureComponentMinConfidence < 0.0 ||
        options.featureComponentMinConfidence > 1.0) {
        throw std::invalid_argument("featureComponentMinConfidence must be finite and in [0, 1].");
    }
}

void validateFeatureInput(const Mesh& mesh) {
    if (mesh.faces.empty()) {
        return;
    }
    std::string error;
    if (!validateMeshGeometry(mesh, &error)) {
        throw std::invalid_argument(error);
    }
}

class EdgeEvidenceStage {
public:
    void run(FeatureDetectionContext& context) const {
        context.featureEdges = detector_detail::collectFeatureEdges(context.mesh, context.options, context.builder);
    }
};

class FeatureGraphStage {
public:
    void run(FeatureDetectionContext& context) const {
        detector_detail::initializeFeatureGraph(context.featureEdges, context.analysis());
        context.trace =
            detector_detail::buildTraceGraph(context.mesh, context.options, context.featureEdges, context.analysis());
    }
};

class FeatureGraphCleanupStage {
public:
    void run(FeatureDetectionContext& context) const {
        detector_detail::cleanupTraceGraph(context.mesh, context.options, context.trace, context.analysis());
    }
};

class LoopRecoveryStage {
public:
    void run(FeatureDetectionContext& context) const {
        detector_detail::recoverFeatureLoops(
            context.mesh, context.options, context.trace, context.analysis(), context.builder.nextLoopId()
        );
    }
};

class FeatureComponentSummaryStage {
public:
    void run(FeatureDetectionContext& context) const {
        detector_detail::summarizeFeatureComponents(context.mesh, context.options, context.trace, context.analysis());
    }
};

class FeatureGraphFinalizeStage {
public:
    void run(FeatureDetectionContext& context) const {
        detector_detail::finalizeFeatureGraphMarkers(context.analysis());
    }
};

class FeatureDetectionPipeline {
public:
    FeatureAnalysis run(const Mesh& mesh, const FeatureOptions& options) const {
        validateFeatureOptionsImpl(options);
        validateFeatureInput(mesh);

        FeatureDetectionContext context(mesh, options);
        if (mesh.empty()) {
            return context.builder.build();
        }

        edgeEvidence_.run(context);
        featureGraph_.run(context);
        cleanup_.run(context);
        loopRecovery_.run(context);
        componentSummary_.run(context);
        finalize_.run(context);
        return context.builder.build();
    }

private:
    EdgeEvidenceStage edgeEvidence_;
    FeatureGraphStage featureGraph_;
    FeatureGraphCleanupStage cleanup_;
    LoopRecoveryStage loopRecovery_;
    FeatureComponentSummaryStage componentSummary_;
    FeatureGraphFinalizeStage finalize_;
};

} // namespace

void validateFeatureOptions(const FeatureOptions& options) { validateFeatureOptionsImpl(options); }

struct FeatureDetector::Impl {
    FeatureOptions options;
};

FeatureDetector::FeatureDetector(FeatureOptions options)
    : impl_(std::make_unique<Impl>()) {
    impl_->options = std::move(options);
}

FeatureDetector::~FeatureDetector() = default;

FeatureDetector::FeatureDetector(const FeatureDetector& other)
    : impl_(other.impl_ ? std::make_unique<Impl>(*other.impl_) : std::make_unique<Impl>()) {}

FeatureDetector& FeatureDetector::operator=(const FeatureDetector& other) {
    if (this != &other) {
        impl_ = other.impl_ ? std::make_unique<Impl>(*other.impl_) : std::make_unique<Impl>();
    }
    return *this;
}

FeatureDetector::FeatureDetector(FeatureDetector&& other) noexcept
    : impl_(std::move(other.impl_)) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    other.impl_ = std::make_unique<Impl>();
}

FeatureDetector& FeatureDetector::operator=(FeatureDetector&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        if (!impl_) {
            impl_ = std::make_unique<Impl>();
        }
        other.impl_ = std::make_unique<Impl>();
    }
    return *this;
}

const FeatureOptions& FeatureDetector::options() const { return impl_->options; }

void FeatureDetector::setOptions(FeatureOptions options) { impl_->options = std::move(options); }

FeatureAnalysis FeatureDetector::analyze(const Mesh& mesh) const { return detectFeatureCurves(mesh, impl_->options); }

FeatureAnalysis detectFeatureCurves(const Mesh& mesh, const FeatureOptions& options) {
    return FeatureDetectionPipeline{}.run(mesh, options);
}

DirectionalCurveError measureLoopAgainstCircle(
    const Mesh& mesh, const FeatureLoop& loop, const Vec3& center, const Vec3& normalIn, double radius
) {
    return primitive_fit_detail::measureLoopAgainstCircle(mesh, loop, center, normalIn, radius);
}

std::string featureReportHeaderCsv() {
    return "loop_id,component_id,component_confidence,weak_feature,primitive_residual,"
           "vertices,edges,closed,primitive,circular,mostly_boundary,cx,cy,cz,"
           "nx,ny,nz,major_axis_x,major_axis_y,major_axis_z,minor_axis_x,"
           "minor_axis_y,minor_axis_z,radius,major_radius,minor_radius,axis_ratio,"
           "rms_radial,max_radial,rms_ellipse,max_ellipse,rms_plane,max_plane,"
           "convex_edges,concave_edges,unknown_signed_edges";
}

std::string featureLoopRowCsv(const FeatureLoop& loop) {
    std::ostringstream out;
    out << std::setprecision(12);
    out << loop.id << "," << loop.componentId << "," << loop.componentConfidence << "," << (loop.weakFeature ? 1 : 0)
        << "," << loop.primitiveResidual << "," << loop.vertices.size() << "," << loop.edgeCount << ","
        << (loop.closed ? 1 : 0) << "," << toString(loop.primitive) << "," << (loop.circular ? 1 : 0) << ","
        << (loop.mostlyBoundary ? 1 : 0) << "," << loop.center.x() << "," << loop.center.y() << "," << loop.center.z()
        << "," << loop.normal.x() << "," << loop.normal.y() << "," << loop.normal.z() << "," << loop.majorAxis.x()
        << "," << loop.majorAxis.y() << "," << loop.majorAxis.z() << "," << loop.minorAxis.x() << ","
        << loop.minorAxis.y() << "," << loop.minorAxis.z() << "," << loop.radius << "," << loop.majorRadius << ","
        << loop.minorRadius << "," << loop.axisRatio << "," << loop.rmsRadialError << "," << loop.maxRadialError << ","
        << loop.rmsEllipseError << "," << loop.maxEllipseError << "," << loop.rmsPlaneError << "," << loop.maxPlaneError
        << "," << loop.convexEdges << "," << loop.concaveEdges << "," << loop.unknownSignedEdges;
    return out.str();
}

std::string toString(FeaturePrimitiveType primitive) {
    switch (primitive) {
    case FeaturePrimitiveType::Unknown:
        return "unknown";
    case FeaturePrimitiveType::Circle:
        return "circle";
    case FeaturePrimitiveType::NearCircle:
        return "near-circle";
    case FeaturePrimitiveType::Ellipse:
        return "ellipse";
    case FeaturePrimitiveType::PolygonalLoop:
        return "polygonal-loop";
    }
    return "unknown";
}

FeatureEdgeBenchmark benchmarkFeatureEdges(
    const FeatureAnalysis& analysis,
    const std::vector<std::pair<int, int>>& groundTruthEdges,
    const std::vector<int>& groundTruthJunctionVertices
) {
    FeatureEdgeBenchmark result;
    std::unordered_set<std::uint64_t> truthEdges;
    truthEdges.reserve(groundTruthEdges.size());
    for (const auto& [a, b] : groundTruthEdges) {
        if (a >= 0 && b >= 0 && a != b) {
            truthEdges.insert(manumesh::detail::meshEdgeKey(a, b));
        }
    }

    std::unordered_set<std::uint64_t> detectedEdges;
    detectedEdges.reserve(analysis.graph.edges.size());
    for (const FeatureGraphEdge& edge : analysis.graph.edges) {
        if (edge.removedByCleanup || edge.a < 0 || edge.b < 0 || edge.a == edge.b) {
            continue;
        }
        detectedEdges.insert(manumesh::detail::meshEdgeKey(edge.a, edge.b));
    }

    result.groundTruthEdges = static_cast<int>(truthEdges.size());
    result.detectedEdges = static_cast<int>(detectedEdges.size());
    for (std::uint64_t edge : detectedEdges) {
        if (truthEdges.find(edge) != truthEdges.end()) {
            ++result.truePositiveEdges;
        } else {
            ++result.falsePositiveEdges;
        }
    }
    for (std::uint64_t edge : truthEdges) {
        if (detectedEdges.find(edge) == detectedEdges.end()) {
            ++result.falseNegativeEdges;
        }
    }

    auto ratio = [](int numerator, int denominator) {
        return denominator > 0 ? static_cast<double>(numerator) / static_cast<double>(denominator) : 0.0;
    };
    auto f1 = [](double precision, double recall) {
        return precision + recall > 0.0 ? 2.0 * precision * recall / (precision + recall) : 0.0;
    };
    result.edgePrecision = ratio(result.truePositiveEdges, result.truePositiveEdges + result.falsePositiveEdges);
    result.edgeRecall = ratio(result.truePositiveEdges, result.truePositiveEdges + result.falseNegativeEdges);
    result.edgeF1 = f1(result.edgePrecision, result.edgeRecall);

    std::unordered_set<int> truthJunctions;
    truthJunctions.reserve(groundTruthJunctionVertices.size());
    for (int id : groundTruthJunctionVertices) {
        if (id >= 0) {
            truthJunctions.insert(id);
        }
    }
    std::unordered_set<int> detectedJunctions(
        analysis.graph.junctionVertices.begin(), analysis.graph.junctionVertices.end()
    );
    result.groundTruthJunctions = static_cast<int>(truthJunctions.size());
    result.detectedJunctions = static_cast<int>(detectedJunctions.size());
    for (int id : detectedJunctions) {
        if (truthJunctions.find(id) != truthJunctions.end()) {
            ++result.truePositiveJunctions;
        } else {
            ++result.falsePositiveJunctions;
        }
    }
    for (int id : truthJunctions) {
        if (detectedJunctions.find(id) == detectedJunctions.end()) {
            ++result.falseNegativeJunctions;
        }
    }
    result.junctionPrecision =
        ratio(result.truePositiveJunctions, result.truePositiveJunctions + result.falsePositiveJunctions);
    result.junctionRecall =
        ratio(result.truePositiveJunctions, result.truePositiveJunctions + result.falseNegativeJunctions);
    result.junctionF1 = f1(result.junctionPrecision, result.junctionRecall);

    if (!analysis.components.empty()) {
        double closureSum = 0.0;
        for (const FeatureComponent& component : analysis.components) {
            closureSum += component.closureRate;
        }
        result.loopClosureRate = closureSum / static_cast<double>(analysis.components.size());
    }
    result.meanComponentConfidence = analysis.meanFeatureComponentConfidence;
    return result;
}

} // namespace manumesh::feature
