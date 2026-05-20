#include <type_traits>
#include <concepts>
#include <ranges>

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