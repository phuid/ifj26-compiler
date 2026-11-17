#include <climits>

#include "gtest/gtest.h"

extern "C" {
#include "../src/ast.h"
#include "../src/function_table.h"
}

static ASTNode *Int(long long v) { return ast_node_create_int(v, 1U, 1U); }
static void Append(ASTNode *block, ASTNode *statement) {
  ASSERT_EQ(ast_node_block_append_statement(block, statement), AST_OK);
}
static void AppendArg(ASTNode *call, ASTNode *argument) {
  ASSERT_EQ(ast_node_call_append_argument(call, argument), AST_OK);
}

class AstGapTest : public ::testing::Test {
protected:
  void SetUp() override {
    scope = symbol_table_create_root();
    ASSERT_EQ(symbol_table_define(scope, "x", TYPE_INT, true, false, &x),
              SYMTABLE_OK);
    ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);
    ASSERT_EQ(function_table_get_or_create(&table, "f", &f), FUNCTION_TABLE_OK);
  }
  void TearDown() override {
    function_table_destroy(&table);
    symbol_table_destroy(scope);
  }
  SymbolTable *scope = nullptr;
  Symbol *x = nullptr;
  FunctionTable table{};
  FunctionSymbol *f = nullptr;
};

/*
 * AI-generated test block — binary operator kind table.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   create_binary_op accepts all 10 binary kinds and rejects non-binary ones.
 */
TEST_F(AstGapTest, BinaryOpKindTable) {
  for (ASTNodeKind kind :
       {AST_ADD, AST_SUBTRACT, AST_MULTIPLY, AST_DIVIDE, AST_LESS, AST_EQUAL,
        AST_LESS_EQUAL, AST_GREATER, AST_GREATER_EQUAL, AST_NOT_EQUAL}) {
    ASTNode *node = ast_node_create_binary_op(kind, Int(1), Int(2), 1U, 1U);
    ASSERT_NE(node, nullptr) << kind;
    EXPECT_EQ(node->kind, kind);
    ast_node_destroy(node);
  }
  ASTNode *left = Int(1);
  ASTNode *right = Int(2);
  for (ASTNodeKind kind : {AST_CONSTANT_INT, AST_IF, AST_ASSIGN, AST_BLOCK}) {
    EXPECT_EQ(ast_node_create_binary_op(kind, left, right, 1U, 1U), nullptr);
  }
  ast_node_destroy(right);
  ast_node_destroy(left);
}

/*
 * AI-generated test block — remaining constructors.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   real/unit/assign/return/if constructors: kind + key field + destroy.
 */
TEST_F(AstGapTest, RemainingConstructorsSetKindAndKeyField) {
  ASTNode *real = ast_node_create_real(3.25, 1U, 1U);
  ASTNode *unit = ast_node_create_unit(1U, 1U);
  ASTNode *assign = ast_node_create_assign(x, Int(3), 1U, 1U);
  ASTNode *ret = ast_node_create_return(nullptr, 1U, 1U);
  ASTNode *then_block = ast_node_create_block(1U, 1U);
  ASTNode *branch = ast_node_create_if(Int(1), then_block, nullptr, 1U, 1U);
  EXPECT_EQ(real->kind, AST_CONSTANT_REAL);
  EXPECT_DOUBLE_EQ(real->data.real_value, 3.25);
  EXPECT_EQ(unit->kind, AST_CONSTANT_UNIT);
  EXPECT_EQ(assign->kind, AST_ASSIGN);
  EXPECT_EQ(assign->data.assignment.target, x);
  EXPECT_EQ(ret->kind, AST_RETURN);
  EXPECT_EQ(ret->data.return_statement.value, nullptr);
  EXPECT_EQ(branch->kind, AST_IF);
  EXPECT_EQ(branch->data.if_statement.then_branch, then_block);
  for (ASTNode *node : {real, unit, assign, ret, branch}) {
    ast_node_destroy(node);
  }
}

/*
 * AI-generated test block — invalid arguments, growth and destruction.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   NULL args, 50-arg growth, deep tree destroy, borrowed records survive.
 */
TEST_F(AstGapTest, NullArgumentsAreRejected) {
  constexpr ASTStatus kBad = AST_INVALID_ARGUMENT;
  ASTNode *child = Int(1);
  ASTNode *block = ast_node_create_block(1U, 1U);
  EXPECT_EQ(ast_node_create_binary_op(AST_ADD, nullptr, nullptr, 1U, 1U),
            nullptr);
  EXPECT_EQ(ast_node_create_assign(nullptr, child, 1U, 1U), nullptr);
  EXPECT_EQ(ast_node_create_assign(x, nullptr, 1U, 1U), nullptr);
  EXPECT_EQ(ast_node_create_if(nullptr, block, nullptr, 1U, 1U), nullptr);
  EXPECT_EQ(ast_node_create_if(child, nullptr, nullptr, 1U, 1U), nullptr);
  EXPECT_EQ(ast_node_create_variable_ref(nullptr, 1U, 1U), nullptr);
  EXPECT_EQ(ast_node_create_function_call(nullptr, 1U, 1U), nullptr);
  EXPECT_EQ(ast_node_call_append_argument(nullptr, child), kBad);
  EXPECT_EQ(ast_node_call_append_argument(block, child), kBad);
  EXPECT_EQ(ast_node_block_append_statement(nullptr, child), kBad);
  EXPECT_EQ(ast_node_block_append_statement(block, nullptr), kBad);
  EXPECT_EQ(block->data.block.statement_count, 0U);
  ast_node_destroy(block);
  ast_node_destroy(child);
}

TEST_F(AstGapTest, CallAppendGrowsAndKeepsOrder) {
  constexpr long long kCount = 50;
  ASTNode *call = ast_node_create_function_call(f, 1U, 1U);
  for (long long i = 0; i < kCount; i++) {
    AppendArg(call, Int(i));
  }
  ASSERT_EQ(call->data.call.argument_count, static_cast<size_t>(kCount));
  for (long long i = 0; i < kCount; i++) {
    EXPECT_EQ(call->data.call.arguments[i]->data.int_value, i);
  }
  ast_node_destroy(call);
}

TEST_F(AstGapTest, DestroyDeepMixedTree) {
  ASTNode *call = ast_node_create_function_call(f, 1U, 1U);
  AppendArg(call, ast_node_create_string("s", 1U, 1U));
  AppendArg(call, ast_node_create_implicit_int_to_real(Int(2)));
  ASTNode *nested = ast_node_create_block(1U, 1U);
  for (int depth = 0; depth < 100; depth++) {
    ASTNode *outer = ast_node_create_block(1U, 1U);
    Append(outer, nested);
    nested = outer;
  }
  ASTNode *then_branch = ast_node_create_block(1U, 1U);
  Append(then_branch, ast_node_create_return(call, 1U, 1U));
  Append(then_branch, ast_node_create_assign(x, Int(7), 1U, 1U));
  ASTNode *cond = ast_node_create_binary_op(
      AST_LESS, ast_node_create_variable_ref(x, 1U, 1U), Int(10), 1U, 1U);
  ast_node_destroy(ast_node_create_if(cond, then_branch, nested, 1U, 1U));
}

TEST_F(AstGapTest, DestroyKeepsBorrowedSymbolAndFunction) {
  ASTNode *call = ast_node_create_function_call(f, 1U, 1U);
  AppendArg(call, ast_node_create_variable_ref(x, 1U, 1U));
  ast_node_destroy(ast_node_create_assign(x, call, 1U, 1U));
  EXPECT_EQ(symbol_table_lookup_local(scope, "x"), x);
  EXPECT_EQ(function_table_lookup(&table, "f"), f);
}

/*
 * AI-generated test block — constructor fields and positions.
 * Model: Claude (Claude Code).
 * Prompt:
 *   return/if/assign/int constructors store the given children, flags,
 *   line/column and full long long range; destroy(NULL) is a no-op.
 */
TEST_F(AstGapTest, ConstructorsStoreChildrenFlagsAndPosition) {
  ASTNode *value = Int(7);
  ASTNode *ret = ast_node_create_return(value, 4U, 9U);
  ASSERT_NE(ret, nullptr);
  EXPECT_EQ(ret->data.return_statement.value, value);
  EXPECT_EQ(ret->line, 4U);
  EXPECT_EQ(ret->column, 9U);

  ASTNode *cond = Int(1);
  ASTNode *then_block = ast_node_create_block(5U, 8U);
  ASTNode *else_block = ast_node_create_block(6U, 3U);
  ASTNode *branch = ast_node_create_if(cond, then_block, else_block, 5U, 3U);
  ASSERT_NE(branch, nullptr);
  EXPECT_EQ(branch->data.if_statement.condition, cond);
  EXPECT_EQ(branch->data.if_statement.else_branch, else_block);
  EXPECT_EQ(branch->line, 5U);
  EXPECT_EQ(branch->column, 3U);

  ASTNode *assign = ast_node_create_assign(x, Int(3), 7U, 5U);
  ASSERT_NE(assign, nullptr);
  EXPECT_FALSE(assign->data.assignment.is_definition);
  EXPECT_EQ(assign->data.assignment.value->data.int_value, 3);
  EXPECT_EQ(assign->line, 7U);
  EXPECT_EQ(assign->column, 5U);

  ASTNode *low = ast_node_create_int(LLONG_MIN, 1U, 1U);
  ASTNode *high = ast_node_create_int(LLONG_MAX, 1U, 1U);
  ASSERT_TRUE(low != nullptr && high != nullptr);
  EXPECT_EQ(low->data.int_value, LLONG_MIN);
  EXPECT_EQ(high->data.int_value, LLONG_MAX);

  ast_node_destroy(nullptr);
  for (ASTNode *node : {ret, branch, assign, low, high}) {
    ast_node_destroy(node);
  }
}
