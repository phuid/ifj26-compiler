#ifndef IMPL_BST_H
#define IMPL_BST_H

#include <stddef.h>

typedef struct Node
{
    void *value;
    int height;
    struct Node *left;
    struct Node *right;
} Node;

typedef int (*BSTCompareFn)(const void *, const void *);
typedef const void *(*BSTCopyValueFn)(const void *);
typedef const void *(*BSTDestroyValueFn)(const void *);

struct BST
{
    Node *root;
    size_t elementSize;
    BSTCompareFn compareData;
    BSTCopyValueFn copyData;
    BSTDestroyValueFn destroyData;
};

#endif