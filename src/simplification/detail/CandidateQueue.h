/**
 * @file src/simplification/detail/CandidateQueue.h
 * @brief 声明带缓存位置和版本戳的坍缩候选队列。
 * @ingroup manumesh_simplification
 *
 * @details 队列只负责排序与失效检测；候选合法性由 CollapseAttempt 单独评估。
 */

#pragma once

#include "detail/SimplificationTypes.h"

#include <queue>
#include <vector>

namespace manumesh {
namespace simplification {

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
     * @brief 返回当前堆中候选条目数（包括尚未弹出的过期条目）。
     *
     * 该数量只用于调度压缩阈值，不参与候选排序或合法性判断。
     */
    std::size_t size() const;
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

} // namespace simplification
} // namespace manumesh
