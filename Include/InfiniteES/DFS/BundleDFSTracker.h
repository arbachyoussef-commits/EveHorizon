#pragma once

#include <cassert>
#include <cstddef>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include "EveHorizonConcepts.h"

/**
 * Dynamic bundle-event-structure tracker for the infinite DFS explorer.
 *
 * Every enabling bundle is an OR-set: at least one member must fire. An event
 * is enabled when all of its bundles are satisfied and no fired event inhibits
 * it. State is created for roots and discovered successors. push/pop use an
 * undo stack to restore bundle counters and inhibitor counts across DFS
 * branches.
 */
template <typename EventSet, typename BundleGetter, typename SuccessorGetter, typename InhibitorGetter>
    requires NestedEventRangeGetter<BundleGetter, std::ranges::range_value_t<EventSet>> &&
             EventRangeGetter<SuccessorGetter, std::ranges::range_value_t<EventSet>> &&
             EventRangeGetter<InhibitorGetter, std::ranges::range_value_t<EventSet>>
class BundleDFSTracker
{
public:
    using Event = std::ranges::range_value_t<EventSet>;

private:
    struct EventState
    {
        std::size_t required_bundles = 0;
        std::vector<std::size_t> fired_per_bundle;
        std::size_t inhibitor_count = 0;
        bool fired = false;

        bool is_enabled() const
        {
            return !fired && inhibitor_count == 0 && required_bundles == 0;
        }
    };

    struct BundleUndo
    {
        Event target;
        std::size_t bundle_index;
    };

    struct UndoFrame
    {
        Event fired_event;
        std::vector<Event> inhibited_events;
        std::vector<BundleUndo> incremented_bundle_counts;
        std::vector<Event> touched_events;
    };

    BundleGetter get_bundles_;
    SuccessorGetter get_successors_;
    InhibitorGetter get_inhibitors_;
    std::unordered_map<Event, EventState> states_;
    std::vector<UndoFrame> undo_stack_;

    EventState make_initial_state(const Event &event) const
    {
        EventState state;
        for (const auto &bundle : get_bundles_(event))
        {
            (void)bundle;
            ++state.required_bundles;
            state.fired_per_bundle.push_back(0);
        }
        return state;
    }

    EventState &ensure_state_registered(const Event &event)
    {
        auto it = states_.find(event);
        if (it != states_.end())
            return it->second;
        return states_.emplace(event, make_initial_state(event)).first->second;
    }

    void garbage_collect_if_idle(const Event &event)
    {
        const auto it = states_.find(event);
        if (it == states_.end())
            return;

        const auto &state = it->second;
        if (state.fired || state.inhibitor_count != 0)
            return;

        const auto initial_state = make_initial_state(event);
        if (state.required_bundles == initial_state.required_bundles &&
            state.fired_per_bundle == initial_state.fired_per_bundle)
            states_.erase(it);
    }

public:
    BundleDFSTracker(const EventSet &initial_roots,
                     BundleGetter get_bundles,
                     SuccessorGetter get_successors,
                     InhibitorGetter get_inhibitors)
        : get_bundles_(std::move(get_bundles)),
          get_successors_(std::move(get_successors)),
          get_inhibitors_(std::move(get_inhibitors))
    {
        for (const auto &root : initial_roots)
            ensure_state_registered(root);
    }

    bool is_enabled(const Event &event) const
    {
        const auto it = states_.find(event);
        if (it != states_.end())
            return it->second.is_enabled();

        // A reclaimed event is at its structural baseline. With no bundles,
        // that baseline is enabled unless an inhibitor is still active (which
        // would keep the event registered).
        return std::ranges::empty(get_bundles_(event));
    }

    std::vector<Event> push(const Event &fired_event)
    {
        auto &fired_state = ensure_state_registered(fired_event);
        assert(fired_state.is_enabled());
        fired_state.fired = true;

        UndoFrame undo{fired_event, {}, {}, {fired_event}};

        // Apply inhibitors before enabling updates, matching BundleBFSTracker.
        for (const auto &inhibited : get_inhibitors_(fired_event))
        {
            ++ensure_state_registered(inhibited).inhibitor_count;
            undo.inhibited_events.push_back(inhibited);
            undo.touched_events.push_back(inhibited);
        }

        std::vector<Event> newly_enabled;
        for (const auto &successor : get_successors_(fired_event))
        {
            auto &state = ensure_state_registered(successor);
            const bool was_enabled = state.is_enabled();
            if (state.fired || state.inhibitor_count != 0)
                continue;

            std::size_t bundle_index = 0;
            for (const auto &bundle : get_bundles_(successor))
            {
                if (std::ranges::find(bundle, fired_event) != std::ranges::end(bundle))
                {
                    auto &fired_count = state.fired_per_bundle[bundle_index];
                    if (fired_count++ == 0)
                        --state.required_bundles;
                    undo.incremented_bundle_counts.push_back({successor, bundle_index});
                    undo.touched_events.push_back(successor);
                }
                ++bundle_index;
            }

            if (!was_enabled && state.is_enabled())
                newly_enabled.push_back(successor);
        }

        undo_stack_.push_back(std::move(undo));
        return newly_enabled;
    }

    void pop(const Event &fired_event)
    {
        assert(!undo_stack_.empty());
        assert(undo_stack_.back().fired_event == fired_event);

        auto undo = std::move(undo_stack_.back());
        undo_stack_.pop_back();

        for (auto it = undo.incremented_bundle_counts.rbegin(); it != undo.incremented_bundle_counts.rend(); ++it)
        {
            auto &state = ensure_state_registered(it->target);
            if (--state.fired_per_bundle[it->bundle_index] == 0)
                ++state.required_bundles;
        }

        for (auto it = undo.inhibited_events.rbegin(); it != undo.inhibited_events.rend(); ++it)
            --ensure_state_registered(*it).inhibitor_count;

        ensure_state_registered(fired_event).fired = false;

        for (const auto &event : undo.touched_events)
            garbage_collect_if_idle(event);
    }
};

template <typename EventSet, typename BundleGetter, typename SuccessorGetter, typename InhibitorGetter>
auto getBundleDFSTrackerFactory(BundleGetter get_bundles,
                                SuccessorGetter get_successors,
                                InhibitorGetter get_inhibitors)
{
    return [get_bundles = std::move(get_bundles),
            get_successors = std::move(get_successors),
            get_inhibitors = std::move(get_inhibitors)](const EventSet &events)
    {
        return BundleDFSTracker<EventSet, BundleGetter, SuccessorGetter, InhibitorGetter>(
            events, get_bundles, get_successors, get_inhibitors);
    };
}
