/**
 * @file bst.c
 * @brief Height-balanced binary search tree implementation
 * @author Milan Novák
 */

#include "bst.h"
#include "impl_bst.h"
#include <stdlib.h>

/**
 * @brief Returns the larger of two integers.
 *
 * @param a First integer.
 * @param b Second integer.
 * @return The maximum value between a and b.
 */
static int max(const int a, const int b) {
  if (a > b)
    return a;
  else
    return b;
}

/**
 * @brief Creates a new node containing the given value.
 *
 * @param value Value stored in the node.
 * @return Newly allocated node or NULL on allocation failure.
 */
static Node *createNode(void *value) {
  Node *node = malloc(sizeof(Node));
  if (node == NULL) {
    return NULL;
  }

  node->height = 1;
  node->left = NULL;
  node->right = NULL;
  node->value = value;
  return node;
}

/**
 * @brief Recursively destroys a subtree rooted at the given node.
 *
 * @param tree Tree that owns the node values.
 * @param root Root of the subtree to destroy.
 * @return BST_OK on success or BST_INVALID_PARAMS if tree is NULL.
 */
static BST_STATUS destroySubtree(BST *tree, Node *root) {
  if (tree == NULL) {
    return BST_INVALID_PARAMS;
  }
  if (root == NULL) {
    return BST_OK;
  }

  destroySubtree(tree, root->left);
  destroySubtree(tree, root->right);

  tree->destroyData(root->value);
  free(root);

  return BST_OK;
}

/**
 * @brief Returns the height of a node.
 *
 * @param node Node to inspect.
 * @return Height of the node, or 0 for NULL.
 */
static int getNodeHeight(Node *node) { return node == NULL ? 0 : node->height; }

/**
 * @brief Calculates the balance of a node.
 *
 * @param node Tree node.
 * @return Balance of the node or 0 for NULL node.
 */
static int calculateBalance(Node *node) {
  if (node == NULL)
    return 0;

  return getNodeHeight(node->right) - getNodeHeight(node->left);
}

/**
 * @brief Computes the height of a node based on its children.
 *
 * @param node Tree node.
 * @return Height of the subtree rooted at the node.
 */
static int calculateHeight(Node *node) {
  if (node == NULL) {
    return 0;
  }
  int leftHeight = getNodeHeight(node->left);
  int rightHeight = getNodeHeight(node->right);
  return max(leftHeight, rightHeight) + 1;
}

/**
 * @brief Rotates the subtree rooted at root to the right.
 *
 * @param root Root of the subtree.
 * @return New root or NULL for NULL root node.
 */
static Node *rotateRight(Node *root) {
  Node *newRoot = root->left;
  if (newRoot == NULL) {
    return NULL;
  }
  root->left = newRoot->right;
  newRoot->right = root;

  root->height = calculateHeight(root);
  newRoot->height = calculateHeight(newRoot);
  return newRoot;
}

/**
 * @brief Rotates the subtree rooted at root to the left.
 *
 * @param root Root of the subtree.
 * @return New root or NULL for NULL root node.
 */
static Node *rotateLeft(Node *root) {
  Node *newRoot = root->right;
  if (newRoot == NULL) {
    return NULL;
  }
  root->right = newRoot->left;
  newRoot->left = root;

  root->height = calculateHeight(root);
  newRoot->height = calculateHeight(newRoot);
  return newRoot;
}

/**
 * @brief Rebalances a node and its subtree if needed.
 *
 * @param root Root of the subtree to rebalance.
 * @return Root of the balanced subtree.
 */
static Node *bstRebalance(Node *root) {
  root->height = calculateHeight(root);
  int balance = calculateBalance(root);

  if (balance > 1) {
    if (calculateBalance(root->right) < 0) {
      root->right = rotateRight(root->right);
    }
    return rotateLeft(root);
  }

  if (balance < -1) {
    if (calculateBalance(root->left) > 0) {
      root->left = rotateLeft(root->left);
    }
    return rotateRight(root);
  }
  return root;
}

/**
 * @brief Inserts a value into the subtree rooted at root.
 *
 * @param tree Tree that defines the data operations.
 * @param root Current subtree root.
 * @param value Value to insert.
 * @param err Output status. BST_OK on success, BST_INVALID_PARAMS if tree is
 *            NULL, BST_ALLOCATION_FAIL if copying or node allocation fails,
 *            or BST_VALUE_OVERWRITTEN if a value is replaced.
 * @return Updated subtree root or NULL for errors.
 */
static Node *insertValue(BST *tree, Node *root, const void *value,
                         BST_STATUS *err) {
  if (err == NULL) {
    return NULL;
  }
  if (tree == NULL) {
    *err = BST_INVALID_PARAMS;
    return NULL;
  }

  if (root == NULL) {
    void *copy = tree->copyData(value);
    if (copy == NULL) {
      *err = BST_ALLOCATION_FAIL;
      return NULL;
    }
    Node *node = createNode(copy);
    if (node == NULL) {
      *err = BST_ALLOCATION_FAIL;
      tree->destroyData(copy);
      return NULL;
    }
    return node;
  }

  int cmp = tree->compareData(value, root->value);

  if (cmp < 0) {
    root->left = insertValue(tree, root->left, value, err);
  } else if (cmp > 0) {
    root->right = insertValue(tree, root->right, value, err);
  } else {
    // Cleanup old data, replace with new
    void *copy = tree->copyData(value);
    if (copy == NULL) {
      *err = BST_ALLOCATION_FAIL;
      return root;
    }
    tree->destroyData(root->value);
    root->value = copy;
    *err = BST_VALUE_OVERWRITTEN;
    return root;
  }

  if (*err != BST_OK) {
    return root;
  }

  return bstRebalance(root);
}

/**
 * @brief Searches the subtree rooted at root for the given value.
 *
 * @param tree Tree used to compare values.
 * @param root Current subtree root.
 * @param value Value to find.
 * @return Matching node or NULL if not found.
 */
static Node *search(const BST *tree, Node *root, const void *value) {
  if (root == NULL) {
    return NULL;
  }

  const int cmp = tree->compareData(value, root->value);

  if (cmp == 0) {
    return root;
  }

  if (cmp < 0) {
    return search(tree, root->left, value);
  }

  return search(tree, root->right, value);
}

/**
 * @brief Finds the minimum-valued node in a subtree.
 *
 * @param root Root of the subtree.
 * @return Node containing the minimum value or NULL for NULL root.
 */
static Node *findMin(Node *root) {
  if (root == NULL) {
    return NULL;
  }

  return root->left == NULL ? root : findMin(root->left);
}

/**
 * @brief Removes a value from the subtree rooted at root.
 *
 * @param tree Tree that defines the data operations.
 * @param root Current subtree root.
 * @param value Value to remove.
 * @param err Output status. BST_OK on success, BST_NOT_FOUND if the value is
 *            missing or BST_ALLOCATION_FAIL if copying a successor fails.
 * @return Updated subtree root or NULL for NULL root
 */
static Node *removeValue(BST *tree, Node *root, const void *value,
                         BST_STATUS *err) {
  if (err == NULL) {
    return NULL;
  }
  if (root == NULL) {
    *err = BST_NOT_FOUND;
    return NULL;
  }

  const int cmp = tree->compareData(value, root->value);

  if (cmp == 0) {
    if (root->left != NULL && root->right != NULL) {
      // both children
      Node *min = findMin(root->right);
      void *successorCopy = tree->copyData(min->value);
      if (successorCopy == NULL) {
        *err = BST_ALLOCATION_FAIL;
        return root;
      }
      tree->destroyData(root->value);
      root->value = successorCopy;
      root->right = removeValue(tree, root->right, min->value, err);
      if (*err != BST_OK) {
        return root;
      }
    } else {
      // this also covers the case when both are NULL
      Node *tmp = root->left == NULL ? root->right : root->left;
      tree->destroyData(root->value);
      free(root);
      *err = BST_OK;
      return tmp;
    }
  } else if (cmp < 0) {
    root->left = removeValue(tree, root->left, value, err);
    if (*err == BST_NOT_FOUND) {
      return root;
    }
  } else {
    root->right = removeValue(tree, root->right, value, err);
    if (*err == BST_NOT_FOUND) {
      return root;
    }
  }

  if (*err != BST_OK) {
    return root;
  }

  return bstRebalance(root);
}

BST_STATUS bstCreate(BSTCompareFn compare, BSTCopyValueFn copy,
                     BSTDestroyValueFn destroy, BST **bst) {
  if (compare == NULL || copy == NULL || destroy == NULL || bst == NULL) {
    return BST_INVALID_PARAMS;
  }

  *bst = NULL;

  BST *tree = malloc(sizeof(BST));
  if (tree == NULL) {
    return BST_ALLOCATION_FAIL;
  }

  tree->root = NULL;
  tree->compareData = compare;
  tree->copyData = copy;
  tree->destroyData = destroy;
  *bst = tree;
  return BST_OK;
}

BST_STATUS bstDestroy(BST **tree) {
  if (tree == NULL || *tree == NULL) {
    return BST_INVALID_PARAMS;
  }

  destroySubtree(*tree, (*tree)->root);
  free(*tree);
  *tree = NULL;
  return BST_OK;
}

BST_STATUS bstInsert(BST *tree, const void *value) {
  if (tree == NULL || value == NULL) {
    return BST_INVALID_PARAMS;
  }

  BST_STATUS err = BST_OK;
  tree->root = insertValue(tree, tree->root, value, &err);
  return err;
}

BST_STATUS bstSearch(const BST *tree, const void *value, const void **result) {
  if (tree == NULL || value == NULL || result == NULL) {
    return BST_INVALID_PARAMS;
  }

  *result = NULL;
  Node *res = search(tree, tree->root, value);
  if (res == NULL) {
    return BST_NOT_FOUND;
  }

  *result = res->value;
  return BST_OK;
}

BST_STATUS bstRemove(BST *tree, const void *value) {
  if (tree == NULL || value == NULL) {
    return BST_INVALID_PARAMS;
  }
  BST_STATUS err = BST_OK;
  tree->root = removeValue(tree, tree->root, value, &err);
  return err;
}
