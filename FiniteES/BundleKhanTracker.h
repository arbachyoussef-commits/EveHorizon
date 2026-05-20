#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

/**
 * BundleKhanTracker is a state tracker used for Khan's algorithm of generating traces specific to Bundle Event Structures, which have a more complex enabling condition than Prime or Stable ES. In a Bundle ES, an event can be enabled by multiple "bundles" of events, where each bundle is an OR-set of events (i.e., at least one event from the bundle must have fired). 
 * It allows for asymmetric conflicts (inhibitors) where the firing of one event can disable another event, which allows the tracker to be used for extended bundle event structures as well.
 * The BundleKhanTracker maintains the state of which events have fired, how many bundles for each event have been satisfied, and how many conflicts are currently active for each event. This allows it to efficiently determine which events are currently enabled and to update the state when events are fired or retracted.
 * This tracker can be used to explore traces of a sub-set of events (say a configuration), or the whole set of events.
 */
template <typename Event>
class BundleKhanTracker {

    struct Bundle {
        Event target;
        std::size_t fired_count = 0; // How many events in this OR-set have fired
    };

    struct NodeState {
        std::size_t satisfied_bundles = 0;
        std::size_t required_bundles = 0;
        std::size_t inhibitor_count = 0; // For Extended: used for inhibitors
        bool fired = false;
        std::vector<std::size_t> bundles_i_belong_to;   //indexes to all_bundles
    };

    std::unordered_map<Event, NodeState> registry;
    std::vector<Bundle> all_bundles;
    std::function<std::vector<Event>(const Event&)> inhibited_by;

public:
    /**
     * Constructor takes:
     * - A list of events in the system (to initialize the registry). It can be the whole set of events, or a subset (e.g., when exploring a configuration).
     * - A function that, given an event, returns the bundles (OR-sets) that enable it. Each bundle is represented as a vector of nodes, where at least one must be present for the bundle to be satisfied.
     * - A function that, given an event, returns the list of events it inhibits (asymmetric conflict). This allows the tracker to be used for extended bundle event structures where events can disable others.
     * 
     * The tracker will keep track of all events in the set passed, as well as any events that appear in the bundles of those events, even if they are not in the initial set (since they can affect enabling conditions). However, it will ignore any events that do not appear in the initial set or in the bundles of those events, as they are irrelevant for the exploration of that set.
     * Besides, it keeps track of the events inhibited by the events in the set, even if they are not in the set themselves, since they can affect the enabling conditions of events in the set.
     * This design allows for flexibility in exploring different subsets of the event structure (e.g., configurations) while still correctly handling the enabling and conflict logic that may involve events outside of that subset.
     * Note: the tracker does not check the validity of the input (e.g., that the events of the one bundle are in conflict), as it assumes that the input is already a valid representation of a Bundle ES. It simply implements the enabling and conflict logic according to the definition of Bundle ES.
     */
    BundleKhanTracker(const auto& events, auto&& get_bundles, auto&& get_inhibited_by) : inhibited_by(std::forward<decltype(get_inhibited_by)>(get_inhibited_by)) {
        for (const auto& e : events) {
            auto bundles = get_bundles(e);
            registry[e].required_bundles = bundles.size();
            for (auto& b_set : bundles) {
                all_bundles.emplace_back(e, 0);
                for (const auto& member : b_set) registry[member].bundles_i_belong_to.push_back(all_bundles.size() - 1);
            }
        }
    }

    bool is_enabled(const Event& e) const {
        auto it = registry.find(e);
        if(it == registry.end()) {
            return false;
        }
        const auto& s = it->second;
        return !s.fired && s.inhibitor_count == 0 && s.satisfied_bundles == s.required_bundles;
    }

    void push(const Event& e) {
        auto& s = registry[e];
        s.fired = true;
        for (std::size_t idx : s.bundles_i_belong_to) {
            auto& b = all_bundles[idx];
            if (b.fired_count++ == 0) registry[b.target].satisfied_bundles++;
        }
        for (const auto& inh : inhibited_by(e)) registry[inh].inhibitor_count++;
    }

    void pop(const Event& e) {
        auto& s = registry[e];
        for (const auto& inh : inhibited_by(e)) registry[inh].inhibitor_count--;
        for (std::size_t idx : s.bundles_i_belong_to) {
            auto& b = all_bundles[idx];
            if (--b.fired_count == 0) registry[b.target].satisfied_bundles--;
        }
        s.fired = false;
    }

};

/**
 * A factory creator method that takes a bundle getter function and an inhibitor getter function, and returns a TrackerFactory lambda that can be used to create BundleKhanTracker instances for different sets of nodes.
 * The bundle getter function defines the enabling conditions for each event in terms of bundles (AND-of-ORs), while the inhibitor getter function defines the asymmetric conflicts (inhibitors) between events.
 * The returned lambda, when called with a list of nodes, creates a BundleKhanTracker for those nodes using the provided bundle and inhibitor functions. This design allows us to easily create trackers for different sets of nodes while reusing the same bundle and inhibitor logic.
 */
template <class BundleGetter, class InhibitorGetter>
auto getBundleKhanTrackerFactory(BundleGetter&& get_bundles, InhibitorGetter&& get_inhibited_by) {
    return [get_bundles = std::forward<BundleGetter>(get_bundles), get_inhibited_by = std::forward<InhibitorGetter>(get_inhibited_by)](const auto& nodes) {
        using Event = std::ranges::range_value_t<decltype(nodes)>;
        return BundleKhanTracker<Event>(nodes, get_bundles, get_inhibited_by);
    };
}