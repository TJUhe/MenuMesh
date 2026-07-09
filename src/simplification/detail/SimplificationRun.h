#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "detail/CandidateQueue.h"
#include "detail/CollapseAttempt.h"
#include "detail/DynamicTopology.h"
#include "detail/FeatureConstraints.h"
#include "detail/Quadrics.h"
#include "detail/SimplificationPolicies.h"
#include "detail/SpatialFaceIndex.h"

#include <memory>
#include <unordered_set>
#include <vector>

namespace manumesh::simplification {

class SimplificationRun {
public:
  SimplificationRun(const Mesh& input, const SimplifyOptions& options);

  Mesh execute(SimplifyReport* outReport);

private:
  void initializeReport();
  void analyzeFeatures();
  void initializeFeatureCurveConstraints();
  void initializeVertices();
  void initializeVertexFeature(int vertexId);
  void initializeFaces();
  void initializeBudget();
  void rebuildQueue();
  void collapseUntilTarget();
  bool ensureQueueHasCandidates();
  bool isCurrentCandidate(const Candidate& candidate) const;
  void handleStaleCandidate();
  bool tryCollapse(int keep, int remove);
  void recordRejectedCollapse(const CollapseAttemptResult& result);
  void bumpVersions(int keep, int remove);
  void applyCollapse(int keep, int remove, const Vec3& position, const Mat4& mergedQ);
  std::unordered_set<int> collectAffectedFacesForCollapse(int keep, int remove) const;
  void rewriteIncidentFaces(int keep, int remove);

  const Mesh& input_;
  const SimplifyOptions& options_;
  SimplificationPolicies policies_;
  SimplifyReport report_;
  feature::FeatureAnalysis featureAnalysis_;
  const feature::FeatureAnalysis* featureAnalysisPtr_ = nullptr;
  std::vector<char> boundaryVertices_;
  std::vector<VertexState> vertices_;
  std::vector<FaceState> faces_;
  std::unique_ptr<DynamicTopology> topology_;
  SpatialFaceIndex spatialIndex_;
  std::vector<int> activeLoopCounts_;
  std::vector<FeatureCurveConstraint> featureCurves_;
  CandidateQueue queue_;
  InitialQuadricBuilder quadrics_;
  FeatureConstraintPolicy featurePolicy_;
  int activeFaceCount_ = 0;
  int targetFaces_ = 0;
  double areaEps_ = 0.0;
  double minNormalDot_ = 0.0;
  double maxLocalError_ = 0.0;
  int maxAttemptsWithoutCollapse_ = 0;
  int attemptsWithoutCollapse_ = 0;
  int stalePops_ = 0;
};

} // namespace manumesh::simplification
