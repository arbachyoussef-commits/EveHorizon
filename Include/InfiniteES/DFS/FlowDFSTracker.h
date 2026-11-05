#pragma once

#include <cassert>
#include <cstddef>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include "EveHorizonConcepts.h"

/**
 * Dynamic flow-event-structure tracker for the infinite DFS explorer.
 *
 * A predecessor requirement is satisfied when that predecessor fires or when
 * a fired event conflicts with it. A fired conflict also inhibits the event
 * itself. push/pop record and reverse these changes so sibling DFS branches
 * start from the correct state.
 */
template <typename EventSet, typename PredecessorGetter, typename SuccessorGetter, typename ConflictGetter>
    requires EventRangeGetter<PredecessorGetter, std::ranges::range_value_t<EventSet>> &&
             EventRangeGetter<SuccessorGetter, std::ranges::range_value_t<EventSet>> &&
             EventRangeGetter<ConflictGetter, std::ranges::range_value_t<EventSet>>
class FlowDFSTracker
{
public:
    using Event = std::ranges::range_value_t<EventSet>;

private:
    enum class PredecessorStatus
    {
        Pending,
        Fired,
        Blocked
    };

    struct EventState
    {
        std::size_t unsatisfied_predecessors = 0;
        std::size_t conflict_count = 0;
        std::vector<PredecessorStatus> predecessor_status;
        bool fired = false;

        bool is_enabled() const
        {
            return !fired && conflict_count == 0 && unsatisfied_predecessors == 0;
        }
    };

    struct PredecessorUndo
    {
        Event target;
        std::size_t predecessor_index;
    };

    struct UndoFrame
    {
        Event fired_event;
        std::vector<Event> conflicts;
        std::vector<PredecessorUndo> satisfied_slots;
        std::vector<Event> touched_events;
    };

    PredecessorGetter get_predecessors_;
    SuccessorGetter get_successors_;
    ConflictGetter get_conflicts_;
    std::unordered_map<Event, EventState> states_;
    std::vector<UndoFrame> undo_stack_;

    EventState make_initial_state(const Event &event) const
    {
        EventState state;
        for (const auto &predecessor : get_predecessors_(event))
        {
            (void)predecessor;
            ++state.unsatisfied_predecessors;
            state.predecessor_status.push_back(PredecessorStatus::Pending);
        }
        const auto conflicts = get_conflicts_(event);
        if (std::ranges::find(conflicts, event) != std::ranges::end(conflicts))
            state.conflict_count = 1;
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
        if (state.fired || state.conflict_count != 0)
            return;

        const auto initial_state = make_initial_state(event);
        if (state.unsatisfied_predecessors == initial_state.unsatisfied_predecessors &&
            state.predecessor_status == initial_state.predecessor_status)
            states_.erase(it);
    }

public:
    FlowDFSTracker(const EventSet &initial_roots,
                   PredecessorGetter get_predecessors,
                   SuccessorGetter get_successors,
                   ConflictGetter get_conflicts)
        : get_predecessors_(std::move(get_predecessors)),
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

        // Reclaimed baseline roots are enabled unless they conflict with
        // themselves, matching the registered state initialized above.
        const auto conflicts = get_conflicts_(event);
        return std::ranges::empty(get_predecessors_(event)) &&
               std::ranges::find(conflicts, event) == std::ranges::end(conflicts);
    }

    std::vector<Event> push(const Event &fired_event)
    {
        auto &fired_state = ensure_state_registered(fired_event);
        assert(fired_state.is_enabled());
        fired_state.fired = true;

        UndoFrame undo{fired_event, {}, {}, {fired_event}};
        std::vector<Event> fired_conflicts;
        for (const auto &conflict : get_conflicts_(fired_event))
        {
            fired_conflicts.push_back(conflict);
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

            std::size_t predecessor_index = 0;
            for (const auto &predecessor : get_predecessors_(successor))
            {
                auto &status = state.predecessor_status[predecessor_index];
                if (status == PredecessorStatus::Pending)
                {
                    if (predecessor == fired_event)
                    {
                        status = PredecessorStatus::Fired;
                    }
                    else if (std::ranges::find(fired_conflicts, predecessor) != fired_conflicts.end())
                    {
                        status = PredecessorStatus::Blocked;
                    }

                    if (status != PredecessorStatus::Pending)
                    {
                        --state.unsatisfied_predecessors;
                        undo.satisfied_slots.push_back({successor, predecessor_index});
                        undo.touched_events.push_back(successor);
                    }
                }
                ++predecessor_index;
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

        for (auto it = undo.satisfied_slots.rbegin(); it != undo.satisfied_slots.rend(); ++it)
        {
            auto &state = ensure_state_registered(it->target);
            state.predecessor_status[it->predecessor_index] = PredecessorStatus::Pending;
            ++state.unsatisfied_predecessors;
        }

        for (auto it = undo.conflicts.rbegin(); it != undo.conflicts.rend(); ++it)
            --ensure_state_registered(*it).conflict_count;

        ensure_state_registered(fired_event).fired = false;

        for (const auto &event : undo.touched_events)
            garbage_collect_if_idle(event);
    }
};

template <typename EventSet, typename PredecessorGetter, typename SuccessorGetter, typename ConflictGetter>
auto getFlowDFSTrackerFactory(PredecessorGetter get_predecessors,
                              SuccessorGetter get_successors,
                              ConflictGetter get_conflicts)
{
    return [get_predecessors = std::move(get_predecessors),
            get_successors = std::move(get_successors),
            get_conflicts = std::move(get_conflicts)](const EventSet &events)
    {
        return FlowDFSTracker<EventSet, PredecessorGetter, SuccessorGetter, ConflictGetter>(
            events, get_predecessors, get_successors, get_conflicts);
    };
}
