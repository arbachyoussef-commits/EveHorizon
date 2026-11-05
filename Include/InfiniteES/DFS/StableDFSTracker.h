#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include "EveHorizonConcepts.h"

/**
 * Dynamic stable-event-structure tracker for the infinite DFS explorer.
 *
 * An event is enabled when at least one of its enabling sets has no remaining
 * predecessors and it has not fired or been inhibited by a conflict. Each
 * enabling set is an AND; the alternatives are an OR. State is registered
 * lazily for roots and discovered successors. push/pop maintain an undo stack
 * so DFS can restore the exact state of the parent branch.
 */
template <typename EventSet, typename EnablersGetter, typename SuccessorGetter, typename ConflictGetter>
    requires NestedEventRangeGetter<EnablersGetter, std::ranges::range_value_t<EventSet>> &&
             EventRangeGetter<SuccessorGetter, std::ranges::range_value_t<EventSet>> &&
             EventRangeGetter<ConflictGetter, std::ranges::range_value_t<EventSet>>
class StableDFSTracker
{
public:
    using Event = std::ranges::range_value_t<EventSet>;

private:
    struct EventState
    {
        std::vector<std::size_t> missing_per_enabling_set;
        std::size_t conflict_count = 0;
        bool fired = false;

        bool is_enabled() const
        {
            return !fired && conflict_count == 0 &&
                   std::ranges::any_of(missing_per_enabling_set, [](std::size_t missing)
                                       { return missing == 0; });
        }
    };

    struct CounterUndo
    {
        Event target;
        std::size_t enabling_set_index;
    };

    struct UndoFrame
    {
        Event fired_event;
        std::vector<Event> conflicts;
        std::vector<CounterUndo> decremented_counters;
        std::vector<Event> touched_events;
    };

    EnablersGetter get_enablers_;
    SuccessorGetter get_successors_;
    ConflictGetter get_conflicts_;
    std::unordered_map<Event, EventState> states_;
    std::vector<UndoFrame> undo_stack_;

    EventState make_initial_state(const Event &event) const
    {
        EventState state;
        for (const auto &enabling_set : get_enablers_(event))
            state.missing_per_enabling_set.push_back(std::ranges::distance(enabling_set));
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

        if (it->second.fired || it->second.conflict_count != 0)
            return;

        const auto initial_state = make_initial_state(event);
        if (it->second.missing_per_enabling_set == initial_state.missing_per_enabling_set)
            states_.erase(it);
    }

public:
    StableDFSTracker(const EventSet &initial_roots,
                     EnablersGetter get_enablers,
                     SuccessorGetter get_successors,
                     ConflictGetter get_conflicts)
        : get_enablers_(std::move(get_enablers)),
          get_successors_(std::move(get_successors)),
          get_conflicts_(std::move(get_conflicts))
    {
        for (const auto &root : initial_roots)
            ensure_state_registered(root);
    }

    bool is_enabled(const Event &event) const
    {
        const auto it = states_.find(event);
        if (it != states_.end())
            return it->second.is_enabled();

        // A reclaimed event is at its structural baseline. An empty enabling
        // set is satisfied at that baseline, unless a conflict is active (in
        // which case the state would still be registered).
        return std::ranges::any_of(get_enablers_(event), [](const auto &set)
                                   { return std::ranges::empty(set); });
    }

    std::vector<Event> push(const Event &fired_event)
    {
        auto &fired_state = ensure_state_registered(fired_event);
        assert(fired_state.is_enabled());
        fired_state.fired = true;

        UndoFrame undo{fired_event, {}, {}, {fired_event}};

        // Apply conflicts first, matching the stable BFS tracker. This prevents
        // causality updates to events that are already inhibited in this branch.
        for (const auto &conflict : get_conflicts_(fired_event))
        {
            ++ensure_state_registered(conflict).conflict_count;
            undo.conflicts.push_back(conflict);
            undo.touched_events.push_back(conflict);
        }

        std::vector<Event> newly_enabled;
        for (const auto &successor : get_successors_(fired_event))
        {
            auto &state = ensure_state_registered(successor);
            const bool was_enabled = state.is_enabled();
            if (state.fired || state.conflict_count != 0)
                continue;

            std::size_t set_index = 0;
            for (const auto &enabling_set : get_enablers_(successor))
            {
                if (std::ranges::find(enabling_set, fired_event) != std::ranges::end(enabling_set) &&
                    state.missing_per_enabling_set[set_index] != 0)
                {
                    --state.missing_per_enabling_set[set_index];
                    undo.decremented_counters.push_back({successor, set_index});
                    undo.touched_events.push_back(successor);
                }
                ++set_index;
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

        for (auto it = undo.decremented_counters.rbegin(); it != undo.decremented_counters.rend(); ++it)
            ++ensure_state_registered(it->target).missing_per_enabling_set[it->enabling_set_index];

        for (auto it = undo.conflicts.rbegin(); it != undo.conflicts.rend(); ++it)
            --ensure_state_registered(*it).conflict_count;

        ensure_state_registered(fired_event).fired = false;

        for (const auto &event : undo.touched_events)
            garbage_collect_if_idle(event);
    }
};

template <typename EventSet, typename EnablersGetter, typename SuccessorGetter, typename ConflictGetter>
auto getStableDFSTrackerFactory(EnablersGetter get_enablers,
                                SuccessorGetter get_successors,
                                ConflictGetter get_conflicts)
{
    return [get_enablers = std::move(get_enablers),
            get_successors = std::move(get_successors),
            get_conflicts = std::move(get_conflicts)](const EventSet &events)
    {
        return StableDFSTracker<EventSet, EnablersGetter, SuccessorGetter, ConflictGetter>(
            events, get_enablers, get_successors, get_conflicts);
    };
}
