#ifndef BST_H
#define BST_H

#include <stdlib.h>
#include <stdbool.h>

typedef struct BST BST;
typedef struct Node BST_Node;
typedef int (*BSTCompareFn)(const void *, const void *);
typedef void *(*BSTCopyValueFn)(const void *);
typedef void (*BSTDestroyValueFn)(void *);

BST *bstCreate(size_t elementSize, BSTCompareFn compare, BSTCopyValueFn copy, BSTDestroyValueFn destroy);
void bstDestroy(BST **tree);
void bstInsert(BST *tree, const void *value);
void bstRemove(BST *tree, const void *value);
void *bstSearch(const BST *tree, const void *value);


#endif