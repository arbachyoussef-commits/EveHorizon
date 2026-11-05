#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "InfiniteES/DFS/DFSTraceGenerator.h"
#include "InfiniteES/DFS/StableDFSTracker.h"

class StableDfsTraceExplorerTest : public testing::Test
{
protected:
    using Event = std::string;
    using EventSet = std::vector<Event>;
    using Trace = std::vector<Event>;

    const EventSet initial_roots{"e1", "e2", "e4"};

    static std::vector<std::vector<Event>> get_enablers(const Event &event)
    {
        if (event == "e3")
            return {{"e1"}, {"e2"}}; // Alternative singleton histories: e1 OR e2.
        if (event == "e5")
            return {{"e2", "e4"}}; // Conjunctive history: e2 AND e4.
        return {{}};                    // e1, e2, and e4 are roots.
    }

    static EventSet get_successors(const Event &event)
    {
        if (event == "e1")
            return {"e3"};
        if (event == "e2")
            return {"e3", "e5"};
        if (event == "e4")
            return {"e5"};
        return {};
    }

    static EventSet get_conflicts(const Event &event)
    {
        if (event == "e1")
            return {"e2"};
        if (event == "e2")
            return {"e1"};
        if (event == "e3")
            return {"e4"};
        if (event == "e4")
            return {"e3"};
        return {};
    }

    static auto tracker_factory()
    {
        return getStableDFSTrackerFactory<EventSet>(get_enablers, get_successors, get_conflicts);
    }

    template <typename Limiter>
    std::vector<Trace> generate_traces(Limiter limiter) const
    {
        auto make_engine = getDFSTraceGenFactory<EventSet>(tracker_factory(), std::move(limiter));
        auto engine = make_engine(initial_roots);
        auto generator = engine();

        std::vector<Trace> traces;
        while (generator.next())
        {
            traces.push_back(generator.get_value());
            if (traces.size() > 100)
                break; // Prevent a broken finite fixture from hanging the test.
        }
        return traces;
    }

    inline static const std::vector<Trace> expected_traces_in_dfs_order{
        {},
        {"e1"}, {"e1", "e4"}, {"e1", "e3"},
        {"e2"}, {"e2", "e4"}, {"e2", "e4", "e5"}, {"e2", "e3"},
        {"e4"}, {"e4", "e1"}, {"e4", "e2"}, {"e4", "e2", "e5"},
    };
};

TEST_F(StableDfsTraceExplorerTest, GeneratesTracesInDepthFirstOrder)
{
    const auto traces = generate_traces(NeverPruneTrace{});
    EXPECT_EQ(traces, expected_traces_in_dfs_order);
}

TEST_F(StableDfsTraceExplorerTest, PrunesPrefixesAtTheDepthLimit)
{
    constexpr std::size_t max_depth = 2;
    const auto traces = generate_traces(
        [](const Trace &trace) { return trace.size() >= max_depth; });

    const std::vector<Trace> expected{{}, {"e1"}, {"e2"}, {"e4"}};
    EXPECT_EQ(traces, expected);
}

TEST_F(StableDfsTraceExplorerTest, PruningOneBranchRestoresStateForItsSiblings)
{
    const auto traces = generate_traces([](const Trace &trace)
    {
        return !trace.empty() && trace.front() == "e1";
    });

    const std::vector<Trace> expected{
        {},
        {"e2"}, {"e2", "e4"}, {"e2", "e4", "e5"}, {"e2", "e3"},
        {"e4"}, {"e4", "e1"}, {"e4", "e2"}, {"e4", "e2", "e5"},
    };
    EXPECT_EQ(traces, expected);
}

TEST_F(StableDfsTraceExplorerTest, TrackerMaintainsAlternativeAndConjunctiveHistoriesAcrossBacktracking)
{
    auto tracker = tracker_factory()(initial_roots);
    EXPECT_FALSE(tracker.is_enabled("e3"));
    EXPECT_FALSE(tracker.is_enabled("e5"));

    const auto newly_enabled = tracker.push("e1");
    const EventSet expected_newly_enabled{"e3"};
    EXPECT_EQ(newly_enabled, expected_newly_enabled);
    EXPECT_TRUE(tracker.is_enabled("e3")); // Alternative history {e1} is satisfied.
    EXPECT_FALSE(tracker.is_enabled("e5")); // AND-history still needs e2 and e4.
    EXPECT_FALSE(tracker.is_enabled("e2")); // e1 and e2 conflict.

    tracker.pop("e1");
    EXPECT_TRUE(tracker.is_enabled("e1"));
    EXPECT_TRUE(tracker.is_enabled("e2"));
    EXPECT_FALSE(tracker.is_enabled("e3"));
    EXPECT_FALSE(tracker.is_enabled("e5"));
}

TEST(StableDfsTrackerTest, EnablingCycleDoesNotEnableEitherEvent)
{
    using EventSet = std::vector<std::string>;
    const auto enablers = [](const std::string &event) -> std::vector<EventSet>
    {
        if (event == "a") return {{"b"}};
        if (event == "b") return {{"a"}};
        return {};
    };
    const auto successors = [](const std::string &event) -> EventSet
    {
        if (event == "a") return {"b"};
        if (event == "b") return {"a"};
        return {};
    };
    const auto conflicts = [](const std::string &) -> EventSet { return {}; };
    const EventSet roots{"a"};
    StableDFSTracker<EventSet, decltype(enablers), decltype(successors), decltype(conflicts)>
        tracker(roots, enablers, successors, conflicts);

    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("b"));
}

TEST(StableDfsTrackerTest, ConflictWithSuccessorPreventsAndDoesNotReportEnablement)
{
    using EventSet = std::vector<std::string>;
    const auto enablers = [](const std::string &event) -> std::vector<EventSet>
    {
        if (event == "b") return {{"a"}};
        return {EventSet{}}; // a is rooted.
    };
    const auto successors = [](const std::string &event) -> EventSet
    {
        if (event == "a") return {"b"};
        return {};
    };
    const auto conflicts = [](const std::string &event) -> EventSet
    {
        if (event == "a") return {"b"};
        if (event == "b") return {"a"};
        return {};
    };
    const EventSet roots{"a"};
    StableDFSTracker<EventSet, decltype(enablers), decltype(successors), decltype(conflicts)>
        tracker(roots, enablers, successors, conflicts);

    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.push("a").empty());
    EXPECT_FALSE(tracker.is_enabled("b"));
    tracker.pop("a");
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("b"));
}
