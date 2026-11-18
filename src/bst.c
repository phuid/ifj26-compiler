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
static Node *create_node(void *value) {
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
static BST_STATUS destroy_subtree(BST *tree, Node *root) {
  if (tree == NULL) {
    return BST_INVALID_PARAMS;
  }
  if (root == NULL) {
    return BST_OK;
  }

  destroy_subtree(tree, root->left);
  destroy_subtree(tree, root->right);

  tree->destroy_data(root->value);
  free(root);

  return BST_OK;
}

/**
 * @brief Returns the height of a node.
 *
 * @param node Node to inspect.
 * @return Height of the node, or 0 for NULL.
 */
static int get_node_height(Node *node) {
  return node == NULL ? 0 : node->height;
}

/**
 * @brief Calculates the balance of a node.
 *
 * @param node Tree node.
 * @return Balance of the node or 0 for NULL node.
 */
static int calculate_balance(Node *node) {
  if (node == NULL)
    return 0;

  return get_node_height(node->right) - get_node_height(node->left);
}

/**
 * @brief Computes the height of a node based on its children.
 *
 * @param node Tree node.
 * @return Height of the subtree rooted at the node.
 */
static int calculate_height(Node *node) {
  if (node == NULL) {
    return 0;
  }
  int left_height = get_node_height(node->left);
  int right_height = get_node_height(node->right);
  return max(left_height, right_height) + 1;
}

/**
 * @brief Rotates the subtree rooted at root to the right.
 *
 * @param root Root of the subtree.
 * @return New root or NULL for NULL root node.
 */
static Node *rotate_right(Node *root) {
  Node *new_root = root->left;
  if (new_root == NULL) {
    return NULL;
  }
  root->left = new_root->right;
  new_root->right = root;

  root->height = calculate_height(root);
  new_root->height = calculate_height(new_root);
  return new_root;
}

/**
 * @brief Rotates the subtree rooted at root to the left.
 *
 * @param root Root of the subtree.
 * @return New root or NULL for NULL root node.
 */
static Node *rotate_left(Node *root) {
  Node *new_root = root->right;
  if (new_root == NULL) {
    return NULL;
  }
  root->right = new_root->left;
  new_root->left = root;

  root->height = calculate_height(root);
  new_root->height = calculate_height(new_root);
  return new_root;
}

/**
 * @brief Rebalances a node and its subtree if needed.
 *
 * @param root Root of the subtree to rebalance.
 * @return Root of the balanced subtree.
 */
static Node *bst_rebalance(Node *root) {
  root->height = calculate_height(root);
  int balance = calculate_balance(root);

  if (balance > 1) {
    if (calculate_balance(root->right) < 0) {
      root->right = rotate_right(root->right);
    }
    return rotate_left(root);
  }

  if (balance < -1) {
    if (calculate_balance(root->left) > 0) {
      root->left = rotate_left(root->left);
    }
    return rotate_right(root);
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
static Node *insert_value(BST *tree, Node *root, const void *value,
                          BST_STATUS *err) {
  if (err == NULL) {
    return NULL;
  }
  if (tree == NULL) {
    *err = BST_INVALID_PARAMS;
    return NULL;
  }

  if (root == NULL) {
    void *copy = tree->copy_data(value);
    if (copy == NULL) {
      *err = BST_ALLOCATION_FAIL;
      return NULL;
    }
    Node *node = create_node(copy);
    if (node == NULL) {
      *err = BST_ALLOCATION_FAIL;
      tree->destroy_data(copy);
      return NULL;
    }
    return node;
  }

  int cmp = tree->compare_data(value, root->value);

  if (cmp < 0) {
    root->left = insert_value(tree, root->left, value, err);
  } else if (cmp > 0) {
    root->right = insert_value(tree, root->right, value, err);
  } else {
    // Cleanup old data, replace with new
    void *copy = tree->copy_data(value);
    if (copy == NULL) {
      *err = BST_ALLOCATION_FAIL;
      return root;
    }
    tree->destroy_data(root->value);
    root->value = copy;
    *err = BST_VALUE_OVERWRITTEN;
    return root;
  }

  if (*err != BST_OK) {
    return root;
  }

  return bst_rebalance(root);
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

  const int cmp = tree->compare_data(value, root->value);

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
static Node *find_min(Node *root) {
  if (root == NULL) {
    return NULL;
  }

  return root->left == NULL ? root : find_min(root->left);
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
static Node *remove_value(BST *tree, Node *root, const void *value,
                          BST_STATUS *err) {
  if (err == NULL) {
    return NULL;
  }
  if (root == NULL) {
    *err = BST_NOT_FOUND;
    return NULL;
  }

  const int cmp = tree->compare_data(value, root->value);

  if (cmp == 0) {
    if (root->left != NULL && root->right != NULL) {
      // both children
      Node *min = find_min(root->right);
      void *successor_copy = tree->copy_data(min->value);
      if (successor_copy == NULL) {
        *err = BST_ALLOCATION_FAIL;
        return root;
      }
      tree->destroy_data(root->value);
      root->value = successor_copy;
      root->right = remove_value(tree, root->right, min->value, err);
      if (*err != BST_OK) {
        return root;
      }
    } else {
      // this also covers the case when both are NULL
      Node *tmp = root->left == NULL ? root->right : root->left;
      tree->destroy_data(root->value);
      free(root);
      *err = BST_OK;
      return tmp;
    }
  } else if (cmp < 0) {
    root->left = remove_value(tree, root->left, value, err);
    if (*err == BST_NOT_FOUND) {
      return root;
    }
  } else {
    root->right = remove_value(tree, root->right, value, err);
    if (*err == BST_NOT_FOUND) {
      return root;
    }
  }

  if (*err != BST_OK) {
    return root;
  }

  return bst_rebalance(root);
}

BST_STATUS bst_create(BSTCompareFn compare, BSTCopyValueFn copy,
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
  tree->compare_data = compare;
  tree->copy_data = copy;
  tree->destroy_data = destroy;
  *bst = tree;
  return BST_OK;
}

BST_STATUS bst_destroy(BST **tree) {
  if (tree == NULL || *tree == NULL) {
    return BST_INVALID_PARAMS;
  }

  destroy_subtree(*tree, (*tree)->root);
  free(*tree);
  *tree = NULL;
  return BST_OK;
}

// TODO: bst_insert should return the newly inserted node, so that the caller
// can use it for further operations without having to search for it again.
BST_STATUS bst_insert(BST *tree, const void *value) {
  if (tree == NULL || value == NULL) {
    return BST_INVALID_PARAMS;
  }

  BST_STATUS err = BST_OK;
  tree->root = insert_value(tree, tree->root, value, &err);
  return err;
}

BST_STATUS bst_search(const BST *tree, const void *value, const void **result) {
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

BST_STATUS bst_remove(BST *tree, const void *value) {
  if (tree == NULL || value == NULL) {
    return BST_INVALID_PARAMS;
  }
  BST_STATUS err = BST_OK;
  tree->root = remove_value(tree, tree->root, value, &err);
  return err;
}
