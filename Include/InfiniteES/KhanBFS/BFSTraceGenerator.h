#pragma once
#include <vector>
#include <queue>
#include <set>
#include <unordered_map>
#include <ranges>
#include <coroutine>
#include <functional>
#include <memory>
#include <algorithm>
#include "EveHorizonConcepts.h"

template<typename Node>
struct TraceGenerator {
    struct promise_type {
        const std::vector<Node>* current_value = nullptr;
        auto get_return_object() { return TraceGenerator{handle_type::from_promise(*this)}; }
        auto initial_suspend() { return std::suspend_always{}; }
        auto final_suspend() noexcept { return std::suspend_always{}; }
        void unhandled_exception() { std::terminate(); }
        auto yield_value(std::vector<Node>& value) {
            current_value = &value;
            return std::suspend_always{};
        }
        void return_void() {}
    };
    using handle_type = std::coroutine_handle<promise_type>;
    handle_type h;
    TraceGenerator(handle_type h) : h(h) {}
    ~TraceGenerator() { if (h) h.destroy(); }
    bool next() { if (!h) return false; h.resume(); return !h.done(); }
    const std::vector<Node>& get_value() { return *h.promise().current_value; }
};

template <typename Node>
struct PathNode {
    Node event;
    std::shared_ptr<PathNode> parent;

    // Flatten to a vector ONLY when yielding to the user
    void flatten(std::vector<Node>& path) const {
        path.clear();
        for (const PathNode* curr = this; curr != nullptr; curr = curr->parent.get()) {
            path.push_back(curr->event);
        }
        std::reverse(path.begin(), path.end());
    }
};

template <typename Node, typename LocalKahnBFSTracker>
struct BFSBranchState {
    std::shared_ptr<PathNode<Node>> path_tail;
    std::vector<Node> frontier;
    LocalKahnBFSTracker local_tracker;
};


/**
 * Fully encapsulated C++20 Hybrid BFS Trace Generator.
 * Safely traverses infinite event structures with zero historical memory lookup penalties.
 */
template <typename Node, typename LocalKahnBFSTracker>
requires KhanBFSTracker<LocalKahnBFSTracker, Node>
TraceGenerator<Node> exploreInfiniteHybridBFS(
    std::vector<Node> initialRoots, 
    LocalKahnBFSTracker initialTracker
) {
    std::queue<BFSBranchState<Node, LocalKahnBFSTracker>> q;
    std::vector<Node> result_path; // Reusable vector for flattening paths when yielding

    // Bootstrapping the initial state entry
    BFSBranchState<Node, LocalKahnBFSTracker> root_state{nullptr, std::move(initialRoots), std::move(initialTracker)};
    for (const auto& r : root_state.frontier) 
        root_state.local_tracker.register_discovered_node(r);
    
    q.push(std::move(root_state));

    while (!q.empty()) {
        auto current = std::move(q.front());
        q.pop();

        // Yield the current prefix path sequence by flattening the shared tree node
        if (current.path_tail)
            current.path_tail->flatten(result_path);
        else
            result_path.clear();

        co_yield result_path;

        for (const auto& next_event : current.frontier) {
            // Fork the branch tracker. O(1) pointer management for the path
            BFSBranchState<Node, LocalKahnBFSTracker> next_state{std::make_shared<PathNode<Node>>(next_event, current.path_tail), {}, current.local_tracker};
            
            // O(1) incremental counter advancement and horizon discovery
            const auto& newly_discovered = next_state.local_tracker.fire_and_propagate(next_event);
            
            // Compile the next localized frontier wavefront
            for (const auto& f : current.frontier) 
                if (f != next_event && next_state.local_tracker.is_enabled(f))
                    next_state.frontier.push_back(f);
            
            next_state.frontier.insert(next_state.frontier.end(), std::begin(newly_discovered), std::end(newly_discovered));

            q.push(std::move(next_state));
        }
    }
}