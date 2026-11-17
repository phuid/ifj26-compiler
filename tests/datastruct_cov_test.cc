#include "gtest/gtest.h"

extern "C" {
#include "../src/ast.h"
#include "../src/function_table.h"
#include "../src/symtable.h"
}

namespace {

/*
 * AI-generated test block — AST append rejects invalid arguments.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Cover the remaining invalid-argument combinations of
 *   ast_node_call_append_argument and ast_node_block_append_statement
 *   (null argument, non-block target); per ast.h the call returns
 *   AST_INVALID_ARGUMENT and ownership stays with the caller.
 */
class AstAppendCovTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);
    ASSERT_EQ(function_table_get_or_create(&table, "f", &f), FUNCTION_TABLE_OK);
  }
  void TearDown() override { function_table_destroy(&table); }

  FunctionTable table = {};
  FunctionSymbol *f = nullptr;
};

TEST_F(AstAppendCovTest, CallAppendRejectsNullArgument) {
  ASTNode *call = ast_node_create_function_call(f, 2U, 5U);
  ASSERT_NE(call, nullptr);

  EXPECT_EQ(ast_node_call_append_argument(call, nullptr), AST_INVALID_ARGUMENT);
  EXPECT_EQ(call->data.call.argument_count, 0U);
  EXPECT_EQ(call->data.call.arguments, nullptr);

  ASTNode *argument = ast_node_create_int(4, 2U, 7U);
  ASSERT_NE(argument, nullptr);
  ASSERT_EQ(ast_node_call_append_argument(call, argument), AST_OK);
  EXPECT_EQ(call->data.call.argument_count, 1U);
  EXPECT_EQ(call->data.call.arguments[0], argument);
  ast_node_destroy(call);
}

TEST_F(AstAppendCovTest, BlockAppendRejectsNonBlockTargetAndKeepsOwnership) {
  ASTNode *call = ast_node_create_function_call(f, 1U, 1U);
  ASTNode *literal = ast_node_create_int(1, 1U, 1U);
  ASTNode *statement = ast_node_create_return(nullptr, 2U, 3U);
  ASSERT_NE(call, nullptr);
  ASSERT_NE(literal, nullptr);
  ASSERT_NE(statement, nullptr);

  EXPECT_EQ(ast_node_block_append_statement(call, statement),
            AST_INVALID_ARGUMENT);
  EXPECT_EQ(call->data.call.argument_count, 0U);
  EXPECT_EQ(call->data.call.arguments, nullptr);
  EXPECT_EQ(ast_node_block_append_statement(literal, statement),
            AST_INVALID_ARGUMENT);
  EXPECT_EQ(literal->data.int_value, 1);

  // The rejected statement is still owned by the caller.
  ast_node_destroy(statement);
  ast_node_destroy(literal);
  ast_node_destroy(call);
}

/*
 * AI-generated test block — lookup in a scope without a BVS.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Defensive check: a hand-built scope whose bindings BVS is null breaks
 *   the structure invariant (as in symtable_gap_test.cc). lookup_local on it
 *   must find nothing and symbol_table_resolve must skip it and keep
 *   searching its parents.
 */
TEST(SymbolTableCovTest, LookupInScopeWithoutBindingsFindsNothing) {
  SymbolTable *root = symbol_table_create_root();
  ASSERT_NE(root, nullptr);
  Symbol *x = nullptr;
  ASSERT_EQ(symbol_table_define(root, "x", TYPE_INT, true, false, &x),
            SYMTABLE_OK);

  // Dropping the `bindings == NULL` check is an equivalent mutant
  // (bstSearch(NULL, ...) also yields null), so this test mainly guards
  // that resolve skips the broken scope and continues into the parent.
  SymbolTable broken = {nullptr, nullptr, 0U, 0U, nullptr};
  EXPECT_EQ(symbol_table_lookup_local(&broken, "x"), nullptr);
  EXPECT_EQ(symbol_table_resolve(&broken, "x"), nullptr);

  broken.parent = root;
  EXPECT_EQ(symbol_table_lookup_local(&broken, "x"), nullptr);
  EXPECT_EQ(symbol_table_resolve(&broken, "x"), x);
  EXPECT_EQ(symbol_table_resolve(&broken, "y"), nullptr);

  symbol_table_destroy(root);
}

} // namespace
