#ifndef IMPL_BST_H
#define IMPL_BST_H

#include <stdlib.h>
#include "bst.h"

typedef struct Node
{
    void *value;
    int height;
    int balance;
    struct Node *left;
    struct Node *right;
} Node;


struct BST
{
    Node *root;
    BSTCompareFn compareData;
    BSTCopyValueFn copyData;
    BSTDestroyValueFn destroyData;
};

#endif