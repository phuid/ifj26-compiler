#include "gtest/gtest.h"

extern "C" {
#include "../src/ast.h"
}

TEST(AstExtraTest, BlockKeepsStatementOrder) {
  const size_t count = 50U;
  ASTNode *body = ast_node_create_block(2U, 3U);
  ASSERT_NE(body, nullptr);
  ASTNode *statements[50] = {nullptr};

  for (size_t index = 0U; index < count; index += 1U) {
    statements[index] =
        ast_node_create_int(static_cast<long long>(index) * 10, index + 3U, 3U);
    ASSERT_NE(statements[index], nullptr) << index;
    ASSERT_EQ(ast_node_block_append_statement(body, statements[index]), AST_OK)
        << index;
  }

  ASSERT_EQ(body->data.block.statement_count, count);
  EXPECT_GE(body->data.block.statement_capacity, count);
  for (size_t index = 0U; index < count; index += 1U) {
    ASTNode *statement = body->data.block.statements[index];
    EXPECT_EQ(statement, statements[index]) << index;
    EXPECT_EQ(statement->data.int_value, static_cast<long long>(index) * 10)
        << index;
    EXPECT_EQ(statement->line, index + 3U) << index;
  }

  ast_node_destroy(body);
}

TEST(AstExtraTest, BinaryOpOwnsBothOperands) {
  ASTNode *one = ast_node_create_int(1, 4U, 15U);
  ASTNode *two = ast_node_create_int(2, 4U, 19U);
  ASTNode *three = ast_node_create_int(3, 4U, 25U);
  ASSERT_NE(one, nullptr);
  ASSERT_NE(two, nullptr);
  ASSERT_NE(three, nullptr);

  ASTNode *sum = ast_node_create_binary_op(AST_ADD, one, two, 4U, 17U);
  ASSERT_NE(sum, nullptr);
  ASTNode *product =
      ast_node_create_binary_op(AST_MULTIPLY, sum, three, 4U, 22U);
  ASSERT_NE(product, nullptr);

  EXPECT_EQ(product->kind, AST_MULTIPLY);
  EXPECT_EQ(product->line, 4U);
  EXPECT_EQ(product->column, 22U);
  EXPECT_EQ(product->data.binary_op.left, sum);
  EXPECT_EQ(product->data.binary_op.right, three);
  EXPECT_EQ(sum->kind, AST_ADD);
  EXPECT_EQ(sum->line, 4U);
  EXPECT_EQ(sum->column, 17U);
  EXPECT_EQ(sum->data.binary_op.left, one);
  EXPECT_EQ(sum->data.binary_op.right, two);
  EXPECT_EQ(one->data.int_value, 1);
  EXPECT_EQ(two->data.int_value, 2);
  EXPECT_EQ(three->data.int_value, 3);

  ast_node_destroy(product);
}

TEST(AstExtraTest, BinaryOpRejectsMissingOperandOrWrongKind) {
  ASTNode *limit = ast_node_create_int(10, 7U, 12U);
  ASTNode *step = ast_node_create_int(2, 7U, 17U);
  ASSERT_NE(limit, nullptr);
  ASSERT_NE(step, nullptr);

  EXPECT_EQ(ast_node_create_binary_op(AST_LESS, limit, nullptr, 7U, 15U),
            nullptr);
  EXPECT_EQ(ast_node_create_binary_op(AST_LESS, nullptr, step, 7U, 15U),
            nullptr);
  EXPECT_EQ(ast_node_create_binary_op(AST_RETURN, limit, step, 7U, 15U),
            nullptr);
  EXPECT_EQ(ast_node_create_binary_op(AST_BLOCK, limit, step, 7U, 15U),
            nullptr);
  EXPECT_EQ(
      ast_node_create_binary_op(AST_IMPLICIT_INT_TO_REAL, limit, step, 7U, 15U),
      nullptr);

  EXPECT_EQ(limit->kind, AST_CONSTANT_INT);
  EXPECT_EQ(limit->data.int_value, 10);
  EXPECT_EQ(step->data.int_value, 2);

  ast_node_destroy(step);
  ast_node_destroy(limit);
}

TEST(AstExtraTest, ImplicitConversionWrapsIntAsReal) {
  ASTNode *literal = ast_node_create_int(5, 6U, 19U);
  ASSERT_NE(literal, nullptr);

  ASTNode *conversion = ast_node_create_implicit_int_to_real(literal);
  ASSERT_NE(conversion, nullptr);

  EXPECT_EQ(conversion->kind, AST_IMPLICIT_INT_TO_REAL);
  EXPECT_EQ(conversion->data.conversion.operand, literal);
  EXPECT_TRUE(
      type_equal(conversion->inferred_type, type_scalar(TYPE_REAL)));
  EXPECT_EQ(conversion->line, 6U);
  EXPECT_EQ(conversion->column, 19U);
  EXPECT_EQ(literal->kind, AST_CONSTANT_INT);
  EXPECT_EQ(literal->data.int_value, 5);
  EXPECT_EQ(ast_node_create_implicit_int_to_real(nullptr), nullptr);

  ast_node_destroy(conversion);
}

TEST(AstExtraTest, StringLiteralCopiesText) {
  char token_text[] = "Hello, IFJ26";
  ASTNode *greeting = ast_node_create_string(token_text, 2U, 11U);
  ASSERT_NE(greeting, nullptr);
  token_text[0] = 'J';
  token_text[7] = '\0';

  EXPECT_EQ(greeting->kind, AST_CONSTANT_STRING);
  EXPECT_NE(greeting->data.string_value, token_text);
  EXPECT_STREQ(greeting->data.string_value, "Hello, IFJ26");
  EXPECT_TRUE(
      type_equal(greeting->inferred_type, type_scalar(TYPE_STRING)));
  EXPECT_EQ(greeting->line, 2U);
  EXPECT_EQ(greeting->column, 11U);

  char empty_text[] = "";
  ASTNode *empty = ast_node_create_string(empty_text, 3U, 11U);
  ASSERT_NE(empty, nullptr);
  EXPECT_NE(empty->data.string_value, empty_text);
  EXPECT_STREQ(empty->data.string_value, "");
  EXPECT_EQ(ast_node_create_string(nullptr, 4U, 11U), nullptr);

  ast_node_destroy(empty);
  ast_node_destroy(greeting);
}
