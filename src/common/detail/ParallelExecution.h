/**
 * @file src/common/detail/ParallelExecution.h
 * @brief 内部稳定的范围并行执行适配层。
 * @ingroup manumesh_common
 *
 * @details 公共 SDK 不暴露调度器类型。算法模块只通过本接口申请独立、半开
 * 区间的计算；oneTBB 不可用或被关闭时实现保持相同的串行语义。
 */

#pragma once

#include "core/ExecutionOptions.h"

#include <cstddef>
#include <functional>

namespace manumesh {
namespace common {
namespace parallel {

/** @brief 对一次独立范围计算的资源和调度约束。 */
struct RangeExecutionOptions {
    /** false 时强制串行执行，适合基准、调试和可重复性诊断。 */
    bool enabled = true;
    /** 0 表示交由后端选择；正数限制本次调用可使用的 worker 数。 */
    int maxConcurrency = 0;
    /** 每个调度块的最小元素数；0 会使用实现的保守默认值。 */
    std::size_t grainSize = 0;
};

using RangeFunction = std::function<void(std::size_t begin, std::size_t end)>;

/** @brief 编译后的库是否包含 oneTBB 后端。 */
bool isOneTbbAvailable() noexcept;

/** @brief 将稳定公共策略转换为内部范围调度参数。 */
RangeExecutionOptions makeRangeExecutionOptions(const ExecutionOptions& options);

/**
 * @brief 执行 [begin, end) 内不重叠的范围任务。
 *
 * 回调可能在多个线程中并发调用，因此只能写入不重叠输出区间或使用调用方
 * 自己的同步。任务分块顺序不是排序/归约契约；浮点归约必须由算法显式固定。
 */
void forEachRange(
    std::size_t begin, std::size_t end, const RangeExecutionOptions& options, const RangeFunction& function
);

} // namespace parallel
} // namespace common
} // namespace manumesh
