#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "InfiniteES/DFS/BundleDFSTracker.h"
#include "InfiniteES/DFS/DFSTraceGenerator.h"

class BundleDfsTraceExplorerTest : public testing::Test
{
protected:
    using Event = std::string;
    using EventSet = std::vector<Event>;
    using Trace = std::vector<Event>;

    const EventSet initial_roots{"a", "b"};

    static std::vector<std::vector<Event>> get_bundles(const Event &event)
    {
        if (event == "c")
            return {{"a", "b"}}; // One OR-bundle: a OR b.
        if (event == "d")
            return {{"c"}}; // c already requires one of a or b.
        return {}; // a and b are roots.
    }

    static EventSet get_successors(const Event &event)
    {
        if (event == "a" || event == "b")
            return {"c"};
        if (event == "c")
            return {"d"};
        return {};
    }

    static EventSet get_inhibitors(const Event &event)
    {
        if (event == "a")
            return {"b"};
        if (event == "b")
            return {"a"}; // In the standard Bundle ES fixture, a and b conflict symmetrically.
        return {};
    }

    static auto tracker_factory()
    {
        return getBundleDFSTrackerFactory<EventSet>(get_bundles, get_successors, get_inhibitors);
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
        {"a"}, {"a", "c"}, {"a", "c", "d"},
        {"b"}, {"b", "c"}, {"b", "c", "d"},
    };
};

TEST_F(BundleDfsTraceExplorerTest, GeneratesBundleTracesInDepthFirstOrder)
{
    const auto traces = generate_traces(NeverPruneTrace{});
    EXPECT_EQ(traces, expected_traces_in_dfs_order);
}

TEST_F(BundleDfsTraceExplorerTest, PrunesPrefixesAtTheDepthLimit)
{
    constexpr std::size_t max_depth = 2;
    const auto traces = generate_traces(
        [](const Trace &trace) { return trace.size() >= max_depth; });

    const std::vector<Trace> expected{{}, {"a"}, {"b"}};
    EXPECT_EQ(traces, expected);
}

TEST_F(BundleDfsTraceExplorerTest, PruningOneBranchRestoresStateForItsSiblings)
{
    const auto traces = generate_traces([](const Trace &trace)
    {
        return !trace.empty() && trace.front() == "a";
    });

    const std::vector<Trace> expected{
        {},
        {"b"}, {"b", "c"}, {"b", "c", "d"},
    };
    EXPECT_EQ(traces, expected);
}

class ExtendedBundleDfsTraceExplorerTest : public testing::Test
{
protected:
    using Event = std::string;
    using EventSet = std::vector<Event>;
    using Trace = std::vector<Event>;

    const EventSet initial_roots{"a", "b"};

    static std::vector<std::vector<Event>> get_bundles(const Event &event)
    {
        if (event == "c")
            return {{"b"}};
        return {};
    }

    static EventSet get_successors(const Event &event)
    {
        if (event == "b")
            return {"c"};
        return {};
    }

    static EventSet get_inhibitors(const Event &event)
    {
        if (event == "a")
            return {"b"}; // a inhibits b, but b does not inhibit a.
        return {};
    }
};

TEST_F(ExtendedBundleDfsTraceExplorerTest, AllowsTheNonInhibitedOrder)
{
    auto tracker_factory = getBundleDFSTrackerFactory<EventSet>(
        get_bundles, get_successors, get_inhibitors);
    auto make_engine = getDFSTraceGenFactory<EventSet>(
        tracker_factory, NeverPruneTrace{});
    auto engine = make_engine(initial_roots);
    auto generator = engine();

    std::vector<Trace> traces;
    while (generator.next())
        traces.push_back(generator.get_value());

    const std::vector<Trace> expected{
        {},
        {"a"},
        {"b"}, {"b", "a"}, {"b", "a", "c"},
        {"b", "c"}, {"b", "c", "a"},
    };
    EXPECT_EQ(traces, expected);
}

TEST(BundleDfsTrackerTest, BundleAlternativesAndConjunctiveBundlesAreTrackedAndReverted)
{
    using EventSet = std::vector<std::string>;
    const auto bundles = [](const std::string &event) -> std::vector<EventSet>
    {
        if (event == "e") return {{"a", "b"}, {"c"}};
        return {};
    };
    const auto successors = [](const std::string &event) -> EventSet
    {
        if (event == "a" || event == "c") return {"e"};
        return {};
    };
    const auto inhibitors = [](const std::string &) -> EventSet { return {}; };
    const EventSet roots{"a", "c"};
    BundleDFSTracker<EventSet, decltype(bundles), decltype(successors), decltype(inhibitors)>
        tracker(roots, bundles, successors, inhibitors);

    EXPECT_TRUE(tracker.push("a").empty()); // Satisfies {a,b}, but not {c}.
    EXPECT_FALSE(tracker.is_enabled("e"));
    const auto newly_enabled = tracker.push("c");
    const EventSet expected_newly_enabled{"e"};
    EXPECT_EQ(newly_enabled, expected_newly_enabled);
    EXPECT_TRUE(tracker.is_enabled("e"));

    tracker.pop("c");
    EXPECT_FALSE(tracker.is_enabled("e"));
    EXPECT_TRUE(tracker.is_enabled("c"));
    tracker.pop("a");
    EXPECT_TRUE(tracker.is_enabled("a"));
}

TEST(BundleDfsTrackerTest, EnablingCycleDoesNotEnableEitherEvent)
{
    using EventSet = std::vector<std::string>;
    const auto bundles = [](const std::string &event) -> std::vector<EventSet>
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
    const auto inhibitors = [](const std::string &) -> EventSet { return {}; };
    const EventSet roots{"a"};
    BundleDFSTracker<EventSet, decltype(bundles), decltype(successors), decltype(inhibitors)>
        tracker(roots, bundles, successors, inhibitors);

    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("b"));
}

TEST(BundleDfsTrackerTest, ConflictWithSuccessorIsAppliedBeforeEnabling)
{
    using EventSet = std::vector<std::string>;
    const auto bundles = [](const std::string &event) -> std::vector<EventSet>
    {
        if (event == "b") return {{"a"}};
        return {};
    };
    const auto successors = [](const std::string &event) -> EventSet
    {
        if (event == "a") return {"b"};
        return {};
    };
    const auto inhibitors = [](const std::string &event) -> EventSet
    {
        if (event == "a") return {"b"};
        if (event == "b") return {"a"};
        return {};
    };
    const EventSet roots{"a"};
    BundleDFSTracker<EventSet, decltype(bundles), decltype(successors), decltype(inhibitors)>
        tracker(roots, bundles, successors, inhibitors);

    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_TRUE(tracker.push("a").empty());
    EXPECT_FALSE(tracker.is_enabled("b"));
    tracker.pop("a");
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("b"));
}

TEST(BundleDfsTrackerTest, FiredConflictWithAnEnablerKeepsSuccessorDisabled)
{
    using EventSet = std::vector<std::string>;
    const auto bundles = [](const std::string &event) -> std::vector<EventSet>
    {
        if (event == "e") return {{"a"}};
        return {};
    };
    const auto successors = [](const std::string &event) -> EventSet
    {
        if (event == "a") return {"e"};
        return {};
    };
    const auto inhibitors = [](const std::string &event) -> EventSet
    {
        if (event == "x") return {"a"};
        return {};
    };
    const EventSet roots{"x", "a"};
    BundleDFSTracker<EventSet, decltype(bundles), decltype(successors), decltype(inhibitors)>
        tracker(roots, bundles, successors, inhibitors);

    tracker.push("x");
    EXPECT_FALSE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("e"));
    tracker.pop("x");
    EXPECT_TRUE(tracker.is_enabled("a"));
}

TEST(BundleDfsTrackerTest, PreviouslyInhibitedSuccessorStaysDisabledWhenEnablerFires)
{
    using EventSet = std::vector<std::string>;
    const auto bundles = [](const std::string &event) -> std::vector<EventSet>
    {
        if (event == "e") return {{"a"}};
        return {};
    };
    const auto successors = [](const std::string &event) -> EventSet
    {
        if (event == "a") return {"e"};
        return {};
    };
    const auto inhibitors = [](const std::string &event) -> EventSet
    {
        if (event == "x") return {"e"};
        return {};
    };
    const EventSet roots{"x", "a"};
    BundleDFSTracker<EventSet, decltype(bundles), decltype(successors), decltype(inhibitors)>
        tracker(roots, bundles, successors, inhibitors);

    tracker.push("x");
    EXPECT_FALSE(tracker.is_enabled("e"));
    EXPECT_TRUE(tracker.push("a").empty());
    EXPECT_FALSE(tracker.is_enabled("e"));
    tracker.pop("a");
    tracker.pop("x");
    EXPECT_TRUE(tracker.is_enabled("a"));
    EXPECT_FALSE(tracker.is_enabled("e"));
}
