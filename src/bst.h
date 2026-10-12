/**
 * @file bst.h
 * @brief Public API for the binary search tree.
 * @author Milan Novák
 * @todo Again, rethink error and return values for public funnctions.
 */

#ifndef BST_H
#define BST_H

#include <stdbool.h>
#include <stdlib.h>

typedef struct BST BST;
typedef struct Node BST_Node;

/**
 * @brief Classic C comparison function, should return negative for A > B,
 * positive for A < B and 0 for A == B.
 */
typedef int (*BSTCompareFn)(const void *, const void *);

/**
 * @brief Function to create a copy of the stored value.
 */
typedef void *(*BSTCopyValueFn)(const void *);

/**
 * @brief Function to free the stored value.
 */
typedef void (*BSTDestroyValueFn)(void *);

/**
 * @brief Creates a new binary search tree.
 *
 * @param compare Comparison function used to order values in the tree.
 * @param copy Function used to create a copy of a stored value.
 * @param destroy Function used to destroy a stored value.
 * @return Newly created BST or NULL on invalid input or allocation failure.
 * @note The tree keeps its own copy of the insterted data.
 */
BST *bstCreate(BSTCompareFn compare, BSTCopyValueFn copy,
               BSTDestroyValueFn destroy);

/**
 * @brief Destroys a BST and frees all associated memory.
 *
 * @param tree Pointer to the BST pointer to destroy.
 */
void bstDestroy(BST **tree);

/**
 * @brief Inserts a value into the BST.
 *
 * @param tree Target tree.
 * @param value Value to insert.
 */
void bstInsert(BST *tree, const void *value);

/**
 * @brief Removes a value from the BST.
 *
 * @param tree Target tree.
 * @param value Value to remove.
 */
void bstRemove(BST *tree, const void *value);

/**
 * @brief Searches for a value in the BST.
 *
 * @param tree Tree to search in.
 * @param value Value to find.
 * @return Pointer to the stored value, or NULL if not found.
 */
void *bstSearch(const BST *tree, const void *value);

#endif