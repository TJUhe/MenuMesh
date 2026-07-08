#include "detail/SimplificationRun.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "common/detail/MeshQueries.h"
#include "detail/DynamicTopology.h"
#include "detail/FeatureConstraints.h"
#include "detail/Quadrics.h"
#include "detail/ResultBuilder.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>

namespace manumesh::simplification {

SimplificationRun::SimplificationRun(const Mesh& input, const SimplifyOptions& options)
    : input_(input), options_(options),
      policies_(SimplificationPolicies::fromOptions(options)), quadrics_(options),
      featurePolicy_(options) {
}

Mesh SimplificationRun::execute(SimplifyReport* outReport) {
  initializeReport();
  analyzeFeatures();
  initializeVertices();
  initializeFaces();
  initializeBudget();
  rebuildQueue();
  collapseUntilTarget();

  Mesh result = compactResult(vertices_, faces_);
  report_.finalVertices = static_cast<int>(result.vertices.size());
  report_.finalFaces = static_cast<int>(result.faces.size());
  if (outReport) {
    *outReport = report_;
  }
  return result;
}

void SimplificationRun::initializeReport() {
  report_ = SimplifyReport{};
  report_.initialVertices = static_cast<int>(input_.vertices.size());
  report_.initialFaces = static_cast<int>(input_.faces.size());
}

void SimplificationRun::analyzeFeatures() {
  featureAnalysis_ = feature::FeatureAnalysis{};
  featureAnalysisPtr_ = nullptr;
  if (!policies_.features.enabled) {
    return;
  }

  const feature::FeatureOptions featureOptions = policies_.features.toFeatureOptions();
  featureAnalysis_ = feature::detectFeatureCurves(input_, featureOptions);
  featureAnalysisPtr_ = &featureAnalysis_;
  report_.featureLoops = static_cast<int>(featureAnalysis_.loops.size());
  report_.normalTensorFeatureEdges = featureAnalysis_.normalTensorFeatureEdges;
  for (const feature::FeatureLoop& loop : featureAnalysis_.loops) {
    if (loop.circular) {
      ++report_.circularFeatureLoops;
    }
  }
  for (const feature::VertexFeature& vertex : featureAnalysis_.vertices) {
    if (vertex.isFeature) {
      ++report_.featureVertices;
    }
  }
  initializeFeatureCurveConstraints();
}

void SimplificationRun::initializeFeatureCurveConstraints() {
  featureCurves_.clear();
  featureCurves_.resize(featureAnalysis_.loops.size());
  for (const feature::FeatureLoop& loop : featureAnalysis_.loops) {
    if (loop.id < 0 || loop.id >= static_cast<int>(featureCurves_.size())) {
      continue;
    }
    FeatureCurveConstraint constraint;
    constraint.valid = loop.vertices.size() >= 2;
    constraint.closed = loop.closed;
    constraint.primitive = loop.primitive;
    constraint.samples.reserve(loop.vertices.size());
    for (int vertexId : loop.vertices) {
      if (vertexId >= 0 && vertexId < static_cast<int>(input_.vertices.size())) {
        constraint.samples.push_back(input_.vertices[vertexId]);
      }
    }
    constraint.valid = constraint.valid && constraint.samples.size() >= 2;
    featureCurves_[loop.id] = std::move(constraint);
  }
}

void SimplificationRun::initializeVertices() {
  const std::vector<Mat4> initialQuadrics =
      quadrics_.build(input_, featureAnalysisPtr_, report_);
  boundaryVertices_ = policies_.legality.preserveBoundary
                          ? detail::computeBoundaryVertices(input_)
                          : std::vector<char>();
  vertices_.assign(input_.vertices.size(), VertexState{});
  for (int i = 0; i < static_cast<int>(input_.vertices.size()); ++i) {
    vertices_[i].p = input_.vertices[i];
    vertices_[i].q = initialQuadrics[i];
    vertices_[i].isBoundary =
        i < static_cast<int>(boundaryVertices_.size()) && boundaryVertices_[i] != 0;
    initializeVertexFeature(i);
  }

  activeLoopCounts_.clear();
  if (!featureAnalysisPtr_) {
    return;
  }
  activeLoopCounts_.assign(featureAnalysisPtr_->loops.size(), 0);
  for (const VertexState& vertex : vertices_) {
    if (vertex.isFeature && vertex.featureLoopId >= 0 &&
        vertex.featureLoopId < static_cast<int>(activeLoopCounts_.size())) {
      ++activeLoopCounts_[vertex.featureLoopId];
    }
  }
}

void SimplificationRun::initializeVertexFeature(int vertexId) {
  if (!featureAnalysisPtr_ ||
      vertexId >= static_cast<int>(featureAnalysisPtr_->vertices.size())) {
    return;
  }
  const feature::VertexFeature& vf = featureAnalysisPtr_->vertices[vertexId];
  VertexState& vertex = vertices_[vertexId];
  vertex.isFeature = vf.isFeature;
  vertex.circularFeature = vf.circular;
  vertex.featureJunction = vf.junction;
  vertex.featurePrimitive = vf.primitive;
  vertex.featureLoopId = vf.loopId;
  vertex.curveTangent = vf.tangent;
  vertex.circleCenter = vf.circleCenter;
  vertex.circleNormal = vf.circleNormal;
  vertex.circleRadius = vf.circleRadius;
  vertex.ellipseCenter = vf.ellipseCenter;
  vertex.ellipseNormal = vf.ellipseNormal;
  vertex.ellipseMajorAxis = vf.ellipseMajorAxis;
  vertex.ellipseMinorAxis = vf.ellipseMinorAxis;
  vertex.ellipseMajorRadius = vf.ellipseMajorRadius;
  vertex.ellipseMinorRadius = vf.ellipseMinorRadius;
}

void SimplificationRun::initializeFaces() {
  faces_.assign(input_.faces.size(), FaceState{});
  for (int i = 0; i < static_cast<int>(input_.faces.size()); ++i) {
    faces_[i].v = input_.faces[i].v;
  }
  topology_ =
      std::make_unique<DynamicTopology>(faces_, static_cast<int>(vertices_.size()));
  activeFaceCount_ = static_cast<int>(faces_.size());
  if (policies_.legality.preventLocalIntersections) {
    spatialIndex_.rebuild(faces_, vertices_);
  }
}

void SimplificationRun::initializeBudget() {
  targetFaces_ =
      policies_.target.resolveTargetFaceCount(static_cast<int>(input_.faces.size()));
  const double diag = std::max(1e-12, input_.bboxDiag());
  areaEps_ = diag * diag * 1e-18;
  minNormalDot_ = policies_.legality.resolveMinNormalDot();
  maxLocalError_ = policies_.legality.resolveMaxLocalError(diag);

  const int initialActiveEdgeCount =
      static_cast<int>(collectActiveEdges(faces_).size());
  maxAttemptsWithoutCollapse_ = std::max(1000, std::max(1, initialActiveEdgeCount) * 6);
  attemptsWithoutCollapse_ = 0;
  stalePops_ = 0;
}

void SimplificationRun::rebuildQueue() {
  queue_.clear();
  for (const auto& [a, b] : collectActiveEdges(faces_)) {
    queue_.pushEdge(a, b, vertices_);
  }
  ++report_.queueRebuilds;
}

void SimplificationRun::collapseUntilTarget() {
  if (activeFaceCount_ <= targetFaces_) {
    report_.terminationReason = report_.collapsedEdges > 0
                                    ? SimplifyTerminationReason::ReachedTarget
                                    : SimplifyTerminationReason::AlreadyAtOrBelowTarget;
    return;
  }

  while (activeFaceCount_ > targetFaces_) {
    if (!ensureQueueHasCandidates()) {
      report_.terminationReason = SimplifyTerminationReason::NoCandidates;
      break;
    }

    const Candidate candidate = queue_.pop();
    if (!isCurrentCandidate(candidate)) {
      handleStaleCandidate();
      continue;
    }
    stalePops_ = 0;

    if (!tryCollapse(candidate.a, candidate.b)) {
      if (attemptsWithoutCollapse_ > maxAttemptsWithoutCollapse_) {
        report_.terminationReason = SimplifyTerminationReason::RejectionLimit;
        break;
      }
      continue;
    }
    attemptsWithoutCollapse_ = 0;

    if (options_.verbose && report_.collapsedEdges % 1000 == 0) {
      std::cerr << "collapsed " << report_.collapsedEdges << ", faces "
                << activeFaceCount_ << "/" << targetFaces_ << "\n";
    }
  }

  if (activeFaceCount_ <= targetFaces_) {
    report_.terminationReason = SimplifyTerminationReason::ReachedTarget;
  } else if (report_.terminationReason == SimplifyTerminationReason::NotStarted) {
    report_.terminationReason = SimplifyTerminationReason::NoCandidates;
  }
}

bool SimplificationRun::ensureQueueHasCandidates() {
  if (!queue_.empty()) {
    return true;
  }
  rebuildQueue();
  return !queue_.empty();
}

bool SimplificationRun::isCurrentCandidate(const Candidate& candidate) const {
  const int a = candidate.a;
  const int b = candidate.b;
  return a >= 0 && b >= 0 && a < static_cast<int>(vertices_.size()) &&
         b < static_cast<int>(vertices_.size()) && vertices_[a].active &&
         vertices_[b].active && vertices_[a].version == candidate.versionA &&
         vertices_[b].version == candidate.versionB &&
         areAdjacent(a, b, faces_, *topology_);
}

void SimplificationRun::handleStaleCandidate() {
  if (++stalePops_ > 10000) {
    rebuildQueue();
    stalePops_ = 0;
  }
}

bool SimplificationRun::tryCollapse(int keep, int remove) {
  const CollapseEdge edge{keep, remove};
  const Mat4 mergedQ = vertices_[keep].q + vertices_[remove].q;
  const std::vector<SolveResult> placements =
      solvePlacementCandidates(mergedQ, vertices_[keep].p, vertices_[remove].p);
  if (!placements.empty() && placements.front().usedFallback) {
    ++report_.solverFallbacks;
  }

  const CollapseAttemptResult result = evaluateCollapseAttempt(
      {edge, mergedQ, placements, options_, policies_, vertices_, faces_, *topology_,
       activeLoopCounts_, featureCurves_, featurePolicy_,
       policies_.legality.preventLocalIntersections ? &spatialIndex_ : nullptr,
       input_.bboxDiag(), areaEps_, minNormalDot_, maxLocalError_});
  if (result.accepted()) {
    applyCollapse(edge.keep, edge.remove, result.acceptedPosition, mergedQ);
    if (result.projected) {
      ++report_.projectedFeaturePlacements;
    }
    return true;
  }

  recordRejectedCollapse(result);
  return false;
}

void SimplificationRun::recordRejectedCollapse(const CollapseAttemptResult& result) {
  ++report_.rejectedCollapses;

  const char* message = "constraints leave no valid collapses";
  switch (result.status) {
  case CollapseAttemptStatus::Accepted:
    break;
  case CollapseAttemptStatus::FeatureRejected:
    ++report_.featureRejectedCollapses;
    if (result.featureRejectKind == FeatureCollapseRejectKind::Primitive) {
      ++report_.primitiveFeatureRejectedCollapses;
    } else if (result.featureRejectKind == FeatureCollapseRejectKind::Generic) {
      ++report_.genericFeatureRejectedCollapses;
    }
    message = "feature constraints leave no valid collapses";
    break;
  case CollapseAttemptStatus::BoundaryRejected:
    ++report_.boundaryRejectedCollapses;
    message = "boundary constraints leave no valid collapses";
    break;
  case CollapseAttemptStatus::CurveBudgetRejected:
    ++report_.curveBudgetRejectedCollapses;
    message = "feature curve budgets leave no valid collapses";
    break;
  case CollapseAttemptStatus::LegalityRejected:
    switch (result.legalityReason) {
    case CollapseRejectReason::Topology:
      ++report_.topologyRejectedCollapses;
      break;
    case CollapseRejectReason::NormalFlip:
      ++report_.normalFlipRejectedCollapses;
      break;
    case CollapseRejectReason::TriangleQuality:
      ++report_.qualityRejectedCollapses;
      break;
    case CollapseRejectReason::SelfIntersection:
      ++report_.selfIntersectionRejectedCollapses;
      break;
    case CollapseRejectReason::LocalError:
      ++report_.errorRejectedCollapses;
      break;
    case CollapseRejectReason::None:
      break;
    }
    message = "legality checks leave no valid collapses";
    break;
  }
  if (++attemptsWithoutCollapse_ > maxAttemptsWithoutCollapse_ && options_.verbose) {
    std::cerr << "stopped: " << message << "\n";
  }
}

void SimplificationRun::bumpVersions(int keep, int remove) {
  vertices_[keep].version++;
  vertices_[remove].version++;
}

void SimplificationRun::applyCollapse(int keep, int remove, const Vec3& position,
                                      const Mat4& mergedQ) {
  const bool mergedFeatureLoop =
      vertices_[keep].isFeature && vertices_[remove].isFeature &&
      vertices_[keep].featureLoopId == vertices_[remove].featureLoopId &&
      vertices_[keep].featureLoopId >= 0 &&
      vertices_[keep].featureLoopId < static_cast<int>(activeLoopCounts_.size());

  const std::unordered_set<int> affectedFaces =
      collectAffectedFacesForCollapse(keep, remove);
  if (policies_.legality.preventLocalIntersections) {
    for (int faceId : affectedFaces) {
      spatialIndex_.removeFace(faceId);
    }
  }

  vertices_[keep].p = position;
  vertices_[keep].q = mergedQ;
  refreshCircularTangent(vertices_[keep]);
  refreshEllipseTangent(vertices_[keep]);
  vertices_[remove].active = false;
  if (mergedFeatureLoop) {
    --activeLoopCounts_[vertices_[keep].featureLoopId];
  }
  bumpVersions(keep, remove);

  rewriteIncidentFaces(keep, remove);
  if (policies_.legality.preventLocalIntersections) {
    for (int faceId : affectedFaces) {
      if (faceId >= 0 && faceId < static_cast<int>(faces_.size()) &&
          faces_[faceId].active) {
        spatialIndex_.updateFace(faceId, faces_[faceId], vertices_);
      }
    }
  }
  ++report_.collapsedEdges;

  for (int neighbor : activeNeighborsOf(keep, faces_, vertices_, *topology_)) {
    queue_.pushEdge(keep, neighbor, vertices_);
  }
}

std::unordered_set<int>
SimplificationRun::collectAffectedFacesForCollapse(int keep, int remove) const {
  std::unordered_set<int> affected;
  if (keep >= 0 && keep < static_cast<int>(topology_->vertexFaces.size())) {
    affected.insert(topology_->vertexFaces[keep].begin(),
                    topology_->vertexFaces[keep].end());
  }
  if (remove >= 0 && remove < static_cast<int>(topology_->vertexFaces.size())) {
    affected.insert(topology_->vertexFaces[remove].begin(),
                    topology_->vertexFaces[remove].end());
  }
  return affected;
}

void SimplificationRun::rewriteIncidentFaces(int keep, int remove) {
  const std::vector<int> removeIncidentFaces(topology_->vertexFaces[remove].begin(),
                                             topology_->vertexFaces[remove].end());
  for (int faceId : removeIncidentFaces) {
    FaceState& face = faces_[faceId];
    if (!face.active || !containsVertex(face, remove)) {
      continue;
    }
    topology_->removeFace(faceId, face);
    for (int& id : face.v) {
      if (id == remove) {
        id = keep;
      }
    }
    if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2] ||
        topology_->hasDuplicateFace(faceId, face)) {
      face.active = false;
      --activeFaceCount_;
    } else {
      topology_->addFace(faceId, face);
    }
  }
}

} // namespace manumesh::simplification
