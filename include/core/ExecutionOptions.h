/**
 * @file include/core/ExecutionOptions.h
 * @brief 定义算法执行模式和并发资源约束。
 * @ingroup manumesh_core
 *
 * @details 公共 SDK 只暴露稳定的运行时策略，不暴露 oneTBB 调度器、任务或容器类型。
 *          未编译并行后端时，并行请求会按同一算法契约串行执行。
 */

#pragma once

#include "Export.h"

#include <cstddef>

namespace manumesh {

/// 算法调用使用的执行模式。
enum class ExecutionMode {
    /// 保持单线程执行，适合兼容调用、调试和基准基线。
    Serial = 0,
    /// 对算法声明为独立的范围启用内部并行后端。
    Parallel = 1,
};

/**
 * @brief 一次算法调用的并发和任务粒度约束。
 *
 * 该类型不拥有线程池。并行后端由库内部管理；拓扑修改、全局图整理和需要
 * 固定归约顺序的阶段仍可按算法契约串行执行。
 */
struct ExecutionOptions {
    /// 默认保持旧入口的串行行为；调用方应显式选择 Parallel。
    ExecutionMode mode = ExecutionMode::Serial;
    /// Parallel 模式下的最大并发度；0 表示由后端选择，1 等价于串行调度。
    int maxConcurrency = 0;
    /// 单个调度块的最小元素数，避免在小网格上产生过多任务。
    std::size_t minItemsPerTask = 4096;
};

/// 校验并发度和任务粒度。
/// @throws std::invalid_argument 当模式未知、并发度为负数或任务粒度为零时抛出。
MANUMESH_API void validateExecutionOptions(const ExecutionOptions& options);

/// @return 当前库是否编译了可用于 Parallel 模式的并行后端。
MANUMESH_API bool isParallelExecutionAvailable() noexcept;

/// @return 当前库的并行后端名称；无后端时返回 "serial"。返回字符串具有静态生命周期。
MANUMESH_API const char* parallelExecutionBackendName() noexcept;

} // namespace manumesh
