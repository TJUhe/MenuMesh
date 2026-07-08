#include "algorithms/feature_detection/FeatureDetector.h"

#include "detail/FeatureDetectionTypes.h"
#include "detail/FeatureEvidence.h"
#include "detail/FeatureGraph.h"
#include "detail/FeatureLoopRecovery.h"
#include "detail/PrimitiveFit.h"

#include <iomanip>
#include <memory>
#include <sstream>
#include <utility>

namespace manumesh::feature {
namespace {

using detector_detail::CandidateEdge;
using detector_detail::FeatureAnalysisBuilder;
using detector_detail::TraceGraph;

struct FeatureDetectionContext {
  FeatureDetectionContext(const Mesh& inputMesh, const FeatureOptions& inputOptions)
      : mesh(inputMesh), options(inputOptions),
        builder(static_cast<int>(inputMesh.vertices.size())) {}

  FeatureAnalysis& analysis() { return builder.analysis(); }

  const Mesh& mesh;
  const FeatureOptions& options;
  FeatureAnalysisBuilder builder;
  std::vector<CandidateEdge> featureEdges;
  TraceGraph trace;
};

class EdgeEvidenceStage {
public:
  void run(FeatureDetectionContext& context) const {
    context.featureEdges = detector_detail::collectFeatureEdges(
        context.mesh, context.options, context.builder);
  }
};

class FeatureGraphStage {
public:
  void run(FeatureDetectionContext& context) const {
    detector_detail::initializeFeatureGraph(context.featureEdges, context.analysis());
    context.trace = detector_detail::buildTraceGraph(context.mesh, context.options,
                                                     context.featureEdges);
  }
};

class LoopRecoveryStage {
public:
  void run(FeatureDetectionContext& context) const {
    detector_detail::recoverFeatureLoops(context.mesh, context.options, context.trace,
                                         context.analysis(),
                                         context.builder.nextLoopId());
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
    FeatureDetectionContext context(mesh, options);
    if (mesh.empty()) {
      return context.builder.build();
    }

    edgeEvidence_.run(context);
    featureGraph_.run(context);
    loopRecovery_.run(context);
    finalize_.run(context);
    return context.builder.build();
  }

private:
  EdgeEvidenceStage edgeEvidence_;
  FeatureGraphStage featureGraph_;
  LoopRecoveryStage loopRecovery_;
  FeatureGraphFinalizeStage finalize_;
};

} // namespace

struct FeatureDetector::Impl {
  FeatureOptions options;
};

FeatureDetector::FeatureDetector(FeatureOptions options)
    : impl_(std::make_unique<Impl>()) {
  impl_->options = std::move(options);
}

FeatureDetector::~FeatureDetector() = default;

FeatureDetector::FeatureDetector(const FeatureDetector& other)
    : impl_(other.impl_ ? std::make_unique<Impl>(*other.impl_)
                        : std::make_unique<Impl>()) {
}

FeatureDetector& FeatureDetector::operator=(const FeatureDetector& other) {
  if (this != &other) {
    impl_ =
        other.impl_ ? std::make_unique<Impl>(*other.impl_) : std::make_unique<Impl>();
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

const FeatureOptions& FeatureDetector::options() const {
  return impl_->options;
}

void FeatureDetector::setOptions(FeatureOptions options) {
  impl_->options = std::move(options);
}

FeatureAnalysis FeatureDetector::analyze(const Mesh& mesh) const {
  return detectFeatureCurves(mesh, impl_->options);
}

FeatureAnalysis detectFeatureCurves(const Mesh& mesh, const FeatureOptions& options) {
  return FeatureDetectionPipeline{}.run(mesh, options);
}

DirectionalCurveError measureLoopAgainstCircle(const Mesh& mesh,
                                               const FeatureLoop& loop,
                                               const Vec3& center, const Vec3& normalIn,
                                               double radius) {
  return primitive_fit_detail::measureLoopAgainstCircle(mesh, loop, center, normalIn,
                                                        radius);
}

std::string featureReportHeaderCsv() {
  return "loop_id,vertices,edges,closed,primitive,circular,mostly_boundary,cx,cy,cz,"
         "nx,ny,nz,major_axis_x,major_axis_y,major_axis_z,minor_axis_x,"
         "minor_axis_y,minor_axis_z,radius,major_radius,minor_radius,axis_ratio,"
         "rms_radial,max_radial,rms_ellipse,max_ellipse,rms_plane,max_plane,"
         "convex_edges,concave_edges,unknown_signed_edges";
}

std::string featureLoopRowCsv(const FeatureLoop& loop) {
  std::ostringstream out;
  out << std::setprecision(12);
  out << loop.id << "," << loop.vertices.size() << "," << loop.edgeCount << ","
      << (loop.closed ? 1 : 0) << "," << toString(loop.primitive) << ","
      << (loop.circular ? 1 : 0) << "," << (loop.mostlyBoundary ? 1 : 0) << ","
      << loop.center.x() << "," << loop.center.y() << "," << loop.center.z() << ","
      << loop.normal.x() << "," << loop.normal.y() << "," << loop.normal.z() << ","
      << loop.majorAxis.x() << "," << loop.majorAxis.y() << "," << loop.majorAxis.z()
      << "," << loop.minorAxis.x() << "," << loop.minorAxis.y() << ","
      << loop.minorAxis.z() << "," << loop.radius << "," << loop.majorRadius << ","
      << loop.minorRadius << "," << loop.axisRatio << "," << loop.rmsRadialError << ","
      << loop.maxRadialError << "," << loop.rmsEllipseError << ","
      << loop.maxEllipseError << "," << loop.rmsPlaneError << "," << loop.maxPlaneError
      << "," << loop.convexEdges << "," << loop.concaveEdges << ","
      << loop.unknownSignedEdges;
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
