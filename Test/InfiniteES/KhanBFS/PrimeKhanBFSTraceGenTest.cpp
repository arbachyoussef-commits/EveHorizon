#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include "InfiniteES/KhanBFS/BFSTraceGenerator.h"
#include "InfiniteES/KhanBFS/PrimeKhanBFSTracker.h"

using namespace testing;

namespace {
    // Define simple mock getters for testing the tracker logic
    auto no_bundles = [](const std::string&) -> std::vector<std::vector<std::string>> { return {}; };
    auto no_succ = [](const std::string&) -> std::vector<std::string> { return {}; };
    auto no_conf = [](const std::string&) -> std::vector<std::string> { return {}; };
}

TEST(PrimeKhanBfsTraceExplorerTest, SmokeTest) {
    std::vector<std::string> events = {"e1","e2","e3","e4"};

    auto get_enablers = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e3") return {"e1", "e2"};
        return {};
    };

    auto get_successors = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e1" || n == "e2") return {"e3"};
        return {};
    };

    auto get_conflicts = [](const std::string& n) -> std::vector<std::string> {
        if (n == "e4") return {"e1"};
        if (n == "e1") return {"e4"};
        return {};
    };

    PrimeKhanBFSTracker<std::string, decltype(get_enablers), decltype(get_successors), decltype(get_conflicts)> tracker(get_enablers, get_successors, get_conflicts);
    auto generator = exploreInfiniteHybridBFS(std::vector<std::string>{"e1", "e2", "e4"}, tracker);

    std::vector<std::vector<std::string>> traces;
    while (generator.next()) {
        traces.push_back(generator.get_value());
    }

    auto expected = std::vector<std::vector<std::string>>{
        {}, {"e1"}, {"e2"}, {"e4"},
        {"e2", "e4"}, {"e4", "e2"},
        {"e1", "e2"}, {"e2", "e1"},
        {"e1", "e2", "e3"}, {"e2", "e1", "e3"}
    };
    ASSERT_THAT(traces, UnorderedElementsAreArray(expected));
}
