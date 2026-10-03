#include "bst.h"
#include "impl_bst.h"
#include <stdlib.h>


BST *bstCreate(size_t elementSize, BSTCompareFn compare, BSTCopyValueFn copy, BSTDestroyValueFn destroy){
    BST *tree = malloc(sizeof(BST));
    if (!tree){
        return NULL;
    }

    tree->root = NULL;
    tree->compareData = compare;
    tree->copyData = copy;
    tree->destroyData = destroy;
    tree->elementSize = elementSize;
    return tree;
}


Node *createNode(const void* value, int height){
    Node* node = malloc(sizeof(Node));
    if(node == NULL){
        // TODO
        return NULL;
    }

    node->height = height;
    node->left = NULL;
    node->right = NULL;
    node->value = value;
}

void destroyNode(BST *tree, Node *node){
    tree->destroyData(node->value);
    free(node);
}

Node* insertValue(BST *tree, Node **current, const void *value, const int height){
    if(tree == NULL){
        // TODO
        exit(1);
    }

    if(*current == NULL){
        return createNode(tree->copyData(value), height);
    }

    int cmp = tree->compareData(value, (*current)->value);

    if(cmp < 0){
        (*current)->left = insertValue(tree, (*current)->left, value, height+1);
    }else if(cmp > 0) {
        (*current)->right = insertValue(tree, (*current)->right, value, height+1);
    }else{
        // Cleanup old node, insert new one
        destroyNode(tree, *current);
        *current = createNode(tree->copyData(value), height);
    }
}

void bstInsert(BST *tree, const void *value){
    if(tree == NULL){
        //TODO
        return;
    }

    insertValue(tree, tree->root, value, tree->root->height);
}





