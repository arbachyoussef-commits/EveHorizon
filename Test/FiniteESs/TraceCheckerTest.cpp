#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <algorithm>
#include <cassert>
#include "FiniteES/BundleKhanTracker.h"
#include "FiniteES/TraceChecker.h"

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

    auto expected = std::vector<std::vector<std::string>>{
        {}, {"e1"}, {"e3"}, {"e2"},
        {"e1", "e3"},
        {"e3", "e1"},
        {"e2", "e4"},
        {"e1", "e3", "e4"},
        {"e3", "e1", "e4"},
    };

    auto trackerFactory = getBundleKhanTrackerFactory(get_bundles, get_conflicts);

    for(auto & trace : expected) {
        EXPECT_TRUE(isTrace(trace, trackerFactory)) << "Trace failed validation: " << testing::PrintToString(trace);
    }

    EXPECT_EQ(isTrace(std::vector<std::string>({"e1", "e2"}), trackerFactory).first_invalid_event(), "e2") << "Invalid trace passed validation";
    EXPECT_EQ(isTrace(std::vector<std::string>({"e3", "e2"}), trackerFactory).first_invalid_event(), "e2") << "Invalid trace passed validation";
    EXPECT_EQ(isTrace(std::vector<std::string>({"e4"}), trackerFactory).first_invalid_event(), "e4") << "Invalid trace passed validation";
    EXPECT_EQ(isTrace(std::vector<std::string>({"e1", "e4"}), trackerFactory).first_invalid_event(), "e4") << "Invalid trace passed validation";
    EXPECT_EQ(isTrace(std::vector<std::string>({"e3", "e4"}), trackerFactory).first_invalid_event(), "e4") << "Invalid trace passed validation";
    EXPECT_EQ(isTrace(std::vector<std::string>({"e1", "e1"}), trackerFactory).first_invalid_event(), "e1") << "Invalid trace passed validation";
}

TEST(ExtendedBundleTraceExplorerTest, ProducesExpectedExtendedBundleTraces) {
    std::vector<std::string> events = {"e1","e2","e3","e4"};

    auto get_bundles = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "e4") {
            return {
                {"e1", "e2"}
            };
        }
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

    auto trackerFactory = getBundleKhanTrackerFactory(get_bundles, get_inhibited_by);

    for(auto & trace : expected) {
        EXPECT_TRUE(isTrace(trace, trackerFactory)) << "Trace failed validation: " << testing::PrintToString(trace);
    }

    EXPECT_FALSE(isTrace(std::vector<std::string>({"e1", "e2"}), trackerFactory)) << "Invalid trace passed validation";
    EXPECT_EQ(isTrace(std::vector<std::string>({"e3", "e1"}), trackerFactory).first_invalid_event(), "e1") << "Invalid trace passed validation";
    EXPECT_FALSE(isTrace(std::vector<std::string>({"e4"}), trackerFactory)) << "Invalid trace passed validation";
    EXPECT_FALSE(isTrace(std::vector<std::string>({"e3", "e4"}), trackerFactory)) << "Invalid trace passed validation";
    EXPECT_FALSE(isTrace(std::vector<std::string>({"e1", "e1"}), trackerFactory)) << "Invalid trace passed validation";
}
