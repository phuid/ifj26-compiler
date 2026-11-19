#include "gtest/gtest.h"

#include "bst_test_util.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <random>
#include <set>
#include <vector>

extern "C" {
#include "../src/bst.h"
#include "../src/impl_bst.h"
}

namespace {

int copies = 0, destroys = 0;

int compareInts(const void *a, const void *b) {
  return *static_cast<const int *>(a) - *static_cast<const int *>(b);
}

void *copyInt(const void *value) {
  ++copies;
  return new int(*static_cast<const int *>(value));
}

void destroyInt(void *value) {
  ++destroys;
  delete static_cast<int *>(value);
}

int valueOf(const BST_Node *node) { return *static_cast<int *>(node->value); }

// Checks order, stored height and balance of every node; smaller keys go
// left. The balance factor is derived from the child heights.
int expectValidAVL(const BST_Node *node, long long low = INT64_MIN,
                   long long high = INT64_MAX) {
  if (node == nullptr) {
    return 0;
  }
  const int value = valueOf(node);
  EXPECT_TRUE(low < value && value < high) << "order violated at " << value;
  const int left = expectValidAVL(node->left, low, value);
  const int right = expectValidAVL(node->right, value, high);
  EXPECT_EQ(node->height, 1 + std::max(left, right)) << "at " << value;
  EXPECT_LE(std::abs(right - left), 1) << "at " << value;
  return 1 + std::max(left, right);
}

class BSTGapTest : public ::testing::Test {
protected:
  void SetUp() override {
    copies = destroys = 0;
    tree = make_tree(compareInts, copyInt, destroyInt);
    ASSERT_NE(tree, nullptr);
  }
  void TearDown() override { bst_destroy(&tree); }
  BST *tree = nullptr;
};

} // namespace

/*
 * AI-generated test block — insertion rotations.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Table of the four rotations: insert order -> expected root, left, right.
 */
TEST_F(BSTGapTest, InsertionRotationsProduceExpectedShape) {
  struct Case {
    std::vector<int> order;
    int root, left, right;
  };
  const Case cases[] = {{{10, 20, 30}, 20, 10, 30},
                        {{30, 20, 10}, 20, 10, 30},
                        {{10, 30, 20}, 20, 10, 30},
                        {{30, 10, 20}, 20, 10, 30}};
  for (const Case &c : cases) {
    SCOPED_TRACE(::testing::PrintToString(c.order));
    for (int value : c.order) {
      bst_insert(tree, &value);
    }
    expectValidAVL(tree->root);
    ASSERT_EQ(tree->root->height, 2); // both children present
    EXPECT_EQ(valueOf(tree->root), c.root);
    EXPECT_EQ(valueOf(tree->root->left), c.left);
    EXPECT_EQ(valueOf(tree->root->right), c.right);
    TearDown();
    SetUp();
  }
}

/*
 * AI-generated test block — randomized mixed operations.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Random inserts/removes must keep AVL invariants and match std::set.
 */
TEST_F(BSTGapTest, RandomMixedOperationsMatchStdSet) {
  const unsigned kKeyRange = 200U;
  std::mt19937 rng(20261114U);
  std::set<int> reference;
  for (int step = 0; step < 2000 && !HasFailure(); ++step) {
    const int key = static_cast<int>(rng() % kKeyRange);
    if (rng() % 2U == 0U) {
      bst_insert(tree, &key);
      reference.insert(key);
    } else {
      bst_remove(tree, &key);
      reference.erase(key);
    }
    expectValidAVL(tree->root);
    EXPECT_EQ(find_value(tree, &key) != nullptr, reference.count(key) == 1U);
    EXPECT_EQ(static_cast<size_t>(copies - destroys), reference.size());
  }
}

/*
 * AI-generated test block — API edge cases.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   NULL args, only-node removal, failed duplicate copy, string-key structs.
 */
TEST_F(BSTGapTest, NullArgumentsAreRejectedOrIgnored) {
  BST *created = nullptr;
  EXPECT_EQ(bst_create(nullptr, copyInt, destroyInt, &created),
            BST_INVALID_PARAMS);
  EXPECT_EQ(bst_create(compareInts, nullptr, destroyInt, &created),
            BST_INVALID_PARAMS);
  EXPECT_EQ(bst_create(compareInts, copyInt, nullptr, &created),
            BST_INVALID_PARAMS);
  EXPECT_EQ(bst_create(compareInts, copyInt, destroyInt, nullptr),
            BST_INVALID_PARAMS);
  EXPECT_EQ(created, nullptr);
  BST *nullTree = nullptr;
  bst_destroy(&nullTree);
  bst_destroy(nullptr);
  const int value = 5;
  EXPECT_EQ(bst_insert(nullptr, &value), BST_INVALID_PARAMS);
  EXPECT_EQ(bst_remove(nullptr, &value), BST_INVALID_PARAMS);
  const void *result = &value;
  EXPECT_EQ(bst_search(nullptr, &value, &result), BST_INVALID_PARAMS);
  EXPECT_EQ(bst_search(tree, &value, nullptr), BST_INVALID_PARAMS);
  EXPECT_EQ(bst_insert(tree, nullptr), BST_INVALID_PARAMS);
  EXPECT_EQ(bst_remove(tree, nullptr), BST_INVALID_PARAMS);
  EXPECT_EQ(bst_search(tree, nullptr, &result), BST_INVALID_PARAMS);
  EXPECT_EQ(result, &value) << "result is not modified on invalid params";
  EXPECT_EQ(copies + destroys, 0);
}

TEST_F(BSTGapTest, RemoveOnlyNodeLeavesRootNull) {
  const int key = 42;
  bst_insert(tree, &key);
  bst_remove(tree, &key);
  EXPECT_EQ(tree->root, nullptr);
  EXPECT_EQ(destroys, 1);
}

TEST_F(BSTGapTest, FailedCopyOnDuplicateUpdateKeepsOldElement) {
  const int key = 2;
  ASSERT_EQ(bst_insert(tree, &key), BST_OK);
  const void *before = find_value(tree, &key);
  tree->copy_data = [](const void *) -> void * { return nullptr; };
  EXPECT_EQ(bst_insert(tree, &key), BST_ALLOCATION_FAIL);
  EXPECT_EQ(find_value(tree, &key), before);
  EXPECT_EQ(destroys, 0);
}

TEST(BSTGapStructTest, HeapKeyedEntriesSupportFullLifecycle) {
  struct Entry {
    char *key;
    int payload;
  };
  BST *entries = make_tree(
      [](const void *a, const void *b) {
        return std::strcmp(static_cast<const Entry *>(a)->key,
                           static_cast<const Entry *>(b)->key);
      },
      [](const void *v) -> void * {
        const Entry *src = static_cast<const Entry *>(v);
        return new Entry{strdup(src->key), src->payload};
      },
      [](void *v) {
        free(static_cast<Entry *>(v)->key);
        delete static_cast<Entry *>(v);
      });
  Entry original{const_cast<char *>("foo"), 1}, update{original.key, 99};
  EXPECT_EQ(bst_insert(entries, &original), BST_OK);
  EXPECT_EQ(bst_insert(entries, &update), BST_VALUE_OVERWRITTEN);
  const Entry *found =
      static_cast<const Entry *>(find_value(entries, &original));
  ASSERT_NE(found, nullptr);
  EXPECT_NE(found->key, original.key) << "tree must own its key copy";
  EXPECT_EQ(found->payload, 99);
  EXPECT_EQ(bst_remove(entries, &original), BST_OK);
  EXPECT_EQ(entries->root, nullptr);
  bst_destroy(&entries);
}
