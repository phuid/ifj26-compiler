#include "gtest/gtest.h"
#include <algorithm>
#include <vector>

extern "C" {
#include "../src/bst.h"
#include "../src/impl_bst.h"
}

namespace {

int copied_values = 0;
int destroyed_values = 0;
bool fail_next_copy = false;
std::vector<void *> owned_values;

// Returns the actual subtree height while checking every AVL invariant.
int validate_avl_node(const BST_Node *node, long long min_value,
                      long long max_value) {
  if (node == nullptr) {
    return 0;
  }

  if (node->value == nullptr) {
    ADD_FAILURE() << "Node has a null value";
    return 0;
  }

  const int value = *static_cast<const int *>(node->value);

  EXPECT_GT(value, min_value) << "BST ordering violation";
  EXPECT_LT(value, max_value) << "BST ordering violation";

  const int left_height = validate_avl_node(node->left, min_value, value);
  const int right_height = validate_avl_node(node->right, value, max_value);

  const int actual_height = 1 + std::max(left_height, right_height);
  const int actual_balance = right_height - left_height;

  EXPECT_EQ(node->height, actual_height)
      << "Incorrect height at value " << value;

  EXPECT_LE(std::abs(actual_balance), 1)
      << "AVL balance violation at value " << value;

  return actual_height;
}

void expect_valid_avl(const BST *tree) {
  ASSERT_NE(tree, nullptr);

  validate_avl_node(tree->root, std::numeric_limits<long long>::min(),
                    std::numeric_limits<long long>::max());
}

int compare_ints(const void *left, const void *right) {
  const int left_value = *static_cast<const int *>(left);
  const int right_value = *static_cast<const int *>(right);
  return (left_value > right_value) - (left_value < right_value);
}

void *copy_int(const void *value) {
  if (fail_next_copy) {
    fail_next_copy = false;
    return nullptr;
  }

  int *copy = static_cast<int *>(malloc(sizeof(int)));
  if (copy != nullptr) {
    *copy = *static_cast<const int *>(value);
    ++copied_values;
    owned_values.push_back(copy);
  }
  return copy;
}

void destroy_int(void *value) {
  const auto owned_value =
      std::find(owned_values.begin(), owned_values.end(), value);
  if (owned_value != owned_values.end()) {
    ++destroyed_values;
    owned_values.erase(owned_value);
    free(value);
  }
}

bool contains_value(const BST *tree, int value) {
  const void *result = nullptr;
  return bst_search(tree, &value, &result) == BST_OK;
}

BST *create_tree() {
  BST *tree = nullptr;
  EXPECT_EQ(bst_create(compare_ints, copy_int, destroy_int, &tree), BST_OK);
  return tree;
}

BST_STATUS insert_value(BST *tree, const void *value) {
  const BST_STATUS status = bst_insert(tree, value);
  EXPECT_TRUE(status == BST_OK || status == BST_VALUE_OVERWRITTEN);
  return status;
}

const void *search_value(const BST *tree, const void *value) {
  const void *result = nullptr;
  const BST_STATUS status = bst_search(tree, value, &result);
  EXPECT_TRUE(status == BST_OK || status == BST_NOT_FOUND);
  EXPECT_EQ(status == BST_OK, result != nullptr);
  return result;
}

BST_STATUS remove_value(BST *tree, const void *value) {
  const BST_STATUS status = bst_remove(tree, value);
  EXPECT_TRUE(status == BST_OK || status == BST_NOT_FOUND);
  return status;
}

void destroy_tree(BST **tree) { EXPECT_EQ(bst_destroy(tree), BST_OK); }

class BSTTest : public ::testing::Test {
protected:
  void SetUp() override {
    copied_values = 0;
    destroyed_values = 0;
    fail_next_copy = false;
    owned_values.clear();
    tree = create_tree();
    ASSERT_NE(tree, nullptr);
  }

  void TearDown() override {
    if (tree != nullptr) {
      destroy_tree(&tree);
    }
    for (void *value : owned_values) {
      free(value);
    }
    owned_values.clear();
  }

  BST *tree = nullptr;
};

} // namespace

TEST_F(BSTTest, FailedCopyDuringInsertPreservesExistingTree) {
  for (int value : {20, 10, 30}) {
    insert_value(tree, &value);
  }
  const int failed_value = 5;
  fail_next_copy = true;

  EXPECT_EQ(bst_insert(tree, &failed_value), BST_ALLOCATION_FAIL);

  for (int value : {10, 20, 30}) {
    EXPECT_NE(search_value(tree, &value), nullptr);
  }
  EXPECT_EQ(search_value(tree, &failed_value), nullptr);
  expect_valid_avl(tree);
}

TEST_F(BSTTest, FailedCopyDuringTwoChildRemovalPreservesExistingTree) {
  for (int value : {20, 10, 30, 25, 35}) {
    insert_value(tree, &value);
  }
  const int removed_value = 20;
  fail_next_copy = true;

  EXPECT_EQ(bst_remove(tree, &removed_value), BST_ALLOCATION_FAIL);

  for (int value : {10, 20, 25, 30, 35}) {
    EXPECT_NE(search_value(tree, &value), nullptr);
  }
  EXPECT_EQ(destroyed_values, 0);
  expect_valid_avl(tree);
}

TEST_F(BSTTest, EmptyTreeSearchReturnsNull) {
  const int value = 42;
  EXPECT_EQ(search_value(tree, &value), nullptr);
}

TEST_F(BSTTest, InsertCopiesValuesAndSearchFindsThem) {
  const int input_values[] = {12, 4, 19, 2, 8, 16, 25};
  for (int value : input_values) {
    insert_value(tree, &value);
  }

  EXPECT_EQ(copied_values, 7);
  for (int value : input_values) {
    const void *result = search_value(tree, &value);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*static_cast<const int *>(result), value);
  }

  const int missing_value = 100;
  EXPECT_EQ(search_value(tree, &missing_value), nullptr);
}

TEST_F(BSTTest, InsertedValueIsIndependentOfCallerStorage) {
  int value = 17;
  insert_value(tree, &value);
  value = 99;

  const int original_value = 17;
  const int changed_value = 99;
  const void *result = search_value(tree, &original_value);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*static_cast<const int *>(result), original_value);
  EXPECT_EQ(search_value(tree, &changed_value), nullptr);
}

TEST_F(BSTTest, DuplicateInsertReplacesStoredValue) {
  const int value = 5;
  EXPECT_EQ(insert_value(tree, &value), BST_OK);
  EXPECT_EQ(insert_value(tree, &value), BST_VALUE_OVERWRITTEN);

  EXPECT_EQ(copied_values, 2);
  EXPECT_EQ(destroyed_values, 1);
  ASSERT_NE(search_value(tree, &value), nullptr);
  EXPECT_EQ(*static_cast<const int *>(search_value(tree, &value)), value);
}

TEST_F(BSTTest, MissingValueDoesNotChangeTree) {
  const int values[] = {20, 10, 30};
  for (int value : values) {
    insert_value(tree, &value);
  }

  const int missing_value = 99;
  EXPECT_EQ(search_value(tree, &missing_value), nullptr);
  for (int value : values) {
    EXPECT_NE(search_value(tree, &value), nullptr);
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
      insert_value(tree, &value);
    }
    for (int value : sequence) {
      const void *result = search_value(tree, &value);
      ASSERT_NE(result, nullptr);
      EXPECT_EQ(*static_cast<const int *>(result), value);
    }
    destroy_tree(&tree);
    tree = create_tree();
    ASSERT_NE(tree, nullptr);
  }
}

TEST_F(BSTTest, OrderedInsertionsRemainSearchable) {
  for (int value = 0; value < 100; ++value) {
    insert_value(tree, &value);
  }

  for (int value = 0; value < 100; ++value) {
    const void *result = search_value(tree, &value);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*static_cast<const int *>(result), value);
  }
}

TEST_F(BSTTest, DestroyReleasesAllStoredValuesAndClearsHandle) {
  for (int value = 0; value < 5; ++value) {
    insert_value(tree, &value);
  }

  destroy_tree(&tree);

  EXPECT_EQ(tree, nullptr);
  EXPECT_EQ(destroyed_values, 5);
}

TEST_F(BSTTest, RemovingMissingValuePreservesAllValues) {
  const int values[] = {20, 10, 30};
  for (int value : values) {
    insert_value(tree, &value);
  }

  const int missing_value = 99;
  EXPECT_EQ(remove_value(tree, &missing_value), BST_NOT_FOUND);

  for (int value : values) {
    EXPECT_NE(search_value(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, RemovingLeafPreservesOtherValues) {
  const int values[] = {20, 10, 30};
  for (int value : values) {
    insert_value(tree, &value);
  }

  const int removed_value = 10;
  remove_value(tree, &removed_value);

  EXPECT_EQ(search_value(tree, &removed_value), nullptr);
  EXPECT_TRUE(contains_value(tree, 20));
  EXPECT_TRUE(contains_value(tree, 30));
}

TEST_F(BSTTest, RemovingNodeWithOnlyRightChildPreservesChild) {
  const int values[] = {20, 10, 30, 25};
  for (int value : values) {
    insert_value(tree, &value);
  }

  const int removed_value = 30;
  remove_value(tree, &removed_value);

  EXPECT_EQ(search_value(tree, &removed_value), nullptr);
  EXPECT_TRUE(contains_value(tree, 25));
  EXPECT_TRUE(contains_value(tree, 20));
  EXPECT_TRUE(contains_value(tree, 10));
}

TEST_F(BSTTest, RemovingNodeWithTwoChildrenPreservesSuccessorAndSubtrees) {
  const int values[] = {20, 10, 30, 25, 35};
  for (int value : values) {
    insert_value(tree, &value);
  }

  const int removed_value = 20;
  remove_value(tree, &removed_value);

  EXPECT_EQ(search_value(tree, &removed_value), nullptr);
  for (int value : {10, 30, 25, 35}) {
    EXPECT_NE(search_value(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, RemovingLeafFromBalancedTreePreservesRemainingValues) {
  const int values[] = {4, 2, 6, 1, 3, 5, 7};
  for (int value : values) {
    insert_value(tree, &value);
  }

  const int removed_value = 7;
  remove_value(tree, &removed_value);

  EXPECT_EQ(search_value(tree, &removed_value), nullptr);
  for (int value : {4, 2, 6, 1, 3, 5}) {
    EXPECT_NE(search_value(tree, &value), nullptr);
  }
}

#include <random>
#include <set>

// Repeated removals must preserve every value that remains in the tree.
TEST_F(BSTTest, RemovingValuesInAscendingOrderPreservesRemainingValues) {
  std::set<int> expected;

  for (int value = 0; value < 100; ++value) {
    insert_value(tree, &value);
    expected.insert(value);
  }

  for (int value = 0; value < 100; ++value) {
    remove_value(tree, &value);
    expected.erase(value);

    for (int remaining : expected) {
      ASSERT_NE(search_value(tree, &remaining), nullptr)
          << "Missing value " << remaining << " after removing " << value;
    }

    EXPECT_EQ(search_value(tree, &value), nullptr);
  }

  EXPECT_EQ(destroyed_values, 100);
}

TEST_F(BSTTest, RemovingValuesInDescendingOrderPreservesRemainingValues) {
  std::set<int> expected;

  for (int value = 0; value < 100; ++value) {
    insert_value(tree, &value);
    expected.insert(value);
  }

  for (int value = 99; value >= 0; --value) {
    remove_value(tree, &value);
    expected.erase(value);

    for (int remaining : expected) {
      ASSERT_NE(search_value(tree, &remaining), nullptr)
          << "Missing value " << remaining << " after removing " << value;
    }

    EXPECT_EQ(search_value(tree, &value), nullptr);
  }

  EXPECT_EQ(destroyed_values, 100);
}

TEST_F(BSTTest, RemovingRootWithOnlyLeftChildPreservesChild) {
  const int values[] = {20, 10};

  for (int value : values) {
    insert_value(tree, &value);
  }

  const int removed_value = 20;
  remove_value(tree, &removed_value);

  EXPECT_EQ(search_value(tree, &removed_value), nullptr);
  EXPECT_TRUE(contains_value(tree, 10));
  EXPECT_EQ(destroyed_values, 1);
}

TEST_F(BSTTest, RemovingRootWithOnlyRightChildPreservesChild) {
  const int values[] = {20, 30};

  for (int value : values) {
    insert_value(tree, &value);
  }

  const int removed_value = 20;
  remove_value(tree, &removed_value);

  EXPECT_EQ(search_value(tree, &removed_value), nullptr);
  EXPECT_TRUE(contains_value(tree, 30));
  EXPECT_EQ(destroyed_values, 1);
}

TEST_F(BSTTest, RemovingRootWithTwoChildrenDestroysExactlyOneValue) {
  const int values[] = {20, 10, 30, 25, 35};

  for (int value : values) {
    insert_value(tree, &value);
  }

  const int removed_value = 20;
  remove_value(tree, &removed_value);

  EXPECT_EQ(search_value(tree, &removed_value), nullptr);
  EXPECT_EQ(destroyed_values, 2);
  EXPECT_EQ(static_cast<int>(owned_values.size()), 4);

  for (int value : {10, 25, 30, 35}) {
    EXPECT_NE(search_value(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, RemovingRootWithTwoChildrenMaintainsSearchability) {
  const int values[] = {40, 20, 60, 10, 30, 50, 70, 45, 55, 42, 47, 43};

  for (int value : values) {
    insert_value(tree, &value);
  }

  const int removed_value = 40;
  remove_value(tree, &removed_value);

  EXPECT_EQ(search_value(tree, &removed_value), nullptr);

  for (int value : values) {
    if (value != removed_value) {
      EXPECT_NE(search_value(tree, &value), nullptr)
          << "Missing value " << value;
    }
  }
}

TEST_F(BSTTest, RemovingAllValuesLeavesEmptyTree) {
  for (int value = 0; value < 50; ++value) {
    insert_value(tree, &value);
  }

  for (int value = 0; value < 50; ++value) {
    remove_value(tree, &value);
  }

  for (int value = 0; value < 50; ++value) {
    EXPECT_EQ(search_value(tree, &value), nullptr);
  }

  EXPECT_EQ(destroyed_values, 50);
  EXPECT_TRUE(owned_values.empty());
}

TEST_F(BSTTest, RepeatedRemovalDoesNotDestroyValueTwice) {
  const int values[] = {20, 10, 30, 25, 35};

  for (int value : values) {
    insert_value(tree, &value);
  }

  const int removed_value = 10;

  remove_value(tree, &removed_value);
  EXPECT_EQ(destroyed_values, 1);

  remove_value(tree, &removed_value);
  EXPECT_EQ(destroyed_values, 1);

  EXPECT_EQ(search_value(tree, &removed_value), nullptr);
  EXPECT_EQ(static_cast<int>(owned_values.size()), 4);
}

TEST_F(BSTTest, DuplicateReplacementPreservesOwnershipCounts) {
  const int value = 42;

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(insert_value(tree, &value),
              i == 0 ? BST_OK : BST_VALUE_OVERWRITTEN);

    EXPECT_EQ(copied_values, i + 1);
    EXPECT_EQ(destroyed_values, i);
    EXPECT_EQ(static_cast<int>(owned_values.size()), 1);
  }

  destroy_tree(&tree);

  EXPECT_EQ(destroyed_values, 10);
  EXPECT_TRUE(owned_values.empty());
}

TEST_F(BSTTest, RandomizedInsertionsAndRemovalsMatchReferenceSet) {
  constexpr int value_count = 200;
  constexpr int operations = 2000;

  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> value_distribution(0, value_count - 1);
  std::bernoulli_distribution insert_distribution(0.5);

  std::set<int> expected;

  for (int i = 0; i < operations; ++i) {
    const int value = value_distribution(rng);

    if (insert_distribution(rng)) {
      const bool is_new_value = expected.insert(value).second;
      EXPECT_EQ(insert_value(tree, &value),
                is_new_value ? BST_OK : BST_VALUE_OVERWRITTEN);
    } else {
      remove_value(tree, &value);
      expected.erase(value);
    }

    for (int candidate = 0; candidate < value_count; ++candidate) {
      const bool should_exist = expected.find(candidate) != expected.end();
      const bool actually_exists = search_value(tree, &candidate) != nullptr;

      EXPECT_EQ(actually_exists, should_exist)
          << "Operation " << i << ", candidate " << candidate
          << ", operation value " << value;
    }
  }

  for (int value : expected) {
    EXPECT_NE(search_value(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, DeletionTriggersLLRotation) {
  // Removing 7, 6, then 5 makes the root left-heavy.
  const int values[] = {4, 2, 6, 1, 3, 5, 7};

  for (int value : values) {
    insert_value(tree, &value);
  }

  expect_valid_avl(tree);

  for (int value : {7, 6, 5}) {
    remove_value(tree, &value);
    expect_valid_avl(tree);
  }

  for (int value : {1, 2, 3, 4}) {
    EXPECT_NE(search_value(tree, &value), nullptr);
  }
  const int removed_value = 5;
  EXPECT_EQ(search_value(tree, &removed_value), nullptr);
}

TEST_F(BSTTest, DeletionTriggersRRRotation) {
  // Mirror of the LL case.
  const int values[] = {4, 2, 6, 1, 3, 5, 7};

  for (int value : values) {
    insert_value(tree, &value);
  }

  expect_valid_avl(tree);

  for (int value : {1, 2, 3}) {
    remove_value(tree, &value);
    expect_valid_avl(tree);
  }

  for (int value : {4, 5, 6, 7}) {
    EXPECT_NE(search_value(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, DeletionTriggersLRRotation) {
  // Removing 1 makes node 2 right-heavy.
  // Subsequent removals shrink the root's right subtree.
  const int values[] = {4, 2, 6, 1, 3, 5, 7};

  for (int value : values) {
    insert_value(tree, &value);
  }

  expect_valid_avl(tree);

  for (int value : {1, 7, 6, 5}) {
    remove_value(tree, &value);
    expect_valid_avl(tree);
  }

  for (int value : {2, 3, 4}) {
    EXPECT_NE(search_value(tree, &value), nullptr);
  }

  for (int value : {1, 5, 6, 7}) {
    EXPECT_EQ(search_value(tree, &value), nullptr);
  }
}

TEST_F(BSTTest, DeletionTriggersRLRotation) {
  // Mirror of the LR case.
  const int values[] = {4, 2, 6, 1, 3, 5, 7};

  for (int value : values) {
    insert_value(tree, &value);
  }

  expect_valid_avl(tree);

  for (int value : {7, 1, 2, 3}) {
    remove_value(tree, &value);
    expect_valid_avl(tree);
  }

  for (int value : {4, 5, 6}) {
    EXPECT_NE(search_value(tree, &value), nullptr);
  }

  for (int value : {1, 2, 3, 7}) {
    EXPECT_EQ(search_value(tree, &value), nullptr);
  }
}