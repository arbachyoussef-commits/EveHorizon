#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include "FiniteES/DFSTraceGenerator.h"
#include "FiniteES/BundleKhanDFSTracker.h"

using testing::UnorderedElementsAreArray;

TEST(BundleTrackerTest, BundleEnablementAndConflictBehavior) {
    std::vector<std::string> events = {"R1_a", "R1_b", "R2_a", "R2_b", "A"};

    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "A") {
            return {
                {"R1_a", "R1_b"},
                {"R2_a", "R2_b"}
            };
        }
        return {};
    };

    auto get_conflicts = [](const std::string& n) -> std::vector<std::string> {
        if (n == "R1_a") return {"R1_b", "R2_a"};
        if (n == "R1_b") return {"R1_a"};
        if (n == "R2_a") return {"R2_b", "R1_a"};
        if (n == "R2_b") return {"R2_a"};
        return {};
    };

    BundleKhanDFSTracker tracker(events, get_bundles, get_conflicts);

    EXPECT_TRUE(tracker.is_enabled("R1_a"));
    EXPECT_TRUE(tracker.is_enabled("R1_b"));
    EXPECT_TRUE(tracker.is_enabled("R2_a"));
    EXPECT_TRUE(tracker.is_enabled("R2_b"));
    EXPECT_FALSE(tracker.is_enabled("A"));

    tracker.push("R1_a");
    EXPECT_FALSE(tracker.is_enabled("R1_b"));
    EXPECT_FALSE(tracker.is_enabled("R2_a"));
    EXPECT_TRUE(tracker.is_enabled("R2_b"));
    EXPECT_FALSE(tracker.is_enabled("A"));

    tracker.push("R2_b");
    EXPECT_TRUE(tracker.is_enabled("A"));
    EXPECT_FALSE(tracker.is_enabled("R2_a"));
    EXPECT_FALSE(tracker.is_enabled("R1_b"));

    tracker.pop("R2_b");
    EXPECT_FALSE(tracker.is_enabled("A"));
    EXPECT_TRUE(tracker.is_enabled("R2_b"));
}

TEST(BundleTrackerTest, HandlesEnablingCycles) {
    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e") return {{"a"}};
        if (n == "a") return {{"e"}};
        return {};
    };
    
    auto conflicts = [](const std::string&) -> std::vector<std::string> {
        return {};
    };

    BundleKhanDFSTracker tracker(std::array<std::string, 2>{"a", "e"}, get_bundles, conflicts);
    
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("e"));
}

TEST(BundleTrackerTest, HandlesConflictsWithSuccessors) {
    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e") return {{"a"}};
        return {};
    };
    
    auto conflicts = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a") return {"e"};
        if (n == "e") return {"a"};
        return {};
    };

    BundleKhanDFSTracker tracker(std::array<std::string, 2>{"a", "e"}, get_bundles, conflicts);
    
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("e"));

    tracker.push("a");
    EXPECT_FALSE(tracker.is_enabled("e"));
}

TEST(BundleTrackerTest, ExtendedBundleEnablementAndConflictBehavior) {
    std::vector<std::string> events = {"a","b","c","d","e"};

    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e") {
            return {
                {"a", "b"},
                {"b", "c"}
            };
        }
        return {};
    };

    auto get_inhibited_by = [](const std::string& n) -> std::vector<std::string> {
        if (n == "a" || n == "c") return {"b"};
        if (n == "b") return {"a", "c", "d"};
        return {};
    };

    BundleKhanDFSTracker tracker(events, get_bundles, get_inhibited_by);

    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.is_enabled("b"));
    EXPECT_TRUE(tracker.is_enabled("c"));
    EXPECT_TRUE(tracker.is_enabled("d"));
    EXPECT_FALSE(tracker.is_enabled("e"));

    tracker.push("b");
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("c"));
    EXPECT_FALSE(tracker.is_enabled("d"));
    EXPECT_TRUE(tracker.is_enabled("e"));

    tracker.pop("b");
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.is_enabled("b"));
    EXPECT_TRUE(tracker.is_enabled("c"));
    EXPECT_TRUE(tracker.is_enabled("d"));
    EXPECT_FALSE(tracker.is_enabled("e"));

    tracker.push("d");
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.is_enabled("b"));
    EXPECT_TRUE(tracker.is_enabled("c"));
    EXPECT_FALSE(tracker.is_enabled("e"));
}

TEST(BundleTrackerTest, DualEnablementAndConflictBehavior) {
    std::vector<std::string> events = {"a","b","c","e"};

    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e") {
            return {
                {"a", "b"},
                {"b", "c"}
            };
        }
        return {};
    };

    auto get_inhibited_by = [](const std::string&) -> std::vector<std::string> {
        return {};
    };

    BundleKhanDFSTracker tracker(events, get_bundles, get_inhibited_by);

    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.is_enabled("b"));
    EXPECT_TRUE(tracker.is_enabled("c"));
    EXPECT_FALSE(tracker.is_enabled("e"));

    tracker.push("b");
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.is_enabled("c"));
    EXPECT_TRUE(tracker.is_enabled("e"));

    tracker.push("c");
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.is_enabled("e"));
}

TEST(BundleTraceExplorerTest, ProducesExpectedBundleTraces) {
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

    auto get_conflicts = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e1") return {"e2"};
        if (n == "e2") return {"e1", "e3"};
        if (n == "e3") return {"e2"};
        return {};
    };

    auto trackerFactory = getBundleKhanTrackerFactory(get_bundles, get_conflicts);
    auto engine = getDFSTraceGenFactory(trackerFactory)(events);
    auto generator = engine();

    std::vector<std::vector<std::string>> maximalTraces, traces;
    while (generator.next()) {
        auto result = generator.move_value();
        traces.push_back(result.path);
        if (result.is_maximal) {
            maximalTraces.push_back(std::move(result.path));
        }
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

    auto expectedMaximal = std::vector<std::vector<std::string>>{
        {"e2", "e4"},
        {"e1", "e3", "e4"},
        {"e3", "e1", "e4"},
    };

    ASSERT_THAT(maximalTraces, UnorderedElementsAreArray(expectedMaximal));
}
