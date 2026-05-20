#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include "FiniteES/PrimeKhanTracker.h"
#include "FiniteES/TraceGenerator.h"

using testing::UnorderedElementsAreArray;

TEST(PrimeKhanTrackerTest, EnablementPushPopTransitions) {
    std::vector<std::string> events = {"A", "B"};
    auto pred = [](const std::string& n) -> std::vector<std::string> {
        if (n == "B") return {"A"};
        return {};
    };
    auto conflicts = [](const std::string&) -> std::vector<std::string> {
        return {};
    };

    PrimeKhanTracker<std::string> tracker(events, pred, conflicts);

    EXPECT_TRUE(tracker.is_enabled("A"));
    EXPECT_FALSE(tracker.is_enabled("B"));

    tracker.push("A");
    EXPECT_FALSE(tracker.is_enabled("A"));
    EXPECT_TRUE(tracker.is_enabled("B"));

    tracker.pop("A");
    EXPECT_TRUE(tracker.is_enabled("A"));
    EXPECT_FALSE(tracker.is_enabled("B"));
}

TEST(PrimeKhanTrackerTest, EnablementPushPopTransitionsWorksWithSubsets) {
    std::vector<std::string> events = {"A", "B", "C", "D"};
    auto pred = [](const std::string& n) -> std::vector<std::string> {
        if (n == "B") return {"A"};
        return {};
    };
    auto conflicts = [](const std::string& e) -> std::vector<std::string> {
        if(e=="C") return {"D"};
        if(e == "D") return {"C"};
        return {};
    };

    PrimeKhanTracker<std::string> tracker(events, pred, conflicts),
    trackerBC(std::vector<std::string>{"B", "C"}, pred, conflicts);

    EXPECT_EQ(tracker.is_enabled("B"), trackerBC.is_enabled("B"));
    EXPECT_EQ(tracker.is_enabled("C"), trackerBC.is_enabled("C"));

    tracker.push("A"); trackerBC.push("A");
    EXPECT_EQ(tracker.is_enabled("B"), trackerBC.is_enabled("B"));
    EXPECT_EQ(tracker.is_enabled("C"), trackerBC.is_enabled("C"));

    tracker.push("D"); trackerBC.push("D");
    EXPECT_EQ(tracker.is_enabled("B"), trackerBC.is_enabled("B"));
    EXPECT_EQ(tracker.is_enabled("C"), trackerBC.is_enabled("C"));

    tracker.pop("D"); trackerBC.pop("D");
    EXPECT_EQ(tracker.is_enabled("B"), trackerBC.is_enabled("B"));
    EXPECT_EQ(tracker.is_enabled("C"), trackerBC.is_enabled("C"));

}

TEST(PrimeKhanTraceExplorerTest, ComplexCausalityAndConflictProducesExpectedTraces) {
    std::vector<std::string> events = {"e1","e2","e3","e4"};

    auto pred = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e3") return {"e1", "e2"};
        return {};
    };

    auto conflicts = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e4") return {"e3"};
        if (n == "e3") return {"e4"};
        return {};
    };

    auto trackerFactory = getPrimeKhanTrackerFactory(pred, conflicts);
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
        {}, {"e1"}, {"e2"}, {"e4"},
        {"e2", "e4"},
        {"e4", "e2"},
        {"e1", "e4"},
        {"e4", "e1"},
        {"e1", "e2"},
        {"e2", "e1"},
        {"e1", "e2", "e3"},
        {"e2", "e1", "e3"},
        {"e1", "e2", "e4"},
        {"e1", "e4", "e2"},
        {"e2", "e1", "e4"},
        {"e2", "e4", "e1"},
        {"e4", "e1", "e2"},
        {"e4", "e2", "e1"}
    };
    auto expectedMaximal = std::vector<std::vector<std::string>>{
        {"e1", "e2", "e3"},
        {"e2", "e1", "e3"},
        {"e1", "e2", "e4"},
        {"e1", "e4", "e2"},
        {"e2", "e1", "e4"},
        {"e2", "e4", "e1"},
        {"e4", "e1", "e2"},
        {"e4", "e2", "e1"}
    };

    ASSERT_THAT(traces, UnorderedElementsAreArray(expected));
    ASSERT_THAT(maximalTraces, UnorderedElementsAreArray(expectedMaximal));
}
