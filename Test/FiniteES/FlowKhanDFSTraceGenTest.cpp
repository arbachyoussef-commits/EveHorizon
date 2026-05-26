#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include "FiniteES/DFSTraceGenerator.h"
#include "FiniteES/FlowKhanDFSTracker.h"

using testing::UnorderedElementsAreArray;

TEST(FlowTrackerTest, FlowEnablementAndConflictBehavior) {
    auto events = std::vector<std::string>{"A", "B", "C", "D", "E"};
    auto get_flow_preds = [](const std::string& n) -> std::vector<std::string> {
        if (n == "E") return {"A", "B", "C"};
        return {};
    };
    auto get_conflicts = [](const std::string& n) -> std::vector<std::string> {
        if (n == "A") return {"B"};
        if (n == "B") return {"A", "C", "D"};
        if (n == "C") return {"B"};
        if (n == "D") return {"B"};
        return {};
    };

    FlowKhanDFSTracker tracker(events, get_flow_preds, get_conflicts);

    EXPECT_TRUE(tracker.is_enabled("A"));
    EXPECT_TRUE(tracker.is_enabled("B"));
    EXPECT_TRUE(tracker.is_enabled("C"));
    EXPECT_TRUE(tracker.is_enabled("D"));
    EXPECT_FALSE(tracker.is_enabled("E"));

    tracker.push("A");
    EXPECT_FALSE(tracker.is_enabled("B"));
    EXPECT_TRUE(tracker.is_enabled("C"));
    EXPECT_TRUE(tracker.is_enabled("D"));
    EXPECT_FALSE(tracker.is_enabled("E"));

    tracker.push("C");
    EXPECT_TRUE(tracker.is_enabled("E"));
    EXPECT_FALSE(tracker.is_enabled("B"));
    EXPECT_TRUE(tracker.is_enabled("D"));

    tracker.pop("C");
    tracker.pop("A");
    tracker.push("B");
    EXPECT_TRUE(tracker.is_enabled("E"));
    EXPECT_FALSE(tracker.is_enabled("A"));
    EXPECT_FALSE(tracker.is_enabled("C"));
    EXPECT_FALSE(tracker.is_enabled("D"));
}

TEST(FlowTraceExplorerTest, ProducesExpectedFlowTraces) {
    auto events = std::vector<std::string>{"A", "B", "C", "E"};
    auto get_flow_preds = [](const std::string& n) -> std::vector<std::string> {
        if (n == "E") return {"A", "B", "C"};
        return {};
    };
    auto get_conflicts = [](const std::string& n) -> std::vector<std::string> {
        if (n == "A") return {"B"};
        if (n == "B") return {"A", "C"};
        if (n == "C") return {"B"};
        return {};
    };

    auto trackerFactory = getFlowKhanTrackerFactory(get_flow_preds, get_conflicts);
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
        {}, {"A"}, {"C"}, {"B"},
        {"A", "C"},
        {"C", "A"},
        {"B", "E"},
        {"A", "C", "E"},
        {"C", "A", "E"},
    };
    ASSERT_THAT(traces, UnorderedElementsAreArray(expected));

    auto expectedMaximal = std::vector<std::vector<std::string>>{
        {"B", "E"},
        {"A", "C", "E"},
        {"C", "A", "E"},
    };

    ASSERT_THAT(maximalTraces, UnorderedElementsAreArray(expectedMaximal));
}
