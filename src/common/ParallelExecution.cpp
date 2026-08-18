/**
 * @file src/common/ParallelExecution.cpp
 * @brief 实现内部范围并行执行适配层。
 * @ingroup manumesh_common
 */

#include "common/detail/ParallelExecution.h"

#if defined(MANUMESH_HAS_ONETBB)
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/partitioner.h>
#include <tbb/task_arena.h>
#endif

#include <algorithm>
#include <stdexcept>

namespace manumesh {
namespace common {
namespace parallel {
namespace {

constexpr std::size_t kDefaultGrainSize = 256;

} // namespace

bool isOneTbbAvailable() noexcept {
#if defined(MANUMESH_HAS_ONETBB)
    return true;
#else
    return false;
#endif
}

RangeExecutionOptions makeRangeExecutionOptions(const ExecutionOptions& options) {
    validateExecutionOptions(options);
    RangeExecutionOptions result;
    result.enabled = options.mode == ExecutionMode::Parallel;
    result.maxConcurrency = options.maxConcurrency;
    result.grainSize = options.minItemsPerTask;
    return result;
}

void forEachRange(
    std::size_t begin, std::size_t end, const RangeExecutionOptions& options, const RangeFunction& function
) {
    if (!function) {
        throw std::invalid_argument("parallel range execution requires a callback");
    }
    if (begin >= end) {
        return;
    }

    const std::size_t grainSize = std::max<std::size_t>(1, options.grainSize ? options.grainSize : kDefaultGrainSize);

#if defined(MANUMESH_HAS_ONETBB)
    if (options.enabled && options.maxConcurrency != 1) {
        const auto work = [&]() {
            // static_partitioner fixes the logical range split. Algorithms that
            // require deterministic floating-point reduction still own it.
            tbb::parallel_for(
                tbb::blocked_range<std::size_t>(begin, end, grainSize),
                [&](const tbb::blocked_range<std::size_t>& range) { function(range.begin(), range.end()); },
                tbb::static_partitioner{}
            );
        };
        if (options.maxConcurrency > 1) {
            tbb::task_arena arena(options.maxConcurrency);
            arena.execute(work);
        } else {
            work();
        }
        return;
    }
#else
    static_cast<void>(options);
    static_cast<void>(grainSize);
#endif

    function(begin, end);
}

} // namespace parallel
} // namespace common
} // namespace manumesh

namespace manumesh {

void validateExecutionOptions(const ExecutionOptions& options) {
    if (options.mode != ExecutionMode::Serial && options.mode != ExecutionMode::Parallel) {
        throw std::invalid_argument("ExecutionOptions::mode is invalid.");
    }
    if (options.maxConcurrency < 0) {
        throw std::invalid_argument("ExecutionOptions::maxConcurrency must be non-negative.");
    }
    if (options.minItemsPerTask == 0) {
        throw std::invalid_argument("ExecutionOptions::minItemsPerTask must be positive.");
    }
}

bool isParallelExecutionAvailable() noexcept { return common::parallel::isOneTbbAvailable(); }

const char* parallelExecutionBackendName() noexcept {
    return common::parallel::isOneTbbAvailable() ? "oneTBB" : "serial";
}

} // namespace manumesh
