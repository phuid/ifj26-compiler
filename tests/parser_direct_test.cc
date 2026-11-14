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

TEST(ParserDirectLexerTest, ParsesMultidimensionalListTypeAnnotations) {
  FILE *stream =
      stream_from("def consume(matrix: List[List[List[Int]]]) =\n  return");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  ASSERT_EQ(p_program(&lexer, &program), PARSER_OK);

  FunctionSymbol *function =
      function_table_lookup(&program.functions, "consume");
  ASSERT_NE(function, nullptr);
  Symbol *matrix = symbol_table_lookup_local(function->local_scope, "matrix");
  ASSERT_NE(matrix, nullptr);
  DataType expected = type_list(type_list(type_list(type_scalar(TYPE_INT))));
  EXPECT_TRUE(type_equal(matrix->type.type, expected));
  EXPECT_EQ(matrix->type.type.list_depth, 3U);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(ParserDirectLexerTest, RejectsMalformedNestedListTypeAnnotation) {
  FILE *stream = stream_from("def consume(matrix: List[List[Int]) =\n  return");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_SYNTAX_ERROR);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(ParserDirectLexerTest, ChainedIndexingWrapsThePreviousResult) {
  FILE *stream = stream_from(
      "def index(matrix: List[List[Int]]) =\n  val value = matrix(0)(1)");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  ASSERT_EQ(p_program(&lexer, &program), PARSER_OK);

  FunctionSymbol *function = function_table_lookup(&program.functions, "index");
  ASSERT_NE(function, nullptr);
  ASSERT_NE(function->ast_root, nullptr);
  ASSERT_EQ(function->ast_root->data.block.statement_count, 1U);
  ASTNode *outer =
      function->ast_root->data.block.statements[0]->data.assignment.value;
  ASSERT_NE(outer, nullptr);
  ASSERT_EQ(outer->kind, AST_LIST_INDEXING);
  ASSERT_NE(outer->data.binary_op.right, nullptr);
  EXPECT_EQ(outer->data.binary_op.right->kind, AST_CONSTANT_INT);
  EXPECT_EQ(outer->data.binary_op.right->data.int_value, 1);

  ASTNode *inner = outer->data.binary_op.left;
  ASSERT_NE(inner, nullptr);
  ASSERT_EQ(inner->kind, AST_LIST_INDEXING);
  ASSERT_NE(inner->data.binary_op.left, nullptr);
  EXPECT_EQ(inner->data.binary_op.left->kind, AST_VARIABLE_REF);
  ASSERT_NE(inner->data.binary_op.right, nullptr);
  EXPECT_EQ(inner->data.binary_op.right->kind, AST_CONSTANT_INT);
  EXPECT_EQ(inner->data.binary_op.right->data.int_value, 0);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(ParserDirectLexerTest, IndexesListConstructorResult) {
  FILE *stream = stream_from("def make() =\n  val value = List(1, 2)(0)");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  ASSERT_EQ(p_program(&lexer, &program), PARSER_OK);

  FunctionSymbol *function = function_table_lookup(&program.functions, "make");
  ASSERT_NE(function, nullptr);
  ASSERT_NE(function->ast_root, nullptr);
  ASSERT_EQ(function->ast_root->data.block.statement_count, 1U);
  ASTNode *indexing =
      function->ast_root->data.block.statements[0]->data.assignment.value;
  ASSERT_NE(indexing, nullptr);
  ASSERT_EQ(indexing->kind, AST_LIST_INDEXING);
  ASTNode *constructor = indexing->data.binary_op.left;
  ASSERT_NE(constructor, nullptr);
  EXPECT_EQ(constructor->kind, AST_LIST_CONSTRUCTOR);
  EXPECT_EQ(constructor->data.call.argument_count, 2U);
  ASSERT_NE(indexing->data.binary_op.right, nullptr);
  EXPECT_EQ(indexing->data.binary_op.right->kind, AST_CONSTANT_INT);
  EXPECT_EQ(indexing->data.binary_op.right->data.int_value, 0);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(ParserDirectLexerTest, IndexesFunctionCallResult) {
  FILE *stream = stream_from("def getItems() =\n"
                             "  return List(1, 2)\n"
                             "def use() =\n"
                             "  val value = getItems()(0)");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  ASSERT_EQ(p_program(&lexer, &program), PARSER_OK);

  FunctionSymbol *function = function_table_lookup(&program.functions, "use");
  ASSERT_NE(function, nullptr);
  ASSERT_NE(function->ast_root, nullptr);
  ASSERT_EQ(function->ast_root->data.block.statement_count, 1U);
  ASTNode *indexing =
      function->ast_root->data.block.statements[0]->data.assignment.value;
  ASSERT_NE(indexing, nullptr);
  ASSERT_EQ(indexing->kind, AST_LIST_INDEXING);
  ASSERT_NE(indexing->data.binary_op.left, nullptr);
  EXPECT_EQ(indexing->data.binary_op.left->kind, AST_FUNCTION_CALL);
  ASSERT_NE(indexing->data.binary_op.right, nullptr);
  EXPECT_EQ(indexing->data.binary_op.right->kind, AST_CONSTANT_INT);
  EXPECT_EQ(indexing->data.binary_op.right->data.int_value, 0);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(ParserDirectLexerTest, ParsesMultilinePostfixIndexes) {
  FILE *stream = stream_from("def index(matrix: List[List[Int]]) =\n"
                             "  val value = matrix(\n"
                             "    0\n"
                             "    )(\n"
                             "    1\n"
                             "    )");
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

TEST(ParserDirectLexerTest, PostfixIndexCanFollowNewlineInsideParentheses) {
  FILE *stream = stream_from("def make() =\n"
                             "  val value = (\n"
                             "    List(1, 2)\n"
                             "    (0)\n"
                             "    )");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  ASSERT_EQ(p_program(&lexer, &program), PARSER_OK);

  FunctionSymbol *function = function_table_lookup(&program.functions, "make");
  ASSERT_NE(function, nullptr);
  ASSERT_NE(function->ast_root, nullptr);
  ASSERT_EQ(function->ast_root->data.block.statement_count, 1U);
  ASTNode *indexing =
      function->ast_root->data.block.statements[0]->data.assignment.value;
  ASSERT_NE(indexing, nullptr);
  ASSERT_EQ(indexing->kind, AST_LIST_INDEXING);
  ASSERT_NE(indexing->data.binary_op.left, nullptr);
  EXPECT_EQ(indexing->data.binary_op.left->kind, AST_LIST_CONSTRUCTOR);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}
