#pragma once

#include <vector>
#include <algorithm>
#include <memory>
#include <ranges>
#include <coroutine>
#include <utility>
#include "EveHorizonConcepts.h"
#include "InfiniteES/Util.h"

/**
 * Trace-limit predicate that never prunes a prefix.
 *
 * Use this explicitly when exhaustive traversal is intended. On an infinite
 * event structure, it may allow the generator to run forever.
 */
struct NeverPruneTrace
{
    template <typename Trace>
    constexpr bool operator()(const Trace &) const noexcept
    {
        return false;
    }
};

template <typename Node>
struct PathNode
{
    Node event;
    std::shared_ptr<PathNode> parent;

    /**
     * Highly optimized, zero-allocation history extractor.
     * Walks backward up the smart pointer tree links and flattens
     * the trace into the caller's pre-allocated memory buffer.
     */
    void flatten_to(std::vector<Node> &dest_buffer) const
    {
        dest_buffer.clear();

        // 1. Walk backward up the pointer chain
        for (const PathNode *curr = this; curr != nullptr; curr = curr->parent.get())
        {
            dest_buffer.push_back(curr->event);
        }

        // 2. Reverse the array in-place to restore correct forward chronological order
        std::ranges::reverse(dest_buffer);
    }
};

template <typename Node>
struct DFSBranchFrame
{
    std::shared_ptr<PathNode<Node>> path_tail;
    size_t frontier_start_idx = 0; // Where this frame's concurrent wave begins
    size_t frontier_end_idx = 0;   // Where this frame's concurrent wave ends
    size_t current_search_idx = 0; // The active candidate index being tested
    size_t added_by_this_step = 0; // How many nodes this specific step appended
    bool has_yielded = false;      // Ensures each prefix is emitted once per DFS frame
};

template <typename EventSet, typename TrackerFactory, typename LimitPredicate>
    requires GenerativeDFSTrackerFactory<TrackerFactory, EventSet> &&
             TraceLimitPredicate<LimitPredicate, std::vector<std::ranges::range_value_t<EventSet>>>
class DFSTraceGenEngine
{
public:
    using Node = std::ranges::range_value_t<EventSet>;

private:
    using Tracker = std::invoke_result_t<TrackerFactory, const EventSet &>;

    std::vector<Node> initial_roots_;
    Tracker tracker_;
    LimitPredicate limit_predicate_;

    template <std::ranges::input_range Range>
        requires std::convertible_to<std::ranges::range_value_t<Range>, Node>
    static std::vector<Node> collect_roots(Range &&roots)
    {
        std::vector<Node> collected;
        for (auto &&root : roots)
            collected.emplace_back(std::forward<decltype(root)>(root));
        return collected;
    }

    // --- PRIVATE HELPERS (SINGLE RESPONSIBILITY) ---

    /**
     * Initializes the first stack frame and registers root events in the tracker.
     */
    void bootstrap_initial_state(std::vector<Node> &master_frontier,
                                 std::vector<DFSBranchFrame<Node>> &stack)
    {
        master_frontier = initial_roots_;
        // for (const auto &r : initial_roots_)
        // {
        //     tracker_.ensure_state_registered(r);
        // }
        // Root frame: Spans across the initial roots indices
        stack.push_back({nullptr, 0, master_frontier.size(), 0, 0});
    }

    /**
     * Clones remaining independent events from the current wave and appends new downstream
     * discoveries to grow the master frontier buffer for the next branch layer.
     */
    void expand_frontier_branch(const DFSBranchFrame<Node> &current_frame,
                                size_t candidate_idx,
                                const std::vector<Node> &newly_discovered,
                                std::vector<Node> &master_frontier,
                                std::vector<DFSBranchFrame<Node>> &stack,
                                const Node &next_node)
    {
        size_t next_start = master_frontier.size();

        // 1. Inherit concurrent ready events from the current segment
        for (size_t i = current_frame.frontier_start_idx; i < current_frame.frontier_end_idx; ++i)
        {
            if (i != candidate_idx && tracker_.is_enabled(master_frontier[i]))
            {
                master_frontier.push_back(master_frontier[i]);
            }
        }

        // 2. Insert newly unlocked events
        master_frontier.insert(master_frontier.end(), newly_discovered.begin(), newly_discovered.end());

        size_t next_end = master_frontier.size();
        size_t total_added = next_end - next_start;

        // 3. Commit the new frame onto the stack
        auto next_tail = std::make_shared<PathNode<Node>>(next_node, current_frame.path_tail);
        stack.push_back({std::move(next_tail), next_start, next_end, next_start, total_added});
    }

    /**
     * Symmetrically backs out of a stack frame by rolling back tracker state
     * and truncating appended elements from the contiguous master vector buffer.
     */
    void backtrack_current_frame(std::vector<Node> &master_frontier,
                                 std::vector<DFSBranchFrame<Node>> &stack)
    {
        auto current_frame = stack.back();

        // Every non-root frame represents a fired event, including pruned frames.
        // Always undo it before exploring a sibling branch.
        if (current_frame.path_tail)
        {
            tracker_.pop(current_frame.path_tail->event);
        }

        // Instantly clear the master frontier allocation buffer for this level
        master_frontier.resize(master_frontier.size() - current_frame.added_by_this_step);
        stack.pop_back();
    }

public:
    template <std::ranges::input_range Range>
        requires std::convertible_to<std::ranges::range_value_t<Range>, Node>
    DFSTraceGenEngine(Range &&initialRoots, TrackerFactory trackerFactory, LimitPredicate limitPredicate)
        : initial_roots_(collect_roots(std::forward<Range>(initialRoots))),
          tracker_(trackerFactory(initial_roots_)),
          limit_predicate_(std::move(limitPredicate))
    {
    }

    /**
     * Clean, Parameterless Lvalue Execution Loop
     */
    TraceGenerator<Node> operator()() &
    {
        std::vector<Node> shared_yield_buffer;
        shared_yield_buffer.reserve(256);

        std::vector<DFSBranchFrame<Node>> execution_stack;
        std::vector<Node> master_frontier_buffer;
        master_frontier_buffer.reserve(1024);

        bootstrap_initial_state(master_frontier_buffer, execution_stack);

        while (!execution_stack.empty())
        {
            auto &current_frame = execution_stack.back();

            if (!current_frame.has_yielded)
            {
                // Flatten structural tree pointers into our reusable trace scratchpad.
                if (current_frame.path_tail)
                    current_frame.path_tail->flatten_to(shared_yield_buffer);
                else
                    shared_yield_buffer.clear();

                // A matching prefix is pruned before it is yielded or expanded.
                if (limit_predicate_(shared_yield_buffer))
                {
                    backtrack_current_frame(master_frontier_buffer, execution_stack);
                    continue;
                }

                current_frame.has_yielded = true;
                co_yield shared_yield_buffer;
            }

            bool branched = false;
            // Move horizontally through candidate events in the virtual frame segment
            while (current_frame.current_search_idx < current_frame.frontier_end_idx)
            {
                size_t candidate_idx = current_frame.current_search_idx++;
                Node next_node = master_frontier_buffer[candidate_idx];

                if (tracker_.is_enabled(next_node))
                {
                    auto newly_discovered = tracker_.push(next_node);

                    expand_frontier_branch(current_frame, candidate_idx, newly_discovered,
                                           master_frontier_buffer, execution_stack, next_node);

                    branched = true;
                    break;
                }
            }

            // End of crossroads reached: pop frame and reverse metrics
            if (!branched)
                backtrack_current_frame(master_frontier_buffer, execution_stack);
        }
    }

    TraceGenerator<Node> operator()() && = delete;
};

template <typename EventSet, typename TrackerFactory, typename LimitPredicate>
    requires GenerativeDFSTrackerFactory<TrackerFactory, EventSet> &&
             TraceLimitPredicate<LimitPredicate, std::vector<std::ranges::range_value_t<EventSet>>>
auto getDFSTraceGenFactory(TrackerFactory trackerFactory, LimitPredicate limitPredicate)
{
    return [trackerFactory, limitPredicate](auto &&initialRoots)
    {
        return DFSTraceGenEngine<EventSet, TrackerFactory, LimitPredicate>(
            std::forward<decltype(initialRoots)>(initialRoots),
            trackerFactory,
            limitPredicate);
    };
}
