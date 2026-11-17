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
 *   list, after a function header `=` or after a `;` in a block must be
 *   reported as PARSER_LEXER_ERROR (exit code 1), never as OK or syntax.
 */
TEST(ParserCovStatusTest, LexerErrorAfterNewlineIsPropagated) {
  const char *const sources[] = {
      "def f(\n$",           // newline after `(`         (parser.c:942)
      "def f(a\n$",          // newline after a parameter (parser.c:976)
      "def f(a: Int\n$",     // newline after its type    (parser.c:995)
      "def f() =\n$",        // newline after `=`         (parser.c:1180)
      "def f() =\n  ;\n  $", // newline after `;`         (parser.c:840)
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(Parsed(source).status, PARSER_LEXER_ERROR);
  }
}

/*
 * AI-generated test block — block of separators ended by end of input.
 * Model: Claude (Claude Code).
 * Prompt:
 *   A block is read until a DEDENT or EOF (p_code_block, parser.c:828), as
 *   the accepted "def f() =\n  return" shows; a block whose last tokens are
 *   `;` separators may therefore end with end of input, and its AST_BLOCK
 *   holds only the real statements.
 */
TEST(ParserCovStatusTest, SeparatorsBeforeEndOfInputCloseTheBlock) {
  struct Case {
    const char *source;
    size_t statements;
  };
  const Case cases[] = {
      {"def f() =\n  ;", 0U},
      {"def f() =\n  return\n  ;", 1U},
      {"def f() =\n  return\n  ;;", 1U},
  };
  for (const Case &c : cases) {
    SCOPED_TRACE(c.source);
    Parsed p(c.source);
    ASSERT_EQ(p.status, PARSER_OK);
    ASSERT_EQ(p.program.functions.function_count, 1U);
    ASTNode *root = p.program.functions.functions[0]->ast_root;
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(root->kind, AST_BLOCK);
    EXPECT_EQ(root->data.block.statement_count, c.statements);
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
// BUG: parser.c:1226 – INDENTs after @main are skipped untracked (NULL delta);
// the matching DEDENTs reach p_program (parser.c:1291) only when a final
// newline makes the lexer emit them, without it EOF hides them.
TEST(ParserCovKnownGapTest,
     DISABLED_FinalNewlineDoesNotChangeIndentedMainVerdict) {
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
 *   p_function), with or without a final newline. Two causes keep it red:
 *   p_expression is not called in p_function, and p_program does not skip
 *   the top-level NEWLINE left after the implicit return.
 */
// BUG: parser.c:1163 – p_expression call is commented out, ast_root stays
// NULL and `a` is left for p_program; same cause as
// ParserGapKnownGapTest.DISABLED_MissingFunctionBodyIsSyntaxError.
// BUG: parser.c:1276 – p_program does not skip top-level NEWLINE (nor does
// p_function consume the one after the implicit return), so the variant with
// `\n` stays red even after the first fix; same cause as
// ParserGapKnownGapTest.DISABLED_LeadingBlankLineIsAccepted.
TEST(ParserCovKnownGapTest, DISABLED_ExpressionBodyIsAccepted) {
  const char *const sources[] = {
      "def f(a: Int): Int = a",   // needs only the parser.c:1163 fix
      "def f(a: Int): Int = a\n", // needs the parser.c:1276 fix as well
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    Parsed p(source);
    EXPECT_EQ(p.status, PARSER_OK);
    EXPECT_EQ(p.program.functions.function_count, 1U);
    if (p.status == PARSER_OK && p.program.functions.function_count == 1U) {
      EXPECT_NE(p.program.functions.functions[0]->ast_root, nullptr);
    }
  }
}
