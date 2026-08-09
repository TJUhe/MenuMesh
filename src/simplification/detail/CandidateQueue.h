/**
 * @file src/simplification/detail/CandidateQueue.h
 * @brief 声明 ManuMesh 的简化模块的候选队列功能。
 * @ingroup manumesh_simplification
 *
 * @details 本文件属于带特征感知的边折叠流水线。二次误差代价用于排序候选；拓扑、几何、特征、边界、误差及可选纹理策略共同决定一个放置是否可以修改网格。
 */

#pragma once

#include "detail/SimplificationTypes.h"

#include <queue>
#include <vector>

namespace manumesh::simplification {

/**
 * @brief 带版本号且缓存放置解的折叠候选最小堆。
 */
class CandidateQueue {
public:
    /**
     * @brief 移除队列中的所有候选。
     */
    void clear();
    /**
     * @return 队列中没有候选时返回 true。
     */
    bool empty() const;
    /**
     * @brief 移除并返回代价最低的候选。
     * @pre empty() 必须为 false。
     */
    Candidate pop();
    /**
     * @brief 压入复用调用方已求解放置的边候选（按代价升序排列）。这里不会重新求解二次误差。
     */
    void pushEdge(
        int a,
        int b,
        const std::vector<VertexState>& vertices,
        const std::vector<SolveResult>& placements,
        double additionalCost = 0.0
    );

private:
    std::priority_queue<Candidate> queue_;
};

} // 结束 manumesh::simplification 命名空间
