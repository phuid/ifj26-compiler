#include <chrono>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "../src/ast.h"
#include "../src/bst.h"
#include "../src/symtable.h"
#include "../src/type.h"
}

/*
 * AI-generated test block — data structure defects found by bug hunting.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write GoogleTest cases for three data structure defects: resolving a
 *   chain of dependency diamonds takes exponential time because
 *   type_entity_resolve revisits dependents that already have the type
 *   (specifikace.txt only recurses into UNKNOWN dependents); destroying an
 *   AST node leaves its inferred_type registered as a dependent of the
 *   symbol's type, so a later resolve writes to freed memory; and removing
 *   a BST node with two children frees the stored value of a different key,
 *   invalidating pointers returned by bstSearch. Do not dereference freed
 *   memory in the assertions. These tests are expected to fail until fixed.
 */
namespace {

int compare_int(const void *a, const void *b) {
  int left = *static_cast<const int *>(a);
  int right = *static_cast<const int *>(b);
  return (left > right) - (left < right);
}

void *copy_int(const void *value) {
  int *copy = static_cast<int *>(malloc(sizeof(int)));
  if (copy != nullptr) {
    *copy = *static_cast<const int *>(value);
  }
  return copy;
}

void destroy_int(void *value) { free(value); }

} // namespace

TEST(DataStructBugTest, DiamondChainResolvesInLinearTime) {
  // top -> {a, b} -> bottom, where every bottom is the next diamond's top.
  constexpr int kDiamonds = 28;
  std::vector<TypeEntity> entities(3 * kDiamonds + 1);
  for (TypeEntity &entity : entities) {
    type_entity_init(&entity);
  }
  for (int i = 0; i < kDiamonds; i++) {
    TypeEntity *top = &entities[3 * i];
    TypeEntity *a = &entities[3 * i + 1];
    TypeEntity *b = &entities[3 * i + 2];
    TypeEntity *bottom = &entities[3 * i + 3];
    ASSERT_EQ(type_entity_depend(top, a), TYPE_RESOLVE_OK);
    ASSERT_EQ(type_entity_depend(top, b), TYPE_RESOLVE_OK);
    ASSERT_EQ(type_entity_depend(a, bottom), TYPE_RESOLVE_OK);
    ASSERT_EQ(type_entity_depend(b, bottom), TYPE_RESOLVE_OK);
  }

  auto start = std::chrono::steady_clock::now();
  EXPECT_EQ(type_entity_resolve(&entities[0], type_scalar(TYPE_INT)),
            TYPE_RESOLVE_OK);
  auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_LT(elapsed, std::chrono::milliseconds(500));
  EXPECT_TRUE(type_is_scalar(entities.back().type, TYPE_INT));
  for (TypeEntity &entity : entities) {
    type_entity_destroy(&entity);
  }
}

TEST(DataStructBugTest, DestroyedAstNodeIsNoLongerADependent) {
  SymbolTable *root = symbol_table_create_root();
  ASSERT_NE(root, nullptr);
  Symbol *symbol = nullptr;
  ASSERT_EQ(symbol_table_define(root, "x", type_unknown(), false, false,
                                &symbol),
            SYMTABLE_OK);
  ASTNode *reference = ast_node_create_variable_ref(symbol, 1, 1);
  ASSERT_NE(reference, nullptr);
  ASSERT_EQ(type_entity_depend(&symbol->type, &reference->inferred_type),
            TYPE_RESOLVE_OK);

  ast_node_destroy(reference);

  // A remaining edge points into freed memory; resolving the symbol now
  // would write there, so only the edge count is checked.
  EXPECT_EQ(symbol->type.dependent_count, 0U);
  symbol_table_destroy(root);
}

TEST(DataStructBugTest, TwoChildRemovalKeepsOtherValuePointersStable) {
  BST *tree = bstCreate(compare_int, copy_int, destroy_int);
  ASSERT_NE(tree, nullptr);
  for (int key : {20, 10, 30, 5, 15, 25, 35}) {
    bstInsert(tree, &key);
  }
  int kept = 15;
  int removed = 20;
  void *kept_before = bstSearch(tree, &kept);
  ASSERT_NE(kept_before, nullptr);

  bstRemove(tree, &removed);

  // bst.h: bstSearch returns a pointer to the stored value; removing another
  // key must not free and replace it.
  EXPECT_EQ(bstSearch(tree, &kept), kept_before);
  bstDestroy(&tree);
}
