#include <cstdio>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "../src/function_table.h"
}

TEST(FunctionTableExtraTest, LookupFindsOnlyCreatedFunctions) {
  FunctionTable table;
  ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);

  FunctionSymbol *print = nullptr;
  FunctionSymbol *println = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "print", &print),
            FUNCTION_TABLE_OK);
  ASSERT_EQ(function_table_get_or_create(&table, "println", &println),
            FUNCTION_TABLE_OK);
  ASSERT_NE(print, nullptr);
  ASSERT_NE(println, nullptr);
  ASSERT_NE(print, println);

  EXPECT_EQ(function_table_lookup(&table, "print"), print);
  EXPECT_EQ(function_table_lookup(&table, "println"), println);
  EXPECT_EQ(function_table_lookup(&table, "printl"), nullptr);
  EXPECT_EQ(function_table_lookup(&table, "Print"), nullptr);
  EXPECT_EQ(function_table_lookup(&table, "pri"), nullptr);
  EXPECT_EQ(function_table_lookup(&table, ""), nullptr);
  EXPECT_EQ(table.function_count, 2U);

  EXPECT_FALSE(print->is_defined);
  EXPECT_FALSE(print->is_main);
  EXPECT_FALSE(println->is_defined);
  EXPECT_FALSE(println->is_main);

  function_table_destroy(&table);
}

TEST(FunctionTableExtraTest, LocalScopeIsCreatedOnlyOnce) {
  FunctionTable table;
  ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);

  FunctionSymbol *average = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "average", &average),
            FUNCTION_TABLE_OK);
  ASSERT_NE(average, nullptr);
  EXPECT_EQ(average->local_scope, nullptr);

  SymbolTable *first = function_symbol_create_local_scope(average);
  ASSERT_NE(first, nullptr);
  Symbol *sum = nullptr;
  ASSERT_EQ(symbol_table_define(first, "sum", type_scalar(TYPE_REAL), true,
                                false, &sum),
            SYMTABLE_OK);
  ASSERT_NE(sum, nullptr);

  SymbolTable *second = function_symbol_create_local_scope(average);
  EXPECT_EQ(second, first);
  EXPECT_EQ(average->local_scope, first);
  EXPECT_EQ(symbol_table_lookup_local(average->local_scope, "sum"), sum);
  EXPECT_EQ(function_symbol_create_local_scope(nullptr), nullptr);

  function_table_destroy(&table);
}

TEST(FunctionTableExtraTest, FunctionPointersStayValidWhenTableGrows) {
  const size_t count = 64U;
  FunctionTable table;
  ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);
  std::vector<FunctionSymbol *> created;

  for (size_t index = 0U; index < count; index += 1U) {
    char name[32];
    snprintf(name, sizeof(name), "helper_%zu", index);
    FunctionSymbol *function = nullptr;
    ASSERT_EQ(function_table_get_or_create(&table, name, &function),
              FUNCTION_TABLE_OK)
        << index;
    ASSERT_NE(function, nullptr) << index;
    created.push_back(function);
  }
  ASSERT_EQ(table.function_count, count);
  EXPECT_GE(table.function_capacity, count);

  for (size_t index = 0U; index < count; index += 1U) {
    char name[32];
    snprintf(name, sizeof(name), "helper_%zu", index);
    EXPECT_EQ(function_table_lookup(&table, name), created[index]) << index;
    EXPECT_STREQ(created[index]->name, name) << index;
  }

  function_table_destroy(&table);
}

/*
 * AI-generated test block — second AST root for one function (known bug).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Implement only the test below. Model a parser that reaches `def f()`
 *   twice and hands a second body to function_symbol_set_ast_root. The call
 *   must be rejected without touching the first body, the caller keeps the
 *   rejected node, and function_table_destroy must free everything once.
 */
// Disabled: set_ast_root destroys the first root on a second call but keeps
// the dangling pointer, so function_table_destroy frees it again. The header
// says the call replaces the root, the implementation rejects it.
TEST(FunctionTableExtraTest, DISABLED_SecondAstRootIsRejectedSafely) {
  FunctionTable table;
  ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);
  FunctionSymbol *f = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "f", &f), FUNCTION_TABLE_OK);
  ASSERT_NE(f, nullptr);

  ASTNode *first_body = ast_node_create_block(1U, 9U);
  ASSERT_NE(first_body, nullptr);
  ASTNode *second_body = ast_node_create_block(4U, 9U);
  ASSERT_NE(second_body, nullptr);

  ASSERT_EQ(function_symbol_set_ast_root(f, first_body), FUNCTION_TABLE_OK);
  EXPECT_EQ(function_symbol_set_ast_root(f, second_body),
            FUNCTION_TABLE_INVALID_ARGUMENT);
  EXPECT_EQ(f->ast_root, first_body);
  EXPECT_EQ(f->ast_root->kind, AST_BLOCK);
  EXPECT_EQ(f->ast_root->line, 1U);

  ast_node_destroy(second_body);
  function_table_destroy(&table);
}
