/**
 * @file include/algorithms/feature_detection/FeatureAnalysisViews.h
 * @brief FeatureAnalysis 结果的只读、非拥有视图。
 * @ingroup manumesh_feature_detection
 *
 * @details FeatureAnalysis 仍是检测器返回的源码兼容聚合结果。新消费者可以依赖更窄的视图，
 *          让局部证据、恢复曲线、面分区和运行诊断不再隐式绑定为一个整体契约。
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh {
namespace feature {

/// 构建特征图所使用的只读局部证据。
///
/// 视图不拥有数据，生命周期不得超过所引用的分析结果。
/// `graphEdges()` 包含合成恢复边；只需要原始局部证据时，应检查 FeatureGraphEdge::synthetic()。
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
    const std::vector<double>& smoothCurvatureVertexWeights() const noexcept {
        return analysis_->smoothCurvatureVertexWeights;
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

/// 恢复曲线拓扑和几何基元归属的只读视图。
///
/// 视图不拥有数据，生命周期不得超过所引用的分析结果。
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

/// 活动特征图诱导的只读曲面分区视图。
///
/// 视图不拥有数据，生命周期不得超过所引用的分析结果。
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

/// 检测运行产生的只读计数和质量诊断视图。
///
/// 证据来源计数位于 FeatureEvidenceView，曲面分区数据位于 FeatureSegmentationView。
/// 视图不拥有数据，生命周期不得超过所引用的分析结果。
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
