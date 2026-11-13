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
