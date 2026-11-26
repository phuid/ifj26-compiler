/**
 * @file bst.h
 * Project: IFJ26 compiler
 * @brief Public API for the binary search tree.
 * @author Milan Novák (xnovakm00)
 */
#include <stddef.h>

#ifndef BST_H
#define BST_H

typedef struct BST BST;
typedef struct Node BST_Node;

typedef struct {
  const void **data;
  size_t capacity;
} NodeArray;

/**
 * @brief Classic C comparison function, should return negative for A < B,
 * positive for A > B and 0 for A == B.
 */
typedef int (*BSTCompareFn)(const void *, const void *);

/**
 * @brief Function to create a copy of the stored value.
 * @return The copy, or NULL if the copy cannot be created.
 */
typedef void *(*BSTCopyValueFn)(const void *);

/**
 * @brief Function to destroy a value owned by the tree.
 */
typedef void (*BSTDestroyValueFn)(void *);

typedef enum {
  BST_OK,
  BST_INVALID_PARAMS,
  BST_ALLOCATION_FAIL,
  BST_VALUE_OVERWRITTEN,
  BST_NOT_FOUND
} BST_STATUS;

/**
 * @brief Creates a new binary search tree.
 *
 * @param compare Comparison function used to order values in the tree.
 * @param copy Function used to create a copy of a stored value.
 * @param destroy Function used to destroy a stored value.
 * @param bst Output parameter. For valid arguments, *bst is
 *            set to new tree on success or NULL on allocation fail.
 * @return BST_OK on success, BST_INVALID_PARAMS for invalid arguments,
 *         BST_ALLOCATION_FAIL if allocation fails.
 * @note The tree keeps its own copy of the inserted data.
 */
BST_STATUS bst_create(BSTCompareFn compare, BSTCopyValueFn copy,
                      BSTDestroyValueFn destroy, BST **bst);

/**
 * @brief Destroys a BST and frees all associated memory.
 *
 * @param tree Pointer to the BST pointer to destroy, *tree is set to
 *             NULL.
 * @return BST_OK on success or status code on fail.
 */
BST_STATUS bst_destroy(BST **tree);

/**
 * @brief Inserts a value into the BST.
 *
 * @param tree Target tree.
 * @param value Non-NULL value to insert. If an equivalent value is already
 *              present, its stored copy is replaced.
 * @return BST_OK when a new value is inserted, BST_VALUE_OVERWRITTEN when an
 *         existing value is replaced, BST_INVALID_PARAMS for invalid arguments,
 *         or BST_ALLOCATION_FAIL if copying or node allocation fails.
 */
BST_STATUS bst_insert(BST *tree, const void *value);

/**
 * @brief Removes a value from the BST.
 *
 * @param tree Target tree.
 * @param value Value to remove.
 * @return BST_OK when the value is removed, BST_NOT_FOUND if it is missing,
 *         BST_INVALID_PARAMS for invalid arguments, or BST_ALLOCATION_FAIL if
 *         copying the successor value fails.
 */
BST_STATUS bst_remove(BST *tree, const void *value);

/**
 * @brief Searches for a value in the BST.
 *
 * @param tree Tree to search in.
 * @param value Value to find.
 * @param result Output parameter. Contains pointer to stored value on success,
 * NULL on BST_NOT_FOUND and is not modified otherwise.
 * @return BST_OK when the value is found, BST_NOT_FOUND if it is absent, or
 *         BST_INVALID_PARAMS for invalid arguments.
 */
BST_STATUS bst_search(const BST *tree, const void *value, const void **result);

/**
 * @brief Fills an array with all values in the BST in sorted order.
 *
 * @param tree Tree to traverse.
 * @param result Output parameter. Contains a NodeArray with pointers to all
 *               stored values on success. The array is empty on BST_OK when
 *               the tree is empty. The caller owns the array and must free it.
 * @return BST_OK on success, or status code on fail.
 */
BST_STATUS bst_list(const BST *tree, NodeArray *result);

#endif