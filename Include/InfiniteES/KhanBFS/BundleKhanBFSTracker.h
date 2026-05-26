#pragma once
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <memory>
#include "EveHorizonConcepts.h"

template <typename Node, class BundleGetter, class SuccessorGetter, class InhibitedGetter>
requires NestedEventRangeGetter<BundleGetter, Node> &&
         EventRangeGetter<SuccessorGetter, Node> &&
         EventRangeGetter<InhibitedGetter, Node>
class BundleKhanBFSTracker {
public:
    struct PendingState {
        std::size_t required_bundles = 0;   //Will hold the number of bundles enabling this event initially, and will be decremented as we fire events that satisfy those bundles. Once it hits 0, this event is enabled.
        bool inhibited = false; // If true, this event is currently inhibited by a fired conflict and cannot become enabled.
        
        bool is_enabled() const {
            return !inhibited && required_bundles == 0;
        }
    };

private:
    // Captures your core system logic getters
    BundleGetter get_bundles;
    SuccessorGetter get_successors;
    InhibitedGetter get_inhibited;

    // The ACTIVE local memory of this specific execution branch
    // Only contains nodes that have been discovered (direct successors of fired events) but haven't fired yet.
    std::unordered_map<Node, PendingState> active_watch_list;


public:
    BundleKhanBFSTracker(BundleGetter b, SuccessorGetter s, InhibitedGetter i)
        : get_bundles(std::forward<BundleGetter>(b)), get_successors(std::forward<SuccessorGetter>(s)), get_inhibited(std::forward<InhibitedGetter>(i)) {}

    // Bootstrap: seed the tracking system with an initial node
    PendingState& register_discovered_node(const Node& n) {
        auto itr = active_watch_list.find(n); 
        if (itr != active_watch_list.end()) return itr->second;
        
        //else create a new state and set the required bundle count to the number of bundles for this node.
        return active_watch_list.insert({n, PendingState{std::size(get_bundles(n)), false}}).first->second;
    }

    /**
     * Slashes computation to O(1) per node.
     * A discovered node is fully enabled if its causality is met and it isn't inhibited.
     */
    bool is_enabled(const Node& n) const {
        auto it = active_watch_list.find(n);
        return it != active_watch_list.end() && it->second.is_enabled();
    }

    /**
     * Incremental Step: Update local counters and garbage-collect fired memory.
     * Returns newly discovered and newly enabled nodes for the BFS frontier.
     */
    std::vector<Node> fire_and_propagate(const Node& firedNode) {
        std::vector<Node> newly_enabled;

        // 1. Clean up the fired event itself
        active_watch_list.erase(firedNode);

        // 2. Disable conflicting events before updating the enablement relation as there can be an event {a}->b and a#b
        for (const auto& inh : get_inhibited(firedNode))
            // Mark it as inhibited. If no state, create a fresh one; we do not care about its enabling counter as it is inhibited anyway
            active_watch_list[inh].inhibited = true; 

        // 3. Discover and update successors
        for (const auto& succ : get_successors(firedNode)) {
            auto& state = register_discovered_node(succ);
            
            if(state.inhibited) continue; // No need to update enabling sets for inhibited events, they cannot be enabled anyway. Besides, the counter might not have the right value.
            
            for (const auto& b : get_bundles(succ)) 
                if (std::ranges::find(b, firedNode) != std::end(b))
                    state.required_bundles--;

            if (state.is_enabled()) 
                newly_enabled.push_back(succ);
        }

        return newly_enabled;
    }

};
