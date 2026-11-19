/*
 * AI-generated test block — shared black-box fixtures for lexical spec tests.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write spec conformance tests for IFJ26 chapter 1.1/1.2 (lexical units,
 *   indentation, logical lines, commas and semicolons) and the lexical part
 *   of chapter 2 (prolog, @main). Compile whole programs through
 *   tests/spec_harness.hpp and compare the exit code with spec chapter 6.
 *   Group the cases into TEST_P tables by topic; every row names its rule.
 *   Tests that fail today because the compiler breaks the spec stay enabled.
 */
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>

#include "spec_harness.hpp"
#include "gtest/gtest.h"

/*
 * Root causes named in "Fails today" comments (src/ module: defect):
 *   RC-PROLOG   parser.c p_program: prolog `import ifj26.*` is not parsed
 *               (`import` -> 2) and its absence is not checked.
 *   RC-EXPR     parser.c p_expression is a stub: `val x = e` -> 99.
 *   RC-CRASH    parser.c p_statement/p_code_block: a statement starting
 *               with a term (call) yields NULL and crashes (harness -1).
 *   RC-ONELINE  parser.c p_function: one-line function body is not parsed.
 *   RC-COMMENT  lexer_dfa.c/lexer.c: `//` and nested comments are not
 *               lexed, `/` and `*` come out as operators.
 *   RC-LEADWS   lexer.c STATE_S: whitespace at the start of file -> 1.
 *   RC-WSEOF    lexer.c update_indent_delta: space-only last line -> INDENT.
 *   RC-SEMI     parser.c p_return_statement/p_code_block: `;` after a bare
 *               `return` is taken as an expression start.
 *   RC-DBLRANGE lexer.c finish_token: strtod without ERANGE/inf check.
 *   RC-LEADZERO lexer_dfa.c STATE_INT0: no digit after `0`, so `007` and
 *               `00.5` split into several tokens.
 */

namespace {

/** One black-box program and the exit code spec chapter 6 assigns to it. */
struct SpecCase {
  const char *name;
  const char *source;
  int expected;
};

/** Prints only the case name so gtest does not dump the raw bytes. */
void PrintTo(const SpecCase &spec_case, std::ostream *out) {
  *out << spec_case.name;
}

std::string spec_case_name(const ::testing::TestParamInfo<SpecCase> &info) {
  return info.param.name;
}

/** Program bodies: the harness prepends the prolog `import ifj26.*`. */
class SpecLexicalTest : public ::testing::TestWithParam<SpecCase> {};

/** Whole source files compiled verbatim (no prolog is added). */
class SpecLexicalRawTest : public ::testing::TestWithParam<SpecCase> {};

} // namespace

// Chapter 6: each row gets the exit code of the rule named in its comment.
TEST_P(SpecLexicalTest, ProgramGetsSpecExitCode) {
  // Arrange
  const SpecCase &spec_case = GetParam();

  // Act
  int code = ifj26_exit_code(spec_case.source);

  // Assert
  EXPECT_EQ(code, spec_case.expected) << spec_case.source;
}

// Chapter 6: each raw file gets the exit code of the rule in its comment.
TEST_P(SpecLexicalRawTest, SourceFileGetsSpecExitCode) {
  // Arrange
  const SpecCase &spec_case = GetParam();

  // Act
  int code = ifj26_exit_code_raw(spec_case.source);

  // Assert
  EXPECT_EQ(code, spec_case.expected) << spec_case.source;
}

/*
 * AI-generated test block — identifiers, keywords and unknown characters.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for spec 1 and 1.1: characters that are not part of any
 *   lexeme give error 1, case variants and Scala-only keywords are plain
 *   identifiers, `_`-identifiers are valid, and `@main` must be written
 *   exactly. Keywords or a lone `_` as a name give 2; each such case has a
 *   positive twin with a valid name so it cannot pass by accident.
 */
namespace {

const SpecCase kIdentifierCases[] = {
    // A1-001 §1.1: `$` is neither an identifier character nor a lexeme -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"DollarInIdentifierIsLexicalError", R"IFJ(@main def main(): Unit =
  val x$y = 1
  return
)IFJ",
     1},
    // A1-002 §1.1: identifiers in backticks are not supported -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"BacktickIdentifierIsLexicalError", R"IFJ(@main def main(): Unit =
  val `x` = 1
  return
)IFJ",
     1},
    // A1-003 §1.1: operator identifiers are not supported and "Neznámý
    // znak ... vedou na lexikální chybu 1"; without `#` the program is valid.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"UnknownCharacterHashIsLexicalError", R"IFJ(@main def main(): Unit =
  val x#y = 1
  return
)IFJ",
     1},
    // A1-003 §1.1: unknown character `?` -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"UnknownCharacterQuestionMarkIsLexicalError",
     R"IFJ(@main def main(): Unit =
  val x?y = 1
  return
)IFJ",
     1},
    // A1-003 §1.1: unknown character `~` -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"UnknownCharacterTildeIsLexicalError", R"IFJ(@main def main(): Unit =
  val x~y = 1
  return
)IFJ",
     1},
    // A1-003 §1.1: unknown character `^` -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"UnknownCharacterCaretIsLexicalError", R"IFJ(@main def main(): Unit =
  val x^y = 1
  return
)IFJ",
     1},
    // A1-003 §1.1: unknown character `\` -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"UnknownCharacterBackslashIsLexicalError", R"IFJ(@main def main(): Unit =
  val x\y = 1
  return
)IFJ",
     1},
    // A1-003 §1.1: `%` is not an IFJ26 operator (chapter 3) -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"UnknownCharacterPercentIsLexicalError", R"IFJ(@main def main(): Unit =
  val x%y = 1
  return
)IFJ",
     1},
    // A1-003 §1.1: a lone `&` is no lexeme (`&&` is BOOLTHEN only) -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"UnknownCharacterAmpersandIsLexicalError", R"IFJ(@main def main(): Unit =
  val x&y = 1
  return
)IFJ",
     1},
    // A1-020 §2: no character may stand between `@` and `main` -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"SpaceBetweenAtAndMainIsLexicalError", R"IFJ(@ main def main(): Unit =
  return
)IFJ",
     1},
    // A1-021 §1 + §2: case-sensitive, `@Main` is not `@main` -> 1. §2: "@ se
    // v jiném kontextu nepoužívá", so `@` alone is an unknown character.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"CapitalizedMainAnnotationIsLexicalError", R"IFJ(@Main def main(): Unit =
  return
)IFJ",
     1},
    // A1-030 §1: case-sensitive, `If`, `iNT`, `sTRING`, `unit` are names.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"KeywordCaseVariantsAreIdentifiers", R"IFJ(def If(): Unit =
  return

def iNT(Int_: Int, sTRING: String, unit: Double): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     0},
    // A1-031 §1 + §1.1: Scala-only keywords are IFJ26 identifiers.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"ScalaOnlyKeywordsAreIfj26Identifiers",
     R"IFJ(def object(new: Int, lazy: String): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     0},
    // A1-032 §1.1: an identifier starts with a letter or `_`.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"UnderscoreIdentifiersAreAccepted",
     R"IFJ(def f(_a: Int, __: Int, _1: Int, a_: Int, A1_b: Int): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     0},
};

INSTANTIATE_TEST_SUITE_P(Identifiers, SpecLexicalTest,
                         ::testing::ValuesIn(kIdentifierCases), spec_case_name);

const SpecCase kKeywordNameCases[] = {
    // A1-200 §1.1e: keywords cannot be identifiers -> 2.
    // Passes today only by accident (RC-PROLOG gives 2); twin A1-200p.
    {"KeywordWhileAsValNameIsSyntaxError", R"IFJ(@main def main(): Unit =
  val while = 1
)IFJ",
     2},
    // A1-200p §1.1e: positive twin of A1-200 with a valid name.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"ValidValNameTwinIsAccepted", R"IFJ(@main def main(): Unit =
  val w = 1
)IFJ",
     0},
    // A1-201 §1.1e: keyword `if` as a function name -> 2.
    // Passes today only by accident (RC-PROLOG gives 2); twin A1-201p.
    {"KeywordIfAsFunctionNameIsSyntaxError", R"IFJ(def if(): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     2},
    // A1-201p §1.1e: positive twin of A1-201 (`iff`).
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"ValidFunctionNameTwinIsAccepted", R"IFJ(def iff(): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     0},
    // A1-202 §1.1e: keyword `then` as a parameter name -> 2.
    // Passes today only by accident (RC-PROLOG gives 2); twin A1-202p.
    {"KeywordThenAsParameterNameIsSyntaxError", R"IFJ(def f(then: Int): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     2},
    // A1-202p §1.1e: positive twin of A1-202 (`thenx`).
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"ValidParameterNameTwinIsAccepted", R"IFJ(def f(thenx: Int): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     0},
    // A1-203 §1.1e: keyword `do` as a var name -> 2.
    // Passes today only by accident (RC-PROLOG gives 2); twin A1-203p.
    {"KeywordDoAsVarNameIsSyntaxError", R"IFJ(@main def main(): Unit =
  var do = 1
)IFJ",
     2},
    // A1-203p §1.1e: positive twin of A1-203 (`do1`).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"ValidVarNameTwinIsAccepted", R"IFJ(@main def main(): Unit =
  var do1 = 1
)IFJ",
     0},
    // A1-204 §1.1c: "Samotné podtržítko není identifikátor" - `_` is a valid
    // lexeme (reserved for extensions) in a wrong place, so 2, not 1.
    // Passes today only by accident (RC-PROLOG gives 2); twin A1-204p.
    {"WildcardAsParameterNameIsSyntaxError", R"IFJ(def f(_: Int): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     2},
    // A1-204p §1.1c: positive twin of A1-204 (`a`).
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"ValidParameterTwinOfWildcardIsAccepted", R"IFJ(def f(a: Int): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     0},
    // A1-205 §1.1c: `val _ = 1` is Scala, not IFJ26: "Samotné podtržítko
    // není identifikátor"; the lexeme is valid, so syntax error 2.
    // Passes today only by accident (RC-PROLOG gives 2); twin A1-205p.
    {"WildcardAsValNameIsSyntaxError", R"IFJ(@main def main(): Unit =
  val _ = 1
)IFJ",
     2},
    // A1-205p §1.1c: positive twin of A1-205 (`a`).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"ValidValTwinOfWildcardIsAccepted", R"IFJ(@main def main(): Unit =
  val a = 1
)IFJ",
     0},
    // A1-206 §1.1c: a lone `_` as a function name -> 2 (valid lexeme, not an
    // identifier).
    // Passes today only by accident (RC-PROLOG gives 2); twin A1-206p.
    {"WildcardAsFunctionNameIsSyntaxError", R"IFJ(def _(): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     2},
    // A1-206p §1.1c: positive twin of A1-206 (`f_`).
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"ValidFunctionTwinOfWildcardIsAccepted", R"IFJ(def f_(): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     0},
    // A1-207 §1.1e + §2: the entry function needs a valid identifier -> 2.
    // Passes today only by accident (RC-PROLOG gives 2); twin A1-207p.
    {"KeywordValAsMainNameIsSyntaxError", R"IFJ(@main def val(): Unit =
  return
)IFJ",
     2},
    // A1-207p §2: positive twin of A1-207 (`value`).
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"ValidMainNameTwinIsAccepted", R"IFJ(@main def value(): Unit =
  return
)IFJ",
     0},
    // A1-208 §1.1e: type keywords `Int`, `String` as parameter names -> 2.
    // Passes today only by accident (RC-PROLOG gives 2); twin A1-208p.
    {"TypeKeywordsAsParameterNamesAreSyntaxError",
     R"IFJ(def f(Int: Int, String: String): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     2},
    // A1-208p §1.1e: positive twin of A1-208 (`Int1`, `String1`).
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"ValidTypeLikeParameterNamesAreAccepted",
     R"IFJ(def f(Int1: Int, String1: String): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     0},
};

INSTANTIATE_TEST_SUITE_P(KeywordNames, SpecLexicalTest,
                         ::testing::ValuesIn(kKeywordNameCases),
                         spec_case_name);

} // namespace

/*
 * AI-generated test block — Int and Unit literals.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for spec 1.1 Int literals: `_` only between digits,
 *   no leading zero on a non-zero literal, accepted forms with separators,
 *   negative literals as unary minus, and the Unit literal `()`.
 */
namespace {

const SpecCase kIntCases[] = {
    // A1-013 §1.1: `_` only between two digits -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"TrailingUnderscoreInIntIsLexicalError", R"IFJ(@main def main(): Unit =
  val i = 1_
  return
)IFJ",
     1},
    // A1-017 §1.1: a non-zero literal must not start with 0 -> 1.
    // Fails today: RC-PROLOG (2); without the prolog RC-LEADZERO splits
    // `007` into three Ints and RC-EXPR gives 99.
    {"IntWithLeadingZeroIsLexicalError", R"IFJ(@main def main(): Unit =
  val i = 007
  return
)IFJ",
     1},
    // A1-034 §1.1: `0`, `1_000_000` and 2147483647 are valid Int literals.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"IntLiteralFormsAreAccepted", R"IFJ(@main def main(): Unit =
  val a = 0
  val b = 1_000_000
  val c = 2147483647
)IFJ",
     0},
    // A1-041 §1.1: a negative literal is unary `-` and a positive literal.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"NegativeLiteralsAreUnaryMinus", R"IFJ(@main def main(): Unit =
  val x = -5
  val y = - 5
  val z = -.5
)IFJ",
     0},
    // A1-040 §1.1: the Unit literal is written `()`.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"UnitLiteralIsAccepted", R"IFJ(@main def main(): Unit =
  val u = ()
  val v: Unit = ()
)IFJ",
     0},
};

INSTANTIATE_TEST_SUITE_P(IntAndUnitLiterals, SpecLexicalTest,
                         ::testing::ValuesIn(kIntCases), spec_case_name);

} // namespace

/*
 * AI-generated test block — Double literals.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for spec 1.1 Double literals: an exponent needs a digit,
 *   values outside IEEE 754 binary64 give 1, both grammar forms with
 *   exponent, separators and the d/D suffix are valid, and the integer part
 *   of a Double may have leading zeros (only Int forbids them).
 */
namespace {

const SpecCase kDoubleCases[] = {
    // A1-011 §1.1: the exponent "musí následovat alespoň jedna číslice";
    // `1e` is a malformed lexeme (ch. 6 code 1 "chybná struktura aktuálního
    // lexému"), not `1` followed by the identifier `e`.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"ExponentWithoutDigitsIsLexicalError", R"IFJ(@main def main(): Unit =
  val d = 1e
  return
)IFJ",
     1},
    // A1-016 §1.1: range is IEEE 754 binary64, `1e999` is out of range -> 1.
    // Fails today: RC-PROLOG (2); without the prolog RC-DBLRANGE accepts
    // `inf` and RC-EXPR gives 99.
    {"DoubleOutOfRangeIsLexicalError", R"IFJ(@main def main(): Unit =
  val d = 1e999
  return
)IFJ",
     1},
    // A1-035 §1.1: both Double forms, exponent, `_` and the d/D suffix.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"DoubleLiteralFormsAreAccepted", R"IFJ(@main def main(): Unit =
  val a = .5
  val b = 1.5e3
  val c = 1e5
  val d = 1E-5d
  val e = 1_0.0_1e1_0D
  val f = 0.0
  val g = 7.25d
)IFJ",
     0},
    // A1-106 §1.1: "Celá část a desetinná část jsou sekvence číslic"; the
    // leading-zero ban ("Nenulový literál nesmí začínat nulou") is for Int.
    // Fails today: RC-PROLOG (2); without the prolog RC-LEADZERO splits
    // `00.5` into `0` `0.5` and RC-EXPR gives 99.
    {"DoubleIntegerPartMayHaveLeadingZeros", R"IFJ(@main def main(): Unit =
  val d = 00.5
  val e = 007.5
  val f = 00e1
)IFJ",
     0},
};

INSTANTIATE_TEST_SUITE_P(DoubleLiterals, SpecLexicalTest,
                         ::testing::ValuesIn(kDoubleCases), spec_case_name);

} // namespace

/*
 * AI-generated test block — string literals and escape sequences.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for spec 1.1 string literals: only `"` delimits a
 *   string, only \\ \" \n \r \t are escapes, a real LF or EOF inside a
 *   string gives 1; all five escapes, empty strings, comment markers and a
 *   real tab inside a string are accepted.
 */
namespace {

const SpecCase kStringCases[] = {
    // A1-004 §1.1: no char literal, `'` is an unknown character -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"CharLiteralQuoteIsLexicalError", R"IFJ(@main def main(): Unit =
  val c = 'a'
  return
)IFJ",
     1},
    // A1-005 §1.1: `\q` is not an escape sequence -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"UnknownEscapeIsLexicalError", R"IFJ(@main def main(): Unit =
  val s = "a\qb"
  return
)IFJ",
     1},
    // A1-006 §1.1: no other escapes, `\u0041` is not IFJ26 -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"UnicodeEscapeIsLexicalError", R"IFJ(@main def main(): Unit =
  val s = "\u0041"
  return
)IFJ",
     1},
    // A1-007 §1.1: a string must not contain a real end of line -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"StringWithRealNewlineIsLexicalError", R"IFJ(@main def main(): Unit =
  val s = "abc
  return
)IFJ",
     1},
    // A1-008 §1.1: an unterminated string at end of file -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"StringUnterminatedAtEofIsLexicalError",
     "@main def main(): Unit =\n  return\n  val s = \"abc", 1},
    // A1-036 §1.1: all five escape sequences are accepted.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"AllEscapeSequencesAreAccepted", R"IFJ(@main def main(): Unit =
  val s = "a\\b\"c\nd\re\tf"
)IFJ",
     0},
    // A1-037 §1.1: empty string and empty multiline string.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"EmptyStringLiteralsAreAccepted", R"IFJ(@main def main(): Unit =
  val s = ""
  val t = """"""
)IFJ",
     0},
    // A1-042 §1.1: comment markers inside a string are string content.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"CommentMarkersInsideStringAreContent", R"IFJ(@main def main(): Unit =
  val s = "http://x/*y*/"
)IFJ",
     0},
    // A1-043 §1.1: only a real LF is forbidden, a real tab is allowed.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"RealTabInsideStringIsAccepted",
     "@main def main(): Unit =\n  val s = \"a\tb\"\n", 0},
};

INSTANTIATE_TEST_SUITE_P(StringLiterals, SpecLexicalTest,
                         ::testing::ValuesIn(kStringCases), spec_case_name);

} // namespace

/*
 * AI-generated test block — multiline string literals.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for spec 1.1/1.2 multiline strings: an unterminated one
 *   gives 1; content is verbatim (LF, quotes, `\q`, tabs and zero indent
 *   inside are not layout), the spec example is valid, and a LF inside does
 *   not end the logical line.
 */
namespace {

const SpecCase kMultilineCases[] = {
    // A1-009 §1.1: an unterminated multiline string swallows the file -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"UnterminatedMultilineStringIsLexicalError", R"IFJ(@main def main(): Unit =
  val s = """abc
  return
)IFJ",
     1},
    // A1-038 §1.1 + §1.2: content is verbatim; a tab or zero indent inside
    // is not indentation.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"MultilineStringContentIgnoresLayoutRules",
     "@main def main(): Unit =\n  val s = \"\"\"first\nno indent here\n"
     "\ttab here \\q \"quoted\"\n        deep\"\"\"\n  val t = s\n",
     0},
    // A1-039 §1.1: the multiline example of the spec.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"SpecMultilineExampleIsAccepted", R"IFJ(@main def main(): Unit =
  val x = """A
   hoj s\nvete
    """
)IFJ",
     0},
    // A1-085 §1.2: a LF in a multiline string does not end the logical line;
    // the next statement is compared with its first physical line.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"StatementAfterMultilineString", R"IFJ(@main def main(): Unit =
  val s = """a
b"""
  val t = s
)IFJ",
     0},
};

INSTANTIATE_TEST_SUITE_P(MultilineStrings, SpecLexicalTest,
                         ::testing::ValuesIn(kMultilineCases), spec_case_name);

} // namespace

/*
 * AI-generated test block — line and block comments.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for spec 1.1/1.2 comments: unterminated (also nested)
 *   block comments give 1; a line comment counts as one end of line, a
 *   block comment without LF as one whitespace and with LF as one end of
 *   line; comment-only lines are ignored at any indent; nested comments,
 *   comments next to @main, inside parentheses and at EOF without LF are
 *   valid; a body holding only a comment is an empty block (2).
 */
namespace {

const SpecCase kCommentCases[] = {
    // A1-022 §1.1: an unterminated block comment -> 1.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"UnterminatedBlockCommentIsLexicalError", R"IFJ(@main def main(): Unit =
  return
/* never closed
)IFJ",
     1},
    // A1-023 §1.1: an unterminated nested block comment -> 1.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"UnterminatedNestedBlockCommentIsLexicalError",
     R"IFJ(@main def main(): Unit =
  return /* a /* b */
)IFJ",
     1},
    // A1-050 §1.1 + §1.2: a comment after a block header is the end of line.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"LineCommentAfterFunctionHeader",
     R"IFJ(@main def main(): Unit = // header comment
  return
)IFJ",
     0},
    // A1-051 §1.2: comment-only lines are ignored and keep the indentation.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"CommentOnlyLinesAtAnyIndentAreIgnored", R"IFJ(@main def main(): Unit =
  return
// col 0 comment
      // deep comment
  return
)IFJ",
     0},
    // A1-052 §1.1 + §2: a block comment without LF separates tokens.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"BlockCommentSeparatesMainAndDef", R"IFJ(@main/* c */def main(): Unit =
  return
)IFJ",
     0},
    // A1-053 §1.1: block comments may be nested.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"NestedBlockCommentOnStatementLine", R"IFJ(@main def main(): Unit =
  return /* a /* nested */ still comment */
)IFJ",
     0},
    // A1-054 §1.1: a block comment with LF acts as one end of line.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"MultilineBlockCommentEndsHeaderLine", R"IFJ(@main def main(): Unit = /* a
 b */
  return
)IFJ",
     0},
    // A1-056 §1.1 + §1.2: a line comment also ends at EOF.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"LineCommentAtEndOfFileWithoutNewline",
     "@main def main(): Unit =\n  return // end without LF", 0},
    // A1-057 §1.1: `/*` inside a line comment opens nothing.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"BlockOpenerInsideLineCommentIsIgnored", R"IFJ(@main def main(): Unit =
  // a /* not opened
  return
)IFJ",
     0},
    // A1-058 §1.1 + §1.2: a line comment inside parentheses.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"LineCommentInsideParameterList", R"IFJ(def f(a: Int, // first
      b: Int): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     0},
    // A1-062 §1.2: blocks cannot be empty; the comment line is ignored -> 2.
    // Passes today only by accident (RC-PROLOG, RC-COMMENT); A1-057 is the
    // positive check.
    {"BodyWithOnlyCommentIsSyntaxError", R"IFJ(@main def main(): Unit =
  // only a comment
)IFJ",
     2},
    // A1-068 §1.1 + §1.2: a tab inside a block comment is not indentation.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"TabInsideMultilineBlockCommentIsIgnored",
     "@main def main(): Unit =\n  return /* a\n\tb */\n  return\n", 0},
    // A1-102 §2 + §1.1: `@main` on its own line ending with a block comment.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"MainAnnotationLineEndingWithComment", R"IFJ(@main/* c */
def main(): Unit =
  return
)IFJ",
     0},
    // A1-103 §1.1 + §1.2: a line comment inside a parenthesized expression.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99), then
    // RC-COMMENT.
    {"LineCommentInsideParenthesizedExpression", R"IFJ(@main def main(): Unit =
  val x = (1 + // c
    2)
)IFJ",
     0},
    // A1-104 §1.1: a block comment with LF ends the statement.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99), then
    // RC-COMMENT.
    {"MultilineBlockCommentEndsStatement", R"IFJ(@main def main(): Unit =
  val x = 1 /* c
 */
  val y = x
)IFJ",
     0},
};

INSTANTIATE_TEST_SUITE_P(Comments, SpecLexicalTest,
                         ::testing::ValuesIn(kCommentCases), spec_case_name);

} // namespace

/*
 * AI-generated test block — indentation and logical lines.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for spec 1.2: a tab at the start of a line gives 1,
 *   blank and space-only lines keep the indentation, tabs between tokens are
 *   whitespace, the indent of a logical line is its first physical line,
 *   continuation lines must be deeper, a line cannot start with an operator
 *   and an unfinished expression cannot continue on the next logical line.
 *   Every accidental 2 has a positive twin.
 */
namespace {

const SpecCase kIndentationCases[] = {
    // A1-018 §1.2: a tab at the start of a non-empty line -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"TabIndentationIsLexicalError", "@main def main(): Unit =\n\treturn\n", 1},
    // A1-019 §1.2: only spaces indent ("Pro odsazení se používají pouze
    // mezery"); a tab in the leading whitespace of a non-empty line -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"TabAfterSpacesInIndentationIsLexicalError",
     "@main def main(): Unit =\n  return\n  \treturn\n", 1},
    // A1-027 §1.2: "tabulátor na začátku neprázdného řádku" speaks of every
    // physical line, also one inside parentheses -> 1.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"TabInsideParenthesesContinuationIsLexicalError",
     "def f(a: Int,\n\tb: Int): Unit =\n  return\n\n"
     "@main def main(): Unit =\n  return\n",
     1},
    // A1-070 §1.2: blank and space-only lines do not change the indentation
    // (a tab-only line is left out: whether it is "neprázdný" is unclear).
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"BlankAndSpaceOnlyLinesInsideBlock",
     "@main def main(): Unit =\n  return\n\n      \n  return\n", 0},
    // A1-071 §1.2: a space-only last line does not change the indentation.
    // Fails today: RC-PROLOG (2); without the prolog RC-WSEOF (2).
    {"WhitespaceOnlyLastLine", "@main def main(): Unit =\n  return\n   ", 0},
    // A1-073 §1.2: a tab that is not at the start of a line is whitespace.
    // Fails today: RC-PROLOG (2); without the prolog it passes.
    {"TabsBetweenTokensAreWhitespace",
     "@main\tdef\tmain(\t)\t:\tUnit\t=\t\n  return\t\n", 0},
    // A1-081 §1.2: indent of a logical line = its first physical line.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"ContinuationIndentDoesNotChangeBlockIndent",
     R"IFJ(@main def main(): Unit =
  val x = (1 +
      2)
  val y = x
)IFJ",
     0},
    // A1-084 §1.2: a continuation one space deeper is enough.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"ContinuationOneSpaceDeeperIsAccepted", R"IFJ(@main def main(): Unit =
  val x = (1 +
   2)
)IFJ",
     0},
    // A1-107 §1.2: statements of a block have the same indent; the next one
    // is compared with the first physical line -> 2.
    // Passes today only by accident (RC-PROLOG gives 2); without the prolog
    // it fails with RC-EXPR (99). A1-081 is the positive check.
    {"StatementAtContinuationIndentIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val x = (1 +
      2)
      val y = x
)IFJ",
     2},
    // A1-087 §1.2: a logical line cannot start with an operator -> 2.
    // Passes today only by accident (RC-PROLOG gives 2); without the prolog
    // it fails with RC-EXPR (99). Twin A1-087p below.
    {"LineStartingWithBinaryOperatorIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val x = 1
  * 2
)IFJ",
     2},
    // A1-087p §1.2: the same operator inside one logical line.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"MultiplicationOnOneLineTwinIsAccepted", R"IFJ(@main def main(): Unit =
  val x = 1
  val y = x * 2
)IFJ",
     0},
    // A1-088 §1.2: "Logický řádek nemůže začínat operátorem" - the rule has
    // no exception for `-`, so a leading `-` is not a new unary statement.
    // Passes today only by accident (RC-PROLOG gives 2); without the prolog
    // it fails with RC-EXPR (99). Twin A1-088p below.
    {"LineStartingWithMinusIsSyntaxError", R"IFJ(@main def main(): Unit =
  val x = 1
  - x
)IFJ",
     2},
    // A1-088p §1.2: the same minus inside one logical line.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"SubtractionOnOneLineTwinIsAccepted", R"IFJ(@main def main(): Unit =
  val x = 1
  val y = x - 1
)IFJ",
     0},
    // A1-089 §1.2: an unfinished expression cannot continue deeper -> 2.
    // Passes today only by accident (RC-PROLOG gives 2); without the prolog
    // it fails with RC-EXPR (99). A1-081 is the positive check.
    {"UnfinishedExpressionDeeperNextLineIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val x = 1 +
    2
)IFJ",
     2},
    // A1-090 §1.2: an unfinished expression cannot continue at the same
    // indent -> 2.
    // Passes today only by accident (RC-PROLOG gives 2); without the prolog
    // it fails with RC-EXPR (99). A1-081 is the positive check.
    {"UnfinishedExpressionSameIndentIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val x = 1 +
  2
)IFJ",
     2},
};

INSTANTIATE_TEST_SUITE_P(IndentationAndLines, SpecLexicalTest,
                         ::testing::ValuesIn(kIndentationCases),
                         spec_case_name);

} // namespace

/*
 * AI-generated test block — trailing commas and semicolons.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for spec 1.2: a trailing comma is allowed only when a
 *   LF (with only whitespace and comments) stands between it and the closing
 *   parenthesis, also in call arguments; `;` separates statements on one
 *   logical line but a logical line holds only one function definition.
 */
namespace {

const SpecCase kCommaCases[] = {
    // A1-059 §1.2: trailing comma followed by a line comment and LF.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"TrailingCommaFollowedByLineComment", R"IFJ(def f(
  a: Int, // last
  ): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     0},
    // A1-060 §1.2: trailing comma followed by a block comment and LF.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"TrailingCommaFollowedByBlockComment", R"IFJ(def f(a: Int, /* c */
  ): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     0},
    // A1-061 §1.2: a comment without LF is not the required end of line -> 2.
    // Passes today only by accident (RC-PROLOG, RC-COMMENT); A1-060 is the
    // positive check.
    {"TrailingCommaWithSameLineCommentIsSyntaxError",
     R"IFJ(def f(a: Int, /* c */): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     2},
    // A1-069 §1.2: a whole comment line between trailing comma and `)`.
    // Fails today: RC-PROLOG (2); without the prolog RC-COMMENT (2).
    {"TrailingCommaFollowedByCommentLine", R"IFJ(def f(
  a: Int,
  // comment line
  ): Unit =
  return

@main def main(): Unit =
  return
)IFJ",
     0},
    // A1-091 §1.2: trailing comma and LF in call arguments.
    // Fails today: RC-PROLOG (2); without the prolog RC-CRASH (-1).
    {"TrailingCommaInCallArguments", R"IFJ(def g(a: Int): Unit =
  return

@main def main(): Unit =
  g(1,
    )
)IFJ",
     0},
    // A1-092 §1.2: LF right after `(` of a call is whitespace.
    // Fails today: RC-PROLOG (2); without the prolog RC-CRASH (-1).
    {"NewlineRightAfterCallParenthesis", R"IFJ(def g(a: Int): Unit =
  return

@main def main(): Unit =
  g(
    1)
)IFJ",
     0},
    // A1-094 §1.2: trailing comma without LF before `)` in a call -> 2.
    // Passes today only by accident (RC-PROLOG gives 2); without the prolog
    // it fails with RC-CRASH (-1). A1-091 is the positive check.
    {"TrailingCommaWithoutNewlineInCallIsSyntaxError",
     R"IFJ(def g(a: Int): Unit =
  return

@main def main(): Unit =
  g(1,)
)IFJ",
     2},
    // A1-080 §1.2: `;` separates statements on one logical line.
    // Fails today: RC-PROLOG (2); without the prolog RC-SEMI (2).
    {"SemicolonAfterBareReturn", R"IFJ(@main def main(): Unit =
  return; return
)IFJ",
     0},
    // A1-095 §1.2: a logical line holds one function definition -> 2.
    // Passes today only by accident (RC-PROLOG, RC-ONELINE); twin A1-095p.
    {"TwoDefinitionsOnOneLineAreSyntaxError", R"IFJ(def a(): Unit = ()
def b(): Unit = (); def c(): Unit = ()
@main def main(): Unit =
  return
)IFJ",
     2},
    // A1-095p §1.2: the same definitions on separate lines.
    // Fails today: RC-PROLOG (2); without the prolog RC-ONELINE (2).
    {"DefinitionsOnSeparateLinesAreAccepted", R"IFJ(def a(): Unit = ()
def b(): Unit = ()
def c(): Unit = ()
@main def main(): Unit =
  return
)IFJ",
     0},
};

INSTANTIATE_TEST_SUITE_P(CommasAndSemicolons, SpecLexicalTest,
                         ::testing::ValuesIn(kCommaCases), spec_case_name);

} // namespace

/*
 * AI-generated test block — prolog and the start of the source file.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Raw black-box cases for spec 2: only whitespace and comments may stand
 *   before the prolog (an unknown character there is still error 1), the
 *   keyword `import` is case-sensitive (a missing prolog gives 2), and the
 *   plain prolog is the positive check.
 */
namespace {

const SpecCase kPrologCases[] = {
    // A1-024 §2 + §1.1: a lexical error before the prolog comes first -> 1.
    // Passes today (the bad character is the first token).
    {"UnknownCharacterBeforePrologIsLexicalError", R"IFJ(#
import ifj26.*
@main def main(): Unit =
  return
)IFJ",
     1},
    // A1-076 §2: whitespace may stand before the prolog on its line.
    // Fails today: RC-LEADWS (1), then RC-PROLOG.
    {"SpacesBeforePrologOnItsLine", R"IFJ(   import ifj26.*
@main def main(): Unit =
  return
)IFJ",
     0},
    // A1-099 §1 + §2: `Import` is not the keyword, the prolog is missing -> 2.
    // Passes today only by accident (RC-PROLOG gives 2); twin A1-099p.
    {"CapitalizedImportIsMissingProlog", R"IFJ(Import ifj26.*
@main def main(): Unit =
  return
)IFJ",
     2},
    // A1-099p §2: the same file with the correct prolog.
    // Fails today: RC-PROLOG (2).
    {"CorrectPrologTwinIsAccepted", R"IFJ(import ifj26.*
@main def main(): Unit =
  return
)IFJ",
     0},
    // A1-111 §2 + §1.1: a nested block comment before the prolog.
    // Fails today: RC-COMMENT (2), then RC-PROLOG.
    {"NestedCommentBeforePrologOnSameLine",
     R"IFJ(/* a /* nested */ b */ import ifj26.*
@main def main(): Unit =
  return
)IFJ",
     0},
};

INSTANTIATE_TEST_SUITE_P(PrologAndFileStart, SpecLexicalRawTest,
                         ::testing::ValuesIn(kPrologCases), spec_case_name);

} // namespace

/*
 * AI-generated test block — white-box lexer helpers.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Copy of the lex_all helper of tests/lexer_edge_test.cc: lex a string
 *   through lexer_init/lexer_pls to the first non-OK status (with a hard
 *   token limit), copy every token with its text and position, release the
 *   lexer and the tmpfile, and compare with a compact expected table.
 */
namespace {

/** A token copied out of the lexer; @c text outlives the lexer. */
struct Lexed {
  Token token;
  std::string text;
};

struct LexResult {
  std::vector<Lexed> tokens;
  LexerStatus status;
};

/** Hard stop so a lexer that never finishes cannot hang the test. */
const size_t kMaxTokens = 1000U;

bool owns_text(TokenType type) {
  return type != TOKEN_INT && type != TOKEN_DOUBLE && type != TOKEN_INDENT &&
         type != TOKEN_DEDENT && type != TOKEN_EOF && type != TOKEN_INVALID;
}

/** Lexes @p input until the first status other than LEXER_OK. */
LexResult lex_all(const std::string &input) {
  LexResult result{{}, LEXER_INVALID_ARGUMENT};
  std::vector<char> buffer(input.begin(), input.end());
  buffer.push_back('\0');
  Lexer lexer{};
  if (lexer_init(&lexer, buffer.data()) != LEXER_OK) {
    ADD_FAILURE() << "cannot set up lexer";
    return result;
  }
  while (result.tokens.size() < kMaxTokens &&
         (result.status = lexer_pls(&lexer)) == LEXER_OK) {
    Lexed lexed{lexer.token, ""};
    if (owns_text(lexer.token.type)) {
      if (lexer.token.value.string_value != nullptr) {
        lexed.text = lexer.token.value.string_value;
      }
      lexed.token.value.string_value = nullptr;
    }
    result.tokens.push_back(lexed);
  }
  lexer_destroy(&lexer);
  return result;
}

/** Expected token; @c line 0 means the position is not checked. */
struct Expected {
  TokenType type;
  const char *text;
  double number;
  size_t line;
  size_t column;
};

Expected tok(TokenType type, const char *text = nullptr, size_t line = 0U,
             size_t column = 0U) {
  return {type, text, 0.0, line, column};
}
Expected ident(const char *text) { return tok(TOKEN_IDENTIFIER, text); }
Expected int_at(long long value, size_t line, size_t column) {
  return {TOKEN_INT, nullptr, static_cast<double>(value), line, column};
}
Expected dbl(double value) { return {TOKEN_DOUBLE, nullptr, value, 0U, 0U}; }

void expect_token(const Lexed &got, const Expected &want) {
  EXPECT_EQ(got.token.type, want.type);
  if (want.type == TOKEN_INT) {
    EXPECT_EQ(static_cast<double>(got.token.value.int_value), want.number);
  } else if (want.type == TOKEN_DOUBLE) {
    EXPECT_DOUBLE_EQ(got.token.value.double_value, want.number);
  } else if (want.text != nullptr) {
    EXPECT_EQ(got.text, want.text);
  }
  if (want.line != 0U) {
    EXPECT_EQ(got.token.line, want.line);
    EXPECT_EQ(got.token.column, want.column);
  }
}

/** One input and the exact token sequence that ends with LEXER_EOF. */
struct TokenCase {
  const char *name;
  std::string input;
  std::vector<Expected> tokens;
};

/** Prints only the case name so gtest does not dump the raw bytes. */
void PrintTo(const TokenCase &token_case, std::ostream *out) {
  *out << token_case.name;
}

std::string token_case_name(const ::testing::TestParamInfo<TokenCase> &info) {
  return info.param.name;
}

class SpecLexicalTokensTest : public ::testing::TestWithParam<TokenCase> {};

} // namespace

// §1.1: each row lexes to exactly the listed tokens, then LEXER_EOF.
TEST_P(SpecLexicalTokensTest, InputLexesToExactTokens) {
  // Arrange
  const TokenCase &token_case = GetParam();

  // Act
  LexResult result = lex_all(token_case.input);

  // Assert
  EXPECT_EQ(result.status, LEXER_EOF) << token_case.input;
  ASSERT_EQ(result.tokens.size(), token_case.tokens.size()) << token_case.input;
  for (size_t index = 0U; index < token_case.tokens.size(); index += 1U) {
    SCOPED_TRACE(index);
    expect_token(result.tokens[index], token_case.tokens[index]);
  }
}

/*
 * AI-generated test block — white-box token sequences for comments and
 * Double literals.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   White-box lexer tables for spec 1.1: a block comment is one whitespace
 *   between tokens (also overlapping markers, only stars, a quote inside), a
 * quote in a line comment starts no string, and a Double integer part may have
 *   leading zeros so `00.5`, `007.5`, `00e1` are one Double token each.
 */
namespace {

const TokenCase kCommentTokenCases[] = {
    // A1-122 §1.1: a block comment without LF is whitespace: `1` and `2`.
    // Fails today: RC-COMMENT (`/` `*` `*` `/` are operators).
    {"BlockCommentSeparatesNumbers",
     "1/**/2",
     {int_at(1, 1U, 1U), int_at(2, 1U, 6U)}},
    // A1-123 §1.1: `/*/` opens a comment but does not close it.
    // Fails today: RC-COMMENT.
    {"OverlappingCommentMarkersDoNotClose",
     "a /*/ b */ c",
     {ident("a"), ident("c")}},
    // A1-124 §1.1: `/***/` is a complete comment.
    // Fails today: RC-COMMENT.
    {"CommentWithStarsOnly", "a /***/ b", {ident("a"), ident("b")}},
    // A1-125 §1.1: content of a comment is ignored, `"` starts no string.
    // Fails today: RC-COMMENT (unterminated string error).
    {"QuoteInsideBlockCommentIsIgnored",
     "a /* \" */ b",
     {ident("a"), ident("b")}},
    // A1-126 §1.1: a line comment is one end of line, `"` starts no string.
    // Fails today: RC-COMMENT (unterminated string error).
    {"QuoteInsideLineCommentIsIgnored",
     "a // \"\nb",
     {ident("a"), tok(TOKEN_NEWLINE), ident("b")}},
};

INSTANTIATE_TEST_SUITE_P(Comments, SpecLexicalTokensTest,
                         ::testing::ValuesIn(kCommentTokenCases),
                         token_case_name);

const TokenCase kDoubleTokenCases[] = {
    // A1-133 §1.1: "Celá část ... jsou sekvence číslic"; the leading-zero
    // ban is stated only for Int. Stricter twin of lexer_bug_test.cc
    // LeadingZeroDoubleIsNotSplitIntoIntAndDouble (exactly one Double).
    // Fails today: RC-LEADZERO (`0` + `0.5`).
    {"DoubleWithTwoLeadingZeros", "00.5", {dbl(0.5)}},
    // A1-133 §1.1: `007.5` is one Double.
    // Fails today: RC-LEADZERO (`0` `0` `7.5`).
    {"DoubleWithLeadingZerosBeforeSeven", "007.5", {dbl(7.5)}},
    // A1-133 §1.1: `00e1` is one Double (exponent form).
    // Fails today: RC-LEADZERO.
    {"DoubleExponentFormWithLeadingZeros", "00e1", {dbl(0.0)}},
};

INSTANTIATE_TEST_SUITE_P(DoubleLiterals, SpecLexicalTokensTest,
                         ::testing::ValuesIn(kDoubleTokenCases),
                         token_case_name);

} // namespace

/*
 * AI-generated test block — white-box lexer cases with positions.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   White-box lexer tests for spec 1.1/1.2 and 2: a comment between `@` and
 *   `main` is a forbidden character, a tab inside a block comment is no
 *   indentation, the position after a multiline comment is exact, a Double
 *   just above DBL_MAX is out of range, and a multiline string lexeme keeps
 *   its raw text with exact token positions.
 */
namespace {

/** Number of tokens of @p type in @p result. */
size_t count_type(const LexResult &result, TokenType type) {
  size_t count = 0U;
  for (const Lexed &lexed : result.tokens) {
    count += lexed.token.type == type ? 1U : 0U;
  }
  return count;
}

/** Index of the first token that is not TOKEN_NEWLINE, or the size. */
size_t first_non_newline(const LexResult &result) {
  size_t index = 0U;
  while (index < result.tokens.size() &&
         result.tokens[index].token.type == TOKEN_NEWLINE) {
    index += 1U;
  }
  return index;
}

class SpecLexicalLexerTest : public ::testing::Test {};

} // namespace

// A1-120 §2 + §1.1: no character (a comment is whitespace) between `@` and
// `main`, so the first token is invalid. Passes today also because the lexer
// does not know comments at all (RC-COMMENT); it stays meaningful once
// comments are lexed as whitespace.
TEST_F(SpecLexicalLexerTest, CommentBetweenAtAndMainIsRejected) {
  // Arrange
  const std::string input = "@/**/main";

  // Act
  LexResult result = lex_all(input);

  // Assert
  EXPECT_TRUE(result.tokens.empty());
  EXPECT_EQ(result.status, LEXER_INVALID_TOKEN);
}

// A1-129 §1.1 + §1.2: a tab inside a block comment is content, not indent.
// Fails today: RC-COMMENT (tab at line start gives LEXER_INVALID_TOKEN 2:2).
TEST_F(SpecLexicalLexerTest, TabInsideBlockCommentIsNotIndentation) {
  // Arrange
  const std::string input = "a /* x\n\tb */\nc";

  // Act
  LexResult result = lex_all(input);

  // Assert
  EXPECT_EQ(result.status, LEXER_EOF);
  EXPECT_GE(count_type(result, TOKEN_NEWLINE), 1U);
  EXPECT_EQ(count_type(result, TOKEN_INDENT), 0U);
  ASSERT_GE(result.tokens.size(), 2U);
  EXPECT_EQ(result.tokens.front().text, "a");
  EXPECT_EQ(result.tokens.back().text, "c");
  EXPECT_EQ(result.tokens.size() - count_type(result, TOKEN_NEWLINE), 2U);
}

// A1-130 §1.1: after a multiline block comment `x` is at line 3, column 1.
// Fails today: RC-COMMENT (the first token is `/` at 1:1).
TEST_F(SpecLexicalLexerTest, PositionAfterMultilineBlockComment) {
  // Arrange
  const std::string input = "/* a\nb */\nx";

  // Act
  LexResult result = lex_all(input);
  size_t index = first_non_newline(result);

  // Assert
  ASSERT_LT(index, result.tokens.size());
  EXPECT_EQ(result.tokens[index].token.type, TOKEN_IDENTIFIER);
  EXPECT_EQ(result.tokens[index].text, "x");
  EXPECT_EQ(result.tokens[index].token.line, 3U);
  EXPECT_EQ(result.tokens[index].token.column, 1U);
}

// A1-134 §1.1: 1.7976931348623159e308 rounds to +inf, outside binary64.
// Fails today: RC-DBLRANGE (lexed as DOUBLE inf).
TEST_F(SpecLexicalLexerTest, DoubleJustAboveMaxIsLexicalError) {
  // Arrange
  const std::string input = "1.7976931348623159e308";

  // Act
  LexResult result = lex_all(input);

  // Assert
  EXPECT_TRUE(result.tokens.empty());
  EXPECT_EQ(result.status, LEXER_INVALID_TOKEN);
}

// A1-136 §1.1: a multiline string "Obsahuje doslova všechny znaky": the raw
// lexeme (string_value, `"""` delimiters included, nothing decoded) holds
// the LFs and the two characters `\` `n` unchanged, and the token positions
// around it are exact. The decoded value is not checked here (the lexer
// decodes no escapes in any string; that belongs to code generation).
TEST_F(SpecLexicalLexerTest, MultilineLexemeKeepsRawTextAndPositions) {
  // Arrange
  const std::string input =
      "val x = \"\"\"A\n       hoj s\\nvete\n        \"\"\"\nval y";

  // Act
  LexResult result = lex_all(input);

  // Assert
  EXPECT_EQ(result.status, LEXER_EOF);
  ASSERT_GE(result.tokens.size(), 6U);
  const Lexed &string = result.tokens[3];
  EXPECT_EQ(string.token.type, TOKEN_STRING);
  EXPECT_EQ(string.token.line, 1U);
  EXPECT_EQ(string.token.column, 9U);
  EXPECT_EQ(string.text, "\"\"\"A\n       hoj s\\nvete\n        \"\"\"");
  EXPECT_EQ(result.tokens[4].token.type, TOKEN_NEWLINE);
  EXPECT_EQ(result.tokens[4].token.line, 3U);
  EXPECT_EQ(result.tokens[4].token.column, 12U);
  EXPECT_EQ(result.tokens[5].token.type, TOKEN_KW_VAL);
  EXPECT_EQ(result.tokens[5].token.line, 4U);
}
