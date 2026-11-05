#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <utility>
#include <ranges>

/**
 * Compile-time optimized Generative DFS Tracker for Prime Event Structures.
 * Uses static polymorphism (templates) instead of runtime type erasure (std::function).
 */
template <
    typename EventSet,
    typename PredGetter,
    typename SuccGetter,
    typename ConflictGetter>
class PrimeDFSTracker
{
public:
    using Node = std::ranges::range_value_t<EventSet>;

private:
    struct NodeState
    {
        std::size_t missing_predecessors = 0;
        std::size_t conflict_count = 0;
        bool fired = false;

        bool is_enabled() const
        {
            return !fired && missing_predecessors == 0 && conflict_count == 0;
        }
    };

    PredGetter get_predecessors_;
    SuccGetter get_successors_;
    ConflictGetter get_conflicts_;

    std::unordered_map<Node, NodeState> live_registry_;

    /**
     * Checks if a downstream successor or conflict node has fully reverted
     * to its structural default baseline. If so, it is safely vaporized.
     */
    void garbage_collect_if_idle(const Node &n)
    {
        auto it = live_registry_.find(n);
        if (it == live_registry_.end())
            return;

        // Reclaim memory if the node returned completely to its unconstrained baseline
        if (it->second.missing_predecessors == std::ranges::size(get_predecessors_(n)) &&
            it->second.conflict_count == 0)
        {
            live_registry_.erase(it);
        }
    }

    /**
     * Propagates metrics to all immediate forward successors.
     * Reuses auto-callbacks to avoid vector allocations during backtrack loops.
     */
    void propagate_causality(const Node &firedNode, int delta, auto callBack)
    {
        for (const auto &succ : get_successors_(firedNode))
        {
            ensure_state_registered(succ);

            auto &state = live_registry_[succ];
            state.missing_predecessors += delta;

            if (delta == -1 && state.is_enabled())
            {
                callBack(succ);
            }
            else if (delta == 1)
            {
                garbage_collect_if_idle(succ);
            }
        }
    }

    void update_conflict_metrics(const Node &firedNode, int delta)
    {
        for (const auto &conf : get_conflicts_(firedNode))
        {
            ensure_state_registered(conf);
            live_registry_[conf].conflict_count += delta;

            if (delta == -1)
            {
                garbage_collect_if_idle(conf);
            }
        }
    }

    void ensure_state_registered(const Node &n)
    {
        if (live_registry_.contains(n))
            return;

        NodeState initial_state{std::ranges::size(get_predecessors_(n)), 0};
        live_registry_.emplace(n, initial_state);
    }

public:
    PrimeDFSTracker(const EventSet &events, PredGetter p, SuccGetter s, ConflictGetter c)
        : get_predecessors_(std::move(p)), get_successors_(std::move(s)), get_conflicts_(std::move(c))
    {
        for (const auto &n : events)
        {
            ensure_state_registered(n);
        }
    }

    bool is_enabled(const Node &n) const
    {
        auto it = live_registry_.find(n);
        if (it != live_registry_.end())
            return it->second.is_enabled();

        // Entries at their structural baseline may be reclaimed. A node with
        // no predecessors is enabled in that unregistered baseline state.
        return std::ranges::empty(get_predecessors_(n));
    }

    std::vector<Node> push(const Node &firedNode)
    {
        std::vector<Node> newly_enabled;
        ensure_state_registered(firedNode);
        live_registry_.at(firedNode).fired = true;

        // Apply conflicts before causal updates. A successor that conflicts
        // with the fired event must never be reported as newly enabled.
        update_conflict_metrics(firedNode, 1);

        // Advance causality downstream
        propagate_causality(firedNode, -1, [&newly_enabled](const Node &n)
                            { newly_enabled.push_back(n); });

        return newly_enabled;
    }

    void pop(const Node &firedNode)
    {
        // Restore a fired marker while undoing this branch, then return the
        // event to its enabled parent state before baseline reclamation.
        live_registry_[firedNode] = NodeState{0, 0, true};

        // Revert structural conflicts
        update_conflict_metrics(firedNode, -1);

        // Revert causality (empty lambda optimizes down to zero execution instructions)
        propagate_causality(firedNode, 1, [](const Node &) {});

        live_registry_[firedNode].fired = false;

        // Reclaim the backtracked node frame if it hits structural baseline
        garbage_collect_if_idle(firedNode);
    }
};

// --- CONVENIENCE DEDUCTION FACTORY ---
template <typename EventSet, typename PredG, typename SuccG, typename ConfG>
auto getPrimeDFSTrackerFactory(PredG p, SuccG s, ConfG c)
{
    return [p = std::move(p), s = std::move(s), c = std::move(c)](const auto &events)
    {
        return PrimeDFSTracker<EventSet, PredG, SuccG, ConfG>(events, p, s, c);
    };
}
