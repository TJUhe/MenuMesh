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

class SimplificationRun {
public:
    SimplificationRun(const Mesh& input, const SimplifyOptions& options);
    SimplificationRun(const Mesh& input, const SimplifyOptions& options, const feature::FeatureAnalysis* features);

    Mesh execute(SimplifyReport* outReport);

private:
    void initializeReport();
    void analyzeFeatures();
    void initializeVertices();
    void initializeVertexFeature(int vertexId);
    void initializeFaces();
    void initializeBudget();
    void rebuildQueue();
    /// Solves the edge's placements once, prices the texture protection from
    /// the same solve, and pushes the candidate with the cached placements.
    /// Returns true when the (near-)midpoint placement is texture-rejected,
    /// which feeds the textureProtectedEdges diagnostic on the initial build.
    bool pushEdgeCandidate(int a, int b);
    void collapseUntilTarget();
    void refineQuality();
    bool ensureQueueHasCandidates();
    bool isCurrentCandidate(const Candidate& candidate) const;
    void handleStaleCandidate();
    bool tryCollapse(const Candidate& candidate);
    void recordRejectedCollapse(const CollapseAttemptResult& result);
    void bumpVersions(int keep, int remove);
    void applyCollapse(
        int keep, int remove, const Vec3& position, const Mat4& mergedQ, const TextureUpdatePlan& texturePlan
    );
    std::unordered_set<int> collectAffectedFacesForCollapse(int keep, int remove) const;
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
    /// Compact side table of circle/ellipse fit data; only feature vertices on
    /// fitted primitive loops own an entry (VertexState::primitiveFitId).
    std::vector<FeaturePrimitiveFit> primitiveFits_;
    CandidateQueue queue_;
    InitialQuadricBuilder quadrics_;
    FeatureConstraintPolicy featurePolicy_;
    TextureProtection textureProtection_;
    int activeFaceCount_ = 0;
    int targetFaces_ = 0;
    double areaEps_ = 0.0;
    /// Input bounding-box diagonal, computed once in initializeBudget.
    /// tryCollapse runs per collapse attempt, so it must not recompute this
    /// O(V) scan (doing so made the whole run quadratic in the mesh size).
    double meshDiagonal_ = 0.0;
    double minNormalDot_ = 0.0;
    double maxLocalError_ = 0.0;
    int maxAttemptsWithoutCollapse_ = 0;
    int attemptsWithoutCollapse_ = 0;
    int stalePops_ = 0;
    bool queueBuiltOnce_ = false;
};

} // namespace manumesh::simplification
