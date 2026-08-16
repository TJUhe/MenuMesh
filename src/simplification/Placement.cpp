/**
 * @file src/simplification/Placement.cpp
 * @brief 求解、约束并排序一条边的候选坍缩位置。
 * @ingroup manumesh_simplification
 *
 * @details 求解并排序一条边收缩的候选位置。
 * @algorithm 在考虑尺度的谱条件下尝试完整的 Garland-Heckbert 3x3 最优解。秩亏系统使用收缩边上的一维最优解，然后依次尝试端点和中点。边界与解析几何基元约束会在最终代价排序前投影或夹紧候选位置。
 * @failuremodes 拒绝非有限或病态的求解结果，不将其作为放置候选返回。
 */

#include "detail/Placement.h"
#include "core/MathUtils.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace manumesh {
namespace simplification {
namespace {

/**
 * @brief 收缩边界边周围的有向边界链摘要：e1 是有向边界半边（v1 - v0）的总和，e2 是 v0 × v1 的总和，遵循 Lindstrom-Turk 的边界保持约束。半边方向取自一致的面方向。
 */
struct BoundaryChainSums {
    Vec3 e1 = Vec3::Zero();
    Vec3 e2 = Vec3::Zero();
    double lengthSum = 0.0;
    int edgeCount = 0;
};

BoundaryChainSums collectBoundaryChain(const BoundaryProjectionInput& input) {
    BoundaryChainSums sums;
    // 按确定性顺序（排序并去重）收集与任一端点相邻的有向边界半边，使浮点累加不依赖 unordered_set 的遍历顺序。
    std::vector<std::pair<int, int>> halfEdges;
    for (const int vertex : {input.edge.keep, input.edge.remove}) {
        if (vertex < 0 || vertex >= static_cast<int>(input.topology.vertexFaces.size())) {
            continue;
        }
        for (const int faceId : input.topology.vertexFaces[vertex]) {
            if (faceId < 0 || faceId >= static_cast<int>(input.faces.size()) || !input.faces[faceId].active) {
                continue;
            }
            const FaceState& face = input.faces[faceId];
            for (int corner = 0; corner < 3; ++corner) {
                const int u = face.v[corner];
                const int w = face.v[(corner + 1) % 3];
                if (u != vertex && w != vertex) {
                    continue;
                }
                if (mesh_edit::activeIncidentFaceCountForEdge(u, w, input.faces, input.topology) == 1) {
                    halfEdges.emplace_back(u, w);
                }
            }
        }
    }
    std::sort(halfEdges.begin(), halfEdges.end());
    halfEdges.erase(std::unique(halfEdges.begin(), halfEdges.end()), halfEdges.end());
    for (const auto& pairEntry : halfEdges) {
        const int u = pairEntry.first;
        const int w = pairEntry.second;
        const Vec3& p0 = input.vertices[u].p;
        const Vec3& p1 = input.vertices[w].p;
        sums.e1 += p1 - p0;
        sums.e2 += p0.cross(p1);
        sums.lengthSum += (p1 - p0).norm();
        ++sums.edgeCount;
    }
    return sums;
}

Vec3 clampToSegment(const Vec3& position, const Vec3& a, const Vec3& b) {
    const Vec3 edge = b - a;
    const double len2 = edge.squaredNorm();
    if (len2 <= 1e-30) {
        return 0.5 * (a + b);
    }
    const double t = manumesh::clampValue((position - a).dot(edge) / len2, 0.0, 1.0);
    return a + t * edge;
}

} // 结束匿名命名空间

bool projectBoundaryPlacement(const BoundaryProjectionInput& input, Vec3& position) {
    if (!input.decision.boundaryEdge) {
        return false;
    }

    const int keep = input.edge.keep;
    const int remove = input.edge.remove;
    const std::vector<VertexState>& vertices = input.vertices;
    const Vec3& a = vertices[keep].p;
    const Vec3& b = vertices[remove].p;
    if ((b - a).squaredNorm() <= 1e-30) {
        position = 0.5 * (a + b);
        return true;
    }

    // Lindstrom-Turk 边界保持：有向边界面积变化 fB(v) = ||e2 - v × e1||^2 的极小点组成直线 p0 + t * e1，其中 p0 = (e1 × e2) / (e1 · e1)。将放置点投影到该直线（而不是直接夹回旧线段）可使边界折线保持一阶性质：直链情况下该直线就是边界；有拐角时则平衡两侧的扫掠面积。
    const BoundaryChainSums chain = collectBoundaryChain(input);
    const double e1Norm = chain.e1.norm();
    if (chain.edgeCount >= 2 && e1Norm > 1e-6 * std::max(chain.lengthSum, 1e-30)) {
        const Vec3 direction = chain.e1 / e1Norm;
        const Vec3 p0 = chain.e1.cross(chain.e2) / chain.e1.squaredNorm();
        // 将点夹到约束线上的收缩边投影，避免放置点沿整条边界滑移。
        const double ta = (a - p0).dot(direction);
        const double tb = (b - p0).dot(direction);
        const double t = manumesh::clampValue((position - p0).dot(direction), std::min(ta, tb), std::max(ta, tb));
        const Vec3 constrained = p0 + t * direction;
        // 安全保护：若约束解偏离收缩线段超过一个边长（例如边界链异常或 e1 近似抵消），则拒绝该解，退回普通线段夹紧。
        if (constrained.allFinite() &&
            (constrained - clampToSegment(constrained, a, b)).squaredNorm() <= (b - a).squaredNorm()) {
            position = constrained;
            return true;
        }
    }

    position = clampToSegment(position, a, b);
    return true;
}

} // namespace simplification
} // namespace manumesh
