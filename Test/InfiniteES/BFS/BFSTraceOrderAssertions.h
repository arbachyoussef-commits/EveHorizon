#pragma once

#include <cstddef>
#include <ranges>

#include <gtest/gtest.h>

/**
 * Checks the breadth-first ordering invariant for a sequence of traces.
 *
 * A breadth-first explorer emits every trace of depth d before any trace of
 * depth d + 1, so trace lengths must be nondecreasing in the generated
 * sequence. The order among traces with the same length is intentionally not
 * constrained, since it can depend on the order in which enabled events are
 * discovered.
 *
 * Returns a GoogleTest assertion result. If the depths decrease, the failure
 * message reports the adjacent trace indices and their depths.
 */
template <std::ranges::forward_range TraceRange>
testing::AssertionResult has_breadth_first_trace_order(const TraceRange &traces)
{
    auto current = std::ranges::begin(traces);
    const auto end = std::ranges::end(traces);
    if (current == end)
        return testing::AssertionSuccess();

    auto previous_depth = std::ranges::distance(*current);
    std::size_t previous_index = 0;
    for (++current; current != end; ++current)
    {
        const auto depth = std::ranges::distance(*current);
        const auto index = previous_index + 1;
        if (depth < previous_depth)
        {
            return testing::AssertionFailure()
                << "trace at index " << index << " has depth " << depth
                << " after trace at index " << previous_index
                << " with depth " << previous_depth;
        }

        previous_depth = depth;
        previous_index = index;
    }

    return testing::AssertionSuccess();
}
