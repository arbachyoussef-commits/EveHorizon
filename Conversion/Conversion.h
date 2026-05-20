#include <vector>
#include <type_traits>
#include <utility>
#include <ranges>
#include <array>
#include <algorithm>

/**
 * Automatically wraps a Prime causality function into a Stable one.
 * Uses the vector initializer-list constructor to wrap the single history.
 */
template <typename Node>
auto wrapPrimeAsStable(auto p_pred_func) {
    using RangeT = std::invoke_result_t<decltype(p_pred_func), const Node&>;

    return [p_pred_func](const Node& n) {
        // Construct a vector of size 1 containing the moved predecessors range
        return std::vector<RangeT>{ p_pred_func(n) };
    };
}

/**
 * Returns an owning view that captures the moved predecessors.
 * Each element is lazily wrapped in a single-element vector.
 */
template <typename Node, typename InnerContainerT = std::array<Node, 1>>
auto wrapPrimeAsBundle(auto p_pred_func) {
    return [p_pred_func](const Node& n) {
        // 1. Get the predecessors (RangeT)
        // 2. Moving it into the pipeline creates an std::ranges::owning_view
        return std::views::all(p_pred_func(n)) 
             | std::views::transform([](const auto& p) {
                   return InnerContainerT{ p };
               });
    };
}

/**
 * Statelessly converts Bundle causality (AND-of-ORs) to Stable causality (OR-of-ANDs).
 * Automatically deduces Node type from the return of get_bundles_func.
 */
template <typename Node>
auto wrapBundleAsStable(auto get_bundles_func) {
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

