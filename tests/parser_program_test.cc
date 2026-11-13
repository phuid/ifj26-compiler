#include <cstdio>

#include "gtest/gtest.h"

extern "C" {
#include "../src/parser.h"
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

/** Returns a temporary stream that reads @p text from its beginning. */
FILE *stream_from(const char *text) {
  FILE *stream = tmpfile();
  if (stream == nullptr) {
    return nullptr;
  }
  fputs(text, stream);
  rewind(stream);
  return stream;
}

/**
 * Parses @p source into @p program, which the caller has initialised.
 * Returns PARSER_INTERNAL_ERROR when the stream or lexer cannot be set up.
 */
ParserStatus parse_text(const char *source, Program *program) {
  FILE *stream = stream_from(source);
  if (stream == nullptr) {
    return PARSER_INTERNAL_ERROR;
  }
  Lexer lexer;
  if (lexer_init(&lexer, stream) != LEXER_OK) {
    fclose(stream);
    return PARSER_INTERNAL_ERROR;
  }
  ParserStatus status = p_program(&lexer, program);
  lexer_destroy(&lexer);
  fclose(stream);
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
 *   PARSER_SEMANTIC_ERROR. P6: a @main function with a typed parameter gives
 *   PARSER_SEMANTIC_ERROR. Check function_count in every test and use own
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
  EXPECT_EQ(main_function->return_type.type, TYPE_UNIT);
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
  EXPECT_EQ(parse_text(source, &program), PARSER_SEMANTIC_ERROR);
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
  EXPECT_EQ(parse_text(source, &program), PARSER_SEMANTIC_ERROR);
  EXPECT_EQ(program.functions.function_count, 1U);

  program_destroy(&program);
}
