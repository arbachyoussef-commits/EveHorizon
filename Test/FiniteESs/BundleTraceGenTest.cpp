#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include "FiniteES/TraceGenerator.h"
#include "FiniteES/BundleKhanTracker.h"

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

    BundleKhanTracker<std::string> tracker(events, get_bundles, get_conflicts);

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
    auto engine = getKhanTraceGenFactory(trackerFactory)(events);
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
