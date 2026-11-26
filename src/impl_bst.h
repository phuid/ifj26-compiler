/**
 * @file impl_bst.h
 * Project: IFJ26 compiler
 * @brief Internal node and tree structure definitions for the BST.
 * @author Milan Novák (xnovakm00)
 */

#ifndef IMPL_BST_H
#define IMPL_BST_H

#include "bst.h"

typedef struct Node {
  void *value;
  int height;
  struct Node *left;
  struct Node *right;
} Node;

struct BST {
  Node *root;
  BSTCompareFn compare_data;
  BSTCopyValueFn copy_data;
  BSTDestroyValueFn destroy_data;
};

#endif