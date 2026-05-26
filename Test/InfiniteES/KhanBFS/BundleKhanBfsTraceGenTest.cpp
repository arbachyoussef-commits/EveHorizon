#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include "InfiniteES/KhanBFS/BFSTraceGenerator.h"
#include "InfiniteES/KhanBFS/BundleKhanBFSTracker.h"

using namespace testing;

namespace {
    // Define simple mock getters for testing the tracker logic
    auto no_bundles = [](const std::string&) -> std::vector<std::vector<std::string>> { return {}; };
    auto no_succ = [](const std::string&) -> std::vector<std::string> { return {}; };
    auto no_conf = [](const std::string&) -> std::vector<std::string> { return {}; };
}

TEST(BundleKhanBFSTrackerTest, InitialRegistrationAndMissingCounts) {
    // Event 'e' is enabled by bundles {a, b} AND {c}
    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e") return {{"a", "b"}, {"c"}};
        return {};
    };

    BundleKhanBFSTracker<std::string, decltype(get_bundles), decltype(no_succ), decltype(no_conf)> 
        tracker(get_bundles, no_succ, no_conf);

    tracker.register_discovered_node("a");
    tracker.register_discovered_node("b");
    tracker.register_discovered_node("c");
    
    // It should not be enabled initially because neither bundle is satisfied
    EXPECT_FALSE(tracker.is_enabled("e"));
}

TEST(BundleKhanBFSTrackerTest, FireAndPropagateBundleEnabling) {
    // 'e' enabled by bundles {a, b} AND {c}
    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e") return {{"a", "b"}, {"c"}};
        return {};
    };
    // 'a' and 'c' both lead to 'e'
    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a" || n == "c") return {"e"};
        return {};
    };

    BundleKhanBFSTracker<std::string, decltype(get_bundles), decltype(get_succ), decltype(no_conf)> 
        tracker(get_bundles, get_succ, no_conf);

    tracker.register_discovered_node("a");
    tracker.register_discovered_node("b");
    tracker.register_discovered_node("c");

    // Firing 'a' satisfies the first bundle {a, b}, but 'e' still needs the bundle {c} to be satisfied
    auto newly_enabled = tracker.fire_and_propagate("a");
    EXPECT_TRUE(newly_enabled.empty());
    EXPECT_FALSE(tracker.is_enabled("e"));

    // Firing 'c' satisfies the second bundle. Now all bundles are satisfied.
    newly_enabled = tracker.fire_and_propagate("c");
    EXPECT_THAT(newly_enabled, ElementsAre("e"));
    EXPECT_TRUE(tracker.is_enabled("e"));
}

TEST(BundleKhanBFSTrackerTest, ConflictVaporization) {
    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e") return {{"a"}};
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

    BundleKhanBFSTracker<std::string, decltype(get_bundles), decltype(get_succ), decltype(get_conf)> 
        tracker(get_bundles, get_succ, get_conf);

    tracker.register_discovered_node("a");
    tracker.register_discovered_node("b");
    tracker.register_discovered_node("c");

    // Fire 'x' first. This should inhibit 'e'.
    tracker.fire_and_propagate("x");

    // Even if we now fire 'a' (the enabler for 'e'), 'e' should not become enabled
    auto newly_enabled = tracker.fire_and_propagate("a");
    EXPECT_TRUE(newly_enabled.empty());
    EXPECT_FALSE(tracker.is_enabled("e"));
}

TEST(BundleKhanBFSTrackerTest, SelfCleanupAfterFire) {
    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e") return {{"a"}};
        return {};
    };
    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a") return {"e"};
        return {};
    };

    BundleKhanBFSTracker<std::string, decltype(get_bundles), decltype(get_succ), decltype(no_conf)> 
        tracker(get_bundles, get_succ, no_conf);

    tracker.register_discovered_node("a");
    tracker.register_discovered_node("b");
    tracker.register_discovered_node("c");

    tracker.fire_and_propagate("a");
    EXPECT_TRUE(tracker.is_enabled("e"));

    // Firing 'e' itself should remove it from the watch list
    tracker.fire_and_propagate("e");
    EXPECT_FALSE(tracker.is_enabled("e"));
}

TEST(BundleKhanBFSTrackerTest, HandlesEnablingCycles) {
    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e") return {{"a"}};
        if (n == "a") return {{"e"}};
        return {};
    };
    
    auto get_succ = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a") return {"e"};
        if (n == "e") return {"a"};
        return {};
    };

    BundleKhanBFSTracker<std::string, decltype(get_bundles), decltype(get_succ), decltype(no_conf)> 
        tracker(get_bundles, get_succ, no_conf);

    tracker.register_discovered_node("a");
    
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("e"));
}

TEST(BundleKhanBFSTrackerTest, HandlesConflictsWithSuccessors) {
    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e") return {{"a"}};
        return {};
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

    BundleKhanBFSTracker<std::string, decltype(get_bundles), decltype(get_succ), decltype(conflicts)> 
        tracker(get_bundles, get_succ, conflicts);

    tracker.register_discovered_node("a");
    
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("e"));

    auto newly_enabled = tracker.fire_and_propagate("a");
    EXPECT_THAT(newly_enabled, ElementsAre());
    EXPECT_FALSE(tracker.is_enabled("e"));
}

TEST(BundleKhanBfsTraceExplorerTest, ProducesExpectedBundleTraces) {
    std::vector<std::string> events = {"e1","e2","e3","e4"};

    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e4") {
            return {
                {"e1", "e2"},
                {"e3", "e2"}
            };
        }
        return {};
    };

    auto get_successors = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e1") return {"e4"};
        if (n == "e2") return {"e4"};
        if (n == "e3") return {"e4"};
        return {};
    };

    auto get_conflicts = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e1") return {"e2"};
        if (n == "e2") return {"e1", "e3"};
        if (n == "e3") return {"e2"};
        return {};
    };

    BundleKhanBFSTracker<std::string, decltype(get_bundles), decltype(get_successors), decltype(get_conflicts)> tracker(get_bundles, get_successors, get_conflicts);
    auto generator = exploreInfiniteHybridBFS(std::vector<std::string>{"e1", "e2", "e3"}, tracker);

    std::vector<std::vector<std::string>> traces;
    while (generator.next()) {
        traces.push_back(generator.get_value());
    }

    auto expected = std::vector<std::vector<std::string>>{
        {}, {"e1"}, {"e3"}, {"e2"},
        {"e1", "e3"},
        {"e3", "e1"},
        {"e2", "e4"},
        {"e1", "e3", "e4"},
        {"e3", "e1", "e4"},
    };
    ASSERT_THAT(traces, UnorderedElementsAreArray(expected));
}

TEST(BundleKhanBfsTraceExplorerTest, ProducesExpectedExtendedBundleTraces) {
    std::vector<std::string> events = {"e1","e2","e3","e4"};

    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e4") {
            return {
                {"e1", "e2"}
            };
        }
        return {};
    };

    auto get_successors = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e1") return {"e4"};
        if (n == "e2") return {"e4"};
        return {};
    };

    auto get_inhibited_by = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e1") return {"e2"};
        if (n == "e2") return {"e1"};
        if (n == "e3") return {"e1"};
        return {};
    };

    auto expected = std::vector<std::vector<std::string>>{
        {}, {"e1"}, {"e3"}, {"e2"},
        {"e1", "e3"}, //{"e3", "e1"}, not valid because e3 inhibits e1
        {"e2", "e3"},
        {"e3", "e2"},
        {"e2", "e4"},
        {"e1", "e4"},
        {"e1", "e3", "e4"},
        {"e1", "e4", "e3"},
        {"e2", "e3", "e4"},
        {"e2", "e4", "e3"},
        {"e3", "e2", "e4"},
    };

    BundleKhanBFSTracker<std::string, decltype(get_bundles), decltype(get_successors), decltype(get_inhibited_by)> tracker(get_bundles, get_successors, get_inhibited_by);
    auto generator = exploreInfiniteHybridBFS(std::vector<std::string>{"e1", "e2", "e3"}, tracker);

    std::vector<std::vector<std::string>> traces;
    while (generator.next()) {
        traces.push_back(generator.get_value());
    }

    ASSERT_THAT(traces, UnorderedElementsAreArray(expected));
}
