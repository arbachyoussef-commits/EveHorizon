#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include "InfiniteES/KhanBFS/BFSTraceGenerator.h"
#include "InfiniteES/KhanBFS/FlowKhanBFSTracker.h"

using namespace testing;

namespace {
    // Define simple mock getters for testing the tracker logic
    auto no_bundles = [](const std::string&) -> std::vector<std::vector<std::string>> { return {}; };
    auto no_succ = [](const std::string&) -> std::vector<std::string> { return {}; };
    auto no_conf = [](const std::string&) -> std::vector<std::string> { return {}; };
}

TEST(FlowKhanBFSTrackerTest, InitialRegistrationAndMissingCounts) {
    // Event 'e' is enabled by {a, b}
    auto get_preds = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e") return {"a", "b"};
        return {};
    };

    FlowKhanBFSTracker<std::string, decltype(get_preds), decltype(no_succ), decltype(no_conf)> tracker(get_preds, no_succ, no_conf);

    tracker.register_discovered_node("e");
    
    // It should not be enabled initially because neither bundle is satisfied
    EXPECT_FALSE(tracker.is_enabled("e"));
}

TEST(FlowKhanBFSTrackerTest, FireAndPropagatePredecessors) {
    // 'e' depends on a,c
    auto get_preds = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e") return {"a", "c"};
        return {};
    };

    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a" || n == "c") return {"e"};
        return {};
    };
    
    FlowKhanBFSTracker<std::string, decltype(get_preds), decltype(get_succ), decltype(no_conf)> 
        tracker(get_preds, get_succ, no_conf);

    tracker.register_discovered_node("e");

    auto newly_enabled = tracker.fire_and_propagate("a");
    EXPECT_TRUE(newly_enabled.empty());
    EXPECT_FALSE(tracker.is_enabled("e"));

    newly_enabled = tracker.fire_and_propagate("c");
    EXPECT_THAT(newly_enabled, ElementsAre("e"));
    EXPECT_TRUE(tracker.is_enabled("e"));
}

TEST(FlowKhanBFSTrackerTest, ConflictVaporization) {
    auto get_preds = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e") return {"a"};
        return {};
    };
    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a") return {"e"};
        return {};
    };
    // 'x' is in conflict with 'e'
    auto get_conf = [](const std::string& n) -> std::vector<std::string> {
        if (n == "x") return {"e"};
        return {};
    };

    FlowKhanBFSTracker<std::string, decltype(get_preds), decltype(get_succ), decltype(get_conf)> 
        tracker(get_preds, get_succ, get_conf);

    tracker.register_discovered_node("e");

    // Fire 'x' first. This should inhibit 'e'.
    tracker.fire_and_propagate("x");

    // Even if we now fire 'a' (the enabler for 'e'), 'e' should not become enabled
    auto newly_enabled = tracker.fire_and_propagate("a");
    EXPECT_TRUE(newly_enabled.empty());
    EXPECT_FALSE(tracker.is_enabled("e"));
}

TEST(BundleKhanBFSTrackerTest, SelfCleanupAfterFire) {
     auto get_preds = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e") return {"a"};
        return {};
    };
    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a") return {"e"};
        return {};
    };
    // 'x' is in conflict with 'e'
    auto get_conf = [](const std::string& n) -> std::vector<std::string> {
        if (n == "x") return {"e"};
        return {};
    };

    FlowKhanBFSTracker<std::string, decltype(get_preds), decltype(get_succ), decltype(get_conf)> 
        tracker(get_preds, get_succ, get_conf);
        
    tracker.register_discovered_node("e");
    tracker.fire_and_propagate("a");
    EXPECT_TRUE(tracker.is_enabled("e"));

    // Firing 'e' itself should remove it from the watch list
    tracker.fire_and_propagate("e");
    EXPECT_FALSE(tracker.is_enabled("e"));
}

TEST(FlowKhanBFSTrackerTest, HandlesConflictsAmongPredecessors) {
    // 'e' depends on a,b,c
    auto get_preds = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e") return {"a", "b", "c"};
        return {};
    };

    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a" || n == "b" || n == "c") return {"e"};
        return {};
    };
    
    auto get_conflict = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a"|| n == "c") return {"b"};
        if (n == "b") return {"a", "c"};
        return {};
    };

    FlowKhanBFSTracker<std::string, decltype(get_preds), decltype(get_succ), decltype(get_conflict)> 
        tracker(get_preds, get_succ, get_conflict);

    tracker.register_discovered_node("a");
    tracker.register_discovered_node("b");
    tracker.register_discovered_node("c");


    auto newly_enabled = tracker.fire_and_propagate("a");
    EXPECT_THAT(newly_enabled, ElementsAre());
    EXPECT_TRUE(tracker.is_enabled("c"));
    EXPECT_FALSE(tracker.is_enabled("e"));
    EXPECT_FALSE(tracker.is_enabled("b"));

    newly_enabled = tracker.fire_and_propagate("c");
    EXPECT_THAT(newly_enabled, ElementsAre("e"));
    EXPECT_TRUE(tracker.is_enabled("e"));
    EXPECT_FALSE(tracker.is_enabled("b"));
}

TEST(FlowKhanBFSTrackerTest, HandlesConflictsAmongPredecessors2) {
    // 'e' depends on a,b,c
    auto get_preds = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e") return {"a", "b", "c"};
        return {};
    };

    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a" || n == "b" || n == "c") return {"e"};
        return {};
    };
    
    auto get_conflict = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a"|| n == "c") return {"b"};
        if (n == "b") return {"a", "c"};
        return {};
    };

    FlowKhanBFSTracker<std::string, decltype(get_preds), decltype(get_succ), decltype(get_conflict)> 
        tracker(get_preds, get_succ, get_conflict);

    tracker.register_discovered_node("a");
    tracker.register_discovered_node("b");
    tracker.register_discovered_node("c");

    auto newly_enabled = tracker.fire_and_propagate("b");
    EXPECT_THAT(newly_enabled, ElementsAre("e"));
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("c"));

    newly_enabled = tracker.fire_and_propagate("e");
    EXPECT_TRUE(newly_enabled.empty());
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("c"));
}

TEST(FlowKhanBFSTrackerTest, DistinguishesBetweenConflictsInsideAndOutsidePredecessors) {
    // 'e' depends on a,b,c
    auto get_preds = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e") return {"a", "b", "c"};
        return {};
    };

    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a" || n == "b" || n == "c") return {"e"};
        return {};
    };
    
    auto get_conflict = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a"|| n == "c" || n == "d") return {"b"};
        if (n == "b") return {"a", "c", "d"};
        return {};
    };

    FlowKhanBFSTracker<std::string, decltype(get_preds), decltype(get_succ), decltype(get_conflict)> 
        tracker(get_preds, get_succ, get_conflict);

    tracker.register_discovered_node("a");
    tracker.register_discovered_node("b");
    tracker.register_discovered_node("c");
    tracker.register_discovered_node("d");


    auto newly_enabled = tracker.fire_and_propagate("b");
    EXPECT_THAT(newly_enabled, ElementsAre("e"));
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("c"));
    EXPECT_FALSE(tracker.is_enabled("d"));

    newly_enabled = tracker.fire_and_propagate("e");
    EXPECT_TRUE(newly_enabled.empty());
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("c"));
    EXPECT_FALSE(tracker.is_enabled("d"));
}

TEST(FlowKhanBFSTrackerTest, HandlesSelfConflictingEventsSuccessfully) {
    // a#a, a->d, while b is initially enabled, b->c, b->d and c#c
    auto get_preds = [](const std::string& n) -> std::vector<std::string> {
        if (n == "c") return {"b"};
        if (n == "d") return {"a", "b"};
        return {};
    };

    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "b") return {"c", "d"};
        if (n == "a") return {"d"};
        return {};
    };
    
    auto get_conflict = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a") return {"a"};
        if (n == "c") return {"c"};
        return {};
    };

    FlowKhanBFSTracker<std::string, decltype(get_preds), decltype(get_succ), decltype(get_conflict)> 
        tracker(get_preds, get_succ, get_conflict);

    tracker.register_discovered_node("b");

    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.is_enabled("b"));
    EXPECT_FALSE(tracker.is_enabled("c"));
    EXPECT_FALSE(tracker.is_enabled("d"));

    auto newly_enabled = tracker.fire_and_propagate("b");
    EXPECT_THAT(newly_enabled, ElementsAre());
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("c"));
    EXPECT_FALSE(tracker.is_enabled("d"));
}

TEST(FlowKhanBFSTrackerTest, HandlesSelfDependentEventsSuccessfully) {
    // a->a, a->d, while b is initially enabled, b->c, b->d, c->c
    auto get_preds = [](const std::string& n) -> std::vector<std::string> {
        if (n == "c") return {"b", "c"};
        if (n == "d") return {"a", "b"};
        if (n == "a") return {"a"};
        return {};
    };

    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "b") return {"c", "d"};
        if (n == "a") return {"d", "a"};
        return {};
    };
    
    auto get_conflict = [](const std::string&) -> std::vector<std::string> {
        return {};
    };

    FlowKhanBFSTracker<std::string, decltype(get_preds), decltype(get_succ), decltype(get_conflict)> 
        tracker(get_preds, get_succ, get_conflict);

    tracker.register_discovered_node("b");

    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.is_enabled("b"));
    EXPECT_FALSE(tracker.is_enabled("c"));
    EXPECT_FALSE(tracker.is_enabled("d"));

    auto newly_enabled = tracker.fire_and_propagate("b");
    EXPECT_THAT(newly_enabled, ElementsAre());
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("c"));
    EXPECT_FALSE(tracker.is_enabled("d"));
}

TEST(FlowKhanBFSTrackerTest, HandlesEventConflictingWithItsSuccessorSuccessfully) {
    // a->b, a#b, a->c, a#c, c->c
    auto get_preds = [](const std::string& n) -> std::vector<std::string> {
        if (n == "c") return {"a", "c"};
        if (n == "b") return {"a"};
        return {};
    };

    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a") return {"b", "c"};
        return {};
    };
    
    auto get_conflict = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a") return {"c", "b"};
        if (n == "c" || n == "b") return {"a"};
        return {};
    };

    FlowKhanBFSTracker<std::string, decltype(get_preds), decltype(get_succ), decltype(get_conflict)> 
        tracker(get_preds, get_succ, get_conflict);

    tracker.register_discovered_node("a");

    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("b"));
    EXPECT_FALSE(tracker.is_enabled("c"));

    auto newly_enabled = tracker.fire_and_propagate("a");
    EXPECT_THAT(newly_enabled, ElementsAre());
    EXPECT_FALSE(tracker.is_enabled("b"));
    EXPECT_FALSE(tracker.is_enabled("c"));
}

TEST(FlowKhanBfsTraceExplorerTest, ProducesExpectedFlowTraces) {
    std::vector<std::string> events = {"a","b","c","e"};
    // 'e' depends on a,b,c
    auto get_preds = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e") return {"a", "b", "c"};
        return {};
    };

    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a" || n == "b" || n == "c") return {"e"};
        return {};
    };
    
    auto get_conflict = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a"|| n == "c") return {"b"};
        if (n == "b") return {"a", "c"};
        return {};
    };

    FlowKhanBFSTracker<std::string, decltype(get_preds), decltype(get_succ), decltype(get_conflict)> 
        tracker(get_preds, get_succ, get_conflict);
    auto generator = exploreInfiniteHybridBFS(std::vector<std::string>{"a", "b", "c"}, tracker);

    std::vector<std::vector<std::string>> traces;
    while (generator.next()) {
        traces.push_back(generator.get_value());
    }

    auto expected = std::vector<std::vector<std::string>>{
        {}, {"a"}, {"b"}, {"c"},
        {"a", "c"}, {"c", "a"},
        {"b", "e"},
        {"a", "c", "e"},
        {"c", "a", "e"}
    };
    ASSERT_THAT(traces, UnorderedElementsAreArray(expected));
}