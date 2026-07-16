/**
 * @file src/simplification/detail/SimplificationRun.h
 * @brief Declares simplification run facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "common/detail/MeshDistanceIndex.h"
#include "core/Mesh.h"
#include "detail/CandidateQueue.h"
#include "detail/CollapseAttempt.h"
#include "detail/CollapseTopology.h"
#include "detail/FeatureConstraints.h"
#include "detail/FeatureGuidance.h"
#include "detail/Quadrics.h"
#include "detail/SimplificationPolicies.h"
#include "detail/SpatialFaceIndex.h"
#include "detail/TextureProtection.h"

#include <memory>
#include <unordered_set>
#include <vector>

namespace manumesh::feature {
struct FeatureAnalysis;
}

namespace manumesh::simplification {

/**
 * @brief Mutable, single-use execution object for one edge-collapse simplification.
 */
class SimplificationRun {
public:
    /**
     * @brief Creates a run that computes feature analysis from `options`.
     * @param[in] input Immutable source mesh that must outlive the run.
     * @param[in] options Immutable policy that must outlive the run.
     */
    SimplificationRun(const Mesh& input, const SimplifyOptions& options);
    /**
     * @brief Creates a run that may reuse caller-provided feature analysis.
     * @param[in] input Immutable source mesh that must outlive the run.
     * @param[in] options Immutable policy that must outlive the run.
     * @param[in] features Optional analysis whose mesh must match `input`.
     */
    SimplificationRun(const Mesh& input, const SimplifyOptions& options, const feature::FeatureAnalysis* features);

    /**
     * @brief Executes initialization, collapse, optional refinement, and compaction.
     */
    Mesh execute(SimplifyReport* outReport);

private:
    /** @brief Resets all report fields and records input dimensions. */
    void initializeReport();
    /** @brief Reuses or computes feature analysis and builds guidance tables. */
    void analyzeFeatures();
    /** @brief Creates mutable vertex records and initial quadrics. */
    void initializeVertices();
    /** @brief Copies feature ownership and constraints onto one vertex. */
    void initializeVertexFeature(int vertexId);
    /** @brief Creates mutable face, UV, topology, and spatial-index state. */
    void initializeFaces();
    /** @brief Resolves target counts and scale-dependent legality budgets. */
    void initializeBudget();
    /** @brief Rebuilds the candidate heap from all current active edges. */
    void rebuildQueue();
    /**
     * @brief Solves the edge's placements once, prices the texture protection from
     * the same solve, and pushes the candidate with the cached placements.
     * Returns true when the (near-)midpoint placement is texture-rejected,
     * which feeds the textureProtectedEdges diagnostic on the initial build.
     */
    bool pushEdgeCandidate(int a, int b);
    /** @brief Pops and evaluates candidates until a configured stop condition. */
    void collapseUntilTarget();
    /** @brief Runs optional fixed-topology quality refinement. */
    void refineQuality();
    /** @brief Rebuilds an exhausted heap when active topology can still progress. */
    bool ensureQueueHasCandidates();
    /** @brief Checks endpoint activity and version stamps for a queued candidate. */
    bool isCurrentCandidate(const Candidate& candidate) const;
    /** @brief Accounts for a stale queue entry and triggers periodic rebuilding. */
    void handleStaleCandidate();
    /** @brief Evaluates and applies one current candidate when a placement passes. */
    bool tryCollapse(const Candidate& candidate);
    /** @brief Maps a categorized failed attempt into report counters. */
    void recordRejectedCollapse(const CollapseAttemptResult& result);
    /** @brief Invalidates queued candidates incident to either endpoint. */
    void bumpVersions(int keep, int remove);
    /** @brief Commits topology, geometry, quadric, UV, and index updates. */
    void applyCollapse(
        int keep, int remove, const Vec3& position, const Mat4& mergedQ, const TextureUpdatePlan& texturePlan
    );
    /** @brief Collects faces whose broad-phase registrations may change. */
    std::unordered_set<int> collectAffectedFacesForCollapse(int keep, int remove) const;
    /** @brief Rewrites incident faces and removes duplicates after a collapse. */
    void rewriteIncidentFaces(int keep, int remove);

    const Mesh& input_;
    const SimplifyOptions& options_;
    const feature::FeatureAnalysis* precomputedFeatures_ = nullptr;
    SimplificationPolicies policies_;
    SimplifyReport report_;
    FeatureGuidance featureGuidance_;
    std::vector<char> boundaryVertices_;
    std::vector<VertexState> vertices_;
    std::vector<FaceState> faces_;
    std::vector<FaceTexCoords> faceTexCoords_;
    std::unique_ptr<DynamicTopology> topology_;
    SpatialFaceIndex spatialIndex_;
    std::unique_ptr<manumesh::common::MeshDistanceIndex> referenceSurface_;
    std::vector<int> activeLoopCounts_;
    /**
     * @brief Compact side table of circle/ellipse fit data; only feature vertices on
     * fitted primitive loops own an entry (VertexState::primitiveFitId).
     */
    std::vector<FeaturePrimitiveFit> primitiveFits_;
    CandidateQueue queue_;
    InitialQuadricBuilder quadrics_;
    FeatureConstraintPolicy featurePolicy_;
    TextureProtection textureProtection_;
    int activeFaceCount_ = 0;
    int targetFaces_ = 0;
    double areaEps_ = 0.0;
    /**
     * @brief Input bounding-box diagonal, computed once in initializeBudget.
     * tryCollapse runs per collapse attempt, so it must not recompute this
     * O(V) scan (doing so made the whole run quadratic in the mesh size).
     */
    double meshDiagonal_ = 0.0;
    double minNormalDot_ = 0.0;
    double maxLocalError_ = 0.0;
    int maxAttemptsWithoutCollapse_ = 0;
    int attemptsWithoutCollapse_ = 0;
    int stalePops_ = 0;
    bool queueBuiltOnce_ = false;
};

} // namespace manumesh::simplification
