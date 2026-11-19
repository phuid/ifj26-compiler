#include <string>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "../src/parser.h"
#include "../src/semantic.h"
}

namespace {

/** Creates a NUL-terminated source buffer containing text. */
std::vector<char> source_from(const char *text) {
  std::vector<char> source;
  for (; *text != '\0'; ++text) {
    source.push_back(*text);
  }
  source.push_back('\0');
  return source;
}

/** Valid main function added to parsed test programs. */
const char *const kMainFunction = "@main def added_main(): Unit =\n"
                                  "  return\n";

/**
 * @brief Like @ref source_from, but prepends a valid main function.
 * @details Keeps the missing-main error (code 3) out of tests aimed at other
 * constructs. The main function goes first so that @p text keeps its exact
 * ending (e.g. no trailing newline).
 */
std::vector<char> source_with_main(const char *text) {
  return source_from((std::string(kMainFunction) + text).c_str());
}

/**
 * @brief Like @ref source_from, but appends a valid main function.
 * @details Used by error tests: every error in @p text is detected before the
 * appended main, and an `@main` in @p text stays the first one.
 */
std::vector<char> source_with_main_after(const char *text) {
  return source_from((std::string(text) + "\n" + kMainFunction).c_str());
}

/// Parses @p text preceded by a valid main function and returns only the
/// parser status. Prepending keeps the exact ending of @p text.
ParserStatus parse_status(const char *text) {
  std::vector<char> source = source_with_main(text);
  Lexer lexer;
  if (lexer_init(&lexer, source.data()) != LEXER_OK) {
    return PARSER_INTERNAL_ERROR;
  }
  Program program;
  ParserStatus status = program_init(&program);
  if (status == PARSER_OK) {
    status = p_program(&lexer, &program);
  }
  program_destroy(&program);
  lexer_destroy(&lexer);
  return status;
}

/// Parses @p text preceded by a valid main function and, when parsing
/// succeeds, runs the semantic pass. Returns the compiler exit code.
int compile_status(const char *text) {
  std::vector<char> source = source_with_main(text);
  Lexer lexer;
  if (lexer_init(&lexer, source.data()) != LEXER_OK) {
    return PARSER_INTERNAL_ERROR;
  }
  Program program;
  ParserStatus status = program_init(&program);
  if (status == PARSER_OK) {
    status = p_program(&lexer, &program);
  }
  int code = parser_status_exit_code(status);
  if (status == PARSER_OK) {
    code = semantic_check(&program);
  }
  program_destroy(&program);
  lexer_destroy(&lexer);
  return code;
}

} // namespace

TEST(ParserDirectLexerTest, EmptyInputMissesMainFunction) {
  std::vector<char> source = source_from("");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_UNDEFINED_ERROR);
  EXPECT_EQ(program.functions.function_count, 0U);

  program_destroy(&program);
  lexer_destroy(&lexer);
}

TEST(ParserDirectLexerTest, IfOperandIsMarkedAsExpression) {
  std::vector<char> source =
      source_with_main("def foo() =\n  val value = if true then 1 else 2");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
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
}

TEST(ParserDirectLexerTest, ElseIfOperandCreatesNestedConditional) {
  std::vector<char> source = source_with_main(
      "def foo() =\n  val value = if true then 1 else if false then 2 else 3");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
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
}

TEST(ParserDirectLexerTest, IfExpressionMarksNestedBranchesAsExpressions) {
  std::vector<char> source = source_with_main(
      "def foo() =\n  val value = if true then if false then 1 else 2 else 3");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
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
}

TEST(ParserDirectLexerTest, ElseIfOperandWithoutFinalElseIsAllowed) {
  std::vector<char> source = source_with_main(
      "def foo() =\n  val value = if true then 1 else if false then 2");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_OK);

  program_destroy(&program);
  lexer_destroy(&lexer);
}

TEST(ParserDirectLexerTest, ParenthesizedElseIfCanStartOnNextLine) {
  std::vector<char> source =
      source_with_main("def foo() =\n"
                       "  val value = (if true then 1 else\n"
                       "    if false then 2 else\n"
                       "    if true then 3 else 4)");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
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
}

TEST(ParserDirectLexerTest, TrailingIfStatementInBlockIsNotAnExpression) {
  std::vector<char> source =
      source_with_main("def foo() =\n  if true then return\n"
                       "  if false then return\n  else return");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
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
  EXPECT_FALSE(last->data.if_statement.used_as_an_expression);

  program_destroy(&program);
  lexer_destroy(&lexer);
}

TEST(ParserDirectLexerTest, IfOperandWithoutElseIsAllowed) {
  std::vector<char> source =
      source_with_main("def foo() =\n  val value = if true then 1");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_OK);

  program_destroy(&program);
  lexer_destroy(&lexer);
}

TEST(ParserDirectLexerTest, LastIfStatementWithoutElseIsAllowed) {
  std::vector<char> source =
      source_with_main("def foo() =\n  if true then return");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_OK);

  program_destroy(&program);
  lexer_destroy(&lexer);
}

TEST(ParserDirectLexerTest, ParsesMultidimensionalListTypeAnnotations) {
  std::vector<char> source = source_with_main(
      "def consume(matrix: List[List[List[Int]]]) =\n  return");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
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
}

TEST(ParserDirectLexerTest, RejectsMalformedNestedListTypeAnnotation) {
  std::vector<char> source =
      source_with_main("def consume(matrix: List[List[Int]) =\n  return");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_SYNTAX_ERROR);

  program_destroy(&program);
  lexer_destroy(&lexer);
}

TEST(ParserDirectLexerTest, ChainedIndexingWrapsThePreviousResult) {
  std::vector<char> source = source_with_main(
      "def index(matrix: List[List[Int]]) =\n  val value = matrix(0)(1)");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
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
}

TEST(ParserDirectLexerTest, IndexesListConstructorResult) {
  std::vector<char> source =
      source_with_main("def make() =\n  val value = List(1, 2)(0)");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
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
}

TEST(ParserDirectLexerTest, IndexesFunctionCallResult) {
  std::vector<char> source = source_with_main("def getItems() =\n"
                                              "  return List(1, 2)\n"
                                              "def use() =\n"
                                              "  val value = getItems()(0)");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
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
}

TEST(ParserDirectLexerTest, ParsesMultilinePostfixIndexes) {
  std::vector<char> source =
      source_with_main("def index(matrix: List[List[Int]]) =\n"
                       "  val value = matrix(\n"
                       "    0\n"
                       "    )(\n"
                       "    1\n"
                       "    )");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_OK);

  program_destroy(&program);
  lexer_destroy(&lexer);
}

TEST(ParserDirectLexerTest, PostfixIndexCanFollowNewlineInsideParentheses) {
  std::vector<char> source = source_with_main("def make() =\n"
                                              "  val value = (\n"
                                              "    List(1, 2)\n"
                                              "    (0)\n"
                                              "    )");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
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
}

namespace {

/**
 * @brief Parses @p text followed by a valid main function.
 * @return The parser status.
 */
ParserStatus parse_text(const char *text) {
  std::vector<char> source = source_with_main_after(text);
  Lexer lexer;
  if (lexer_init(&lexer, source.data()) != LEXER_OK) {
    return PARSER_INTERNAL_ERROR;
  }
  Program program;
  if (program_init(&program) != PARSER_OK) {
    lexer_destroy(&lexer);
    return PARSER_INTERNAL_ERROR;
  }
  ParserStatus status = p_program(&lexer, &program);
  program_destroy(&program);
  lexer_destroy(&lexer);
  return status;
}

} // namespace

TEST(ParserErrorTest, StatusValuesAreExitCodes) {
  EXPECT_EQ(parser_status_exit_code(PARSER_OK), 0);
  EXPECT_EQ(parser_status_exit_code(PARSER_LEXER_ERROR), 1);
  EXPECT_EQ(parser_status_exit_code(PARSER_SYNTAX_ERROR), 2);
  EXPECT_EQ(parser_status_exit_code(PARSER_UNDEFINED_ERROR), 3);
  EXPECT_EQ(parser_status_exit_code(PARSER_REDEFINITION_ERROR), 4);
  EXPECT_EQ(parser_status_exit_code(PARSER_BAD_CALL_ERROR), 5);
  EXPECT_EQ(parser_status_exit_code(PARSER_TYPE_ERROR), 6);
  EXPECT_EQ(parser_status_exit_code(PARSER_INTERNAL_ERROR), 99);
  EXPECT_EQ(parser_status_exit_code(PARSER_INVALID_ARGUMENT), 99);
}

TEST(ParserErrorTest, MissingMainIsUndefined) {
  std::vector<char> source = source_from("def f(): Unit =\n  return\n");
  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_UNDEFINED_ERROR);
  program_destroy(&program);
  lexer_destroy(&lexer);
}

TEST(ParserErrorTest, DuplicateParameterIsRedefinition) {
  EXPECT_EQ(parse_text("def f(a: Int, a: Int): Unit =\n  return\n"),
            PARSER_REDEFINITION_ERROR);
}

TEST(ParserErrorTest, DuplicateFunctionIsRedefinition) {
  EXPECT_EQ(parse_text("def f(): Unit =\n  return\n"
                       "def f(): Unit =\n  return\n"),
            PARSER_REDEFINITION_ERROR);
}

TEST(ParserErrorTest, SecondMainIsRedefinition) {
  EXPECT_EQ(parse_text("@main def m(): Unit =\n  return\n"
                       "@main def n(): Unit =\n  return\n"),
            PARSER_REDEFINITION_ERROR);
}

TEST(ParserErrorTest, MainWithParameterIsBadCall) {
  EXPECT_EQ(parse_text("@main def m(a: Int): Unit =\n  return\n"),
            PARSER_BAD_CALL_ERROR);
}

TEST(ParserErrorTest, SemicolonSeparatesStatementsOnOneLine) {
  EXPECT_EQ(parse_text("def f(): Unit =\n  return; return; return\n"),
            PARSER_OK);
}

TEST(ParserErrorTest, TrailingSemicolonIsNotAllowed) {
  EXPECT_EQ(parse_text("def f(): Unit =\n  return;\n  return\n"),
            PARSER_SYNTAX_ERROR);
}

TEST(ParserErrorTest, EmptyStatementsAreNotAllowed) {
  EXPECT_EQ(parse_text("def f(): Unit =\n  ; return\n"), PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_text("def f(): Unit =\n  return;; return\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_text("def f(): Unit =\n  ;;return;;\n  return\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_text("def f(): Unit =\n  return;;\n"), PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_text("def f(): Unit =\n  ;\n"), PARSER_SYNTAX_ERROR);
}

TEST(ParserErrorTest, StatementsOnOneLineNeedSeparator) {
  EXPECT_EQ(parse_text("def f(): Unit =\n  return return\n"),
            PARSER_SYNTAX_ERROR);
}

TEST(ParserErrorTest, BlankLinesBetweenTopLevelDefinitions) {
  EXPECT_EQ(parse_text("\n\ndef f(): Unit =\n  return\n\n\n"
                       "def g(): Unit =\n  return\n\n"),
            PARSER_OK);
}

TEST(ParserErrorTest, LineInsideParenthesesMustBeIndentedMore) {
  EXPECT_EQ(parse_text("def f(\na: Int): Unit =\n  return\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_text("def f(a: Int,\nb: Int): Unit =\n  return\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_text("def f(a: Int,\n  b: Int): Unit =\n  return\n"),
            PARSER_OK);
  EXPECT_EQ(parse_text("def f(a: Int,\n  ): Unit =\n  return\n"), PARSER_OK);
}

TEST(ParserDirectLexerTest, IfOperandFollowedByNewlineEndsTheBlock) {
  EXPECT_EQ(parse_status("def foo() =\n  val value = if true then 1 else 2\n"),
            PARSER_OK);
}

TEST(ParserDirectLexerTest, IfOperandFollowedByAnotherStatement) {
  EXPECT_EQ(parse_status("def foo() =\n"
                         "  val value = if true then 1 else 2\n"
                         "  val other = value\n"),
            PARSER_OK);
  EXPECT_EQ(parse_status("def foo() =\n"
                         "  val value = 1 + if true then 1 else 2\n"
                         "  val other = value\n"),
            PARSER_OK);
}

TEST(ParserDirectLexerTest, IfOperandWithoutElseFollowedByAnotherStatement) {
  EXPECT_EQ(parse_status("def foo() =\n"
                         "  val value = if true then 1\n"
                         "  val other = value\n"),
            PARSER_OK);
}

TEST(ParserDirectLexerTest, IfOperandWithElseOnNextLine) {
  EXPECT_EQ(parse_status("def foo() =\n"
                         "  val value = if true then 1\n"
                         "  else 2\n"
                         "  val other = value\n"),
            PARSER_OK);
}

TEST(ParserDirectLexerTest, ReturnedIfOperandFollowedByNewline) {
  EXPECT_EQ(parse_status("def foo(): Int =\n  return if true then 1 else 2\n"),
            PARSER_OK);
}

TEST(ParserDirectLexerTest, OneLineIfBodyFollowedByDefinition) {
  /// The trailing if has already consumed the NEWLINE ending the body while
  /// looking for `else`, so the next definition must still be accepted.
  EXPECT_EQ(parse_status("def f(): Unit = if true then ()\n"
                         "def g(): Unit =\n  return\n"),
            PARSER_OK);
  EXPECT_EQ(parse_status("def f(): Int = if true then 1 else 2\n"
                         "def g(): Unit =\n  return\n"),
            PARSER_OK);
  EXPECT_EQ(parse_status("def f(): Int = 1 + if true then 1 else 2\n"
                         "def g(): Unit =\n  return\n"),
            PARSER_OK);
  EXPECT_EQ(parse_status("def f(): Unit = if true then ()\n\n"
                         "def g(): Unit =\n  return\n"),
            PARSER_OK);
}

TEST(ParserDirectLexerTest, IfBodyWithBlockBranchFollowedByDefinition) {
  /// The DEDENT closing the final block branch ends the body's line as well.
  EXPECT_EQ(parse_status("def f(): Int = if true then 1 else\n"
                         "  2\n"
                         "def g(): Unit =\n  return\n"),
            PARSER_OK);
  EXPECT_EQ(parse_status("def f(): Unit = if true then\n"
                         "  ()\n"
                         "def g(): Unit =\n  return\n"),
            PARSER_OK);
  EXPECT_EQ(parse_status("def f(): Int = if true then 1 else if false then\n"
                         "  2\n"
                         "def g(): Unit =\n  return\n"),
            PARSER_OK);
}

TEST(ParserDirectLexerTest, RejectsDefinitionAfterIfBodyOnTheSameLine) {
  EXPECT_EQ(parse_status("def f(): Unit = if true then () def g(): Unit =\n"
                         "  return\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def f(): Int = 1 def g(): Unit =\n  return\n"),
            PARSER_SYNTAX_ERROR);
}

TEST(ParserDirectLexerTest, ParenthesizedIfOperandFollowedByNewline) {
  EXPECT_EQ(parse_status("def foo() =\n"
                         "  val value = (if true then 1 else 2)\n"
                         "  val other = value\n"),
            PARSER_OK);
}

TEST(ParserDirectLexerTest, RejectsStatementAfterIfOperandOnTheSameLine) {
  EXPECT_EQ(parse_status("def foo() =\n"
                         "  val value = if true then 1 else 2 val other = 3\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(
      parse_status("def foo() =\n"
                   "  val value = (if true then 1 else 2) val other = 3\n"),
      PARSER_SYNTAX_ERROR);
}

TEST(ParserDirectLexerTest, SemicolonSeparatesStatementsOnOneLine) {
  EXPECT_EQ(parse_status("def foo() =\n  val a = 1; val b = 2\n"), PARSER_OK);
  EXPECT_EQ(parse_status("def foo() =\n  val a = 1; val b = 2; val c = 3"),
            PARSER_OK);
}

TEST(ParserDirectLexerTest, RejectsSemicolonAfterLastStatementOfLine) {
  /// A semicolon only separates statements on one line, so nothing may end
  /// with it.
  EXPECT_EQ(parse_status("def foo() =\n  val a = 1;\n  val b = 2\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def foo() =\n  val a = 1; val b = 2;\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def foo() =\n  val a = 1;"), PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def foo() =\n  return;\n"), PARSER_SYNTAX_ERROR);
}

TEST(ParserDirectLexerTest, RejectsEmptyStatements) {
  EXPECT_EQ(parse_status("def foo() =\n  val a = 1;; val b = 2\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def foo() =\n  ; val a = 1\n"), PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def foo() =\n  val a = 1\n  ;\n"),
            PARSER_SYNTAX_ERROR);
}

TEST(ParserDirectLexerTest, BareReturnFollowedBySemicolon) {
  EXPECT_EQ(parse_status("def foo() =\n  return; val a = 1\n"), PARSER_OK);
}

TEST(ParserDirectLexerTest, SemicolonAfterIfOperand) {
  EXPECT_EQ(
      parse_status("def foo() =\n"
                   "  val value = if true then 1 else 2; val other = 2\n"),
      PARSER_OK);
  EXPECT_EQ(
      parse_status("def foo() =\n"
                   "  val value = (if true then 1 else 2); val other = 2\n"),
      PARSER_OK);
}

TEST(ParserDirectLexerTest, SemicolonEndsOneLineIfStatement) {
  std::vector<char> source =
      source_with_main("def foo() =\n  if true then val a = 1; val b = 2\n");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  ASSERT_EQ(p_program(&lexer, &program), PARSER_OK);

  FunctionSymbol *function = function_table_lookup(&program.functions, "foo");
  ASSERT_NE(function, nullptr);
  ASSERT_NE(function->ast_root, nullptr);
  /// `val b = 2` follows the if; it is not part of its then branch.
  ASSERT_EQ(function->ast_root->data.block.statement_count, 2U);
  ASTNode *conditional = function->ast_root->data.block.statements[0];
  ASSERT_EQ(conditional->kind, AST_IF);
  ASSERT_NE(conditional->data.if_statement.then_branch, nullptr);
  EXPECT_EQ(conditional->data.if_statement.then_branch->kind, AST_ASSIGN);
  EXPECT_EQ(conditional->data.if_statement.else_branch, nullptr);
  EXPECT_EQ(function->ast_root->data.block.statements[1]->kind, AST_ASSIGN);

  program_destroy(&program);
  lexer_destroy(&lexer);
}

TEST(ParserDirectLexerTest, RejectsSemicolonStartingLineAfterBlockBranch) {
  EXPECT_EQ(parse_status("def foo() =\n"
                         "  if true then\n"
                         "    val a = 1\n"
                         "  ; val b = 2\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def foo() =\n"
                         "  val a = if true then 1 else\n"
                         "    2\n"
                         "  ; val b = 2\n"),
            PARSER_SYNTAX_ERROR);
}

TEST(ParserDirectLexerTest, BlockBranchesOfIfAreKept) {
  std::vector<char> source = source_with_main("def foo() =\n"
                                              "  if true then\n"
                                              "    val a = 1\n"
                                              "  else\n"
                                              "    val b = 2\n"
                                              "  val c = 3\n");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  ASSERT_EQ(p_program(&lexer, &program), PARSER_OK);

  FunctionSymbol *function = function_table_lookup(&program.functions, "foo");
  ASSERT_NE(function, nullptr);
  ASSERT_NE(function->ast_root, nullptr);
  ASSERT_EQ(function->ast_root->data.block.statement_count, 2U);
  ASTNode *conditional = function->ast_root->data.block.statements[0];
  ASSERT_EQ(conditional->kind, AST_IF);
  ASTNode *then_branch = conditional->data.if_statement.then_branch;
  ASTNode *else_branch = conditional->data.if_statement.else_branch;
  ASSERT_NE(then_branch, nullptr);
  ASSERT_NE(else_branch, nullptr);
  EXPECT_EQ(then_branch->kind, AST_BLOCK);
  EXPECT_EQ(else_branch->kind, AST_BLOCK);
  EXPECT_EQ(function->ast_root->data.block.statements[1]->kind, AST_ASSIGN);

  program_destroy(&program);
  lexer_destroy(&lexer);
}

TEST(ParserDirectLexerTest, ParsesAssignmentToVariable) {
  std::vector<char> source = source_with_main("def foo() =\n"
                                              "  var a = 1\n"
                                              "  a = 5 + 6\n");

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, source.data()), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  ASSERT_EQ(p_program(&lexer, &program), PARSER_OK);

  FunctionSymbol *function = function_table_lookup(&program.functions, "foo");
  ASSERT_NE(function, nullptr);
  ASSERT_EQ(function->ast_root->data.block.statement_count, 2U);
  ASTNode *definition = function->ast_root->data.block.statements[0];
  ASTNode *assignment = function->ast_root->data.block.statements[1];
  ASSERT_EQ(assignment->kind, AST_ASSIGN);
  EXPECT_FALSE(assignment->data.assignment.is_definition);
  EXPECT_EQ(assignment->data.assignment.target,
            definition->data.assignment.target);
  ASSERT_NE(assignment->data.assignment.value, nullptr);
  EXPECT_EQ(assignment->data.assignment.value->kind, AST_ADD);
  EXPECT_EQ(assignment->line, 5U); // after the 2-line main prelude
  EXPECT_EQ(assignment->column, 3U);

  program_destroy(&program);
  lexer_destroy(&lexer);
}

TEST(ParserDirectLexerTest, AssignmentAcceptsAnyExpressionOnTheRight) {
  EXPECT_EQ(compile_status("def bar(x: Int): Int = x\n"
                           "def foo() =\n"
                           "  var a = 1\n"
                           "  a = bar(a) * 2\n"
                           "  a = if a < 3 then 1 else 2\n"
                           "  a = (a + 1)\n"
                           "  var b = true\n"
                           "  b = a == 2 && !b\n"
                           "  var l = List(1, 2)\n"
                           "  l = l ++ List(a)\n"
                           "  a = l(0); a = 3\n"),
            0);
}

TEST(ParserDirectLexerTest, AssignmentIfOperandFollowedByStatement) {
  EXPECT_EQ(compile_status("def foo() =\n"
                           "  var a = 1\n"
                           "  a = if a < 3 then 1 else 2\n"
                           "  a = 4\n"),
            0);
}

TEST(ParserDirectLexerTest, AssignmentEndsBlockWithUnit) {
  EXPECT_EQ(compile_status("def foo(): Unit =\n"
                           "  var a = 1\n"
                           "  a = 2\n"),
            0);
  EXPECT_EQ(compile_status("def foo(): Int =\n"
                           "  var a = 1\n"
                           "  a = 2\n"),
            6);
}

TEST(ParserDirectLexerTest, RejectsAssignmentToNonVariable) {
  EXPECT_EQ(parse_status("def foo() =\n  var a = 1\n  a + 1 = 2\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def foo() =\n  var a = 1\n  (a) = 2\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def foo() =\n  var a = 1\n  (a = 2)\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def foo() =\n  val l = List(1)\n  l(0) = 2\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def bar() = 1\ndef foo() =\n  bar() = 2\n"),
            PARSER_SYNTAX_ERROR);
}

TEST(ParserDirectLexerTest, RejectsAssignmentInsideExpressions) {
  EXPECT_EQ(parse_status("def foo() =\n  var a = 1\n  val b = a = 2\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def foo() =\n  var a = 1\n  a = a = 2\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def bar(x: Int) = x\n"
                         "def foo() =\n  var a = 1\n  bar(a = 2)\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def foo() =\n  var a = 1\n"
                         "  if a = 2 then\n    a\n  else\n    a\n"),
            PARSER_SYNTAX_ERROR);
  EXPECT_EQ(parse_status("def foo() =\n  var a = 1\n  a =\n"),
            PARSER_SYNTAX_ERROR);
}

TEST(ParserDirectLexerTest, AssignmentToUndefinedVariable) {
  EXPECT_EQ(compile_status("def foo() =\n  a = 1\n"), 3);
  EXPECT_EQ(compile_status("def foo() =\n"
                           "  if true then\n    var a = 1\n  else\n"
                           "    ()\n"
                           "  a = 2\n"),
            3);
}

TEST(ParserDirectLexerTest, AssignmentToImmutableIsError7) {
  EXPECT_EQ(compile_status("def foo() =\n  val a = 1\n  a = 2\n"), 7);
  EXPECT_EQ(compile_status("def foo(a: Int) =\n  a = 2\n"), 7);
}

TEST(ParserDirectLexerTest, AssignmentOfIncompatibleTypeIsError6) {
  EXPECT_EQ(compile_status("def foo() =\n  var a = 1\n  a = \"s\"\n"), 6);
  EXPECT_EQ(compile_status("def foo() =\n  var a: String = \"x\"\n"
                           "  a = 1 + 2\n"),
            6);
}
