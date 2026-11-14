#include <cstdio>

#include "gtest/gtest.h"

extern "C" {
#include "../src/parser.h"
}

namespace {

FILE *stream_from(const char *text) {
  FILE *stream = tmpfile();
  if (stream == nullptr) {
    return nullptr;
  }
  fputs(text, stream);
  rewind(stream);
  return stream;
}

} // namespace

TEST(ParserDirectLexerTest, EmptyInputFinishesWithoutSyntaxRules) {
  FILE *stream = stream_from("");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_OK);
  EXPECT_EQ(program.functions.function_count, 0U);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(ParserDirectLexerTest, IfOperandIsMarkedAsExpression) {
  FILE *stream =
      stream_from("def foo() =\n  val value = if true then 1 else 2");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  ASSERT_EQ(p_program(&lexer, &program), PARSER_OK);

  FunctionSymbol *function = function_table_lookup(&program.functions, "foo");
  ASSERT_NE(function, nullptr);
  ASSERT_NE(function->ast_root, nullptr);
  ASSERT_EQ(function->ast_root->kind, AST_BLOCK);
  ASSERT_EQ(function->ast_root->data.block.statement_count, 1U);
  ASTNode *assignment = function->ast_root->data.block.statements[0];
  ASSERT_EQ(assignment->kind, AST_ASSIGN);
  ASTNode *conditional = assignment->data.assignment.value;
  ASSERT_EQ(conditional->kind, AST_IF);
  EXPECT_TRUE(conditional->data.if_statement.used_as_an_expression);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(ParserDirectLexerTest, ElseIfOperandCreatesNestedConditional) {
  FILE *stream = stream_from(
      "def foo() =\n  val value = if true then 1 else if false then 2 else 3");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  ASSERT_EQ(p_program(&lexer, &program), PARSER_OK);

  FunctionSymbol *function = function_table_lookup(&program.functions, "foo");
  ASSERT_NE(function, nullptr);
  ASSERT_NE(function->ast_root, nullptr);
  ASSERT_EQ(function->ast_root->kind, AST_BLOCK);
  ASSERT_EQ(function->ast_root->data.block.statement_count, 1U);
  ASTNode *assignment = function->ast_root->data.block.statements[0];
  ASSERT_EQ(assignment->kind, AST_ASSIGN);
  ASTNode *outer = assignment->data.assignment.value;
  ASSERT_EQ(outer->kind, AST_IF);
  ASTNode *nested = outer->data.if_statement.else_branch;
  ASSERT_NE(nested, nullptr);
  ASSERT_EQ(nested->kind, AST_IF);
  ASSERT_NE(nested->data.if_statement.then_branch, nullptr);
  ASSERT_NE(nested->data.if_statement.else_branch, nullptr);
  EXPECT_EQ(nested->data.if_statement.then_branch->kind, AST_CONSTANT_INT);
  EXPECT_EQ(nested->data.if_statement.else_branch->kind, AST_CONSTANT_INT);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(ParserDirectLexerTest, IfExpressionMarksNestedBranchesAsExpressions) {
  FILE *stream = stream_from(
      "def foo() =\n  val value = if true then if false then 1 else 2 else 3");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  ASSERT_EQ(p_program(&lexer, &program), PARSER_OK);

  FunctionSymbol *function = function_table_lookup(&program.functions, "foo");
  ASSERT_NE(function, nullptr);
  ASSERT_NE(function->ast_root, nullptr);
  ASSERT_EQ(function->ast_root->kind, AST_BLOCK);
  ASSERT_EQ(function->ast_root->data.block.statement_count, 1U);
  ASTNode *assignment = function->ast_root->data.block.statements[0];
  ASSERT_EQ(assignment->kind, AST_ASSIGN);
  ASTNode *outer = assignment->data.assignment.value;
  ASSERT_EQ(outer->kind, AST_IF);
  ASTNode *then_if = outer->data.if_statement.then_branch;
  ASSERT_NE(then_if, nullptr);
  ASSERT_EQ(then_if->kind, AST_IF);
  EXPECT_TRUE(outer->data.if_statement.used_as_an_expression);
  EXPECT_TRUE(then_if->data.if_statement.used_as_an_expression);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(ParserDirectLexerTest, ElseIfOperandWithoutFinalElseIsAllowed) {
  FILE *stream = stream_from(
      "def foo() =\n  val value = if true then 1 else if false then 2");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_OK);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(ParserDirectLexerTest, ParenthesizedElseIfCanStartOnNextLine) {
  FILE *stream = stream_from("def foo() =\n"
                             "  val value = (if true then 1 else\n"
                             "    if false then 2 else\n"
                             "    if true then 3 else 4)");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  ASSERT_EQ(p_program(&lexer, &program), PARSER_OK);

  FunctionSymbol *function = function_table_lookup(&program.functions, "foo");
  ASSERT_NE(function, nullptr);
  ASSERT_NE(function->ast_root, nullptr);
  ASSERT_EQ(function->ast_root->kind, AST_BLOCK);
  ASSERT_EQ(function->ast_root->data.block.statement_count, 1U);
  ASTNode *assignment = function->ast_root->data.block.statements[0];
  ASSERT_EQ(assignment->kind, AST_ASSIGN);
  ASTNode *outer = assignment->data.assignment.value;
  ASSERT_EQ(outer->kind, AST_IF);
  ASTNode *nested = outer->data.if_statement.else_branch;
  ASSERT_NE(nested, nullptr);
  ASSERT_EQ(nested->kind, AST_IF);
  ASTNode *final_if = nested->data.if_statement.else_branch;
  ASSERT_NE(final_if, nullptr);
  ASSERT_EQ(final_if->kind, AST_IF);
  ASSERT_NE(final_if->data.if_statement.else_branch, nullptr);
  EXPECT_EQ(final_if->data.if_statement.else_branch->kind, AST_CONSTANT_INT);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(ParserDirectLexerTest, OnlyLastIfStatementInBlockIsMarkedAsExpression) {
  FILE *stream = stream_from("def foo() =\n  if true then return\n"
                             "  if false then return\n  else return");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  ASSERT_EQ(p_program(&lexer, &program), PARSER_OK);

  FunctionSymbol *function = function_table_lookup(&program.functions, "foo");
  ASSERT_NE(function, nullptr);
  ASSERT_NE(function->ast_root, nullptr);
  ASSERT_EQ(function->ast_root->kind, AST_BLOCK);
  ASSERT_EQ(function->ast_root->data.block.statement_count, 2U);
  ASTNode *first = function->ast_root->data.block.statements[0];
  ASTNode *last = function->ast_root->data.block.statements[1];
  ASSERT_EQ(first->kind, AST_IF);
  ASSERT_EQ(last->kind, AST_IF);
  EXPECT_FALSE(first->data.if_statement.used_as_an_expression);
  EXPECT_TRUE(last->data.if_statement.used_as_an_expression);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(ParserDirectLexerTest, IfOperandWithoutElseIsAllowed) {
  FILE *stream = stream_from("def foo() =\n  val value = if true then 1");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_OK);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(ParserDirectLexerTest, LastIfStatementWithoutElseIsAllowed) {
  FILE *stream = stream_from("def foo() =\n  if true then return");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_OK);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}
