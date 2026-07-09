#include "detail/FeatureGuidance.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "common/detail/MathConstants.h"
#include "common/detail/MeshQueries.h"
#include "detail/SimplificationPolicies.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace manumesh::simplification {
namespace {

using manumesh::detail::kPi;

FeatureCurveKind toFeatureCurveKind(feature::FeaturePrimitiveType primitive) {
  switch (primitive) {
  case feature::FeaturePrimitiveType::Unknown:
    return FeatureCurveKind::Unknown;
  case feature::FeaturePrimitiveType::Circle:
    return FeatureCurveKind::Circle;
  case feature::FeaturePrimitiveType::NearCircle:
    return FeatureCurveKind::NearCircle;
  case feature::FeaturePrimitiveType::Ellipse:
    return FeatureCurveKind::Ellipse;
  case feature::FeaturePrimitiveType::PolygonalLoop:
    return FeatureCurveKind::PolygonalLoop;
  }
  return FeatureCurveKind::Unknown;
}

FeatureVertexGuidance toVertexGuidance(const feature::VertexFeature& source) {
  FeatureVertexGuidance target;
  target.isFeature = source.isFeature;
  target.circular = source.circular;
  target.junction = source.junction;
  target.weakFeature = source.weakFeature;
  target.primitive = toFeatureCurveKind(source.primitive);
  target.loopId = source.loopId;
  target.componentId = source.componentId;
  target.confidence = source.confidence;
  target.tangent = source.tangent;
  target.circleCenter = source.circleCenter;
  target.circleNormal = source.circleNormal;
  target.circleRadius = source.circleRadius;
  target.ellipseCenter = source.ellipseCenter;
  target.ellipseNormal = source.ellipseNormal;
  target.ellipseMajorAxis = source.ellipseMajorAxis;
  target.ellipseMinorAxis = source.ellipseMinorAxis;
  target.ellipseMajorRadius = source.ellipseMajorRadius;
  target.ellipseMinorRadius = source.ellipseMinorRadius;
  return target;
}

int curveStorageSize(const feature::FeatureAnalysis& analysis) {
  int size = static_cast<int>(analysis.loops.size());
  for (const feature::FeatureLoop& loop : analysis.loops) {
    if (loop.id >= 0) {
      size = std::max(size, loop.id + 1);
    }
  }
  return size;
}

int resolveNormalTensorMinPersistentScales(const SimplifyOptions& options) {
  return std::clamp(options.normalTensorMinPersistentScales, 1,
                    std::max(1, options.normalTensorScaleCount));
}

void summarizeNormalTensorScores(const std::vector<feature::NormalTensorVertex>& tensor,
                                 FeatureWeightScores& result) {
  double localScaleSum = 0.0;
  double persistenceSum = 0.0;
  for (const feature::NormalTensorVertex& vertex : tensor) {
    result.maxNormalTensorPersistentScore =
        std::max(result.maxNormalTensorPersistentScore, vertex.persistentFeatureScore);
    if (vertex.featureScore <= 1e-12 && vertex.persistentFeatureScore <= 1e-12) {
      continue;
    }
    ++result.normalTensorScoredVertices;
    localScaleSum += vertex.localScale;
    persistenceSum += static_cast<double>(vertex.persistentScales);
  }

  if (result.normalTensorScoredVertices > 0) {
    const double count = static_cast<double>(result.normalTensorScoredVertices);
    result.meanNormalTensorLocalScale = localScaleSum / count;
    result.meanNormalTensorPersistence = persistenceSum / count;
  }
}

} // namespace

FeatureGuidance buildFeatureGuidance(const Mesh& mesh,
                                     const FeatureDetectionPolicy& policy) {
  FeatureGuidance guidance;
  if (!policy.enabled) {
    return guidance;
  }

  guidance.enabled = true;
  const feature::FeatureAnalysis analysis =
      feature::detectFeatureCurves(mesh, policy.options);

  guidance.summary.featureLoops = static_cast<int>(analysis.loops.size());
  guidance.summary.tracedFeatureEdges = analysis.tracedFeatureEdges;
  guidance.summary.untracedFeatureEdges = analysis.untracedFeatureEdges;
  guidance.summary.normalTensorFeatureEdges = analysis.normalTensorFeatureEdges;
  guidance.summary.normalTensorScoredVertices = analysis.normalTensorScoredVertices;
  guidance.summary.featureComponents = static_cast<int>(analysis.components.size());
  guidance.summary.weakFeatureComponents = analysis.weakFeatureComponents;
  guidance.summary.highConfidenceFeatureComponents =
      analysis.highConfidenceFeatureComponents;
  guidance.summary.graphCleanupBridgedGaps = analysis.graphCleanupBridgedGaps;
  guidance.summary.graphCleanupRemovedSpurs = analysis.graphCleanupRemovedSpurs;
  guidance.summary.graphCleanupMergedJunctions = analysis.graphCleanupMergedJunctions;
  guidance.summary.maxNormalTensorPersistentScore =
      analysis.maxNormalTensorPersistentScore;
  guidance.summary.meanNormalTensorLocalScale = analysis.meanNormalTensorLocalScale;
  guidance.summary.meanNormalTensorPersistence = analysis.meanNormalTensorPersistence;
  guidance.summary.meanFeatureComponentConfidence =
      analysis.meanFeatureComponentConfidence;
  guidance.summary.minFeatureComponentConfidence =
      analysis.minFeatureComponentConfidence;

  guidance.vertices.reserve(analysis.vertices.size());
  for (const feature::VertexFeature& vertex : analysis.vertices) {
    guidance.vertices.push_back(toVertexGuidance(vertex));
    if (vertex.isFeature) {
      ++guidance.summary.featureVertices;
    }
  }

  guidance.curves.resize(curveStorageSize(analysis));
  for (const feature::FeatureLoop& loop : analysis.loops) {
    if (loop.circular) {
      ++guidance.summary.circularFeatureLoops;
    }
    if (loop.id < 0 || loop.id >= static_cast<int>(guidance.curves.size())) {
      continue;
    }
    FeatureCurveConstraint constraint;
    constraint.valid = loop.vertices.size() >= 2;
    constraint.closed = loop.closed;
    constraint.primitive = toFeatureCurveKind(loop.primitive);
    constraint.samples.reserve(loop.vertices.size());
    for (int vertexId : loop.vertices) {
      if (vertexId >= 0 && vertexId < static_cast<int>(mesh.vertices.size())) {
        constraint.samples.push_back(mesh.vertices[vertexId]);
      }
    }
    constraint.valid = constraint.valid && constraint.samples.size() >= 2;
    guidance.curves[loop.id] = std::move(constraint);
  }

  return guidance;
}

FeatureWeightScores computeFeatureWeightScores(const Mesh& mesh,
                                               const SimplifyOptions& options) {
  const WeightMode mode = options.weightMode;
  FeatureWeightScores result;
  std::vector<double>& score = result.values;
  score.assign(mesh.vertices.size(), 0.0);
  if (mode == WeightMode::Uniform) {
    return result;
  }

  const Vec3 lo = mesh.bboxMin();
  const Vec3 hi = mesh.bboxMax();
  const Vec3 span = hi - lo;

  if (mode == WeightMode::Height) {
    const double denom = std::max(1e-12, span.z());
    for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
      score[i] = std::clamp((mesh.vertices[i].z() - lo.z()) / denom, 0.0, 1.0);
    }
    return result;
  }

  if (mode == WeightMode::XBand) {
    const double denom = std::max(1e-12, span.x());
    for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
      const double x = (mesh.vertices[i].x() - lo.x()) / denom;
      score[i] = std::exp(-80.0 * (x - 0.5) * (x - 0.5));
    }
    return result;
  }

  if (mode == WeightMode::NormalTensor) {
    const std::vector<feature::NormalTensorVertex> tensor =
        feature::computeNormalTensorFeatures(
            mesh, feature::NormalTensorOptions{options.normalTensorSmoothingIterations,
                                               options.normalTensorScaleCount});
    summarizeNormalTensorScores(tensor, result);
    const int requiredPersistentScales =
        resolveNormalTensorMinPersistentScales(options);
    for (int i = 0; i < static_cast<int>(tensor.size()); ++i) {
      if (tensor[i].persistentScales >= requiredPersistentScales) {
        score[i] = tensor[i].persistentFeatureScore;
      }
    }
    return result;
  }

  const std::vector<Vec3> faceNormals = detail::computeFaceNormals(mesh);
  const detail::MeshEdgeInfoMap edgeInfo = detail::buildMeshEdgeInfo(mesh);
  const double threshold = options.featureAngleDeg * kPi / 180.0;
  const double denom = std::max(1e-12, kPi - threshold);
  for (const auto& [key, info] : edgeInfo) {
    double edgeScore = 0.0;
    if (info.faces.size() == 1) {
      edgeScore = 1.0;
    } else if (info.faces.size() == 2) {
      const double dot = std::clamp(
          std::abs(faceNormals[info.faces[0]].dot(faceNormals[info.faces[1]])), -1.0,
          1.0);
      const double angle = std::acos(dot);
      edgeScore = std::clamp((angle - threshold) / denom, 0.0, 1.0);
    }
    if (edgeScore > 0.0) {
      const auto [a, b] = detail::unpackMeshEdgeKey(key);
      score[a] = std::max(score[a], edgeScore);
      score[b] = std::max(score[b], edgeScore);
    }
  }
  return result;
}

void applyFeatureGuidanceSummary(const FeatureGuidanceSummary& summary,
                                 SimplifyReport& report) {
  report.featureLoops = summary.featureLoops;
  report.circularFeatureLoops = summary.circularFeatureLoops;
  report.featureVertices = summary.featureVertices;
  report.tracedFeatureEdges = summary.tracedFeatureEdges;
  report.untracedFeatureEdges = summary.untracedFeatureEdges;
  report.normalTensorFeatureEdges = summary.normalTensorFeatureEdges;
  report.normalTensorScoredVertices = summary.normalTensorScoredVertices;
  report.featureComponents = summary.featureComponents;
  report.weakFeatureComponents = summary.weakFeatureComponents;
  report.highConfidenceFeatureComponents = summary.highConfidenceFeatureComponents;
  report.graphCleanupBridgedGaps = summary.graphCleanupBridgedGaps;
  report.graphCleanupRemovedSpurs = summary.graphCleanupRemovedSpurs;
  report.graphCleanupMergedJunctions = summary.graphCleanupMergedJunctions;
  report.maxNormalTensorPersistentScore = summary.maxNormalTensorPersistentScore;
  report.meanNormalTensorLocalScale = summary.meanNormalTensorLocalScale;
  report.meanNormalTensorPersistence = summary.meanNormalTensorPersistence;
  report.meanFeatureComponentConfidence = summary.meanFeatureComponentConfidence;
  report.minFeatureComponentConfidence = summary.minFeatureComponentConfidence;
}

} // namespace manumesh::simplification
