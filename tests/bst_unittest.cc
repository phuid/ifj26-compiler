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
bool failNextCopy = false;
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
  return (leftValue > rightValue) - (leftValue < rightValue);
}

void *copyInt(const void *value) {
  if (failNextCopy) {
    failNextCopy = false;
    return nullptr;
  }

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
  const void *result = nullptr;
  return bstSearch(tree, &value, &result) == BST_OK;
}

BST *createTree() {
  BST *tree = nullptr;
  EXPECT_EQ(bstCreate(compareInts, copyInt, destroyInt, &tree), BST_OK);
  return tree;
}

BST_STATUS insertValue(BST *tree, const void *value) {
  const BST_STATUS status = bstInsert(tree, value);
  EXPECT_TRUE(status == BST_OK || status == BST_VALUE_OVERWRITTEN);
  return status;
}

const void *searchValue(const BST *tree, const void *value) {
  const void *result = nullptr;
  const BST_STATUS status = bstSearch(tree, value, &result);
  EXPECT_TRUE(status == BST_OK || status == BST_NOT_FOUND);
  EXPECT_EQ(status == BST_OK, result != nullptr);
  return result;
}

BST_STATUS removeValue(BST *tree, const void *value) {
  const BST_STATUS status = bstRemove(tree, value);
  EXPECT_TRUE(status == BST_OK || status == BST_NOT_FOUND);
  return status;
}

void destroyTree(BST **tree) { EXPECT_EQ(bstDestroy(tree), BST_OK); }

class BSTTest : public ::testing::Test {
protected:
  void SetUp() override {
    copiedValues = 0;
    destroyedValues = 0;
    failNextCopy = false;
    ownedValues.clear();
    tree = createTree();
    ASSERT_NE(tree, nullptr);
  }

  void TearDown() override {
    if (tree != nullptr) {
      destroyTree(&tree);
    }
    for (void *value : ownedValues) {
      free(value);
    }
    ownedValues.clear();
  }

  BST *tree = nullptr;
};

} // namespace

TEST_F(BSTTest, FailedCopyDuringInsertPreservesExistingTree) {
  for (int value : {20, 10, 30}) {
    insertValue(tree, &value);
  }
  const int failedValue = 5;
  failNextCopy = true;

  EXPECT_EQ(bstInsert(tree, &failedValue), BST_ALLOCATION_FAIL);

  for (int value : {10, 20, 30}) {
    EXPECT_NE(searchValue(tree, &value), nullptr);
  }
  EXPECT_EQ(searchValue(tree, &failedValue), nullptr);
  expectValidAVL(tree);
}

TEST_F(BSTTest, FailedCopyDuringTwoChildRemovalPreservesExistingTree) {
  for (int value : {20, 10, 30, 25, 35}) {
    insertValue(tree, &value);
  }
  const int removedValue = 20;
  failNextCopy = true;

  EXPECT_EQ(bstRemove(tree, &removedValue), BST_ALLOCATION_FAIL);

  for (int value : {10, 20, 25, 30, 35}) {
    EXPECT_NE(searchValue(tree, &value), nullptr);
  }
  EXPECT_EQ(destroyedValues, 0);
  expectValidAVL(tree);
}

TEST_F(BSTTest, EmptyTreeSearchReturnsNull) {
  const int value = 42;
  EXPECT_EQ(searchValue(tree, &value), nullptr);
}

TEST_F(BSTTest, InsertCopiesValuesAndSearchFindsThem) {
  const int inputValues[] = {12, 4, 19, 2, 8, 16, 25};
  for (int value : inputValues) {
    insertValue(tree, &value);
  }

  EXPECT_EQ(copiedValues, 7);
  for (int value : inputValues) {
    const void *result = searchValue(tree, &value);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*static_cast<const int *>(result), value);
  }

  const int missingValue = 100;
  EXPECT_EQ(searchValue(tree, &missingValue), nullptr);
}

TEST_F(BSTTest, InsertedValueIsIndependentOfCallerStorage) {
  int value = 17;
  insertValue(tree, &value);
  value = 99;

  const int originalValue = 17;
  const int changedValue = 99;
  const void *result = searchValue(tree, &originalValue);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*static_cast<const int *>(result), originalValue);
  EXPECT_EQ(searchValue(tree, &changedValue), nullptr);
}

TEST_F(BSTTest, DuplicateInsertReplacesStoredValue) {
  const int value = 5;
  EXPECT_EQ(insertValue(tree, &value), BST_OK);
  EXPECT_EQ(insertValue(tree, &value), BST_VALUE_OVERWRITTEN);

  EXPECT_EQ(copiedValues, 2);
  EXPECT_EQ(destroyedValues, 1);
  ASSERT_NE(searchValue(tree, &value), nullptr);
  EXPECT_EQ(*static_cast<const int *>(searchValue(tree, &value)), value);
}

TEST_F(BSTTest, MissingValueDoesNotChangeTree) {
  const int values[] = {20, 10, 30};
  for (int value : values) {
    insertValue(tree, &value);
  }

  const int missingValue = 99;
  EXPECT_EQ(searchValue(tree, &missingValue), nullptr);
  for (int value : values) {
    EXPECT_NE(searchValue(tree, &value), nullptr);
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
      insertValue(tree, &value);
    }
    for (int value : sequence) {
      const void *result = searchValue(tree, &value);
      ASSERT_NE(result, nullptr);
      EXPECT_EQ(*static_cast<const int *>(result), value);
    }
    destroyTree(&tree);
    tree = createTree();
    ASSERT_NE(tree, nullptr);
  }
}

TEST_F(BSTTest, OrderedInsertionsRemainSearchable) {
  for (int value = 0; value < 100; ++value) {
    insertValue(tree, &value);
  }

  for (int value = 0; value < 100; ++value) {
    const void *result = searchValue(tree, &value);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*static_cast<const int *>(result), value);
  }
}

TEST_F(BSTTest, DestroyReleasesAllStoredValuesAndClearsHandle) {
  for (int value = 0; value < 5; ++value) {
    insertValue(tree, &value);
  }

  destroyTree(&tree);

  EXPECT_EQ(tree, nullptr);
  EXPECT_EQ(destroyedValues, 5);
}

TEST_F(BSTTest, RemovingMissingValuePreservesAllValues) {
  const int values[] = {20, 10, 30};
  for (int value : values) {
    insertValue(tree, &value);
  }

  const int missingValue = 99;
  EXPECT_EQ(removeValue(tree, &missingValue), BST_NOT_FOUND);

  for (int value : values) {
    EXPECT_NE(searchValue(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, RemovingLeafPreservesOtherValues) {
  const int values[] = {20, 10, 30};
  for (int value : values) {
    insertValue(tree, &value);
  }

  const int removedValue = 10;
  removeValue(tree, &removedValue);

  EXPECT_EQ(searchValue(tree, &removedValue), nullptr);
  EXPECT_TRUE(containsValue(tree, 20));
  EXPECT_TRUE(containsValue(tree, 30));
}

TEST_F(BSTTest, RemovingNodeWithOnlyRightChildPreservesChild) {
  const int values[] = {20, 10, 30, 25};
  for (int value : values) {
    insertValue(tree, &value);
  }

  const int removedValue = 30;
  removeValue(tree, &removedValue);

  EXPECT_EQ(searchValue(tree, &removedValue), nullptr);
  EXPECT_TRUE(containsValue(tree, 25));
  EXPECT_TRUE(containsValue(tree, 20));
  EXPECT_TRUE(containsValue(tree, 10));
}

TEST_F(BSTTest, RemovingNodeWithTwoChildrenPreservesSuccessorAndSubtrees) {
  const int values[] = {20, 10, 30, 25, 35};
  for (int value : values) {
    insertValue(tree, &value);
  }

  const int removedValue = 20;
  removeValue(tree, &removedValue);

  EXPECT_EQ(searchValue(tree, &removedValue), nullptr);
  for (int value : {10, 30, 25, 35}) {
    EXPECT_NE(searchValue(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, RemovingLeafFromBalancedTreePreservesRemainingValues) {
  const int values[] = {4, 2, 6, 1, 3, 5, 7};
  for (int value : values) {
    insertValue(tree, &value);
  }

  const int removedValue = 7;
  removeValue(tree, &removedValue);

  EXPECT_EQ(searchValue(tree, &removedValue), nullptr);
  for (int value : {4, 2, 6, 1, 3, 5}) {
    EXPECT_NE(searchValue(tree, &value), nullptr);
  }
}

#include <random>
#include <set>

// Repeated removals must preserve every value that remains in the tree.
TEST_F(BSTTest, RemovingValuesInAscendingOrderPreservesRemainingValues) {
  std::set<int> expected;

  for (int value = 0; value < 100; ++value) {
    insertValue(tree, &value);
    expected.insert(value);
  }

  for (int value = 0; value < 100; ++value) {
    removeValue(tree, &value);
    expected.erase(value);

    for (int remaining : expected) {
      ASSERT_NE(searchValue(tree, &remaining), nullptr)
          << "Missing value " << remaining << " after removing " << value;
    }

    EXPECT_EQ(searchValue(tree, &value), nullptr);
  }

  EXPECT_EQ(destroyedValues, 100);
}

TEST_F(BSTTest, RemovingValuesInDescendingOrderPreservesRemainingValues) {
  std::set<int> expected;

  for (int value = 0; value < 100; ++value) {
    insertValue(tree, &value);
    expected.insert(value);
  }

  for (int value = 99; value >= 0; --value) {
    removeValue(tree, &value);
    expected.erase(value);

    for (int remaining : expected) {
      ASSERT_NE(searchValue(tree, &remaining), nullptr)
          << "Missing value " << remaining << " after removing " << value;
    }

    EXPECT_EQ(searchValue(tree, &value), nullptr);
  }

  EXPECT_EQ(destroyedValues, 100);
}

TEST_F(BSTTest, RemovingRootWithOnlyLeftChildPreservesChild) {
  const int values[] = {20, 10};

  for (int value : values) {
    insertValue(tree, &value);
  }

  const int removedValue = 20;
  removeValue(tree, &removedValue);

  EXPECT_EQ(searchValue(tree, &removedValue), nullptr);
  EXPECT_TRUE(containsValue(tree, 10));
  EXPECT_EQ(destroyedValues, 1);
}

TEST_F(BSTTest, RemovingRootWithOnlyRightChildPreservesChild) {
  const int values[] = {20, 30};

  for (int value : values) {
    insertValue(tree, &value);
  }

  const int removedValue = 20;
  removeValue(tree, &removedValue);

  EXPECT_EQ(searchValue(tree, &removedValue), nullptr);
  EXPECT_TRUE(containsValue(tree, 30));
  EXPECT_EQ(destroyedValues, 1);
}

TEST_F(BSTTest, RemovingRootWithTwoChildrenDestroysExactlyOneValue) {
  const int values[] = {20, 10, 30, 25, 35};

  for (int value : values) {
    insertValue(tree, &value);
  }

  const int removedValue = 20;
  removeValue(tree, &removedValue);

  EXPECT_EQ(searchValue(tree, &removedValue), nullptr);
  EXPECT_EQ(destroyedValues, 2);
  EXPECT_EQ(static_cast<int>(ownedValues.size()), 4);

  for (int value : {10, 25, 30, 35}) {
    EXPECT_NE(searchValue(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, RemovingRootWithTwoChildrenMaintainsSearchability) {
  const int values[] = {40, 20, 60, 10, 30, 50, 70, 45, 55, 42, 47, 43};

  for (int value : values) {
    insertValue(tree, &value);
  }

  const int removedValue = 40;
  removeValue(tree, &removedValue);

  EXPECT_EQ(searchValue(tree, &removedValue), nullptr);

  for (int value : values) {
    if (value != removedValue) {
      EXPECT_NE(searchValue(tree, &value), nullptr)
          << "Missing value " << value;
    }
  }
}

TEST_F(BSTTest, RemovingAllValuesLeavesEmptyTree) {
  for (int value = 0; value < 50; ++value) {
    insertValue(tree, &value);
  }

  for (int value = 0; value < 50; ++value) {
    removeValue(tree, &value);
  }

  for (int value = 0; value < 50; ++value) {
    EXPECT_EQ(searchValue(tree, &value), nullptr);
  }

  EXPECT_EQ(destroyedValues, 50);
  EXPECT_TRUE(ownedValues.empty());
}

TEST_F(BSTTest, RepeatedRemovalDoesNotDestroyValueTwice) {
  const int values[] = {20, 10, 30, 25, 35};

  for (int value : values) {
    insertValue(tree, &value);
  }

  const int removedValue = 10;

  removeValue(tree, &removedValue);
  EXPECT_EQ(destroyedValues, 1);

  removeValue(tree, &removedValue);
  EXPECT_EQ(destroyedValues, 1);

  EXPECT_EQ(searchValue(tree, &removedValue), nullptr);
  EXPECT_EQ(static_cast<int>(ownedValues.size()), 4);
}

TEST_F(BSTTest, DuplicateReplacementPreservesOwnershipCounts) {
  const int value = 42;

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(insertValue(tree, &value),
              i == 0 ? BST_OK : BST_VALUE_OVERWRITTEN);

    EXPECT_EQ(copiedValues, i + 1);
    EXPECT_EQ(destroyedValues, i);
    EXPECT_EQ(static_cast<int>(ownedValues.size()), 1);
  }

  destroyTree(&tree);

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
      const bool isNewValue = expected.insert(value).second;
      EXPECT_EQ(insertValue(tree, &value),
                isNewValue ? BST_OK : BST_VALUE_OVERWRITTEN);
    } else {
      removeValue(tree, &value);
      expected.erase(value);
    }

    for (int candidate = 0; candidate < VALUE_COUNT; ++candidate) {
      const bool shouldExist = expected.find(candidate) != expected.end();
      const bool actuallyExists = searchValue(tree, &candidate) != nullptr;

      EXPECT_EQ(actuallyExists, shouldExist)
          << "Operation " << i << ", candidate " << candidate
          << ", operation value " << value;
    }
  }

  for (int value : expected) {
    EXPECT_NE(searchValue(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, DeletionTriggersLLRotation) {
  // Removing 7, 6, then 5 makes the root left-heavy.
  const int values[] = {4, 2, 6, 1, 3, 5, 7};

  for (int value : values) {
    insertValue(tree, &value);
  }

  expectValidAVL(tree);

  for (int value : {7, 6, 5}) {
    removeValue(tree, &value);
    expectValidAVL(tree);
  }

  for (int value : {1, 2, 3, 4}) {
    EXPECT_NE(searchValue(tree, &value), nullptr);
  }
  const int removedValue = 5;
  EXPECT_EQ(searchValue(tree, &removedValue), nullptr);
}

TEST_F(BSTTest, DeletionTriggersRRRotation) {
  // Mirror of the LL case.
  const int values[] = {4, 2, 6, 1, 3, 5, 7};

  for (int value : values) {
    insertValue(tree, &value);
  }

  expectValidAVL(tree);

  for (int value : {1, 2, 3}) {
    removeValue(tree, &value);
    expectValidAVL(tree);
  }

  for (int value : {4, 5, 6, 7}) {
    EXPECT_NE(searchValue(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, DeletionTriggersLRRotation) {
  // Removing 1 makes node 2 right-heavy.
  // Subsequent removals shrink the root's right subtree.
  const int values[] = {4, 2, 6, 1, 3, 5, 7};

  for (int value : values) {
    insertValue(tree, &value);
  }

  expectValidAVL(tree);

  for (int value : {1, 7, 6, 5}) {
    removeValue(tree, &value);
    expectValidAVL(tree);
  }

  for (int value : {2, 3, 4}) {
    EXPECT_NE(searchValue(tree, &value), nullptr);
  }

  for (int value : {1, 5, 6, 7}) {
    EXPECT_EQ(searchValue(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, DeletionTriggersRLRotation) {
  // Mirror of the LR case.
  const int values[] = {4, 2, 6, 1, 3, 5, 7};

  for (int value : values) {
    insertValue(tree, &value);
  }

  expectValidAVL(tree);

  for (int value : {7, 1, 2, 3}) {
    removeValue(tree, &value);
    expectValidAVL(tree);
  }

  for (int value : {4, 5, 6}) {
    EXPECT_NE(searchValue(tree, &value), nullptr);
  }

  for (int value : {1, 2, 3, 7}) {
    EXPECT_EQ(searchValue(tree, &value), nullptr);
  }
}