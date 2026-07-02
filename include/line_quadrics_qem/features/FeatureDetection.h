#pragma once

#include "line_quadrics_qem/Export.h"
#include "line_quadrics_qem/core/Mesh.h"

#include <string>
#include <vector>

namespace lq {

/// Fitted primitive type for one detected feature loop.
enum class FeaturePrimitiveType {
  Unknown,
  Circle,
  NearCircle,
  Ellipse,
  PolygonalLoop,
};

/// Parameters for crease, boundary, and feature-loop detection.
struct FeatureOptions {
  double featureAngleDeg = 40.0;
  double circleFitRelativeThreshold = 0.05;
  double ellipseFitRelativeThreshold = 0.05;
  double nearCircleAxisRatioTolerance = 0.08;
  int minFeatureLoopVertices = 8;
  bool useNormalTensorFeatures = true;
  double normalTensorFeatureThreshold = 0.16;
  double normalTensorMinEdgeAlignment = 0.45;
  int normalTensorSmoothingIterations = 0;
};

/// Parameters for Tsuchie-Higashi style normal-tensor feature scoring.
struct NormalTensorOptions {
  int smoothingIterations = 0;
};

/// Per-vertex normal-tensor decomposition and feature saliency.
struct NormalTensorVertex {
  Vec3 normal = Vec3(0.0, 0.0, 1.0);
  Vec3 creaseTangent = Vec3(1.0, 0.0, 0.0);
  double surfaceSaliency = 0.0;
  double creaseSaliency = 0.0;
  double cornerSaliency = 0.0;
  double featureScore = 0.0;
};

/// One connected feature curve or loop detected in the mesh.
struct FeatureLoop {
  int id = -1;
  std::vector<int> vertices;
  int edgeCount = 0;
  bool closed = false;
  bool circular = false;
  bool mostlyBoundary = false;
  FeaturePrimitiveType primitive = FeaturePrimitiveType::Unknown;
  Vec3 center = Vec3::Zero();
  Vec3 normal = Vec3(0.0, 0.0, 1.0);
  Vec3 majorAxis = Vec3(1.0, 0.0, 0.0);
  Vec3 minorAxis = Vec3(0.0, 1.0, 0.0);
  double radius = 0.0;
  double majorRadius = 0.0;
  double minorRadius = 0.0;
  double axisRatio = 0.0;
  double rmsRadialError = 0.0;
  double maxRadialError = 0.0;
  double rmsEllipseError = 0.0;
  double maxEllipseError = 0.0;
  double rmsPlaneError = 0.0;
  double maxPlaneError = 0.0;
  int convexEdges = 0;
  int concaveEdges = 0;
  int unknownSignedEdges = 0;
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
  int normalTensorFeatureEdges = 0;
  int nonManifoldFeatureEdges = 0;
  int convexFeatureEdges = 0;
  int concaveFeatureEdges = 0;
  int unknownSignedFeatureEdges = 0;
  double maxNormalTensorFeatureScore = 0.0;
};

/// Error of a detected loop against a circular reference curve.
struct DirectionalCurveError {
  int samples = 0;
  double radialRms = 0.0;
  double radialMax = 0.0;
  double planeRms = 0.0;
  double planeMax = 0.0;
};

/// Computes local normal-tensor scores from one-ring face normals.
LQ_API std::vector<NormalTensorVertex>
computeNormalTensorFeatures(const Mesh& mesh, const NormalTensorOptions& options = {});

/// Detects boundary, non-manifold, dihedral, tensor, and circular feature curves.
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
/// Stable string name for a fitted feature primitive.
LQ_API std::string toString(FeaturePrimitiveType primitive);

} // namespace lq
