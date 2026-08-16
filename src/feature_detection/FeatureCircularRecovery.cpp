/**
 * @file src/feature_detection/FeatureCircularRecovery.cpp
 * @brief 从稀疏但空间连贯的特征顶点簇恢复圆形曲线。
 * @ingroup manumesh_feature_detection
 *
 * @details 恢复严格图追踪遗漏的稀疏圆形顶点簇。
 * @algorithm 在拟合平面内分组兼容的特征顶点，围绕候选圆按角度排序，
 *            校验径向/平面残差及图支持后，才物化为备用特征环。
 * @failuremodes 该面向 CAD 的有界回退会拒绝不完整圆弧，以及角覆盖或残差
 *               不足以支持闭合圆的顶点簇。
 */

#include "detail/FeatureCircularRecovery.h"

#include "detail/FeatureGraph.h"
#include "detail/FeatureLoopBuilder.h"
#include "detail/PrimitiveFit.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <utility>

namespace manumesh {
namespace feature {
namespace detector_detail {
namespace {

using primitive_fit_detail::applyPrimitiveFit;
using primitive_fit_detail::fitPrimitive;
using primitive_fit_detail::PrimitiveFit;

CycleSignature vertexSetSignature(const std::vector<int>& ids) {
    CycleSignature signature;
    signature.reserve(ids.size());
    for (int id : ids) {
        signature.push_back(static_cast<std::uint64_t>(id));
    }
    std::sort(signature.begin(), signature.end());
    return signature;
}

/** @brief 由三个不共线采样点恢复的圆。 */
struct ThreePointCircle {
    bool valid = false;
    Vec3 center = Vec3::Zero();
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    double radius = 0.0;
};

ThreePointCircle fitCircleFromThree(const Mesh& mesh, int ia, int ib, int ic) {
    ThreePointCircle result;
    const Vec3& a = mesh.vertices[ia];
    const Vec3& b = mesh.vertices[ib];
    const Vec3& c = mesh.vertices[ic];
    const Vec3 ab = b - a;
    const Vec3 ac = c - a;
    result.normal = ab.cross(ac);
    const double n2 = result.normal.squaredNorm();
    if (n2 <= 1e-20) {
        return result;
    }
    const Vec3 term1 = ac.squaredNorm() * result.normal.cross(ab);
    const Vec3 term2 = ab.squaredNorm() * ac.cross(result.normal);
    result.center = a + (term1 + term2) / (2.0 * n2);
    result.radius = (result.center - a).norm();
    result.normal.normalize();
    result.valid = std::isfinite(result.radius) && result.radius > 1e-20 && result.center.allFinite();
    return result;
}

std::vector<int> sortAroundCircle(std::vector<int> ids, const Mesh& mesh, const Vec3& center, const Vec3& normal) {
    Vec3 axisX = mesh.vertices[ids.front()] - center;
    axisX -= normal * axisX.dot(normal);
    if (axisX.norm() <= 1e-20) {
        axisX = std::abs(normal.x()) < 0.9 ? Vec3(1.0, 0.0, 0.0) : Vec3(0.0, 1.0, 0.0);
        axisX -= normal * axisX.dot(normal);
    }
    axisX.normalize();
    const Vec3 axisY = normal.cross(axisX).normalized();
    std::sort(ids.begin(), ids.end(), [&](int lhs, int rhs) {
        const Vec3 dl = mesh.vertices[lhs] - center;
        const Vec3 dr = mesh.vertices[rhs] - center;
        const double al = std::atan2(dl.dot(axisY), dl.dot(axisX));
        const double ar = std::atan2(dr.dot(axisY), dr.dot(axisX));
        return al < ar;
    });
    return ids;
}

double angularCoverage(const std::vector<int>& ids, const Mesh& mesh, const Vec3& center, const Vec3& normal) {
    Vec3 axisX = mesh.vertices[ids.front()] - center;
    axisX -= normal * axisX.dot(normal);
    if (axisX.norm() <= 1e-20) {
        return 0.0;
    }
    axisX.normalize();
    const Vec3 axisY = normal.cross(axisX).normalized();
    std::vector<double> angles;
    angles.reserve(ids.size());
    for (int id : ids) {
        const Vec3 d = mesh.vertices[id] - center;
        double angle = std::atan2(d.dot(axisY), d.dot(axisX));
        if (angle < 0.0) {
            angle += 2.0 * kPi;
        }
        angles.push_back(angle);
    }
    std::sort(angles.begin(), angles.end());
    double maxGap = 0.0;
    for (int i = 0; i < static_cast<int>(angles.size()); ++i) {
        const double a = angles[i];
        const double b = i + 1 < static_cast<int>(angles.size()) ? angles[i + 1] : angles.front() + 2.0 * kPi;
        maxGap = std::max(maxGap, b - a);
    }
    return 2.0 * kPi - maxGap;
}

/**
 * @brief 连续簇顶点之间在没有特征边支持时，允许跨越的最大整圆比例。
 *        该恢复用于闭合被阈值截断的小证据间隙，而不是凭空生成圆；因此大部分圆周
 *        必须已由轨迹图边连接。角度限制无量纲且对统一缩放不变，0.25 对应既有的
 *        1.5*pi 角覆盖要求（最多缺失四分之一圆周）。
 */
constexpr double kMaxCircularRecoveryGapFraction = 0.25;

/**
 * @brief 恢复圆的证据连通性门限：按拟合圆排序后，连续顶点对应尽量是轨迹图特征边；
 *        不受支持的角跨度总和必须小于 kMaxCircularRecoveryGapFraction 所限定的整圆比例。
 *        仅有几何共圆并不构成证据；没有特征边连接的顶点集合（例如倒角棱柱的共面角行）
 *        不得被拼接为圆。
 */
bool clusterSupportedByTraceEdges(
    const std::vector<int>& sortedCluster,
    const Mesh& mesh,
    const TraceGraph& trace,
    const Vec3& center,
    const Vec3& normal
) {
    Vec3 axisX = mesh.vertices[sortedCluster.front()] - center;
    axisX -= normal * axisX.dot(normal);
    if (axisX.norm() <= 1e-20) {
        return false;
    }
    axisX.normalize();
    const Vec3 axisY = normal.cross(axisX).normalized();
    const auto angleOf = [&](int id) {
        const Vec3 d = mesh.vertices[id] - center;
        return std::atan2(d.dot(axisY), d.dot(axisX));
    };
    double unsupportedAngle = 0.0;
    const int count = static_cast<int>(sortedCluster.size());
    for (int i = 0; i < count; ++i) {
        const int a = sortedCluster[i];
        const int b = sortedCluster[(i + 1) % count];
        if (traceGraphHasEdge(trace, a, b)) {
            continue;
        }
        double gap = angleOf(b) - angleOf(a);
        if (gap < 0.0) {
            gap += 2.0 * kPi;
        }
        unsupportedAngle += gap;
        if (unsupportedAngle > kMaxCircularRecoveryGapFraction * 2.0 * kPi) {
            return false;
        }
    }
    return true;
}

/** @brief 参与圆形恢复的一个连通轨迹图分量。 */
struct TraceComponent {
    std::vector<int> vertices;
    bool hasWeakEvidenceEdge = false;
    bool alreadyHasCircularLoop = false;
};

std::vector<TraceComponent> collectTraceComponents(const TraceGraph& trace, const FeatureAnalysis& analysis) {
    std::vector<TraceComponent> components;
    std::vector<char> visited(trace.adjacency.size(), 0);
    for (int seed = 0; seed < static_cast<int>(trace.adjacency.size()); ++seed) {
        if (visited[seed] || trace.adjacency[seed].empty()) {
            continue;
        }
        TraceComponent component;
        std::queue<int> queue;
        queue.push(seed);
        visited[seed] = 1;
        while (!queue.empty()) {
            const int v = queue.front();
            queue.pop();
            component.vertices.push_back(v);
            if (v >= 0 && v < static_cast<int>(analysis.vertices.size())) {
                component.alreadyHasCircularLoop = component.alreadyHasCircularLoop || analysis.vertices[v].circular;
            }
            for (int nb : trace.adjacency[v]) {
                if (!component.hasWeakEvidenceEdge) {
                    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, v, nb);
                    component.hasWeakEvidenceEdge = attrs != nullptr && (attrs->normalTensor || attrs->smoothCurvature);
                }
                if (!visited[nb]) {
                    visited[nb] = 1;
                    queue.push(nb);
                }
            }
        }
        components.push_back(std::move(component));
    }
    return components;
}

} // 匿名命名空间

void recoverCircularVertexClusters(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
) {
    CycleSignatureSet seenClusters;
    for (const TraceComponent& component : collectTraceComponents(trace, analysis)) {
        if (component.hasWeakEvidenceEdge || component.alreadyHasCircularLoop ||
            static_cast<int>(component.vertices.size()) < options.minFeatureLoopVertices ||
            component.vertices.size() > 120) {
            continue;
        }

        // 仅从轨迹图长度为 2 的路径（a-b-c，且两条边均存在）生成圆种子。
        // 回退阶段修补的是已有部分边证据的圆；任何至少含 3 个顶点的有证据圆弧
        // 都包含这样的路径，因此不会遗漏合法圆。相比盲目的 O(n^3) 三元组扫描，
        // 该策略不会把互不连通的共圆顶点拼成圆，也避免在小网格上消耗数秒。
        constexpr int kMaxCircularClusterSeedScans = 32768;
        int seedScans = 0;
        std::vector<int> candidates = component.vertices;
        std::sort(candidates.begin(), candidates.end());
        for (int middle : candidates) {
            // 对邻接顶点排序，确保恢复结果不依赖邻接表的插入顺序。
            std::vector<int> around = trace.adjacency[middle];
            std::sort(around.begin(), around.end());
            for (int i = 0; i < static_cast<int>(around.size()) && seedScans < kMaxCircularClusterSeedScans; ++i) {
                for (int j = i + 1; j < static_cast<int>(around.size()) && seedScans < kMaxCircularClusterSeedScans;
                     ++j) {
                    ++seedScans;
                    const ThreePointCircle circle = fitCircleFromThree(mesh, around[i], middle, around[j]);
                    if (!circle.valid) {
                        continue;
                    }
                    const double allowed = std::max(3.0 * options.circleFitRelativeThreshold, 0.08) * circle.radius;
                    std::vector<int> cluster;
                    for (int id : candidates) {
                        const Vec3 delta = mesh.vertices[id] - circle.center;
                        const double plane = std::abs(delta.dot(circle.normal));
                        const Vec3 inPlane = delta - circle.normal * delta.dot(circle.normal);
                        const double radial = std::abs(inPlane.norm() - circle.radius);
                        if (std::max(plane, radial) <= allowed) {
                            cluster.push_back(id);
                        }
                    }
                    if (static_cast<int>(cluster.size()) < options.minFeatureLoopVertices ||
                        angularCoverage(cluster, mesh, circle.center, circle.normal) < 1.5 * kPi) {
                        continue;
                    }
                    cluster = sortAroundCircle(std::move(cluster), mesh, circle.center, circle.normal);
                    if (!clusterSupportedByTraceEdges(cluster, mesh, trace, circle.center, circle.normal)) {
                        continue;
                    }
                    if (!seenClusters.insert(vertexSetSignature(cluster)).second) {
                        continue;
                    }

                    FeatureLoop loop;
                    loop.id = loopId;
                    loop.vertices = std::move(cluster);
                    loop.edgeCount = static_cast<int>(loop.vertices.size());
                    loop.closed = true;
                    const PrimitiveFit fit = fitPrimitive(mesh, loop, options);
                    applyPrimitiveFit(fit, loop);
                    if (!loop.circular) {
                        continue;
                    }
                    ++loopId;
                    assignLoopToVertices(loop, mesh, trace.adjacency, analysis);
                    analysis.loops.push_back(std::move(loop));
                }
            }
        }
        // 达到上限表示穷举路径扫描被截断（每个种子循环恰好在该上限停止枚举）。
        if (seedScans >= kMaxCircularClusterSeedScans) {
            ++analysis.circularRecoveryTruncated;
        }
    }
}

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
