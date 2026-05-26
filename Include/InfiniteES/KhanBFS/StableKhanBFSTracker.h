#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <utility>
#include "EveHorizonConcepts.h"

template <typename Event, class EnablingSetsGetter, class SuccessorGetter, class ConflictsGetter>
requires NestedEventRangeGetter<EnablingSetsGetter, Event> &&
         EventRangeGetter<SuccessorGetter, Event> &&
         EventRangeGetter<ConflictsGetter, Event>
class StableKhanBFSTracker {
public:
    struct EventState {
        // Tracks how many nodes of EACH enabling set have fired. Once any element in this vector hits 0, that event is enabled.
        std::vector<std::size_t> missing_counts_per_set;
        bool inhibited = false; // If true, this event is currently inhibited by a fired conflict and cannot become enabled.
        
        bool is_enabled() const {
            std::logical_not is_zero; // Function object for cleaner code: is_zero(x) returns true iff x == 0
            return !inhibited && std::ranges::any_of(missing_counts_per_set, is_zero);
        }
    };

private:
    // Stable Causality: Node -> List of alternative histories (OR of ANDs)
    EnablingSetsGetter get_enabling_sets_;
    SuccessorGetter get_successors_;
    ConflictsGetter get_conflicts_;

    // Standard hash map layout: intuitive, robust, and clean
    std::unordered_map<Event, EventState> active_watch_list_;

public:
    /**
     * Constructor for the StableKhanBFSTracker.
     * @param enabling_sets_getter A callable that retrieves the enabling sets for a given event. It must return the exact same enabling sets for an event (in the same order) if it is called multiple times, as the tracker relies on consistent indexing to update missing counts correctly.
     * @param successores_getter A callable that retrieves the successors for a given event.
     * @param conflicts_getter A callable that retrieves the conflicts for a given event.
     */
    StableKhanBFSTracker(auto enabling_sets_getter, auto successores_getter, auto conflicts_getter) 
        : get_enabling_sets_(enabling_sets_getter), get_successors_(successores_getter), get_conflicts_(conflicts_getter) {}

    EventState& register_discovered_node(const Event& n) {
        auto itr = active_watch_list_.find(n); 
        
        if(itr != active_watch_list_.end()) // Already watching
            return itr->second;

        const auto& enablers = get_enabling_sets_(n);
        EventState state;
        
        //Little optimization: reserve vector capacity upfront based on the number of enabling sets. This is a minor detail but it can save us some reallocations in cases where events have many alternative enabling sets.
        if constexpr (std::ranges::forward_range<decltype(enablers)>) { //Otherwise we cannot iterate twice on the same range
            state.missing_counts_per_set.reserve(std::size(enablers));
        }

        // For each alternative history, store the number of missing predecessors
        for (const auto& set : enablers)
            state.missing_counts_per_set.push_back(std::size(set));
        
        return active_watch_list_.emplace(n, std::move(state)).first->second;//TODO check the failure
    }

    // O(1) condition: At least one enabling set has 0 missing requirements, and no active conflicts
    bool is_enabled(const Event& n) const {
        auto it = active_watch_list_.find(n);
        return it != active_watch_list_.end() && it->second.is_enabled();
    }

    /**
     * Incremental Stable Step
     * Mutates local branch counters and removes elements aggressively.
     */
    std::vector<Event> fire_and_propagate(const Event& firedEvent) {
        std::vector<Event> newly_enabled;

        // 1. Clean up memory: The event has fired, drop it from the watcher
        active_watch_list_.erase(firedEvent);

        // 2. Propagate Symmetric Conflicts. Do it before setting the enablement counters, as there might be an event {a}->b & b#a
        for (const auto& conf : get_conflicts_(firedEvent))
            // FiredEvent has occurred, meaning everything in symmetric conflict with it is now inhibited.
            active_watch_list_[conf].inhibited = true; // If no state, create a fresh one and mark it as inhibited; we do not care about its enabling sets as it is inhibited anyway

        // 3. Propagate causality to potential successors
        for (const auto& succ : get_successors_(firedEvent)) {
            auto& state = register_discovered_node(succ);
            
            if(state.inhibited) continue; // No need to update enabling sets for inhibited events, they cannot be enabled anyway. Besides, the vector might not be filled with values.

            //We fetch enabling sets (assuming they are returned in the same order as when we registered the node) and decrement the corresponding counters for each set that contains the fired event. 
            //We need the index to know which counter to decrement, but we cannot use an index on the enabling-sets range as it might not be a random-access range. Instead, we can just maintain a separate index variable that increments each time we move to the next enabling set.
            std::size_t set_index = 0;
            for (const auto& set: get_enabling_sets_(succ)) {
                // If the fired event satisfies a component of this specific history path...
                if (std::ranges::find(set, firedEvent) != std::end(set)) {
                    // Find the corresponding missing count and decrement it
                    --state.missing_counts_per_set[set_index];
                }
                set_index++;
            }

            if (state.is_enabled())
                newly_enabled.push_back(succ);
        }

        return newly_enabled;
    }
};
