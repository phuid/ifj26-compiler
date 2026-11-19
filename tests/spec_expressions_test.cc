/*
 * AI-generated test block — shared table row and runner for expressions.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write black-box spec conformance tests for IFJ26 expressions (spec 1.1
 *   negative numbers, 1.2 logical lines, 1.3 conversions, 3). Each case is
 *   a whole program body run through tests/spec_harness.h and checked
 *   against the exit code of spec chapter 6. Group the cases into TEST_P
 *   tables by topic, give every row a readable name and a comment with the
 *   spec rule and, when it fails today, the root cause.
 */

// Root causes named in the comments (review revB, section D):
//   RC-PROLOG     parser.c p_program: `import ifj26.*` is unknown -> 2.
//   RC-EXPR       parser.c p_expression is a stub returning NULL: val/var
//                 initialisation -> 99, if condition / return -> 2.
//   RC-STMT-CRASH parser.c p_statement/p_code_block: a statement starting
//                 with a term dereferences NULL (SIGSEGV, harness -1).
//   RC-UNARY      psa.h/ast.h/semantic.c: unary minus has no terminal, no
//                 AST node and no type rule.
//   RC-PARENNL    psa/parser: NEWLINE/INDENT/DEDENT inside parentheses of
//                 an expression are not skipped.

#include <string>

#include "gtest/gtest.h"

#include "spec_harness.h"

namespace {

/** One black-box program body with the exit code the spec requires. */
struct ExprCase {
  const char *name;
  const char *source;
  int expected;
};

/** Uses the row name as the gtest parameter name. */
std::string expr_case_name(const ::testing::TestParamInfo<ExprCase> &info) {
  return info.param.name;
}

/** Compiles the row's program and checks the chapter 6 exit code. */
void expect_spec_exit_code(const ExprCase &row) {
  // Arrange: the row holds the program body; the harness adds the prolog.
  const char *source = row.source;

  // Act
  int code = ifj26_exit_code(source);

  // Assert
  EXPECT_EQ(code, row.expected) << source;
}

} // namespace

/*
 * AI-generated test block — precedence of relations vs. arithmetic and
 * concatenation (spec 3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover the precedence table of spec 3 where it is visible in types:
 *   relations and equality bind looser than + - * / and concatenation,
 *   parentheses really group (a Boolean operand then gives code 6), plus
 *   long arithmetic chains and redundant parentheses that must stay Int.
 */
namespace {

const ExprCase kPrecedenceCases[] = {
    // §3 (A5-001): `<` (level 4) binds looser than + and * (2, 3);
    // the wrong grouping 1 + (2 < 3) * 4 would be a type error.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (if/return -> 2).
    {"RelationalBindsLooserThanArithmetic",
     R"IFJ(@main def main(): Unit =
  if 1 + 2 < 3 * 4 then
    ()
  else
    ()
)IFJ",
     0},
    // §3 (A5-002): `s + s < s` is (s + s) < s, a lexicographic
    // comparison; s + (s < s) would be String + Boolean.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"RelationalBindsLooserThanConcatenation",
     R"IFJ(@main def main(): Unit =
  val s = "ab"
  if s + s < s then
    ()
  else
    ()
)IFJ",
     0},
    // §3 (A5-003): `s == s + s` is s == (s + s); (s == s) + s would be
    // Boolean + String.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"EqualityBindsLooserThanConcatenation",
     R"IFJ(@main def main(): Unit =
  val s = "ab"
  if s == s + s then
    ()
  else
    ()
)IFJ",
     0},
    // §3 (A5-005): parentheses force (1 < 2) as an operand of `*`, so a
    // Boolean is used in arithmetic -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (if/return -> 2).
    {"ParenthesizedRelationTimesIntIsTypeError",
     R"IFJ(@main def main(): Unit =
  if (1 < 2) * 3 > 0 then
    ()
  else
    ()
)IFJ",
     6},
    // §3 (A5-006): ((s + s) < s) + s is Boolean + String -> 6; checks
    // that parentheses really group.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"ParenthesizedRelationPlusStringIsTypeError",
     R"IFJ(@main def main(): Unit =
  val s = "ab"
  if (s + s < s) + s == s then
    ()
  else
    ()
)IFJ",
     6},
    // §3 (A5-012): a chain of / / * - - + over Int stays Int. Positive
    // control for the syntax errors in the lines/parentheses table.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"LongArithmeticChainIsInt",
     R"IFJ(@main def main(): Unit =
  val r: Int = 8 / 2 / 2 * 3 - 1 - 1 + 4
  print(r)
)IFJ",
     0},
    // §3 (A5-013): parentheses around subexpressions and around a lone
    // literal (5). Positive control for the parentheses syntax errors.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"RedundantParenthesesAreAccepted",
     R"IFJ(@main def main(): Unit =
  val r: Int = (1 + 2) * (3 - 4) / (5)
  print(r)
)IFJ",
     0},
};

class SpecExpressionsPrecedenceTest
    : public ::testing::TestWithParam<ExprCase> {};

TEST_P(SpecExpressionsPrecedenceTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecExpressions, SpecExpressionsPrecedenceTest,
                         ::testing::ValuesIn(kPrecedenceCases), expr_case_name);

} // namespace

/*
 * AI-generated test block — unary minus (spec 1.1, 3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Unary minus has the highest precedence and right associativity, works
 *   only on numbers and keeps the type; a negative number is unary minus
 *   applied to a literal and `5-3` without spaces is a binary minus.
 */
namespace {

const ExprCase kUnaryMinusCases[] = {
    // §3 (A5-007): `-i < i` is (-i) < i; -(i < i) would be a type error.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-UNARY.
    {"UnaryMinusBindsTighterThanRelational",
     R"IFJ(@main def main(): Unit =
  val i = 1
  if -i < i then
    ()
  else
    ()
)IFJ",
     0},
    // §3 (A5-008): unary minus applies only to a number; -(i < i) -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-UNARY.
    {"UnaryMinusOverBooleanIsTypeError",
     R"IFJ(@main def main(): Unit =
  val i = 1
  if -(i < i) then
    ()
  else
    ()
)IFJ",
     6},
    // §3 (A5-009): (-i) * i == -(i * i) compares Int with Int.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-UNARY.
    {"UnaryMinusBindsTighterThanMultiplication",
     R"IFJ(@main def main(): Unit =
  val i = 1
  if -i * i == -(i * i) then
    ()
  else
    ()
)IFJ",
     0},
    // §3, §1.3 (A5-022): unary minus keeps the type, so -2.5 is Double
    // and cannot initialise an Int -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-UNARY.
    {"NegatedDoubleIntoIntIsTypeError",
     R"IFJ(@main def main(): Unit =
  val r: Int = -2.5
  print(r)
)IFJ",
     6},
    // §3, §1.3 (A5-024): -i is Int and widens to Double implicitly.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-UNARY.
    {"NegatedIntWidensToDouble",
     R"IFJ(@main def main(): Unit =
  val i = 2
  val r: Double = -i
  print(r)
)IFJ",
     0},
    // §3 (A5-025): unary minus applies only to a number; -s -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-UNARY.
    {"UnaryMinusOverStringIsTypeError",
     R"IFJ(@main def main(): Unit =
  val s = "ab"
  val r = -s
  print(r)
)IFJ",
     6},
    // §3, §1.1 (A5-027): unary minus over the Unit literal () -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-UNARY.
    {"UnaryMinusOverUnitLiteralIsTypeError",
     R"IFJ(@main def main(): Unit =
  val r = -()
  print(r)
)IFJ",
     6},
    // §3 (A5-028): `- -5` is -(-5), an Int (unary is right associative).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-UNARY.
    {"DoubleUnaryMinusIsRightAssociative",
     R"IFJ(@main def main(): Unit =
  val r: Int = - -5
  print(r)
)IFJ",
     0},
    // §3 (A5-030): `i - -i` is i - (-i).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-UNARY.
    {"BinaryMinusFollowedByUnaryMinus",
     R"IFJ(@main def main(): Unit =
  val i = 3
  val r: Int = i - -i
  print(r)
)IFJ",
     0},
    // §3 (A5-032): unary minus over a parenthesized sum, times Int, is
    // accepted as an Int (any grouping gives Int; checks acceptance only).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-UNARY.
    {"UnaryMinusOverParenthesizedSum",
     R"IFJ(@main def main(): Unit =
  val i = 3
  val r: Int = -(i + i) * i
  print(r)
)IFJ",
     0},
    // §3, §1.3 (A5-034): (-1.5) < (-1) compares Double with a widened Int.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (if/return -> 2),
    // then RC-UNARY.
    {"NegativeDoubleLessThanNegativeInt",
     R"IFJ(@main def main(): Unit =
  if -1.5 < -1 then
    ()
  else
    ()
)IFJ",
     0},
    // §1.1 (A5-036): a negative number is unary minus plus a literal, so
    // `5-3` and `i-1` are binary minus, not two adjacent operands.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"MinusWithoutSpacesIsBinaryOperator",
     R"IFJ(@main def main(): Unit =
  val i = 3
  val r: Int = 5-3 + i-1
  print(r)
)IFJ",
     0},
    // §3 (A5-037): -d * 2 with a Double d is Double (any grouping) and
    // cannot initialise an Int -> 6. Unlike A5-022 the Double comes from a
    // variable and a product, not from a negated literal.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-UNARY.
    {"NegatedDoubleTimesIntIntoIntIsTypeError",
     R"IFJ(@main def main(): Unit =
  val d = 0.5
  val r: Int = -d * 2
  print(r)
)IFJ",
     6},
};

class SpecExpressionsUnaryMinusTest
    : public ::testing::TestWithParam<ExprCase> {};

TEST_P(SpecExpressionsUnaryMinusTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecExpressions, SpecExpressionsUnaryMinusTest,
                         ::testing::ValuesIn(kUnaryMinusCases), expr_case_name);

} // namespace

/*
 * AI-generated test block — Int/Double arithmetic and the implicit conversion
 * (spec 1.3, 3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   + - * over Int give Int, with a Double operand they give Double; the
 *   only implicit conversion is Int -> Double, so a Double where an Int is
 *   expected (initialisation, return) is a type error, and the Unit
 *   literal is not a number.
 */
namespace {

const ExprCase kArithmeticCases[] = {
    // §3, §1.3 (A5-015): 1 + 2 * 3.0 is Double whatever the grouping.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"IntPlusDoubleProductIntoDoubleIsAccepted",
     R"IFJ(@main def main(): Unit =
  val r: Double = 1 + 2 * 3.0
  print(r)
)IFJ",
     0},
    // §3, §1.3 (A5-016): a Double result cannot initialise an Int -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"IntPlusDoubleProductIntoIntIsTypeError",
     R"IFJ(@main def main(): Unit =
  val r: Int = 1 + 2 * 3.0
  print(r)
)IFJ",
     6},
    // §1.3 (A5-048): Double + Int is Double and fits a Double var.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"DoublePlusIntAssignedToDoubleVarIsAccepted",
     R"IFJ(@main def main(): Unit =
  var r = 1.5
  r = r + 1
  print(r)
)IFJ",
     0},
    // §1.3, §2.3 rule 4 (A5-055): return of a Double expression from an
    // Int function -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (if/return -> 2).
    {"ReturnedDoubleSumFromIntFunctionIsTypeError",
     R"IFJ(def f(): Int =
  return 1 + 2.0

@main def main(): Unit =
  val r = f()
  print(r)
)IFJ",
     6},
    // §1.1, §3 (A5-133): () is the Unit literal, not empty parentheses,
    // so 1 + () is Int + Unit -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"IntPlusUnitLiteralIsTypeError",
     R"IFJ(@main def main(): Unit =
  val r = 1 + ()
  print(r)
)IFJ",
     6},
};

class SpecExpressionsArithmeticTest
    : public ::testing::TestWithParam<ExprCase> {};

TEST_P(SpecExpressionsArithmeticTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecExpressions, SpecExpressionsArithmeticTest,
                         ::testing::ValuesIn(kArithmeticCases), expr_case_name);

} // namespace

/*
 * AI-generated test block — division (spec 3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Division of two Int values is integer division and gives Int; with a
 *   Double operand it gives Double. The Int quotient may widen to Double
 *   afterwards. Division by zero is a run-time matter, so it compiles.
 */
namespace {

const ExprCase kDivisionCases[] = {
    // §3 (A5-040): division of two Int values is integer division, Int.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"IntDivisionIsInt",
     R"IFJ(@main def main(): Unit =
  val r: Int = 7 / 2
  print(r)
)IFJ",
     0},
    // §3 (A5-041): 7 / 2.0 is Double and cannot initialise an Int -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"DivisionByDoubleIntoIntIsTypeError",
     R"IFJ(@main def main(): Unit =
  val r: Int = 7 / 2.0
  print(r)
)IFJ",
     6},
    // §3, §1.3 (A5-042): the Int quotient 7 / 2 widens to Double.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"IntQuotientWidensToDouble",
     R"IFJ(@main def main(): Unit =
  val r: Double = 7 / 2
  print(r)
)IFJ",
     0},
    // §3, §1.3 (A5-049): the Int quotient 1 / 2 may be assigned to a
    // Double var (checks acceptance only; the value 0 vs 0.5 is run time).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"IntQuotientAssignedToDoubleVarIsAccepted",
     R"IFJ(@main def main(): Unit =
  var r = 0.0
  r = 1 / 2
  print(r)
)IFJ",
     0},
    // §3, chapter 6 (A5-050): integer division by zero is a run-time
    // error, the compiler must not reject it.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"IntDivisionByZeroLiteralCompiles",
     R"IFJ(@main def main(): Unit =
  val r: Int = 1 / 0
  print(r)
)IFJ",
     0},
    // §3 (A5-051): floating division by zero follows IFJcode26 at run
    // time; it compiles.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"DoubleDivisionByZeroCompiles",
     R"IFJ(@main def main(): Unit =
  val r: Double = 1.0 / 0.0
  print(r)
)IFJ",
     0},
    // §2.3, §3 (A5-058): an unannotated function ending with a / 2
    // infers Int, which fits `val r: Int`.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"InferredResultOfIntQuotientIsInt",
     R"IFJ(def f() =
  val a = 7
  a / 2

@main def main(): Unit =
  val r: Int = f()
  print(r)
)IFJ",
     0},
    // §2.3, §3, §1.3 (A5-059): a / 2.0 infers Double, which cannot
    // initialise an Int -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"InferredResultOfDoubleQuotientIntoIntIsTypeError",
     R"IFJ(def f() =
  val a = 7
  a / 2.0

@main def main(): Unit =
  val r: Int = f()
  print(r)
)IFJ",
     6},
};

class SpecExpressionsDivisionTest : public ::testing::TestWithParam<ExprCase> {
};

TEST_P(SpecExpressionsDivisionTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecExpressions, SpecExpressionsDivisionTest,
                         ::testing::ValuesIn(kDivisionCases), expr_case_name);

} // namespace

/*
 * AI-generated test block — concatenation and no implicit conversion to String
 * (spec 2.1, 3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   + over two String values concatenates; there is no automatic
 *   conversion to String (toStr must be used), * is not defined on String,
 *   and a discarded expression statement is still type checked.
 */
namespace {

const ExprCase kConcatenationCases[] = {
    // §3 (A5-011): a left-associative concatenation chain including ""
    // is a String. Positive control for the syntax errors in the
    // lines/parentheses table.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"ConcatenationChainWithEmptyStringIsString",
     R"IFJ(@main def main(): Unit =
  val r: String = "a" + "b" + "c" + ""
  print(r)
)IFJ",
     0},
    // §3 (A5-062): no automatic conversion to String; "a" + 1 -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"StringPlusIntIsTypeError",
     R"IFJ(@main def main(): Unit =
  val r = "a" + 1
  print(r)
)IFJ",
     6},
    // §3 (A5-065): "a" + () is String + Unit -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"StringPlusUnitIsTypeError",
     R"IFJ(@main def main(): Unit =
  val r = "a" + ()
  print(r)
)IFJ",
     6},
    // §3 (A5-066): only + is defined on String; "a" * 2 -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"StringTimesIntIsTypeError",
     R"IFJ(@main def main(): Unit =
  val r = "a" * 2
  print(r)
)IFJ",
     6},
    // §3, §2.2 (A5-069): the correct way is toStr first, then +.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"ConcatenationWithToStrResultIsAccepted",
     R"IFJ(@main def main(): Unit =
  val t = toStr(1)
  val r: String = "a" + t
  print(r)
)IFJ",
     0},
    // §3 (A5-070): 1 + 2 + "a" mixes Int and String -> 6 (either grouping
    // gives 6; checks that the String operand is not converted).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"IntSumPlusStringIsTypeError",
     R"IFJ(@main def main(): Unit =
  val r = 1 + 2 + "a"
  print(r)
)IFJ",
     6},
    // §2.1, §3 (A5-116): a stand-alone expression statement is type
    // checked too; "a" + 1 -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-STMT-CRASH (-1).
    {"DiscardedStringPlusIntStatementIsTypeError",
     R"IFJ(@main def main(): Unit =
  "a" + 1
  ()
)IFJ",
     6},
};

class SpecExpressionsConcatenationTest
    : public ::testing::TestWithParam<ExprCase> {};

TEST_P(SpecExpressionsConcatenationTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecExpressions, SpecExpressionsConcatenationTest,
                         ::testing::ValuesIn(kConcatenationCases),
                         expr_case_name);

} // namespace

/*
 * AI-generated test block — relational operators (spec 3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   < <= > >= compare two numbers (with the Int -> Double conversion) or
 *   two strings lexicographically; any other operand pair, including Unit,
 *   is a type error.
 */
namespace {

const ExprCase kRelationalCases[] = {
    // §3 (A5-072): two strings compare lexicographically.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (if/return -> 2).
    {"StringLessThanStringIsAccepted",
     R"IFJ(@main def main(): Unit =
  if "abc" < "abd" then
    ()
  else
    ()
)IFJ",
     0},
    // §3, §1.3 (A5-074): Int < Double with the implicit conversion.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (if/return -> 2).
    {"IntLessThanDoubleIsAccepted",
     R"IFJ(@main def main(): Unit =
  if 1 < 2.5 then
    ()
  else
    ()
)IFJ",
     0},
    // §3 (A5-075): (i + i) <= (d * 2) compares Int with Double.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"IntSumLessEqualDoubleProductIsAccepted",
     R"IFJ(@main def main(): Unit =
  val i = 1
  val d = 0.5
  if i + i <= d * 2 then
    ()
  else
    ()
)IFJ",
     0},
    // §3 (A5-076): only numbers or two strings; "a" < 1 -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (if/return -> 2).
    {"StringLessThanIntIsTypeError",
     R"IFJ(@main def main(): Unit =
  if "a" < 1 then
    ()
  else
    ()
)IFJ",
     6},
    // §3 (A5-078): Unit values cannot be ordered; u < u -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"UnitLessThanUnitIsTypeError",
     R"IFJ(@main def main(): Unit =
  val u = ()
  if u < u then
    ()
  else
    ()
)IFJ",
     6},
};

class SpecExpressionsRelationalTest
    : public ::testing::TestWithParam<ExprCase> {};

TEST_P(SpecExpressionsRelationalTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecExpressions, SpecExpressionsRelationalTest,
                         ::testing::ValuesIn(kRelationalCases), expr_case_name);

} // namespace

/*
 * AI-generated test block — == and != (spec 3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   == and != compare two values of the same type or two numbers (with
 *   the Int -> Double conversion); mixed types are a type error. Per the
 *   lead's decision Unit == Unit is accepted (same types).
 */
namespace {

const ExprCase kEqualityCases[] = {
    // §3 (A5-079): == over two numbers, Int == Double.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (if/return -> 2).
    {"IntEqualsDoubleIsAccepted",
     R"IFJ(@main def main(): Unit =
  if 1 == 1.0 then
    ()
  else
    ()
)IFJ",
     0},
    // §3 (A5-080): == over two values of the same type.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (if/return -> 2).
    {"StringEqualsStringIsAccepted",
     R"IFJ(@main def main(): Unit =
  if "a" == "a" then
    ()
  else
    ()
)IFJ",
     0},
    // §3 (A5-081): != needs the same types or two numbers -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (if/return -> 2).
    {"StringNotEqualsIntIsTypeError",
     R"IFJ(@main def main(): Unit =
  if "a" != 1 then
    ()
  else
    ()
)IFJ",
     6},
    // §3 (A5-082): == compares values of the same type; Unit == Unit
    // is accepted (lead's decision: 0).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"UnitEqualsUnitIsAccepted",
     R"IFJ(@main def main(): Unit =
  val u = ()
  if u == () then
    ()
  else
    ()
)IFJ",
     0},
    // §3 (A5-085): Int == String -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"IntEqualsStringIsTypeError",
     R"IFJ(@main def main(): Unit =
  val i = 1
  val s = "a"
  if i == s then
    ()
  else
    ()
)IFJ",
     6},
};

class SpecExpressionsEqualityTest : public ::testing::TestWithParam<ExprCase> {
};

TEST_P(SpecExpressionsEqualityTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecExpressions, SpecExpressionsEqualityTest,
                         ::testing::ValuesIn(kEqualityCases), expr_case_name);

} // namespace

/*
 * AI-generated test block — Boolean only directly in a condition (spec 1.3,
 * 2.4, 3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Without BOOLTHEN a Boolean result may only be used directly as the
 *   condition of if/while: it cannot be stored, assigned, returned, used
 *   as an operand (also not of ==), and a condition must be Boolean.
 */
namespace {

const ExprCase kBooleanCases[] = {
    // §3 (A5-010): a Boolean may be used only directly in a condition,
    // an operand of == is not one; (i < i) == (i < i) -> 6 (lead's
    // decision; only without BOOLTHEN).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"BooleanOperandsOfEqualityAreTypeError",
     R"IFJ(@main def main(): Unit =
  val i = 1
  if (i < i) == (i < i) then
    ()
  else
    ()
)IFJ",
     6},
    // §3, §2.2 (A5-100): a Boolean cannot be stored; val b = 1 < 2 -> 6
    // (only without BOOLTHEN; restores A2-030/A5-100 lost in cross-dedup).
    // Control: RelationalBindsLooserThanArithmetic.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"StoringRelationInValIsTypeError",
     R"IFJ(@main def main(): Unit =
  val b = 1 < 2
  print(1)
)IFJ",
     6},
    // §3, chapter 6 (A5-101): a Boolean assigned to an Int var -> 6 (a
    // type mismatch; the stand-alone ban is A5-100 and A5-103).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"AssigningRelationToIntVarIsTypeError",
     R"IFJ(@main def main(): Unit =
  var x = 0
  x = 1 < 2
  print(x)
)IFJ",
     6},
    // §3 (A5-102): return 1 < 2 from an Int function -> 6 (a type
    // mismatch; the stand-alone ban is A5-100 and A5-103).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (if/return -> 2).
    {"ReturningRelationFromIntFunctionIsTypeError",
     R"IFJ(def f(): Int =
  return 1 < 2

@main def main(): Unit =
  val r = f()
  print(r)
)IFJ",
     6},
    // §3, §2.3 (A5-103): an unannotated function ending with a
    // relation would need the Boolean type -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"InferredBooleanResultTypeIsTypeError",
     R"IFJ(def f() =
  val a = 1
  a < 2

@main def main(): Unit =
  f()
)IFJ",
     6},
    // §3 (A5-104): (1 < 2) + 1 uses a Boolean as an operand -> 6. Unlike
    // A5-005 the result initialises a val instead of being a condition.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"ParenthesizedRelationPlusIntIsTypeError",
     R"IFJ(@main def main(): Unit =
  val r = (1 < 2) + 1
  print(r)
)IFJ",
     6},
    // §2.4, §3 (A5-108): the condition -i is Int, not Boolean -> 6 (not 2:
    // chapter 6 code 6 names control constructs). Unlike A4-030/031 in
    // spec_control_flow_test.cc the Int comes from unary minus.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-UNARY.
    {"NegatedIntConditionIsTypeError",
     R"IFJ(@main def main(): Unit =
  val i = 1
  if -i then
    ()
  else
    ()
)IFJ",
     6},
};

class SpecExpressionsBooleanTest : public ::testing::TestWithParam<ExprCase> {};

TEST_P(SpecExpressionsBooleanTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecExpressions, SpecExpressionsBooleanTest,
                         ::testing::ValuesIn(kBooleanCases), expr_case_name);

} // namespace

/*
 * AI-generated test block — logical lines and parentheses in expressions (spec
 * 1.2, 3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   A line break inside parentheses is white space (also blank lines and
 *   a physical line starting with an operator), but a logical line cannot
 *   start with an operator nor continue an unfinished expression;
 *   unbalanced parentheses, adjacent operands or operators are syntax
 *   errors; the last expression may end at EOF without a line break.
 */
namespace {

const ExprCase kLinesAndParenthesesCases[] = {
    // §1.2 (A5-122): a logical line cannot start with an operator (no
    // Scala 3 leading infix); the deeper indent without a block header is
    // also a block-structure error, the same-level case is A1-087.
    // Control: PhysicalLineInsideParenthesesMayStartWithOperator.
    // Passes vacuously today (RC-PROLOG); without the prolog RC-EXPR (val/var
    // -> 99).
    {"LogicalLineStartingWithOperatorIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val r = 1
    * 2
  print(r)
)IFJ",
     2},
    // §1.2 (A5-124): line breaks inside nested parentheses of one
    // expression are white space (the flat case is A1-081).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-PARENNL.
    {"LineBreaksInsideNestedParenthesesAreWhitespace",
     R"IFJ(@main def main(): Unit =
  val r = (1 +
    (2 *
      3))
  print(r)
)IFJ",
     0},
    // §1.2 (A5-125): line breaks right after ( and before ) inside the
    // parentheses are white space.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-PARENNL.
    {"ParenthesesOnTheirOwnLinesAreAccepted",
     R"IFJ(@main def main(): Unit =
  val r = (
      1 + 2
    )
  print(r)
)IFJ",
     0},
    // §1.2 (A5-127): a blank physical line inside parentheses is ignored.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-PARENNL.
    {"BlankLineInsideParenthesesIsIgnored",
     R"IFJ(@main def main(): Unit =
  val r = (1 +

      2)
  print(r)
)IFJ",
     0},
    // §3 (A5-129): an unclosed ( on the last line before EOF.
    // Control: RedundantParenthesesAreAccepted.
    // Passes vacuously today (RC-PROLOG); without the prolog RC-STMT-CRASH
    // (-1).
    {"UnclosedParenthesisBeforeEofIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  print(1)
  val r = (1 + 2
)IFJ",
     2},
    // §3 (A5-130): a surplus ). Control: RedundantParenthesesAreAccepted.
    // Passes vacuously today (RC-PROLOG); without the prolog RC-EXPR (val/var
    // -> 99).
    {"ExtraClosingParenthesisIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val r = 1 + 2)
  print(r)
)IFJ",
     2},
    // §3 (A5-131): two operands without an operator.
    // Control: RedundantParenthesesAreAccepted.
    // Passes vacuously today (RC-PROLOG); without the prolog RC-EXPR (val/var
    // -> 99).
    {"AdjacentOperandsAreSyntaxError",
     R"IFJ(@main def main(): Unit =
  val r = 1 2
  print(r)
)IFJ",
     2},
    // §3 (A5-132): two binary operators in a row.
    // Control: LongArithmeticChainIsInt.
    // Passes vacuously today (RC-PROLOG); without the prolog RC-EXPR (val/var
    // -> 99).
    {"AdjacentBinaryOperatorsAreSyntaxError",
     R"IFJ(@main def main(): Unit =
  val r = 1 * / 2
  print(r)
)IFJ",
     2},
    // §1.2, §3 (A5-135): an expression ending with an operator right
    // before EOF. Control: ConcatenationChainWithEmptyStringIsString.
    // Passes vacuously today (RC-PROLOG); without the prolog RC-EXPR (val/var
    // -> 99).
    {"ExpressionEndingWithOperatorAtEofIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val r = 1 +
)IFJ",
     2},
    // §1.2 (A5-136): a physical line inside parentheses is not a logical
    // line, so it may start with + (contrast with A5-122).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99),
    // then RC-PARENNL.
    {"PhysicalLineInsideParenthesesMayStartWithOperator",
     R"IFJ(@main def main(): Unit =
  val r = (1
      + 2)
  print(r)
)IFJ",
     0},
    // §1.2 (A5-137): the last expression of a body ends at EOF without
    // a line break.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var -> 99).
    {"LastExpressionMayEndAtEofWithoutNewline",
     R"IFJ(@main def main(): Unit =
  val r = f()
  print(r)

def f(): Int =
  1 * 2 + 3)IFJ",
     0},
};

class SpecExpressionsLinesAndParenthesesTest
    : public ::testing::TestWithParam<ExprCase> {};

TEST_P(SpecExpressionsLinesAndParenthesesTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecExpressions,
                         SpecExpressionsLinesAndParenthesesTest,
                         ::testing::ValuesIn(kLinesAndParenthesesCases),
                         expr_case_name);

} // namespace
