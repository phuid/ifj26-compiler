#include "bst.h"
#include "impl_bst.h"
#include <stdlib.h>
#include <stdbool.h>

int max(const int a, const int b)
{
    if (a > b)
        return a;
    else
        return b;
}

BST *bstCreate(BSTCompareFn compare, BSTCopyValueFn copy, BSTDestroyValueFn destroy)
{
    BST *tree = malloc(sizeof(BST));
    if (!tree)
    {
        return NULL;
    }

    tree->root = NULL;
    tree->compareData = compare;
    tree->copyData = copy;
    tree->destroyData = destroy;
    return tree;
}

void destroyNode(BST *tree, Node *node)
{
    if (node == NULL)
    {
        return;
    }

    destroyNode(tree, node->left);
    destroyNode(tree, node->right);

    tree->destroyData(node->value);
    free(node);
}

void bstDestroy(BST **tree)
{
    destroyNode(*tree, (*tree)->root);
    (*tree) = NULL;
}

Node *createNode(void *value)
{
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        // TODO
        return NULL;
    }

    node->height = 1;
    node->left = NULL;
    node->right = NULL;
    node->value = value;
    node->balance = 0;
    return node;
}

int calculateBalance(Node *node)
{
    if (node == NULL)
        return 0;
    int leftHeight = node->left == NULL ? 0 : node->left->height;
    int rightHeight = node->right == NULL ? 0 : node->right->height;
    return rightHeight - leftHeight;
}

int calculateHeight(Node *node)
{
    if (node == NULL)
    {
        return 0;
    }
    int leftHeight = node->left == NULL ? 0 : node->left->height;
    int rightHeight = node->right == NULL ? 0 : node->right->height;
    return max(leftHeight, rightHeight) + 1;
}

Node *rotateRight(Node *node)
{
    Node *newRoot = node->left;
    if (newRoot == NULL)
    {
        exit(1); // TODO
    }
    node->left = newRoot->right;
    newRoot->right = node;

    // TODO is the balance calculation needed
    node->height = calculateHeight(node);
    node->balance = calculateBalance(node);
    newRoot->height = calculateHeight(newRoot);
    newRoot->balance = calculateBalance(newRoot);
    return newRoot;
}

Node *rotateLeft(Node *node)
{
    Node *newRoot = node->right;
    if (newRoot == NULL)
    {
        exit(1); // TODO
    }
    node->right = newRoot->left;
    newRoot->left = node;

    // TODO is the balance calculation needed
    node->height = calculateHeight(node);
    node->balance = calculateBalance(node);
    newRoot->height = calculateHeight(newRoot);
    newRoot->balance = calculateBalance(newRoot);
    return newRoot;
}

// TODO maybe pass the function pointers instead of the whole tree
Node *insertValue(BST *tree, Node *root, const void *value)
{
    if (tree == NULL)
    {
        // TODO
        exit(1);
    }

    if (root == NULL)
    {
        return createNode(tree->copyData(value));
    }

    int cmp = tree->compareData(root->value, value);

    if (cmp < 0)
    {
        root->left = insertValue(tree, root->left, value);
    }
    else if (cmp > 0)
    {
        root->right = insertValue(tree, root->right, value);
    }
    else
    {
        // Cleanup old data, replace with new
        tree->destroyData(root->value);
        root->value = tree->copyData(value);
        return root;
    }

    root->height = calculateHeight(root);
    root->balance = calculateBalance(root);

    if (root->balance > 1)
    {
        int rightBalance = calculateBalance(root->right);
        if (rightBalance >= 0)
        {
            // RR - rotate left
            return rotateLeft(root);
        }
        else
        {
            root->right = rotateRight(root->right);
            return rotateLeft(root);
            // RL - rotate right left
        }
    }

    if (root->balance < -1)
    {
        int leftBalance = calculateBalance(root->left);
        if (leftBalance <= 0)
        {
            // LL - rotate right
            return rotateRight(root);
        }
        else
        {
            root->left = rotateLeft(root->left);
            return rotateRight(root);
            // LR - rotate left right
        }
    }

    return root;
}

void bstInsert(BST *tree, const void *value)
{
    if (tree == NULL)
    {
        // TODO
        return;
    }

    tree->root = insertValue(tree, tree->root, value);
}

Node *search(const BST *tree, Node *root, const void *value)
{
    if (root == NULL)
    {
        // Not found
        return NULL;
    }

    const int cmp = tree->compareData(root->value, value);

    if (cmp == 0)
    {
        // Found
        return root;
    }

    if (cmp < 0)
    {
        // Search left
        return search(tree, root->left, value);
    }

    // Search right
    return search(tree, root->right, value);
}

void *bstSearch(const BST *tree, const void *value)
{
    Node *res = search(tree, tree->root, value);
    return res == NULL ? NULL : res->value;
}

Node *findMin(Node *root)
{
    if (root == NULL)
        return NULL;
    if (root->left == NULL)
        return root;
    else
        return findMin(root->left);
}

Node *remove(BST *tree, Node *root, const void *value)
{
    if (root == NULL)
    {
        return NULL;
    }

    const int cmp = tree->compareData(root->value, value);

    if (cmp == 0)
    {
        if (root->left != NULL && root->left != NULL)
        {
            // both children
            Node *min = findMin(root->right);
            tree->destroyData(root->value);
            root->value = tree->copyData(min->value);
            root->right = remove(tree, root->right, min->value);
            return root;
        }
        // left child
        if (root->left != NULL)
        {
            Node *leftChild = root->left;
            tree->destroyData(root);
            free(root);
            return leftChild;
        }

        // right child
        if (root->right != NULL)
        {
            Node *rightChild = root->right;
            tree->destroyData(root);
            free(root);
            return rightChild;
        }

        // no children
        tree->destroyData(root->value);
        free(root);
        return NULL;
    }
    else if (cmp < 0)
    {
        // Search left
        root->left = remove(tree, root->left, value);
    }
    else
    {
        // Search right
        root->right = search(tree, root->right, value);
    }

    root->height = calculateHeight(root);
    root->balance = calculateBalance(root);

    if (root->balance > 1)
    {
        int rightBalance = calculateBalance(root->right);
        if (rightBalance >= 0)
        {
            // RR - rotate left
            return rotateLeft(root);
        }
        else
        {
            root->right = rotateRight(root->right);
            return rotateLeft(root);
            // RL - rotate right left
        }
    }

    if (root->balance < -1)
    {
        int leftBalance = calculateBalance(root->left);
        if (leftBalance <= 0)
        {
            // LL - rotate right
            return rotateRight(root);
        }
        else
        {
            root->left = rotateLeft(root->left);
            return rotateRight(root);
            // LR - rotate left right
        }
    }

    return root;
}

void bstRemove(BST *tree, const void *value)
{
    remove(tree, tree->root, value);
}
