#pragma once

#include <functional>
#include <coroutine>
#include <vector>
template <typename Node>
struct TraceGenerator
{
    struct promise_type
    {
        const std::vector<Node> *current_value = nullptr;
        auto get_return_object() { return TraceGenerator{handle_type::from_promise(*this)}; }
        auto initial_suspend() { return std::suspend_always{}; }
        auto final_suspend() noexcept { return std::suspend_always{}; }
        void unhandled_exception() { std::terminate(); }
        auto yield_value(std::vector<Node> &value)
        {
            current_value = &value;
            return std::suspend_always{};
        }
        void return_void() {}
    };
    
    using handle_type = std::coroutine_handle<promise_type>;
    handle_type h;
    
    TraceGenerator(handle_type h) : h(h) {}
    ~TraceGenerator() { if (h) h.destroy(); }

    //MANDATORY SAFETY LAYER: Prevent double-destruction handle bugs
    TraceGenerator(const TraceGenerator&) = delete;
    TraceGenerator& operator=(const TraceGenerator&) = delete;

    // Support moving the generator out of factory context scopes cleanly
    TraceGenerator(TraceGenerator&& other) noexcept : h(other.h) { other.h = nullptr; }
    TraceGenerator& operator=(TraceGenerator&& other) noexcept {
        if (this != &other) {
            if (h) h.destroy();
            h = other.h;
            other.h = nullptr;
        }
        return *this;
    }

    bool next()
    {
        if (!h) return false;
        h.resume();
        return !h.done();
    }
    
    // Marked const so that the wrapper method itself can be invoked on const generator instances
    const std::vector<Node> &get_value() const { return *h.promise().current_value; }
};
