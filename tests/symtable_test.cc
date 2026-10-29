#include "gtest/gtest.h"

extern "C" {
#include "../src/impl_bst.h"
#include "../src/symtable.h"
}

TEST(SymbolTableTest, ChildScopeFindsParentBinding) {
  SymbolTable *root = symbol_table_create_root();
  ASSERT_NE(root, nullptr);
  SymbolTable *child = symbol_table_create_child(root);
  ASSERT_NE(child, nullptr);

  Symbol *outer = nullptr;
  ASSERT_EQ(symbol_table_define(root, "b", TYPE_UNKNOWN, true, &outer),
            SYMTABLE_OK);
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(symbol_table_resolve(child, "b"), outer);

  symbol_table_destroy(root);
}

TEST(SymbolTableTest, ChildBindingShadowsParentBinding) {
  SymbolTable *root = symbol_table_create_root();
  ASSERT_NE(root, nullptr);
  SymbolTable *child = symbol_table_create_child(root);
  ASSERT_NE(child, nullptr);

  Symbol *outer = nullptr;
  Symbol *inner = nullptr;
  ASSERT_EQ(symbol_table_define(root, "value", TYPE_INT, true, &outer),
            SYMTABLE_OK);
  ASSERT_EQ(symbol_table_define(child, "value", TYPE_REAL, true, &inner),
            SYMTABLE_OK);

  ASSERT_NE(outer, inner);
  EXPECT_EQ(symbol_table_resolve(child, "value"), inner);
  EXPECT_EQ(symbol_table_resolve(root, "value"), outer);

  symbol_table_destroy(root);
}

TEST(SymbolTableTest, LocalBindingsAreStoredInMainBst) {
  SymbolTable *root = symbol_table_create_root();
  Symbol *symbol = nullptr;

  ASSERT_NE(root, nullptr);
  ASSERT_NE(root->bindings, nullptr);
  ASSERT_EQ(symbol_table_define(root, "item", TYPE_INT, true, &symbol),
            SYMTABLE_OK);
  ASSERT_NE(symbol, nullptr);
  EXPECT_NE(root->bindings->root, nullptr);
  EXPECT_EQ(symbol_table_lookup_local(root, "item"), symbol);

  symbol_table_destroy(root);
}

TEST(SymbolTableTest, RejectsDuplicateInSameScope) {
  SymbolTable *root = symbol_table_create_root();
  ASSERT_NE(root, nullptr);

  ASSERT_EQ(symbol_table_define(root, "a", TYPE_INT, true, nullptr),
            SYMTABLE_OK);
  EXPECT_EQ(symbol_table_define(root, "a", TYPE_REAL, true, nullptr),
            SYMTABLE_DUPLICATE);
  EXPECT_EQ(symbol_table_resolve(root, "missing"), nullptr);

  symbol_table_destroy(root);
}
