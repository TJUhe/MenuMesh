/**
 * @file src/feature_detection/FeatureDetector.cpp
 * @brief 实现 ManuMesh 的特征检测模块的特征检测器功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 负责编排一次完整的特征分析，并在公共入口处统一校验选项和网格。
 * @algorithm 依次校验输入、构建共享几何缓存、收集边证据、初始化并清理轨迹图，
 *            可选合并相邻分量，然后恢复特征环、汇总分量与分支，最后按需分割曲面面片。
 * @invariants 所有阶段使用同一份只读网格及法向、邻接和局部尺度缓存；特征环 ID 单调递增。
 */

#include "algorithms/feature_detection/FeatureDetector.h"
#include "core/MathUtils.h"

#include "common/detail/MeshQueries.h"
// DebugUtil instrumentation is temporarily disabled.
// #include "detail/FeatureDebugInstrumentation.h"
#include "detail/FeatureDetectionCache.h"
#include "detail/FeatureDetectionTypes.h"
#include "detail/FeatureEvidence.h"
#include "detail/FeatureGraph.h"
#include "detail/FeatureGraphCleanup.h"
#include "detail/FeatureGraphConsolidation.h"
#include "detail/FeatureInputValidation.h"
#include "detail/FeatureLoopRecovery.h"
#include "detail/FeatureNormalFilter.h"
#include "detail/FeatureSegmentation.h"
#include "detail/PrimitiveFit.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace manumesh {
namespace feature {
namespace {

using detector_detail::CandidateEdge;
using detector_detail::FeatureAnalysisBuilder;
using detector_detail::FeatureDetectionCache;
using detector_detail::TraceGraph;

/** @brief 一次流水线运行所需的共享输入、缓存和中间结果。 */
struct FeatureDetectionContext {
    /** @brief 绑定网格和选项，并初始化检测缓存与分析构建器。 */
    FeatureDetectionContext(const Mesh& inputMesh, const FeatureOptions& inputOptions)
        : mesh(inputMesh),
          options(inputOptions),
          cache(inputMesh, inputOptions.normalFilter),
          builder(static_cast<int>(inputMesh.vertices.size())) {
        builder.analysis().source = featureAnalysisSource(inputMesh);
    }

    /** @brief 返回所有阶段共享的分析累加器。 */
    FeatureAnalysis& analysis() { return builder.analysis(); }

    const Mesh& mesh;
    const FeatureOptions& options;
    FeatureDetectionCache cache;
    FeatureAnalysisBuilder builder;
    std::vector<CandidateEdge> featureEdges;
    TraceGraph trace;
};

void requireFiniteNonNegative(double value, const char* name) {
    if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(std::string(name) + " must be finite and non-negative.");
    }
}

void validateFeatureOptionsImpl(const FeatureOptions& options) {
    if (!std::isfinite(options.featureAngleDeg) || options.featureAngleDeg < 0.0 || options.featureAngleDeg > 180.0) {
        throw std::invalid_argument("featureAngleDeg must be finite and in [0, 180].");
    }
    if (!std::isfinite(options.loopTraceAngleDeg) ||
        (options.loopTraceAngleDeg >= 0.0 && options.loopTraceAngleDeg > 180.0)) {
        throw std::invalid_argument("loopTraceAngleDeg must be negative or finite and in [0, 180].");
    }
    requireFiniteNonNegative(options.circleFitRelativeThreshold, "circleFitRelativeThreshold");
    requireFiniteNonNegative(options.ellipseFitRelativeThreshold, "ellipseFitRelativeThreshold");
    requireFiniteNonNegative(options.nearCircleAxisRatioTolerance, "nearCircleAxisRatioTolerance");
    requireFiniteNonNegative(options.normalTensorFeatureThreshold, "normalTensorFeatureThreshold");
    if (!std::isfinite(options.normalTensorMinEdgeAlignment) || options.normalTensorMinEdgeAlignment < 0.0 ||
        options.normalTensorMinEdgeAlignment > 1.0) {
        throw std::invalid_argument("normalTensorMinEdgeAlignment must be finite and in [0, 1].");
    }
    if (options.minFeatureLoopVertices < 3) {
        throw std::invalid_argument("minFeatureLoopVertices must be at least 3.");
    }
    if (options.normalTensorSmoothingIterations < 0 ||
        options.normalTensorSmoothingIterations > kMaxNormalTensorSmoothingIterations) {
        throw std::invalid_argument(
            "normalTensorSmoothingIterations must be in [0, " + std::to_string(kMaxNormalTensorSmoothingIterations) +
            "]."
        );
    }
    if (options.normalTensorScaleCount < 1 || options.normalTensorScaleCount > kMaxNormalTensorScaleCount) {
        throw std::invalid_argument(
            "normalTensorScaleCount must be in [1, " + std::to_string(kMaxNormalTensorScaleCount) + "]."
        );
    }
    if (options.normalTensorMinPersistentScales < 1 ||
        options.normalTensorMinPersistentScales > options.normalTensorScaleCount) {
        throw std::invalid_argument("normalTensorMinPersistentScales must be in [1, normalTensorScaleCount].");
    }
    requireFiniteNonNegative(options.smoothCurvatureFeatureThreshold, "smoothCurvatureFeatureThreshold");
    if (!std::isfinite(options.smoothCurvatureMinEdgeAlignment) || options.smoothCurvatureMinEdgeAlignment < 0.0 ||
        options.smoothCurvatureMinEdgeAlignment > 1.0) {
        throw std::invalid_argument("smoothCurvatureMinEdgeAlignment must be finite and in [0, 1].");
    }
    if (!std::isfinite(options.smoothCurvatureMinTangentConsistency) ||
        options.smoothCurvatureMinTangentConsistency < 0.0 || options.smoothCurvatureMinTangentConsistency > 1.0) {
        throw std::invalid_argument("smoothCurvatureMinTangentConsistency must be finite and in [0, 1].");
    }
    if (options.smoothCurvatureBaseNeighborhoodRings < 1 ||
        options.smoothCurvatureBaseNeighborhoodRings > kMaxSmoothCurvatureBaseNeighborhoodRings) {
        throw std::invalid_argument(
            "smoothCurvatureBaseNeighborhoodRings must be in [1, " +
            std::to_string(kMaxSmoothCurvatureBaseNeighborhoodRings) + "]."
        );
    }
    if (options.smoothCurvatureScaleCount < 1 || options.smoothCurvatureScaleCount > kMaxSmoothCurvatureScaleCount) {
        throw std::invalid_argument(
            "smoothCurvatureScaleCount must be in [1, " + std::to_string(kMaxSmoothCurvatureScaleCount) + "]."
        );
    }
    if (options.smoothCurvatureMinPersistentScales < 1 ||
        options.smoothCurvatureMinPersistentScales > options.smoothCurvatureScaleCount) {
        throw std::invalid_argument("smoothCurvatureMinPersistentScales must be in [1, smoothCurvatureScaleCount].");
    }
    if (options.smoothCurvatureRobustFitIterations < 0 ||
        options.smoothCurvatureRobustFitIterations > kMaxSmoothCurvatureRobustFitIterations) {
        throw std::invalid_argument(
            "smoothCurvatureRobustFitIterations must be in [0, " +
            std::to_string(kMaxSmoothCurvatureRobustFitIterations) + "]."
        );
    }
    if (!std::isfinite(options.smoothCurvatureMinScaleStability) || options.smoothCurvatureMinScaleStability < 0.0 ||
        options.smoothCurvatureMinScaleStability > 1.0) {
        throw std::invalid_argument("smoothCurvatureMinScaleStability must be finite and in [0, 1].");
    }
    requireFiniteNonNegative(options.featureGraphGapLengthRatio, "featureGraphGapLengthRatio");
    if (options.featureGraphMaxWeakSpurEdges < 0) {
        throw std::invalid_argument("featureGraphMaxWeakSpurEdges must be non-negative.");
    }
    if (!std::isfinite(options.featureComponentMinConfidence) || options.featureComponentMinConfidence < 0.0 ||
        options.featureComponentMinConfidence > 1.0) {
        throw std::invalid_argument("featureComponentMinConfidence must be finite and in [0, 1].");
    }
    requireFiniteNonNegative(options.featureGraphMinWeakSpurStrength, "featureGraphMinWeakSpurStrength");
    detector_detail::validateFeatureNormalFilterOptions(options.normalFilter);
    requireFiniteNonNegative(options.graphConsolidation.maxGapLengthRatio, "graphConsolidation.maxGapLengthRatio");
    if (!std::isfinite(options.graphConsolidation.minAlignment) || options.graphConsolidation.minAlignment < 0.0 ||
        options.graphConsolidation.minAlignment > 1.0) {
        throw std::invalid_argument("graphConsolidation.minAlignment must be finite and in [0, 1].");
    }
}

void validateFeatureInput(const Mesh& mesh) { detector_detail::validateFeatureMeshInput(mesh); }

/** @brief 收集边界、二面角、法向张量和曲率等边证据。 */
class EdgeEvidenceStage {
public:
    /** @brief 生成候选边，并更新证据统计信息。 */
    void run(FeatureDetectionContext& context) const {
        context.featureEdges =
            detector_detail::collectFeatureEdges(context.mesh, context.options, context.cache, context.builder);
    }
};

/**
 * @brief 降级依赖无效面法向的证据通道。
 *
 * 宽松输入校验允许零面积三角形存在，但其法向量为零，无法用于二面角计算；
 * 若直接参与点积，会被误判为 90 度的伪折痕。本阶段移除所有与退化面相邻
 * 内部边的二面角证据，并删除失去其他证据的候选边，同时保持分析计数一致。
 * 法向张量和光顺曲率在累加时已跳过退化面；边界与非流形证据只依赖拓扑，
 * 因此仅需降级二面角通道。被容忍的退化面数量仍记录在
 * FeatureAnalysis::degenerateFaces 中。
 */
class DegenerateEvidenceFilterStage {
public:
    /** @brief 移除与退化面相邻、依赖法向的边证据。 */
    void run(FeatureDetectionContext& context) const {
        if (context.analysis().degenerateFaces == 0) {
            return;
        }
        std::vector<char> degenerateFace(context.mesh.faces.size(), 0);
        const std::vector<Vec3>& faceNormals = context.cache.faceNormals();
        for (std::size_t faceIndex = 0; faceIndex < faceNormals.size(); ++faceIndex) {
            // triangleNormal 对退化面返回精确的零向量。
            if (faceNormals[faceIndex].squaredNorm() <= 0.0) {
                degenerateFace[faceIndex] = 1;
            }
        }

        const manumesh::common::MeshEdgeInfoMap& edgeInfo = context.cache.edgeInfo();
        FeatureAnalysis& analysis = context.analysis();
        std::vector<CandidateEdge> kept;
        kept.reserve(context.featureEdges.size());
        for (CandidateEdge edge : context.featureEdges) {
            if (edge.dihedral) {
                const auto it = edgeInfo.find(manumesh::common::meshEdgeKey(edge.a, edge.b));
                bool touchesDegenerate = false;
                if (it != edgeInfo.end()) {
                    for (int faceId : it->second.faces) {
                        if (faceId >= 0 && faceId < static_cast<int>(degenerateFace.size()) && degenerateFace[faceId]) {
                            touchesDegenerate = true;
                            break;
                        }
                    }
                }
                if (touchesDegenerate) {
                    --analysis.dihedralFeatureEdges;
                    if (edge.signedKind > 0) {
                        --analysis.convexFeatureEdges;
                    } else if (edge.signedKind < 0) {
                        --analysis.concaveFeatureEdges;
                    } else {
                        --analysis.unknownSignedFeatureEdges;
                    }
                    edge.dihedral = false;
                    edge.signedKind = 0;
                    edge.angleRad = 0.0;
                }
            }
            if (edge.boundary || edge.dihedral || edge.normalTensor || edge.smoothCurvature || edge.nonManifold) {
                kept.push_back(edge);
            } else {
                --analysis.featureEdges;
            }
        }
        context.featureEdges = std::move(kept);
    }
};

/** @brief 初始化顶点标记，并构建第一版特征轨迹图。 */
class FeatureGraphStage {
public:
    /** @brief 将已接受的证据边写入图存储。 */
    void run(FeatureDetectionContext& context) const {
        detector_detail::initializeFeatureGraph(context.featureEdges, context.analysis());
        context.trace =
            detector_detail::buildTraceGraph(context.mesh, context.options, context.featureEdges, context.analysis());
    }
};

/** @brief 按选项清理弱毛刺，并桥接兼容的短间隙。 */
class FeatureGraphCleanupStage {
public:
    /** @brief 根据图清理策略修改轨迹图。 */
    void run(FeatureDetectionContext& context) const {
        detector_detail::cleanupTraceGraph(
            context.mesh, context.options, context.cache, context.trace, context.analysis()
        );
    }
};

/** @brief 合并由可支持短间隙分隔、且证据兼容的图分量。 */
class FeatureGraphConsolidationStage {
public:
    /** @brief 添加被接受的合并桥，并更新诊断计数。 */
    void run(FeatureDetectionContext& context) const {
        detector_detail::consolidateFeatureGraph(
            context.mesh, context.options, context.cache, context.trace, context.analysis()
        );
    }
};

/** @brief 追踪图链并恢复符合条件的闭合特征环。 */
class LoopRecoveryStage {
public:
    /** @brief 使用单调递增 ID 追加恢复出的特征环。 */
    void run(FeatureDetectionContext& context) const {
        detector_detail::recoverFeatureLoops(
            context.mesh, context.options, context.trace, context.analysis(), context.builder.nextLoopId()
        );
    }
};

/** @brief 计算连通分量及分叉延续统计信息。 */
class FeatureComponentSummaryStage {
public:
    /** @brief 写入分量置信度和分支配对诊断信息。 */
    void run(FeatureDetectionContext& context) const {
        detector_detail::summarizeFeatureComponents(context.mesh, context.options, context.trace, context.analysis());
    }
};

/** @brief 在环和分量恢复后完成每个顶点的图标记。 */
class FeatureGraphFinalizeStage {
public:
    /** @brief 使公共顶点记录与最终图状态保持同步。 */
    void run(FeatureDetectionContext& context) const {
        detector_detail::finalizeFeatureGraphMarkers(context.mesh, context.analysis());
    }
};

/** @brief 可选地将曲面划分为由特征边界限定的面片。 */
class FeatureSegmentationStage {
public:
    /** @brief 启用面片分析时构建曲面面片。 */
    void run(FeatureDetectionContext& context) const {
        if (context.options.surfacePatches.enabled) {
            detector_detail::buildFeaturePatches(context.mesh, context.analysis(), context.options.surfacePatches);
        }
    }
};

/** @brief 按固定顺序协调全部特征检测阶段。 */
class FeatureDetectionPipeline {
public:
    /**
     * @brief 校验输入并执行一次完整、确定性的特征分析。
     * @param[in] mesh 待分析的三角形曲面。
     * @param[in] options 特征检测与恢复策略。
     * @return 已完成的特征分析结果。
     */
    FeatureAnalysis run(const Mesh& mesh, const FeatureOptions& options) const {
        validateFeatureOptionsImpl(options);
        validateFeatureInput(mesh);

        FeatureDetectionContext context(mesh, options);
        // 宽松校验允许零面积面存在；其法向不可用，证据阶段会跳过这些面。
        // 将退化面数量公开给调用方，以便识别覆盖范围受限的分析结果。
        context.analysis().degenerateFaces = countDegenerateFaces(mesh);
        if (mesh.empty()) {
            return context.builder.build();
        }

        edgeEvidence_.run(context);
        degenerateFilter_.run(context);
        featureGraph_.run(context);
        cleanup_.run(context);
        consolidation_.run(context);
        // DebugUtil snapshots are temporarily disabled; diagnostics remain
        // available through the structured FeatureAnalysis result.
        // MANUMESH_DEBUG_UTIL_FEATURES("05_consolidation", mesh, context.analysis());
        loopRecovery_.run(context);
        // MANUMESH_DEBUG_UTIL_FEATURES("06_consolidation", mesh, context.analysis());
        componentSummary_.run(context);
        finalize_.run(context);
        segmentation_.run(context);
        return context.builder.build();
    }

private:
    EdgeEvidenceStage edgeEvidence_;
    DegenerateEvidenceFilterStage degenerateFilter_;
    FeatureGraphStage featureGraph_;
    FeatureGraphCleanupStage cleanup_;
    FeatureGraphConsolidationStage consolidation_;
    LoopRecoveryStage loopRecovery_;
    FeatureComponentSummaryStage componentSummary_;
    FeatureGraphFinalizeStage finalize_;
    FeatureSegmentationStage segmentation_;
};

} // namespace

void validateFeatureOptions(const FeatureOptions& options) { validateFeatureOptionsImpl(options); }

constexpr std::uint64_t kFingerprintOffset = 14695981039346656037ull;
constexpr std::uint64_t kFingerprintPrime = 1099511628211ull;

void appendFingerprintByte(std::uint64_t& hash, std::uint8_t byte) {
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= kFingerprintPrime;
}

void appendFingerprintU64(std::uint64_t& hash, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        appendFingerprintByte(hash, static_cast<std::uint8_t>((value >> shift) & 0xffull));
    }
}

void appendFingerprintI32(std::uint64_t& hash, int value) {
    appendFingerprintU64(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(value)));
}

void appendFingerprintDouble(std::uint64_t& hash, double value) {
    // Coordinate validation rejects NaNs. Canonicalize signed zero because it
    // represents the same mesh geometry and commonly changes during IO.
    if (value == 0.0) {
        value = 0.0;
    }
    static_assert(sizeof(double) == sizeof(std::uint64_t), "Feature fingerprints require binary64 doubles.");
    static_assert(std::numeric_limits<double>::is_iec559, "Feature fingerprints require IEEE-754 doubles.");
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    appendFingerprintU64(hash, bits);
}

[[noreturn]] void invalidFeatureAnalysis(const std::string& detail) {
    throw std::invalid_argument("FeatureAnalysis is incompatible with the mesh: " + detail);
}

void requireVertexId(int id, std::size_t vertexCount, const std::string& owner) {
    if (id < 0 || static_cast<std::size_t>(id) >= vertexCount) {
        invalidFeatureAnalysis(owner + " references an invalid vertex index.");
    }
}

void requireLoopId(int id, std::size_t loopCount, const std::string& owner) {
    if (id < 0 || static_cast<std::size_t>(id) >= loopCount) {
        invalidFeatureAnalysis(owner + " references an invalid loop id.");
    }
}

void requireComponentId(int id, std::size_t componentCount, const std::string& owner) {
    if (id < 0 || static_cast<std::size_t>(id) >= componentCount) {
        invalidFeatureAnalysis(owner + " references an invalid component id.");
    }
}

std::vector<int> sortedUniqueIds(const std::vector<int>& ids, const std::string& duplicateDetail) {
    std::vector<int> sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) {
        invalidFeatureAnalysis(duplicateDetail);
    }
    return sorted;
}

bool isCircularPrimitive(FeaturePrimitiveType primitive) {
    return primitive == FeaturePrimitiveType::Circle || primitive == FeaturePrimitiveType::NearCircle;
}

void requireValidPrimitive(FeaturePrimitiveType primitive, const std::string& owner) {
    switch (primitive) {
    case FeaturePrimitiveType::Unknown:
    case FeaturePrimitiveType::Circle:
    case FeaturePrimitiveType::NearCircle:
    case FeaturePrimitiveType::Ellipse:
    case FeaturePrimitiveType::PolygonalLoop:
        return;
    }
    invalidFeatureAnalysis(owner + " contains an invalid primitive type.");
}

void requireFiniteValue(double value, const std::string& owner) {
    if (!std::isfinite(value)) {
        invalidFeatureAnalysis(owner + " must be finite.");
    }
}

void requireFiniteNonNegativeValue(double value, const std::string& owner) {
    if (!std::isfinite(value) || value < 0.0) {
        invalidFeatureAnalysis(owner + " must be finite and non-negative.");
    }
}

void requireUnitIntervalValue(double value, const std::string& owner) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        invalidFeatureAnalysis(owner + " must be finite and in [0, 1].");
    }
}

void requireFiniteVector(const Vec3& value, const std::string& owner) {
    if (!value.allFinite() || !std::isfinite(value.norm())) {
        invalidFeatureAnalysis(owner + " must contain finite, numerically usable coordinates.");
    }
}

void requireUsableDirection(const Vec3& value, const std::string& owner) {
    requireFiniteVector(value, owner);
    if (value.norm() <= 1e-20) {
        invalidFeatureAnalysis(owner + " must be non-zero.");
    }
}

bool nearlyEqual(double lhs, double rhs, double relativeTolerance = 1e-10) {
    return std::abs(lhs - rhs) <= relativeTolerance * std::max({1.0, std::abs(lhs), std::abs(rhs)});
}

double expectedClosureRate(int endpointCount, int cycleRank) {
    if (endpointCount <= 0) {
        return 1.0;
    }
    const double endpointPenalty = manumesh::clampValue(1.0 - 0.25 * static_cast<double>(endpointCount), 0.0, 1.0);
    return cycleRank > 0 ? endpointPenalty : 0.5 * endpointPenalty;
}

void validateVertexFeatureValues(const VertexFeature& vertex) {
    requireValidPrimitive(vertex.primitive, "Vertex feature");
    requireUnitIntervalValue(vertex.confidence, "Vertex feature confidence");
    requireFiniteVector(vertex.tangent, "Vertex feature tangent");
    requireFiniteVector(vertex.circleCenter, "Vertex feature circle center");
    requireFiniteVector(vertex.circleNormal, "Vertex feature circle normal");
    requireFiniteNonNegativeValue(vertex.circleRadius, "Vertex feature circle radius");
    requireFiniteVector(vertex.ellipseCenter, "Vertex feature ellipse center");
    requireFiniteVector(vertex.ellipseNormal, "Vertex feature ellipse normal");
    requireFiniteVector(vertex.ellipseMajorAxis, "Vertex feature ellipse major axis");
    requireFiniteVector(vertex.ellipseMinorAxis, "Vertex feature ellipse minor axis");
    requireFiniteNonNegativeValue(vertex.ellipseMajorRadius, "Vertex feature ellipse major radius");
    requireFiniteNonNegativeValue(vertex.ellipseMinorRadius, "Vertex feature ellipse minor radius");

    if (vertex.circular != isCircularPrimitive(vertex.primitive)) {
        invalidFeatureAnalysis("vertex feature circular marker does not match its primitive type.");
    }
    if (isCircularPrimitive(vertex.primitive)) {
        if (vertex.circleRadius <= 1e-20) {
            invalidFeatureAnalysis("circular vertex feature must contain a positive circle radius.");
        }
        requireUsableDirection(vertex.circleNormal, "Vertex feature circle normal");
    }
    if (vertex.primitive == FeaturePrimitiveType::Ellipse) {
        if (vertex.ellipseMajorRadius <= 1e-20 || vertex.ellipseMinorRadius <= 1e-20) {
            invalidFeatureAnalysis("elliptic vertex feature must contain positive ellipse radii.");
        }
        requireUsableDirection(vertex.ellipseNormal, "Vertex feature ellipse normal");
        requireUsableDirection(vertex.ellipseMajorAxis, "Vertex feature ellipse major axis");
        requireUsableDirection(vertex.ellipseMinorAxis, "Vertex feature ellipse minor axis");
    }
}

void validateFeatureLoopValues(const FeatureLoop& loop) {
    requireValidPrimitive(loop.primitive, "Feature loop");
    requireUnitIntervalValue(loop.componentConfidence, "Feature loop component confidence");
    requireFiniteNonNegativeValue(loop.primitiveResidual, "Feature loop primitive residual");
    requireFiniteVector(loop.center, "Feature loop center");
    requireFiniteVector(loop.normal, "Feature loop normal");
    requireFiniteVector(loop.majorAxis, "Feature loop major axis");
    requireFiniteVector(loop.minorAxis, "Feature loop minor axis");
    requireFiniteNonNegativeValue(loop.radius, "Feature loop radius");
    requireFiniteNonNegativeValue(loop.majorRadius, "Feature loop major radius");
    requireFiniteNonNegativeValue(loop.minorRadius, "Feature loop minor radius");
    requireFiniteNonNegativeValue(loop.axisRatio, "Feature loop axis ratio");
    requireFiniteNonNegativeValue(loop.rmsRadialError, "Feature loop RMS radial error");
    requireFiniteNonNegativeValue(loop.maxRadialError, "Feature loop maximum radial error");
    requireFiniteNonNegativeValue(loop.rmsEllipseError, "Feature loop RMS ellipse error");
    requireFiniteNonNegativeValue(loop.maxEllipseError, "Feature loop maximum ellipse error");
    requireFiniteNonNegativeValue(loop.rmsPlaneError, "Feature loop RMS plane error");
    requireFiniteNonNegativeValue(loop.maxPlaneError, "Feature loop maximum plane error");

    if (loop.circular != isCircularPrimitive(loop.primitive)) {
        invalidFeatureAnalysis("feature loop circular marker does not match its primitive type.");
    }
    if (loop.primitive != FeaturePrimitiveType::Unknown && !loop.closed) {
        invalidFeatureAnalysis("fitted feature primitive must belong to a closed loop.");
    }
    if (isCircularPrimitive(loop.primitive)) {
        if (loop.radius <= 1e-20) {
            invalidFeatureAnalysis("circular feature loop must contain a positive radius.");
        }
        requireUsableDirection(loop.normal, "Feature loop normal");
    }
    if (loop.primitive == FeaturePrimitiveType::Ellipse) {
        if (loop.majorRadius <= 1e-20 || loop.minorRadius <= 1e-20) {
            invalidFeatureAnalysis("elliptic feature loop must contain positive radii.");
        }
        requireUsableDirection(loop.normal, "Feature loop normal");
        requireUsableDirection(loop.majorAxis, "Feature loop major axis");
        requireUsableDirection(loop.minorAxis, "Feature loop minor axis");
    }
}

void validateFeatureComponentValues(const FeatureComponent& component) {
    const int counts[] = {
        component.edgeCount,
        component.boundaryEdges,
        component.dihedralEdges,
        component.normalTensorEdges,
        component.smoothCurvatureEdges,
        component.nonManifoldEdges,
        component.cleanupBridgeEdges,
        component.consolidationBridgeEdges,
        component.strongEvidenceEdges,
        component.weakEvidenceEdges,
        component.junctionVertices,
        component.endpointVertices,
        component.cycleRank,
    };
    if (std::any_of(std::begin(counts), std::end(counts), [](int value) {
            return value < 0;
        })) {
        invalidFeatureAnalysis("feature component contains a negative count.");
    }
    requireUnitIntervalValue(component.closureRate, "Feature component closure rate");
    requireUnitIntervalValue(component.strongEvidenceRatio, "Feature component strong-evidence ratio");
    requireFiniteNonNegativeValue(component.meanTensorPersistence, "Feature component tensor persistence");
    requireFiniteNonNegativeValue(component.meanCurvaturePersistence, "Feature component curvature persistence");
    requireFiniteNonNegativeValue(component.meanPrimitiveResidual, "Feature component primitive residual");
    requireUnitIntervalValue(component.confidence, "Feature component confidence");
}

std::vector<int>
validateFeatureComponents(const FeatureAnalysis& analysis, std::size_t vertexCount, std::size_t componentCount) {
    std::vector<int> componentByVertex(vertexCount, -1);
    std::vector<char> componentIds(componentCount, 0);
    for (const FeatureComponent& component : analysis.components) {
        requireComponentId(component.id, componentCount, "Feature component");
        if (componentIds[static_cast<std::size_t>(component.id)] != 0) {
            invalidFeatureAnalysis("feature component ids are not unique.");
        }
        componentIds[static_cast<std::size_t>(component.id)] = 1;
        validateFeatureComponentValues(component);
        for (int vertexId : component.vertices) {
            requireVertexId(vertexId, vertexCount, "Feature component");
        }
        const std::vector<int> uniqueVertices =
            sortedUniqueIds(component.vertices, "feature component contains duplicate vertex indices.");
        if (uniqueVertices.empty()) {
            invalidFeatureAnalysis("feature component must contain at least one vertex.");
        }
        for (int vertexId : uniqueVertices) {
            int& owner = componentByVertex[static_cast<std::size_t>(vertexId)];
            if (owner != -1) {
                invalidFeatureAnalysis("feature components overlap at a vertex.");
            }
            owner = component.id;
        }

        if (component.edgeCount <= 0 || component.edgeCount < static_cast<int>(uniqueVertices.size()) - 1) {
            invalidFeatureAnalysis("feature component edge count cannot form its recorded connected vertex set.");
        }
        const int expectedCycleRank = component.edgeCount - static_cast<int>(uniqueVertices.size()) + 1;
        if (component.cycleRank != expectedCycleRank) {
            invalidFeatureAnalysis("feature component cycle rank does not match its vertices and edges.");
        }
        if (component.strongEvidenceEdges !=
            component.boundaryEdges + component.dihedralEdges + component.nonManifoldEdges) {
            invalidFeatureAnalysis("feature component strong-evidence count does not match its source counts.");
        }
        if (component.weakEvidenceEdges != component.normalTensorEdges + component.smoothCurvatureEdges +
                                               component.cleanupBridgeEdges + component.consolidationBridgeEdges) {
            invalidFeatureAnalysis("feature component weak-evidence count does not match its source counts.");
        }
        const int perEdgeCounts[] = {
            component.boundaryEdges,
            component.dihedralEdges,
            component.normalTensorEdges,
            component.smoothCurvatureEdges,
            component.nonManifoldEdges,
            component.cleanupBridgeEdges,
            component.consolidationBridgeEdges,
        };
        if (std::any_of(std::begin(perEdgeCounts), std::end(perEdgeCounts), [&](int count) {
                return count > component.edgeCount;
            })) {
            invalidFeatureAnalysis("feature component source count exceeds its edge count.");
        }
        if (component.junctionVertices + component.endpointVertices > static_cast<int>(uniqueVertices.size())) {
            invalidFeatureAnalysis("feature component endpoint/junction counts exceed its vertex count.");
        }
        const bool expectedClosed = component.endpointVertices == 0 && component.cycleRank >= 0;
        if (component.closed != expectedClosed) {
            invalidFeatureAnalysis("feature component closed marker does not match its topology counts.");
        }
        if (!nearlyEqual(component.closureRate, expectedClosureRate(component.endpointVertices, component.cycleRank))) {
            invalidFeatureAnalysis("feature component closure rate does not match its topology counts.");
        }
        const double expectedStrongRatio =
            static_cast<double>(component.strongEvidenceEdges) / static_cast<double>(component.edgeCount);
        if (!nearlyEqual(component.strongEvidenceRatio, expectedStrongRatio)) {
            invalidFeatureAnalysis("feature component strong-evidence ratio does not match its counts.");
        }
    }

    struct ComponentGraphCounts {
        int forcedEdges = 0;
        int optionalDihedralEdges = 0;
        int forcedDihedralEdges = 0;
        int boundaryEdges = 0;
        int normalTensorEdges = 0;
        int smoothCurvatureEdges = 0;
        int nonManifoldEdges = 0;
        int cleanupBridgeEdges = 0;
        int consolidationBridgeEdges = 0;
    };
    std::vector<ComponentGraphCounts> graphCounts(componentCount);
    std::vector<std::vector<int>> possibleAdjacency(vertexCount);
    for (const FeatureGraphEdge& edge : analysis.graph.edges) {
        if (edge.removedByCleanup) {
            continue;
        }
        const int componentA = componentByVertex[static_cast<std::size_t>(edge.a)];
        const int componentB = componentByVertex[static_cast<std::size_t>(edge.b)];
        const bool forcedTrace = edge.boundary || edge.normalTensor || edge.smoothCurvature || edge.nonManifold ||
                                 edge.cleanupBridge || edge.consolidationBridge;
        if (forcedTrace && (componentA < 0 || componentA != componentB)) {
            invalidFeatureAnalysis("a mandatory traced feature edge crosses recorded feature components.");
        }
        if (componentA < 0 || componentA != componentB) {
            continue;
        }

        possibleAdjacency[static_cast<std::size_t>(edge.a)].push_back(edge.b);
        possibleAdjacency[static_cast<std::size_t>(edge.b)].push_back(edge.a);
        ComponentGraphCounts& counts = graphCounts[static_cast<std::size_t>(componentA)];
        if (forcedTrace) {
            ++counts.forcedEdges;
            if (edge.dihedral) {
                ++counts.forcedDihedralEdges;
            }
            if (edge.boundary) {
                ++counts.boundaryEdges;
            }
            if (edge.normalTensor) {
                ++counts.normalTensorEdges;
            }
            if (edge.smoothCurvature) {
                ++counts.smoothCurvatureEdges;
            }
            if (edge.nonManifold) {
                ++counts.nonManifoldEdges;
            }
            if (edge.cleanupBridge) {
                ++counts.cleanupBridgeEdges;
            }
            if (edge.consolidationBridge) {
                ++counts.consolidationBridgeEdges;
            }
        } else {
            ++counts.optionalDihedralEdges;
        }
    }

    for (const FeatureComponent& component : analysis.components) {
        const ComponentGraphCounts& counts = graphCounts[static_cast<std::size_t>(component.id)];
        const int optionalTracedEdges = component.edgeCount - counts.forcedEdges;
        if (optionalTracedEdges < 0 || optionalTracedEdges > counts.optionalDihedralEdges) {
            invalidFeatureAnalysis("feature component edge count is incompatible with the public feature graph.");
        }
        if (component.boundaryEdges != counts.boundaryEdges ||
            component.normalTensorEdges != counts.normalTensorEdges ||
            component.smoothCurvatureEdges != counts.smoothCurvatureEdges ||
            component.nonManifoldEdges != counts.nonManifoldEdges ||
            component.cleanupBridgeEdges != counts.cleanupBridgeEdges ||
            component.consolidationBridgeEdges != counts.consolidationBridgeEdges ||
            component.dihedralEdges != counts.forcedDihedralEdges + optionalTracedEdges) {
            invalidFeatureAnalysis("feature component source counts do not match compatible graph edges.");
        }

        std::vector<char> visited(vertexCount, 0);
        std::queue<int> queue;
        queue.push(component.vertices.front());
        visited[static_cast<std::size_t>(component.vertices.front())] = 1;
        int reached = 0;
        while (!queue.empty()) {
            const int vertexId = queue.front();
            queue.pop();
            ++reached;
            for (int neighbor : possibleAdjacency[static_cast<std::size_t>(vertexId)]) {
                if (componentByVertex[static_cast<std::size_t>(neighbor)] == component.id &&
                    visited[static_cast<std::size_t>(neighbor)] == 0) {
                    visited[static_cast<std::size_t>(neighbor)] = 1;
                    queue.push(neighbor);
                }
            }
        }
        if (reached != static_cast<int>(component.vertices.size())) {
            invalidFeatureAnalysis("feature component vertices are disconnected in the public feature graph.");
        }
    }

    for (std::size_t vertexId = 0; vertexId < vertexCount; ++vertexId) {
        const int componentId = componentByVertex[vertexId];
        const VertexFeature& vertex = analysis.vertices[vertexId];
        if (componentId >= 0) {
            if (!vertex.isFeature || vertex.componentId != componentId) {
                invalidFeatureAnalysis("feature component vertices do not match per-vertex component ownership.");
            }
        } else if (vertex.componentId != -1) {
            invalidFeatureAnalysis("vertex feature references a component that does not contain it.");
        }
    }
    return componentByVertex;
}

void validateFeatureLoopMembership(
    const Mesh& mesh,
    const FeatureAnalysis& analysis,
    std::size_t vertexCount,
    std::size_t componentCount,
    const std::vector<int>& componentByVertex
) {
    const std::size_t loopCount = analysis.loops.size();
    std::vector<char> loopIds(loopCount, 0);
    std::vector<std::vector<int>> expectedLoopIds(vertexCount);
    std::unordered_set<std::uint64_t> activeGraphEdges;
    activeGraphEdges.reserve(analysis.graph.edges.size());
    for (const FeatureGraphEdge& edge : analysis.graph.edges) {
        if (!edge.removedByCleanup) {
            activeGraphEdges.insert(manumesh::common::meshEdgeKey(edge.a, edge.b));
        }
    }
    for (const FeatureLoop& loop : analysis.loops) {
        requireLoopId(loop.id, loopCount, "Feature loop");
        if (loopIds[static_cast<std::size_t>(loop.id)] != 0) {
            invalidFeatureAnalysis("feature loop ids are not unique.");
        }
        loopIds[static_cast<std::size_t>(loop.id)] = 1;
        if (loop.componentId != -1) {
            requireComponentId(loop.componentId, componentCount, "Feature loop");
        }
        validateFeatureLoopValues(loop);

        for (int vertexId : loop.vertices) {
            requireVertexId(vertexId, vertexCount, "Feature loop");
        }
        const std::vector<int> uniqueVertices =
            sortedUniqueIds(loop.vertices, "feature loop contains duplicate vertex indices.");
        const std::size_t minimumVertices = loop.closed ? 3u : 2u;
        if (loop.vertices.size() < minimumVertices) {
            invalidFeatureAnalysis("feature loop contains too few vertices for its open/closed state.");
        }
        const std::size_t expectedEdgeCount = loop.closed ? loop.vertices.size() : loop.vertices.size() - 1u;
        if (loop.edgeCount < 0 || static_cast<std::size_t>(loop.edgeCount) != expectedEdgeCount) {
            invalidFeatureAnalysis("feature loop edge count does not match its ordered vertex path.");
        }
        double unsupportedCircularAngle = 0.0;
        Vec3 circularNormal = loop.normal;
        if (loop.circular) {
            circularNormal.normalize();
        }
        for (std::size_t index = 0; index < expectedEdgeCount; ++index) {
            const int a = loop.vertices[index];
            const int b = loop.vertices[(index + 1u) % loop.vertices.size()];
            if (activeGraphEdges.find(manumesh::common::meshEdgeKey(a, b)) == activeGraphEdges.end()) {
                if (!loop.circular) {
                    invalidFeatureAnalysis("feature loop vertex order does not follow active feature graph edges.");
                }
                Vec3 radialA = mesh.vertices[static_cast<std::size_t>(a)] - loop.center;
                Vec3 radialB = mesh.vertices[static_cast<std::size_t>(b)] - loop.center;
                radialA -= circularNormal * radialA.dot(circularNormal);
                radialB -= circularNormal * radialB.dot(circularNormal);
                if (!radialA.allFinite() || !radialB.allFinite() || radialA.norm() <= 1e-20 ||
                    radialB.norm() <= 1e-20) {
                    invalidFeatureAnalysis("circular recovery gap has invalid radial geometry.");
                }
                const double cosine = manumesh::clampValue(radialA.normalized().dot(radialB.normalized()), -1.0, 1.0);
                unsupportedCircularAngle += std::acos(cosine);
            }
        }
        constexpr double kMaximumCircularRecoveryGap = 0.5 * 3.141592653589793238462643383279502884;
        if (unsupportedCircularAngle > kMaximumCircularRecoveryGap + 1e-10) {
            invalidFeatureAnalysis("circular feature loop exceeds the supported recovery gap.");
        }
        if (loop.componentId == -1) {
            invalidFeatureAnalysis("feature loop is missing its feature component ownership.");
        }
        const FeatureComponent& component = analysis.components[static_cast<std::size_t>(loop.componentId)];
        const bool expectedWeak = component.weakEvidenceEdges > component.strongEvidenceEdges;
        if (loop.componentConfidence != component.confidence || loop.weakFeature != expectedWeak) {
            invalidFeatureAnalysis("feature loop confidence does not match its feature component.");
        }
        for (int vertexId : uniqueVertices) {
            if (componentByVertex[static_cast<std::size_t>(vertexId)] != loop.componentId) {
                invalidFeatureAnalysis("feature loop vertices do not belong to its feature component.");
            }
            expectedLoopIds[static_cast<std::size_t>(vertexId)].push_back(loop.id);
        }
    }

    for (std::size_t vertexId = 0; vertexId < vertexCount; ++vertexId) {
        std::vector<int>& expected = expectedLoopIds[vertexId];
        std::sort(expected.begin(), expected.end());

        const FeatureGraphVertex& graphVertex = analysis.graph.vertices[vertexId];
        for (int loopId : graphVertex.loopIds) {
            requireLoopId(loopId, loopCount, "Feature graph vertex");
        }
        const std::vector<int> actual =
            sortedUniqueIds(graphVertex.loopIds, "feature graph vertex contains duplicate loop ids.");
        if (actual != expected) {
            invalidFeatureAnalysis("feature graph loop ownership does not match recovered loops.");
        }

        const VertexFeature& vertex = analysis.vertices[vertexId];
        validateVertexFeatureValues(vertex);
        if (expected.empty()) {
            if (vertex.loopId != -1) {
                invalidFeatureAnalysis("vertex feature ownership does not match recovered loops.");
            }
            if (vertex.isFeature) {
                const bool hasActiveGraphIncidence =
                    std::any_of(graphVertex.incidentEdges.begin(), graphVertex.incidentEdges.end(), [&](int edgeId) {
                        return edgeId >= 0 && static_cast<std::size_t>(edgeId) < analysis.graph.edges.size() &&
                               !analysis.graph.edges[static_cast<std::size_t>(edgeId)].removedByCleanup;
                    });
                if (!hasActiveGraphIncidence) {
                    invalidFeatureAnalysis("untraced feature vertex has no active feature graph incidence.");
                }
            }
        } else {
            if (!vertex.isFeature || vertex.loopId == -1) {
                invalidFeatureAnalysis("vertex feature ownership does not match recovered loops.");
            }
            requireLoopId(vertex.loopId, loopCount, "Vertex feature");
            if (!std::binary_search(expected.begin(), expected.end(), vertex.loopId)) {
                invalidFeatureAnalysis("vertex feature primary loop does not contain the vertex.");
            }
            if (vertex.componentId == -1) {
                invalidFeatureAnalysis("recovered feature vertex is missing its component ownership.");
            }
        }
        const bool expectedVertexJunction = vertex.isFeature && (graphVertex.junction || graphVertex.shared);
        if (vertex.junction != expectedVertexJunction) {
            invalidFeatureAnalysis("vertex feature junction marker does not match the feature graph.");
        }
        if (vertex.componentId != -1) {
            requireComponentId(vertex.componentId, componentCount, "Vertex feature");
            if (componentByVertex[vertexId] != vertex.componentId) {
                invalidFeatureAnalysis("vertex feature component ownership does not match feature components.");
            }
            const FeatureComponent& component = analysis.components[static_cast<std::size_t>(vertex.componentId)];
            const bool expectedWeak = component.weakEvidenceEdges > component.strongEvidenceEdges;
            if (vertex.confidence != component.confidence || vertex.weakFeature != expectedWeak) {
                invalidFeatureAnalysis("vertex feature confidence does not match its feature component.");
            }
        } else if (!vertex.isFeature && vertex.confidence != 0.0) {
            invalidFeatureAnalysis("non-feature vertex contains feature confidence without component ownership.");
        }
    }
}

void validateMarkerList(
    const std::vector<int>& actual, const std::vector<int>& expected, std::size_t vertexCount, const std::string& owner
) {
    for (int vertexId : actual) {
        requireVertexId(vertexId, vertexCount, owner);
    }
    if (sortedUniqueIds(actual, owner + " contains duplicate vertex ids.") != expected) {
        invalidFeatureAnalysis(owner + " does not match per-vertex graph markers.");
    }
}

void validateFeatureGraph(const Mesh& mesh, const FeatureAnalysis& analysis) {
    const std::size_t vertexCount = mesh.vertices.size();
    const std::size_t edgeCount = analysis.graph.edges.size();
    std::vector<unsigned char> endpointIncidence(edgeCount, 0);
    const common::MeshEdgeInfoMap meshEdges = common::buildMeshEdgeInfo(mesh);
    std::unordered_set<std::uint64_t> graphEdgeKeys;
    graphEdgeKeys.reserve(edgeCount);

    for (const FeatureGraphEdge& edge : analysis.graph.edges) {
        requireVertexId(edge.a, vertexCount, "Feature graph edge");
        requireVertexId(edge.b, vertexCount, "Feature graph edge");
        if (edge.a == edge.b) {
            invalidFeatureAnalysis("feature graph edge has identical endpoints.");
        }
        const std::uint64_t key = common::meshEdgeKey(edge.a, edge.b);
        if (!graphEdgeKeys.insert(key).second) {
            invalidFeatureAnalysis("feature graph contains duplicate undirected edges.");
        }
        if (edge.signedKind < -1 || edge.signedKind > 1) {
            invalidFeatureAnalysis("feature graph edge contains an invalid signed kind.");
        }
        if (edge.cleanupBridge && edge.consolidationBridge) {
            invalidFeatureAnalysis("feature graph edge cannot be both a cleanup and consolidation bridge.");
        }
        const bool recoveryBridge = edge.cleanupBridge || edge.consolidationBridge;
        const bool evidence =
            edge.boundary || edge.dihedral || edge.normalTensor || edge.smoothCurvature || edge.nonManifold;
        if (!recoveryBridge) {
            if (!evidence) {
                invalidFeatureAnalysis("feature graph evidence edge has no evidence source.");
            }
            if (meshEdges.find(key) == meshEdges.end()) {
                invalidFeatureAnalysis("feature graph evidence edge is not an edge of the source mesh.");
            }
        }
    }

    std::vector<int> expectedJunctions;
    std::vector<int> expectedSharedVertices;
    std::vector<int> expectedEndpoints;
    int expectedBranchPairs = 0;
    int expectedAmbiguousJunctions = 0;
    for (std::size_t vertexId = 0; vertexId < analysis.graph.vertices.size(); ++vertexId) {
        const FeatureGraphVertex& graphVertex = analysis.graph.vertices[vertexId];
        for (int edgeId : graphVertex.incidentEdges) {
            if (edgeId < 0 || static_cast<std::size_t>(edgeId) >= edgeCount) {
                invalidFeatureAnalysis("feature graph vertex references an invalid edge id.");
            }
            const FeatureGraphEdge& edge = analysis.graph.edges[static_cast<std::size_t>(edgeId)];
            const int owner = static_cast<int>(vertexId);
            if (edge.a != owner && edge.b != owner) {
                invalidFeatureAnalysis("feature graph incident edge does not connect its owner vertex.");
            }
            const unsigned char endpointBit = edge.a == owner ? 0x1u : 0x2u;
            unsigned char& incidence = endpointIncidence[static_cast<std::size_t>(edgeId)];
            if ((incidence & endpointBit) != 0) {
                invalidFeatureAnalysis("feature graph vertex contains a duplicate incident edge.");
            }
            incidence = static_cast<unsigned char>(incidence | endpointBit);
        }
        const std::vector<int> incidentEdges =
            sortedUniqueIds(graphVertex.incidentEdges, "feature graph vertex contains a duplicate incident edge.");

        int activeIncidentEdges = 0;
        std::vector<int> expectedBranchEdges;
        for (int edgeId : incidentEdges) {
            const FeatureGraphEdge& edge = analysis.graph.edges[static_cast<std::size_t>(edgeId)];
            if (edge.removedByCleanup) {
                continue;
            }
            ++activeIncidentEdges;
            const int owner = static_cast<int>(vertexId);
            const int neighbor = edge.a == owner ? edge.b : edge.a;
            if ((mesh.vertices[static_cast<std::size_t>(neighbor)] - mesh.vertices[vertexId]).squaredNorm() > 1e-30) {
                expectedBranchEdges.push_back(edgeId);
            }
        }

        std::vector<int> actualBranchEdges;
        actualBranchEdges.reserve(graphVertex.branches.size());
        for (const FeatureGraphBranch& branch : graphVertex.branches) {
            if (branch.edgeId < 0 || static_cast<std::size_t>(branch.edgeId) >= edgeCount) {
                invalidFeatureAnalysis("feature graph branch references an invalid edge id.");
            }
            requireVertexId(branch.neighborVertex, vertexCount, "Feature graph branch");
            const FeatureGraphEdge& edge = analysis.graph.edges[static_cast<std::size_t>(branch.edgeId)];
            const int owner = static_cast<int>(vertexId);
            if (edge.a != owner && edge.b != owner) {
                invalidFeatureAnalysis("feature graph branch edge does not connect its owner vertex.");
            }
            const int expectedNeighbor = edge.a == owner ? edge.b : edge.a;
            if (branch.neighborVertex != expectedNeighbor) {
                invalidFeatureAnalysis("feature graph branch neighbor does not match its edge.");
            }
            if (edge.removedByCleanup ||
                !std::binary_search(incidentEdges.begin(), incidentEdges.end(), branch.edgeId)) {
                invalidFeatureAnalysis("feature graph branch does not reference an active incident edge.");
            }
            if (branch.signedKind != edge.signedKind) {
                invalidFeatureAnalysis("feature graph branch sign does not match its edge.");
            }
            requireUsableDirection(branch.tangent, "Feature graph branch tangent");
            actualBranchEdges.push_back(branch.edgeId);
        }
        actualBranchEdges = sortedUniqueIds(actualBranchEdges, "feature graph vertex contains duplicate branch edges.");
        if (actualBranchEdges != expectedBranchEdges) {
            invalidFeatureAnalysis("feature graph branches do not match active incident edges.");
        }

        std::vector<char> pairedBranches(graphVertex.branches.size(), 0);
        for (const FeatureGraphBranchPair& pair : graphVertex.branchPairs) {
            if (pair.firstBranch < 0 || pair.secondBranch < 0 ||
                static_cast<std::size_t>(pair.firstBranch) >= graphVertex.branches.size() ||
                static_cast<std::size_t>(pair.secondBranch) >= graphVertex.branches.size() ||
                pair.firstBranch == pair.secondBranch) {
                invalidFeatureAnalysis("feature graph branch pair references an invalid branch id.");
            }
            if (pairedBranches[static_cast<std::size_t>(pair.firstBranch)] != 0 ||
                pairedBranches[static_cast<std::size_t>(pair.secondBranch)] != 0) {
                invalidFeatureAnalysis("feature graph branch pairs reuse a branch.");
            }
            requireUnitIntervalValue(pair.alignment, "Feature graph branch-pair alignment");
            pairedBranches[static_cast<std::size_t>(pair.firstBranch)] = 1;
            pairedBranches[static_cast<std::size_t>(pair.secondBranch)] = 1;
        }

        const bool expectedJunction = activeIncidentEdges > 2;
        const bool expectedShared = graphVertex.loopIds.size() > 1;
        const bool expectedEndpoint = activeIncidentEdges == 1;
        if (graphVertex.junction != expectedJunction || graphVertex.shared != expectedShared ||
            graphVertex.endpoint != expectedEndpoint) {
            invalidFeatureAnalysis("feature graph vertex markers do not match graph incidence and loop ownership.");
        }
        if (!expectedJunction && !graphVertex.branchPairs.empty()) {
            invalidFeatureAnalysis("non-junction feature graph vertex contains branch pairs.");
        }
        const int unpairedBranches =
            static_cast<int>(graphVertex.branches.size()) - 2 * static_cast<int>(graphVertex.branchPairs.size());
        const bool expectedAmbiguous = expectedJunction && unpairedBranches > 1;
        if (graphVertex.ambiguousJunction != expectedAmbiguous) {
            invalidFeatureAnalysis("feature graph ambiguous-junction marker does not match branch pairs.");
        }

        const int owner = static_cast<int>(vertexId);
        if (expectedJunction) {
            expectedJunctions.push_back(owner);
        }
        if (expectedShared) {
            expectedSharedVertices.push_back(owner);
        }
        if (expectedEndpoint) {
            expectedEndpoints.push_back(owner);
        }
        expectedBranchPairs += static_cast<int>(graphVertex.branchPairs.size());
        if (expectedAmbiguous) {
            ++expectedAmbiguousJunctions;
        }
    }

    for (unsigned char incidence : endpointIncidence) {
        if (incidence != 0x3u) {
            invalidFeatureAnalysis("feature graph edge is missing incidence at one or both endpoints.");
        }
    }
    validateMarkerList(analysis.graph.junctionVertices, expectedJunctions, vertexCount, "Feature graph junction list");
    validateMarkerList(
        analysis.graph.sharedVertices, expectedSharedVertices, vertexCount, "Feature graph shared-vertex list"
    );
    validateMarkerList(analysis.graph.endpointVertices, expectedEndpoints, vertexCount, "Feature graph endpoint list");
    if (analysis.junctionBranchPairs != expectedBranchPairs) {
        invalidFeatureAnalysis("feature graph branch-pair count does not match per-vertex branch pairs.");
    }
    if (analysis.ambiguousJunctions != expectedAmbiguousJunctions) {
        invalidFeatureAnalysis("ambiguous-junction count does not match per-vertex markers.");
    }
}

void validateFeaturePatches(const Mesh& mesh, const FeatureAnalysis& analysis) {
    const bool hasPatchRecords = !analysis.patches.empty() || !analysis.patchAdjacencies.empty();
    if (analysis.facePatchIds.empty()) {
        if (hasPatchRecords) {
            invalidFeatureAnalysis("surface patch records exist without per-face patch ids.");
        }
        if (analysis.closedSurfacePatches != 0) {
            invalidFeatureAnalysis("closed surface patch count exists without surface patches.");
        }
        return;
    }
    if (analysis.facePatchIds.size() != mesh.faces.size()) {
        invalidFeatureAnalysis("face patch id count does not match the source mesh.");
    }

    const std::size_t patchCount = analysis.patches.size();
    if (!mesh.faces.empty() && patchCount == 0) {
        invalidFeatureAnalysis("per-face patch ids exist without surface patch records.");
    }
    std::vector<int> faceCounts(patchCount, 0);
    std::vector<double> patchAreas(patchCount, 0.0);
    std::vector<Vec3> patchNormalSums(patchCount, Vec3::Zero());
    for (std::size_t faceId = 0; faceId < mesh.faces.size(); ++faceId) {
        const int patchId = analysis.facePatchIds[faceId];
        if (patchId < 0 || static_cast<std::size_t>(patchId) >= patchCount) {
            invalidFeatureAnalysis("face patch list references an invalid patch id.");
        }
        ++faceCounts[static_cast<std::size_t>(patchId)];
        const Face& face = mesh.faces[faceId];
        const Vec3 cross =
            (mesh.vertices[static_cast<std::size_t>(face.v[1])] - mesh.vertices[static_cast<std::size_t>(face.v[0])])
                .cross(
                    mesh.vertices[static_cast<std::size_t>(face.v[2])] -
                    mesh.vertices[static_cast<std::size_t>(face.v[0])]
                );
        patchAreas[static_cast<std::size_t>(patchId)] += 0.5 * cross.norm();
        patchNormalSums[static_cast<std::size_t>(patchId)] += cross;
    }

    std::vector<char> patchIds(patchCount, 0);
    for (const FeaturePatch& patch : analysis.patches) {
        if (patch.id < 0 || static_cast<std::size_t>(patch.id) >= patchCount ||
            patchIds[static_cast<std::size_t>(patch.id)] != 0) {
            invalidFeatureAnalysis("surface patch ids are invalid or not unique.");
        }
        patchIds[static_cast<std::size_t>(patch.id)] = 1;
        if (faceCounts[static_cast<std::size_t>(patch.id)] == 0) {
            invalidFeatureAnalysis("surface patch must contain at least one face.");
        }
        if (patch.faceCount != faceCounts[static_cast<std::size_t>(patch.id)]) {
            invalidFeatureAnalysis("surface patch face count does not match per-face patch ids.");
        }
        requireFiniteNonNegativeValue(patch.area, "Surface patch area");
        requireFiniteVector(patch.normal, "Surface patch normal");
        if (!nearlyEqual(patch.area, patchAreas[static_cast<std::size_t>(patch.id)])) {
            invalidFeatureAnalysis("surface patch area does not match its source faces.");
        }
        const Vec3& normalSum = patchNormalSums[static_cast<std::size_t>(patch.id)];
        const Vec3 expectedNormal =
            detector_detail::resolveSurfacePatchNormal(normalSum, patchAreas[static_cast<std::size_t>(patch.id)]);
        if ((patch.normal - expectedNormal).norm() > 1e-9) {
            invalidFeatureAnalysis("surface patch normal does not match its source faces.");
        }
    }

    const common::MeshEdgeInfoMap meshEdges = common::buildMeshEdgeInfo(mesh);
    std::unordered_set<std::uint64_t> activeFeatureMeshEdges;
    activeFeatureMeshEdges.reserve(analysis.graph.edges.size());
    for (const FeatureGraphEdge& edge : analysis.graph.edges) {
        if (edge.removedByCleanup) {
            continue;
        }
        const bool evidence =
            edge.boundary || edge.dihedral || edge.normalTensor || edge.smoothCurvature || edge.nonManifold;
        const std::uint64_t key = common::meshEdgeKey(edge.a, edge.b);
        if (evidence && meshEdges.find(key) != meshEdges.end()) {
            activeFeatureMeshEdges.insert(key);
        }
    }

    std::vector<int> featureBoundaryEdges(patchCount, 0);
    std::vector<int> meshBoundaryEdges(patchCount, 0);
    std::vector<int> nonManifoldBoundaryEdges(patchCount, 0);
    std::map<std::pair<int, int>, int> expectedAdjacencyCounts;
    for (const auto& pairEntry : meshEdges) {
        const auto& key = pairEntry.first;
        const auto& info = pairEntry.second;
        if (info.faces.size() == 1) {
            const int patchId = analysis.facePatchIds[static_cast<std::size_t>(info.faces.front())];
            ++meshBoundaryEdges[static_cast<std::size_t>(patchId)];
            continue;
        }
        if (info.faces.size() > 2) {
            std::vector<int> incidentPatches;
            incidentPatches.reserve(info.faces.size());
            for (int faceId : info.faces) {
                incidentPatches.push_back(analysis.facePatchIds[static_cast<std::size_t>(faceId)]);
            }
            std::sort(incidentPatches.begin(), incidentPatches.end());
            incidentPatches.erase(std::unique(incidentPatches.begin(), incidentPatches.end()), incidentPatches.end());
            for (int patchId : incidentPatches) {
                ++nonManifoldBoundaryEdges[static_cast<std::size_t>(patchId)];
            }
            continue;
        }

        const int firstPatch = analysis.facePatchIds[static_cast<std::size_t>(info.faces[0])];
        const int secondPatch = analysis.facePatchIds[static_cast<std::size_t>(info.faces[1])];
        if (firstPatch == secondPatch) {
            continue;
        }
        if (activeFeatureMeshEdges.find(key) == activeFeatureMeshEdges.end()) {
            invalidFeatureAnalysis("surface patches are separated by an edge without active feature evidence.");
        }
        ++featureBoundaryEdges[static_cast<std::size_t>(firstPatch)];
        ++featureBoundaryEdges[static_cast<std::size_t>(secondPatch)];
        ++expectedAdjacencyCounts[std::minmax(firstPatch, secondPatch)];
    }

    int closedPatches = 0;
    for (const FeaturePatch& patch : analysis.patches) {
        const std::size_t patchId = static_cast<std::size_t>(patch.id);
        if (patch.featureBoundaryEdges != featureBoundaryEdges[patchId] ||
            patch.meshBoundaryEdges != meshBoundaryEdges[patchId] ||
            patch.nonManifoldBoundaryEdges != nonManifoldBoundaryEdges[patchId]) {
            invalidFeatureAnalysis("surface patch boundary counts do not match the mesh and patch labels.");
        }
        const bool expectedClosed = meshBoundaryEdges[patchId] == 0 && nonManifoldBoundaryEdges[patchId] == 0;
        if (patch.closed != expectedClosed) {
            invalidFeatureAnalysis("surface patch closed marker does not match its mesh boundary counts.");
        }
        if (patch.closed) {
            ++closedPatches;
        }
    }

    std::vector<std::vector<int>> expectedNeighbors(patchCount);
    std::vector<std::pair<int, int>> adjacencyPairs;
    adjacencyPairs.reserve(analysis.patchAdjacencies.size());
    for (const FeaturePatchAdjacency& adjacency : analysis.patchAdjacencies) {
        if (adjacency.firstPatch < 0 || adjacency.secondPatch < 0 || adjacency.firstPatch == adjacency.secondPatch ||
            static_cast<std::size_t>(adjacency.firstPatch) >= patchCount ||
            static_cast<std::size_t>(adjacency.secondPatch) >= patchCount) {
            invalidFeatureAnalysis("surface patch adjacency references an invalid patch id.");
        }
        if (adjacency.featureEdges <= 0) {
            invalidFeatureAnalysis("surface patch adjacency must contain at least one feature edge.");
        }
        const std::pair<int, int> pair = std::minmax(adjacency.firstPatch, adjacency.secondPatch);
        const auto expectedAdjacency = expectedAdjacencyCounts.find(pair);
        if (expectedAdjacency == expectedAdjacencyCounts.end() || adjacency.featureEdges != expectedAdjacency->second) {
            invalidFeatureAnalysis("surface patch adjacency count does not match separating feature edges.");
        }
        adjacencyPairs.push_back(pair);
        expectedNeighbors[static_cast<std::size_t>(pair.first)].push_back(pair.second);
        expectedNeighbors[static_cast<std::size_t>(pair.second)].push_back(pair.first);
    }
    std::sort(adjacencyPairs.begin(), adjacencyPairs.end());
    if (std::adjacent_find(adjacencyPairs.begin(), adjacencyPairs.end()) != adjacencyPairs.end()) {
        invalidFeatureAnalysis("surface patch adjacencies contain duplicate patch pairs.");
    }
    if (adjacencyPairs.size() != expectedAdjacencyCounts.size()) {
        invalidFeatureAnalysis("surface patch adjacency records do not cover all separated patch pairs.");
    }
    for (std::vector<int>& neighbors : expectedNeighbors) {
        std::sort(neighbors.begin(), neighbors.end());
    }

    for (const FeaturePatch& patch : analysis.patches) {
        for (int neighbor : patch.neighboringPatches) {
            if (neighbor < 0 || static_cast<std::size_t>(neighbor) >= patchCount || neighbor == patch.id) {
                invalidFeatureAnalysis("surface patch references an invalid neighboring patch.");
            }
        }
        const std::vector<int> actualNeighbors =
            sortedUniqueIds(patch.neighboringPatches, "surface patch contains duplicate neighboring patches.");
        if (actualNeighbors != expectedNeighbors[static_cast<std::size_t>(patch.id)]) {
            invalidFeatureAnalysis("surface patch neighbors do not match patch adjacencies.");
        }
    }
    if (analysis.closedSurfacePatches != closedPatches) {
        invalidFeatureAnalysis("closed surface patch count does not match patch records.");
    }
}
FeatureAnalysisSource featureAnalysisSource(const Mesh& mesh) {
    FeatureAnalysisSource source;
    source.vertexCount = static_cast<std::uint64_t>(mesh.vertices.size());
    source.faceCount = static_cast<std::uint64_t>(mesh.faces.size());

    std::uint64_t topology = kFingerprintOffset;
    appendFingerprintU64(topology, source.vertexCount);
    appendFingerprintU64(topology, source.faceCount);
    for (const Face& face : mesh.faces) {
        for (int vertexId : face.v) {
            appendFingerprintI32(topology, vertexId);
        }
    }
    source.topologyFingerprint = topology;

    std::uint64_t geometry = topology;
    for (const Vec3& vertex : mesh.vertices) {
        appendFingerprintDouble(geometry, vertex.x());
        appendFingerprintDouble(geometry, vertex.y());
        appendFingerprintDouble(geometry, vertex.z());
    }
    source.geometryFingerprint = geometry;
    return source;
}

void validateFeatureAnalysis(const Mesh& mesh, const FeatureAnalysis& analysis) {
    detector_detail::validateFeatureMeshInput(mesh);
    const FeatureAnalysisSource expected = featureAnalysisSource(mesh);
    if (analysis.source.vertexCount != expected.vertexCount || analysis.source.faceCount != expected.faceCount) {
        invalidFeatureAnalysis("source vertex/face counts do not match.");
    }
    if (analysis.source.topologyFingerprint != expected.topologyFingerprint) {
        invalidFeatureAnalysis("source topology fingerprint does not match.");
    }
    if (analysis.source.geometryFingerprint != expected.geometryFingerprint) {
        invalidFeatureAnalysis("source geometry fingerprint does not match.");
    }

    const std::size_t vertexCount = mesh.vertices.size();
    const std::size_t componentCount = analysis.components.size();
    if (analysis.vertices.size() != vertexCount) {
        invalidFeatureAnalysis("vertex feature count does not match the source mesh.");
    }
    if (analysis.graph.vertices.size() != vertexCount) {
        invalidFeatureAnalysis("feature graph vertex count does not match the source mesh.");
    }
    if (!analysis.normalTensorVertexWeights.empty() && analysis.normalTensorVertexWeights.size() != vertexCount) {
        invalidFeatureAnalysis("Normal Tensor weight count does not match the source mesh.");
    }
    for (double weight : analysis.normalTensorVertexWeights) {
        if (!std::isfinite(weight) || weight < 0.0 || weight > 1.0) {
            invalidFeatureAnalysis("Normal Tensor weights must be finite and in [0, 1].");
        }
    }

    validateFeatureGraph(mesh, analysis);
    const std::vector<int> componentByVertex = validateFeatureComponents(analysis, vertexCount, componentCount);
    validateFeatureLoopMembership(mesh, analysis, vertexCount, componentCount, componentByVertex);
    validateFeaturePatches(mesh, analysis);
}

/** @brief 公共 FeatureDetector 值类型使用的私有选项存储。 */
struct FeatureDetector::Impl {
    FeatureOptions options;
};

FeatureDetector::FeatureDetector(FeatureOptions options)
    : impl_(std::make_unique<Impl>()) {
    validateFeatureOptions(options);
    impl_->options = std::move(options);
}

FeatureDetector::~FeatureDetector() = default;

FeatureDetector::FeatureDetector(const FeatureDetector& other)
    : impl_(other.impl_ ? std::make_unique<Impl>(*other.impl_) : std::make_unique<Impl>()) {}

FeatureDetector& FeatureDetector::operator=(const FeatureDetector& other) {
    if (this != &other) {
        impl_ = other.impl_ ? std::make_unique<Impl>(*other.impl_) : std::make_unique<Impl>();
    }
    return *this;
}

FeatureDetector::FeatureDetector(FeatureDetector&& other) noexcept
    : impl_(std::move(other.impl_)) {}

FeatureDetector& FeatureDetector::operator=(FeatureDetector&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

const FeatureOptions& FeatureDetector::options() const {
    static const FeatureOptions defaultOptions;
    return impl_ ? impl_->options : defaultOptions;
}

void FeatureDetector::setOptions(FeatureOptions options) {
    validateFeatureOptions(options);
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->options = std::move(options);
}

FeatureAnalysis FeatureDetector::analyze(const Mesh& mesh) const { return detectFeatureCurves(mesh, options()); }

FeatureAnalysis detectFeatureCurves(const Mesh& mesh, const FeatureOptions& options) {
    return FeatureDetectionPipeline{}.run(mesh, options);
}

DirectionalCurveError measureLoopAgainstCircle(
    const Mesh& mesh, const FeatureLoop& loop, const Vec3& center, const Vec3& normalIn, double radius
) {
    return primitive_fit_detail::measureLoopAgainstCircle(mesh, loop, center, normalIn, radius);
}

std::string featureReportHeaderCsv() {
    return "loop_id,component_id,component_confidence,weak_feature,primitive_residual,"
           "vertices,edges,closed,primitive,circular,mostly_boundary,cx,cy,cz,"
           "nx,ny,nz,major_axis_x,major_axis_y,major_axis_z,minor_axis_x,"
           "minor_axis_y,minor_axis_z,radius,major_radius,minor_radius,axis_ratio,"
           "rms_radial,max_radial,rms_ellipse,max_ellipse,rms_plane,max_plane,"
           "convex_edges,concave_edges,unknown_signed_edges";
}

std::string featureLoopRowCsv(const FeatureLoop& loop) {
    std::ostringstream out;
    out << std::setprecision(12);
    out << loop.id << "," << loop.componentId << "," << loop.componentConfidence << "," << (loop.weakFeature ? 1 : 0)
        << "," << loop.primitiveResidual << "," << loop.vertices.size() << "," << loop.edgeCount << ","
        << (loop.closed ? 1 : 0) << "," << toString(loop.primitive) << "," << (loop.circular ? 1 : 0) << ","
        << (loop.mostlyBoundary ? 1 : 0) << "," << loop.center.x() << "," << loop.center.y() << "," << loop.center.z()
        << "," << loop.normal.x() << "," << loop.normal.y() << "," << loop.normal.z() << "," << loop.majorAxis.x()
        << "," << loop.majorAxis.y() << "," << loop.majorAxis.z() << "," << loop.minorAxis.x() << ","
        << loop.minorAxis.y() << "," << loop.minorAxis.z() << "," << loop.radius << "," << loop.majorRadius << ","
        << loop.minorRadius << "," << loop.axisRatio << "," << loop.rmsRadialError << "," << loop.maxRadialError << ","
        << loop.rmsEllipseError << "," << loop.maxEllipseError << "," << loop.rmsPlaneError << "," << loop.maxPlaneError
        << "," << loop.convexEdges << "," << loop.concaveEdges << "," << loop.unknownSignedEdges;
    return out.str();
}

std::string toString(FeaturePrimitiveType primitive) {
    switch (primitive) {
    case FeaturePrimitiveType::Unknown:
        return "unknown";
    case FeaturePrimitiveType::Circle:
        return "circle";
    case FeaturePrimitiveType::NearCircle:
        return "near-circle";
    case FeaturePrimitiveType::Ellipse:
        return "ellipse";
    case FeaturePrimitiveType::PolygonalLoop:
        return "polygonal-loop";
    }
    return "unknown";
}

} // namespace feature
} // namespace manumesh
