#include <cstdio>

#include "gtest/gtest.h"

extern "C" {
#include "../src/parser.h"
}

/*
 * AI-generated test block — parser coverage helpers.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Run the lexer and p_program over an in-memory IFJ26 source; the helper
 *   owns the stream, the lexer and the Program of one parse and exposes the
 *   status and the parsed functions.
 */
namespace {

FILE *stream_from(const char *text) {
  FILE *stream = tmpfile();
  if (stream != nullptr) {
    fputs(text, stream);
    rewind(stream);
  }
  return stream;
}

struct Parsed {
  Program program;
  ParserStatus status = PARSER_INTERNAL_ERROR;
  explicit Parsed(const char *source) {
    if (program_init(&program) != PARSER_OK) {
      return;
    }
    FILE *stream = stream_from(source);
    if (stream == nullptr) {
      return;
    }
    Lexer lexer;
    if (lexer_init(&lexer, stream) == LEXER_OK) {
      status = p_program(&lexer, &program);
      lexer_destroy(&lexer);
    }
    fclose(stream);
  }
  ~Parsed() { program_destroy(&program); }
  Parsed(const Parsed &) = delete;
  Parsed &operator=(const Parsed &) = delete;
};

} // namespace

/*
 * AI-generated test block — lexer without an input stream.
 * Model: Claude (Claude Code).
 * Prompt:
 *   lexer_destroy keeps the struct alive but sets input = NULL; lexer_pls
 *   rejects such a lexer with LEXER_INVALID_ARGUMENT as a defensive
 *   convention (LexerGapTest.NullArgumentsAreRejectedOrIgnored), not a
 *   documented contract. p_program must translate it to
 *   PARSER_INVALID_ARGUMENT, as for a NULL lexer, and add no function.
 */
TEST(ParserCovApiTest, DestroyedLexerIsInvalidArgument) {
  FILE *stream = stream_from("def f() =\n  return\n");
  ASSERT_NE(stream, nullptr);
  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  // Not a use-after-free: the struct still lives, only input is NULL.
  lexer_destroy(&lexer);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_INVALID_ARGUMENT);
  EXPECT_EQ(program.functions.function_count, 0U);
  program_destroy(&program);
  fclose(stream);
}

/*
 * AI-generated test block — lexer errors right after a NEWLINE.
 * Model: Claude (Claude Code).
 * Prompt:
 *   A lexical error in the token that follows a NEWLINE inside a parameter
 *   list or after a function header `=`, or right after a `;` in a block must
 *   be reported as PARSER_LEXER_ERROR (exit code 1), never as OK or syntax.
 */
TEST(ParserCovStatusTest, LexerErrorAfterNewlineIsPropagated) {
  const char *const sources[] = {
      "def f(\n$",             // newline after `(`         (parser.c:942)
      "def f(a\n$",            // newline after a parameter (parser.c:976)
      "def f(a: Int\n$",       // newline after its type    (parser.c:995)
      "def f() =\n$",          // newline after `=`         (parser.c:1180)
      "def f() =\n  return;$", // token after `;`
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(Parsed(source).status, PARSER_LEXER_ERROR);
  }
}

/*
 * AI-generated test block — lines made only of separators.
 * Model: Claude (Claude Code).
 * Prompt:
 *   A logical line holds one statement, or several separated by `;` (spec
 *   1.2), so a line of separators without a statement is a syntax error.
 *   A `;` after the last statement of a line is not decided by the spec
 *   ("není další oddělovač nutný") and is deliberately not tested.
 */
TEST(ParserCovStatusTest, SeparatorOnlyLinesAreSyntaxErrors) {
  const char *const sources[] = {
      "def f() =\n  ;",
      "def f() =\n  return\n  ;",
      "def f() =\n  return\n  ;;",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(Parsed(source).status, PARSER_SYNTAX_ERROR);
  }
}

/*
 * AI-generated test block — non-def on an indented line after @main.
 * Model: Claude (Claude Code).
 * Prompt:
 *   `@main` must be followed by `def` (p_main_function grammar); anything
 *   else is a syntax error regardless of indentation, and the layout path
 *   taken with a NULL indent_delta after `@main` must not crash.
 */
TEST(ParserCovStatusTest, IndentedNonDefAfterMainIsSyntaxError) {
  const char *const sources[] = {"@main\n  5\n", "@main\n  return\n",
                                 "@main\n\n    x"};
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(Parsed(source).status, PARSER_SYNTAX_ERROR);
  }
}

/*
 * AI-generated test block — final newline after an indented @main def.
 * Model: Claude (Claude Code).
 * Prompt:
 *   A final newline must not change the verdict for an indented def after
 *   @main; both forms must give the same status, and that status must be
 *   PARSER_OK or PARSER_SYNTAX_ERROR. Disabled: the parser differs today.
 */
TEST(ParserCovKnownGapTest, FinalNewlineDoesNotChangeIndentedMainVerdict) {
  const ParserStatus with_newline =
      Parsed("@main\n  def main() =\n    return\n").status;
  const ParserStatus without_newline =
      Parsed("@main\n  def main() =\n    return").status;
  EXPECT_TRUE(with_newline == PARSER_OK || with_newline == PARSER_SYNTAX_ERROR);
  EXPECT_TRUE(without_newline == PARSER_OK ||
              without_newline == PARSER_SYNTAX_ERROR);
  EXPECT_EQ(with_newline, without_newline);
}

/*
 * AI-generated test block — function body given as an expression.
 * Model: Claude (Claude Code).
 * Prompt:
 *   `def f(a: Int): Int = a` is a valid function (specifikace.txt, doc of
 *   p_function), with or without a final newline. A valid @main goes first
 *   so that the source keeps its exact ending.
 */
TEST(ParserCovKnownGapTest, ExpressionBodyIsAccepted) {
  const char *const sources[] = {
      "@main def main(): Unit =\n  return\ndef f(a: Int): Int = a",
      "@main def main(): Unit =\n  return\ndef f(a: Int): Int = a\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    Parsed p(source);
    EXPECT_EQ(p.status, PARSER_OK);
    EXPECT_EQ(p.program.functions.function_count, 2U);
    if (p.status == PARSER_OK && p.program.functions.function_count == 2U) {
      EXPECT_NE(p.program.functions.functions[1]->ast_root, nullptr);
    }
  }
}
