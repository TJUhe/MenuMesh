#include "common/detail/ParallelExecution.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace manumesh {
namespace common {
namespace parallel {
namespace {

TEST(ParallelExecution, CoversEachElementExactlyOnce) {
    constexpr std::size_t kElementCount = 4099;
    std::vector<std::atomic<int>> visits(kElementCount);
    for (std::atomic<int>& visit : visits) {
        visit.store(0, std::memory_order_relaxed);
    }

    RangeExecutionOptions options;
    options.grainSize = 37;
    options.maxConcurrency = 2;
    forEachRange(0, kElementCount, options, [&](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            visits[index].fetch_add(1, std::memory_order_relaxed);
        }
    });

    for (const std::atomic<int>& visit : visits) {
        EXPECT_EQ(visit.load(std::memory_order_relaxed), 1);
    }
}

TEST(ParallelExecution, DisabledModeRunsSingleFullRange) {
    RangeExecutionOptions options;
    options.enabled = false;
    options.maxConcurrency = 8;
    options.grainSize = 1;

    std::size_t observedBegin = 0;
    std::size_t observedEnd = 0;
    int calls = 0;
    forEachRange(3, 17, options, [&](std::size_t begin, std::size_t end) {
        observedBegin = begin;
        observedEnd = end;
        ++calls;
    });

    EXPECT_EQ(calls, 1);
    EXPECT_EQ(observedBegin, 3u);
    EXPECT_EQ(observedEnd, 17u);
}

TEST(ParallelExecution, RejectsEmptyCallback) {
    EXPECT_THROW(forEachRange(0, 1, {}, RangeFunction{}), std::invalid_argument);
}

} // namespace
} // namespace parallel
} // namespace common
} // namespace manumesh
