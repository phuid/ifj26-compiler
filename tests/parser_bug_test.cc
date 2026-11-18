#include <cstdio>

#include "gtest/gtest.h"

extern "C" {
#include "../src/parser.h"
}

/*
 * AI-generated test block — parser spec deviations found by bug hunting.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write GoogleTest cases for parser behaviour that contradicts
 *   specifikace-jazyka.pdf: the mandatory `import ifj26.*` prolog is not
 *   parsed and its absence is accepted, a block without any statement is
 *   accepted, and errors that the spec maps to different exit codes
 *   (redefinition 4, bad main signature 5) collapse into one ParserStatus.
 *   These tests are expected to fail until the parser is fixed.
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

/** Parses @p source into a fresh Program and returns the parser result. */
ParserStatus parse_source(const char *source) {
  Program program;
  if (program_init(&program) != PARSER_OK) {
    return PARSER_INTERNAL_ERROR;
  }
  FILE *stream = stream_from(source);
  if (stream == nullptr) {
    program_destroy(&program);
    return PARSER_INTERNAL_ERROR;
  }
  Lexer lexer;
  ParserStatus status = PARSER_INTERNAL_ERROR;
  if (lexer_init(&lexer, stream) == LEXER_OK) {
    status = p_program(&lexer, &program);
    lexer_destroy(&lexer);
  }
  fclose(stream);
  program_destroy(&program);
  return status;
}

} // namespace

TEST(ParserBugTest, ProgramWithPrologIsAccepted) {
  // Spec 2: every program starts with the prolog `import ifj26.*`.
  EXPECT_EQ(parse_source("import ifj26.*\n"
                         "@main\n"
                         "def main(): Unit =\n"
                         "  return\n"),
            PARSER_OK);
}

TEST(ParserBugTest, MissingPrologIsSyntaxError) {
  // Spec 2: "Absence prologu povede na syntaktickou chybu 2."
  EXPECT_EQ(parse_source("@main\n"
                         "def main(): Unit =\n"
                         "  return\n"),
            PARSER_SYNTAX_ERROR);
}

TEST(ParserBugTest, EmptySourceIsSyntaxError) {
  // An empty file has no prolog either.
  EXPECT_EQ(parse_source(""), PARSER_SYNTAX_ERROR);
}

TEST(ParserBugTest, BlockWithOnlySeparatorIsSyntaxError) {
  // Spec 1.2: "Bloky nemohou být prázdné."
  EXPECT_EQ(parse_source("def f() =\n  ;\n"), PARSER_SYNTAX_ERROR);
}

TEST(ParserBugTest, BlockEndingAtEofWithoutStatementIsSyntaxError) {
  EXPECT_EQ(parse_source("def f() =\n  "), PARSER_SYNTAX_ERROR);
}

TEST(ParserBugTest, RedefinitionAndBadMainSignatureHaveDistinctStatus) {
  // Spec 6: redefinition is error 4, a wrong parameter count of the entry
  // function is error 5; the parser result must keep them apart.
  ParserStatus duplicate_parameter =
      parse_source("def f(x: Int, x: Int) =\n  return\n");
  ParserStatus main_with_parameter =
      parse_source("@main\ndef m(x: Int) =\n  return\n");

  EXPECT_NE(duplicate_parameter, PARSER_OK);
  EXPECT_NE(main_with_parameter, PARSER_OK);
  EXPECT_NE(duplicate_parameter, main_with_parameter);
}
