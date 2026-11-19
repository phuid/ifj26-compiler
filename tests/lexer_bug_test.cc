#include <cstdio>
#include <cstring>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "../src/lexer.h"
}

/*
 * AI-generated test block — lexer spec deviations found by bug hunting.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write GoogleTest cases for lexer behaviour that contradicts
 *   specifikace-jazyka.pdf section 1.1/1.2: comments are not recognised,
 *   whitespace before the first token is rejected, literals with a leading
 *   zero are split into several tokens, whitespace-only last lines change
 *   the indentation or fail on a tab, a multiline string ending with more
 *   than three quotes is cut short, and an out-of-range Double literal is
 *   accepted. These tests are expected to fail until the lexer is fixed.
 */
namespace {

/** Returns a null-terminated copy of @p text for the lexer to borrow. */
std::vector<char> buffer_from(const char *text) {
  return std::vector<char>(text, text + strlen(text) + 1);
}

/** Owns a lexer over an in-memory source for one test. */
class LexerBugTest : public ::testing::Test {
protected:
  std::vector<char> buffer;
  Lexer lexer{};

  void open(const char *text) {
    buffer = buffer_from(text);
    ASSERT_EQ(lexer_init(&lexer, buffer.data()), LEXER_OK);
  }

  void TearDown() override { lexer_destroy(&lexer); }

  /// Reads the next token and checks it is an identifier spelled @p name.
  void expect_identifier(const char *name) {
    ASSERT_EQ(lexer_pls(&lexer), LEXER_OK);
    ASSERT_EQ(lexer.token.type, TOKEN_IDENTIFIER);
    EXPECT_STREQ(lexer.token.value.string_value, name);
  }

  void expect_token(TokenType type) {
    ASSERT_EQ(lexer_pls(&lexer), LEXER_OK);
    EXPECT_EQ(lexer.token.type, type);
  }
};

} // namespace

TEST_F(LexerBugTest, LineCommentIsIgnoredAndCountsAsNewline) {
  // Spec 1.1: `//` starts a comment up to the end of the line; it is one EOL.
  open("a // comment\nb");
  expect_identifier("a");
  expect_token(TOKEN_NEWLINE);
  expect_identifier("b");
  EXPECT_EQ(lexer_pls(&lexer), LEXER_EOF);
}

TEST_F(LexerBugTest, NestedBlockCommentIsIgnored) {
  // Spec 1.1: block comments /* */ may be nested.
  open("a /* x /* y */ z */ b");
  expect_identifier("a");
  expect_identifier("b");
  EXPECT_EQ(lexer_pls(&lexer), LEXER_EOF);
}

TEST_F(LexerBugTest, UnterminatedBlockCommentIsLexicalError) {
  // Spec 1.1: an unterminated (also nested) comment is lexical error 1.
  open("a /* x /* y */");
  expect_identifier("a");
  EXPECT_EQ(lexer_pls(&lexer), LEXER_INVALID_TOKEN);
}

TEST_F(LexerBugTest, WhitespaceBeforeFirstTokenIsSkipped) {
  // Spec 2: only whitespace and comments may precede the prolog.
  open("  import");
  expect_token(TOKEN_KW_IMPORT);
  EXPECT_EQ(lexer_pls(&lexer), LEXER_EOF);
}

TEST_F(LexerBugTest, IntLiteralWithLeadingZeroIsLexicalError) {
  // Spec 1.1: a non-zero literal must not start with zero.
  open("007");
  EXPECT_EQ(lexer_pls(&lexer), LEXER_INVALID_TOKEN);
}

TEST_F(LexerBugTest, LeadingZeroBeforeSeparatorIsLexicalError) {
  open("0_1");
  EXPECT_EQ(lexer_pls(&lexer), LEXER_INVALID_TOKEN);
}

TEST_F(LexerBugTest, LeadingZeroDoubleIsNotSplitIntoIntAndDouble) {
  // `01.5` must be one Double or a lexical error, never INT(0) DOUBLE(1.5).
  open("01.5");
  LexerStatus status = lexer_pls(&lexer);
  if (status == LEXER_OK) {
    EXPECT_EQ(lexer.token.type, TOKEN_DOUBLE);
    EXPECT_EQ(lexer_pls(&lexer), LEXER_EOF);
  } else {
    EXPECT_EQ(status, LEXER_INVALID_TOKEN);
  }
}

TEST_F(LexerBugTest, WhitespaceOnlyLastLineDoesNotIndent) {
  // Spec 1.2: whitespace-only lines do not change the indentation level.
  open("x\n   ");
  expect_identifier("x");
  expect_token(TOKEN_NEWLINE);
  EXPECT_EQ(lexer_pls(&lexer), LEXER_EOF);
}

TEST_F(LexerBugTest, TabOnWhitespaceOnlyLastLineIsAccepted) {
  // Spec 1.2: a tab is an error only at the start of a non-empty line.
  open("a\n\t");
  expect_identifier("a");
  expect_token(TOKEN_NEWLINE);
  EXPECT_EQ(lexer_pls(&lexer), LEXER_EOF);
}

TEST_F(LexerBugTest, MultilineStringMayEndWithExtraQuote) {
  // Scala semantics (spec 1): `"""a""""` is one string with content `a"`.
  open("\"\"\"a\"\"\"\"");
  expect_token(TOKEN_STRING);
  EXPECT_EQ(lexer_pls(&lexer), LEXER_EOF);
}

TEST_F(LexerBugTest, MultilineStringWithFiveClosingQuotesIsOneToken) {
  // Today this is silently split into `"""a"""` and `""`.
  open("\"\"\"a\"\"\"\"\"");
  expect_token(TOKEN_STRING);
  EXPECT_EQ(lexer_pls(&lexer), LEXER_EOF);
}

TEST_F(LexerBugTest, DoubleLiteralOutOfRangeIsLexicalError) {
  // Spec 1.1: a literal outside the binary64 range is lexical error 1.
  open("1e999");
  EXPECT_EQ(lexer_pls(&lexer), LEXER_INVALID_TOKEN);
}
