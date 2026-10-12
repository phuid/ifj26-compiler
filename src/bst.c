#include "bst.h"
#include "impl_bst.h"
#include <stdbool.h>
#include <stdlib.h>

int max(const int a, const int b) {
  if (a > b)
    return a;
  else
    return b;
}

BST *bstCreate(BSTCompareFn compare, BSTCopyValueFn copy,
               BSTDestroyValueFn destroy) {
  if (compare == NULL || copy == NULL || destroy == NULL) {
    // TODO(Milan Novak):
    return NULL;
  }

  BST *tree = malloc(sizeof(BST));
  if (!tree) {
    // TODO(Milan Novak):
    return NULL;
  }

  tree->root = NULL;
  tree->compareData = compare;
  tree->copyData = copy;
  tree->destroyData = destroy;
  return tree;
}

void destroyNode(BST *tree, Node *node) {
  if (node == NULL) {
    return;
  }

  destroyNode(tree, node->left);
  destroyNode(tree, node->right);

  tree->destroyData(node->value);
  free(node);
}

void bstDestroy(BST **tree) {
  if (tree == NULL || *tree == NULL) {
    return;
  }

  destroyNode(*tree, (*tree)->root);
  free(*tree);
  *tree = NULL;
}

Node *createNode(void *value) {
  Node *node = malloc(sizeof(Node));
  if (node == NULL) {
    // TODO(Milan Novak):
    return NULL;
  }

  node->height = 1;
  node->left = NULL;
  node->right = NULL;
  node->value = value;
  node->balance = 0;
  return node;
}

int getNodeHeight(Node *node) { return node == NULL ? 0 : node->height; }

int calculateBalance(Node *node) {
  if (node == NULL)
    return 0;

  return getNodeHeight(node->right) - getNodeHeight(node->left);
}

int calculateHeight(Node *node) {
  if (node == NULL) {
    return 0;
  }
  int leftHeight = getNodeHeight(node->left);
  int rightHeight = getNodeHeight(node->right);
  return max(leftHeight, rightHeight) + 1;
}

Node *rotateRight(Node *root) {
  Node *newRoot = root->left;
  if (newRoot == NULL) {
    // TODO(Milan Novak):
    return NULL;
  }
  root->left = newRoot->right;
  newRoot->right = root;

  root->height = calculateHeight(root);
  root->balance = calculateBalance(root);
  newRoot->height = calculateHeight(newRoot);
  newRoot->balance = calculateBalance(newRoot);
  return newRoot;
}

Node *rotateLeft(Node *root) {
  Node *newRoot = root->right;
  if (newRoot == NULL) {
    // TODO(Milan Novak):
    return NULL;
  }
  root->right = newRoot->left;
  newRoot->left = root;

  root->height = calculateHeight(root);
  root->balance = calculateBalance(root);
  newRoot->height = calculateHeight(newRoot);
  newRoot->balance = calculateBalance(newRoot);
  return newRoot;
}

Node *bstRebalance(Node *root) {
  root->height = calculateHeight(root);
  root->balance = calculateBalance(root);

  if (root->balance > 1) {
    if (calculateBalance(root->right) < 0) {
      root->right = rotateRight(root->right);
    }
    return rotateLeft(root);
  }

  if (root->balance < -1) {
    if (calculateBalance(root->left) > 0) {
      root->left = rotateLeft(root->left);
    }
    return rotateRight(root);
  }
  return root;
}

// TODO(Milan Novak):  maybe pass the function pointers instead of the whole
// tree
Node *insertValue(BST *tree, Node *root, const void *value) {
  if (tree == NULL) {
    // TODO(Milan Novak):
    return NULL;
  }

  if (root == NULL) {
    void *copy = tree->copyData(value);
    if (copy == NULL) {
      // TODO(Milan Novak):
      return NULL;
    }
    return createNode(copy);
  }

  int cmp = tree->compareData(root->value, value);

  if (cmp < 0) {
    root->left = insertValue(tree, root->left, value);
  } else if (cmp > 0) {
    root->right = insertValue(tree, root->right, value);
  } else {
    // Cleanup old data, replace with new
    void *copy = tree->copyData(value);
    if (copy == NULL) {
      // TODO(Milan Novak):
      return NULL;
    }
    tree->destroyData(root->value);
    root->value = copy;
    return root;
  }

  return bstRebalance(root);
}

void bstInsert(BST *tree, const void *value) {
  if (tree == NULL || value == NULL) {
    // TODO(Milan Novak):
    return;
  }

  tree->root = insertValue(tree, tree->root, value);
}

Node *search(const BST *tree, Node *root, const void *value) {
  if (root == NULL) {
    return NULL;
  }

  const int cmp = tree->compareData(root->value, value);

  if (cmp == 0) {
    return root;
  }

  if (cmp < 0) {
    return search(tree, root->left, value);
  }

  return search(tree, root->right, value);
}

void *bstSearch(const BST *tree, const void *value) {
  if (tree == NULL || value == NULL) {
    return NULL;
  }

  Node *res = search(tree, tree->root, value);
  return res == NULL ? NULL : res->value;
}

Node *findMin(Node *root) {
  if (root == NULL) {
    return NULL;
  }

  return root->left == NULL ? root : findMin(root->left);
}

Node *remove(BST *tree, Node *root, const void *value) {
  if (root == NULL) {
    return NULL;
  }

  const int cmp = tree->compareData(root->value, value);

  if (cmp == 0) {
    if (root->left != NULL && root->right != NULL) {
      // both children
      Node *min = findMin(root->right);
      tree->destroyData(root->value);
      root->value = tree->copyData(min->value);
      root->right = remove(tree, root->right, min->value);
    } else {
      // this also covers the case when both are NULL
      Node *tmp = root->left == NULL ? root->right : root->left;
      tree->destroyData(root->value);
      free(root);
      return tmp;
    }
  } else if (cmp < 0) {
    root->left = remove(tree, root->left, value);
  } else {
    root->right = remove(tree, root->right, value);
  }

  return bstRebalance(root);
}

void bstRemove(BST *tree, const void *value) {
  if (tree == NULL || value == NULL) {
    // TODO(Milan Novak):  error
    return;
  }
  tree->root = remove(tree, tree->root, value);
}
