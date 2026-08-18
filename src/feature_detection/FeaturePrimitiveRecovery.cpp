/**
 * @file src/feature_detection/FeaturePrimitiveRecovery.cpp
 * @brief 在未归属图分量中恢复可拟合的几何基元曲线。
 * @ingroup manumesh_feature_detection
 *
 * @details 为尚未由追踪环表示的图分量应用几何基元拟合。
 * @algorithm 按图连续性排序分量顶点，拟合并依据图边校验，
 *            仅在不与已有更强环的归属冲突时写入结果。
 */

#include "detail/FeaturePrimitiveRecovery.h"

#include "common/detail/ParallelExecution.h"
#include "detail/FeatureGraph.h"
#include "detail/FeatureLoopBuilder.h"
#include "detail/PrimitiveFit.h"

#include <algorithm>
#include <cstddef>
#include <queue>
#include <utility>
#include <vector>

namespace manumesh {
namespace feature {
namespace detector_detail {

using primitive_fit_detail::applyPrimitiveFit;
using primitive_fit_detail::fitPrimitive;
using primitive_fit_detail::PrimitiveFit;

namespace {

/**
 * @brief 保存一个待拟合组件及其并行计算结果。
 *
 * 组件收集顺序是恢复阶段的确定性契约；拟合只写入自己的槽位，
 * 因而可以并行计算而不触碰共享的分析归属和 loop ID。
 */
struct PrimitiveComponentWork {
    FeatureLoop loop;
    PrimitiveFit fit;
};

bool isAcceptedPrimitive(const FeatureLoop& loop) {
    return loop.primitive == FeaturePrimitiveType::Circle || loop.primitive == FeaturePrimitiveType::NearCircle ||
           loop.primitive == FeaturePrimitiveType::Ellipse;
}

common::parallel::RangeExecutionOptions makePrimitiveFitExecutionOptions(
    const common::parallel::RangeExecutionOptions& requested, const std::vector<PrimitiveComponentWork>& components
) {
    if (!requested.enabled || requested.maxConcurrency == 1 || components.size() < 4) {
        return requested;
    }

    std::size_t totalVertices = 0;
    for (const PrimitiveComponentWork& component : components) {
        totalVertices += component.loop.vertices.size();
    }
    const std::size_t averageVertices =
        std::max<std::size_t>(1, (totalVertices + components.size() - 1) / components.size());
    // ExecutionOptions::minItemsPerTask is expressed in the dominant vertex/face
    // ranges.  Convert that work estimate to component units so a few hundred
    // expensive fits do not collapse into one task merely because the default
    // range grain is 4096 vertices.
    const std::size_t requestedWork = requested.grainSize == 0 ? 256 : requested.grainSize;
    std::size_t componentGrain = std::max<std::size_t>(1, requestedWork / averageVertices);

    // Leave enough independent chunks for the backend without creating one task
    // per tiny component.  maxConcurrency==0 delegates the worker count to TBB;
    // eight is a conservative scheduling hint for that unbounded case.
    const std::size_t workerHint = requested.maxConcurrency > 1
                                       ? static_cast<std::size_t>(requested.maxConcurrency)
                                       : static_cast<std::size_t>(8);
    const std::size_t targetTaskCount = std::max<std::size_t>(1, workerHint * 2);
    const std::size_t taskLimitedGrain =
        std::max<std::size_t>(1, (components.size() + targetTaskCount - 1) / targetTaskCount);
    componentGrain = std::min(componentGrain, taskLimitedGrain);

    common::parallel::RangeExecutionOptions result = requested;
    result.grainSize = componentGrain;
    return result;
}

} // namespace

void recoverPrimitiveComponents(
    const Mesh& mesh,
    const FeatureOptions& options,
    const TraceGraph& trace,
    FeatureAnalysis& analysis,
    int& loopId,
    const common::parallel::RangeExecutionOptions& executionOptions
) {
    const std::vector<std::vector<int>>& adjacency = trace.adjacency;
    std::vector<char> componentVisited(mesh.vertices.size(), 0);
    std::vector<PrimitiveComponentWork> components;
    components.reserve(adjacency.size() / static_cast<std::size_t>(std::max(1, options.minFeatureLoopVertices)));
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

        PrimitiveComponentWork work;
        work.loop.vertices = std::move(component);
        work.loop.edgeCount = edgeCount;
        work.loop.closed = true;
        work.loop.mostlyBoundary =
            work.loop.edgeCount > 0 && boundaryEdges >= static_cast<int>(0.6 * static_cast<double>(work.loop.edgeCount));
        work.loop.convexEdges = convexEdges;
        work.loop.concaveEdges = concaveEdges;
        work.loop.unknownSignedEdges = unknownSignedEdges;
        components.push_back(std::move(work));
    }

    // The fit is read-only with respect to the mesh and independent for each
    // component.  Keep all publication and ownership updates below in the
    // original component order so loop IDs and primary vertex ownership stay
    // byte-for-byte deterministic.
    const common::parallel::RangeExecutionOptions fitExecutionOptions =
        makePrimitiveFitExecutionOptions(executionOptions, components);
    common::parallel::forEachRange(
        0,
        components.size(),
        fitExecutionOptions,
        [&](std::size_t begin, std::size_t end) {
            for (std::size_t index = begin; index < end; ++index) {
                PrimitiveComponentWork& work = components[index];
                work.fit = fitPrimitive(mesh, work.loop, options);
                applyPrimitiveFit(work.fit, work.loop);
            }
        }
    );

    for (PrimitiveComponentWork& work : components) {
        if (!isAcceptedPrimitive(work.loop)) {
            continue;
        }
        work.loop.id = loopId++;
        assignLoopToVertices(work.loop, mesh, adjacency, analysis);
        analysis.loops.push_back(std::move(work.loop));
    }
}

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
