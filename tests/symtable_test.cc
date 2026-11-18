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
  ASSERT_EQ(symbol_table_define(root, "b", type_unknown(), true, false, &outer),
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
  ASSERT_EQ(symbol_table_define(root, "value", type_scalar(TYPE_INT), true,
                                false, &outer),
            SYMTABLE_OK);
  ASSERT_EQ(symbol_table_define(child, "value", type_scalar(TYPE_REAL), true,
                                false, &inner),
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
  ASSERT_EQ(symbol_table_define(root, "item", type_scalar(TYPE_INT), true,
                                false, &symbol),
            SYMTABLE_OK);
  ASSERT_NE(symbol, nullptr);
  EXPECT_NE(root->bindings->root, nullptr);
  EXPECT_EQ(symbol_table_lookup_local(root, "item"), symbol);

  symbol_table_destroy(root);
}

TEST(SymbolTableTest, RejectsDuplicateInSameScope) {
  SymbolTable *root = symbol_table_create_root();
  ASSERT_NE(root, nullptr);

  ASSERT_EQ(symbol_table_define(root, "a", type_scalar(TYPE_INT), true, false,
                                nullptr),
            SYMTABLE_OK);
  EXPECT_EQ(symbol_table_define(root, "a", type_scalar(TYPE_REAL), true, false,
                                nullptr),
            SYMTABLE_DUPLICATE);
  EXPECT_EQ(symbol_table_resolve(root, "missing"), nullptr);

  symbol_table_destroy(root);
}

TEST(SymbolTableTest, ScopeSymbolListReturnsSymbolArray) {
  SymbolTable *root = symbol_table_create_root();
  ASSERT_NE(root, nullptr);
  SymbolTable *child = symbol_table_create_child(root);
  ASSERT_NE(child, nullptr);

  ASSERT_EQ(symbol_table_define(root, "root_value", type_scalar(TYPE_INT), true,
                                false, nullptr),
            SYMTABLE_OK);
  ASSERT_EQ(symbol_table_define(child, "child_value", type_scalar(TYPE_REAL),
                                true, false, nullptr),
            SYMTABLE_OK);

  SymbolArray symbols = {.data = nullptr, .capacity = 0};
  ASSERT_EQ(symbol_table_get_scope_symbol_list(root, &symbols), SYMTABLE_OK);
  ASSERT_EQ(symbols.capacity, 2);
  ASSERT_NE(symbols.data, nullptr);
  EXPECT_EQ(std::string(symbols.data[0]->name), "root_value");
  EXPECT_EQ(std::string(symbols.data[1]->name), "child_value");

  free(symbols.data);
  symbol_table_destroy(root);
}
