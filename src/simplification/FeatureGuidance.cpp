/**
 * @file src/simplification/FeatureGuidance.cpp
 * @brief 实现 ManuMesh 的简化模块的特征引导功能。
 * @ingroup manumesh_simplification
 *
 * @details 将特征证据转换为软二次误差项和队列优先级引导。
 * @algorithm 分量置信度用于缩放点到切线的直线二次误差项。在自适应模式下，Wang 风格的敏感度与放置二次误差解耦，仅作为候选优先级乘数使用。
 * @invariants 软引导不能绕过硬性特征策略。
 */

#include "detail/FeatureGuidance.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "common/detail/MathConstants.h"
#include "common/detail/MeshQueries.h"
#include "detail/FeatureConstraints.h"
#include "detail/SimplificationPolicies.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace manumesh::simplification {
namespace {

using manumesh::common::kPi;

FeatureCurveKind toFeatureCurveKind(feature::FeaturePrimitiveType primitive) {
    switch (primitive) {
    case feature::FeaturePrimitiveType::Unknown:
        return FeatureCurveKind::Unknown;
    case feature::FeaturePrimitiveType::Circle:
        return FeatureCurveKind::Circle;
    case feature::FeaturePrimitiveType::NearCircle:
        return FeatureCurveKind::NearCircle;
    case feature::FeaturePrimitiveType::Ellipse:
        return FeatureCurveKind::Ellipse;
    case feature::FeaturePrimitiveType::PolygonalLoop:
        return FeatureCurveKind::PolygonalLoop;
    }
    return FeatureCurveKind::Unknown;
}

FeatureVertexGuidance toVertexGuidance(const feature::VertexFeature& source) {
    FeatureVertexGuidance target;
    target.isFeature = source.isFeature;
    target.circular = source.circular;
    target.junction = source.junction;
    target.weakFeature = source.weakFeature;
    target.primitive = toFeatureCurveKind(source.primitive);
    target.loopId = source.loopId;
    target.componentId = source.componentId;
    target.confidence = source.confidence;
    target.tangent = source.tangent;
    target.circleCenter = source.circleCenter;
    target.circleNormal = source.circleNormal;
    target.circleRadius = source.circleRadius;
    target.ellipseCenter = source.ellipseCenter;
    target.ellipseNormal = source.ellipseNormal;
    target.ellipseMajorAxis = source.ellipseMajorAxis;
    target.ellipseMinorAxis = source.ellipseMinorAxis;
    target.ellipseMajorRadius = source.ellipseMajorRadius;
    target.ellipseMinorRadius = source.ellipseMinorRadius;
    return target;
}

int curveStorageSize(const feature::FeatureAnalysis& analysis) {
    int size = static_cast<int>(analysis.loops.size());
    for (const feature::FeatureLoop& loop : analysis.loops) {
        if (loop.id >= 0) {
            size = std::max(size, loop.id + 1);
        }
    }
    return size;
}

int resolveNormalTensorMinPersistentScales(const SimplifyOptions& options) {
    return std::clamp(options.normalTensorMinPersistentScales, 1, std::max(1, options.normalTensorScaleCount));
}

void summarizeNormalTensorScores(const std::vector<feature::NormalTensorVertex>& tensor, FeatureWeightScores& result) {
    double localScaleSum = 0.0;
    double persistenceSum = 0.0;
    for (const feature::NormalTensorVertex& vertex : tensor) {
        result.maxNormalTensorPersistentScore =
            std::max(result.maxNormalTensorPersistentScore, vertex.persistentFeatureScore);
        if (vertex.featureScore <= 1e-12 && vertex.persistentFeatureScore <= 1e-12) {
            continue;
        }
        ++result.normalTensorScoredVertices;
        localScaleSum += vertex.localScale;
        persistenceSum += static_cast<double>(vertex.persistentScales);
    }

    if (result.normalTensorScoredVertices > 0) {
        const double count = static_cast<double>(result.normalTensorScoredVertices);
        result.meanNormalTensorLocalScale = localScaleSum / count;
        result.meanNormalTensorPersistence = persistenceSum / count;
    }
}

FeatureGuidance buildFeatureGuidanceFromAnalysis(const Mesh& mesh, const feature::FeatureAnalysis& analysis) {
    FeatureGuidance guidance;
    guidance.enabled = true;

    guidance.summary.featureLoops = static_cast<int>(analysis.loops.size());
    guidance.summary.tracedFeatureEdges = analysis.tracedFeatureEdges;
    guidance.summary.untracedFeatureEdges = analysis.untracedFeatureEdges;
    guidance.summary.normalTensorFeatureEdges = analysis.normalTensorFeatureEdges;
    guidance.summary.normalTensorScoredVertices = analysis.normalTensorScoredVertices;
    guidance.summary.smoothCurvatureFeatureEdges = analysis.smoothCurvatureFeatureEdges;
    guidance.summary.smoothCurvatureScoredVertices = analysis.smoothCurvatureScoredVertices;
    guidance.summary.featureComponents = static_cast<int>(analysis.components.size());
    guidance.summary.weakFeatureComponents = analysis.weakFeatureComponents;
    guidance.summary.highConfidenceFeatureComponents = analysis.highConfidenceFeatureComponents;
    guidance.summary.graphCleanupBridgedGaps = analysis.graphCleanupBridgedGaps;
    guidance.summary.graphCleanupRemovedSpurs = analysis.graphCleanupRemovedSpurs;
    guidance.summary.graphCleanupMergedJunctions = analysis.graphCleanupMergedJunctions;
    guidance.summary.maxNormalTensorPersistentScore = analysis.maxNormalTensorPersistentScore;
    guidance.summary.meanNormalTensorLocalScale = analysis.meanNormalTensorLocalScale;
    guidance.summary.meanNormalTensorPersistence = analysis.meanNormalTensorPersistence;
    guidance.summary.maxSmoothCurvaturePersistentScore = analysis.maxSmoothCurvaturePersistentScore;
    guidance.summary.meanSmoothCurvatureLocalScale = analysis.meanSmoothCurvatureLocalScale;
    guidance.summary.meanSmoothCurvaturePersistence = analysis.meanSmoothCurvaturePersistence;
    guidance.summary.meanSmoothCurvatureScaleStability = analysis.meanSmoothCurvatureScaleStability;
    guidance.summary.meanFeatureComponentConfidence = analysis.meanFeatureComponentConfidence;
    guidance.summary.minFeatureComponentConfidence = analysis.minFeatureComponentConfidence;
    guidance.summary.inconsistentWindingEdges = analysis.inconsistentWindingEdges;
    guidance.summary.graphCleanupSkippedByCap = analysis.graphCleanupSkippedByCap;
    guidance.summary.circularRecoveryTruncated = analysis.circularRecoveryTruncated;
    guidance.summary.normalFilter = analysis.normalFilter;
    guidance.summary.graphConsolidationBridges = analysis.graphConsolidationBridges;
    guidance.summary.graphConsolidationSkippedByCap = analysis.graphConsolidationSkippedByCap;
    guidance.summary.junctionBranchPairs = analysis.junctionBranchPairs;
    guidance.summary.ambiguousFeatureJunctions = analysis.ambiguousJunctions;

    guidance.vertices.reserve(analysis.vertices.size());
    for (const feature::VertexFeature& vertex : analysis.vertices) {
        guidance.vertices.push_back(toVertexGuidance(vertex));
        if (vertex.isFeature) {
            ++guidance.summary.featureVertices;
        }
    }

    guidance.curves.resize(curveStorageSize(analysis));
    for (const feature::FeatureLoop& loop : analysis.loops) {
        if (loop.circular) {
            ++guidance.summary.circularFeatureLoops;
        }
        if (loop.id < 0 || loop.id >= static_cast<int>(guidance.curves.size())) {
            continue;
        }
        FeatureCurveConstraint constraint;
        constraint.valid = loop.vertices.size() >= 2;
        constraint.closed = loop.closed;
        constraint.primitive = toFeatureCurveKind(loop.primitive);
        constraint.samples.reserve(loop.vertices.size());
        for (int vertexId : loop.vertices) {
            if (vertexId >= 0 && vertexId < static_cast<int>(mesh.vertices.size())) {
                constraint.samples.push_back(mesh.vertices[vertexId]);
            }
        }
        constraint.valid = constraint.valid && constraint.samples.size() >= 2;
        // 对长折线只构建一次线段索引，使每次折叠的最近点查询从 O(L) 降为 O(log L)。
        if (constraint.valid && constraint.primitive == FeatureCurveKind::PolygonalLoop) {
            buildPolylineSegmentIndex(constraint);
        }
        guidance.curves[loop.id] = std::move(constraint);
    }

    return guidance;
}

} // 结束匿名命名空间

FeatureGuidance buildFeatureGuidance(const Mesh& mesh, const FeatureDetectionPolicy& policy) {
    return buildFeatureGuidance(mesh, policy, nullptr);
}

FeatureGuidance buildFeatureGuidance(
    const Mesh& mesh, const FeatureDetectionPolicy& policy, const feature::FeatureAnalysis* precomputed
) {
    FeatureGuidance guidance;
    if (!policy.enabled) {
        return guidance;
    }

    if (precomputed) {
        return buildFeatureGuidanceFromAnalysis(mesh, *precomputed);
    }

    const feature::FeatureAnalysis analysis = feature::detectFeatureCurves(mesh, policy.options);
    return buildFeatureGuidanceFromAnalysis(mesh, analysis);
}

FeatureWeightScores computeFeatureWeightScores(const Mesh& mesh, const SimplifyOptions& options) {
    const WeightMode mode = options.weightMode;
    FeatureWeightScores result;
    std::vector<double>& score = result.values;
    score.assign(mesh.vertices.size(), 0.0);
    if (mode == WeightMode::Uniform) {
        return result;
    }

    const Vec3 lo = mesh.bboxMin();
    const Vec3 hi = mesh.bboxMax();
    const Vec3 span = hi - lo;

    if (mode == WeightMode::Height) {
        const double denom = std::max(1e-12, span.z());
        for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
            score[i] = std::clamp((mesh.vertices[i].z() - lo.z()) / denom, 0.0, 1.0);
        }
        return result;
    }

    if (mode == WeightMode::XBand) {
        const double denom = std::max(1e-12, span.x());
        for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
            const double x = (mesh.vertices[i].x() - lo.x()) / denom;
            score[i] = std::exp(-80.0 * (x - 0.5) * (x - 0.5));
        }
        return result;
    }

    if (mode == WeightMode::NormalTensor) {
        const std::vector<feature::NormalTensorVertex> tensor = feature::computeNormalTensorFeatures(
            mesh,
            feature::NormalTensorOptions{
                options.normalTensorSmoothingIterations,
                options.normalTensorScaleCount,
            },
            options.normalTensorFeatureThreshold
        );
        summarizeNormalTensorScores(tensor, result);
        const int requiredPersistentScales = resolveNormalTensorMinPersistentScales(options);
        for (int i = 0; i < static_cast<int>(tensor.size()); ++i) {
            if (tensor[i].persistentScales >= requiredPersistentScales) {
                score[i] = tensor[i].persistentFeatureScore;
            }
        }
        return result;
    }

    const std::vector<Vec3> faceNormals = common::computeFaceNormals(mesh);
    const common::MeshEdgeInfoMap edgeInfo = common::buildMeshEdgeInfo(mesh);
    const std::vector<char> windingFlip = common::harmonizeFaceWindings(mesh, edgeInfo);
    const double threshold = options.featureAngleDeg * kPi / 180.0;
    const double denom = std::max(1e-12, kPi - threshold);
    for (const auto& [key, info] : edgeInfo) {
        double edgeScore = 0.0;
        if (info.faces.size() == 1) {
            edgeScore = 1.0;
        } else if (info.faces.size() == 2) {
            const Vec3& n0 = faceNormals[info.faces[0]];
            const Vec3& n1 = faceNormals[info.faces[1]];
            // 退化（三角形面积为零）的面法向量为零，其点积会伪装成 90 度折痕。跳过该边，不对其评分。
            if (n0.squaredNorm() > 0.0 && n1.squaredNorm() > 0.0) {
                const auto [a, b] = common::unpackMeshEdgeKey(key);
                const double angle =
                    common::computeOrientedDihedralAngle(mesh, faceNormals, windingFlip, info, a, b).angleRad;
                edgeScore = std::clamp((angle - threshold) / denom, 0.0, 1.0);
            }
        }
        if (edgeScore > 0.0) {
            const auto [a, b] = common::unpackMeshEdgeKey(key);
            score[a] = std::max(score[a], edgeScore);
            score[b] = std::max(score[b], edgeScore);
        }
    }
    return result;
}

void applyFeatureGuidanceSummary(const FeatureGuidanceSummary& summary, SimplifyReport& report) {
    report.featureLoops = summary.featureLoops;
    report.circularFeatureLoops = summary.circularFeatureLoops;
    report.featureVertices = summary.featureVertices;
    report.tracedFeatureEdges = summary.tracedFeatureEdges;
    report.untracedFeatureEdges = summary.untracedFeatureEdges;
    report.normalTensorFeatureEdges = summary.normalTensorFeatureEdges;
    report.normalTensorScoredVertices = summary.normalTensorScoredVertices;
    report.smoothCurvatureFeatureEdges = summary.smoothCurvatureFeatureEdges;
    report.smoothCurvatureScoredVertices = summary.smoothCurvatureScoredVertices;
    report.featureComponents = summary.featureComponents;
    report.weakFeatureComponents = summary.weakFeatureComponents;
    report.highConfidenceFeatureComponents = summary.highConfidenceFeatureComponents;
    report.graphCleanupBridgedGaps = summary.graphCleanupBridgedGaps;
    report.graphCleanupRemovedSpurs = summary.graphCleanupRemovedSpurs;
    report.graphCleanupMergedJunctions = summary.graphCleanupMergedJunctions;
    report.maxNormalTensorPersistentScore = summary.maxNormalTensorPersistentScore;
    report.meanNormalTensorLocalScale = summary.meanNormalTensorLocalScale;
    report.meanNormalTensorPersistence = summary.meanNormalTensorPersistence;
    report.maxSmoothCurvaturePersistentScore = summary.maxSmoothCurvaturePersistentScore;
    report.meanSmoothCurvatureLocalScale = summary.meanSmoothCurvatureLocalScale;
    report.meanSmoothCurvaturePersistence = summary.meanSmoothCurvaturePersistence;
    report.meanSmoothCurvatureScaleStability = summary.meanSmoothCurvatureScaleStability;
    report.meanFeatureComponentConfidence = summary.meanFeatureComponentConfidence;
    report.minFeatureComponentConfidence = summary.minFeatureComponentConfidence;
    report.inconsistentWindingEdges = summary.inconsistentWindingEdges;
    report.graphCleanupSkippedByCap = summary.graphCleanupSkippedByCap;
    report.circularRecoveryTruncated = summary.circularRecoveryTruncated;
    report.featureNormalFilterIterationsCompleted = summary.normalFilter.iterationsCompleted;
    report.featureNormalFilterChangedFaces = summary.normalFilter.changedFaces;
    report.featureNormalFilterPreservedEdges = summary.normalFilter.preservedEdges;
    report.meanFeatureNormalFilterAngularChangeDeg = summary.normalFilter.meanAngularChangeDeg;
    report.maxFeatureNormalFilterAngularChangeDeg = summary.normalFilter.maxAngularChangeDeg;
    report.meanFeatureNormalFilterEdgeIndicator = summary.normalFilter.meanEdgeIndicator;
    report.graphConsolidationBridges = summary.graphConsolidationBridges;
    report.graphConsolidationSkippedByCap = summary.graphConsolidationSkippedByCap;
    report.junctionBranchPairs = summary.junctionBranchPairs;
    report.ambiguousFeatureJunctions = summary.ambiguousFeatureJunctions;
}

} // 结束 manumesh::simplification 命名空间
