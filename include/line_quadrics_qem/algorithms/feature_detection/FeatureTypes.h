#pragma once

#include "line_quadrics_qem/core/Mesh.h"

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
///
/// The detector is tuned for CAD/STL-style meshes first: explicit boundary and
/// dihedral evidence form the feature graph, while tensor evidence is a
/// secondary signal for weak creases. Thresholds should therefore be chosen
/// against the mesh scale and provenance instead of reused blindly across
/// scanned/noisy and clean CAD inputs.
struct FeatureOptions {
  /// Dihedral angle threshold for hard feature edges, in degrees.
  double featureAngleDeg = 40.0;
  /// Relative radial tolerance used when validating circular loops.
  double circleFitRelativeThreshold = 0.05;
  /// Relative residual tolerance used when validating elliptical loops.
  double ellipseFitRelativeThreshold = 0.05;
  /// Axis-ratio tolerance below which an ellipse is treated as near-circular.
  double nearCircleAxisRatioTolerance = 0.08;
  /// Minimum ordered vertices required before a traced curve is reported.
  int minFeatureLoopVertices = 8;
  /// Enables tensor-derived weak feature candidates in addition to graph edges.
  bool useNormalTensorFeatures = true;
  /// Minimum tensor saliency score for weak feature classification.
  double normalTensorFeatureThreshold = 0.16;
  /// Minimum edge/tangent alignment for accepting tensor-derived edge evidence.
  double normalTensorMinEdgeAlignment = 0.45;
  /// One-ring normal smoothing passes before tensor scoring.
  int normalTensorSmoothingIterations = 0;
  /// Number of tensor scales sampled for weak feature scoring.
  int normalTensorScaleCount = 1;
};

/// Parameters for Tsuchie-Higashi style normal-tensor feature scoring.
struct NormalTensorOptions {
  int smoothingIterations = 0;
  int scaleCount = 1;
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
  // Topology of the recovered chain/loop.
  int id = -1;
  std::vector<int> vertices;
  int edgeCount = 0;
  bool closed = false;
  bool circular = false;
  bool mostlyBoundary = false;

  // Primitive fit. Circle and near-circle use radius; ellipse uses major/minor.
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

  // Signed dihedral summary for the graph edges that make up this loop.
  int convexEdges = 0;
  int concaveEdges = 0;
  int unknownSignedEdges = 0;
};

/// Per-vertex feature classification used by feature-preserving simplification.
struct VertexFeature {
  // Ownership and graph role.
  bool isFeature = false;
  bool circular = false;
  bool junction = false;
  FeaturePrimitiveType primitive = FeaturePrimitiveType::Unknown;
  int loopId = -1;
  Vec3 tangent = Vec3::Zero();

  // Circle projection data for circular and near-circular feature vertices.
  Vec3 circleCenter = Vec3::Zero();
  Vec3 circleNormal = Vec3(0.0, 0.0, 1.0);
  double circleRadius = 0.0;

  // Ellipse projection data for fitted elliptical feature vertices.
  Vec3 ellipseCenter = Vec3::Zero();
  Vec3 ellipseNormal = Vec3(0.0, 0.0, 1.0);
  Vec3 ellipseMajorAxis = Vec3(1.0, 0.0, 0.0);
  Vec3 ellipseMinorAxis = Vec3(0.0, 1.0, 0.0);
  double ellipseMajorRadius = 0.0;
  double ellipseMinorRadius = 0.0;
};

/// One edge in the explicit feature graph.
struct FeatureGraphEdge {
  int a = -1;
  int b = -1;
  bool boundary = false;
  bool dihedral = false;
  bool normalTensor = false;
  bool nonManifold = false;
  int signedKind = 0;
};

/// Per-vertex ownership in the explicit feature graph.
struct FeatureGraphVertex {
  std::vector<int> incidentEdges;
  std::vector<int> loopIds;
  bool junction = false;
  bool shared = false;
};

/// Explicit graph view of detected feature edges and recovered loops.
struct FeatureGraph {
  std::vector<FeatureGraphEdge> edges;
  std::vector<FeatureGraphVertex> vertices;
  std::vector<int> junctionVertices;
  std::vector<int> sharedVertices;
};

/// Full feature-detection result for a mesh.
///
/// Counts distinguish the evidence source used to build the explicit graph.
/// Downstream algorithms should prefer `loops` and `vertices` for feature
/// ownership, and use the counters for diagnostics and policy validation.
struct FeatureAnalysis {
  std::vector<VertexFeature> vertices;
  std::vector<FeatureLoop> loops;
  FeatureGraph graph;
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

} // namespace lq
