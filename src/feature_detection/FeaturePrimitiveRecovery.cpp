/**
 * @file src/feature_detection/FeaturePrimitiveRecovery.cpp
 * @brief 实现 ManuMesh 的特征检测模块的特征图元恢复功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 为尚未由追踪环表示的图分量应用图元拟合。
 * @algorithm 按图连续性排序分量顶点，拟合并依据图边校验，
 *            仅在不与已有更强环的归属冲突时写入结果。
 */

#include "detail/FeaturePrimitiveRecovery.h"

#include "detail/FeatureGraph.h"
#include "detail/FeatureLoopBuilder.h"
#include "detail/PrimitiveFit.h"

#include <queue>

namespace manumesh::feature::detector_detail {

using primitive_fit_detail::applyPrimitiveFit;
using primitive_fit_detail::fitPrimitive;

void recoverPrimitiveComponents(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
) {
    const std::vector<std::vector<int>>& adjacency = trace.adjacency;
    std::vector<char> componentVisited(mesh.vertices.size(), 0);
    for (int seed = 0; seed < static_cast<int>(adjacency.size()); ++seed) {
        if (componentVisited[seed] || adjacency[seed].empty()) {
            continue;
        }

        std::vector<int> component;
        std::queue<int> queue;
        queue.push(seed);
        componentVisited[seed] = 1;
        while (!queue.empty()) {
            const int v = queue.front();
            queue.pop();
            component.push_back(v);
            for (int nb : adjacency[v]) {
                if (!componentVisited[nb]) {
                    componentVisited[nb] = 1;
                    queue.push(nb);
                }
            }
        }

        bool alreadyHasCircular = false;
        int edgeCount2x = 0;
        int boundaryEdges = 0;
        int convexEdges = 0;
        int concaveEdges = 0;
        int unknownSignedEdges = 0;
        for (int v : component) {
            alreadyHasCircular = alreadyHasCircular || analysis.vertices[v].circular;
            edgeCount2x += static_cast<int>(adjacency[v].size());
            for (int nb : adjacency[v]) {
                if (v >= nb) {
                    continue;
                }
                const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, v, nb);
                const bool boundary = attrs != nullptr && attrs->boundary;
                const int sign = attrs == nullptr ? 0 : attrs->signedKind;
                if (boundary) {
                    ++boundaryEdges;
                }
                if (sign > 0)
                    ++convexEdges;
                if (sign < 0)
                    ++concaveEdges;
                if (sign == 0 && !boundary)
                    ++unknownSignedEdges;
            }
        }
        const int edgeCount = edgeCount2x / 2;
        if (alreadyHasCircular || static_cast<int>(component.size()) < options.minFeatureLoopVertices ||
            edgeCount < static_cast<int>(component.size()) || edgeCount > static_cast<int>(component.size()) * 3) {
            continue;
        }

        FeatureLoop loop;
        loop.id = loopId;
        loop.vertices = std::move(component);
        loop.edgeCount = edgeCount;
        loop.closed = true;
        loop.mostlyBoundary =
            loop.edgeCount > 0 && boundaryEdges >= static_cast<int>(0.6 * static_cast<double>(loop.edgeCount));
        loop.convexEdges = convexEdges;
        loop.concaveEdges = concaveEdges;
        loop.unknownSignedEdges = unknownSignedEdges;
        applyPrimitiveFit(fitPrimitive(mesh, loop, options), loop);
        if (loop.primitive != FeaturePrimitiveType::Circle && loop.primitive != FeaturePrimitiveType::NearCircle &&
            loop.primitive != FeaturePrimitiveType::Ellipse) {
            continue;
        }
        ++loopId;
        assignLoopToVertices(loop, mesh, adjacency, analysis);
        analysis.loops.push_back(std::move(loop));
    }
}

} // 命名空间 manumesh::feature::detector_detail
