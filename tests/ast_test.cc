#include "gtest/gtest.h"

extern "C" {
#include "../src/ast.h"
#include "../src/function_table.h"
}

TEST(AstTest, VariableReferenceRetainsResolvedSymbolPointer) {
  SymbolTable *scope = symbol_table_create_root();
  ASSERT_NE(scope, nullptr);
  Symbol *symbol = nullptr;
  ASSERT_EQ(symbol_table_define(scope, "a", type_scalar(TYPE_INT), true, false,
                                &symbol),
            SYMTABLE_OK);

  ASTNode *node = ast_node_create_variable_ref(symbol, 3U, 5U);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->kind, AST_VARIABLE_REF);
  EXPECT_EQ(node->data.variable.symbol, symbol);

  ast_node_destroy(node);
  symbol_table_destroy(scope);
}

TEST(AstTest, ListConstructorAcceptsOwnedArguments) {
  ASTNode *constructor = ast_node_create_list_constructor(1U, 1U);
  ASTNode *argument = ast_node_create_int(42, 1U, 6U);
  ASSERT_NE(constructor, nullptr);
  ASSERT_NE(argument, nullptr);

  ASSERT_EQ(ast_node_call_append_argument(constructor, argument), AST_OK);
  ASSERT_EQ(constructor->data.call.argument_count, 1U);
  EXPECT_EQ(constructor->data.call.arguments[0], argument);

  ast_node_destroy(constructor);
}

TEST(AstTest, ListIndexingRejectsNullOperandsWithoutTakingOwnership) {
  ASTNode *list = ast_node_create_list_constructor(1U, 1U);
  ASTNode *index = ast_node_create_int(0, 1U, 2U);
  ASSERT_NE(list, nullptr);
  ASSERT_NE(index, nullptr);

  EXPECT_EQ(ast_node_create_list_indexing(nullptr, index, 1U, 1U), nullptr);
  EXPECT_EQ(ast_node_create_list_indexing(list, nullptr, 1U, 1U), nullptr);

  ast_node_destroy(index);
  ast_node_destroy(list);
}

TEST(AstTest, FunctionOwnsAssignedAstRoot) {
  FunctionTable table;
  ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);
  FunctionSymbol *function = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "entry", &function),
            FUNCTION_TABLE_OK);

  ASTNode *root = ast_node_create_int(1, 1U, 1U);
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(function_symbol_set_ast_root(function, root), FUNCTION_TABLE_OK);
  EXPECT_EQ(function->ast_root, root);

  function_table_destroy(&table);
}

TEST(AstTest, CallRetainsFunctionAndArgumentOrder) {
  FunctionTable table;
  ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);
  FunctionSymbol *function = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "foo", &function),
            FUNCTION_TABLE_OK);

  ASTNode *call = ast_node_create_function_call(function, 1U, 1U);
  ASTNode *first = ast_node_create_int(7, 1U, 5U);
  ASTNode *second = ast_node_create_int(11, 1U, 8U);
  ASSERT_NE(call, nullptr);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_EQ(ast_node_call_append_argument(call, first), AST_OK);
  ASSERT_EQ(ast_node_call_append_argument(call, second), AST_OK);

  EXPECT_EQ(call->data.call.function, function);
  ASSERT_EQ(call->data.call.argument_count, 2U);
  EXPECT_EQ(call->data.call.arguments[0], first);
  EXPECT_EQ(call->data.call.arguments[1], second);

  ast_node_destroy(call);
  function_table_destroy(&table);
}

TEST(AstTest, ReturnKeepsExplicitValue) {
  ASTNode *value = ast_node_create_int(7, 1U, 8U);
  ASSERT_NE(value, nullptr);
  ASTNode *node = ast_node_create_return(value, 1U, 1U);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->data.return_statement.value, value);

  ast_node_destroy(node);
}
