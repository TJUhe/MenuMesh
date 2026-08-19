/**
 * @file src/simplification/SimplificationRun.cpp
 * @brief 调度一次完整的边坍缩简化运行。
 * @ingroup manumesh_simplification
 *
 * @details 拥有可变状态，并调度一次完整的简化运行。
 * @algorithm 初始化顶点/面二次误差矩阵和动态拓扑，构建候选堆；反复丢弃过期条目或评估当前放置，应用接受的局部编辑并刷新受影响候选；达到目标、无候选或拒绝次数上限后停止，最后压缩并可选地细化结果。
 * @invariants 每当端点邻域发生变化时，候选版本都会更新；活动面只引用活动顶点；报告中的拒绝计数记录每个当前候选触发的首个硬过滤器。
 */

#include "detail/SimplificationRun.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "common/detail/MeshQueries.h"
#include "common/detail/ParallelExecution.h"
#include "detail/CollapseTopology.h"
#include "detail/FeatureConstraints.h"
#include "detail/FeatureGuidance.h"
#include "detail/Placement.h"
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

namespace manumesh {
namespace simplification {

SimplificationRun::SimplificationRun(const Mesh& input, const SimplifyOptions& options)
    : SimplificationRun(input, options, nullptr, ExecutionOptions{}) {}

SimplificationRun::SimplificationRun(
    const Mesh& input, const SimplifyOptions& options, const ExecutionOptions& executionOptions
)
    : SimplificationRun(input, options, nullptr, executionOptions) {}

SimplificationRun::SimplificationRun(
    const Mesh& input, const SimplifyOptions& options, const feature::FeatureAnalysis* features
)
    : SimplificationRun(input, options, features, ExecutionOptions{}) {}

SimplificationRun::SimplificationRun(
    const Mesh& input,
    const SimplifyOptions& options,
    const feature::FeatureAnalysis* features,
    const ExecutionOptions& executionOptions
)
    : input_(input),
      options_(options),
      executionOptions_(executionOptions),
      precomputedFeatures_(features),
      featureAnalysis_(features),
      policies_(SimplificationPolicies::fromOptions(options)),
      quadrics_(options, executionOptions),
      featurePolicy_(options),
      textureProtection_(input, options) {
    validateExecutionOptions(executionOptions_);
}

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
    if (options_.qualityRefinementIterations <= 0) {
        return;
    }
    if (textureProtection_.active()) {
        report_.qualityRefinementSkippedForTexture = true;
        return;
    }
    runQualityRefinement(
        {options_,
         vertices_,
         faces_,
         *topology_,
         featurePolicy_,
         featureGuidance_.constraints,
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
    // 输入中的零面积面会被宽松校验允许存在；它们在 computeInitialQuadrics 中回退到小的点二次误差项，合法性检查中的面积条件则阻止退化继续扩散。记录数量，避免被容忍的脏输入悄然通过。
    report_.degenerateInputFaces = countDegenerateFaces(input_);
}

void SimplificationRun::analyzeFeatures() {
    featureGuidance_ = FeatureGuidance{};
    if (!policies_.features.enabled) {
        return;
    }

    if (featureAnalysis_ == nullptr) {
        ownedFeatureAnalysis_ = std::make_unique<feature::FeatureAnalysis>(
            feature::detectFeatureCurves(input_, policies_.features.options, executionOptions_)
        );
        featureAnalysis_ = ownedFeatureAnalysis_.get();
    }
    featureGuidance_ = buildFeatureGuidance(input_, policies_.features, featureAnalysis_);
    applyFeatureAnalysisReport(*featureAnalysis_, featureGuidance_, report_);
}

void SimplificationRun::initializeVertices() {
    const feature::FeatureAnalysis* weightAnalysis = precomputedFeatures_;
    if (weightAnalysis == nullptr && featureAnalysis_ != nullptr) {
        const bool needsNormalTensor =
            options_.weightMode == WeightMode::NormalTensor &&
            !featureAnalysis_->normalTensorVertexWeights.empty();
        const bool needsSmoothCurvature =
            options_.weightMode == WeightMode::SmoothCurvature &&
            !featureAnalysis_->smoothCurvatureVertexWeights.empty();
        if (needsNormalTensor || needsSmoothCurvature) {
            weightAnalysis = featureAnalysis_;
        }
    }
    const InitialQuadrics initialQuadrics = quadrics_.build(input_, featureGuidance_, weightAnalysis, report_);
    // 始终计算边界标志（一次 O(E) 遍历），因为扩展链接条件需要在 preserveBoundary 关闭时也阻止边界弦收缩。preserveBoundary 保持原有含义：只限制边界顶点的移动或合并方式。
    boundaryVertices_ = common::computeBoundaryVertices(input_);
    primitiveFits_ = featureGuidance_.primitiveFits;
    vertices_.assign(input_.vertices.size(), VertexState{});
    common::parallel::forEachRange(
        0,
        input_.vertices.size(),
        common::parallel::makeRangeExecutionOptions(executionOptions_),
        [&](std::size_t begin, std::size_t end) {
            for (std::size_t vertexId = begin; vertexId < end; ++vertexId) {
                VertexState& vertex = vertices_[vertexId];
                vertex.p = input_.vertices[vertexId];
                vertex.q = initialQuadrics.quadrics[vertexId];
                if (vertexId < initialQuadrics.priorityScales.size()) {
                    vertex.priorityScale = initialQuadrics.priorityScales[vertexId];
                }
                vertex.isBoundary = vertexId < boundaryVertices_.size() && boundaryVertices_[vertexId] != 0;
                initializeVertexFeature(static_cast<int>(vertexId));
            }
        }
    );

    activeLoopCounts_.clear();
    if (!featureGuidance_.enabled) {
        return;
    }
    activeLoopCounts_.assign(featureGuidance_.curves.size(), 0);
    for (int vertexId = 0; vertexId < static_cast<int>(vertices_.size()); ++vertexId) {
        const VertexState& vertex = vertices_[static_cast<std::size_t>(vertexId)];
        if (!vertex.isFeature) {
            continue;
        }
        const FeatureConstraintVertex* constraintVertex =
            vertexId < static_cast<int>(featureGuidance_.constraints.vertices.size())
                ? &featureGuidance_.constraints.vertices[static_cast<std::size_t>(vertexId)]
                : nullptr;
        if (constraintVertex == nullptr || constraintVertex->loopIds.empty()) {
            if (vertex.featureLoopId >= 0 && vertex.featureLoopId < static_cast<int>(activeLoopCounts_.size())) {
                ++activeLoopCounts_[vertex.featureLoopId];
            }
            continue;
        }
        for (int loopId : constraintVertex->loopIds) {
            if (loopId >= 0 && loopId < static_cast<int>(activeLoopCounts_.size())) {
                ++activeLoopCounts_[loopId];
            }
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
    vertex.primitiveFitId = vf.primitiveFitId;
}

void SimplificationRun::initializeFaces() {
    faces_.assign(input_.faces.size(), FaceState{});
    common::parallel::forEachRange(
        0,
        input_.faces.size(),
        common::parallel::makeRangeExecutionOptions(executionOptions_),
        [&](std::size_t begin, std::size_t end) {
            for (std::size_t faceId = begin; faceId < end; ++faceId) {
                faces_[faceId].v = input_.faces[faceId].v;
            }
        }
    );
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
    noProgressQueueRebuilds_ = 0;
}

void SimplificationRun::rebuildQueue() {
    const std::vector<std::pair<int, int>> edges = collectActiveEdges(faces_);
    std::vector<Candidate> candidates(edges.size());
    std::vector<char> midpointProtected(edges.size(), 0);
    common::parallel::forEachRange(
        0,
        edges.size(),
        common::parallel::makeRangeExecutionOptions(executionOptions_),
        [&](std::size_t begin, std::size_t end) {
            for (std::size_t edgeId = begin; edgeId < end; ++edgeId) {
                const PreparedEdgeCandidate prepared =
                    prepareEdgeCandidate(edges[edgeId].first, edges[edgeId].second);
                candidates[edgeId] = prepared.candidate;
                midpointProtected[edgeId] = prepared.midpointProtected ? 1 : 0;
            }
        }
    );
    queue_.rebuild(std::move(candidates));
    int textureProtectedEdges = 0;
    for (char protectedEdge : midpointProtected) {
        if (protectedEdge) {
            ++textureProtectedEdges;
        }
    }
    // 初始构建不计作重建；这里只统计后续补充或过期候选恢复触发的重建。初始构建还兼作 textureProtectedEdges 统计：复用每个放置的纹理评估，避免单独执行 O(E) 遍历。
    if (queueBuiltOnce_) {
        ++report_.queueRebuilds;
    } else {
        report_.textureProtectedEdges = textureProtectedEdges;
    }
    queueBuiltOnce_ = true;
}

bool SimplificationRun::pushEdgeCandidate(int a, int b) {
    const PreparedEdgeCandidate prepared = prepareEdgeCandidate(a, b);
    queue_.pushCandidate(prepared.candidate);
    return prepared.midpointProtected;
}

SimplificationRun::PreparedEdgeCandidate SimplificationRun::prepareEdgeCandidate(int a, int b) const {
    PreparedEdgeCandidate prepared;
    if (a == b) {
        return prepared;
    }
    // 端点按规范顺序排列，使缓存的放置列表与弹出时折叠尝试重新求解的列表完全一致。
    const int first = std::min(a, b);
    const int second = std::max(a, b);
    if (!vertices_[first].active || !vertices_[second].active) {
        return prepared;
    }
    const Mat4 q = vertices_[first].q + vertices_[second].q;
    const std::vector<SolveResult> placements = solvePlacementCandidates(q, vertices_[first].p, vertices_[second].p);

    double textureCost = 0.0;
    bool midpointProtected = false;
    if (textureProtection_.active()) {
        const CollapseEdge edge{first, second};
        const BoundaryCollapseDecision boundaryDecision =
            boundaryCollapseDecision({edge, faces_, vertices_, *topology_, options_});
        const Vec3 midpoint = 0.5 * (vertices_[first].p + vertices_[second].p);
        double bestCombinedCost = std::numeric_limits<double>::infinity();
        double bestMidpointDistance = std::numeric_limits<double>::infinity();
        for (const SolveResult& placement : placements) {
            Vec3 evaluatedPosition = placement.position;
            projectBoundaryPlacement({edge, boundaryDecision, vertices_, faces_, *topology_}, evaluatedPosition);
            const bool projected = featurePolicy_.projectPlacement(
                {edge, vertices_, featureGuidance_.curves, primitiveFits_, featureGuidance_.constraints},
                evaluatedPosition
            );
            if (projected && boundaryDecision.boundaryEdge) {
                projectBoundaryPlacement({edge, boundaryDecision, vertices_, faces_, *topology_}, evaluatedPosition);
            }
            if (!featureCurveBudgetAllows(
                    vertices_[first],
                    vertices_[second],
                    featureGuidance_.curves,
                    primitiveFits_,
                    options_,
                    meshDiagonal_,
                    evaluatedPosition,
                    &featureGuidance_.constraints,
                    edge
                )) {
                continue;
            }
            const TextureCollapseEvaluation textureEvaluation =
                textureProtection_.evaluate(edge, evaluatedPosition, faces_, vertices_, *topology_, faceTexCoords_);
            // 候选列表始终包含中点（或与中点重合的端点），因此选取距离中点最近的放置，可以精确复现之前的中点保护统计。
            const double midpointDistance = (placement.position - midpoint).squaredNorm();
            if (midpointDistance < bestMidpointDistance) {
                bestMidpointDistance = midpointDistance;
                midpointProtected = !textureEvaluation.allowed();
            }
            if (textureEvaluation.allowed()) {
                const double projectedCost = evaluateQuadric(q, evaluatedPosition);
                if (std::isfinite(projectedCost)) {
                    bestCombinedCost = std::min(bestCombinedCost, projectedCost + textureEvaluation.cost);
                }
            }
        }
        if (!std::isfinite(bestCombinedCost)) {
            textureCost = std::numeric_limits<double>::infinity();
        } else {
            textureCost = std::max(0.0, bestCombinedCost - placements.front().cost);
        }
    }
    if (!placements.empty() && std::isfinite(textureCost)) {
        const double priorityScale = std::max(vertices_[first].priorityScale, vertices_[second].priorityScale);
        prepared.candidate.cost = placements.front().cost * priorityScale + textureCost;
        prepared.candidate.a = first;
        prepared.candidate.b = second;
        prepared.candidate.versionA = vertices_[first].version;
        prepared.candidate.versionB = vertices_[second].version;
        prepared.candidate.placementCount = std::min(
            static_cast<int>(prepared.candidate.placements.size()), static_cast<int>(placements.size())
        );
        for (int i = 0; i < prepared.candidate.placementCount; ++i) {
            prepared.candidate.placements[static_cast<std::size_t>(i)] = placements[static_cast<std::size_t>(i)];
        }
    }
    prepared.midpointProtected = midpointProtected;
    return prepared;
}

void SimplificationRun::collapseUntilTarget() {
    if (activeFaceCount_ <= targetFaces_) {
        report_.terminationReason = report_.collapsedEdges > 0 ? SimplifyTerminationReason::ReachedTarget
                                                               : SimplifyTerminationReason::AlreadyAtOrBelowTarget;
        return;
    }

    while (activeFaceCount_ > targetFaces_) {
        if (!ensureQueueHasCandidates()) {
            // 有候选但整轮均被拒绝时，不再对同一拓扑重复扫描；保留原有的拒绝终止分类。
            report_.terminationReason = attemptsWithoutCollapse_ > 0 ? SimplifyTerminationReason::RejectionLimit
                                                                     : SimplifyTerminationReason::NoCandidates;
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
    if (noProgressQueueRebuilds_ >= 1) {
        return false;
    }
    rebuildQueue();
    ++noProgressQueueRebuilds_;
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
    ++stalePops_;
    // 堆中的过期条目只会增加比较和内存开销。达到固定上限，或堆规模明显超过当前面数时，
    // 直接按当前活动拓扑重建，避免 stale 条目长期累积。重建不改变候选排序和结果。
    const std::size_t faceScaledLimit = static_cast<std::size_t>(std::max(1024, std::max(1, activeFaceCount_) * 4));
    if (stalePops_ > 10000 || queue_.size() > faceScaledLimit) {
        rebuildQueue();
        stalePops_ = 0;
    }
}

bool SimplificationRun::tryCollapse(const Candidate& candidate) {
    const int keep = candidate.a;
    const int remove = candidate.b;
    const CollapseEdge edge{keep, remove};
    const Mat4 mergedQ = vertices_[keep].q + vertices_[remove].q;
    // isCurrentCandidate 校验的版本戳保证缓存的放置求解仍然精确，因此这里无需重新求解。
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
         featureGuidance_.constraints,
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
    // 尝试阶段已经为接受的放置构建好计划，因此应用时直接重放，无需重新构建图表配对。
    const bool textureApplied = textureProtection_.apply(texturePlan, faceTexCoords_);
    assert(textureApplied && "accepted collapse must carry an applicable texture update plan");
    if (!textureApplied) {
        ++report_.textureApplyFailures;
    }

    // 任何被移除的特征顶点都会离开其所在环，包括跨环合并，因此各环的活动顶点计数始终准确反映剩余顶点。
    const VertexState& removedVertex = vertices_[remove];
    const FeatureConstraintVertex* removedConstraintVertex =
        remove >= 0 && remove < static_cast<int>(featureGuidance_.constraints.vertices.size())
            ? &featureGuidance_.constraints.vertices[static_cast<std::size_t>(remove)]
            : nullptr;
    if (removedVertex.isFeature && (removedConstraintVertex == nullptr || removedConstraintVertex->loopIds.empty()) &&
        removedVertex.featureLoopId >= 0 && removedVertex.featureLoopId < static_cast<int>(activeLoopCounts_.size())) {
        --activeLoopCounts_[removedVertex.featureLoopId];
    } else if (removedVertex.isFeature && removedConstraintVertex != nullptr) {
        for (int loopId : removedConstraintVertex->loopIds) {
            if (loopId >= 0 && loopId < static_cast<int>(activeLoopCounts_.size())) {
                --activeLoopCounts_[loopId];
            }
        }
    }
    featureGuidance_.constraints.contractVertex(keep, remove);

    vertices_[keep].p = position;
    vertices_[keep].q = mergedQ;
    // 队列优先级增益跟随保留下来的特征证据。
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
    // 折叠改变了版本戳和拓扑，下一次队列耗尽时允许重新做一次完整兜底扫描。
    noProgressQueueRebuilds_ = 0;

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
    std::vector<int> removeIncidentFaces(topology_->vertexFaces[remove].begin(), topology_->vertexFaces[remove].end());
    std::sort(removeIncidentFaces.begin(), removeIncidentFaces.end());
    removeIncidentFaces.erase(
        std::unique(removeIncidentFaces.begin(), removeIncidentFaces.end()), removeIncidentFaces.end()
    );
    std::vector<int> boundaryCandidates;
    boundaryCandidates.push_back(keep);
    for (int faceId : removeIncidentFaces) {
        if (faceId < 0 || faceId >= static_cast<int>(faces_.size())) {
            continue;
        }
        FaceState& face = faces_[faceId];
        if (!face.active || !containsVertex(face, remove)) {
            continue;
        }
        boundaryCandidates.insert(boundaryCandidates.end(), face.v.begin(), face.v.end());
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

    std::sort(boundaryCandidates.begin(), boundaryCandidates.end());
    boundaryCandidates.erase(
        std::unique(boundaryCandidates.begin(), boundaryCandidates.end()), boundaryCandidates.end()
    );
    vertices_[remove].isBoundary = false;
    for (int vertex : boundaryCandidates) {
        if (vertex < 0 || vertex >= static_cast<int>(vertices_.size()) || !vertices_[vertex].active ||
            vertex >= static_cast<int>(topology_->vertexFaces.size())) {
            continue;
        }
        bool isBoundary = false;
        for (int faceId : topology_->vertexFaces[vertex]) {
            if (faceId < 0 || faceId >= static_cast<int>(faces_.size()) || !faces_[faceId].active) {
                continue;
            }
            for (int neighbor : faces_[faceId].v) {
                if (neighbor != vertex && activeIncidentFaceCountForEdge(vertex, neighbor, faces_, *topology_) == 1) {
                    isBoundary = true;
                    break;
                }
            }
            if (isBoundary) {
                break;
            }
        }
        vertices_[vertex].isBoundary = isBoundary;
    }
}

} // namespace simplification
} // namespace manumesh
