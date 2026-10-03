#ifndef BST_H
#define BST_H

typedef struct BST BST;
typedef struct Node BST_Node;

BST *bstCreate(size_t elementSize, BSTCompareFn compare, BSTCopyValueFn);
void bstInsert(BST *tree, const void *value);
void bstRemove();
void bstSearch();


#endif