#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <utility>
#include "EveHorizonConcepts.h"

template <typename Event, class PredecessorGetter, class SuccessorGetter, class ConflictsGetter>
requires EventRangeGetter<PredecessorGetter, Event> &&
         EventRangeGetter<SuccessorGetter, Event> &&
         EventRangeGetter<ConflictsGetter, Event>
class FlowKhanBFSTracker {
public:
    struct EventState {
        
        // Tracks how many predesessor this event has already. These might be fired or might conflict with other predecessors that are be fired
        std::size_t missing_predecessors = 0;
        
        bool inhibited = false; // If true, this event is currently inhibited by a fired conflict and cannot become enabled.
        
        //Track how many of the predecessors has been inhibited by other predecessors, this will be used when a predecessor is fire 
        std::vector<bool> disabled_preds;
        
        EventState(std::size_t pred_count, bool inhibited) : missing_predecessors(pred_count), inhibited(inhibited), disabled_preds(pred_count) {}
        EventState() = default;

        // O(1) condition: At least one enabling set has 0 missing requirements, and no active conflicts
        bool is_enabled() const {
            return !inhibited && missing_predecessors == 0;
        }
    };

private:
    // Stable Causality: Node -> List of alternative histories (OR of ANDs)
    PredecessorGetter get_predecessors_;
    SuccessorGetter get_successors_;
    ConflictsGetter get_conflicts_;

    // Standard hash map layout: intuitive, robust, and clean
    std::unordered_map<Event, EventState> active_watch_list_;


    bool is_self_conflicting(const Event& n){
        auto conflicts = get_conflicts_(n);
        return std::ranges::find(conflicts, n) != std::end(conflicts);
    }

    auto sorted_conflicts_of(const Event& n){
        auto&& conflicts = get_conflicts_(n);
        std::vector<Event> sorted_conflicts(std::begin(conflicts), std::end(conflicts));
        std::sort(sorted_conflicts.begin(), sorted_conflicts.end());
        return sorted_conflicts;
    }

public:
    /**
     * Constructor for the StableKhanBFSTracker.
     * @param predecessor_getter A callable that retrieves the enabling sets for a given event. It must return the exact same predecessors for an event (in the same order) if it is called multiple times, as the tracker relies on the order to update missing counts correctly.
     * @param successores_getter A callable that retrieves the successors for a given event.
     * @param conflicts_getter A callable that retrieves the conflicts for a given event.
     */
    FlowKhanBFSTracker(PredecessorGetter predecessor_getter, SuccessorGetter successores_getter, ConflictsGetter conflicts_getter) 
        : get_predecessors_(predecessor_getter), get_successors_(successores_getter), get_conflicts_(conflicts_getter) {}

    EventState& register_discovered_node(const Event& n) {
        auto itr = active_watch_list_.find(n); 
        if(itr != active_watch_list_.end())
            return itr->second; // Already watching

        //We build a new state, we check the predecessors and the self-conflict
        EventState state(std::size(get_predecessors_(n)), is_self_conflicting(n));
        return active_watch_list_.emplace(n, std::move(state)).first->second;//TODO: must check failure here
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

        auto sorted_conflicts = sorted_conflicts_of(firedEvent);

        // Propagate Symmetric Conflicts first. This might disable a successor that conflicts with the fired event: a->b, a#b
        // which cannot be caught later unless we disable it now
        for (const auto& conf : sorted_conflicts) {
            // FiredEvent has occurred, meaning everything in symmetric conflict with it is now dead.
            active_watch_list_[conf].inhibited = true; // Mark it as inhibited. If no state, create a fresh one; we do not care about its enabling sets as it is inhibited anyway
        }

        // Propagate causality to potential successors
        for (const auto& succ : get_successors_(firedEvent)) {
            
            auto& state = register_discovered_node(succ);
            
            if(state.inhibited) continue; // No need to update the predecessors for inhibited events, they cannot be enabled anyway.

            --state.missing_predecessors;

            //fetch the predecessors, iterate over them with an index
            std::size_t idx = 0;
            for (const auto& pred: get_predecessors_(succ)) {
                //If any predecessor conflicts with the fired event, it is no longer needed for the successor to happen
                //So we reduce the missing_predecessors counter, unless it has been disabled by another predecessor before
                if(std::ranges::binary_search(sorted_conflicts, pred) && !state.disabled_preds[idx]){
                    state.disabled_preds[idx] = true;
                    --state.missing_predecessors;
                }
                ++idx;
            }

            if (state.is_enabled())
                newly_enabled.push_back(succ);
        }

        return newly_enabled;
    }
};
