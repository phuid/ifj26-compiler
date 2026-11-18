#include "gtest/gtest.h"

#include "bst_test_util.h"

extern "C" {
#include "../src/bst.h"
}

/*
 * AI-generated test block — data structure defects found by bug hunting.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write GoogleTest cases for three data structure defects: resolving a
 *   chain of dependency diamonds takes exponential time because
 *   type_entity_resolve revisits dependents that already have the type
 *   (specifikace.txt only recurses into UNKNOWN dependents); destroying an
 *   AST node leaves its inferred_type registered as a dependent of the
 *   symbol's type, so a later resolve writes to freed memory; and removing
 *   a BST node with two children frees the stored value of a different key,
 *   invalidating pointers returned by bst_search. Do not dereference freed
 *   memory in the assertions. These tests are expected to fail until fixed.
 * Note: the two TypeEntity tests from this block were removed together with
 * TypeEntity itself.
 */
namespace {

int compare_int(const void *a, const void *b) {
  int left = *static_cast<const int *>(a);
  int right = *static_cast<const int *>(b);
  return (left > right) - (left < right);
}

void *copy_int(const void *value) {
  int *copy = static_cast<int *>(malloc(sizeof(int)));
  if (copy != nullptr) {
    *copy = *static_cast<const int *>(value);
  }
  return copy;
}

void destroy_int(void *value) { free(value); }

} // namespace

TEST(DataStructBugTest, TwoChildRemovalKeepsOtherValuePointersStable) {
  BST *tree = make_tree(compare_int, copy_int, destroy_int);
  ASSERT_NE(tree, nullptr);
  for (int key : {20, 10, 30, 5, 15, 25, 35}) {
    bst_insert(tree, &key);
  }
  int kept = 15;
  int removed = 20;
  const void *kept_before = find_value(tree, &kept);
  ASSERT_NE(kept_before, nullptr);

  bst_remove(tree, &removed);

  // bst.h: bst_search yields a pointer to the stored value; removing another
  // key must not free and replace it.
  EXPECT_EQ(find_value(tree, &kept), kept_before);
  bst_destroy(&tree);
}
