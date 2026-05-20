#pragma once

#include <vector>
#include <algorithm>
#include <set>
#include <ranges>
#include "TraceGenerator.h"

/**
 * Checks if a maximal trace is "complete".
 * A trace is complete if every event in the system has either:
 * 1. Fired
 * 2. Been inhibited/conflicted by something that fired.
 */
template <std::ranges::random_access_range SortedTrace, std::ranges::input_range EventSet, class GetInhibitors>
bool isComplete(const SortedTrace& sorted_trace, const EventSet& allEvents, const GetInhibitors& get_inhibitors) {
    
    return std::ranges::all_of(allEvents, [&sorted_trace, &get_inhibitors](const auto& e) {
        // Either the event 'n' is in the trace (fired)
        return std::ranges::binary_search(sorted_trace, e) ||
            // or it has a conflict that is in the trace (blocked)
            std::ranges::any_of(get_inhibitors(e), [&sorted_trace](const auto& inh) {
                // Check if any conflicting event 'f' is in the trace
                return std::ranges::binary_search(sorted_trace, inh);
            });
    });
}

template <class Event>
struct DeadlockCheckResult{
    std::optional<std::vector<Event>> _counterexample; // Optional: a trace leading to deadlock if not free
    DeadlockCheckResult(std::vector<Event> trace) : _counterexample(std::move(trace)) {}
    DeadlockCheckResult() : _counterexample(std::nullopt) {}

    constexpr explicit operator bool() const noexcept{
        return !_counterexample.has_value(); // Valid if there is no counterexample trace
    }

    auto counterexample() const {
        return _counterexample.value();
    }
};

/**
 * Reuses the DFS Trace Generator to check for deadlock-freeness.
 * Succeeds if all maximal traces are complete.
 * Returns false with a counterexample trace iff it finds a maximal trace that is not complete, which demonstrates the presence of a deadlock in the system.
 * The function relies on the correctness of the trace generator and the completeness check.
 */
template <typename TrackerFactory, std::ranges::forward_range EventSet>
requires KhanTrackerFactory<TrackerFactory, EventSet>
auto isDeadlockFree(const EventSet& allEvents, const TrackerFactory& trackerFactory, const auto& get_inhibitors) {

    using Event = std::ranges::range_value_t<EventSet>;

    // 1. Launch the DFS Generator
    auto traceEngine = getKhanTraceGenFactory(trackerFactory)(allEvents);
    auto gen = traceEngine();

    // 2. Either the trace is not maximal, or it is complete
    while (gen.next()) {
        auto result = gen.move_value();
        if (!result.is_maximal) continue;

        // For completeness check, we need the trace to be sorted to use binary_search
        //std::vector<Node> sorted_trace = result.path;
        std::ranges::sort(result.path);

        if (!isComplete(result.path, allEvents, get_inhibitors))
            return DeadlockCheckResult<Event>{std::move(result.path)}; // Found a maximal trace that is not complete, return it as a counterexample
    }

    return DeadlockCheckResult<Event>();
}
