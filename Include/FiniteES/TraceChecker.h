#pragma once

#include <vector>
#include <algorithm>
#include <optional>
#include "KhanConcepts.h"

template <class Event>
struct TraceCheckResult {
    std::optional<Event> _first_invalid_event; // Optional: the first event that caused the trace to be invalid

    TraceCheckResult() : _first_invalid_event(std::nullopt) {} // Constructor for valid trace
    TraceCheckResult(Event invalid_event) : _first_invalid_event(std::forward<Event>(invalid_event)) {}

    constexpr explicit operator bool() const noexcept{
        return !_first_invalid_event.has_value(); // Valid if there is no invalid event
    }
    Event first_invalid_event() const {
        return _first_invalid_event.value();
    }
};

/**
 * Validates if a sequence of events is a valid trace for any Event Structure.
 * The event structure is represented through its Tracker, which is instatiated through the facotry passed as argument.
 * The function simulates the firing of events in the given sequence, checking at each step if the next event is enabled according to the tracker's logic. 
 * In other words, every event of the sequence must be enabled after pushing the previous events in the sequence. If such an event is found the function returns false with the first invalid event. If the function successfully simulates the firing of all events in the sequence without finding any invalid event, it returns true.
 */
template <typename EventSeq, typename TrackerFactory>
requires KhanTrackerFactory<TrackerFactory, EventSeq>
auto isTrace(const EventSeq& sequence, TrackerFactory&& tracker_factory) {
    using Event = std::ranges::range_value_t<EventSeq>;

    auto tracker = tracker_factory(sequence);
    for (auto&& event : sequence) {
        if (!tracker.is_enabled(event))
            return TraceCheckResult<Event>{event}; // Invalid trace: event is not enabled
        tracker.push(event); // Update tracker state to reflect this event firing
    }

    return TraceCheckResult<Event>{}; // Valid trace: all events were enabled in sequence
}
