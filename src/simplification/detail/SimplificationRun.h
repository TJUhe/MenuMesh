#pragma once

#include "detail/CandidateQueue.h"
#include "detail/DynamicTopology.h"
#include "detail/FeatureConstraints.h"
#include "detail/Quadrics.h"
#include "detail/SpatialFaceIndex.h"
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"
#include "line_quadrics_qem/features/FeatureDetection.h"

#include <memory>
#include <unordered_set>
#include <vector>

namespace lq {

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
  bool acceptFirstLegalPlacement(const CollapseEdge& edge, const Mat4& mergedQ,
                                 const std::vector<SolveResult>& placements,
                                 const BoundaryCollapseDecision& boundaryDecision,
                                 bool tryFallbackPlacements,
                                 CollapseRejectReason& firstRejectReason,
                                 bool& sawCurveBudgetReject);
  void rejectFeatureCollapse(int keep, int remove, FeatureCollapseRejectKind kind);
  void rejectBoundaryCollapse(int keep, int remove);
  void rejectCurveBudgetCollapse(int keep, int remove);
  bool curveBudgetAllows(int keep, int remove, const Vec3& position) const;
  void rejectLegalityCollapse(int keep, int remove, CollapseRejectReason reason);
  void bumpVersions(int keep, int remove);
  void applyCollapse(int keep, int remove, const Vec3& position, const Mat4& mergedQ);
  std::unordered_set<int> collectAffectedFacesForCollapse(int keep, int remove) const;
  void rewriteIncidentFaces(int keep, int remove);

  const Mesh& input_;
  const SimplifyOptions& options_;
  SimplifyReport report_;
  FeatureAnalysis featureAnalysis_;
  const FeatureAnalysis* featureAnalysisPtr_ = nullptr;
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

} // namespace lq
