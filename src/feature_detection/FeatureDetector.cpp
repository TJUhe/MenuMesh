/**
 * @file src/feature_detection/FeatureDetector.cpp
 * @brief 校验输入并编排一次完整的特征分析。
 * @ingroup manumesh_feature_detection
 *
 * @details 负责编排一次完整的特征分析，并在公共入口处统一校验选项和网格。
 * @algorithm 依次校验输入、构建共享几何缓存、收集边证据、初始化并清理轨迹图，
 *            可选合并相邻分量，然后恢复特征环、汇总分量与分支，最后按需分割曲面面片。
 * @invariants 所有阶段使用同一份只读网格及法向、邻接和局部尺度缓存；特征环 ID 单调递增。
 */

#include "algorithms/feature_detection/FeatureDetector.h"

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

#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
void removeDegenerateDihedralEvidence(FeatureDetectionContext& context) {
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

FeatureAnalysis runFeatureDetection(const Mesh& mesh, const FeatureOptions& options) {
    validateFeatureOptionsImpl(options);
    validateFeatureInput(mesh);

    FeatureDetectionContext context(mesh, options);
    // 宽松校验允许零面积面存在；其法向不可用，证据阶段会跳过这些面。
    // 将退化面数量公开给调用方，以便识别覆盖范围受限的分析结果。
    context.analysis().degenerateFaces = countDegenerateFaces(mesh);
    if (mesh.empty()) {
        return context.builder.build();
    }

    context.featureEdges =
        detector_detail::collectFeatureEdges(context.mesh, context.options, context.cache, context.builder);
    removeDegenerateDihedralEvidence(context);
    detector_detail::initializeFeatureGraph(context.featureEdges, context.analysis());
    context.trace =
        detector_detail::buildTraceGraph(context.mesh, context.options, context.featureEdges, context.analysis());
    detector_detail::cleanupTraceGraph(
        context.mesh, context.options, context.cache, context.trace, context.analysis()
    );
    detector_detail::consolidateFeatureGraph(
        context.mesh, context.options, context.cache, context.trace, context.analysis()
    );
    detector_detail::recoverFeatureLoops(
        context.mesh, context.options, context.trace, context.analysis(), context.builder.nextLoopId()
    );
    detector_detail::summarizeFeatureComponents(context.mesh, context.options, context.trace, context.analysis());
    detector_detail::finalizeFeatureGraphMarkers(context.mesh, context.analysis());
    if (context.options.surfacePatches.enabled) {
        detector_detail::buildFeaturePatches(context.mesh, context.analysis(), context.options.surfacePatches);
    }
    return context.builder.build();
}

} // namespace

void validateFeatureOptions(const FeatureOptions& options) { validateFeatureOptionsImpl(options); }

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
    return runFeatureDetection(mesh, options);
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
