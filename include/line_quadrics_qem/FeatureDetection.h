#pragma once

#include "line_quadrics_qem/Export.h"
#include "line_quadrics_qem/Mesh.h"

#include <string>
#include <vector>

namespace lq {

/// Parameters for crease, boundary, and circular feature-loop detection.
struct FeatureOptions {
  double featureAngleDeg = 40.0;
  double circleFitRelativeThreshold = 0.05;
  int minFeatureLoopVertices = 8;
};

/// One connected feature curve or loop detected in the mesh.
struct FeatureLoop {
  int id = -1;
  std::vector<int> vertices;
  int edgeCount = 0;
  bool closed = false;
  bool circular = false;
  bool mostlyBoundary = false;
  Vec3 center = Vec3::Zero();
  Vec3 normal = Vec3(0.0, 0.0, 1.0);
  double radius = 0.0;
  double rmsRadialError = 0.0;
  double maxRadialError = 0.0;
  double rmsPlaneError = 0.0;
  double maxPlaneError = 0.0;
};

/// Per-vertex feature classification used by feature-preserving simplification.
struct VertexFeature {
  bool isFeature = false;
  bool circular = false;
  bool junction = false;
  int loopId = -1;
  Vec3 tangent = Vec3::Zero();
  Vec3 circleCenter = Vec3::Zero();
  Vec3 circleNormal = Vec3(0.0, 0.0, 1.0);
  double circleRadius = 0.0;
};

/// Full feature-detection result for a mesh.
struct FeatureAnalysis {
  std::vector<VertexFeature> vertices;
  std::vector<FeatureLoop> loops;
  int featureEdges = 0;
  int boundaryFeatureEdges = 0;
  int dihedralFeatureEdges = 0;
  int nonManifoldFeatureEdges = 0;
};

/// Error of a detected loop against a circular reference curve.
struct DirectionalCurveError {
  int samples = 0;
  double radialRms = 0.0;
  double radialMax = 0.0;
  double planeRms = 0.0;
  double planeMax = 0.0;
};

/// Detects boundary, non-manifold, dihedral, and circular feature curves.
LQ_API FeatureAnalysis detectFeatureCurves(const Mesh& mesh,
                                           const FeatureOptions& options);

/// Measures one detected loop against a supplied circle.
LQ_API DirectionalCurveError measureLoopAgainstCircle(const Mesh& mesh,
                                                      const FeatureLoop& loop,
                                                      const Vec3& center,
                                                      const Vec3& normal,
                                                      double radius);

/// CSV header for feature-loop reports.
LQ_API std::string featureReportHeaderCsv();
/// CSV row for one feature loop.
LQ_API std::string featureLoopRowCsv(const FeatureLoop& loop);

} // namespace lq
