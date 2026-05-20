#pragma once

#include <functional>
#include <coroutine>
#include <vector>
#include "KhanConcepts.h"

template<typename Node>
struct TraceResult {
    std::vector<Node> path;
    bool is_maximal;
};

/**
 * The generator yields TraceResult objects, which contain the current path and whether the trace is maximal or not.
 * A trace is maximal if there are no currently enabled events that can be added to it. This allows the caller to distinguish between maximal traces and intermediate ones.
 * The impementation using co-routines & generators allow the caller to decide what to do with each trace (e.g., check if it's a configuration, print it, etc.) without having to generate all traces at once.
 */
template <typename T>
struct TraceGenerator {

    struct promise_type {
        T current_value;
        auto get_return_object() { return TraceGenerator{handle_type::from_promise(*this)}; }
        auto initial_suspend() { return std::suspend_always{}; }
        auto final_suspend() noexcept { return std::suspend_always{}; }
        void unhandled_exception() { std::terminate(); }
        auto yield_value(T&& value) {
            current_value = std::move(value);
            return std::suspend_always{};
        }
        void return_void() {}
    };

    using handle_type = std::coroutine_handle<promise_type>;
    
    handle_type h;

    TraceGenerator(handle_type h) : h(h) {}
    ~TraceGenerator() { if (h) h.destroy(); }
    
    bool next() { if (!h) return false; h.resume(); return !h.done(); }
    
    T move_value() { return std::move(h.promise().current_value); }
};

/**
 * DFS-based Trace Explorer using a recursive version of Khan's algorithm for incremental state updates. 
 * It can be used to generate traces of an event structure, or the traces of a sub-set of events (e.g., a configuration) by simply instantiating the tracker with the relevant subset and letting it dynamically query the relevant predecessors and conflicts for that subset.
 * The generator yields TraceResult objects, which contain the current path and whether the trace is maximal or not.
 * To adapth Khan's algorithm to different kind of Event Structures that have different enabling and conflict logic, we rely on a Tracker object that encapsulates the state of the system and provides methods to query enabled events and update the state when an event fires or is retracted.
 * The generator itself is agnostic to the specific logic of enabling and conflicts, and simply relies on the Tracker's interface. This allows us to reuse the same DFS exploration logic for Prime ES, Stable ES, Bundle ES, etc., by providing different Tracker implementations.
 * The impementation using co-routines & generators allow the caller to decide what to do with each trace (e.g., check if it's a configuration, print it, etc.) without having to generate all traces at once.
 * The generator is designed to be memory efficient, as it does not store all traces in memory at once, but rather generates them on-the-fly as the caller iterates through them. This is particularly beneficial for larger event structures where the number of traces can grow exponentially.
 * The maximality check is integrated into the generator, allowing the caller to easily distinguish between maximal traces (which are prefixes of no other traces). This is useful for various applications, such as configuration checking and deadlock-freeness.
 */
template <std::ranges::forward_range EventSet, class TrackerFactory>
requires KhanTrackerFactory<TrackerFactory, EventSet>
struct TraceEngineKhan{
    using Event = std::ranges::range_value_t<EventSet>;
    using TrackerType = std::invoke_result_t<TrackerFactory, EventSet>;

    EventSet events;
    TrackerType tracker;
    std::vector<Event> currentPath;

    TraceEngineKhan(EventSet&& events, const TrackerFactory& trackerFactory)
        : events(events), tracker(trackerFactory(events)) {}

    TraceGenerator<TraceResult<Event>> operator()() & {
        // 1. Find currently enabled nodes by querying the tracker
        // This replaces the internal frontier logic
        std::vector<Event> enabledCandidates;
        for (const auto& e : events)
            if (tracker.is_enabled(e))
                enabledCandidates.push_back(e);

        // 2. Yield current trace. Maximal if no events can currently fire.
        co_yield TraceResult<Event>{ currentPath, enabledCandidates.empty() };

        // 3. DFS Recursion
        for (const auto& en : enabledCandidates) {
            // --- State Update (Kahn's Incremental Push) ---
            tracker.push(en);
            currentPath.push_back(en);

            // Recurse with the same set of events
            auto subGen = (*this)();
            while (subGen.next())
                co_yield subGen.move_value();
            
            // --- Backtrack (Kahn's Incremental Pop) ---
            currentPath.pop_back();
            tracker.pop(en);
        }
    }
};

template <class TrackerFactory>
auto getKhanTraceGenFactory(TrackerFactory trackerFactory) {
    return [trackerFactory](auto&& events) {
        return TraceEngineKhan<decltype(events), TrackerFactory>(std::forward<decltype(events)>(events), trackerFactory);
    };  
}
