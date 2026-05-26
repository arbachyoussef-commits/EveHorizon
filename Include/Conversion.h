#include <vector>
#include <type_traits>
#include <utility>
#include <ranges>
#include <array>
#include <algorithm>
#include "EveHorizonConcepts.h"

/**
 * Automatically wraps a Prime causality function into a Stable one.
 * Uses the vector initializer-list constructor to wrap the single history.
 */
template <typename Node, typename PredesessorGetter>
requires EventRangeGetter<PredesessorGetter, Node>
auto wrapPrimeAsStable(PredesessorGetter p_pred_func) {
    using RangeT = std::invoke_result_t<PredesessorGetter, const Node&>;

    return [p_pred_func](const Node& n) {
        // Construct an array of size 1 containing the moved predecessors range
        return std::array<RangeT, 1>{ p_pred_func(n) };
    };
}

template <typename Node, typename PredesessorGetter, typename InnerContainerT = std::array<Node, 1>>
struct PrimeToBundleCausalityAdaptor{
    PredesessorGetter p_pred_func;
    PrimeToBundleCausalityAdaptor(PredesessorGetter p_pred_func): p_pred_func(p_pred_func) {}

    auto operator()(const Node& n) const {
        // 1. Get the predecessors (RangeT)
        // 2. Moving it into the pipeline creates an std::ranges::owning_view
        return std::views::all(p_pred_func(n)) | std::views::transform([](const auto& p) {
            return InnerContainerT{ p };
        });
    }
};

/**
 * Returns an owning view that captures the moved predecessors.
 * Each element is lazily wrapped in a single-element vector.
 */
template <typename Node, typename PredesessorGetter, typename InnerContainerT = std::array<Node, 1>>
requires EventRangeGetter<PredesessorGetter, Node>
auto wrapPrimeAsBundle(PredesessorGetter p_pred_func) {
    return PrimeToBundleCausalityAdaptor<Node, PredesessorGetter, InnerContainerT>{ p_pred_func };
}

/**
 * Statelessly converts Bundle causality (AND-of-ORs) to Stable causality (OR-of-ANDs).
 * Automatically deduces Node type from the return of get_bundles_func.
 */
template <typename Node, typename BundleGetter>
requires NestedEventRangeGetter<BundleGetter, Node>
auto wrapBundleAsStable(BundleGetter get_bundles_func) {
    return [get_bundles_func](const Node& n) -> std::vector<std::vector<Node>> {
        auto bundles = get_bundles_func(n);

        // If no bundles, it's a root: enabled by the empty set
        if (std::ranges::empty(bundles)) {
            return {{}};
        }

        // We perform a Cartesian Product to convert AND-of-ORs to OR-of-ANDs
        // Start with one empty history
        std::vector<std::vector<Node>> dnf_histories = {{}};

        for (const auto& bundle : bundles) {
            std::vector<std::vector<Node>> next_step;
            // For every partial history we've built...
            for (const auto& history : dnf_histories) {
                // ...and every alternative in the current bundle...
                for (const auto& choice : bundle) {
                    auto extended_history = history;
                    extended_history.push_back(choice);
                    next_step.push_back(std::move(extended_history));
                }
            }
            dnf_histories = std::move(next_step);
        }

        return dnf_histories;
    };
}


template <typename Node, typename FlowEnablersGetter, typename ConflictsGetter>
requires EventRangeGetter<FlowEnablersGetter, Node> &&
         EventRangeGetter<ConflictsGetter, Node>
auto wrapFlowAsStable(FlowEnablersGetter get_flow_preds, ConflictsGetter get_conflicts) {
    
    return [get_flow_preds, get_conflicts](const Node& e) -> std::vector<std::vector<Node>> {
        
        auto as_vector = [](auto&& range) {
            return std::vector<Node>(std::begin(range), std::end(range));
        };

        auto preds = as_vector(get_flow_preds(e));
        if (std::ranges::empty(preds)) {
            return {{}};    //Rooted event, enabled by the empty set
        }

        // 1. ZERO-ALLOCATION RANGE PIPELINES
        std::ranges::sort(preds);

        auto confs_of_e = as_vector(get_conflicts(e));
        std::ranges::sort(confs_of_e);

        // A lazy, zero-allocation view that filters raw conflicts down to relevant predecessors
        auto get_relevant_rivals_view = [&preds, &get_conflicts](const Node& node) {
            return get_conflicts(node) 
                 | std::views::filter([&preds](const auto& r) {
                       return std::ranges::binary_search(preds, r);
                   });
        };

        std::vector<std::vector<Node>> minimal_histories;
        
        // Single reusable scratchpad vector to eliminate heap thrashing
        std::vector<Node> current_set;
        current_set.reserve(preds.size());

        auto is_in_current_set = [&current_set](const Node& node) {
            return std::ranges::binary_search(current_set, node);
        };

        auto conflicts_with_set = [&get_relevant_rivals_view, &is_in_current_set](const Node& node) {
            return std::ranges::any_of(get_relevant_rivals_view(node), [&is_in_current_set](const auto& rival) {
                return is_in_current_set(rival);
            });
        };

        // Core Backtracking Search. We will pass the explore function/lambda to itself so that we can call it recursively.
        auto explore = [&](size_t idx, auto&& explorer) {
            if (idx == preds.size()) {
                // EARLY PRUNING: Verify this new set isn't a superset of an already found minimal history
                bool is_minimal = std::ranges::none_of(minimal_histories, [&](const auto& minimal_set) {
                    return std::includes(current_set.begin(), current_set.end(), 
                                         minimal_set.begin(), minimal_set.end());
                });
                
                if (is_minimal) {
                    minimal_histories.push_back(current_set);
                }
                return;
            }

            const auto& d = preds[idx];

            // Check if 'd' is already covered (chosen or masked)
            bool is_masked = std::ranges::any_of(get_relevant_rivals_view(d), [&](const auto& rival) {
                return is_in_current_set(rival);
            });

            if (is_masked || is_in_current_set(d)) {
                explorer(idx + 1, explorer); // Skip branching completely
                return;
            }

            // --- BRANCHING FOR UNCOVERED PREDECESSOR ---

            // Choice 1: Try adding 'd' directly (The minimal greedy choice)
            bool has_e_conflict = std::ranges::binary_search(confs_of_e, d);
            if (!has_e_conflict && !conflicts_with_set(d)) {
                current_set.push_back(d);
                explorer(idx + 1, explorer);
                current_set.pop_back(); // Clean backtrack
            }

            // Choice 2: Skip adding 'd', force it to be masked by one of its rivals instead
            for (const auto& rival : get_relevant_rivals_view(d)) {
                if (!is_in_current_set(rival)) {
                    bool rival_e_conflict = std::binary_search(confs_of_e.begin(), confs_of_e.end(), rival);
                    if (!rival_e_conflict && !conflicts_with_set(rival)) {
                        // Insert maintaining sorted integrity
                        auto insert_it = std::lower_bound(current_set.begin(), current_set.end(), rival);
                        current_set.insert(insert_it, rival);
                        
                        explorer(idx + 1, explorer);
                        
                        // Symmetrical erase
                        auto erase_it = std::lower_bound(current_set.begin(), current_set.end(), rival);
                        current_set.erase(erase_it);
                    }
                }
            }
        };

        explore(0, explore);
        return minimal_histories;
    };
}
