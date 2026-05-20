#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <algorithm>
#include "Conversion/Conversion.h"

// Define a simple Node type for testing
struct Node {
    int id;
    std::string name;

    Node() = default;
    Node(int id, const std::string& name = "") : id(id), name(name) {}

    bool operator==(const Node& other) const {
        return id == other.id && name == other.name;
    }

    bool operator<(const Node& other) const {
        return id < other.id;
    }
};

// Test suite for wrapPrimeAsStable
class WrapPrimeAsStableTest : public ::testing::Test {
protected:
    // Simple prime predecessor function: returns direct predecessors
    std::function<std::vector<Node>(const Node&)> simplePrimePred = [](const Node& n) {
        std::vector<Node> preds;
        if (n.id > 0) {
            preds.push_back(Node(n.id - 1, "pred_" + std::to_string(n.id - 1)));
        }
        return preds;
    };

    // Prime predecessor function that returns multiple predecessors
    std::function<std::vector<Node>(const Node&)> multiPrimePred = [](const Node& n) {
        std::vector<Node> preds;
        if (n.id > 0) {
            preds.push_back(Node(n.id - 1));
            if (n.id > 1) {
                preds.push_back(Node(n.id - 2));
            }
        }
        return preds;
    };
};

TEST_F(WrapPrimeAsStableTest, SinglePredecessor) {
    auto stablePred = wrapPrimeAsStable<Node>(simplePrimePred);
    Node testNode(5, "test");
    
    auto result = stablePred(testNode);
    
    // Result should be a vector of vectors (stable causality)
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].size(), 1u);
    EXPECT_EQ(result[0][0], Node(4, "pred_4"));
}

TEST_F(WrapPrimeAsStableTest, MultipleAlternativePredecessors) {
    auto stablePred = wrapPrimeAsStable<Node>(multiPrimePred);
    Node testNode(3);
    
    auto result = stablePred(testNode);
    
    // Result should contain multiple alternatives
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].size(), 2u);
    EXPECT_EQ(result[0][0].id, 2);
    EXPECT_EQ(result[0][1].id, 1);
}

TEST_F(WrapPrimeAsStableTest, NoPredecessors) {
    auto stablePred = wrapPrimeAsStable<Node>(simplePrimePred);
    Node rootNode(0, "root");
    
    auto result = stablePred(rootNode);
    
    // Root node should have empty predecessor list
    EXPECT_EQ(result.size(), 1u);
    EXPECT_TRUE(result[0].empty());
}

TEST_F(WrapPrimeAsStableTest, ChainOfPredecessors) {
    int callCount = 0;
    auto countingPred = [&callCount](const Node& n) {
        callCount++;
        std::vector<Node> preds;
        if (n.id > 0) {
            preds.push_back(Node(n.id - 1));
        }
        return preds;
    };

    auto stablePred = wrapPrimeAsStable<Node>(countingPred);
    Node testNode(10);
    
    auto result = stablePred(testNode);
    
    EXPECT_EQ(callCount, 1);  // Function should be called once
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0][0].id, 9);
}

// Test suite for wrapPrimeAsBundle
class WrapPrimeAsBundleTest : public ::testing::Test {
protected:
    std::function<std::vector<Node>(const Node&)> simplePrimePred = [](const Node& n) {
        std::vector<Node> preds;
        if (n.id > 0) {
            preds.push_back(Node(n.id - 1));
        }
        return preds;
    };

    std::function<std::vector<Node>(const Node&)> multiPrimePred = [](const Node& n) {
        std::vector<Node> preds;
        if (n.id > 0) {
            preds.push_back(Node(n.id - 1));
            if (n.id > 1) {
                preds.push_back(Node(n.id - 2));
            }
        }
        return preds;
    };
};

TEST_F(WrapPrimeAsBundleTest, SinglePredecessorBundle) {
    auto bundlePred = wrapPrimeAsBundle<Node>(simplePrimePred);
    Node testNode(5);
    
    auto result = bundlePred(testNode);
    
    // Result should be a range of bundles (each bundle is a single-element array)
    std::vector<std::array<Node, 1>> bundles(result.begin(), result.end());
    EXPECT_EQ(bundles.size(), 1u);
    EXPECT_EQ(bundles[0][0].id, 4);
}

TEST_F(WrapPrimeAsBundleTest, MultiplePredecessorsAsBundle) {
    auto bundlePred = wrapPrimeAsBundle<Node>(multiPrimePred);
    Node testNode(5);
    
    auto result = bundlePred(testNode);
    
    std::vector<std::array<Node, 1>> bundles(result.begin(), result.end());
    EXPECT_EQ(bundles.size(), 2u);
    EXPECT_EQ(bundles[0][0].id, 4);
    EXPECT_EQ(bundles[1][0].id, 3);
}

TEST_F(WrapPrimeAsBundleTest, NoPredecessorsBundle) {
    auto bundlePred = wrapPrimeAsBundle<Node>(simplePrimePred);
    Node rootNode(0);
    
    auto result = bundlePred(rootNode);
    
    std::vector<std::array<Node, 1>> bundles(result.begin(), result.end());
    EXPECT_TRUE(bundles.empty());
}

// Test suite for wrapBundleAsStable
class WrapBundleAsStableTest : public ::testing::Test {
protected:
    // Bundle function: returns bundles of prerequisites (AND-of-ORs)
    std::function<std::vector<std::vector<Node>>(const Node&)> simpleBundleFunc = [](const Node& n) {
        std::vector<std::vector<Node>> bundles;
        if (n.id > 0) {
            // Single bundle with single alternative
            bundles.push_back({Node(n.id - 1)});
        }
        return bundles;
    };

    // Bundle function with multiple alternatives in one bundle
    std::function<std::vector<std::vector<Node>>(const Node&)> multiAlternativeBundleFunc = [](const Node& n) {
        std::vector<std::vector<Node>> bundles;
        if (n.id > 0) {
            // One bundle with multiple alternatives
            bundles.push_back({Node(n.id - 1), Node(n.id - 2)});
        }
        return bundles;
    };

    // Bundle function with multiple bundles
    std::function<std::vector<std::vector<Node>>(const Node&)> multiBundle = [](const Node& n) {
        std::vector<std::vector<Node>> bundles;
        if (n.id > 2) {
            bundles.push_back({Node(n.id - 1)});
            bundles.push_back({Node(n.id - 2)});
        }
        return bundles;
    };

    // Complex case: multiple bundles with multiple alternatives
    std::function<std::vector<std::vector<Node>>(const Node&)> complexBundleFunc = [](const Node& n) {
        std::vector<std::vector<Node>> bundles;
        if (n.id > 2) {
            bundles.push_back({Node(n.id - 1), Node(n.id - 2)});
            bundles.push_back({Node(n.id - 3)});
        }
        return bundles;
    };
};

TEST_F(WrapBundleAsStableTest, SimpleSingleBundle) {
    auto stablePred = wrapBundleAsStable<Node>(simpleBundleFunc);
    Node testNode(5);
    
    auto result = stablePred(testNode);
    
    // Should convert to single history
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].size(), 1u);
    EXPECT_EQ(result[0][0].id, 4);
}

TEST_F(WrapBundleAsStableTest, MultiAlternativesInBundle) {
    auto stablePred = wrapBundleAsStable<Node>(multiAlternativeBundleFunc);
    Node testNode(5);
    
    auto result = stablePred(testNode);
    
    // Should convert alternatives to separate histories
    EXPECT_EQ(result.size(), 2u);
    
    // First history with Node(4)
    EXPECT_EQ(result[0].size(), 1u);
    EXPECT_EQ(result[0][0].id, 4);
    
    // Second history with Node(3)
    EXPECT_EQ(result[1].size(), 1u);
    EXPECT_EQ(result[1][0].id, 3);
}

TEST_F(WrapBundleAsStableTest, MultipleBundles) {
    auto stablePred = wrapBundleAsStable<Node>(multiBundle);
    Node testNode(5);
    
    auto result = stablePred(testNode);
    
    // Should create Cartesian product: 1 * 1 = 1 history
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].size(), 2u);
    EXPECT_EQ(result[0][0].id, 4);
    EXPECT_EQ(result[0][1].id, 3);
}

TEST_F(WrapBundleAsStableTest, ComplexConversion) {
    auto stablePred = wrapBundleAsStable<Node>(complexBundleFunc);
    Node testNode(5);
    
    auto result = stablePred(testNode);
    
    // Cartesian product: 2 alternatives in first bundle * 1 in second = 2 histories
    EXPECT_EQ(result.size(), 2u);
    
    // First history: (4, 3)
    EXPECT_EQ(result[0].size(), 2u);
    EXPECT_EQ(result[0][0].id, 4);
    EXPECT_EQ(result[0][1].id, 2);
    
    // Second history: (3, 2)
    EXPECT_EQ(result[1].size(), 2u);
    EXPECT_EQ(result[1][0].id, 3);
    EXPECT_EQ(result[1][1].id, 2);
}

TEST_F(WrapBundleAsStableTest, RootNode) {
    auto stablePred = wrapBundleAsStable<Node>(simpleBundleFunc);
    Node rootNode(0);
    
    auto result = stablePred(rootNode);
    
    // Root node: no bundles, should return vector with one empty vector
    EXPECT_EQ(result.size(), 1u);
    EXPECT_TRUE(result[0].empty());
}

TEST_F(WrapBundleAsStableTest, EmptyBundles) {
    auto emptyBundleFunc = [](const Node&) {
        return std::vector<std::vector<Node>>{};
    };
    
    auto stablePred = wrapBundleAsStable<Node>(emptyBundleFunc);
    Node testNode(5);
    
    auto result = stablePred(testNode);
    
    // Empty bundles should produce root-like result
    EXPECT_EQ(result.size(), 1u);
    EXPECT_TRUE(result[0].empty());
}

TEST_F(WrapBundleAsStableTest, LargeCartesianProduct) {
    auto largeBundleFunc = [](const Node&) {
        std::vector<std::vector<Node>> bundles;
        // First bundle: 3 alternatives
        bundles.push_back({Node(1), Node(2), Node(3)});
        // Second bundle: 2 alternatives
        bundles.push_back({Node(4), Node(5)});
        return bundles;
    };
    
    auto stablePred = wrapBundleAsStable<Node>(largeBundleFunc);
    Node testNode(10);
    
    auto result = stablePred(testNode);
    
    // 3 * 2 = 6 total histories
    EXPECT_EQ(result.size(), 6u);
    
    // Verify each history has size 2
    for (const auto& history : result) {
        EXPECT_EQ(history.size(), 2u);
    }
}

// Integration tests
class ConversionIntegrationTest : public ::testing::Test {
protected:
    std::function<std::vector<Node>(const Node&)> primePredFunc = [](const Node& n) {
        std::vector<Node> preds;
        if (n.id == 1) preds.push_back(Node(0));
        else if (n.id == 2) preds.push_back(Node(1));
        else if (n.id == 3) {
            preds.push_back(Node(1));
            preds.push_back(Node(2));
        }
        return preds;
    };
};

TEST_F(ConversionIntegrationTest, PrimeToStableConversion) {
    auto stablePred = wrapPrimeAsStable<Node>(primePredFunc);
    
    Node node0(0);
    Node node1(1);
    Node node3(3);
    
    auto result0 = stablePred(node0);
    auto result1 = stablePred(node1);
    auto result3 = stablePred(node3);
    
    EXPECT_EQ(result0.size(), 1u);
    EXPECT_TRUE(result0[0].empty());
    
    EXPECT_EQ(result1.size(), 1u);
    EXPECT_EQ(result1[0][0].id, 0);
    
    EXPECT_EQ(result3.size(), 1u);
    EXPECT_EQ(result3[0].size(), 2u);
}

TEST_F(ConversionIntegrationTest, PrimeToBundleConversion) {
    auto bundlePred = wrapPrimeAsBundle<Node>(primePredFunc);
    
    Node node3(3);
    auto result = bundlePred(node3);
    
    std::vector<std::array<Node, 1>> bundles(result.begin(), result.end());
    EXPECT_EQ(bundles.size(), 2u);
}
