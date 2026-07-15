#include "algorithms/feature_detection/FeatureDetector.h"

#include "common/detail/MeshQueries.h"
#include "detail/FeatureDetectionCache.h"
#include "detail/FeatureDetectionTypes.h"
#include "detail/FeatureEvidence.h"
#include "detail/FeatureGraph.h"
#include "detail/FeatureGraphCleanup.h"
#include "detail/FeatureGraphConsolidation.h"
#include "detail/FeatureInputValidation.h"
#include "detail/FeatureLoopRecovery.h"
#include "detail/FeatureSegmentation.h"
#include "detail/PrimitiveFit.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace manumesh::feature {
namespace {

using detector_detail::CandidateEdge;
using detector_detail::FeatureAnalysisBuilder;
using detector_detail::FeatureDetectionCache;
using detector_detail::TraceGraph;

struct FeatureDetectionContext {
    FeatureDetectionContext(const Mesh& inputMesh, const FeatureOptions& inputOptions)
        : mesh(inputMesh),
          options(inputOptions),
          cache(inputMesh, inputOptions.normalFilter),
          builder(static_cast<int>(inputMesh.vertices.size())) {}

    FeatureAnalysis& analysis() { return builder.analysis(); }

    const Mesh& mesh;
    const FeatureOptions& options;
    FeatureDetectionCache cache;
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
    if (options.normalTensorSmoothingIterations < 0 ||
        options.normalTensorSmoothingIterations > kMaxNormalTensorSmoothingIterations) {
        throw std::invalid_argument(
            "normalTensorSmoothingIterations must be in [0, " + std::to_string(kMaxNormalTensorSmoothingIterations) +
            "]."
        );
    }
    if (options.normalTensorScaleCount < 1 || options.normalTensorScaleCount > kMaxNormalTensorScaleCount) {
        throw std::invalid_argument(
            "normalTensorScaleCount must be in [1, " + std::to_string(kMaxNormalTensorScaleCount) + "]."
        );
    }
    if (options.normalTensorMinPersistentScales < 1 ||
        options.normalTensorMinPersistentScales > options.normalTensorScaleCount) {
        throw std::invalid_argument("normalTensorMinPersistentScales must be in [1, normalTensorScaleCount].");
    }
    requireFiniteNonNegative(options.smoothCurvatureFeatureThreshold, "smoothCurvatureFeatureThreshold");
    if (!std::isfinite(options.smoothCurvatureMinEdgeAlignment) || options.smoothCurvatureMinEdgeAlignment < 0.0 ||
        options.smoothCurvatureMinEdgeAlignment > 1.0) {
        throw std::invalid_argument("smoothCurvatureMinEdgeAlignment must be finite and in [0, 1].");
    }
    if (!std::isfinite(options.smoothCurvatureMinTangentConsistency) ||
        options.smoothCurvatureMinTangentConsistency < 0.0 || options.smoothCurvatureMinTangentConsistency > 1.0) {
        throw std::invalid_argument("smoothCurvatureMinTangentConsistency must be finite and in [0, 1].");
    }
    if (options.smoothCurvatureBaseNeighborhoodRings < 1 ||
        options.smoothCurvatureBaseNeighborhoodRings > kMaxSmoothCurvatureBaseNeighborhoodRings) {
        throw std::invalid_argument(
            "smoothCurvatureBaseNeighborhoodRings must be in [1, " +
            std::to_string(kMaxSmoothCurvatureBaseNeighborhoodRings) + "]."
        );
    }
    if (options.smoothCurvatureScaleCount < 1 || options.smoothCurvatureScaleCount > kMaxSmoothCurvatureScaleCount) {
        throw std::invalid_argument(
            "smoothCurvatureScaleCount must be in [1, " + std::to_string(kMaxSmoothCurvatureScaleCount) + "]."
        );
    }
    if (options.smoothCurvatureMinPersistentScales < 1 ||
        options.smoothCurvatureMinPersistentScales > options.smoothCurvatureScaleCount) {
        throw std::invalid_argument("smoothCurvatureMinPersistentScales must be in [1, smoothCurvatureScaleCount].");
    }
    if (options.smoothCurvatureRobustFitIterations < 0 ||
        options.smoothCurvatureRobustFitIterations > kMaxSmoothCurvatureRobustFitIterations) {
        throw std::invalid_argument(
            "smoothCurvatureRobustFitIterations must be in [0, " +
            std::to_string(kMaxSmoothCurvatureRobustFitIterations) + "]."
        );
    }
    if (!std::isfinite(options.smoothCurvatureMinScaleStability) || options.smoothCurvatureMinScaleStability < 0.0 ||
        options.smoothCurvatureMinScaleStability > 1.0) {
        throw std::invalid_argument("smoothCurvatureMinScaleStability must be finite and in [0, 1].");
    }
    requireFiniteNonNegative(options.featureGraphGapLengthRatio, "featureGraphGapLengthRatio");
    if (options.featureGraphMaxWeakSpurEdges < 0) {
        throw std::invalid_argument("featureGraphMaxWeakSpurEdges must be non-negative.");
    }
    if (!std::isfinite(options.featureComponentMinConfidence) || options.featureComponentMinConfidence < 0.0 ||
        options.featureComponentMinConfidence > 1.0) {
        throw std::invalid_argument("featureComponentMinConfidence must be finite and in [0, 1].");
    }
    requireFiniteNonNegative(options.featureGraphMinWeakSpurStrength, "featureGraphMinWeakSpurStrength");
    if (options.normalFilter.iterations < 0 || options.normalFilter.iterations > kMaxFeatureNormalFilterIterations) {
        throw std::invalid_argument(
            "normalFilter.iterations must be in [0, " + std::to_string(kMaxFeatureNormalFilterIterations) + "]."
        );
    }
    if (!std::isfinite(options.normalFilter.angleSigmaDeg) || options.normalFilter.angleSigmaDeg <= 0.0 ||
        options.normalFilter.angleSigmaDeg > 180.0) {
        throw std::invalid_argument("normalFilter.angleSigmaDeg must be finite and in (0, 180].");
    }
    if (!std::isfinite(options.normalFilter.preserveAngleDeg) || options.normalFilter.preserveAngleDeg < 0.0 ||
        options.normalFilter.preserveAngleDeg > 180.0) {
        throw std::invalid_argument("normalFilter.preserveAngleDeg must be finite and in [0, 180].");
    }
    if (!std::isfinite(options.normalFilter.relaxation) || options.normalFilter.relaxation < 0.0 ||
        options.normalFilter.relaxation > 1.0) {
        throw std::invalid_argument("normalFilter.relaxation must be finite and in [0, 1].");
    }
    requireFiniteNonNegative(options.graphConsolidation.maxGapLengthRatio, "graphConsolidation.maxGapLengthRatio");
    if (!std::isfinite(options.graphConsolidation.minAlignment) || options.graphConsolidation.minAlignment < 0.0 ||
        options.graphConsolidation.minAlignment > 1.0) {
        throw std::invalid_argument("graphConsolidation.minAlignment must be finite and in [0, 1].");
    }
}

void validateFeatureInput(const Mesh& mesh) { detector_detail::validateFeatureMeshInput(mesh); }

class EdgeEvidenceStage {
public:
    void run(FeatureDetectionContext& context) const {
        context.featureEdges =
            detector_detail::collectFeatureEdges(context.mesh, context.options, context.cache, context.builder);
    }
};

/// Downgrades evidence that depends on unusable face normals.
///
/// Zero-area faces are tolerated by the lenient input validation, but their
/// normals are zero vectors, so any dihedral angle computed against them is
/// meaningless (the zero dot product reads as a 90-degree pseudo-crease).
/// This stage strips dihedral evidence from every interior edge incident to
/// a degenerate face and drops candidates left without any evidence, keeping
/// the analysis counters consistent. Normal-tensor and smooth-curvature
/// scoring already skip degenerate faces during accumulation, and boundary /
/// non-manifold evidence is purely topological, so only the dihedral channel
/// needs the downgrade. The tolerated faces stay visible through
/// FeatureAnalysis::degenerateFaces.
class DegenerateEvidenceFilterStage {
public:
    void run(FeatureDetectionContext& context) const {
        if (context.analysis().degenerateFaces == 0) {
            return;
        }
        std::vector<char> degenerateFace(context.mesh.faces.size(), 0);
        const std::vector<Vec3>& faceNormals = context.cache.faceNormals();
        for (std::size_t faceIndex = 0; faceIndex < faceNormals.size(); ++faceIndex) {
            // triangleNormal returns the exact zero vector for degenerate faces.
            if (faceNormals[faceIndex].squaredNorm() <= 0.0) {
                degenerateFace[faceIndex] = 1;
            }
        }

        const manumesh::common::MeshEdgeInfoMap& edgeInfo = context.cache.edgeInfo();
        FeatureAnalysis& analysis = context.analysis();
        std::vector<CandidateEdge> kept;
        kept.reserve(context.featureEdges.size());
        for (CandidateEdge edge : context.featureEdges) {
            if (edge.dihedral) {
                const auto it = edgeInfo.find(manumesh::common::meshEdgeKey(edge.a, edge.b));
                bool touchesDegenerate = false;
                if (it != edgeInfo.end()) {
                    for (int faceId : it->second.faces) {
                        if (faceId >= 0 && faceId < static_cast<int>(degenerateFace.size()) && degenerateFace[faceId]) {
                            touchesDegenerate = true;
                            break;
                        }
                    }
                }
                if (touchesDegenerate) {
                    --analysis.dihedralFeatureEdges;
                    if (edge.signedKind > 0) {
                        --analysis.convexFeatureEdges;
                    } else if (edge.signedKind < 0) {
                        --analysis.concaveFeatureEdges;
                    } else {
                        --analysis.unknownSignedFeatureEdges;
                    }
                    edge.dihedral = false;
                    edge.signedKind = 0;
                    edge.angleRad = 0.0;
                }
            }
            if (edge.boundary || edge.dihedral || edge.normalTensor || edge.smoothCurvature || edge.nonManifold) {
                kept.push_back(edge);
            } else {
                --analysis.featureEdges;
            }
        }
        context.featureEdges = std::move(kept);
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
        detector_detail::cleanupTraceGraph(
            context.mesh, context.options, context.cache, context.trace, context.analysis()
        );
    }
};

class FeatureGraphConsolidationStage {
public:
    void run(FeatureDetectionContext& context) const {
        detector_detail::consolidateFeatureGraph(
            context.mesh, context.options, context.cache, context.trace, context.analysis()
        );
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
        detector_detail::finalizeFeatureGraphMarkers(context.mesh, context.analysis());
    }
};

class FeatureSegmentationStage {
public:
    void run(FeatureDetectionContext& context) const {
        if (context.options.surfacePatches.enabled) {
            detector_detail::buildFeaturePatches(context.mesh, context.analysis(), context.options.surfacePatches);
        }
    }
};

class FeatureDetectionPipeline {
public:
    FeatureAnalysis run(const Mesh& mesh, const FeatureOptions& options) const {
        validateFeatureOptionsImpl(options);
        validateFeatureInput(mesh);

        FeatureDetectionContext context(mesh, options);
        // Degenerate (zero-area) faces are tolerated by the lenient input
        // validation; their normals are unusable, so evidence stages skip
        // them. Surface the count so callers see the degraded coverage.
        context.analysis().degenerateFaces = countDegenerateFaces(mesh);
        if (mesh.empty()) {
            return context.builder.build();
        }

        edgeEvidence_.run(context);
        degenerateFilter_.run(context);
        featureGraph_.run(context);
        cleanup_.run(context);
        consolidation_.run(context);
        loopRecovery_.run(context);
        componentSummary_.run(context);
        finalize_.run(context);
        segmentation_.run(context);
        return context.builder.build();
    }

private:
    EdgeEvidenceStage edgeEvidence_;
    DegenerateEvidenceFilterStage degenerateFilter_;
    FeatureGraphStage featureGraph_;
    FeatureGraphCleanupStage cleanup_;
    FeatureGraphConsolidationStage consolidation_;
    LoopRecoveryStage loopRecovery_;
    FeatureComponentSummaryStage componentSummary_;
    FeatureGraphFinalizeStage finalize_;
    FeatureSegmentationStage segmentation_;
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

} // namespace manumesh::feature
