/**
 * @file src/simplification/detail/FeatureGuidance.h
 * @brief Declares feature guidance facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"
#include "detail/SimplificationTypes.h"

#include <vector>

namespace manumesh::feature {
struct FeatureAnalysis;
}

namespace manumesh::simplification {

struct FeatureDetectionPolicy;

/**
 * @brief Soft feature attributes copied onto one simplification vertex.
 */
struct FeatureVertexGuidance {
    bool isFeature = false;
    bool circular = false;
    bool junction = false;
    bool weakFeature = false;
    FeatureCurveKind primitive = FeatureCurveKind::Unknown;
    int loopId = -1;
    int componentId = -1;
    double confidence = 0.0;
    Vec3 tangent = Vec3::Zero();
    Vec3 circleCenter = Vec3::Zero();
    Vec3 circleNormal = Vec3(0.0, 0.0, 1.0);
    double circleRadius = 0.0;
    Vec3 ellipseCenter = Vec3::Zero();
    Vec3 ellipseNormal = Vec3(0.0, 0.0, 1.0);
    Vec3 ellipseMajorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 ellipseMinorAxis = Vec3(0.0, 1.0, 0.0);
    double ellipseMajorRadius = 0.0;
    double ellipseMinorRadius = 0.0;
};

/**
 * @brief Aggregate feature-analysis diagnostics copied into SimplifyReport.
 */
struct FeatureGuidanceSummary {
    int featureLoops = 0;
    int circularFeatureLoops = 0;
    int featureVertices = 0;
    int tracedFeatureEdges = 0;
    int untracedFeatureEdges = 0;
    int normalTensorFeatureEdges = 0;
    int normalTensorScoredVertices = 0;
    int smoothCurvatureFeatureEdges = 0;
    int smoothCurvatureScoredVertices = 0;
    int featureComponents = 0;
    int weakFeatureComponents = 0;
    int highConfidenceFeatureComponents = 0;
    int graphCleanupBridgedGaps = 0;
    int graphCleanupRemovedSpurs = 0;
    int graphCleanupMergedJunctions = 0;
    double maxNormalTensorPersistentScore = 0.0;
    double meanNormalTensorLocalScale = 0.0;
    double meanNormalTensorPersistence = 0.0;
    double maxSmoothCurvaturePersistentScore = 0.0;
    double meanSmoothCurvatureLocalScale = 0.0;
    double meanSmoothCurvaturePersistence = 0.0;
    double meanFeatureComponentConfidence = 0.0;
    double minFeatureComponentConfidence = 0.0;
    int inconsistentWindingEdges = 0;
    int graphCleanupSkippedByCap = 0;
    int circularRecoveryTruncated = 0;
    feature::FeatureNormalFilterReport normalFilter;
    double meanSmoothCurvatureScaleStability = 0.0;
    int graphConsolidationBridges = 0;
    int graphConsolidationSkippedByCap = 0;
    int junctionBranchPairs = 0;
    int ambiguousFeatureJunctions = 0;
};

/**
 * @brief Per-vertex soft guidance and primitive fits derived from one feature analysis.
 */
struct FeatureGuidance {
    bool enabled = false;
    std::vector<FeatureVertexGuidance> vertices;
    std::vector<FeatureCurveConstraint> curves;
    FeatureGuidanceSummary summary;
};

/**
 * @brief Queue-priority sensitivity factors decoupled from placement quadrics.
 */
struct FeatureWeightScores {
    std::vector<double> values;
    int normalTensorScoredVertices = 0;
    double maxNormalTensorPersistentScore = 0.0;
    double meanNormalTensorLocalScale = 0.0;
    double meanNormalTensorPersistence = 0.0;
};

/**
 * @brief Detects features and converts them to simplification guidance.
 */
FeatureGuidance buildFeatureGuidance(const Mesh& mesh, const FeatureDetectionPolicy& policy);
FeatureGuidance buildFeatureGuidance(
    const Mesh& mesh, const FeatureDetectionPolicy& policy, const feature::FeatureAnalysis* precomputed
);

/**
 * @brief Computes optional feature-sensitive queue weights without changing quadrics.
 */
FeatureWeightScores computeFeatureWeightScores(const Mesh& mesh, const SimplifyOptions& options);

/**
 * @brief Copies feature diagnostics into the run report.
 */
void applyFeatureGuidanceSummary(const FeatureGuidanceSummary& summary, SimplifyReport& report);

} // namespace manumesh::simplification
