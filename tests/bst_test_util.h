/**
 * @file bst_test_util.h
 * @brief Test-only shorthands over the status-returning BST API.
 * @details bst_create and bst_search report through out-parameters; these
 * wrappers return the tree or the stored value directly (nullptr when the
 * call does not succeed), so assertions on them stay one line long.
 */
#ifndef TESTS_BST_TEST_UTIL_H_
#define TESTS_BST_TEST_UTIL_H_

extern "C" {
#include "../src/bst.h"
}

/** Creates a tree, or returns nullptr when bst_create does not return OK. */
inline BST *make_tree(BSTCompareFn compare, BSTCopyValueFn copy,
                      BSTDestroyValueFn destroy) {
  BST *tree = nullptr;
  return bst_create(compare, copy, destroy, &tree) == BST_OK ? tree : nullptr;
}

/** Returns the stored value equal to @p value, or nullptr when absent. */
inline const void *find_value(const BST *tree, const void *value) {
  const void *result = nullptr;
  return bst_search(tree, value, &result) == BST_OK ? result : nullptr;
}

#endif // TESTS_BST_TEST_UTIL_H_
