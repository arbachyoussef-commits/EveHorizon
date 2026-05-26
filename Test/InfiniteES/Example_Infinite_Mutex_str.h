#pragma once
#include <vector>
#include <algorithm>
#include <functional>
#include <ranges>
#include <tuple>

struct MutexEvent {
    enum class OP { 
        LOCK = 0, 
        CR = 1, //some work
        UNLOCK = 2 };

    OP operation : 2;
    std::size_t process_id : 1; //modeling 2 concurrent processes only
    std::size_t id : 62;    //The iteration number of the lock-unlock pair for this process. For example, LOCK_0_5 is the lock operation of process 0 for the 5th lock-unlock pair it executes.

    bool operator==(const MutexEvent& other) const {
        return id == other.id && operation == other.operation && process_id == other.process_id;
    }
    bool operator<(const MutexEvent& other) const {
        return std::tie(id, operation, process_id) < std::tie(other.id, other.operation, other.process_id);
    }
};

struct InfiniteMutexStr {
    static auto get_flow_predecessors(const MutexEvent& n) {
        if (n.operation == MutexEvent::OP::LOCK) {
            if(n.id == 0) return std::vector<MutexEvent>{};
            else return std::vector<MutexEvent>{{MutexEvent::OP::UNLOCK, 0, n.id-1}, {MutexEvent::OP::UNLOCK, 1, n.id-1}};
        } if (n.operation == MutexEvent::OP::CR) {
            return std::vector<MutexEvent>{ {MutexEvent::OP::LOCK, n.process_id, n.id} };
        } else { // UNLOCK
            return std::vector<MutexEvent>{ {MutexEvent::OP::CR, n.process_id, n.id} };
        }
    }

    static std::vector<std::vector<MutexEvent>> get_stable_enabling_sets(const MutexEvent& n) {
        if (n.operation == MutexEvent::OP::LOCK) {
            if(n.id == 0) return {{}};  //1 enabling set that is empty (rooted)
            else return {{{MutexEvent::OP::UNLOCK, 0, n.id-1}}, {{MutexEvent::OP::UNLOCK, 1, n.id-1}}};//2 enabling sets, each has 1
        } if (n.operation == MutexEvent::OP::CR) {
            return { { {MutexEvent::OP::LOCK, n.process_id, n.id} } };
        } else { // UNLOCK
            return { { {MutexEvent::OP::CR, n.process_id, n.id} } };
        }
    }

    static auto get_successors(const MutexEvent& n) {
        if (n.operation == MutexEvent::OP::LOCK) {
            return std::vector<MutexEvent>{ {MutexEvent::OP::CR, n.process_id, n.id} };
        } else if (n.operation == MutexEvent::OP::CR) {
            return std::vector<MutexEvent>{ {MutexEvent::OP::UNLOCK, n.process_id, n.id} };
        } else { // UNLOCK
            return std::vector<MutexEvent>{ {MutexEvent::OP::LOCK, 0, n.id+1}, {MutexEvent::OP::LOCK, 1, n.id+1}};
        }
    }

    static auto get_conflicts(const MutexEvent& n) {
        if (n.operation == MutexEvent::OP::LOCK)
            return std::vector<MutexEvent>{ {MutexEvent::OP::LOCK, 1 - n.process_id, n.id} }; //Locks of the other process
        if (n.operation == MutexEvent::OP::UNLOCK)
             return std::vector<MutexEvent>{ {MutexEvent::OP::UNLOCK, 1 - n.process_id, n.id} };  //Unlocks of the other process
    }

    static auto get_events() {
        //Theoritically infinite, but will be bound by the max integer of std::size_t in practice.
        return std::views::iota(0) | std::views::transform([](std::size_t i) {
            return std::initializer_list<MutexEvent>{
                {MutexEvent::OP::LOCK, 0, i},
                {MutexEvent::OP::LOCK, 1, i},
                {MutexEvent::OP::UNLOCK, 0, i},
                {MutexEvent::OP::UNLOCK, 1, i},
                {MutexEvent::OP::CR, 0, i},
                {MutexEvent::OP::CR, 1, i}
            };
        }) | std::views::join; 
    }
};