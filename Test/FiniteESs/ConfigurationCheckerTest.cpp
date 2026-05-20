#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <algorithm>
#include <set>
#include <map>
#include <tuple> // For std::pair in dfs_stack
#include "FiniteES/ConfigurationChecker.h"
#include "FiniteES/PrimeKhanTracker.h"
#include "FiniteES/StableKhanTracker.h"
#include "FiniteES/BundleKhanTracker.h"
#include "FiniteES/TraceGenerator.h"

TEST(ConfigurationCheckerPrimeTest, IsConfigurationChecksPrimeES) {
    // Nodes: {1, 2, 3, 4, 5}
    // Causality: 1 -> 3, 2 -> 3, 3 -> 5
    // Conflicts: 1 # 4, 2 # 4

    auto get_prime_predecessors_global = [](int n) -> std::vector<int> {
        if (n == 3) return {1, 2};
        if (n == 5) return {3};
        return {};
    };

    auto get_prime_conflicts_global = [](int n) -> std::vector<int> {
        if (n == 1) return {4};
        if (n == 2) return {4};
        if (n == 4) return {1, 2};
        return {};
    };

    auto trackerFactory = getPrimeKhanTrackerFactory(get_prime_predecessors_global, get_prime_conflicts_global);
    auto trace_gen_factory = getKhanTraceGenFactory(trackerFactory);

    // Test Cases
    // PES: 1->3, 2->3, 3->5; 1#4, 2#4

    // Valid configurations
    EXPECT_TRUE(isConfiguration(std::vector<int>({}), trace_gen_factory).is_config) << "Empty set is a configuration.";
    EXPECT_TRUE(isConfiguration(std::vector<int>({1}), trace_gen_factory).is_config) << "Candidate {1} should be a configuration.";
    EXPECT_TRUE(isConfiguration(std::vector<int>({2}), trace_gen_factory).is_config) << "Candidate {2} should be a configuration.";
    EXPECT_TRUE(isConfiguration(std::vector<int>({4}), trace_gen_factory).is_config) << "Candidate {4} should be a configuration.";
    EXPECT_TRUE(isConfiguration(std::vector<int>({1, 2}), trace_gen_factory).is_config) << "Candidate {1, 2} should be a configuration.";
    EXPECT_TRUE(isConfiguration(std::vector<int>({1, 2, 3}), trace_gen_factory).is_config) << "Candidate {1, 2, 3} should be a configuration.";
    EXPECT_TRUE(isConfiguration(std::vector<int>({1, 2, 3, 5}), trace_gen_factory).is_config) << "Candidate {1, 2, 3, 5} should be a configuration.";

    // Invalid configurations
    EXPECT_FALSE(isConfiguration(std::vector<int>({3}), trace_gen_factory).is_config) << "Candidate {3} is not a configuration (predecessors missing).";
    EXPECT_FALSE(isConfiguration(std::vector<int>({5}), trace_gen_factory).is_config) << "Candidate {5} is not a configuration (predecessors missing).";
    EXPECT_FALSE(isConfiguration(std::vector<int>({1, 3}), trace_gen_factory).is_config) << "Candidate {1, 3} is not a configuration (predecessor 2 missing).";
    EXPECT_FALSE(isConfiguration(std::vector<int>({1, 4}), trace_gen_factory).is_config) << "Candidate {1, 4} is not a configuration (conflict).";
    EXPECT_FALSE(isConfiguration(std::vector<int>({2, 4}), trace_gen_factory).is_config) << "Candidate {2, 4} is not a configuration (conflict).";
    EXPECT_FALSE(isConfiguration(std::vector<int>({1, 2, 3, 4}), trace_gen_factory).is_config) << "Candidate {1, 2, 3, 4} is not a configuration (conflict with 1 and 2).";
    EXPECT_FALSE(isConfiguration(std::vector<int>({1, 1}), trace_gen_factory).is_config) << "Candidate {1, 1} is invalid (duplicate event).";
}

TEST(ConfigurationCheckerStableTest, IsConfigurationChecksStableES) {
    // Nodes: {1, 2, 3, 4}
    // Stable Logic: 
    // Event 3 is enabled by HISTORY {1} OR HISTORY {2}.
    // Event 4 is a root but conflicts with 3.

    auto get_stable_histories = [](int n) -> std::vector<std::vector<int>> {
        if (n == 1 || n == 2 || n == 4) return {{}}; // Roots
        if (n == 3) return {{1}, {2}};               // Disjunctive enabling
        return {};
    };

    auto get_stable_conflicts = [](int n) -> std::vector<int> {
        if (n == 3) return {4};
        if (n == 4) return {3};
        if (n == 1) return {2}; //stability
        if (n == 2) return {1};
        return {};
    };

    auto trackerFactory = getStableKhanTrackerFactory(get_stable_histories, get_stable_conflicts);
    auto trace_gen_factory = getKhanTraceGenFactory(trackerFactory);

    EXPECT_TRUE(isConfiguration(std::vector<int>({}), trace_gen_factory).is_config);
    // --- Valid configurations ---
    // {1, 3} is valid because 3 is justified by {1}
    EXPECT_TRUE(isConfiguration(std::vector<int>({1, 3}), trace_gen_factory).is_config);
    
    // {2, 3} is valid because 3 is justified by {2}
    EXPECT_TRUE(isConfiguration(std::vector<int>({2, 3}), trace_gen_factory).is_config);
    
    // {1, 2, 3} is invalid (both histories satisfied, but only one is allowed due to stability)
    EXPECT_FALSE(isConfiguration(std::vector<int>({1, 2, 3}), trace_gen_factory).is_config);
    
    // {4} is a valid root configuration
    EXPECT_TRUE(isConfiguration(std::vector<int>({4}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration(std::vector<int>({1}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration(std::vector<int>({2}), trace_gen_factory).is_config);

    // --- Invalid configurations ---
    // {3} alone is invalid (needs 1 or 2)
    EXPECT_FALSE(isConfiguration(std::vector<int>({3}), trace_gen_factory).is_config);
    
    // {3, 4} is invalid due to conflict, even if histories are met
    EXPECT_FALSE(isConfiguration(std::vector<int>({1, 3, 4}), trace_gen_factory).is_config);

    // {1, 2} is invalid
    EXPECT_FALSE(isConfiguration(std::vector<int>({1, 2}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration(std::vector<int>({1, 4}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration(std::vector<int>({4, 2}), trace_gen_factory).is_config);
}

TEST(ConfigurationCheckerBundleTest, IsConfigurationChecksBundleES) {
    // Nodes: {1, 2, 3, 4}
    // Bundle Logic:
    // 4 depends on (1 OR 2) AND {3}
    // 1 and 2 are in conflict.

    auto get_bundles = [](int n) -> std::vector<std::vector<int>> {
        if (n == 4) return {{1, 2}, {3}}; 
        return {};
    };

    auto get_conflicts = [](int n) -> std::vector<int> {
        if (n == 1) return {2};
        if (n == 2) return {1};
        return {};
    };

    auto trackerFactory = getBundleKhanTrackerFactory(get_bundles, get_conflicts);
    auto trace_gen_factory = getKhanTraceGenFactory(trackerFactory);

    // --- Valid configurations ---
    EXPECT_TRUE(isConfiguration(std::vector<int>({}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration(std::vector<int>({1}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration(std::vector<int>({2}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration(std::vector<int>({3}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration(std::vector<int>({1, 3, 4}), trace_gen_factory).is_config) << "4 should be enabled by 1 and 3";
    EXPECT_TRUE(isConfiguration(std::vector<int>({2, 3, 4}), trace_gen_factory).is_config) << "4 should be enabled by 2 and 3";
    EXPECT_TRUE(isConfiguration(std::vector<int>({1, 3}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration(std::vector<int>({2, 3}), trace_gen_factory).is_config);
    // --- Invalid configurations ---
    EXPECT_FALSE(isConfiguration(std::vector<int>({4}), trace_gen_factory).is_config) << "4 missing all dependencies";
    EXPECT_FALSE(isConfiguration(std::vector<int>({1, 4}), trace_gen_factory).is_config) << "4 missing dependency 3";
    EXPECT_FALSE(isConfiguration(std::vector<int>({1, 2}), trace_gen_factory).is_config) << "1 & 2 are in conflict";
    EXPECT_FALSE(isConfiguration(std::vector<int>({1, 2, 3, 4}), trace_gen_factory).is_config) << "Conflict between 1 and 2";
}

TEST(ConfigurationCheckerBundleTest, IsConfigurationChecksExtendedBundleES) {
    // Nodes: {1, 2, 3, 4}
    // Bundle Logic:
    // 4 depends on 1 AND 3
    // 1 inhibits 2

    auto get_bundles = [](int n) -> std::vector<std::vector<int>> {
        if (n == 4) return {{1}, {3}}; 
        return {};
    };

    auto inhibited_by = [](int n) -> std::vector<int> {
        if (n == 1) return {2};
        return {};
    };

    auto trackerFactory = getBundleKhanTrackerFactory(get_bundles, inhibited_by);
    auto trace_gen_factory = getKhanTraceGenFactory(trackerFactory);

    // --- Valid configurations ---
    EXPECT_TRUE(isConfiguration<true>(std::vector<int>({}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration<true>(std::vector<int>({1}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration<true>(std::vector<int>({2}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration<true>(std::vector<int>({3}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration<true>(std::vector<int>({1, 3, 4}), trace_gen_factory).is_config) << "4 should be enabled by 1 and 3";
    EXPECT_FALSE(isConfiguration<true>(std::vector<int>({2, 3, 4}), trace_gen_factory).is_config) << "4 should be enabled by 2 and 3";
    EXPECT_TRUE(isConfiguration<true>(std::vector<int>({1, 3}), trace_gen_factory).is_config);
    EXPECT_TRUE(isConfiguration<true>(std::vector<int>({2, 3}), trace_gen_factory).is_config);

    //Check asymmetric conflict: 2 inhibits 1, so the configuration with 1 & 2 has one order only
    auto result_21 = isConfiguration<true>(std::vector<int>({2, 1}), trace_gen_factory);
    EXPECT_TRUE(result_21.is_config);
    EXPECT_TRUE(result_21.trace == std::vector<int>({2, 1})) << "in one order only";
    EXPECT_TRUE(isConfiguration<true>(std::vector<int>({1, 2, 3, 4}), trace_gen_factory).is_config);

    // --- Invalid configurations ---
    EXPECT_FALSE(isConfiguration<true>(std::vector<int>({4}), trace_gen_factory).is_config) << "4 missing all dependencies";
    EXPECT_FALSE(isConfiguration<true>(std::vector<int>({1, 4}), trace_gen_factory).is_config) << "4 missing dependency 3";
}
