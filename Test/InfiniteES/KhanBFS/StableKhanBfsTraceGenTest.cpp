
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include "InfiniteES/KhanBFS/BFSTraceGenerator.h"
#include "InfiniteES/KhanBFS/StableKhanBFSTracker.h"

using testing::UnorderedElementsAreArray;

namespace {
// Mock Event structure logic for testing
// Event 0: Rooted event
// Event 1: Rooted event
// Event 2: Enabled by {1}
// Event 3: Enabled by {1} OR {4} (Stable causality OR)
// Event 4: Rooted event
// Event 5: Enabled by {1, 4} (Stable causality AND)
// Event 6: In symmetric conflict with 1 (1 # 6)

auto get_enablers = [](int e) -> std::vector<std::vector<int>> {
    if (e == 2) return {{1}};
    if (e == 3) return {{1}, {4}};
    if (e == 5) return {{1, 4}};
    if (e == 6) return {{0}}; // Assume 0 is some root
    return {};  //Means 0, 1 and 4 are initially enabled (rooted)
};

auto get_successors = [](int e) -> std::vector<int> {
    if (e == 1) return {2, 3, 5};
    if (e == 4) return {3, 5};
    if (e == 0) return {6};
    return {};
};

auto get_conflicts = [](int e) -> std::vector<int> {
    if (e == 1) return {6};
    if (e == 6) return {1};
    return {};
};

using TestTracker = StableKhanBFSTracker<int, decltype(get_enablers), decltype(get_successors), decltype(get_conflicts)>;

} // namespace

TEST(StableKhanBFSTrackerTest, RegisterAndInitialEnableCheck) {
    TestTracker tracker(get_enablers, get_successors, get_conflicts);
    
    tracker.register_discovered_node(0);
    tracker.register_discovered_node(1);
    tracker.register_discovered_node(4);
    // Should not be enabled yet as event 1 has not fired
    EXPECT_FALSE(tracker.is_enabled(2));
}

TEST(StableKhanBFSTrackerTest, FireAndPropagateSimpleEnabling) {
    TestTracker tracker(get_enablers, get_successors, get_conflicts);
    
    tracker.register_discovered_node(0);
    tracker.register_discovered_node(1);
    tracker.register_discovered_node(4);

    auto newly_enabled = tracker.fire_and_propagate(1);
    
    // Event 2 should be in the returned vector and report as enabled
    EXPECT_TRUE(std::ranges::find(newly_enabled, 2)!=std::end(newly_enabled));
    EXPECT_TRUE(tracker.is_enabled(2));
}

TEST(StableKhanBFSTrackerTest, StableCausalityOrLogic) {
    TestTracker tracker(get_enablers, get_successors, get_conflicts);
    
    tracker.register_discovered_node(0);
    tracker.register_discovered_node(1);
    tracker.register_discovered_node(4);

    // Fire 1 only
    auto newly_enabled = tracker.fire_and_propagate(1);
    
    // Event 3 should be enabled because one of its alternative histories (1) is satisfied
    EXPECT_TRUE(std::ranges::find(newly_enabled, 3) != std::end(newly_enabled));
    EXPECT_TRUE(tracker.is_enabled(3));
}

TEST(StableKhanBFSTrackerTest, StableCausalityAndLogic) {
    TestTracker tracker(get_enablers, get_successors, get_conflicts);
    
    tracker.register_discovered_node(0);
    tracker.register_discovered_node(1);
    tracker.register_discovered_node(4);

    // Fire 1
    tracker.fire_and_propagate(1);
    EXPECT_FALSE(tracker.is_enabled(5)); // Still needs 4

    // Fire 4
    auto newly_enabled = tracker.fire_and_propagate(4);
    EXPECT_TRUE(std::ranges::find(newly_enabled, 5) != std::end(newly_enabled));
    EXPECT_TRUE(tracker.is_enabled(5));
}

TEST(StableKhanBFSTrackerTest, SymmetricConflictVaporization) {
    TestTracker tracker(get_enablers, get_successors, get_conflicts);
    
    tracker.register_discovered_node(0);
    tracker.register_discovered_node(1);
    tracker.register_discovered_node(4);

    // Fire 1 -> This should vaporize 6 because they are in conflict
    tracker.fire_and_propagate(1);
    
    // 6 should no longer be in the watch list/enabled
    EXPECT_FALSE(tracker.is_enabled(6));
}

TEST(StableKhanBFSTrackerTest, FireErasesSelf) {
    TestTracker tracker(get_enablers, get_successors, get_conflicts);
   
    tracker.register_discovered_node(0);
    tracker.register_discovered_node(1);
    tracker.register_discovered_node(4);
    
    // Fire 1 to enable 2
    tracker.fire_and_propagate(1);
    EXPECT_TRUE(tracker.is_enabled(2));

    // Now fire 2
    tracker.fire_and_propagate(2);
    
    // 2 should be erased from active watch list after firing
    EXPECT_FALSE(tracker.is_enabled(2));
}

TEST(StableKhanBFSTrackerTest, HandlesEnablingCycles) {
    auto get_en_sets = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e") return {{"a"}};
        if (n == "a") return {{"e"}};
        return {};
    };
    
    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a") return {"e"};
        if (n == "e") return {"a"};
        return {};
    };

    auto conflicts = [](const std::string&) -> std::vector<std::string> {
        return {};
    };


    StableKhanBFSTracker<std::string, decltype(get_en_sets), decltype(get_succ), decltype(conflicts)> 
        tracker(get_en_sets, get_succ, conflicts);

    tracker.register_discovered_node("a");
    
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("e"));
}

TEST(StableKhanBFSTrackerTest, HandlesConflictsWithSuccessors) {
    auto get_en_sets = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e") return {{"a"}};
        return {{}};    // a is rooted
    };
    
    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a") return {"e"};
        return {};
    };

    auto conflicts = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a") return {"e"};
        if (n == "e") return {"a"};
        return {};
    };

    StableKhanBFSTracker<std::string, decltype(get_en_sets), decltype(get_succ), decltype(conflicts)> 
        tracker(get_en_sets, get_succ, conflicts);

    tracker.register_discovered_node("a");
    
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("e"));

    auto newly_enabled = tracker.fire_and_propagate("a");
    EXPECT_THAT(newly_enabled, testing::ElementsAre());
    EXPECT_FALSE(tracker.is_enabled("e"));
}

TEST(StableKhanBfsTraceExplorerTest, ProducesExpectedStableTraces) {
    // Nodes: {1, 2, 3, 4}
    // Stable Logic: 
    // Event 3 is enabled by HISTORY {1} OR HISTORY {2}.
    // Event 4 is a root but conflicts with 3.

    auto get_stable_histories = [](int n) -> std::vector<std::vector<int>> {
        if (n == 1 || n == 2 || n == 4) return {{}}; // Roots
        if (n == 3) return {{1}, {2}};               // Disjunctive enabling
        return {};
    };

    auto get_successors = [](int n) -> std::vector<int> {
        if (n == 1) return {3};
        if (n == 2) return {3};
        return {};
    };

    auto get_stable_conflicts = [](int n) -> std::vector<int> {
        if (n == 3) return {4};
        if (n == 4) return {3};
        if (n == 1) return {2}; //stability
        if (n == 2) return {1};
        return {};
    };

    StableKhanBFSTracker<int, decltype(get_stable_histories), decltype(get_successors), decltype(get_stable_conflicts)> tracker(get_stable_histories, get_successors, get_stable_conflicts);
    auto generator = exploreInfiniteHybridBFS(std::vector<int>{1, 2, 4}, tracker);

    std::vector<std::vector<int>> traces;
    while (generator.next()) {
        const auto& result = generator.get_value();
        traces.push_back(result);
    }

    auto expected = std::vector<std::vector<int>>{
        {}, {1}, {2}, {4},
        {1, 3}, {2, 3},
        {1, 4}, {4, 1},
        {2, 4}, {4, 2}
    };
    ASSERT_THAT(traces, UnorderedElementsAreArray(expected));
}
