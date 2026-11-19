#include <cfloat>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "../src/lexer.h"
}

/*
 * AI-generated test block — shared helpers for lexer edge-case tests.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write helpers that lex a byte string (NUL bytes allowed) to the first
 *   non-OK status, copy every token with its text and position, and compare
 *   the result with a compact table of expected tokens.
 */
namespace {

using namespace std::string_literals;

/**
 * Returns a null-terminated copy of @p bytes for the lexer to borrow. The
 * lexer reads a C string, so an embedded NUL ends the input there.
 */
std::vector<char> buffer_from(const std::string &bytes) {
  std::vector<char> buffer(bytes.begin(), bytes.end());
  buffer.push_back('\0');
  return buffer;
}

bool owns_text(TokenType type) {
  return type != TOKEN_INT && type != TOKEN_DOUBLE && type != TOKEN_INDENT &&
         type != TOKEN_DEDENT && type != TOKEN_EOF && type != TOKEN_INVALID;
}

/** A token copied out of the lexer; @c text outlives the lexer. */
struct Lexed {
  Token token;
  std::string text;
};

struct LexResult {
  std::vector<Lexed> tokens;
  LexerStatus status;
  Token last;
  size_t final_indent;
};

/** Hard stop so a lexer that never finishes cannot hang the test. */
const size_t kMaxTokens = 10000U;

LexResult lex_all(const std::string &input) {
  LexResult result{{}, LEXER_INVALID_ARGUMENT, Token{}, 0U};
  std::vector<char> buffer = buffer_from(input);
  Lexer lexer;
  if (lexer_init(&lexer, buffer.data()) != LEXER_OK) {
    ADD_FAILURE() << "cannot set up lexer";
    return result;
  }
  while (result.tokens.size() < kMaxTokens &&
         (result.status = lexer_pls(&lexer)) == LEXER_OK) {
    Lexed lexed{lexer.token, ""};
    if (owns_text(lexer.token.type)) {
      lexed.text = lexer.token.value.string_value;
      lexed.token.value.string_value = nullptr;
    }
    result.tokens.push_back(lexed);
  }
  result.last = lexer.token;
  result.final_indent = lexer.current_indent;
  lexer_destroy(&lexer);
  return result;
}

struct Expected {
  TokenType type;
  const char *text;
  long long int_value;
  double double_value;
};
Expected T(TokenType type, const char *text = nullptr) {
  return {type, text, 0, 0.0};
}
Expected Id(const char *text) { return T(TOKEN_IDENTIFIER, text); }
Expected I(long long value) { return {TOKEN_INT, nullptr, value, 0.0}; }
Expected D(double value) { return {TOKEN_DOUBLE, nullptr, 0, value}; }

void expect_token(const Lexed &got, const Expected &want) {
  EXPECT_EQ(got.token.type, want.type);
  if (want.type == TOKEN_INT) {
    EXPECT_EQ(got.token.value.int_value, want.int_value);
  } else if (want.type == TOKEN_DOUBLE) {
    EXPECT_DOUBLE_EQ(got.token.value.double_value, want.double_value);
  } else if (want.text != nullptr) {
    EXPECT_EQ(got.text, want.text);
  }
}

void expect_tokens(const LexResult &result, const std::vector<Expected> &want) {
  EXPECT_EQ(result.status, LEXER_EOF);
  ASSERT_EQ(result.tokens.size(), want.size());
  for (size_t index = 0U; index < want.size(); index += 1U) {
    SCOPED_TRACE(index);
    expect_token(result.tokens[index], want[index]);
  }
}

/** Token types of @p result with every TOKEN_NEWLINE removed. */
std::vector<int> types_without_newlines(const LexResult &result) {
  std::vector<int> types;
  for (const Lexed &lexed : result.tokens) {
    if (lexed.token.type != TOKEN_NEWLINE) {
      types.push_back(lexed.token.type);
    }
  }
  return types;
}

size_t count_type(const LexResult &result, TokenType type) {
  size_t count = 0U;
  for (const Lexed &lexed : result.tokens) {
    count += lexed.token.type == type ? 1U : 0U;
  }
  return count;
}

/** Owns one lexer over an in-memory source for step-by-step tests. */
class LexerEdgeTest : public ::testing::Test {
protected:
  std::vector<char> buffer;
  Lexer lexer{};

  void open(const std::string &text) {
    buffer = buffer_from(text);
    ASSERT_EQ(lexer_init(&lexer, buffer.data()), LEXER_OK);
  }

  void TearDown() override { lexer_destroy(&lexer); }

  void next(TokenType type) {
    ASSERT_EQ(lexer_pls(&lexer), LEXER_OK);
    EXPECT_EQ(lexer.token.type, type);
  }
};

} // namespace

/*
 * AI-generated test block — accepted inputs and their exact token sequences.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover IFJ26 spec 1.1/1.2 edge cases missing from lexer_test.cc and
 *   lexer_gap_test.cc: newline and blank-line layout, unary minus/plus with
 *   literals, number boundaries (separators, exponent signs, DBL_MAX,
 *   subnormal, suffix), string and multiline corner cases, non-keyword
 *   words, realistic extension lines and maximal munch of operator pairs.
 */
namespace {

struct AcceptedCase {
  const char *name;
  std::string input;
  std::vector<Expected> tokens;
};

class LexerEdgeAcceptedTest : public ::testing::TestWithParam<AcceptedCase> {};

const Expected kNl = T(TOKEN_NEWLINE);
const Expected kIndent = T(TOKEN_INDENT);

const AcceptedCase kLayoutCases[] = {
    {"NewlineOnly", "\n", {T(TOKEN_NEWLINE, "\n")}},
    {"NewlineAtEof", "x\n", {Id("x"), T(TOKEN_NEWLINE, "\n")}},
    {"BlankLinesMerge", "x\n\n\n", {Id("x"), T(TOKEN_NEWLINE, "\n\n\n")}},
    {"LeadingBlankLines", "\n\nx", {T(TOKEN_NEWLINE, "\n\n"), Id("x")}},
    {"TabBeforeNewline", "a\t\nb", {Id("a"), T(TOKEN_NEWLINE, "\n"), Id("b")}},
    {"SpacesBeforeNewline",
     "a   \nb",
     {Id("a"), T(TOKEN_NEWLINE, "\n"), Id("b")}},
    {"TabBlankLineInBlock",
     "a\n  b\n\t\n  c",
     {Id("a"), kNl, kIndent, kIndent, Id("b"), T(TOKEN_NEWLINE, "\n\t\n  "),
      Id("c")}},
    {"SpaceBlankLineInBlock",
     "a\n  b\n    \n  c",
     {Id("a"), kNl, kIndent, kIndent, Id("b"), T(TOKEN_NEWLINE, "\n    \n  "),
      Id("c")}},
};

const AcceptedCase kNumberCases[] = {
    {"UnaryMinusInt", "-5", {T(TOKEN_MINUS, "-"), I(5)}},
    {"UnaryMinusZero", "-0", {T(TOKEN_MINUS, "-"), I(0)}},
    {"UnaryMinusFraction", "-.5", {T(TOKEN_MINUS, "-"), D(0.5)}},
    {"UnaryMinusExponent", "-1e-5", {T(TOKEN_MINUS, "-"), D(1e-5)}},
    {"DoubleUnaryMinus", "--5", {T(TOKEN_MINUS), T(TOKEN_MINUS), I(5)}},
    {"UnaryPlus", "+5", {T(TOKEN_PLUS, "+"), I(5)}},
    {"Int32MinMagnitude", "-2147483648", {T(TOKEN_MINUS), I(2147483648LL)}},
    {"LlongMaxSeparators", "9_223_372_036_854_775_807", {I(LLONG_MAX)}},
    {"UpperExponentPlus", "1E+5", {D(1e5)}},
    {"ExponentMinusZero", "2e-0", {D(2.0)}},
    {"ZeroExponentSuffix", "0e0d", {D(0.0)}},
    {"ZeroPointZero", "0.0", {D(0.0)}},
    {"FractionNegExpSuffix", ".5e-3D", {D(5e-4)}},
    {"RepeatedSeparators", "1__0.0__5e1__0", {D(10.05e10)}},
    {"DoubleMax", "1.7976931348623157e308", {D(DBL_MAX)}},
    {"Subnormal", "4.9e-324", {D(4.9e-324)}},
    {"SuffixThenWildcard", "1.5D_", {D(1.5), T(TOKEN_WILDCARD, "_")}},
};

const AcceptedCase kStringCases[] = {
    {"EmptyThenIdentifier", "\"\"x", {T(TOKEN_STRING, ""), Id("x")}},
    {"AdjacentStrings",
     "\"a\"\"b\"",
     {T(TOKEN_STRING, "a"), T(TOKEN_STRING, "b")}},
    {"EscapedBackslashCloses", "\"\\\\\"", {T(TOKEN_STRING, "\\")}},
    {"EscapedQuoteInside", "\"\\\"\"", {T(TOKEN_STRING, "\"")}},
    {"ControlCharsAndDel",
     "\"\x01\x1f\x7f\"",
     {T(TOKEN_STRING, "\x01\x1f\x7f")}},
    {"CommentMarkersInString",
     "\"/* x */ // y\"",
     {T(TOKEN_STRING, "/* x */ // y")}},
    {"CommentMarkersInMultiline",
     "\"\"\"//x\n/*\"\"\"",
     {T(TOKEN_STRING, "//x\n/*")}},
    {"InterpolatedString",
     "s\"$a${n + 1} $$0\"",
     {Id("s"), T(TOKEN_STRING, "$a${n + 1} $$0")}},
    {"MultilineBackslashBeforeEnd",
     "\"\"\"a\\\"\"\"",
     {T(TOKEN_STRING, "a\\")}},
    {"MultilineTabAtLineStart",
     "\"\"\"\n\tx\"\"\"",
     {T(TOKEN_STRING, "\n\tx")}},
};

const AcceptedCase kWordCases[] = {
    {"IdentifierShapes", "a_ A1_b2 ABC", {Id("a_"), Id("A1_b2"), Id("ABC")}},
    {"NonKeywordWords",
     "main true false to until by ifj26 s",
     {Id("main"), Id("true"), Id("false"), Id("to"), Id("until"), Id("by"),
      Id("ifj26"), Id("s")}},
    {"Prolog",
     "import ifj26.*",
     {T(TOKEN_KW_IMPORT, "import"), Id("ifj26"), T(TOKEN_DOT, "."),
      T(TOKEN_MULTIPLY, "*")}},
    {"UnionTypeAlias",
     "type Scalar = Int | Double | String",
     {T(TOKEN_KW_TYPE), Id("Scalar"), T(TOKEN_ASSIGN), T(TOKEN_KW_INT),
      T(TOKEN_UNION), T(TOKEN_KW_DOUBLE), T(TOKEN_UNION), T(TOKEN_KW_STRING)}},
    {"MatchDefaultCase",
     "case _ => 0",
     {T(TOKEN_KW_CASE), T(TOKEN_WILDCARD, "_"), T(TOKEN_ARROW, "=>"), I(0)}},
    {"ForGenerator",
     "for i <- 1 to 10 by 2 do",
     {T(TOKEN_KW_FOR), Id("i"), T(TOKEN_FOR_GENERATOR), I(1), Id("to"), I(10),
      Id("by"), I(2), T(TOKEN_KW_DO)}},
    {"BooleanOperators",
     "!a && b || c",
     {T(TOKEN_NOT), Id("a"), T(TOKEN_AND), Id("b"), T(TOKEN_OR), Id("c")}},
    {"ListType",
     "List[Int]()",
     {T(TOKEN_KW_LIST), T(TOKEN_LEFT_BRACKET), T(TOKEN_KW_INT),
      T(TOKEN_RIGHT_BRACKET), T(TOKEN_LEFT_PAREN), T(TOKEN_RIGHT_PAREN)}},
    {"MainAnnotationForms",
     "@main def\n@main(",
     {T(TOKEN_MAIN_ANNOTATION, "@main"), T(TOKEN_KW_DEF), kNl,
      T(TOKEN_MAIN_ANNOTATION, "@main"), T(TOKEN_LEFT_PAREN)}},
};

const AcceptedCase kMunchCases[] = {
    {"EqualThenGreater", "==>", {T(TOKEN_EQUAL, "=="), T(TOKEN_GREATER, ">")}},
    {"ArrowThenAssign", "=>=", {T(TOKEN_ARROW, "=>"), T(TOKEN_ASSIGN, "=")}},
    {"LessEqualThenMinus", "<=-", {T(TOKEN_LESS_EQUAL), T(TOKEN_MINUS)}},
    {"GeneratorThenAssign", "<-=", {T(TOKEN_FOR_GENERATOR), T(TOKEN_ASSIGN)}},
    {"GreaterEqualThenAssign",
     ">==",
     {T(TOKEN_GREATER_EQUAL), T(TOKEN_ASSIGN)}},
    {"AssignThenLess", "=<", {T(TOKEN_ASSIGN), T(TOKEN_LESS)}},
    {"AndThenAssign", "&&=", {T(TOKEN_AND, "&&"), T(TOKEN_ASSIGN)}},
    {"UnionThenGreater", "|>", {T(TOKEN_UNION, "|"), T(TOKEN_GREATER)}},
    {"DoubleNot", "!!", {T(TOKEN_NOT, "!"), T(TOKEN_NOT, "!")}},
    {"DoubleStar", "**", {T(TOKEN_MULTIPLY), T(TOKEN_MULTIPLY)}},
    {"ShiftLikePairs",
     "<<>>",
     {T(TOKEN_LESS), T(TOKEN_LESS), T(TOKEN_GREATER), T(TOKEN_GREATER)}},
    {"LessGreater", "<>", {T(TOKEN_LESS), T(TOKEN_GREATER)}},
    {"ColonsThenAssign",
     "::=",
     {T(TOKEN_COLON), T(TOKEN_COLON), T(TOKEN_ASSIGN)}},
    {"Delimiters",
     ".(,,;;)(][",
     {T(TOKEN_DOT), T(TOKEN_LEFT_PAREN), T(TOKEN_COMMA), T(TOKEN_COMMA),
      T(TOKEN_SEMICOLON), T(TOKEN_SEMICOLON), T(TOKEN_RIGHT_PAREN),
      T(TOKEN_LEFT_PAREN), T(TOKEN_RIGHT_BRACKET), T(TOKEN_LEFT_BRACKET)}},
    {"StrayCommentCloser", "*/", {T(TOKEN_MULTIPLY), T(TOKEN_DIVIDE, "/")}},
    {"Division", "a / b", {Id("a"), T(TOKEN_DIVIDE, "/"), Id("b")}},
    {"SpacedSlashes", "/ /", {T(TOKEN_DIVIDE), T(TOKEN_DIVIDE)}},
};

std::string case_name(const ::testing::TestParamInfo<AcceptedCase> &info) {
  return info.param.name;
}

} // namespace

TEST_P(LexerEdgeAcceptedTest, ProducesExpectedTokens) {
  const AcceptedCase &test_case = GetParam();
  expect_tokens(lex_all(test_case.input), test_case.tokens);
}

INSTANTIATE_TEST_SUITE_P(Layout, LexerEdgeAcceptedTest,
                         ::testing::ValuesIn(kLayoutCases), case_name);
INSTANTIATE_TEST_SUITE_P(Numbers, LexerEdgeAcceptedTest,
                         ::testing::ValuesIn(kNumberCases), case_name);
INSTANTIATE_TEST_SUITE_P(Strings, LexerEdgeAcceptedTest,
                         ::testing::ValuesIn(kStringCases), case_name);
INSTANTIATE_TEST_SUITE_P(Words, LexerEdgeAcceptedTest,
                         ::testing::ValuesIn(kWordCases), case_name);
INSTANTIATE_TEST_SUITE_P(MaximalMunch, LexerEdgeAcceptedTest,
                         ::testing::ValuesIn(kMunchCases), case_name);

/*
 * AI-generated test block — inputs rejected with LEXER_INVALID_TOKEN.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Spec 1.1 lexical error 1 cases not yet in lexer tests: unknown chars
 *   (backtick, NUL, control, DEL), tab at line start, Unicode identifier,
 *   unterminated (multiline) strings at every quote count, every unsupported
 *   escape, incomplete exponents/separators, and broken & / @main.
 */
namespace {

struct RejectedCase {
  const char *name;
  std::string input;
  size_t tokens_before_error;
};

class LexerEdgeRejectedTest : public ::testing::TestWithParam<RejectedCase> {};

const RejectedCase kUnknownCharCases[] = {
    {"Backtick", "`val`", 0U},
    {"ControlChar", "\x01", 0U},
    {"Escape", "\x1b", 0U},
    {"Del", "\x7f", 0U},
    {"TabAtFileStart", "\tx", 0U},
    {"TabAtLineStart", "a\n\tb", 1U},
    {"UnicodeIdentifier", "caf\xC3\xA9", 1U},
    {"SingleAmpersands", "& &", 0U},
    {"SpaceAfterAt", "@ main", 0U},
    {"NewlineAfterAt", "@\nmain", 0U},
};

const RejectedCase kBadStringCases[] = {
    {"LoneQuote", "\"", 0U},
    {"EscapedQuoteThenEof", "\"\\\"", 0U},
    {"CrThenLfInString", "\"a\r\nb\"", 0U},
    {"MultilineOpenerOnly", "\"\"\"", 0U},
    {"FiveQuotes", "\"\"\"\"\"", 0U},
    {"MultilineOneCloser", "\"\"\"a\"", 0U},
    {"MultilineTwoClosers", "\"\"\"a\"\"", 0U},
    {"EscapeU", "\"\\u0041\"", 0U},
    {"EscapeUBraces", "\"\\u{41}\"", 0U},
    {"EscapeApostrophe", "\"\\'\"", 0U},
    {"EscapeB", "\"\\b\"", 0U},
    {"EscapeF", "\"\\f\"", 0U},
    {"EscapeA", "\"\\a\"", 0U},
    {"EscapeE", "\"\\e\"", 0U},
    {"EscapeV", "\"\\v\"", 0U},
    {"EscapeUpperN", "\"\\N\"", 0U},
    {"EscapeUpperT", "\"\\T\"", 0U},
    {"EscapeUpperR", "\"\\R\"", 0U},
    {"EscapeSlash", "\"\\/\"", 0U},
    {"EscapeSpace", "\"\\ \"", 0U},
    {"EscapeTab", "\"\\\t\"", 0U},
    {"EscapeDollar", "\"\\$\"", 0U},
};

const RejectedCase kBadNumberCases[] = {
    {"FractionExponentNoDigit", ".5e", 0U},
    {"FractionExponentSignOnly", ".5e-", 0U},
    {"ZeroExponentNoDigit", "0e", 0U},
    {"SeparatorAfterSign", "1e+_5", 0U},
    {"SeparatorBeforeExpDigit", "1e_5", 0U},
    {"SeparatorBeforeExponent", "1_e5", 0U},
    {"SeparatorBeforeDot", "1_.5", 0U},
    {"SeparatorBeforeSuffix", "1.5_d", 0U},
    {"ExpSeparatorBeforeSuffix", "1.5e5_d", 0U},
    {"DoubleTrailingSeparator", "1__", 0U},
};

std::string rejected_name(const ::testing::TestParamInfo<RejectedCase> &info) {
  return info.param.name;
}

} // namespace

/*
 * The lexer reads a null-terminated C string (lexer_init(Lexer *, char *)),
 * so a NUL byte is never a character of the input: it ends the input. A
 * NUL in the source file therefore cuts the program short instead of being
 * an invalid token.
 */
TEST(LexerEdgeNulTest, NulEndsTheInput) {
  LexResult result = lex_all("\0"s);
  EXPECT_EQ(result.status, LEXER_EOF);
  EXPECT_TRUE(result.tokens.empty());

  result = lex_all("a\0b"s);
  EXPECT_EQ(result.status, LEXER_EOF);
  ASSERT_EQ(result.tokens.size(), 1U);
  EXPECT_EQ(result.tokens[0].text, "a");
}

TEST_P(LexerEdgeRejectedTest, EndsInInvalidToken) {
  const RejectedCase &test_case = GetParam();
  const LexResult result = lex_all(test_case.input);
  EXPECT_EQ(result.status, LEXER_INVALID_TOKEN);
  EXPECT_EQ(result.tokens.size(), test_case.tokens_before_error);
  EXPECT_EQ(result.last.type, TOKEN_INVALID);
}

INSTANTIATE_TEST_SUITE_P(UnknownCharacters, LexerEdgeRejectedTest,
                         ::testing::ValuesIn(kUnknownCharCases), rejected_name);
INSTANTIATE_TEST_SUITE_P(Strings, LexerEdgeRejectedTest,
                         ::testing::ValuesIn(kBadStringCases), rejected_name);
INSTANTIATE_TEST_SUITE_P(Numbers, LexerEdgeRejectedTest,
                         ::testing::ValuesIn(kBadNumberCases), rejected_name);

/*
 * AI-generated test block — keyword boundaries and case sensitivity.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Using the 22 keywords listed in spec 1.1 (not keyword.c), check that
 *   every prefix, suffix, underscore/digit variant and case variant of a
 *   keyword is an ordinary identifier, and that keywords end at any
 *   non-identifier character.
 */
namespace {

const char *const kSpecKeywords[] = {
    "def", "do",     "Double",  "else", "if",    "import",
    "Int", "return", "String",  "then", "Unit",  "val",
    "var", "while",  "Boolean", "case", "class", "extension",
    "for", "List",   "match",   "type",
};

bool is_spec_keyword(const std::string &word) {
  for (const char *keyword : kSpecKeywords) {
    if (word == keyword) {
      return true;
    }
  }
  return false;
}

std::string flip_first_case(std::string word) {
  const char first = word[0];
  word[0] = (first >= 'a' && first <= 'z') ? static_cast<char>(first - 32)
                                           : static_cast<char>(first + 32);
  return word;
}

std::string to_upper(std::string word) {
  for (char &character : word) {
    if (character >= 'a' && character <= 'z') {
      character = static_cast<char>(character - 32);
    }
  }
  return word;
}

std::vector<std::string> near_misses(const std::string &keyword) {
  return {keyword.substr(0U, keyword.size() - 1U),
          keyword + "x",
          keyword + "_",
          keyword + "1",
          "_" + keyword,
          flip_first_case(keyword),
          to_upper(keyword)};
}

} // namespace

TEST_F(LexerEdgeTest, KeywordNearMissesAreIdentifiers) {
  for (const char *keyword : kSpecKeywords) {
    for (const std::string &word : near_misses(keyword)) {
      if (is_spec_keyword(word)) {
        continue;
      }
      SCOPED_TRACE(word);
      expect_tokens(lex_all(word), {Id(word.c_str())});
    }
  }
}

TEST_F(LexerEdgeTest, KeywordsEndAtNonIdentifierCharacter) {
  expect_tokens(lex_all("if(x)then\"s\"val=1\nInt)do:"),
                {T(TOKEN_KW_IF, "if"), T(TOKEN_LEFT_PAREN), Id("x"),
                 T(TOKEN_RIGHT_PAREN), T(TOKEN_KW_THEN, "then"),
                 T(TOKEN_STRING, "s"), T(TOKEN_KW_VAL, "val"), T(TOKEN_ASSIGN),
                 I(1), kNl, T(TOKEN_KW_INT, "Int"), T(TOKEN_RIGHT_PAREN),
                 T(TOKEN_KW_DO, "do"), T(TOKEN_COLON)});
}

/*
 * AI-generated test block — indentation edge cases.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Spec 1.2 layout: dedent of several levels at once, very deep indentation,
 *   the parser-driven lexer_change_current_indent base for the next newline,
 *   and multiline strings that must not emit NEWLINE/INDENT or change the
 *   current indentation.
 */
TEST_F(LexerEdgeTest, NestedBlocksDedentAllLevelsAtOnce) {
  const Expected dedent = T(TOKEN_DEDENT);
  const LexResult result = lex_all("a\n  b\n    c\nd");
  expect_tokens(result,
                {Id("a"), kNl, kIndent, kIndent, Id("b"), kNl, kIndent, kIndent,
                 Id("c"), kNl, dedent, dedent, dedent, dedent, Id("d")});
  EXPECT_EQ(result.final_indent, 0U);
}

TEST_F(LexerEdgeTest, DeepIndentationIsCountedExactly) {
  const size_t depth = 1000U;
  const LexResult result = lex_all("a\n" + std::string(depth, ' ') + "b\nc");
  ASSERT_EQ(result.status, LEXER_EOF);
  ASSERT_EQ(result.tokens.size(), 2U * depth + 5U);
  for (size_t index = 2U; index < depth + 2U; index += 1U) {
    ASSERT_EQ(result.tokens[index].token.type, TOKEN_INDENT) << index;
  }
  EXPECT_EQ(result.tokens[depth + 2U].text, "b");
  EXPECT_EQ(count_type(result, TOKEN_DEDENT), depth);
  EXPECT_EQ(result.tokens.back().text, "c");
  EXPECT_EQ(result.final_indent, 0U);
}

TEST_F(LexerEdgeTest, IndentAdjustmentChangesBaseForNextNewline) {
  // parser.c resets the level after a parenthesised line break this way.
  open("a\n    b\nc");
  next(TOKEN_IDENTIFIER);
  next(TOKEN_NEWLINE);
  for (int index = 0; index < 4; index += 1) {
    next(TOKEN_INDENT);
  }
  ASSERT_EQ(lexer_change_current_indent(&lexer, -4), LEXER_OK);
  next(TOKEN_IDENTIFIER);
  next(TOKEN_NEWLINE);
  next(TOKEN_IDENTIFIER); // no DEDENT: the base is already 0
  EXPECT_STREQ(lexer.token.value.string_value, "c");
  EXPECT_EQ(lexer_pls(&lexer), LEXER_EOF);
}

TEST_F(LexerEdgeTest, MultilineStringDoesNotChangeIndentation) {
  const LexResult result = lex_all("a\n  x = \"\"\"p\n        q\"\"\"\n  y");
  expect_tokens(result, {Id("a"), kNl, kIndent, kIndent, Id("x"),
                         T(TOKEN_ASSIGN), T(TOKEN_STRING, "p\n        q"),
                         T(TOKEN_NEWLINE, "\n  "), Id("y")});
  EXPECT_EQ(result.final_indent, 2U);
}

/*
 * AI-generated test block — token positions after tricky constructs.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   1-based line/column of tokens after blank lines, tabs, escapes, unary
 *   minus and a multiline string that contains an empty line; EOF after LF.
 */
namespace {

/** Index value that selects the final lexer token (EOF/INVALID). */
const size_t kLastToken = SIZE_MAX;

struct PositionCase {
  const char *name;
  std::string input;
  size_t index;
  size_t line;
  size_t column;
};

class LexerEdgePositionTest : public ::testing::TestWithParam<PositionCase> {};

const PositionCase kPositionCases[] = {
    {"NewlineAtFirstBreak", "a\n\n\n  b", 1U, 1U, 2U},
    {"TokenAfterBlankLines", "a\n\n\n  b", 4U, 4U, 3U},
    {"TabIsOneColumn", "a\tb", 1U, 1U, 3U},
    {"SpacesInsideLine", "a     b", 1U, 1U, 7U},
    {"AfterEscapedString", "\"\\n\" x", 1U, 1U, 6U},
    {"IntAfterUnaryMinus", "-5", 1U, 1U, 2U},
    {"MultilineStartsAtOpener", "\"\"\"a\n\n  b\"\"\"\nc", 0U, 1U, 1U},
    {"NewlineAfterMultiline", "\"\"\"a\n\n  b\"\"\"\nc", 1U, 3U, 7U},
    {"TokenAfterMultiline", "\"\"\"a\n\n  b\"\"\"\nc", 2U, 4U, 1U},
    {"EofAfterTrailingNewline", "x\n", kLastToken, 2U, 1U},
};

std::string position_name(const ::testing::TestParamInfo<PositionCase> &info) {
  return info.param.name;
}

} // namespace

TEST_P(LexerEdgePositionTest, ReportsOneBasedPosition) {
  const PositionCase &test_case = GetParam();
  const LexResult result = lex_all(test_case.input);
  ASSERT_EQ(result.status, LEXER_EOF);
  Token token = result.last;
  if (test_case.index != kLastToken) {
    ASSERT_LT(test_case.index, result.tokens.size());
    token = result.tokens[test_case.index].token;
  }
  EXPECT_EQ(token.line, test_case.line);
  EXPECT_EQ(token.column, test_case.column);
}

INSTANTIATE_TEST_SUITE_P(Positions, LexerEdgePositionTest,
                         ::testing::ValuesIn(kPositionCases), position_name);

/*
 * AI-generated test block — API lifecycle edge cases.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   lexer.h contract: repeated lexer_pls after EOF is stable, token_destroy
 *   is idempotent and leaves numeric tokens alone, lexer_destroy resets the
 *   state without touching the borrowed buffer, and lexing starts at the
 *   given buffer position with 1:1.
 */
TEST_F(LexerEdgeTest, RepeatedEofKeepsTypeAndPosition) {
  open("x\n");
  next(TOKEN_IDENTIFIER);
  next(TOKEN_NEWLINE);
  for (int call = 0; call < 5; call += 1) {
    SCOPED_TRACE(call);
    EXPECT_EQ(lexer_pls(&lexer), LEXER_EOF);
    EXPECT_EQ(lexer.token.type, TOKEN_EOF);
    EXPECT_EQ(lexer.token.line, 2U);
    EXPECT_EQ(lexer.token.column, 1U);
  }
}

TEST_F(LexerEdgeTest, TokenDestroyIsIdempotentAndKeepsNumbers) {
  open("abc 42");
  next(TOKEN_IDENTIFIER);
  token_destroy(&lexer.token);
  EXPECT_EQ(lexer.token.value.string_value, nullptr);
  token_destroy(&lexer.token);
  EXPECT_EQ(lexer.token.value.string_value, nullptr);
  next(TOKEN_INT);
  token_destroy(&lexer.token);
  EXPECT_EQ(lexer.token.value.int_value, 42);
  EXPECT_EQ(lexer.token.type, TOKEN_INT);
}

TEST_F(LexerEdgeTest, LexerDestroyKeepsBufferAndResetsState) {
  open("ab c"); // lookahead is still `c` when the lexer is destroyed
  next(TOKEN_IDENTIFIER);
  lexer_destroy(&lexer);
  EXPECT_EQ(lexer.input_str, nullptr);
  EXPECT_EQ(lexer.lookahead, EOF);
  EXPECT_EQ(lexer.line, 0U);
  EXPECT_EQ(lexer.column, 0U);
  EXPECT_EQ(lexer.token.value.string_value, nullptr);
  EXPECT_STREQ(buffer.data(), "ab c"); // the borrowed buffer is untouched
}

TEST_F(LexerEdgeTest, LexerStartsAtGivenBufferPosition) {
  buffer = buffer_from("skip x");
  ASSERT_EQ(lexer_init(&lexer, buffer.data() + 5), LEXER_OK);
  next(TOKEN_IDENTIFIER);
  EXPECT_STREQ(lexer.token.value.string_value, "x");
  EXPECT_EQ(lexer.token.line, 1U);
  EXPECT_EQ(lexer.token.column, 1U);
  EXPECT_EQ(lexer_pls(&lexer), LEXER_EOF);
}

/*
 * AI-generated test block — known gaps: comments and VALMEM member access.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   The lexer does not implement comments at all (lexer_bug_test.cc already
 *   reports //, nested and unterminated comments). Describe the remaining
 *   spec 1.1/1.2 comment rules and VALMEM `5.toString` as DISABLED_ tests
 *   that can be enabled once the features are implemented.
 */
// Spec 1.1: a block comment without LF "se chová jako jeden bílý znak".
TEST(LexerEdgeKnownGapTest, DISABLED_SingleLineBlockCommentSeparatesTokens) {
  expect_tokens(lex_all("va/**/l"), {Id("va"), Id("l")});
}

// Spec 1.1: a block comment with LF "se chová jako jeden konec řádku".
TEST(LexerEdgeKnownGapTest, DISABLED_MultilineBlockCommentActsAsLineEnd) {
  const LexResult result = lex_all("a /* x\ny */\nb");
  EXPECT_EQ(result.status, LEXER_EOF);
  EXPECT_EQ(types_without_newlines(result),
            (std::vector<int>{TOKEN_IDENTIFIER, TOKEN_IDENTIFIER}));
  EXPECT_GE(count_type(result, TOKEN_NEWLINE), 1U);
}

// Spec 1.1: after a multiline block comment "nesmí být další tokeny";
// spec 6: code 1 covers "chybná struktura ... komentáře".
TEST(LexerEdgeKnownGapTest, DISABLED_TokenAfterMultilineBlockCommentIsError) {
  EXPECT_EQ(lex_all("a /* x\ny */ b").status, LEXER_INVALID_TOKEN);
}

// Spec 1.2: lines holding only comments are ignored (no INDENT/DEDENT).
TEST(LexerEdgeKnownGapTest, DISABLED_CommentOnlyLineKeepsIndentation) {
  const LexResult result = lex_all("a\n  b\n// c\n  d");
  EXPECT_EQ(result.status, LEXER_EOF);
  EXPECT_EQ(types_without_newlines(result),
            (std::vector<int>{TOKEN_IDENTIFIER, TOKEN_INDENT, TOKEN_INDENT,
                              TOKEN_IDENTIFIER, TOKEN_IDENTIFIER}));
}

TEST(LexerEdgeKnownGapTest, DISABLED_LineCommentAtEofWithoutNewline) {
  const LexResult result = lex_all("a // c");
  EXPECT_EQ(result.status, LEXER_EOF);
  EXPECT_EQ(types_without_newlines(result),
            (std::vector<int>{TOKEN_IDENTIFIER}));
}

TEST(LexerEdgeKnownGapTest, DISABLED_BlockOpenerInsideLineCommentIsIgnored) {
  const LexResult result = lex_all("// /*\nx");
  EXPECT_EQ(result.status, LEXER_EOF);
  EXPECT_EQ(types_without_newlines(result),
            (std::vector<int>{TOKEN_IDENTIFIER}));
}

TEST(LexerEdgeKnownGapTest, DISABLED_LineMarkerInsideBlockCommentIsIgnored) {
  expect_tokens(lex_all("a /* // */ b"), {Id("a"), Id("b")});
}

// Spec 2: "Před prologem mohou být pouze bílé znaky a komentáře."
TEST(LexerEdgeKnownGapTest, DISABLED_CommentBeforePrologIsIgnored) {
  expect_tokens(
      lex_all("/* c */import ifj26.*"),
      {T(TOKEN_KW_IMPORT), Id("ifj26"), T(TOKEN_DOT), T(TOKEN_MULTIPLY)});
}

// Spec 5.4 VALMEM: "libovolná hodnota toString"; spec 1.1: `5.` is not a
// Double literal, so `5.toString` is INT DOT IDENTIFIER (as in Scala 3).
// Today NUM_DOT is not final and the lexer reports LEXER_INVALID_TOKEN.
TEST(LexerEdgeKnownGapTest, DISABLED_IntLiteralAllowsMemberAccess) {
  expect_tokens(lex_all("5.toString"),
                {I(5), T(TOKEN_DOT, "."), Id("toString")});
}
