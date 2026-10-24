#include <cstdio>

#include "gtest/gtest.h"

extern "C" {
#include "../src/lexer.h"
}

namespace {

/*
 * AI-generated test block — temporary input stream fixture.
 * Model: gpt-5.6-terra (OpenAI Codex via Hermes Agent).
 * Prompt:
 *   Implement only stream_from below for GoogleTest. Write a NUL-terminated
 *   input string to a temporary FILE, rewind it, and return nullptr if creation
 *   fails. The helper must not own the returned stream after the test closes
 * it.
 */
/** Creates a rewinded temporary stream containing text. */
FILE *stream_from(const char *text) {
  FILE *stream = tmpfile();
  if (stream == nullptr) {
    return nullptr;
  }
  fputs(text, stream);
  rewind(stream);
  return stream;
}

struct ExpectedToken {
  TokenType type;
  const char *lexeme;
};

void expect_token_sequence(const char *input, const ExpectedToken *expected,
                           size_t expected_count) {
  FILE *stream = stream_from(input);
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  for (size_t index = 0U; index < expected_count; index += 1U) {
    Token token;
    ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK) << index;
    EXPECT_EQ(token.type, expected[index].type) << index;
    if (expected[index].lexeme != nullptr) {
      EXPECT_STREQ(token.value.string_value, expected[index].lexeme) << index;
    }
    token_destroy(&token);
  }

  Token token;
  EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_EOF);
  lexer_destroy(&lexer);
  fclose(stream);
}

} // namespace

/*
 * AI-generated test block — Period DFA state.
 * Model: gpt-5.6-terra (OpenAI Codex via Hermes Agent).
 * Prompt:
 *   Implement only the two tests below. Verify KA.md transitions S -> Period
 *   on '.', then Period -> TOKEN_DOT without a following digit and Period ->
 *   Double with a following digit. Check lexeme, value, and source position.
 */
TEST(LexerTest, PeriodIsDotWhenNotFollowedByDigit) {
  FILE *stream = stream_from(".");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);

  Token token;
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_DOT);
  EXPECT_STREQ(token.value.string_value, ".");
  EXPECT_EQ(token.line, 1U);
  EXPECT_EQ(token.column, 1U);
  token_destroy(&token);

  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_EOF);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(LexerTest, PeriodFollowedByDigitIsDouble) {
  FILE *stream = stream_from(".5");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);

  Token token;
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_DOUBLE);
  EXPECT_DOUBLE_EQ(token.value.double_value, 0.5);
  token_destroy(&token);

  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_EOF);
  lexer_destroy(&lexer);
  fclose(stream);
}

/*
 * AI-generated test block — bounded numeric-literal DFA.
 * Model: gpt-5.6-terra (OpenAI Codex via Hermes Agent).
 * Prompt:
 *   Implement only the three tests below for the numeric transitions in KA.md
 *   and IFJ26 specification §1.1: separators occur only between digits, an
 *   integer retains its original lexeme and parsed value, and a decimal may
 *   use separators, exponent sign, and d/D suffix. Do not test other lexer
 *   states. Use GoogleTest and existing stream_from/token_destroy helpers.
 */
TEST(LexerTest, IntegerWithDigitSeparatorsIsInteger) {
  FILE *stream = stream_from("1_000_000");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);

  Token token;
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_INT);
  EXPECT_EQ(token.value.int_value, 1000000);
  token_destroy(&token);

  EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_EOF);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(LexerTest, DecimalWithExponentSuffixAndSeparatorsIsDouble) {
  FILE *stream = stream_from("1_2.3_4e+5_6D");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);

  Token token;
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_DOUBLE);
  EXPECT_DOUBLE_EQ(token.value.double_value, 12.34e56);
  token_destroy(&token);

  EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_EOF);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(LexerTest, SeparatorMustBeBetweenDigits) {
  FILE *stream = stream_from("12_");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);

  Token token;
  EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_INVALID_TOKEN);
  lexer_destroy(&lexer);
  fclose(stream);
}

/*
 * AI-generated test block — identifiers and strings from KA.md.
 * Model: gpt-5.6-terra (OpenAI Codex via Hermes Agent).
 * Prompt:
 *   Add only tests for the identifier and string DFA groups in KA.md. Cover a
 *   standalone wildcard, an identifier with digits/underscores, an empty and
 *   escaped normal string, and a multiline string containing LF. Returned
 *   lexemes retain their source spelling including string delimiters.
 */
TEST(LexerTest, IdentifiersWildcardAndStringsFollowKa) {
  static const ExpectedToken expected[] = {
      {TOKEN_WILDCARD, "_"},
      {TOKEN_IDENTIFIER, "alpha_2"},
      {TOKEN_STRING, "\"\""},
      {TOKEN_STRING, "\"a\\n\\\"\""},
      {TOKEN_STRING, "\"\"\"first\nsecond\"\"\""},
  };

  expect_token_sequence("_ alpha_2 \"\" \"a\\n\\\"\" \"\"\"first\nsecond\"\"\"",
                        expected, sizeof(expected) / sizeof(expected[0]));
}

/*
 * AI-generated test block — all remaining fixed KA.md tokens.
 * Model: gpt-5.6-terra (OpenAI Codex via Hermes Agent).
 * Prompt:
 *   Add one exact token-sequence test for every non-numeric, non-string,
 *   non-identifier final state in KA.md: whitespace and newline, @main, and
 *   all listed one- and two-character operators and delimiters. Keep input
 *   spaces deliberate because KA.md includes TOKEN_SPACE.
 */
TEST(LexerTest, FixedTokensFollowKa) {
  static const ExpectedToken expected[] = {
      {TOKEN_ASSIGN, "="},
      {TOKEN_EQUAL, "=="},
      {TOKEN_NOT, "!"},
      {TOKEN_AND, "&&"},
      {TOKEN_NOT_EQUAL, "!="},
      {TOKEN_ARROW, "=>"},
      {TOKEN_GREATER, ">"},
      {TOKEN_LESS, "<"},
      {TOKEN_GREATER_EQUAL, ">="},
      {TOKEN_LESS_EQUAL, "<="},
      {TOKEN_GENERATOR, "<-"},
      {TOKEN_MAIN_ANNOTATION, "@main"},
      {TOKEN_PLUS, "+"},
      {TOKEN_CONCAT, "++"},
      {TOKEN_MINUS, "-"},
      {TOKEN_MULTIPLY, "*"},
      {TOKEN_DIVIDE, "/"},
      {TOKEN_LEFT_PAREN, "("},
      {TOKEN_RIGHT_PAREN, ")"},
      {TOKEN_COLON, ":"},
      {TOKEN_LEFT_BRACKET, "["},
      {TOKEN_RIGHT_BRACKET, "]"},
      {TOKEN_SEMICOLON, ";"},
      {TOKEN_NEWLINE, "\n"},
      {TOKEN_COMMA, ","},
      {TOKEN_UNION, "|"},
      {TOKEN_OR, "||"},
  };

  expect_token_sequence(
      "= == ! && != => > < >= <= <- @main + ++ - * / ( ) : [ ] ;\n, | || ",
      expected, sizeof(expected) / sizeof(expected[0]));
}

/*
 * AI-generated test block — final-state lookahead preservation.
 * Model: gpt-5.6-terra (OpenAI Codex via Hermes Agent).
 * Prompt:
 *   Add one lexer behavior test for the KA final-state rule: when no outgoing
 *   transition matches lookahead in a final state, lexer_pls returns that token
 *   without consuming lookahead. Verify @mainx becomes TOKEN_MAIN_ANNOTATION
 *   followed by TOKEN_IDENTIFIER x; do not judge syntactic validity.
 */
TEST(LexerTest, FinalStateLeavesLookaheadForNextToken) {
  static const ExpectedToken expected[] = {
      {TOKEN_MAIN_ANNOTATION, "@main"},
      {TOKEN_IDENTIFIER, "x"},
  };
  expect_token_sequence("@mainx", expected,
                        sizeof(expected) / sizeof(expected[0]));
}

/*
 * AI-generated test block — remaining lexer boundary behavior.
 * Model: gpt-5.6-terra (OpenAI Codex via Hermes Agent).
 * Prompt:
 *   Add behavioral GoogleTest coverage only for KA.md DFA transitions and
 *   accepting/non-accepting end states that are also unambiguously covered by
 *   IFJ26 specification §1.1: numeric boundaries, valid/invalid strings,
 *   ASCII identifiers, incomplete operator/annotation prefixes, and source
 *   positions. Exercise lexer_init/lexer_pls/token_destroy; do not inspect DFA
 *   tables or add tests for keywords or comments, because KA.md omits them.
 * Sources and boundary: KA.md; Specifikace_IFJ26_a_IFJcode26.pdf §1.1
 * (revision 3. 10. 2026); public API src/lexer.h. This block deliberately
 * tests their intersection only, not functionality absent from KA.md.
 */
TEST(LexerTest, NumericFinalStatesAcceptBoundaryForms) {
  struct NumericCase {
    const char *input;
    TokenType type;
    long long int_value;
    double double_value;
  };
  static const NumericCase cases[] = {
      {"0", TOKEN_INT, 0, 0.0},        {"42", TOKEN_INT, 42, 0.0},
      {"0e2", TOKEN_DOUBLE, 0, 0.0},   {"7E2", TOKEN_DOUBLE, 0, 700.0},
      {"7.25", TOKEN_DOUBLE, 0, 7.25}, {"7.25d", TOKEN_DOUBLE, 0, 7.25},
  };

  for (const NumericCase &numeric_case : cases) {
    SCOPED_TRACE(numeric_case.input);
    FILE *stream = stream_from(numeric_case.input);
    ASSERT_NE(stream, nullptr);

    Lexer lexer;
    ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
    Token token;
    ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
    EXPECT_EQ(token.type, numeric_case.type);
    if (numeric_case.type == TOKEN_INT) {
      EXPECT_EQ(token.value.int_value, numeric_case.int_value);
    } else {
      EXPECT_DOUBLE_EQ(token.value.double_value, numeric_case.double_value);
    }
    token_destroy(&token);
    EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_EOF);
    lexer_destroy(&lexer);
    fclose(stream);
  }
}

TEST(LexerTest, IncompleteNumericFormsAreLexicalErrors) {
  static const char *const inputs[] = {
      "1_", "1.", "1.0_", "1e", "1e+", "1e0_",
  };

  for (const char *input : inputs) {
    SCOPED_TRACE(input);
    FILE *stream = stream_from(input);
    ASSERT_NE(stream, nullptr);

    Lexer lexer;
    ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
    Token token;
    EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_INVALID_TOKEN);
    lexer_destroy(&lexer);
    fclose(stream);
  }
}

TEST(LexerTest, NumericFinalStatesPreserveFollowingToken) {
  FILE *stream = stream_from("42foo 1.5bar 2e3+");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Token token;

  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_INT);
  EXPECT_EQ(token.value.int_value, 42);
  token_destroy(&token);
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_IDENTIFIER);
  EXPECT_STREQ(token.value.string_value, "foo");
  token_destroy(&token);
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_DOUBLE);
  EXPECT_DOUBLE_EQ(token.value.double_value, 1.5);
  token_destroy(&token);
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_IDENTIFIER);
  EXPECT_STREQ(token.value.string_value, "bar");
  token_destroy(&token);
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_DOUBLE);
  EXPECT_DOUBLE_EQ(token.value.double_value, 2000.0);
  token_destroy(&token);
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_PLUS);
  token_destroy(&token);
  EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_EOF);

  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(LexerTest, NormalStringAcceptsEverySpecifiedEscape) {
  FILE *stream = stream_from("\"\\\\\\\"\\n\\r\\t\"");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Token token;
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_STRING);
  EXPECT_STREQ(token.value.string_value, "\"\\\\\\\"\\n\\r\\t\"");
  token_destroy(&token);
  EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_EOF);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(LexerTest, InvalidOrUnterminatedNormalStringIsLexicalError) {
  static const char *const inputs[] = {"\"\\q\"", "\"line\nbreak\"",
                                       "\"unterminated"};

  for (const char *input : inputs) {
    SCOPED_TRACE(input);
    FILE *stream = stream_from(input);
    ASSERT_NE(stream, nullptr);

    Lexer lexer;
    ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
    Token token;
    EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_INVALID_TOKEN);
    lexer_destroy(&lexer);
    fclose(stream);
  }
}

TEST(LexerTest, MultilineStringAllowsQuotesAndLiteralEscapes) {
  FILE *stream = stream_from("\"\"\"one\"\"two\\q\nthree\"\"\"");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Token token;
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_STRING);
  EXPECT_STREQ(token.value.string_value, "\"\"\"one\"\"two\\q\nthree\"\"\"");
  token_destroy(&token);
  EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_EOF);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(LexerTest, UnterminatedMultilineStringIsLexicalError) {
  FILE *stream = stream_from("\"\"\"unterminated");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Token token;
  EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_INVALID_TOKEN);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(LexerTest, IdentifierMayStartWithUnderscoreWhenNotWildcard) {
  static const ExpectedToken expected[] = {
      {TOKEN_IDENTIFIER, "_9"},
      {TOKEN_IDENTIFIER, "__"},
      {TOKEN_IDENTIFIER, "Z_9"},
  };
  expect_token_sequence("_9 __ Z_9", expected,
                        sizeof(expected) / sizeof(expected[0]));
}

TEST(LexerTest, IncompleteMandatoryPrefixesAreLexicalErrors) {
  static const char *const inputs[] = {"&", "@", "@m", "@ma", "@mai"};

  for (const char *input : inputs) {
    SCOPED_TRACE(input);
    FILE *stream = stream_from(input);
    ASSERT_NE(stream, nullptr);

    Lexer lexer;
    ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
    Token token;
    EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_INVALID_TOKEN);
    lexer_destroy(&lexer);
    fclose(stream);
  }
}

TEST(LexerTest, NewlineProducesIndentAndDedentFromLeadingSpaces) {
  static const ExpectedToken expected[] = {
      {TOKEN_IDENTIFIER, "a"}, {TOKEN_NEWLINE, "\n  "}, {TOKEN_INDENT, nullptr},
      {TOKEN_INDENT, nullptr}, {TOKEN_IDENTIFIER, "b"}, {TOKEN_NEWLINE, "\n"},
      {TOKEN_DEDENT, nullptr}, {TOKEN_DEDENT, nullptr}, {TOKEN_IDENTIFIER, "c"},
  };

  expect_token_sequence("a\n  b\nc", expected,
                        sizeof(expected) / sizeof(expected[0]));
}

TEST(LexerTest, NewlineDfaResetsIndentationAcrossBlankTabbedLines) {
  static const ExpectedToken expected[] = {
      {TOKEN_IDENTIFIER, "a"}, {TOKEN_NEWLINE, "\n   \t\n  \n   "},
      {TOKEN_INDENT, nullptr}, {TOKEN_INDENT, nullptr},
      {TOKEN_INDENT, nullptr}, {TOKEN_IDENTIFIER, "b"},
  };

  expect_token_sequence("a\n   \t\n  \n   b", expected,
                        sizeof(expected) / sizeof(expected[0]));
}

TEST(LexerTest, IgnoresHorizontalWhitespaceInsideLine) {
  static const ExpectedToken expected[] = {
      {TOKEN_IDENTIFIER, "a"},
      {TOKEN_IDENTIFIER, "b"},
  };
  expect_token_sequence("a \t b", expected,
                        sizeof(expected) / sizeof(expected[0]));
}

TEST(LexerTest, RejectsTabsInPrefixOfNonemptyLine) {
  FILE *stream = stream_from("a\n \tb");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  Token token;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  token_destroy(&token);
  EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_INVALID_TOKEN);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(LexerTest, TokensReportPositionsAcrossNewline) {
  FILE *stream = stream_from("\"a\"\nxyz");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Token token;
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_STRING);
  EXPECT_EQ(token.line, 1U);
  EXPECT_EQ(token.column, 1U);
  token_destroy(&token);
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_NEWLINE);
  EXPECT_EQ(token.line, 1U);
  EXPECT_EQ(token.column, 4U);
  token_destroy(&token);
  ASSERT_EQ(lexer_pls(&lexer, &token), LEXER_OK);
  EXPECT_EQ(token.type, TOKEN_IDENTIFIER);
  EXPECT_EQ(token.line, 2U);
  EXPECT_EQ(token.column, 1U);
  token_destroy(&token);
  EXPECT_EQ(lexer_pls(&lexer, &token), LEXER_EOF);
  lexer_destroy(&lexer);
  fclose(stream);
}
