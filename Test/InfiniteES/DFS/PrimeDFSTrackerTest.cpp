#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "InfiniteES/DFS/DFSTraceGenerator.h"
#include "InfiniteES/DFS/PrimeDFSTracker.h"

class PrimeDfsTraceExplorerTest : public testing::Test
{
protected:
    using Event = std::string;
    using EventSet = std::vector<Event>;
    using Trace = std::vector<Event>;

    const EventSet initial_roots{"e1", "e2", "e4"};

    static auto tracker_factory()
    {
        auto get_predecessors = [](const Event &event) -> EventSet
        {
            if (event == "e3")
                return {"e1", "e2"};
            return {};
        };

        auto get_successors = [](const Event &event) -> EventSet
        {
            if (event == "e1" || event == "e2")
                return {"e3"};
            return {};
        };

        auto get_conflicts = [](const Event &event) -> EventSet
        {
            if (event == "e1")
                return {"e4"};
            if (event == "e4")
                return {"e1"};
            return {};
        };

        return getPrimeDFSTrackerFactory<EventSet>(
            get_predecessors,
            get_successors,
            get_conflicts);
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
                break; // Fail promptly if a finite fixture ever starts looping.
        }

        return traces;
    }
};

TEST_F(PrimeDfsTraceExplorerTest, GeneratesAllTracesInDepthFirstOrder)
{
    const auto traces = generate_traces(NeverPruneTrace{});

    const std::vector<Trace> expected{
        {},
        {"e1"},
        {"e1", "e2"},
        {"e1", "e2", "e3"},
        {"e2"},
        {"e2", "e1"},
        {"e2", "e1", "e3"},
        {"e2", "e4"},
        {"e4"},
        {"e4", "e2"},
    };

    EXPECT_EQ(traces, expected);
}

TEST_F(PrimeDfsTraceExplorerTest, PrunesPrefixesAtTheDepthLimit)
{
    constexpr std::size_t max_depth = 2;
    const auto traces = generate_traces(
        [](const Trace &trace) { return trace.size() >= max_depth; });

    const std::vector<Trace> expected{
        {},
        {"e1"},
        {"e2"},
        {"e4"},
    };

    EXPECT_EQ(traces, expected);
}

TEST_F(PrimeDfsTraceExplorerTest, PruningOneBranchRestoresStateForSiblingBranches)
{
    const auto traces = generate_traces([](const Trace &trace)
    {
        return !trace.empty() && trace.front() == "e1";
    });

    const std::vector<Trace> expected{
        {},
        {"e2"},
        {"e2", "e1"},
        {"e2", "e1", "e3"},
        {"e2", "e4"},
        {"e4"},
        {"e4", "e2"},
    };

    EXPECT_EQ(traces, expected);
}

TEST_F(PrimeDfsTraceExplorerTest, TrackerPushAndPopRestoreCausalityAndConflicts)
{
    auto tracker = tracker_factory()(initial_roots);
    EXPECT_TRUE(tracker.is_enabled("e1"));
    EXPECT_TRUE(tracker.is_enabled("e2"));
    EXPECT_TRUE(tracker.is_enabled("e4"));
    EXPECT_FALSE(tracker.is_enabled("e3"));

    EXPECT_TRUE(tracker.push("e1").empty());
    EXPECT_FALSE(tracker.is_enabled("e1"));
    EXPECT_FALSE(tracker.is_enabled("e4")); // e1 conflicts with e4.

    const auto newly_enabled = tracker.push("e2");
    const EventSet expected_newly_enabled{"e3"};
    EXPECT_EQ(newly_enabled, expected_newly_enabled);
    EXPECT_TRUE(tracker.is_enabled("e3"));

    tracker.pop("e2");
    EXPECT_TRUE(tracker.is_enabled("e2"));
    EXPECT_FALSE(tracker.is_enabled("e3"));
    tracker.pop("e1");
    EXPECT_TRUE(tracker.is_enabled("e1"));
    EXPECT_TRUE(tracker.is_enabled("e2"));
    EXPECT_TRUE(tracker.is_enabled("e4"));
}

TEST(PrimeDfsTrackerTest, CausalCycleDoesNotEnableEitherEvent)
{
    using EventSet = std::vector<std::string>;
    const auto predecessors = [](const std::string &event) -> EventSet
    {
        if (event == "a") return {"b"};
        if (event == "b") return {"a"};
        return {};
    };
    const auto successors = [](const std::string &event) -> EventSet
    {
        if (event == "a") return {"b"};
        if (event == "b") return {"a"};
        return {};
    };
    const auto conflicts = [](const std::string &) -> EventSet { return {}; };
    const EventSet events{"a", "b"};
    PrimeDFSTracker<EventSet, decltype(predecessors), decltype(successors), decltype(conflicts)>
        tracker(events, predecessors, successors, conflicts);

    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("b"));
}

TEST(PrimeDfsTrackerTest, ConflictWithSuccessorPreventsAndDoesNotReportEnablement)
{
    using EventSet = std::vector<std::string>;
    const auto predecessors = [](const std::string &event) -> EventSet
    {
        return event == "b" ? EventSet{"a"} : EventSet{};
    };
    const auto successors = [](const std::string &event) -> EventSet
    {
        return event == "a" ? EventSet{"b"} : EventSet{};
    };
    const auto conflicts = [](const std::string &event) -> EventSet
    {
        if (event == "a") return {"b"};
        if (event == "b") return {"a"};
        return {};
    };
    const EventSet roots{"a"};
    PrimeDFSTracker<EventSet, decltype(predecessors), decltype(successors), decltype(conflicts)>
        tracker(roots, predecessors, successors, conflicts);

    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.push("a").empty());
    EXPECT_FALSE(tracker.is_enabled("b"));

    tracker.pop("a");
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("b"));
}
