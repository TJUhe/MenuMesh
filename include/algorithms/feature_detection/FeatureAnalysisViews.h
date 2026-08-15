/**
 * @file include/algorithms/feature_detection/FeatureAnalysisViews.h
 * @brief Read-only, non-owning views over a FeatureAnalysis result.
 * @ingroup manumesh_feature_detection
 *
 * @details FeatureAnalysis remains the source-compatible aggregate returned by
 *          the detector. New consumers can depend on a narrower view so that
 *          local evidence, recovered curves, segmentation, and run diagnostics
 *          do not become an implicit all-or-nothing contract.
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh {
namespace feature {

/// Read-only local evidence used to construct the feature graph.
///
/// The view does not own data and must not outlive the referenced analysis.
/// `graphEdges()` includes synthetic recovery edges; inspect
/// FeatureGraphEdge::synthetic() when only original local evidence is valid.
class FeatureEvidenceView {
public:
    explicit FeatureEvidenceView(const FeatureAnalysis& analysis) noexcept
        : analysis_(&analysis) {}
    FeatureEvidenceView(FeatureAnalysis&&) = delete;
    FeatureEvidenceView(const FeatureAnalysis&&) = delete;

    const std::vector<FeatureGraphEdge>& graphEdges() const noexcept { return analysis_->graph.edges; }
    const std::vector<double>& normalTensorVertexWeights() const noexcept {
        return analysis_->normalTensorVertexWeights;
    }

    int featureEdgeCount() const noexcept { return analysis_->featureEdges; }
    int boundaryFeatureEdgeCount() const noexcept { return analysis_->boundaryFeatureEdges; }
    int dihedralFeatureEdgeCount() const noexcept { return analysis_->dihedralFeatureEdges; }
    int normalTensorFeatureEdgeCount() const noexcept { return analysis_->normalTensorFeatureEdges; }
    int smoothCurvatureFeatureEdgeCount() const noexcept { return analysis_->smoothCurvatureFeatureEdges; }
    int nonManifoldFeatureEdgeCount() const noexcept { return analysis_->nonManifoldFeatureEdges; }

private:
    const FeatureAnalysis* analysis_;
};

/// Read-only recovered curve topology and primitive ownership.
///
/// The view does not own data and must not outlive the referenced analysis.
class FeatureCurveView {
public:
    explicit FeatureCurveView(const FeatureAnalysis& analysis) noexcept
        : analysis_(&analysis) {}
    FeatureCurveView(FeatureAnalysis&&) = delete;
    FeatureCurveView(const FeatureAnalysis&&) = delete;

    const std::vector<VertexFeature>& vertices() const noexcept { return analysis_->vertices; }
    const std::vector<FeatureLoop>& loops() const noexcept { return analysis_->loops; }
    const std::vector<FeatureComponent>& components() const noexcept { return analysis_->components; }
    const FeatureGraph& graph() const noexcept { return analysis_->graph; }

private:
    const FeatureAnalysis* analysis_;
};

/// Read-only surface patches induced by the active feature graph.
///
/// The view does not own data and must not outlive the referenced analysis.
class FeatureSegmentationView {
public:
    explicit FeatureSegmentationView(const FeatureAnalysis& analysis) noexcept
        : analysis_(&analysis) {}
    FeatureSegmentationView(FeatureAnalysis&&) = delete;
    FeatureSegmentationView(const FeatureAnalysis&&) = delete;

    const std::vector<int>& facePatchIds() const noexcept { return analysis_->facePatchIds; }
    const std::vector<FeaturePatch>& patches() const noexcept { return analysis_->patches; }
    const std::vector<FeaturePatchAdjacency>& patchAdjacencies() const noexcept { return analysis_->patchAdjacencies; }
    int closedSurfacePatchCount() const noexcept { return analysis_->closedSurfacePatches; }
    int ignoredRecoveryEdgeCount() const noexcept { return analysis_->segmentationIgnoredRecoveryEdges; }

private:
    const FeatureAnalysis* analysis_;
};

/// Read-only counters and quality diagnostics produced by the detection run.
///
/// Evidence source counts live on FeatureEvidenceView and surface-patch data
/// lives on FeatureSegmentationView. The view does not own data and must not
/// outlive the referenced analysis.
class FeatureDiagnosticsView {
public:
    explicit FeatureDiagnosticsView(const FeatureAnalysis& analysis) noexcept
        : analysis_(&analysis) {}
    FeatureDiagnosticsView(FeatureAnalysis&&) = delete;
    FeatureDiagnosticsView(const FeatureAnalysis&&) = delete;

    int tracedFeatureEdgeCount() const noexcept { return analysis_->tracedFeatureEdges; }
    int untracedFeatureEdgeCount() const noexcept { return analysis_->untracedFeatureEdges; }
    int graphCleanupBridgedGapCount() const noexcept { return analysis_->graphCleanupBridgedGaps; }
    int graphCleanupRemovedSpurCount() const noexcept { return analysis_->graphCleanupRemovedSpurs; }
    int graphCleanupMergedJunctionCount() const noexcept { return analysis_->graphCleanupMergedJunctions; }
    int graphCleanupSkippedByCapCount() const noexcept { return analysis_->graphCleanupSkippedByCap; }
    int graphConsolidationBridgeCount() const noexcept { return analysis_->graphConsolidationBridges; }
    int graphConsolidationSkippedByCapCount() const noexcept { return analysis_->graphConsolidationSkippedByCap; }
    int circularRecoveryTruncatedCount() const noexcept { return analysis_->circularRecoveryTruncated; }
    int inconsistentWindingEdgeCount() const noexcept { return analysis_->inconsistentWindingEdges; }
    int degenerateFaceCount() const noexcept { return analysis_->degenerateFaces; }

    int normalTensorScoredVertexCount() const noexcept { return analysis_->normalTensorScoredVertices; }
    int smoothCurvatureScoredVertexCount() const noexcept { return analysis_->smoothCurvatureScoredVertices; }
    int convexFeatureEdgeCount() const noexcept { return analysis_->convexFeatureEdges; }
    int concaveFeatureEdgeCount() const noexcept { return analysis_->concaveFeatureEdges; }
    int unknownSignedFeatureEdgeCount() const noexcept { return analysis_->unknownSignedFeatureEdges; }

    int weakFeatureComponentCount() const noexcept { return analysis_->weakFeatureComponents; }
    int highConfidenceFeatureComponentCount() const noexcept { return analysis_->highConfidenceFeatureComponents; }
    double meanFeatureComponentConfidence() const noexcept { return analysis_->meanFeatureComponentConfidence; }
    double minFeatureComponentConfidence() const noexcept { return analysis_->minFeatureComponentConfidence; }

    double maxNormalTensorFeatureScore() const noexcept { return analysis_->maxNormalTensorFeatureScore; }
    double maxNormalTensorPersistentScore() const noexcept { return analysis_->maxNormalTensorPersistentScore; }
    double meanNormalTensorLocalScale() const noexcept { return analysis_->meanNormalTensorLocalScale; }
    double meanNormalTensorPersistence() const noexcept { return analysis_->meanNormalTensorPersistence; }
    double maxSmoothCurvatureFeatureScore() const noexcept { return analysis_->maxSmoothCurvatureFeatureScore; }
    double maxSmoothCurvaturePersistentScore() const noexcept { return analysis_->maxSmoothCurvaturePersistentScore; }
    double meanSmoothCurvatureLocalScale() const noexcept { return analysis_->meanSmoothCurvatureLocalScale; }
    double meanSmoothCurvaturePersistence() const noexcept { return analysis_->meanSmoothCurvaturePersistence; }
    double meanSmoothCurvatureScaleStability() const noexcept { return analysis_->meanSmoothCurvatureScaleStability; }

    const FeatureNormalFilterReport& normalFilter() const noexcept { return analysis_->normalFilter; }
    int junctionBranchPairCount() const noexcept { return analysis_->junctionBranchPairs; }
    int ambiguousJunctionCount() const noexcept { return analysis_->ambiguousJunctions; }
    const FeatureAnalysisSource& source() const noexcept { return analysis_->source; }

private:
    const FeatureAnalysis* analysis_;
};

inline FeatureEvidenceView viewFeatureEvidence(const FeatureAnalysis& analysis) noexcept {
    return FeatureEvidenceView(analysis);
}
inline FeatureEvidenceView viewFeatureEvidence(FeatureAnalysis&&) = delete;
inline FeatureEvidenceView viewFeatureEvidence(const FeatureAnalysis&&) = delete;

inline FeatureCurveView viewFeatureCurves(const FeatureAnalysis& analysis) noexcept {
    return FeatureCurveView(analysis);
}
inline FeatureCurveView viewFeatureCurves(FeatureAnalysis&&) = delete;
inline FeatureCurveView viewFeatureCurves(const FeatureAnalysis&&) = delete;

inline FeatureSegmentationView viewFeatureSegmentation(const FeatureAnalysis& analysis) noexcept {
    return FeatureSegmentationView(analysis);
}
inline FeatureSegmentationView viewFeatureSegmentation(FeatureAnalysis&&) = delete;
inline FeatureSegmentationView viewFeatureSegmentation(const FeatureAnalysis&&) = delete;

inline FeatureDiagnosticsView viewFeatureDiagnostics(const FeatureAnalysis& analysis) noexcept {
    return FeatureDiagnosticsView(analysis);
}
inline FeatureDiagnosticsView viewFeatureDiagnostics(FeatureAnalysis&&) = delete;
inline FeatureDiagnosticsView viewFeatureDiagnostics(const FeatureAnalysis&&) = delete;

} // namespace feature
} // namespace manumesh
