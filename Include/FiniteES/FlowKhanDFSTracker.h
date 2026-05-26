#pragma once

#include <algorithm>
#include <vector>
#include <functional>
#include <ranges>
#include <unordered_map>
#include "EveHorizonConcepts.h"


template <std::ranges::input_range NodeSet, class PredecessorGetter, class ConflictGetter>
requires EventRangeGetter<PredecessorGetter, std::ranges::range_value_t<NodeSet>> &&
         EventRangeGetter<ConflictGetter, std::ranges::range_value_t<NodeSet>> 
class FlowKhanDFSTracker {

    using Node = std::ranges::range_value_t<NodeSet>;

    struct SuccessorInfo {
        Node successor;
        //Since enablement depends on conflicts between predecessors of an event, for each successor, we need to track how many of its predecessors are blocked.
        std::size_t conflicts_per_successor = 0;

        SuccessorInfo(Node s) : successor(s) {}

        bool operator<(const SuccessorInfo& other) const {
            return successor < other.successor;
        }
    };

    struct NodeState {
        std::size_t unsatisfied_preds = 0; // Number of flow-predecessors not yet fired or blocked
        std::size_t conflict_count = 0;    // Number of events in the current trace that conflict with this node
        //Keep track of succesors and how many enablers of that successor are in conflict with me
        std::vector<SuccessorInfo> successors;
        bool fired = false;
    };

    std::unordered_map<Node, NodeState> registry;
    std::vector<std::reference_wrapper<SuccessorInfo>> common_successors;   //Helper to be re-used in each push/pop call
    ConflictGetter get_conflicts;

private:

    /**
     * Helper to find common successors of a node and one of its conflicting event that might share the same successor. 
     * Since the info of the blocked_node need to be updated when we push or pop 'n', 
     * we return the common successors from the array of the successors of the blocked node, by reference, so that they can be modified.
     * */
    auto extract_common_successors(const Node& n, const Node& blocked_node) {        
        common_successors.clear();
        std::set_intersection(registry[blocked_node].successors.begin(), registry[blocked_node].successors.end(),
                              registry[n].successors.begin(), registry[n].successors.end(),
                              std::back_inserter(common_successors));
        return common_successors;
    }

    // Internal helper to handle the FES rule: d_fired || d_blocked
    void update_conf_pred_for_push(const Node& n, const Node& blocked_node) {
        // When a node 'd' is blocked by conflicting predecessor to the same successor, it no longer counts as an 
        // "unsatisfied predecessor" for that successor.
        auto common_successors = extract_common_successors(n, blocked_node);

        for (auto& succInfo : common_successors){
            if(succInfo.get().conflicts_per_successor++ == 0)
                registry[succInfo.get().successor].unsatisfied_preds -= 1;
        }

    } 
    
    // Internal helper to handle the FES rule: d_fired || d_blocked
    void update_conf_pred_for_pop(const Node& n, const Node& blocked_node) {
        // When a node 'd' is blocked by conflicting predecessor to the same successor, it no longer counts as an 
        // "unsatisfied predecessor" for that successor.

        auto common_successors = extract_common_successors(n, blocked_node);

        for (auto& succInfo : common_successors){
            if(--succInfo.get().conflicts_per_successor == 0)
                registry[succInfo.get().successor].unsatisfied_preds += 1;
        }
    }

public:
    FlowKhanDFSTracker(const NodeSet& nodes, PredecessorGetter get_preds, ConflictGetter get_confs): get_conflicts(get_confs) {
        for (const auto& n : nodes) {

            std::size_t pred_count = 0;

            // Build successor links: every predecessor must notify 'n' when it fires or is blocked
            for (const auto& p : get_preds(n)) {
                registry[p].successors.push_back(SuccessorInfo(n));
                pred_count++;
            }

            registry[n].unsatisfied_preds = pred_count; //Instead of calling the size of preds, which would require two iterations over pred, ie would require forward range instead of input range
        }

        //Sort all successors for all events. We will search later for common successors
        for (auto& [n, state] : registry)
            std::sort(state.successors.begin(), state.successors.end());
    }

    // O(1) check
    bool is_enabled(const Node& n) const {
        auto itr = registry.find(n);
        return itr != registry.end() && !itr->second.fired && itr->second.conflict_count == 0 && itr->second.unsatisfied_preds == 0;
    }

    void push(const Node& n) {
        auto& state = registry[n];
        state.fired = true;

        // 1. Notify conflicts: Firing 'n' disables everything it conflicts with
        for (const auto& conf_node : get_conflicts(n)) {
            registry[conf_node].conflict_count++;
            // Logic for FES: if a node becomes newly conflicted, it might "satisfy" 
            // the flow requirement for its own successors (the "blocked" rule).
            update_conf_pred_for_push(n, conf_node);
        }

        // 2. Notify successors: Firing 'n' satisfies its own flow-successors
        for (const auto& succInfo : state.successors) {
            registry[succInfo.successor].unsatisfied_preds--;
        }
    }

    void pop(const Node& n) {
        auto& state = registry[n];

        // 1. Revert successors
        for (const auto& succInfo : state.successors) {
            registry[succInfo.successor].unsatisfied_preds++;
        }

        // 2. Revert conflicts
        for (const auto& conf_node : get_conflicts(n)) {
            registry[conf_node].conflict_count--;
            update_conf_pred_for_pop(n, conf_node);
        }

        state.fired = false;
    }
};

auto getFlowKhanTrackerFactory(auto get_preds, auto get_confs) {
    return [get_preds, get_confs](const auto& nodes) {
        return FlowKhanDFSTracker(nodes, get_preds, get_confs);
    };
}
