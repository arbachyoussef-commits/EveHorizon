#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include "EveHorizonConcepts.h"


/**
 * StableKhanTracker is a state tracker used for Khan's algorithm of generating traces specific to Stable Event Structures, which have a more complex enabling condition than Prime ES. In a Stable ES, an event can be enabled by multiple "enabling sets" of events, where each set is an AND-set of events (i.e., all events in the set must have fired for that set to be satisfied), and the overall enabling condition is an OR over those sets (i.e., at least one enabling set must be satisfied for the event to be enabled).
 * The StableKhanTracker maintains the state of which events have fired, how many enabling sets for each event have been satisfied, and how many conflicts are currently active for each event. This allows it to efficiently determine which events are currently enabled and to update the state when events are fired or retracted.
 * This tracker can be used to explore traces of a sub-set of events (say a configuration), or the whole set of events.
 * The enabling sets are represented as a vector of vectors, where each inner vector represents an AND-set of events that can enable the target event. The tracker keeps track of how many events in each enabling set have fired, and when an enabling set becomes fully satisfied (all its events have fired), it increments the count of satisfied sets for the target event. An event is enabled if at least one of its enabling sets is satisfied.
 * The tracker also handles conflicts by keeping a count of how many conflicting events have fired for each event, and an event is not enabled if it has any active conflicts.
 * Note: this tracker does not check the stability condition of the event structure (i.e., that no two enabling sets of the same event can be satisfied at the same time), as it assumes that the input event structure is already a valid Stable ES. It simply implements the enabling and conflict logic according to the definition of Stable ES.
 */
template <std::ranges::input_range EventSet, typename EnablersGetter, typename ConflictGetter>
requires NestedEventRangeGetter<EnablersGetter, std::ranges::range_value_t<EventSet>> &&
         EventRangeGetter<ConflictGetter, std::ranges::range_value_t<EventSet>>
class StableKhanDFSTracker {

    using Event = std::ranges::range_value_t<EventSet>;

    struct EnablingSet {
        Event target;
        std::size_t remaining; // How many nodes in this AND-set still need to fire
        std::size_t initial_count;
    };

    struct NodeState {
        std::size_t satisfied_sets = 0; // If > 0, causality is met
        std::size_t conflict_count = 0;
        bool fired = false;
        std::vector<std::size_t> sets_i_belong_to;
    };

    std::unordered_map<Event, NodeState> registry;
    std::vector<EnablingSet> all_sets;
    ConflictGetter get_conflicts;

public:
    StableKhanDFSTracker(const EventSet& events, EnablersGetter get_enablers, ConflictGetter get_conf) : get_conflicts(get_conf) {
        for (const auto& e : events) {
            for (auto& set : get_enablers(e)) {
                auto& s = all_sets.emplace_back(e, set.size(), set.size());
                if (s.initial_count == 0) 
                    registry[e].satisfied_sets++;
                else
                    for (const auto& dep : set) registry[dep].sets_i_belong_to.push_back(all_sets.size() - 1);
            }
        }
    }

    bool is_enabled(const Event& e) const {
        auto it = registry.find(e);
        if(it == registry.end()) {
            return false;
        }
        const auto& s = it->second;
        return !s.fired && s.conflict_count == 0 && s.satisfied_sets > 0;
    }

    void push(const Event& e) {
        auto& s = registry[e];
        s.fired = true;
        for (const auto& set_idx : s.sets_i_belong_to) {
            auto& set = all_sets[set_idx];
            if (--set.remaining == 0) registry[set.target].satisfied_sets++;
        }
        for (const auto& c : get_conflicts(e)) registry[c].conflict_count++;
    }

    void pop(const Event& e) {
        auto& s = registry[e];
        for (const auto& c : get_conflicts(e)) registry[c].conflict_count--;
        for (const auto& set_idx : s.sets_i_belong_to) {
            auto& set = all_sets[set_idx];
            if (set.remaining == 0) registry[set.target].satisfied_sets--;
            set.remaining++;
        }
        s.fired = false;
    }
};

auto getStableKhanTrackerFactory(auto get_enablers, auto get_confs) {
    return [get_enablers, get_confs](const auto& events) {
        return StableKhanDFSTracker(events, get_enablers, get_confs);
    };
}