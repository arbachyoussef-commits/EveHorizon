#pragma once

#include <concepts>
#include <ranges>
#include <type_traits>
#include <set>

/**
 * C++20 Concept defining the mandatory interface for a Kahn-style tracker used for the trace generator.
 */
template <typename Tracker, typename Event>
concept KahnTracker = requires(Tracker t, const Tracker ct, const Event& e) {
    // 1. Enforce push and pop state-mutation capabilities
    { t.push(e) } -> std::same_as<void>;
    { t.pop(e) }  -> std::same_as<void>;

    // 2. Enforce the presence of a stateless enablement check
    { ct.is_enabled(e) } -> std::convertible_to<bool>;
};

/**
 * C++20 Concept defining the mandatory interface for a Kahn-style tracker factory.
 * 
 * @tparam Factory The callable factory type.
 * @tparam EventRange A valid forward range containing your events/nodes.
 */
template <typename Factory, typename EventRange>
concept KhanTrackerFactory = KahnTracker<std::invoke_result_t<Factory, EventRange>, std::ranges::range_value_t<EventRange>>;

// ============================================================================
// RANGE POLICY TRAITS (Standard-Compliant Enforcers)
// ============================================================================
template <typename R> struct RequiresRange        { static constexpr bool value = std::ranges::range<R>; };
template <typename R> struct RequiresForwardRange { static constexpr bool value = std::ranges::forward_range<R>; };
template <typename R> struct RequiresBidirectional{ static constexpr bool value = std::ranges::bidirectional_range<R>; };
template <typename R> struct RequiresRandomAccess { static constexpr bool value = std::ranges::random_access_range<R>; };

/**
 * 1. Fully Parameterized Single-Layer Getter Concept
 * Enforces that the getter returns a range type that satisfies a specific constraint.
 * 
 * @tparam Getter The callable function/lambda.
 * @tparam Node The underlying event node type.
 * @tparam RangeConstraint A C++20 concept (defaults to standard range).
 */
template <
    typename Getter, 
    typename Node, 
    template<typename> class ConstraintTrait = RequiresRange
>
concept EventRangeGetter = requires(const Getter g, const Node& n) {
    // 1. Enforce the user-specified range capabilities (e.g., standard range, forward_range, etc.)
    requires ConstraintTrait<std::invoke_result_t<Getter, const Node&>>::value;
    
    // 2. Enforce type consistency: the inner value type must be our Node
    requires std::same_as<std::ranges::range_value_t<std::invoke_result_t<Getter, const Node&>>, Node>;
};

/**
 * 2. Fully Parameterized Nested Getter Concept
 * Allows you to specify different constraints for the outer container and inner sets.
 * 
 * @tparam Getter The callable function/lambda.
 * @tparam Node The underlying event node type.
 * @tparam OuterConstraint Concept for the outer structure (defaults to standard range).
 * @tparam InnerConstraint Concept for the inner structures (defaults to standard range).
 */
template <
    typename Getter, 
    typename Node, 
    template<typename> class OuterTrait = RequiresRange,
    template<typename> class InnerTrait = RequiresRange
>
concept NestedEventRangeGetter = requires(const Getter g, const Node& n) {

    // 1. Enforce outer layer capabilities
    requires OuterTrait<std::invoke_result_t<Getter, const Node&>>::value;

    // 2. Enforce inner layer capabilities
    requires InnerTrait<std::ranges::range_value_t<std::invoke_result_t<Getter, const Node&>>>::value;

    // 3. Enforce type consistency down to the atomic element
    requires std::same_as<std::ranges::range_value_t<std::ranges::range_value_t<std::invoke_result_t<Getter, const Node&>>>, Node>;
};


/**
 * C++20 Concept defining the mandatory interface for a stateless BFS-style tracker.
 * 
 * @tparam Tracker The concrete tracker implementation type.
 * @tparam Node The underlying event node type.
 * @tparam RangeConstraint The concept applied to the returned collections (defaults to basic range).
 */
template <
    typename Tracker, 
    typename Node, 
    template<typename> class ConstraintTrait = RequiresRange
>
concept BFSTracker = requires(const Tracker t, const Node& n, const std::set<Node>& fired) {
    
    // 1. Enforce type traits / capabilities structurally
    requires ConstraintTrait<std::invoke_result_t<decltype(&Tracker::get_newly_enabled), const Tracker*, const Node&, const std::set<Node>&>>::value;
    requires ConstraintTrait<std::invoke_result_t<decltype(&Tracker::get_inhibited_by), const Tracker*, const Node&>>::value;

    // 2. Enforce atomic element consistency (value_type must be Node)
    requires std::same_as<std::ranges::range_value_t<std::invoke_result_t<decltype(&Tracker::get_newly_enabled), const Tracker*, const Node&, const std::set<Node>&>>, Node>;
    requires std::same_as<std::ranges::range_value_t<std::invoke_result_t<decltype(&Tracker::get_inhibited_by), const Tracker*, const Node&>>, Node>;
};

/**
 * C++20 Concept defining a Hybrid Kahn-BFS Tracker.
 * Enforces the incremental local-state mutations and branch copy-constructibility.
 * 
 * @tparam Tracker The concrete tracker implementation type.
 * @tparam Node The underlying event node type.
 * @tparam RangeTrait A struct trait defining range expectations for discovery (defaults to standard range).
 */
template <
    typename Tracker, 
    typename Node
>
concept KhanBFSTracker = 
    std::copy_constructible<Tracker> && // Enforces that branches can safely copy the tracker state
    requires(Tracker t, const Tracker ct, const Node& n) {
        
        // 1. Enforce node discovery registration signature
        // Used to register initially enabled events
        t.register_discovered_node(n);

        // 2. Enforce the presence of the pure read-only enablement check
        // Returns true iff n is tracked and is enabled
        { ct.is_enabled(n) } -> std::convertible_to<bool>;

        // 3. Enforce the execution and return type capabilities of the core propagation step
        // It is called on one of the enabled events which were checked through is_enabled(n), to denote the firing of n
        // It must return a range of newly enabled functions, which are direct successors of n, or enabled only after n is fired
        //t.fire_and_propagate(n);
        
        requires std::same_as<std::ranges::range_value_t<std::invoke_result_t<decltype(&Tracker::fire_and_propagate), Tracker*, const Node&>>, Node>;
    };