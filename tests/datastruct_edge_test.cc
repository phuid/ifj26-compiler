/*
 * AI-generated test block — AVL tree edge cases (shared helpers).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover the remaining data structure edge cases of bst.c, symtable.c and
 *   dynamic_array.c that the existing suites miss. Provide a direction-
 *   agnostic AVL auditor (stored height, |balance| <= 1, strictly
 *   monotonic in-order traversal equal to the model key set, owned value
 *   count) and tracking copy/destroy callbacks that can detect reads of a
 *   destroyed value without dereferencing it.
 */
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "bst_test_util.h"

extern "C" {
#include "../src/bst.h"
#include "../src/dynamic_array.h"
#include "../src/impl_bst.h"
#include "../src/symtable.h"
}

namespace {

std::set<const void *> liveValues; // values currently owned by the tree
long copyCalls = 0;
long destroyCalls = 0;
const void *watchedValue = nullptr; // value whose destruction is tracked
bool watchedDestroyed = false;
bool destroyedValueTouched = false;

// True when p is the watched value after the tree destroyed it.
bool isDestroyedWatched(const void *p) {
  return watchedDestroyed && p == watchedValue && liveValues.count(p) == 0U;
}

int compareInt(const void *left, const void *right) {
  if (isDestroyedWatched(left) || isDestroyedWatched(right)) {
    destroyedValueTouched = true; // never dereference freed memory
    return 0;
  }
  const int a = *static_cast<const int *>(left);
  const int b = *static_cast<const int *>(right);
  return (a > b) - (a < b);
}

void *copyInt(const void *value) {
  if (isDestroyedWatched(value)) {
    destroyedValueTouched = true;
    return nullptr;
  }
  int *copy = new int(*static_cast<const int *>(value));
  liveValues.insert(copy);
  ++copyCalls;
  return copy;
}

void destroyInt(void *value) {
  ++destroyCalls;
  if (liveValues.erase(value) == 0U) {
    ADD_FAILURE() << "tree destroyed a value it does not own";
    return;
  }
  if (value == watchedValue) {
    watchedDestroyed = true;
  }
  delete static_cast<int *>(value);
}

void resetTracking() {
  liveValues.clear();
  copyCalls = 0;
  destroyCalls = 0;
  watchedValue = nullptr;
  watchedDestroyed = false;
  destroyedValueTouched = false;
}

int valueOf(const BST_Node *node) { return *static_cast<int *>(node->value); }

// Height of the subtree, or -1 after describing the first broken invariant.
int auditNode(const BST_Node *node, std::vector<int> *inorder,
              std::string *error) {
  if (node == nullptr) {
    return 0;
  }
  const int left = auditNode(node->left, inorder, error);
  if (left < 0) {
    return -1;
  }
  inorder->push_back(valueOf(node));
  const int right = auditNode(node->right, inorder, error);
  if (right < 0) {
    return -1;
  }
  const int height = 1 + std::max(left, right);
  // The balance factor is no longer stored; it is derived from the heights.
  if (node->height != height || std::abs(right - left) > 1) {
    *error = "node " + std::to_string(valueOf(node)) + ": stored height " +
             std::to_string(node->height) + ", actual heights L" +
             std::to_string(left) + " R" + std::to_string(right);
    return -1;
  }
  return height;
}

// Checks every AVL invariant and that the tree owns exactly the model keys.
::testing::AssertionResult holdsExactly(const BST *tree,
                                        const std::set<int> &model) {
  std::vector<int> inorder;
  std::string error;
  if (auditNode(tree->root, &inorder, &error) < 0) {
    return ::testing::AssertionFailure() << error;
  }
  const std::vector<int> ascending(model.begin(), model.end());
  const std::vector<int> descending(model.rbegin(), model.rend());
  if (inorder != ascending && inorder != descending) {
    return ::testing::AssertionFailure()
           << "in-order traversal is not the sorted key set (" << inorder.size()
           << " nodes, " << model.size() << " keys)";
  }
  if (liveValues.size() != model.size()) {
    return ::testing::AssertionFailure()
           << "tree owns " << liveValues.size() << " values, expected "
           << model.size();
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult insertAndAudit(BST *tree, int key,
                                          std::set<int> *model) {
  bst_insert(tree, &key);
  model->insert(key);
  return holdsExactly(tree, *model) << " after inserting " << key;
}

::testing::AssertionResult removeAndAudit(BST *tree, int key,
                                          std::set<int> *model) {
  bst_remove(tree, &key);
  model->erase(key);
  return holdsExactly(tree, *model) << " after removing " << key;
}

// Pre-order (value, height) serialization; null children are INT64_MIN.
void shapeOf(const BST_Node *node, std::vector<long long> *shape) {
  if (node == nullptr) {
    shape->push_back(INT64_MIN);
    return;
  }
  shape->push_back(valueOf(node));
  shape->push_back(node->height);
  shapeOf(node->left, shape);
  shapeOf(node->right, shape);
}

std::vector<long long> shapeOf(const BST *tree) {
  std::vector<long long> shape;
  shapeOf(tree->root, &shape);
  return shape;
}

const BST_Node *findNode(const BST_Node *node, int key) {
  if (node == nullptr || valueOf(node) == key) {
    return node;
  }
  const BST_Node *left = findNode(node->left, key);
  return left != nullptr ? left : findNode(node->right, key);
}

int childCount(const BST_Node *node) {
  return (node->left != nullptr ? 1 : 0) + (node->right != nullptr ? 1 : 0);
}

enum class Pattern {
  kAscending,
  kDescending,
  kZigzagOutsideIn,
  kZigzagInsideOut,
  kInterleavedHalves,
  kShuffled,
  kRootFirst // removal only: always remove the current root
};

std::vector<int> zigzagOutsideIn(int count) {
  std::vector<int> keys;
  for (int lo = 0, hi = count - 1; lo <= hi; ++lo, --hi) {
    keys.push_back(lo);
    if (lo != hi) {
      keys.push_back(hi);
    }
  }
  return keys;
}

std::vector<int> makeOrder(Pattern pattern, int count) {
  std::vector<int> keys;
  for (int i = 0; i < count; ++i) {
    keys.push_back(i);
  }
  switch (pattern) {
  case Pattern::kDescending:
    std::reverse(keys.begin(), keys.end());
    break;
  case Pattern::kZigzagOutsideIn:
    keys = zigzagOutsideIn(count);
    break;
  case Pattern::kZigzagInsideOut:
    keys = zigzagOutsideIn(count);
    std::reverse(keys.begin(), keys.end());
    break;
  case Pattern::kInterleavedHalves: {
    const int half = (count + 1) / 2;
    keys.clear();
    for (int i = 0; i < half; ++i) {
      keys.push_back(i);
      if (half + i < count) {
        keys.push_back(half + i);
      }
    }
    break;
  }
  case Pattern::kShuffled: {
    std::mt19937 rng(20261118U); // portable Fisher-Yates on raw output
    for (int i = count - 1; i > 0; --i) {
      std::swap(keys[i], keys[rng() % static_cast<unsigned>(i + 1)]);
    }
    break;
  }
  default:
    break;
  }
  return keys;
}

const char *patternName(Pattern pattern) {
  static const char *const kNames[] = {
      "Ascending",         "Descending", "ZigzagOutsideIn", "ZigzagInsideOut",
      "InterleavedHalves", "Shuffled",   "RootFirst"};
  return kNames[static_cast<int>(pattern)];
}

class BstEdgeTest : public ::testing::Test {
protected:
  void SetUp() override {
    resetTracking();
    tree = make_tree(compareInt, copyInt, destroyInt);
    ASSERT_NE(tree, nullptr);
  }
  void TearDown() override {
    bst_destroy(&tree);
    EXPECT_TRUE(liveValues.empty()) << "tree leaked owned values";
  }
  BST *tree = nullptr;
};

class BstEdgeOrderTest
    : public BstEdgeTest,
      public ::testing::WithParamInterface<std::tuple<Pattern, Pattern>> {};

constexpr int kOrderKeyCount = 300;

} // namespace

/*
 * AI-generated test block — AVL invariants on adversarial operation orders.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   For every combination of insertion order (ascending, descending, both
 *   zig-zags, interleaved halves, shuffled) and removal order (same plus
 *   "always remove the current root", i.e. repeated two-child deletion),
 *   audit the full AVL invariants after every single insert and remove,
 *   then check the emptied tree is reusable.
 */
TEST_P(BstEdgeOrderTest, InvariantsHoldAfterEveryInsertAndRemove) {
  const Pattern insertPattern = std::get<0>(GetParam());
  const Pattern removePattern = std::get<1>(GetParam());
  std::set<int> model;

  for (int key : makeOrder(insertPattern, kOrderKeyCount)) {
    ASSERT_TRUE(insertAndAudit(tree, key, &model));
  }
  if (removePattern == Pattern::kRootFirst) {
    for (int step = 0; tree->root != nullptr && step < kOrderKeyCount; ++step) {
      ASSERT_TRUE(removeAndAudit(tree, valueOf(tree->root), &model));
    }
  } else {
    for (int key : makeOrder(removePattern, kOrderKeyCount)) {
      ASSERT_TRUE(removeAndAudit(tree, key, &model));
    }
  }
  EXPECT_EQ(tree->root, nullptr);
  EXPECT_EQ(destroyCalls, copyCalls);

  for (int key : {2, 1, 3}) {
    ASSERT_TRUE(insertAndAudit(tree, key, &model));
  }
}

INSTANTIATE_TEST_SUITE_P(
    AllOrders, BstEdgeOrderTest,
    ::testing::Combine(
        ::testing::Values(Pattern::kAscending, Pattern::kDescending,
                          Pattern::kZigzagOutsideIn, Pattern::kZigzagInsideOut,
                          Pattern::kInterleavedHalves, Pattern::kShuffled),
        ::testing::Values(Pattern::kAscending, Pattern::kDescending,
                          Pattern::kZigzagOutsideIn, Pattern::kZigzagInsideOut,
                          Pattern::kInterleavedHalves, Pattern::kShuffled,
                          Pattern::kRootFirst)),
    [](const ::testing::TestParamInfo<std::tuple<Pattern, Pattern>> &info) {
      return std::string("Insert") + patternName(std::get<0>(info.param)) +
             "Remove" + patternName(std::get<1>(info.param));
    });

/*
 * AI-generated test block — AVL tree structural edge cases.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Missing keys (gaps, out of range, INT_MIN/INT_MAX) and duplicate inserts
 *   must not change the tree shape; removal from an empty tree and destroying
 *   an empty tree are no-ops; removing or re-inserting through the pointer
 *   returned by bst_search must never read the destroyed value; a 100 000-key
 *   ascending tree stays within the AVL height bound.
 */
TEST_F(BstEdgeTest, MissingKeysAndDuplicatesLeaveShapeUntouched) {
  std::set<int> model;
  for (int key = 0; key < 400; key += 2) {
    ASSERT_TRUE(insertAndAudit(tree, key, &model));
  }
  const std::vector<long long> before = shapeOf(tree);
  const long destroysBefore = destroyCalls;

  for (int key : {INT_MIN, -1, 1, 199, 399, 400, INT_MAX}) {
    bst_remove(tree, &key);
  }
  EXPECT_EQ(shapeOf(tree), before) << "removing missing keys reshaped tree";
  EXPECT_EQ(destroyCalls, destroysBefore);

  for (int key = 0; key < 400; key += 2) {
    bst_insert(tree, &key);
  }
  EXPECT_EQ(shapeOf(tree), before) << "duplicate inserts reshaped tree";
  EXPECT_EQ(destroyCalls - destroysBefore, 200);
  EXPECT_TRUE(holdsExactly(tree, model));
}

TEST_F(BstEdgeTest, EmptyTreeRemoveAndDestroyAreNoOps) {
  const int key = 7;
  bst_remove(tree, &key);
  EXPECT_EQ(tree->root, nullptr);
  EXPECT_EQ(find_value(tree, &key), nullptr);

  bst_destroy(&tree);
  EXPECT_EQ(tree, nullptr);
  bst_destroy(&tree); // a cleared handle stays safe to destroy again
  EXPECT_EQ(copyCalls + destroyCalls, 0);
}

TEST_F(BstEdgeTest, RemovingThroughStoredPointerNeverReadsDestroyedValue) {
  constexpr int kKeys = 12;
  bool removedKind[3] = {false, false, false};
  for (int target = 1; target <= kKeys; ++target) {
    std::set<int> model;
    for (int key = 1; key <= kKeys; ++key) {
      ASSERT_TRUE(insertAndAudit(tree, key, &model));
    }
    const BST_Node *node = findNode(tree->root, target);
    ASSERT_NE(node, nullptr);
    removedKind[childCount(node)] = true;
    watchedValue = find_value(tree, &target);
    ASSERT_EQ(watchedValue, node->value);

    bst_remove(tree, watchedValue); // probe aliases the stored value
    model.erase(target);

    EXPECT_FALSE(destroyedValueTouched) << "removing " << target;
    EXPECT_TRUE(watchedDestroyed) << "removing " << target;
    EXPECT_TRUE(holdsExactly(tree, model)) << "removing " << target;
    bst_destroy(&tree);
    resetTracking();
    tree = make_tree(compareInt, copyInt, destroyInt);
    ASSERT_NE(tree, nullptr);
  }
  EXPECT_TRUE(removedKind[0] && removedKind[1] && removedKind[2])
      << "leaf, one-child and two-child removals must all be exercised";
}

TEST_F(BstEdgeTest, DuplicateInsertFromStoredPointerCopiesBeforeDestroying) {
  std::set<int> model;
  for (int key = 1; key <= 7; ++key) {
    ASSERT_TRUE(insertAndAudit(tree, key, &model));
  }
  for (int key = 1; key <= 7; ++key) {
    watchedValue = find_value(tree, &key);
    watchedDestroyed = false;
    ASSERT_NE(watchedValue, nullptr);

    bst_insert(tree, watchedValue); // copy source is the replaced value

    EXPECT_FALSE(destroyedValueTouched) << "key " << key;
    EXPECT_TRUE(watchedDestroyed) << "old value must be replaced, key " << key;
    const int *stored = static_cast<const int *>(find_value(tree, &key));
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(*stored, key);
  }
  EXPECT_TRUE(holdsExactly(tree, model));
}

TEST_F(BstEdgeTest, HundredThousandAscendingKeysStayWithinAvlBound) {
  constexpr int kLarge = 100000;
  std::set<int> model;
  for (int key = 0; key < kLarge; ++key) {
    bst_insert(tree, &key);
    model.insert(model.end(), key);
  }
  ASSERT_TRUE(holdsExactly(tree, model));
  // Adelson-Velsky & Landis: h < 1.4405 * log2(n + 2) - 0.3277.
  const double bound = 1.4405 * std::log2(kLarge + 2.0) - 0.3277;
  EXPECT_LT(tree->root->height, bound);

  const std::vector<int> order = zigzagOutsideIn(kLarge);
  for (size_t i = 0; i < order.size(); ++i) {
    bst_remove(tree, &order[i]);
    model.erase(order[i]);
    if (i + 1 == order.size() / 2) {
      ASSERT_TRUE(holdsExactly(tree, model)) << "halfway";
    }
  }
  EXPECT_EQ(tree->root, nullptr);
  EXPECT_TRUE(liveValues.empty());
}

/*
 * AI-generated test block — lexical scope tree visibility.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Per IFJ26 spec 2.2 and specifikace.txt, a name is visible in its own
 *   scope and all nested scopes, a nested scope may shadow it, siblings and
 *   cousins never see each other. Check a full visibility matrix over a
 *   branching scope tree, repeated shadowing over many levels, a 10 000-deep
 *   chain, prefix-chain and 64 KiB names, child array growth past 1000
 *   children and that a binding resolved before shadowing stays the outer.
 */
namespace {

class SymbolTableEdgeTest : public ::testing::Test {
protected:
  void SetUp() override { ASSERT_NE(root, nullptr); }
  void TearDown() override { symbol_table_destroy(root); }

  ::testing::AssertionResult define(SymbolTable *scope, const char *name,
                                    Symbol **out = nullptr) {
    const SymbolTableStatus status = symbol_table_define(
        scope, name, type_scalar(TYPE_INT), true, false, out);
    if (status != SYMTABLE_OK) {
      return ::testing::AssertionFailure()
             << "define " << name << " returned " << status;
    }
    return ::testing::AssertionSuccess();
  }

  SymbolTable *root = symbol_table_create_root();
};

} // namespace

TEST_F(SymbolTableEdgeTest, ResolveSeesOnlyOwnScopeAndAncestors) {
  // root{r} -> A{a, shared} -> A1{a1}, A2{a2, shared}; root -> B{b} -> B1{b1,r}
  SymbolTable *a = symbol_table_create_child(root);
  ASSERT_NE(a, nullptr);
  SymbolTable *a1 = symbol_table_create_child(a);
  SymbolTable *a2 = symbol_table_create_child(a);
  SymbolTable *b = symbol_table_create_child(root);
  ASSERT_TRUE(a1 != nullptr && a2 != nullptr && b != nullptr);
  SymbolTable *b1 = symbol_table_create_child(b);
  ASSERT_NE(b1, nullptr);
  SymbolTable *const scopes[] = {root, a, a1, a2, b, b1};
  const char *const names[] = {"r", "a", "shared", "a1", "a2", "b", "b1"};
  const std::pair<int, const char *> defs[] = {
      {0, "r"},      {1, "a"}, {1, "shared"}, {2, "a1"}, {3, "a2"},
      {3, "shared"}, {4, "b"}, {5, "b1"},     {5, "r"}};
  for (const auto &def : defs) {
    ASSERT_TRUE(define(scopes[def.first], def.second));
  }
  // owner index of the binding each scope resolves per name, -1 = unknown
  const int expected[6][7] = {
      {0, -1, -1, -1, -1, -1, -1}, {0, 1, 1, -1, -1, -1, -1},
      {0, 1, 1, 2, -1, -1, -1},    {0, 1, 3, -1, 3, -1, -1},
      {0, -1, -1, -1, -1, 4, -1},  {5, -1, -1, -1, -1, 4, 5}};
  for (int s = 0; s < 6; ++s) {
    for (int n = 0; n < 7; ++n) {
      SCOPED_TRACE(std::string("scope ") + std::to_string(s) + ", " + names[n]);
      const Symbol *found = symbol_table_resolve(scopes[s], names[n]);
      if (expected[s][n] < 0) {
        EXPECT_EQ(found, nullptr);
        continue;
      }
      ASSERT_NE(found, nullptr);
      EXPECT_EQ(found->owner, scopes[expected[s][n]]);
      EXPECT_EQ(found,
                symbol_table_lookup_local(scopes[expected[s][n]], names[n]));
      EXPECT_STREQ(found->name, names[n]);
    }
  }
}

TEST_F(SymbolTableEdgeTest, RepeatedShadowingResolvesNearestDefinition) {
  constexpr int kDepth = 48;
  std::vector<SymbolTable *> scopes(1U, root);
  std::vector<Symbol *> definedAt(kDepth, nullptr);
  for (int level = 0; level < kDepth; ++level) {
    if (level > 0) {
      scopes.push_back(symbol_table_create_child(scopes.back()));
      ASSERT_NE(scopes.back(), nullptr) << level;
    }
    if (level % 3 == 0) {
      ASSERT_TRUE(define(scopes.back(), "x", &definedAt[level]));
    }
  }
  for (int level = 0; level < kDepth; ++level) {
    const int nearest = level - level % 3;
    const Symbol *found = symbol_table_resolve(scopes[level], "x");
    ASSERT_NE(found, nullptr) << level;
    EXPECT_EQ(found, definedAt[nearest]) << level;
    EXPECT_EQ(found->owner, scopes[nearest]) << level;
    EXPECT_EQ(symbol_table_lookup_local(scopes[level], "x") != nullptr,
              level % 3 == 0)
        << level;
  }
}

TEST_F(SymbolTableEdgeTest, TenThousandNestedScopesResolveAndDestroy) {
  constexpr int kDepth = 10000;
  Symbol *top = nullptr;
  Symbol *middle = nullptr;
  ASSERT_TRUE(define(root, "top", &top));
  SymbolTable *scope = root;
  for (int level = 1; level < kDepth; ++level) {
    scope = symbol_table_create_child(scope);
    ASSERT_NE(scope, nullptr) << level;
    if (level == kDepth / 2) {
      ASSERT_TRUE(define(scope, "middle", &middle));
    }
  }
  EXPECT_EQ(symbol_table_resolve(scope, "top"), top);
  EXPECT_EQ(symbol_table_resolve(scope, "middle"), middle);
  EXPECT_EQ(symbol_table_resolve(scope, "absent"), nullptr);
  EXPECT_EQ(symbol_table_resolve(root, "middle"), nullptr);
  // TearDown destroys the whole chain recursively from the root.
}

TEST_F(SymbolTableEdgeTest, BindingResolvedBeforeShadowingStaysOuter) {
  Symbol *outer = nullptr;
  ASSERT_TRUE(define(root, "x", &outer));
  SymbolTable *block = symbol_table_create_child(root);
  ASSERT_NE(block, nullptr);
  Symbol *const seenBefore = symbol_table_resolve(block, "x");

  Symbol *inner = nullptr;
  ASSERT_TRUE(define(block, "x", &inner));

  EXPECT_EQ(seenBefore, outer);
  EXPECT_NE(inner, outer);
  EXPECT_EQ(symbol_table_resolve(block, "x"), inner);
  EXPECT_EQ(symbol_table_lookup_local(root, "x"), outer);
  EXPECT_EQ(outer->owner, root);
  EXPECT_STREQ(outer->name, "x");
  EXPECT_EQ(symbol_table_define(block, "x", type_scalar(TYPE_REAL), true, false,
                                nullptr),
            SYMTABLE_DUPLICATE);
  EXPECT_EQ(symbol_table_resolve(block, "x"), inner);
}

TEST_F(SymbolTableEdgeTest, PrefixChainsAndLongNamesStayDistinct) {
  for (size_t length = 2U; length <= 128U; length += 2U) {
    ASSERT_TRUE(define(root, std::string(length, 'v').c_str()));
  }
  SymbolTable *block = symbol_table_create_child(root);
  ASSERT_NE(block, nullptr);
  for (size_t length = 1U; length <= 129U; ++length) {
    const std::string name(length, 'v');
    const Symbol *found = symbol_table_resolve(block, name.c_str());
    if (length % 2U == 1U) {
      EXPECT_EQ(found, nullptr) << length;
    } else if (found == nullptr) {
      ADD_FAILURE() << "missing prefix name of length " << length;
    } else {
      EXPECT_EQ(std::strlen(found->name), length);
    }
  }
  const std::string base(65536U, 'k');
  Symbol *first = nullptr;
  Symbol *second = nullptr;
  ASSERT_TRUE(define(block, (base + "a").c_str(), &first));
  ASSERT_TRUE(define(block, (base + "b").c_str(), &second));
  EXPECT_NE(first, second);
  EXPECT_EQ(symbol_table_lookup_local(block, (base + "a").c_str()), first);
  EXPECT_EQ(symbol_table_lookup_local(block, base.c_str()), nullptr);
  EXPECT_EQ(symbol_table_lookup_local(block, (base + "c").c_str()), nullptr);

  Symbol *empty = nullptr;
  ASSERT_TRUE(define(block, "", &empty));
  EXPECT_EQ(symbol_table_resolve(block, ""), empty);
  EXPECT_EQ(symbol_table_resolve(root, ""), nullptr);
}

TEST_F(SymbolTableEdgeTest, ChildArrayGrowthKeepsEarlierScopesAndBindings) {
  constexpr size_t kChildren = 1000U;
  SymbolTable *first = symbol_table_create_child(root);
  ASSERT_NE(first, nullptr);
  Symbol *loopVar = nullptr;
  ASSERT_TRUE(define(first, "i", &loopVar));
  std::set<size_t> capacities;
  for (size_t count = 2U; count <= kChildren; ++count) {
    ASSERT_NE(symbol_table_create_child(root), nullptr) << count;
    ASSERT_EQ(root->child_count, count);
    ASSERT_GE(root->child_capacity, count);
    capacities.insert(root->child_capacity);
  }
  // Geometric growth: 1000 children need only ~log2(1000) reallocations.
  EXPECT_LE(capacities.size(), 10U);
  EXPECT_EQ(root->children[0], first);
  EXPECT_EQ(first->parent, root);
  EXPECT_EQ(symbol_table_resolve(first, "i"), loopVar);
  EXPECT_EQ(symbol_table_resolve(root, "i"), nullptr);
  EXPECT_EQ(symbol_table_resolve(root->children[kChildren - 1U], "i"), nullptr);
}

/*
 * AI-generated test block — dynamic_array_reserve growth and limits.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Growth must double from the current capacity (not restart at 4) and keep
 *   the data; required == capacity keeps the pointer; large growth reaches
 *   the next 4 * 2^k; huge but non-overflowing requests whose realloc fails
 *   and element sizes overflowing the byte count both return OUT_OF_MEMORY
 *   with data and capacity untouched; capacity 0 with a foreign buffer.
 */
#if defined(__SANITIZE_ADDRESS__)
#define DS_EDGE_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define DS_EDGE_SANITIZED 1
#endif
#endif

TEST(DynamicArrayEdgeTest, GrowthDoublesFromCurrentCapacity) {
  // {start capacity, required, expected capacity}
  const size_t cases[][3] = {{1, 2, 2},  {1, 3, 4},   {3, 4, 6},
                             {3, 7, 12}, {5, 6, 10},  {6, 100, 192},
                             {7, 7, 7},  {12, 1, 12}, {4, 4, 4}};
  for (const auto &c : cases) {
    SCOPED_TRACE(::testing::Message() << c[0] << " -> " << c[1]);
    int *values = static_cast<int *>(malloc(c[0] * sizeof(int)));
    ASSERT_NE(values, nullptr);
    for (size_t i = 0; i < c[0]; ++i) {
      values[i] = static_cast<int>(100 + i);
    }
    int *const original = values;
    size_t capacity = c[0];
    ASSERT_EQ(dynamic_array_reserve(reinterpret_cast<void **>(&values),
                                    &capacity, c[1], sizeof(int)),
              DYNAMIC_ARRAY_OK);
    EXPECT_EQ(capacity, c[2]);
    if (c[1] <= c[0]) {
      EXPECT_EQ(values, original) << "no growth needed";
    }
    for (size_t i = 0; i < c[0]; ++i) {
      EXPECT_EQ(values[i], static_cast<int>(100 + i)) << i;
    }
    free(values);
  }
}

TEST(DynamicArrayEdgeTest, LargeGrowthReachesNextPowerOfTwoMultiple) {
  void *bytes = nullptr;
  size_t capacity = 0U;
  ASSERT_EQ(dynamic_array_reserve(&bytes, &capacity, 1000001U, 1U),
            DYNAMIC_ARRAY_OK);
  EXPECT_EQ(capacity, 1048576U);
  ASSERT_EQ(dynamic_array_reserve(&bytes, &capacity, 1048577U, 1U),
            DYNAMIC_ARRAY_OK);
  EXPECT_EQ(capacity, 2097152U);
  free(bytes);

  struct Page {
    unsigned char raw[4096];
  };
  Page *pages = nullptr;
  capacity = 0U;
  for (size_t used = 0U; used < 1000U; ++used) {
    ASSERT_EQ(dynamic_array_reserve(reinterpret_cast<void **>(&pages),
                                    &capacity, used + 1U, sizeof(Page)),
              DYNAMIC_ARRAY_OK);
    memset(pages[used].raw, static_cast<int>(used & 0xFFU), sizeof(Page));
  }
  EXPECT_EQ(capacity, 1024U);
  for (size_t used = 0U; used < 1000U; ++used) {
    ASSERT_EQ(pages[used].raw[0], used & 0xFFU) << used;
    ASSERT_EQ(pages[used].raw[4095], used & 0xFFU) << used;
  }
  free(pages);
}

TEST(DynamicArrayEdgeTest, HugeButRepresentableRequestFailsWithoutLosingData) {
#ifdef DS_EDGE_SANITIZED
  GTEST_SKIP() << "ASan aborts on huge allocations instead of returning NULL";
#endif
  // {required, element size}: the first three pass the overflow check and
  // fail in realloc, the last two overflow the byte count.
  const size_t cases[][2] = {{SIZE_MAX / 2U + 2U, 1U},
                             {SIZE_MAX, 1U},
                             {5U, SIZE_MAX / 8U},
                             {1U, SIZE_MAX / 4U + 1U},
                             {1U, SIZE_MAX}};
  for (const auto &c : cases) {
    SCOPED_TRACE(::testing::Message() << c[0] << " x " << c[1]);
    unsigned char *data = static_cast<unsigned char *>(malloc(4U));
    ASSERT_NE(data, nullptr);
    memcpy(data, "abc", 4U);
    unsigned char *const original = data;
    size_t capacity = c[1] == 1U ? 4U : 0U;
    void *raw = c[1] == 1U ? static_cast<void *>(data) : nullptr;

    EXPECT_EQ(dynamic_array_reserve(&raw, &capacity, c[0], c[1]),
              DYNAMIC_ARRAY_OUT_OF_MEMORY);
    EXPECT_EQ(capacity, c[1] == 1U ? 4U : 0U);
    EXPECT_EQ(raw, c[1] == 1U ? static_cast<void *>(original) : nullptr);
    EXPECT_STREQ(reinterpret_cast<char *>(data), "abc");
    free(data);
  }
}

TEST(DynamicArrayEdgeTest, ZeroCapacityForeignBufferIsReallocated) {
  void *data = malloc(1U);
  ASSERT_NE(data, nullptr);
  size_t capacity = 0U;
  ASSERT_EQ(dynamic_array_reserve(&data, &capacity, 1U, sizeof(double)),
            DYNAMIC_ARRAY_OK);
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(capacity, 4U);
  static_cast<double *>(data)[3] = 1.5; // the whole new capacity is usable
  EXPECT_EQ(static_cast<double *>(data)[3], 1.5);
  free(data);
}
