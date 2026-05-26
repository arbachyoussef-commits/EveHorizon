#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>

#include "FiniteES/DFSTraceGenerator.h"
#include "FiniteES/StableKhanDFSTracker.h"

using ::testing::UnorderedElementsAreArray;

TEST(StableKhanTrackerTest, EnablingAndConflictSemantics) {
    std::vector<std::string> events = {"R1", "R2", "S", "A", "B", "C"};

    auto get_enablers = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "R1") return {{}};
        if (n == "R2") return {{}};
        if (n == "S")  return {{}};
        if (n == "A")  return {{"R1"}, {"R2"}};
        if (n == "B")  return {{"A", "S"}};
        if (n == "C")  return {{"R1", "R2"}};
        return {};
    };

    auto get_conflicts = [](const std::string& n) -> std::vector<std::string> {
        if (n == "R1") return {"R2"};
        if (n == "R2") return {"R1"};
        if (n == "A")  return {"S"};
        if (n == "S")  return {"A"};
        if (n == "C")  return {"B"};
        if (n == "B")  return {"C"};
        return {};
    };

    StableKhanDFSTracker tracker(events, get_enablers, get_conflicts);

    EXPECT_TRUE(tracker.is_enabled("R1"));
    EXPECT_TRUE(tracker.is_enabled("R2"));
    EXPECT_TRUE(tracker.is_enabled("S"));
    EXPECT_FALSE(tracker.is_enabled("A"));
    EXPECT_FALSE(tracker.is_enabled("B"));
    EXPECT_FALSE(tracker.is_enabled("C"));

    tracker.push("R1");
    EXPECT_FALSE(tracker.is_enabled("R2"));
    EXPECT_TRUE(tracker.is_enabled("A"));
    EXPECT_FALSE(tracker.is_enabled("B"));
    EXPECT_FALSE(tracker.is_enabled("C"));

    tracker.push("A");
    EXPECT_FALSE(tracker.is_enabled("S"));
    EXPECT_FALSE(tracker.is_enabled("B"));
    EXPECT_FALSE(tracker.is_enabled("C"));

    tracker.pop("A");
    EXPECT_TRUE(tracker.is_enabled("S"));
    tracker.pop("R1");
    EXPECT_TRUE(tracker.is_enabled("R2"));
}

TEST(StableKhanTraceExplorerTest, ProducesExpectedStableTraces) {
    std::vector<std::string> events = {"R1", "R2", "S", "A", "B", "C"};

    auto get_enablers = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "R1") return {{}};
        if (n == "R2") return {{}};
        if (n == "S")  return {{}};
        if (n == "A")  return {{"R1"}, {"R2"}};
        if (n == "B")  return {{"A", "S"}};
        if (n == "C")  return {{"R1", "R2"}};
        return {};
    };

    auto get_conflicts = [](const std::string& n) -> std::vector<std::string> {
        if (n == "R1") return {"R2"};
        if (n == "R2") return {"R1"};
        if (n == "A")  return {"S"};
        if (n == "S")  return {"A"};
        if (n == "C")  return {"B"};
        if (n == "B")  return {"C"};
        return {};
    };

    auto trackerFactory = getStableKhanTrackerFactory(get_enablers, get_conflicts);
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

    auto expectedMaximal = std::vector<std::vector<std::string>>{
        {"R1", "S"},
        {"S", "R1"},
        {"R2", "S"},
        {"S", "R2"},
        {"R1", "A"},
        {"R2", "A"}
    };
    ASSERT_THAT(maximalTraces, UnorderedElementsAreArray(expectedMaximal));

    auto expected = std::vector<std::vector<std::string>>{
        {}, {"R1"}, {"R2"}, {"S"},
        {"R1", "S"},
        {"S", "R1"},
        {"R2", "S"},
        {"S", "R2"},
        {"R1", "A"},
        {"R2", "A"}
    };
    ASSERT_THAT(traces, UnorderedElementsAreArray(expected));
}
