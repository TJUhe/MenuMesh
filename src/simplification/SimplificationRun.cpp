#include "detail/SimplificationRun.h"

#include "common/detail/MeshQueries.h"
#include "detail/CollapseTopology.h"
#include "detail/FeatureConstraints.h"
#include "detail/FeatureGuidance.h"
#include "detail/Quadrics.h"
#include "detail/QualityRefinement.h"
#include "mesh_edit/detail/MeshCompaction.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>

namespace manumesh::simplification {

SimplificationRun::SimplificationRun(const Mesh& input, const SimplifyOptions& options)
    : SimplificationRun(input, options, nullptr) {}

SimplificationRun::SimplificationRun(
    const Mesh& input, const SimplifyOptions& options, const feature::FeatureAnalysis* features
)
    : input_(input),
      options_(options),
      precomputedFeatures_(features),
      policies_(SimplificationPolicies::fromOptions(options)),
      quadrics_(options),
      featurePolicy_(options),
      textureProtection_(input, options) {}

Mesh SimplificationRun::execute(SimplifyReport* outReport) {
    initializeReport();
    analyzeFeatures();
    initializeVertices();
    initializeFaces();
    initializeBudget();
    rebuildQueue();
    collapseUntilTarget();
    refineQuality();

    std::vector<Vec3> positions;
    std::vector<char> activeVertices;
    positions.reserve(vertices_.size());
    activeVertices.reserve(vertices_.size());
    for (const VertexState& vertex : vertices_) {
        positions.push_back(vertex.p);
        activeVertices.push_back(vertex.active ? 1 : 0);
    }
    mesh_edit::MeshCompactionResult compacted = mesh_edit::compactActiveMesh(positions, activeVertices, faces_);
    if (!faceTexCoords_.empty()) {
        compacted.mesh.faceTexCoords.resize(compacted.mesh.faces.size());
        for (int oldFace = 0; oldFace < static_cast<int>(compacted.oldToNewFaces.size()); ++oldFace) {
            const int newFace = compacted.oldToNewFaces[oldFace];
            if (newFace >= 0 && oldFace < static_cast<int>(faceTexCoords_.size())) {
                compacted.mesh.faceTexCoords[newFace] = faceTexCoords_[oldFace];
            }
        }
    }
    Mesh result = std::move(compacted.mesh);
    report_.finalVertices = static_cast<int>(result.vertices.size());
    report_.finalFaces = static_cast<int>(result.faces.size());
    if (outReport) {
        *outReport = report_;
    }
    return result;
}

void SimplificationRun::refineQuality() {
    if (options_.qualityRefinementIterations <= 0 || textureProtection_.active()) {
        return;
    }
    runQualityRefinement(
        {options_,
         vertices_,
         faces_,
         *topology_,
         featurePolicy_,
         featureGuidance_.curves,
         primitiveFits_,
         policies_.legality.preventLocalIntersections ? &spatialIndex_ : nullptr,
         referenceSurface_.get(),
         meshDiagonal_,
         areaEps_,
         minNormalDot_,
         maxLocalError_},
        report_
    );
}

void SimplificationRun::initializeReport() {
    report_ = SimplifyReport{};
    report_.initialVertices = static_cast<int>(input_.vertices.size());
    report_.initialFaces = static_cast<int>(input_.faces.size());
    // Zero-area input faces are tolerated (lenient input validation); they
    // fall back to the small point quadric in computeInitialQuadrics and the
    // legality area checks keep them from spreading. Surface the count so
    // tolerated dirty input never goes unnoticed.
    report_.degenerateInputFaces = countDegenerateFaces(input_);
}

void SimplificationRun::analyzeFeatures() {
    featureGuidance_ = FeatureGuidance{};
    if (!policies_.features.enabled) {
        return;
    }

    featureGuidance_ = buildFeatureGuidance(input_, policies_.features, precomputedFeatures_);
    applyFeatureGuidanceSummary(featureGuidance_.summary, report_);
}

void SimplificationRun::initializeVertices() {
    const InitialQuadrics initialQuadrics = quadrics_.build(input_, featureGuidance_, report_);
    // Boundary flags are always computed (one O(E) pass) because the extended
    // link condition needs them to block boundary-chord pinches even when
    // preserveBoundary is off. preserveBoundary keeps its original meaning:
    // it only restricts how boundary vertices may move or merge.
    boundaryVertices_ = common::computeBoundaryVertices(input_);
    primitiveFits_.clear();
    vertices_.assign(input_.vertices.size(), VertexState{});
    for (int i = 0; i < static_cast<int>(input_.vertices.size()); ++i) {
        vertices_[i].p = input_.vertices[i];
        vertices_[i].q = initialQuadrics.quadrics[i];
        if (i < static_cast<int>(initialQuadrics.priorityScales.size())) {
            vertices_[i].priorityScale = initialQuadrics.priorityScales[i];
        }
        vertices_[i].isBoundary = i < static_cast<int>(boundaryVertices_.size()) && boundaryVertices_[i] != 0;
        initializeVertexFeature(i);
    }

    activeLoopCounts_.clear();
    if (!featureGuidance_.enabled) {
        return;
    }
    activeLoopCounts_.assign(featureGuidance_.curves.size(), 0);
    for (const VertexState& vertex : vertices_) {
        if (vertex.isFeature && vertex.featureLoopId >= 0 &&
            vertex.featureLoopId < static_cast<int>(activeLoopCounts_.size())) {
            ++activeLoopCounts_[vertex.featureLoopId];
        }
    }
}

void SimplificationRun::initializeVertexFeature(int vertexId) {
    if (!featureGuidance_.enabled || vertexId >= static_cast<int>(featureGuidance_.vertices.size())) {
        return;
    }
    const FeatureVertexGuidance& vf = featureGuidance_.vertices[vertexId];
    VertexState& vertex = vertices_[vertexId];
    vertex.isFeature = vf.isFeature;
    vertex.circularFeature = vf.circular;
    vertex.featureJunction = vf.junction;
    vertex.weakFeature = vf.weakFeature;
    vertex.featurePrimitive = vf.primitive;
    vertex.featureLoopId = vf.loopId;
    vertex.featureComponentId = vf.componentId;
    vertex.featureConfidence = vf.confidence;
    vertex.curveTangent = vf.tangent;
    // Circle/ellipse fit payloads go to the compact side table; vertices
    // without a fitted primitive stay at primitiveFitId == -1 and cost zero
    // extra memory.
    const bool hasCircleFit = vf.circular || vf.circleRadius > 0.0;
    const bool hasEllipseFit = vf.primitive == FeatureCurveKind::Ellipse || vf.ellipseMajorRadius > 0.0;
    if (vf.isFeature && (hasCircleFit || hasEllipseFit)) {
        FeaturePrimitiveFit fit;
        fit.circleCenter = vf.circleCenter;
        fit.circleNormal = vf.circleNormal;
        fit.circleRadius = vf.circleRadius;
        fit.ellipseCenter = vf.ellipseCenter;
        fit.ellipseNormal = vf.ellipseNormal;
        fit.ellipseMajorAxis = vf.ellipseMajorAxis;
        fit.ellipseMinorAxis = vf.ellipseMinorAxis;
        fit.ellipseMajorRadius = vf.ellipseMajorRadius;
        fit.ellipseMinorRadius = vf.ellipseMinorRadius;
        vertex.primitiveFitId = static_cast<int>(primitiveFits_.size());
        primitiveFits_.push_back(fit);
    }
}

void SimplificationRun::initializeFaces() {
    faces_.assign(input_.faces.size(), FaceState{});
    for (int i = 0; i < static_cast<int>(input_.faces.size()); ++i) {
        faces_[i].v = input_.faces[i].v;
    }
    faceTexCoords_ = input_.faceTexCoords;
    topology_ = std::make_unique<DynamicTopology>(faces_, static_cast<int>(vertices_.size()));
    activeFaceCount_ = static_cast<int>(faces_.size());
    if (policies_.legality.preventLocalIntersections) {
        spatialIndex_.rebuild(faces_, vertices_);
    }
}

void SimplificationRun::initializeBudget() {
    targetFaces_ = policies_.target.resolveTargetFaceCount(static_cast<int>(input_.faces.size()));
    const double diag = std::max(1e-12, input_.bboxDiag());
    meshDiagonal_ = input_.bboxDiag();
    areaEps_ = diag * diag * 1e-18;
    minNormalDot_ = policies_.legality.resolveMinNormalDot();
    maxLocalError_ = policies_.legality.resolveMaxLocalError(diag);
    if (maxLocalError_ > 0.0) {
        referenceSurface_ = std::make_unique<manumesh::common::MeshDistanceIndex>(input_);
    }

    const int initialActiveEdgeCount = static_cast<int>(collectActiveEdges(faces_).size());
    maxAttemptsWithoutCollapse_ = std::max(1000, std::max(1, initialActiveEdgeCount) * 6);
    attemptsWithoutCollapse_ = 0;
    stalePops_ = 0;
}

void SimplificationRun::rebuildQueue() {
    queue_.clear();
    int textureProtectedEdges = 0;
    for (const auto& [a, b] : collectActiveEdges(faces_)) {
        if (pushEdgeCandidate(a, b)) {
            ++textureProtectedEdges;
        }
    }
    // The initial queue construction is not a rebuild; only count rebuilds
    // triggered later by refills or stale-candidate recovery. The initial
    // build also doubles as the textureProtectedEdges census: it reuses the
    // per-placement texture evaluations instead of a separate O(E) pass.
    if (queueBuiltOnce_) {
        ++report_.queueRebuilds;
    } else {
        report_.textureProtectedEdges = textureProtectedEdges;
    }
    queueBuiltOnce_ = true;
}

bool SimplificationRun::pushEdgeCandidate(int a, int b) {
    if (a == b) {
        return false;
    }
    // Canonical endpoint order keeps the cached placement list identical to
    // what the collapse attempt would have solved at pop time.
    const int first = std::min(a, b);
    const int second = std::max(a, b);
    if (!vertices_[first].active || !vertices_[second].active) {
        return false;
    }
    const Mat4 q = vertices_[first].q + vertices_[second].q;
    const std::vector<SolveResult> placements = solvePlacementCandidates(q, vertices_[first].p, vertices_[second].p);

    double textureCost = 0.0;
    bool midpointProtected = false;
    if (textureProtection_.active()) {
        const Vec3 midpoint = 0.5 * (vertices_[first].p + vertices_[second].p);
        double bestCombinedCost = std::numeric_limits<double>::infinity();
        double bestMidpointDistance = std::numeric_limits<double>::infinity();
        for (const SolveResult& placement : placements) {
            const TextureCollapseEvaluation textureEvaluation = textureProtection_.evaluate(
                {first, second}, placement.position, faces_, vertices_, *topology_, faceTexCoords_
            );
            // The candidate list always contains the midpoint (or an endpoint
            // coincident with it), so the placement nearest to the midpoint
            // reproduces the previous midpoint-protection census exactly.
            const double midpointDistance = (placement.position - midpoint).squaredNorm();
            if (midpointDistance < bestMidpointDistance) {
                bestMidpointDistance = midpointDistance;
                midpointProtected = !textureEvaluation.allowed();
            }
            if (textureEvaluation.allowed()) {
                bestCombinedCost = std::min(bestCombinedCost, placement.cost + textureEvaluation.cost);
            }
        }
        if (!std::isfinite(bestCombinedCost)) {
            textureCost = std::numeric_limits<double>::infinity();
        } else {
            textureCost = std::max(0.0, bestCombinedCost - placements.front().cost);
        }
    }
    queue_.pushEdge(first, second, vertices_, placements, textureCost);
    return midpointProtected;
}

void SimplificationRun::collapseUntilTarget() {
    if (activeFaceCount_ <= targetFaces_) {
        report_.terminationReason = report_.collapsedEdges > 0 ? SimplifyTerminationReason::ReachedTarget
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

        if (!tryCollapse(candidate)) {
            if (attemptsWithoutCollapse_ > maxAttemptsWithoutCollapse_) {
                report_.terminationReason = SimplifyTerminationReason::RejectionLimit;
                break;
            }
            continue;
        }
        attemptsWithoutCollapse_ = 0;

        if (options_.verbose && report_.collapsedEdges % 1000 == 0) {
            std::cerr << "collapsed " << report_.collapsedEdges << ", faces " << activeFaceCount_ << "/" << targetFaces_
                      << "\n";
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
    return a >= 0 && b >= 0 && a < static_cast<int>(vertices_.size()) && b < static_cast<int>(vertices_.size()) &&
           vertices_[a].active && vertices_[b].active && vertices_[a].version == candidate.versionA &&
           vertices_[b].version == candidate.versionB && areAdjacent(a, b, faces_, *topology_);
}

void SimplificationRun::handleStaleCandidate() {
    if (++stalePops_ > 10000) {
        rebuildQueue();
        stalePops_ = 0;
    }
}

bool SimplificationRun::tryCollapse(const Candidate& candidate) {
    const int keep = candidate.a;
    const int remove = candidate.b;
    const CollapseEdge edge{keep, remove};
    const Mat4 mergedQ = vertices_[keep].q + vertices_[remove].q;
    // The version stamps validated by isCurrentCandidate guarantee the cached
    // placement solve is still exact, so no re-solve happens here.
    const SolveResult* placements = candidate.placements.data();
    const int placementCount = candidate.placementCount;
    if (placementCount > 0 && placements[0].usedFallback) {
        ++report_.solverFallbacks;
    }

    const CollapseAttemptResult result = evaluateCollapseAttempt(
        {edge,
         mergedQ,
         placements,
         placementCount,
         options_,
         policies_,
         vertices_,
         faces_,
         *topology_,
         activeLoopCounts_,
         featureGuidance_.curves,
         primitiveFits_,
         featurePolicy_,
         textureProtection_,
         faceTexCoords_,
         policies_.legality.preventLocalIntersections ? &spatialIndex_ : nullptr,
         referenceSurface_.get(),
         meshDiagonal_,
         areaEps_,
         minNormalDot_,
         maxLocalError_}
    );
    if (result.accepted()) {
        applyCollapse(edge.keep, edge.remove, result.acceptedPosition, mergedQ, result.texturePlan);
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
    case CollapseAttemptStatus::TextureRejected:
        ++report_.textureRejectedCollapses;
        message = "texture constraints leave no valid collapses";
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

void SimplificationRun::applyCollapse(
    int keep, int remove, const Vec3& position, const Mat4& mergedQ, const TextureUpdatePlan& texturePlan
) {
    std::unordered_set<int> affectedFaces;
    if (policies_.legality.preventLocalIntersections) {
        affectedFaces = collectAffectedFacesForCollapse(keep, remove);
        for (int faceId : affectedFaces) {
            spatialIndex_.removeFace(faceId);
        }
    }
    // The plan was built for the accepted placement during the attempt, so
    // applying it is a straight replay without rebuilding chart pairings.
    const bool textureApplied = textureProtection_.apply(texturePlan, faceTexCoords_);
    assert(textureApplied && "accepted collapse must carry an applicable texture update plan");
    if (!textureApplied) {
        ++report_.textureApplyFailures;
    }

    // Any removed feature vertex leaves its loop, including cross-loop merges,
    // so the per-loop active counts track the surviving vertices exactly.
    const VertexState& removedVertex = vertices_[remove];
    if (removedVertex.isFeature && removedVertex.featureLoopId >= 0 &&
        removedVertex.featureLoopId < static_cast<int>(activeLoopCounts_.size())) {
        --activeLoopCounts_[removedVertex.featureLoopId];
    }

    // A vertex merged with a boundary vertex lies on the open boundary
    // afterwards; keep the flag current for the extended link condition.
    vertices_[keep].isBoundary = vertices_[keep].isBoundary || vertices_[remove].isBoundary;
    vertices_[keep].p = position;
    vertices_[keep].q = mergedQ;
    // The queue-priority boost follows the surviving feature evidence.
    vertices_[keep].priorityScale = std::max(vertices_[keep].priorityScale, vertices_[remove].priorityScale);
    refreshCircularTangent(vertices_[keep], primitiveFitOf(vertices_[keep], primitiveFits_));
    refreshEllipseTangent(vertices_[keep], primitiveFitOf(vertices_[keep], primitiveFits_));
    vertices_[remove].active = false;
    bumpVersions(keep, remove);

    rewriteIncidentFaces(keep, remove);
    if (policies_.legality.preventLocalIntersections) {
        for (int faceId : affectedFaces) {
            if (faceId >= 0 && faceId < static_cast<int>(faces_.size()) && faces_[faceId].active) {
                spatialIndex_.updateFace(faceId, faces_[faceId], vertices_);
            }
        }
    }
    ++report_.collapsedEdges;

    for (int neighbor : activeNeighborsOf(keep, faces_, vertices_, *topology_)) {
        pushEdgeCandidate(keep, neighbor);
    }
}

std::unordered_set<int> SimplificationRun::collectAffectedFacesForCollapse(int keep, int remove) const {
    std::unordered_set<int> affected;
    if (keep >= 0 && keep < static_cast<int>(topology_->vertexFaces.size())) {
        affected.insert(topology_->vertexFaces[keep].begin(), topology_->vertexFaces[keep].end());
    }
    if (remove >= 0 && remove < static_cast<int>(topology_->vertexFaces.size())) {
        affected.insert(topology_->vertexFaces[remove].begin(), topology_->vertexFaces[remove].end());
    }
    return affected;
}

void SimplificationRun::rewriteIncidentFaces(int keep, int remove) {
    const std::vector<int> removeIncidentFaces(
        topology_->vertexFaces[remove].begin(), topology_->vertexFaces[remove].end()
    );
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
