#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "../src/keyword.h"
#include "../src/lexer.h"
}

namespace {
/*
 * AI-generated test block — shared lexer test helpers.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   One stream_from plus lex_all that copies every token until the first
 *   non-OK status, and Expected rows (T/I/D) for token tables.
 */
FILE *stream_from(const char *text) {
  FILE *stream = tmpfile();
  if (stream != nullptr) {
    fputs(text, stream);
    rewind(stream);
  }
  return stream;
}
bool owns_text(TokenType type) {
  return type != TOKEN_INT && type != TOKEN_DOUBLE && type != TOKEN_INDENT &&
         type != TOKEN_DEDENT && type != TOKEN_EOF && type != TOKEN_INVALID;
}
struct Lexed {
  Token token;
  std::string text;
}; // text outlives the lexer
struct LexResult {
  std::vector<Lexed> tokens;
  LexerStatus status;
  Token last;
};

LexResult lex_all(const char *input) {
  LexResult result{{}, LEXER_INVALID_ARGUMENT, Token{}};
  FILE *stream = stream_from(input);
  Lexer lexer;
  if (stream == nullptr || lexer_init(&lexer, stream) != LEXER_OK) {
    ADD_FAILURE() << "cannot set up lexer"; // only on a null stream
    return result;
  }
  while ((result.status = lexer_pls(&lexer)) == LEXER_OK) {
    Lexed lexed{lexer.token, ""};
    if (owns_text(lexer.token.type)) {
      lexed.text = lexer.token.value.string_value;
      lexed.token.value.string_value = nullptr;
    }
    result.tokens.push_back(lexed);
  }
  result.last = lexer.token;
  lexer_destroy(&lexer);
  fclose(stream);
  return result;
}

struct Expected {
  TokenType type;
  const char *text;
  long long i;
  double d;
};
Expected T(TokenType t, const char *text = nullptr) { return {t, text, 0, 0}; }
Expected I(long long value) { return {TOKEN_INT, nullptr, value, 0.0}; }
Expected D(double value) { return {TOKEN_DOUBLE, nullptr, 0, value}; }
std::string shown(const char *input) {
  return testing::PrintToString(std::string(input));
}

void expect_tokens(const LexResult &result, const std::vector<Expected> &want) {
  EXPECT_EQ(result.status, LEXER_EOF);
  ASSERT_EQ(result.tokens.size(), want.size());
  for (size_t index = 0U; index < want.size(); index += 1U) {
    SCOPED_TRACE(index);
    const Token &got = result.tokens[index].token;
    EXPECT_EQ(got.type, want[index].type);
    if (want[index].type == TOKEN_INT) {
      EXPECT_EQ(got.value.int_value, want[index].i);
    } else if (want[index].type == TOKEN_DOUBLE) {
      EXPECT_DOUBLE_EQ(got.value.double_value, want[index].d);
    } else if (want[index].text != nullptr) {
      EXPECT_EQ(result.tokens[index].text, want[index].text);
    }
  }
}
} // namespace

/*
 * AI-generated test block — accepted inputs and their token sequences.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   KA.md paths missing from lexer_test.cc: numbers, multiline quotes,
 *   maximal munch, near-keywords (iff, if_, Intx), unaligned dedent.
 */
TEST(LexerGapTest, AcceptedInputsProduceExpectedTokens) {
  const auto id = [](const char *text) { return T(TOKEN_IDENTIFIER, text); };
  const struct {
    const char *input;
    std::vector<Expected> tokens;
  } cases[] = {
      {"", {}},
      {"01", {I(0), I(1)}},
      {"007", {I(0), I(0), I(7)}},
      {"1__0", {I(10)}},
      {"0.5", {D(0.5)}},
      {"1.5__5", {D(1.55)}},
      {"1.5E3", {D(1500.0)}},
      {"1e-5", {D(1e-5)}},
      {"1e5d", {D(1e5)}},
      {"1e5D", {D(1e5)}},
      {"1e5__5", {D(1e55)}},
      {".5e3", {D(500.0)}},
      {".5_5", {D(0.55)}},
      {".5d", {D(0.5)}},
      {"0_1", {I(0), id("_1")}},
      {"9223372036854775807", {I(LLONG_MAX)}},
      {"5d", {I(5), id("d")}},
      {"1.5dx", {D(1.5), id("x")}},
      {"a \t ", {id("a")}},
      {"..", {T(TOKEN_DOT, "."), T(TOKEN_DOT, ".")}},
      {"\"\"\"\"\"\"", {T(TOKEN_STRING, "\"\"\"\"\"\"")}},
      {"\"\"\"a\"\"b\"\"\"", {T(TOKEN_STRING, "\"\"\"a\"\"b\"\"\"")}},
      {"\"a\tb\" \"a\rb\" \"if\" \"\"\"while\"\"\"",
       {T(TOKEN_STRING, "\"a\tb\""), T(TOKEN_STRING, "\"a\rb\""),
        T(TOKEN_STRING, "\"if\""), T(TOKEN_STRING, "\"\"\"while\"\"\"")}},
      {"=== !== <-- +++ |||",
       {T(TOKEN_EQUAL, "=="), T(TOKEN_ASSIGN, "="), T(TOKEN_NOT_EQUAL, "!="),
        T(TOKEN_ASSIGN, "="), T(TOKEN_FOR_GENERATOR, "<-"), T(TOKEN_MINUS, "-"),
        T(TOKEN_LIST_CONCAT, "++"), T(TOKEN_PLUS, "+"), T(TOKEN_OR, "||"),
        T(TOKEN_UNION, "|")}},
      {"iff if_ Intx If double if2",
       {id("iff"), id("if_"), id("Intx"), id("If"), id("double"), id("if2")}},
      {"a\n    b\n  c",
       {id("a"), T(TOKEN_NEWLINE), T(TOKEN_INDENT), T(TOKEN_INDENT),
        T(TOKEN_INDENT), T(TOKEN_INDENT), id("b"), T(TOKEN_NEWLINE),
        T(TOKEN_DEDENT), T(TOKEN_DEDENT), id("c")}},
      {"1.5.2", {D(1.5), D(0.2)}},
      {"x.5", {id("x"), D(0.5)}},
      {".e5", {T(TOKEN_DOT, "."), id("e5")}},
      {"..5", {T(TOKEN_DOT, "."), D(0.5)}},
      {"1.5dd", {D(1.5), id("d")}},
      {"\"\"\"\"a\"\"\"", {T(TOKEN_STRING, "\"\"\"\"a\"\"\"")}},
      {"->", {T(TOKEN_MINUS, "-"), T(TOKEN_GREATER, ">")}},
      {"a<-1", {id("a"), T(TOKEN_FOR_GENERATOR, "<-"), I(1)}},
      {"a < -1", {id("a"), T(TOKEN_LESS, "<"), T(TOKEN_MINUS, "-"), I(1)}},
  };
  for (const auto &test_case : cases) {
    SCOPED_TRACE(shown(test_case.input));
    expect_tokens(lex_all(test_case.input), test_case.tokens);
  }
}

/*
 * AI-generated test block — inputs rejected with LEXER_INVALID_TOKEN.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Bad number tails, bad escapes, non-ASCII, incomplete & / @main, chars
 *   with no edge from S, CRLF, FF/VT and a tab-only last line must fail.
 */
TEST(LexerGapTest, RejectedInputsEndInInvalidToken) {
  static const char *const inputs[] = {
      "1.e5",
      "1._5",
      "0.",
      "0.e1",
      "\"\\0\"",
      "\"\\",
      "\"a\\",
      "\"\\\n\"",
      "\"\\x\"",
      "&x",
      "@x",
      "@maix",
      "@Main",
      "#",
      "$",
      "%",
      "{",
      "}",
      "?",
      "'",
      "\\",
      "\r",
      "\f",
      "\v",
      "~",
      "^",
      "\x80",
      "\xFF",
      " a",
      "\t",
      "a\fb",
      "a\vb",
      "a\r\nb",
      "a\n\t",
      "\"caf\xC3\xA9\"",
      "\"\"\"\xC3\xA9\"\"\"",
      "\"\"\"a\"\"\"\"",
      "\"\"\"\"",
      "&&&",
  };
  for (const char *input : inputs) {
    SCOPED_TRACE(shown(input));
    EXPECT_EQ(lex_all(input).status, LEXER_INVALID_TOKEN);
  }
}

/*
 * AI-generated test block — small API tests.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Layout/EOF positions, position after a multiline string, buffer growth,
 *   NULL arguments, and the keyword table vs the TOKEN_KW_* range.
 */
TEST(LexerGapTest, LayoutTokensCarryEmissionPosition) {
  // a NEWLINE INDENT INDENT b NEWLINE DEDENT DEDENT c (types: lexer_test.cc)
  LexResult result = lex_all("a\n  b\nc");
  static const size_t positions[][2] = {{1, 1}, {1, 2}, {2, 3}, {2, 3}, {2, 3},
                                        {2, 4}, {3, 1}, {3, 1}, {3, 1}};
  ASSERT_EQ(result.status, LEXER_EOF);
  ASSERT_EQ(result.tokens.size(), 9U);
  for (size_t index = 0U; index < 9U; index += 1U) {
    SCOPED_TRACE(index);
    EXPECT_EQ(result.tokens[index].token.line, positions[index][0]);
    EXPECT_EQ(result.tokens[index].token.column, positions[index][1]);
  }
  EXPECT_EQ(result.last.line, 3U);
  EXPECT_EQ(result.last.column, 2U);
}

TEST(LexerGapTest, PositionAfterMultilineString) {
  LexResult result = lex_all("\"\"\"a\nb\"\"\" \t x");
  ASSERT_EQ(result.tokens.size(), 2U);
  EXPECT_EQ(result.tokens[1].token.line, 2U);
  EXPECT_EQ(result.tokens[1].token.column, 8U);
}

TEST(LexerGapTest, LongIdentifierGrowsTokenBuffer) {
  for (size_t length : {16U, 17U, 1000U}) {
    SCOPED_TRACE(length);
    const std::string identifier(length, 'x');
    LexResult result = lex_all((identifier + " y").c_str());
    ASSERT_EQ(result.tokens.size(), 2U);
    EXPECT_EQ(result.tokens[0].text, identifier);
    EXPECT_EQ(result.tokens[1].token.column, length + 2U);
  }
}

TEST(LexerGapTest, NullArgumentsAreRejectedOrIgnored) {
  FILE *stream = stream_from("x");
  ASSERT_NE(stream, nullptr);
  Lexer lexer;
  EXPECT_EQ(lexer_init(nullptr, stream), LEXER_INVALID_ARGUMENT);
  EXPECT_EQ(lexer_init(&lexer, nullptr), LEXER_INVALID_ARGUMENT);
  EXPECT_EQ(lexer_pls(nullptr), LEXER_INVALID_ARGUMENT);
  EXPECT_EQ(lexer_change_current_indent(nullptr, 1), LEXER_INVALID_ARGUMENT);
  lexer_destroy(nullptr);
  token_destroy(nullptr);
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  lexer_destroy(&lexer);
  EXPECT_EQ(lexer_pls(&lexer), LEXER_INVALID_ARGUMENT);
  fclose(stream);
}

/*
 * AI-generated test block — indentation adjustment overflow guards.
 * Model: Claude (Claude Code).
 * Prompt:
 *   lexer_change_current_indent must reject negative and size_t overflow
 *   results without changing current_indent and must keep pending tokens.
 */
TEST(LexerGapTest, IndentAdjustmentRejectsOverflowAndKeepsPendingTokens) {
  FILE *stream = stream_from("");
  ASSERT_NE(stream, nullptr);
  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  EXPECT_EQ(lexer_change_current_indent(&lexer, PTRDIFF_MIN),
            LEXER_INVALID_ARGUMENT);
  EXPECT_EQ(lexer.current_indent, 0U);
  EXPECT_EQ(lexer_change_current_indent(&lexer, 0), LEXER_OK);
  EXPECT_EQ(lexer.current_indent, 0U);
  lexer.current_indent = SIZE_MAX - 1U;
  EXPECT_EQ(lexer_change_current_indent(&lexer, 1), LEXER_OK);
  EXPECT_EQ(lexer.current_indent, SIZE_MAX);
  EXPECT_EQ(lexer_change_current_indent(&lexer, 1), LEXER_INVALID_ARGUMENT);
  EXPECT_EQ(lexer.current_indent, SIZE_MAX);
  static_assert(SIZE_MAX - static_cast<size_t>(PTRDIFF_MAX) ==
                    static_cast<size_t>(PTRDIFF_MAX) + 1U,
                "expects size_t and ptrdiff_t of equal width");
  EXPECT_EQ(lexer_change_current_indent(&lexer, PTRDIFF_MIN), LEXER_OK);
  EXPECT_EQ(lexer.current_indent, static_cast<size_t>(PTRDIFF_MAX));
  lexer.current_indent = 4U;
  EXPECT_EQ(lexer_change_current_indent(&lexer, -4), LEXER_OK);
  EXPECT_EQ(lexer.current_indent, 0U);
  EXPECT_EQ(lexer_change_current_indent(&lexer, -1), LEXER_INVALID_ARGUMENT);
  EXPECT_EQ(lexer.current_indent, 0U);
  lexer.current_indent = 0U;
  lexer.pending_indent_delta = 3;
  EXPECT_EQ(lexer_change_current_indent(&lexer, 5), LEXER_OK);
  EXPECT_EQ(lexer.current_indent, 5U);
  EXPECT_EQ(lexer.pending_indent_delta, 3);
  lexer_destroy(&lexer);
  fclose(stream);
}

TEST(KeywordGapTest, KeywordTableCoversEveryKeywordTokenOnce) {
  LexResult result = lex_all(
      "def do Double else if import Int return String then Unit val var "
      "while Boolean true false case class extension for List match type");
  std::set<int> seen;
  for (const Lexed &lexed : result.tokens) {
    SCOPED_TRACE(lexed.text);
    const Keyword *keyword = keyword_lookup(lexed.text.c_str());
    ASSERT_NE(keyword, nullptr);
    EXPECT_EQ(keyword->type, lexed.token.type);
    EXPECT_TRUE(seen.insert(lexed.token.type).second) << "duplicate mapping";
  }
  EXPECT_EQ(seen.size(), static_cast<size_t>(TOKEN_KW_TYPE - TOKEN_KW_DEF + 1));
  EXPECT_EQ(*seen.begin(), TOKEN_KW_DEF);
  EXPECT_EQ(*seen.rbegin(), TOKEN_KW_TYPE);
}

/*
 * AI-generated test block — known lexer bugs (disabled until fixed).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write the correct expectation for each bug; keep it DISABLED_ until fixed.
 */
// Bug lexer.c:213: strtoll saturates to LLONG_MAX without an errno check.
TEST(LexerGapTest, DISABLED_IntegerOverflowIsLexicalError) {
  EXPECT_EQ(lex_all("9223372036854775808").status, LEXER_INVALID_TOKEN);
  EXPECT_EQ(lex_all("99999999999999999999").status, LEXER_INVALID_TOKEN);
}

// Bug lexer.c:382: a char rejected in S is never consumed, so it repeats.
TEST(LexerGapTest, DISABLED_LexerAdvancesAfterInvalidToken) {
  FILE *stream = stream_from("#a");
  ASSERT_NE(stream, nullptr);
  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  EXPECT_EQ(lexer_pls(&lexer), LEXER_INVALID_TOKEN);
  ASSERT_EQ(lexer_pls(&lexer), LEXER_OK);
  EXPECT_STREQ(lexer.token.value.string_value, "a");
  EXPECT_EQ(lexer_pls(&lexer), LEXER_EOF);
  lexer_destroy(&lexer);
  fclose(stream);
}
