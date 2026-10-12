/**
 * @file impl_bst.h
 * @brief Internal node and tree structure definitions for the BST.
 * @author Milan Novák
 */

#ifndef IMPL_BST_H
#define IMPL_BST_H

#include "bst.h"
#include <stdlib.h>

typedef struct Node {
  void *value;
  int height;
  int balance;
  struct Node *left;
  struct Node *right;
} Node;

struct BST {
  Node *root;
  BSTCompareFn compareData;
  BSTCopyValueFn copyData;
  BSTDestroyValueFn destroyData;
};

#endif