#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "InfiniteES/DFS/DFSTraceGenerator.h"
#include "InfiniteES/DFS/FlowDFSTracker.h"

class FlowDfsTraceExplorerTest : public testing::Test
{
protected:
    using Event = std::string;
    using EventSet = std::vector<Event>;
    using Trace = std::vector<Event>;

    const EventSet initial_roots{"a", "b", "c"};

    static EventSet get_predecessors(const Event &event)
    {
        if (event == "e")
            return {"a", "b", "c"};
        return {};
    }

    static EventSet get_successors(const Event &event)
    {
        if (event == "a" || event == "b" || event == "c")
            return {"e"};
        return {};
    }

    static EventSet get_conflicts(const Event &event)
    {
        if (event == "a")
            return {"b"};
        if (event == "b")
            return {"a", "c"};
        if (event == "c")
            return {"b"};
        return {};
    }

    static auto tracker_factory()
    {
        return getFlowDFSTrackerFactory<EventSet>(
            get_predecessors, get_successors, get_conflicts);
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
        {"a"}, {"a", "c"}, {"a", "c", "e"},
        {"b"}, {"b", "e"},
        {"c"}, {"c", "a"}, {"c", "a", "e"},
    };
};

TEST_F(FlowDfsTraceExplorerTest, GeneratesFlowTracesInDepthFirstOrder)
{
    const auto traces = generate_traces(NeverPruneTrace{});
    EXPECT_EQ(traces, expected_traces_in_dfs_order);
}

TEST_F(FlowDfsTraceExplorerTest, PrunesPrefixesAtTheDepthLimit)
{
    constexpr std::size_t max_depth = 2;
    const auto traces = generate_traces(
        [](const Trace &trace) { return trace.size() >= max_depth; });

    const std::vector<Trace> expected{{}, {"a"}, {"b"}, {"c"}};
    EXPECT_EQ(traces, expected);
}

TEST_F(FlowDfsTraceExplorerTest, PruningOneBranchRestoresStateForItsSiblings)
{
    const auto traces = generate_traces([](const Trace &trace)
    {
        return !trace.empty() && trace.front() == "a";
    });

    const std::vector<Trace> expected{
        {},
        {"b"}, {"b", "e"},
        {"c"}, {"c", "a"}, {"c", "a", "e"},
    };
    EXPECT_EQ(traces, expected);
}

TEST_F(FlowDfsTraceExplorerTest, ConflictingPredecessorsSatisfyFlowRequirementsAndPopRestoresThem)
{
    auto tracker = tracker_factory()(initial_roots);
    const auto newly_enabled = tracker.push("b");
    const EventSet expected_newly_enabled{"e"};
    EXPECT_EQ(newly_enabled, expected_newly_enabled);
    EXPECT_TRUE(tracker.is_enabled("e"));
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("c"));

    tracker.pop("b");
    EXPECT_FALSE(tracker.is_enabled("e"));
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.is_enabled("b"));
    EXPECT_TRUE(tracker.is_enabled("c"));
}

TEST(FlowDfsTrackerTest, ConflictOutsidePredecessorSetIsInhibitedWithoutSatisfyingAnExtraRequirement)
{
    using EventSet = std::vector<std::string>;
    const auto predecessors = [](const std::string &event) -> EventSet
    {
        if (event == "e") return {"a", "b", "c"};
        return {};
    };
    const auto successors = [](const std::string &event) -> EventSet
    {
        if (event == "a" || event == "b" || event == "c") return {"e"};
        return {};
    };
    const auto conflicts = [](const std::string &event) -> EventSet
    {
        if (event == "a" || event == "c" || event == "d") return {"b"};
        if (event == "b") return {"a", "c", "d"};
        return {};
    };
    const EventSet roots{"a", "b", "c", "d"};
    FlowDFSTracker<EventSet, decltype(predecessors), decltype(successors), decltype(conflicts)>
        tracker(roots, predecessors, successors, conflicts);

    const auto newly_enabled = tracker.push("b");
    const EventSet expected_newly_enabled{"e"};
    EXPECT_EQ(newly_enabled, expected_newly_enabled);
    EXPECT_TRUE(tracker.is_enabled("e"));
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("c"));
    EXPECT_FALSE(tracker.is_enabled("d")); // d is conflicted but is not a predecessor of e.

    tracker.pop("b");
    EXPECT_FALSE(tracker.is_enabled("e"));
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.is_enabled("c"));
    EXPECT_TRUE(tracker.is_enabled("d"));
}

TEST(FlowDfsTrackerTest, SelfConflictingRootIsNotInitiallyEnabled)
{
    using EventSet = std::vector<std::string>;
    const auto predecessors = [](const std::string &) -> EventSet { return {}; };
    const auto successors = [](const std::string &) -> EventSet { return {}; };
    const auto conflicts = [](const std::string &event) -> EventSet
    {
        return event == "a" ? EventSet{"a"} : EventSet{};
    };
    const EventSet roots{"a", "b"};
    FlowDFSTracker<EventSet, decltype(predecessors), decltype(successors), decltype(conflicts)>
        tracker(roots, predecessors, successors, conflicts);

    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.is_enabled("b"));
}

TEST(FlowDfsTrackerTest, SelfDependentEventsRemainDisabledAfterTheirOtherPredecessorsFire)
{
    using EventSet = std::vector<std::string>;
    const auto predecessors = [](const std::string &event) -> EventSet
    {
        if (event == "a") return {"a"};
        if (event == "c") return {"b", "c"};
        if (event == "d") return {"a", "b"};
        return {};
    };
    const auto successors = [](const std::string &event) -> EventSet
    {
        if (event == "a") return {"a", "d"};
        if (event == "b") return {"c", "d"};
        return {};
    };
    const auto conflicts = [](const std::string &) -> EventSet { return {}; };
    const EventSet roots{"b"};
    FlowDFSTracker<EventSet, decltype(predecessors), decltype(successors), decltype(conflicts)>
        tracker(roots, predecessors, successors, conflicts);

    EXPECT_TRUE(tracker.is_enabled("b"));
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("c"));
    EXPECT_FALSE(tracker.is_enabled("d"));

    tracker.push("b");
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("c")); // Still requires itself.
    EXPECT_FALSE(tracker.is_enabled("d")); // Still requires a.
}

TEST(FlowDfsTrackerTest, ConflictWithSuccessorPreventsAndDoesNotReportEnablement)
{
    using EventSet = std::vector<std::string>;
    const auto predecessors = [](const std::string &event) -> EventSet
    {
        if (event == "b") return {"a"};
        return {};
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
    FlowDFSTracker<EventSet, decltype(predecessors), decltype(successors), decltype(conflicts)>
        tracker(roots, predecessors, successors, conflicts);

    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.push("a").empty());
    EXPECT_FALSE(tracker.is_enabled("b"));
    tracker.pop("a");
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("b"));
}
