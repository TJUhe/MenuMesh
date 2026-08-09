/**
 * @file src/feature_detection/FeatureGraphCompatibility.cpp
 * @brief 实现 ManuMesh 的特征检测模块的特征图兼容性功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 定义图恢复共用的延续分支和证据兼容性判定。
 * @algorithm 候选分支按绝对切线对齐度排序，并以顶点索引作确定性平局决胜；
 *            强证据类别以及已知凸/凹符号不允许不兼容地桥接。
 */

#include "detail/FeatureGraphCompatibility.h"

#include "detail/FeatureGraph.h"

#include <algorithm>

namespace manumesh::feature::detector_detail {
namespace {

int strongSourceMask(const TraceEdgeAttrs& attrs) {
    return (attrs.boundary ? 1 : 0) | (attrs.dihedral ? 2 : 0) | (attrs.nonManifold ? 4 : 0);
}

int weakSourceMask(const TraceEdgeAttrs& attrs) {
    return (attrs.normalTensor ? 1 : 0) | (attrs.smoothCurvature ? 2 : 0);
}

} // 匿名命名空间

ContinuationBranch
bestContinuationBranch(const Mesh& mesh, const TraceGraph& trace, int vertex, int target, double minAlignment) {
    ContinuationBranch result;
    if (vertex < 0 || target < 0 || vertex >= static_cast<int>(trace.adjacency.size()) ||
        vertex >= static_cast<int>(mesh.vertices.size()) || target >= static_cast<int>(mesh.vertices.size())) {
        return result;
    }
    Vec3 connection = mesh.vertices[target] - mesh.vertices[vertex];
    if (connection.squaredNorm() <= 1e-30) {
        return result;
    }
    connection.normalize();
    for (int neighbor : trace.adjacency[vertex]) {
        if (neighbor < 0 || neighbor >= static_cast<int>(mesh.vertices.size())) {
            continue;
        }
        Vec3 outward = mesh.vertices[vertex] - mesh.vertices[neighbor];
        if (outward.squaredNorm() <= 1e-30) {
            continue;
        }
        outward.normalize();
        const double alignment = outward.dot(connection);
        if (alignment < minAlignment) {
            continue;
        }
        if (result.neighbor < 0 || alignment > result.alignment ||
            (alignment == result.alignment && neighbor < result.neighbor)) {
            result.neighbor = neighbor;
            result.attrs = traceEdgeAttrs(trace, vertex, neighbor);
            result.alignment = alignment;
        }
    }
    return result;
}

bool compatibleFeatureEvidence(const TraceEdgeAttrs* lhs, const TraceEdgeAttrs* rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        return false;
    }
    if (lhs->signedKind != 0 && rhs->signedKind != 0 && lhs->signedKind != rhs->signedKind) {
        return false;
    }
    const int lhsStrong = strongSourceMask(*lhs);
    const int rhsStrong = strongSourceMask(*rhs);
    if (lhsStrong != 0 || rhsStrong != 0) {
        return (lhsStrong & rhsStrong) != 0;
    }
    const int lhsWeak = weakSourceMask(*lhs);
    const int rhsWeak = weakSourceMask(*rhs);
    if (lhsWeak != 0 && rhsWeak != 0) {
        return true;
    }
    const bool lhsRecovery = lhs->cleanupBridge || lhs->consolidationBridge;
    const bool rhsRecovery = rhs->cleanupBridge || rhs->consolidationBridge;
    return lhsRecovery != rhsRecovery;
}

int compatibleSignedKind(const TraceEdgeAttrs* lhs, const TraceEdgeAttrs* rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        return 0;
    }
    if (lhs->signedKind != 0 && rhs->signedKind != 0 && lhs->signedKind != rhs->signedKind) {
        return 0;
    }
    return lhs->signedKind != 0 ? lhs->signedKind : rhs->signedKind;
}

} // 命名空间 manumesh::feature::detector_detail
