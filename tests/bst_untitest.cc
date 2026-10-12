#include "gtest/gtest.h"
#include <algorithm>
#include <vector>

extern "C" {
#include "../src/bst.h"
#include "../src/impl_bst.h"
}

namespace {

int copiedValues = 0;
int destroyedValues = 0;
std::vector<void *> ownedValues;

// Returns the actual subtree height while checking every AVL invariant.
int validateAVLNode(const BST_Node *node, long long minValue,
                    long long maxValue) {
  if (node == nullptr) {
    return 0;
  }

  if (node->value == nullptr) {
    ADD_FAILURE() << "Node has a null value";
    return 0;
  }

  const int value = *static_cast<const int *>(node->value);

  EXPECT_GT(value, minValue) << "BST ordering violation";
  EXPECT_LT(value, maxValue) << "BST ordering violation";

  const int leftHeight = validateAVLNode(node->left, minValue, value);
  const int rightHeight = validateAVLNode(node->right, value, maxValue);

  const int actualHeight = 1 + std::max(leftHeight, rightHeight);
  const int actualBalance = rightHeight - leftHeight;

  EXPECT_EQ(node->height, actualHeight)
      << "Incorrect height at value " << value;

  EXPECT_EQ(node->balance, actualBalance)
      << "Incorrect balance factor at value " << value;

  EXPECT_LE(std::abs(actualBalance), 1)
      << "AVL balance violation at value " << value;

  return actualHeight;
}

void expectValidAVL(const BST *tree) {
  ASSERT_NE(tree, nullptr);

  validateAVLNode(tree->root, std::numeric_limits<long long>::min(),
                  std::numeric_limits<long long>::max());
}

int compareInts(const void *left, const void *right) {
  const int leftValue = *static_cast<const int *>(left);
  const int rightValue = *static_cast<const int *>(right);
  return rightValue - leftValue;
}

void *copyInt(const void *value) {
  int *copy = static_cast<int *>(malloc(sizeof(int)));
  if (copy != nullptr) {
    *copy = *static_cast<const int *>(value);
    ++copiedValues;
    ownedValues.push_back(copy);
  }
  return copy;
}

void destroyInt(void *value) {
  const auto ownedValue =
      std::find(ownedValues.begin(), ownedValues.end(), value);
  if (ownedValue != ownedValues.end()) {
    ++destroyedValues;
    ownedValues.erase(ownedValue);
    free(value);
  }
}

bool containsValue(const BST *tree, int value) {
  return bstSearch(tree, &value) != nullptr;
}

class BSTTest : public ::testing::Test {
protected:
  void SetUp() override {
    copiedValues = 0;
    destroyedValues = 0;
    ownedValues.clear();
    tree = bstCreate(compareInts, copyInt, destroyInt);
    ASSERT_NE(tree, nullptr);
  }

  void TearDown() override {
    if (tree != nullptr) {
      bstDestroy(&tree);
    }
    for (void *value : ownedValues) {
      free(value);
    }
    ownedValues.clear();
  }

  BST *tree = nullptr;
};

} // namespace

TEST_F(BSTTest, EmptyTreeSearchReturnsNull) {
  const int value = 42;
  EXPECT_EQ(bstSearch(tree, &value), nullptr);
}

TEST_F(BSTTest, InsertCopiesValuesAndSearchFindsThem) {
  const int inputValues[] = {12, 4, 19, 2, 8, 16, 25};
  for (int value : inputValues) {
    bstInsert(tree, &value);
  }

  EXPECT_EQ(copiedValues, 7);
  for (int value : inputValues) {
    void *result = bstSearch(tree, &value);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*static_cast<int *>(result), value);
  }

  const int missingValue = 100;
  EXPECT_EQ(bstSearch(tree, &missingValue), nullptr);
}

TEST_F(BSTTest, InsertedValueIsIndependentOfCallerStorage) {
  int value = 17;
  bstInsert(tree, &value);
  value = 99;

  const int originalValue = 17;
  const int changedValue = 99;
  void *result = bstSearch(tree, &originalValue);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*static_cast<int *>(result), originalValue);
  EXPECT_EQ(bstSearch(tree, &changedValue), nullptr);
}

TEST_F(BSTTest, DuplicateInsertReplacesStoredValue) {
  const int value = 5;
  bstInsert(tree, &value);
  bstInsert(tree, &value);

  EXPECT_EQ(copiedValues, 2);
  EXPECT_EQ(destroyedValues, 1);
  ASSERT_NE(bstSearch(tree, &value), nullptr);
  EXPECT_EQ(*static_cast<int *>(bstSearch(tree, &value)), value);
}

TEST_F(BSTTest, MissingValueDoesNotChangeTree) {
  const int values[] = {20, 10, 30};
  for (int value : values) {
    bstInsert(tree, &value);
  }

  const int missingValue = 99;
  EXPECT_EQ(bstSearch(tree, &missingValue), nullptr);
  for (int value : values) {
    EXPECT_NE(bstSearch(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, AllFourInsertionRotationsPreserveSearchResults) {
  const std::vector<std::vector<int>> sequences = {
      {30, 20, 10}, // left-left
      {10, 20, 30}, // right-right
      {30, 10, 20}, // left-right
      {10, 30, 20}, // right-left
  };

  for (const std::vector<int> &sequence : sequences) {
    SCOPED_TRACE(sequence.front());
    for (int value : sequence) {
      bstInsert(tree, &value);
    }
    for (int value : sequence) {
      void *result = bstSearch(tree, &value);
      ASSERT_NE(result, nullptr);
      EXPECT_EQ(*static_cast<int *>(result), value);
    }
    bstDestroy(&tree);
    tree = bstCreate(compareInts, copyInt, destroyInt);
    ASSERT_NE(tree, nullptr);
  }
}

TEST_F(BSTTest, OrderedInsertionsRemainSearchable) {
  for (int value = 0; value < 100; ++value) {
    bstInsert(tree, &value);
  }

  for (int value = 0; value < 100; ++value) {
    void *result = bstSearch(tree, &value);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*static_cast<int *>(result), value);
  }
}

TEST_F(BSTTest, DestroyReleasesAllStoredValuesAndClearsHandle) {
  for (int value = 0; value < 5; ++value) {
    bstInsert(tree, &value);
  }

  bstDestroy(&tree);

  EXPECT_EQ(tree, nullptr);
  EXPECT_EQ(destroyedValues, 5);
}

TEST_F(BSTTest, RemovingMissingValuePreservesAllValues) {
  const int values[] = {20, 10, 30};
  for (int value : values) {
    bstInsert(tree, &value);
  }

  const int missingValue = 99;
  bstRemove(tree, &missingValue);

  for (int value : values) {
    EXPECT_NE(bstSearch(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, RemovingLeafPreservesOtherValues) {
  const int values[] = {20, 10, 30};
  for (int value : values) {
    bstInsert(tree, &value);
  }

  const int removedValue = 10;
  bstRemove(tree, &removedValue);

  EXPECT_EQ(bstSearch(tree, &removedValue), nullptr);
  EXPECT_TRUE(containsValue(tree, 20));
  EXPECT_TRUE(containsValue(tree, 30));
}

TEST_F(BSTTest, RemovingNodeWithOnlyRightChildPreservesChild) {
  const int values[] = {20, 10, 30, 25};
  for (int value : values) {
    bstInsert(tree, &value);
  }

  const int removedValue = 30;
  bstRemove(tree, &removedValue);

  EXPECT_EQ(bstSearch(tree, &removedValue), nullptr);
  EXPECT_TRUE(containsValue(tree, 25));
  EXPECT_TRUE(containsValue(tree, 20));
  EXPECT_TRUE(containsValue(tree, 10));
}

TEST_F(BSTTest, RemovingNodeWithTwoChildrenPreservesSuccessorAndSubtrees) {
  const int values[] = {20, 10, 30, 25, 35};
  for (int value : values) {
    bstInsert(tree, &value);
  }

  const int removedValue = 20;
  bstRemove(tree, &removedValue);

  EXPECT_EQ(bstSearch(tree, &removedValue), nullptr);
  for (int value : {10, 30, 25, 35}) {
    EXPECT_NE(bstSearch(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, RemovingLeafFromBalancedTreePreservesRemainingValues) {
  const int values[] = {4, 2, 6, 1, 3, 5, 7};
  for (int value : values) {
    bstInsert(tree, &value);
  }

  const int removedValue = 7;
  bstRemove(tree, &removedValue);

  EXPECT_EQ(bstSearch(tree, &removedValue), nullptr);
  for (int value : {4, 2, 6, 1, 3, 5}) {
    EXPECT_NE(bstSearch(tree, &value), nullptr);
  }
}

#include <random>
#include <set>

// Repeated removals must preserve every value that remains in the tree.
TEST_F(BSTTest, RemovingValuesInAscendingOrderPreservesRemainingValues) {
  std::set<int> expected;

  for (int value = 0; value < 100; ++value) {
    bstInsert(tree, &value);
    expected.insert(value);
  }

  for (int value = 0; value < 100; ++value) {
    bstRemove(tree, &value);
    expected.erase(value);

    for (int remaining : expected) {
      ASSERT_NE(bstSearch(tree, &remaining), nullptr)
          << "Missing value " << remaining << " after removing " << value;
    }

    EXPECT_EQ(bstSearch(tree, &value), nullptr);
  }

  EXPECT_EQ(destroyedValues, 100);
}

TEST_F(BSTTest, RemovingValuesInDescendingOrderPreservesRemainingValues) {
  std::set<int> expected;

  for (int value = 0; value < 100; ++value) {
    bstInsert(tree, &value);
    expected.insert(value);
  }

  for (int value = 99; value >= 0; --value) {
    bstRemove(tree, &value);
    expected.erase(value);

    for (int remaining : expected) {
      ASSERT_NE(bstSearch(tree, &remaining), nullptr)
          << "Missing value " << remaining << " after removing " << value;
    }

    EXPECT_EQ(bstSearch(tree, &value), nullptr);
  }

  EXPECT_EQ(destroyedValues, 100);
}

TEST_F(BSTTest, RemovingRootWithOnlyLeftChildPreservesChild) {
  const int values[] = {20, 10};

  for (int value : values) {
    bstInsert(tree, &value);
  }

  const int removedValue = 20;
  bstRemove(tree, &removedValue);

  EXPECT_EQ(bstSearch(tree, &removedValue), nullptr);
  EXPECT_TRUE(containsValue(tree, 10));
  EXPECT_EQ(destroyedValues, 1);
}

TEST_F(BSTTest, RemovingRootWithOnlyRightChildPreservesChild) {
  const int values[] = {20, 30};

  for (int value : values) {
    bstInsert(tree, &value);
  }

  const int removedValue = 20;
  bstRemove(tree, &removedValue);

  EXPECT_EQ(bstSearch(tree, &removedValue), nullptr);
  EXPECT_TRUE(containsValue(tree, 30));
  EXPECT_EQ(destroyedValues, 1);
}

TEST_F(BSTTest, RemovingRootWithTwoChildrenDestroysExactlyOneValue) {
  const int values[] = {20, 10, 30, 25, 35};

  for (int value : values) {
    bstInsert(tree, &value);
  }

  const int removedValue = 20;
  bstRemove(tree, &removedValue);

  EXPECT_EQ(bstSearch(tree, &removedValue), nullptr);
  EXPECT_EQ(destroyedValues, 2);
  EXPECT_EQ(static_cast<int>(ownedValues.size()), 4);

  for (int value : {10, 25, 30, 35}) {
    EXPECT_NE(bstSearch(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, RemovingRootWithTwoChildrenMaintainsSearchability) {
  const int values[] = {40, 20, 60, 10, 30, 50, 70, 45, 55, 42, 47, 43};

  for (int value : values) {
    bstInsert(tree, &value);
  }

  const int removedValue = 40;
  bstRemove(tree, &removedValue);

  EXPECT_EQ(bstSearch(tree, &removedValue), nullptr);

  for (int value : values) {
    if (value != removedValue) {
      EXPECT_NE(bstSearch(tree, &value), nullptr) << "Missing value " << value;
    }
  }
}

TEST_F(BSTTest, RemovingAllValuesLeavesEmptyTree) {
  for (int value = 0; value < 50; ++value) {
    bstInsert(tree, &value);
  }

  for (int value = 0; value < 50; ++value) {
    bstRemove(tree, &value);
  }

  for (int value = 0; value < 50; ++value) {
    EXPECT_EQ(bstSearch(tree, &value), nullptr);
  }

  EXPECT_EQ(destroyedValues, 50);
  EXPECT_TRUE(ownedValues.empty());
}

TEST_F(BSTTest, RepeatedRemovalDoesNotDestroyValueTwice) {
  const int values[] = {20, 10, 30, 25, 35};

  for (int value : values) {
    bstInsert(tree, &value);
  }

  const int removedValue = 10;

  bstRemove(tree, &removedValue);
  EXPECT_EQ(destroyedValues, 1);

  bstRemove(tree, &removedValue);
  EXPECT_EQ(destroyedValues, 1);

  EXPECT_EQ(bstSearch(tree, &removedValue), nullptr);
  EXPECT_EQ(static_cast<int>(ownedValues.size()), 4);
}

TEST_F(BSTTest, DuplicateReplacementPreservesOwnershipCounts) {
  const int value = 42;

  for (int i = 0; i < 10; ++i) {
    bstInsert(tree, &value);

    EXPECT_EQ(copiedValues, i + 1);
    EXPECT_EQ(destroyedValues, i);
    EXPECT_EQ(static_cast<int>(ownedValues.size()), 1);
  }

  bstDestroy(&tree);

  EXPECT_EQ(destroyedValues, 10);
  EXPECT_TRUE(ownedValues.empty());
}

TEST_F(BSTTest, RandomizedInsertionsAndRemovalsMatchReferenceSet) {
  constexpr int VALUE_COUNT = 200;
  constexpr int OPERATIONS = 2000;

  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> valueDistribution(0, VALUE_COUNT - 1);
  std::bernoulli_distribution insertDistribution(0.5);

  std::set<int> expected;

  for (int i = 0; i < OPERATIONS; ++i) {
    const int value = valueDistribution(rng);

    if (insertDistribution(rng)) {
      bstInsert(tree, &value);
      expected.insert(value);
    } else {
      bstRemove(tree, &value);
      expected.erase(value);
    }

    for (int candidate = 0; candidate < VALUE_COUNT; ++candidate) {
      const bool shouldExist = expected.find(candidate) != expected.end();
      const bool actuallyExists = bstSearch(tree, &candidate) != nullptr;

      EXPECT_EQ(actuallyExists, shouldExist)
          << "Operation " << i << ", candidate " << candidate
          << ", operation value " << value;
    }
  }

  for (int value : expected) {
    EXPECT_NE(bstSearch(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, DeletionTriggersLLRotation) {
  // Removing 7, 6, then 5 makes the root left-heavy.
  const int values[] = {4, 2, 6, 1, 3, 5, 7};

  for (int value : values) {
    bstInsert(tree, &value);
  }

  expectValidAVL(tree);

  for (int value : {7, 6, 5}) {
    bstRemove(tree, &value);
    expectValidAVL(tree);
  }

  for (int value : {1, 2, 3, 4}) {
    EXPECT_NE(bstSearch(tree, &value), nullptr);
  }
  const int removedValue = 5;
  EXPECT_EQ(bstSearch(tree, &removedValue), nullptr);
}

TEST_F(BSTTest, DeletionTriggersRRRotation) {
  // Mirror of the LL case.
  const int values[] = {4, 2, 6, 1, 3, 5, 7};

  for (int value : values) {
    bstInsert(tree, &value);
  }

  expectValidAVL(tree);

  for (int value : {1, 2, 3}) {
    bstRemove(tree, &value);
    expectValidAVL(tree);
  }

  for (int value : {4, 5, 6, 7}) {
    EXPECT_NE(bstSearch(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, DeletionTriggersLRRotation) {
  // Removing 1 makes node 2 right-heavy.
  // Subsequent removals shrink the root's right subtree.
  const int values[] = {4, 2, 6, 1, 3, 5, 7};

  for (int value : values) {
    bstInsert(tree, &value);
  }

  expectValidAVL(tree);

  for (int value : {1, 7, 6, 5}) {
    bstRemove(tree, &value);
    expectValidAVL(tree);
  }

  for (int value : {2, 3, 4}) {
    EXPECT_NE(bstSearch(tree, &value), nullptr);
  }

  for (int value : {1, 5, 6, 7}) {
    EXPECT_EQ(bstSearch(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, DeletionTriggersRLRotation) {
  // Mirror of the LR case.
  const int values[] = {4, 2, 6, 1, 3, 5, 7};

  for (int value : values) {
    bstInsert(tree, &value);
  }

  expectValidAVL(tree);

  for (int value : {7, 1, 2, 3}) {
    bstRemove(tree, &value);
    expectValidAVL(tree);
  }

  for (int value : {4, 5, 6}) {
    EXPECT_NE(bstSearch(tree, &value), nullptr);
  }

  for (int value : {1, 2, 3, 7}) {
    EXPECT_EQ(bstSearch(tree, &value), nullptr);
  }
}