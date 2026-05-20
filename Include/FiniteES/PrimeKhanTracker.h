#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <functional>

/**
 * PrimeKhanTracker is a state tracker used for Khan's algorithm of generating traces specific to Prime Event Structures, which have a simple enabling condition based on immediate causal predecessors and symmetric conflicts. In a Prime ES, an event is enabled if all of its immediate causal predecessors have fired and none of its conflicting events have fired.
 * The PrimeKhanTracker maintains the state of which events have fired, how many immediate predecessors of each event have not yet fired (using an in-degree counter), and how many conflicts are currently active for each event. This allows it to efficiently determine which events are currently enabled and to update the state when events are fired or retracted.
 * The tracker can be used to explore traces of a sub-set of events (say a configuration), or the whole set of events, as it dynamically queries the predecessors and conflicts for the relevant events.
 */
template <typename Event>
class PrimeKhanTracker {

    struct NodeState {
        std::size_t in_degree = 0;       // Decremented when a predecessor fires
        std::size_t conflict_count = 0;  // Incremented when a conflicting event fires
        bool fired = false;
        
        // Forward links are still pre-calculated to maintain the O(1) push pattern
        std::vector<Event> successors; 
    };

    std::unordered_map<Event, NodeState> registry;
    std::function<std::vector<Event>(const Event&)> get_conflicts;

public:
    /**
     * @param scope The specific subset of events we want to track (e.g. candidateSet)
     * @param get_predecessors Func: Event -> Range of its immediate causal predecessors
     * @param get_conf_func Func: Event -> Range of its symmetric conflicts
     */
    PrimeKhanTracker(
        const auto& scope, 
        auto&& get_predecessors, 
        auto&& get_conf_func
    ) : get_conflicts(std::forward<decltype(get_conf_func)>(get_conf_func)) {
         // Invert the backward causality to build forward link successions
        for (const auto& n : scope) {
            registry[n] = NodeState{};
            auto predecessors = get_predecessors(n);
            for (const auto& pred : predecessors) {
                registry[pred].successors.push_back(n); // Build forward link
                registry[n].in_degree++;                // Set up Kahn counter
            }
        }
    }

    // O(1) Enablement Check
    bool is_enabled(const Event& e) const {
        auto it = registry.find(e);
        if(it == registry.end()) {
            return false;
        }
        const auto& state = it->second;
        return !state.fired && state.in_degree == 0 && state.conflict_count == 0;
    }

    // Incremental Kahn Step: Dynamically loops through functor conflicts
    void push(const Event& n) {
        auto& state = registry[n];
        state.fired = true;

        for (const auto& succ : state.successors) {
            registry[succ].in_degree--;
        }
        
        // On-the-fly conflict iteration
        for (const auto& conf : get_conflicts(n)) {
            if (registry.contains(conf)) {
                registry[conf].conflict_count++;
            }
        }
    }

    // Symmetrical Backtracking step: Dynamically loops through functor conflicts
    void pop(const Event& n) {
        auto& state = registry[n];

        for (const auto& conf : get_conflicts(n)) {
            if (registry.contains(conf)) {
                registry[conf].conflict_count--;
            }
        }
        
        for (const auto& succ : state.successors) {
            registry[succ].in_degree++;
        }
        state.fired = false;
    }
};

template <class PredecessorGetter, class ConflictGetter>
auto getPrimeKhanTrackerFactory(PredecessorGetter&& get_preds, ConflictGetter&& get_confs) {
    return [get_preds = std::forward<PredecessorGetter>(get_preds), get_confs = std::forward<ConflictGetter>(get_confs)](const auto& events) {
        using Event = std::ranges::range_value_t<decltype(events)>;
        return PrimeKhanTracker<Event>(events, get_preds, get_confs);   //Do not move, as this might be multiple times for the same factory with different node sets.
    };
}