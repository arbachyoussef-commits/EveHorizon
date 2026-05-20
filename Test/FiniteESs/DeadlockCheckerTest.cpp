#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>

#include "FiniteES/BundleKhanTracker.h"
#include "FiniteES/StableKhanTracker.h"
#include "FiniteES/DeadlockChecker.h"

TEST(DeadlockCheckerStableTest, RecognizesDeadlockFreeStableStructure) {
    std::vector<std::string> nodes = {"R1", "R2", "A"};

    auto get_enablers = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "R1") return {{}};
        if (n == "R2") return {{}};
        if (n == "A") return {{"R1"}, {"R2"}};
        return {};
    };

    auto get_conflicts = [](const std::string& n) -> std::vector<std::string> {
        if (n == "R1") return {"R2"};
        if (n == "R2") return {"R1"};
        return {};
    };

    auto trackerFactory = getStableKhanTrackerFactory(get_enablers, get_conflicts);
    EXPECT_TRUE(isDeadlockFree(nodes, trackerFactory, get_conflicts));
}

TEST(DeadlockCheckerStableTest, DetectsDeadlockInStableStructure) {
    std::vector<std::string> nodes = {"x", "y", "A"};

    auto get_enablers = [](const std::string& n) -> std::vector<std::vector<std::string>> {
        if (n == "x") return {{}};
        if (n == "y") return {{}};
        if (n == "A") return {{"x", "y"}};
        return {};
    };

    auto get_conflicts = [](const std::string& n) -> std::vector<std::string> {
        if (n == "x") return {"y"};
        if (n == "y") return {"x"}; //ie A is an impossible event
        return {};
    };


    std::vector<std::vector<std::string>> expected_maximals = {
        {"x"}, {"y"}
    };

    auto trackerFactory = getStableKhanTrackerFactory(get_enablers, get_conflicts);
    auto res = isDeadlockFree(nodes, trackerFactory, get_conflicts);
    EXPECT_FALSE(res);
    EXPECT_THAT(res.counterexample(), testing::AnyOfArray(expected_maximals));
}
