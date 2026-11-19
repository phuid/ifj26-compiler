#include <cstdio>
#include <cstring>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "../src/parser.h"
#include "../src/semantic.h"
}

/*
 * AI-generated test block — whole-program parser helper.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Implement only the helper below and the first test. parse_text runs the
 *   lexer and p_program over an in-memory IFJ26 source and releases the
 *   lexer and the stream; the test owns the Program. The first test feeds a
 *   bare top-level identifier `x`, which is not a function definition.
 */
namespace {

/** Returns a null-terminated copy of @p text for the lexer to borrow. */
std::vector<char> buffer_from(const char *text) {
  return std::vector<char>(text, text + strlen(text) + 1);
}

/**
 * Parses @p source into @p program, which the caller has initialised.
 * Returns PARSER_INTERNAL_ERROR when the stream or lexer cannot be set up.
 */
ParserStatus parse_text(const char *source, Program *program) {
  std::vector<char> buffer = buffer_from(source);
  Lexer lexer;
  if (lexer_init(&lexer, buffer.data()) != LEXER_OK) {
    return PARSER_INTERNAL_ERROR;
  }
  ParserStatus status = p_program(&lexer, program);
  lexer_destroy(&lexer);
  return status;
}

} // namespace

TEST(ParserProgramTest, TopLevelIdentifierIsSyntaxError) {
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);

  EXPECT_EQ(parse_text("x\n", &program), PARSER_SYNTAX_ERROR);
  EXPECT_EQ(program.functions.function_count, 0U);

  program_destroy(&program);
}

TEST(ParserProgramTest, UnterminatedStringIsLexerError) {
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);

  EXPECT_EQ(parse_text("\"abc", &program), PARSER_LEXER_ERROR);
  EXPECT_EQ(program.functions.function_count, 0U);

  program_destroy(&program);
}

TEST(ParserProgramTest, UninitializedProgramIsRejected) {
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  program.initialized = false;

  EXPECT_EQ(parse_text("def f(): Unit =\n  return\n", &program),
            PARSER_INVALID_ARGUMENT);
  EXPECT_EQ(program.functions.function_count, 0U);

  program.initialized = true;
  program_destroy(&program);
}

/*
 * AI-generated test block — @main function rules.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Add whole-program parser tests for the @main function in the same style
 *   as the tests above. P4: a @main `def main(): Unit = return`, preceded by
 *   a helper function and with blank lines between @main and def, parses with
 *   PARSER_OK; function_table_lookup finds main with is_main, is_defined and
 *   has_explicit_return_type set, return type TYPE_UNIT and no arguments.
 *   P5: two functions annotated with @main (main and other) give
 *   PARSER_REDEFINITION_ERROR. P6: a @main function with a typed parameter
 *   gives PARSER_BAD_CALL_ERROR. Check function_count in every test and use own
 *   names, types and counts instead of the smallest possible input.
 */
TEST(ParserProgramTest, MainWithUnitReturnTypeIsAccepted) {
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);

  const char *source = "def print_total(total: Double): Unit =\n"
                       "  return\n"
                       "\n"
                       "@main\n"
                       "\n"
                       "\n"
                       "def main(): Unit =\n"
                       "  return\n";
  EXPECT_EQ(parse_text(source, &program), PARSER_OK);
  EXPECT_EQ(program.functions.function_count, 2U);

  FunctionSymbol *main_function =
      function_table_lookup(&program.functions, "main");
  ASSERT_NE(main_function, nullptr);
  EXPECT_TRUE(main_function->is_main);
  EXPECT_TRUE(main_function->is_defined);
  EXPECT_TRUE(main_function->has_explicit_return_type);
  EXPECT_TRUE(
      type_equal(main_function->return_type.type, type_scalar(TYPE_UNIT)));
  EXPECT_EQ(main_function->argument_count, 0U);

  FunctionSymbol *helper =
      function_table_lookup(&program.functions, "print_total");
  ASSERT_NE(helper, nullptr);
  EXPECT_FALSE(helper->is_main);

  program_destroy(&program);
}

TEST(ParserProgramTest, SecondMainIsSemanticError) {
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);

  const char *source = "@main\n"
                       "def main(): Unit =\n"
                       "  return\n"
                       "\n"
                       "@main\n"
                       "def other(): Unit =\n"
                       "  return\n";
  EXPECT_EQ(parse_text(source, &program), PARSER_REDEFINITION_ERROR);
  EXPECT_EQ(program.functions.function_count, 1U);
  EXPECT_EQ(function_table_lookup(&program.functions, "other"), nullptr);

  program_destroy(&program);
}

TEST(ParserProgramTest, MainWithParameterIsSemanticError) {
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);

  const char *source = "@main\n"
                       "def main(count: Int, label: String): Unit =\n"
                       "  return\n";
  EXPECT_EQ(parse_text(source, &program), PARSER_BAD_CALL_ERROR);
  EXPECT_EQ(program.functions.function_count, 1U);

  program_destroy(&program);
}

/*
 * AI-generated test block — function and parameter redefinition.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Add whole-program parser tests for function redefinition in the same
 *   style as the tests above. P7: two or three different functions
 *   `(): Unit = return`, one of them with parameters, parse with PARSER_OK,
 *   function_count matches and function_table_lookup finds each of them; it
 *   is the control case for P8. P8: `def f` defined twice (a different
 *   signature is fine) gives PARSER_REDEFINITION_ERROR. P9: a duplicate
 *   parameter name such as `def f(x: Int, x: Int)` gives
 *   PARSER_REDEFINITION_ERROR; a
 *   different duplicate type or a duplicate in the third position is fine.
 *   Use own names, types and counts instead of the smallest possible input.
 */
TEST(ParserProgramTest, TwoDifferentFunctionsAreBothStored) {
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);

  const char *source = "def open_log(): Unit =\n"
                       "  return\n"
                       "\n"
                       "def write_entry(level: Int, text: String, "
                       "ratio: Double): Unit =\n"
                       "  return\n"
                       "\n"
                       "def close_log(): Unit =\n"
                       "  return\n"
                       "\n"
                       "@main\n"
                       "def main(): Unit =\n"
                       "  return\n";
  EXPECT_EQ(parse_text(source, &program), PARSER_OK);
  EXPECT_EQ(program.functions.function_count, 4U);

  EXPECT_NE(function_table_lookup(&program.functions, "open_log"), nullptr);
  EXPECT_NE(function_table_lookup(&program.functions, "close_log"), nullptr);
  FunctionSymbol *write_entry =
      function_table_lookup(&program.functions, "write_entry");
  ASSERT_NE(write_entry, nullptr);
  EXPECT_EQ(write_entry->argument_count, 3U);

  program_destroy(&program);
}

TEST(ParserProgramTest, DuplicateFunctionIsSemanticError) {
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);

  const char *source = "def reset_counter(): Unit =\n"
                       "  return\n"
                       "\n"
                       "def reset_counter(start: Int): Unit =\n"
                       "  return\n";
  EXPECT_EQ(parse_text(source, &program), PARSER_REDEFINITION_ERROR);
  EXPECT_EQ(program.functions.function_count, 1U);

  program_destroy(&program);
}

TEST(ParserProgramTest, DuplicateParameterIsSemanticError) {
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);

  const char *source =
      "def blend(weight: Double, name: String, weight: Int): Unit =\n"
      "  return\n";
  EXPECT_EQ(parse_text(source, &program), PARSER_REDEFINITION_ERROR);
  EXPECT_EQ(program.functions.function_count, 1U);

  program_destroy(&program);
}

/*
 * AI-generated test block — expressions not yet supported by the parser.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Add two whole-program parser tests for expressions in the same style as
 *   the tests above; each program also defines @main. P10: `def f(): Unit =\n
 * val a = 1\n  return\n` parses with PARSER_OK and
 * symbol_table_lookup_local(f->local_scope, "a") returns a symbol of type
 * TYPE_INT with constant set. P11: `def f() =\n  1\n` parses with PARSER_OK and
 * f->ast_root is not null. Use own names and values instead of the smallest
 * possible input.
 */

TEST(ParserProgramTest, ValInitializerInfersIntConstant) {
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);

  const char *source = "def setup_limits(): Unit =\n"
                       "  val max_retries = 42\n"
                       "  return\n"
                       "\n"
                       "@main\n"
                       "def main(): Unit =\n"
                       "  return\n";
  EXPECT_EQ(parse_text(source, &program), PARSER_OK);
  EXPECT_EQ(program.functions.function_count, 2U);
  // The type of `val` is inferred by semantic analysis.
  EXPECT_EQ(semantic_check(&program), SEMANTIC_OK);

  FunctionSymbol *function =
      function_table_lookup(&program.functions, "setup_limits");
  ASSERT_NE(function, nullptr);
  ASSERT_NE(function->local_scope, nullptr);
  Symbol *symbol =
      symbol_table_lookup_local(function->local_scope, "max_retries");
  ASSERT_NE(symbol, nullptr);
  EXPECT_TRUE(type_equal(symbol->type.type, type_scalar(TYPE_INT)));
  EXPECT_TRUE(symbol->constant);

  program_destroy(&program);
}

TEST(ParserProgramTest, LiteralAsLastExpressionParses) {
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);

  const char *source = "def answer() =\n"
                       "  2026\n"
                       "\n"
                       "@main\n"
                       "def main(): Unit =\n"
                       "  return\n";
  EXPECT_EQ(parse_text(source, &program), PARSER_OK);
  EXPECT_EQ(program.functions.function_count, 2U);

  FunctionSymbol *function =
      function_table_lookup(&program.functions, "answer");
  ASSERT_NE(function, nullptr);
  EXPECT_NE(function->ast_root, nullptr);

  program_destroy(&program);
}
