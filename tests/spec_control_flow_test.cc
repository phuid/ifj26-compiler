/*
 * AI-generated test block — shared table row and runner for control flow.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write black-box spec conformance tests for IFJ26 control flow (spec
 *   2.1, 2.3 rules 2-3, 2.4, 2.5 and FUNEXP 5.1 calls in conditions). Each
 *   case is a whole program body run through tests/spec_harness.hpp and
 *   checked against the exit code of spec chapter 6. Group the cases into
 *   TEST_P tables by topic, give every row a readable name and a comment
 *   with the spec rule and, when it fails today, the root cause.
 */
#include <string>

#include "gtest/gtest.h"

#include "spec_harness.hpp"

namespace {

/** One black-box program body with the exit code the spec requires. */
struct FlowCase {
  const char *name;
  const char *source;
  int expected;
};

/** Uses the row name as the gtest parameter name. */
std::string flow_case_name(const ::testing::TestParamInfo<FlowCase> &info) {
  return info.param.name;
}

/** Compiles the row's program and checks the chapter 6 exit code. */
void expect_spec_exit_code(const FlowCase &row) {
  // Arrange: the row holds the program body; the harness adds the prolog.
  const char *source = row.source;

  // Act
  int code = ifj26_exit_code(source);

  // Assert
  EXPECT_EQ(code, row.expected) << source;
}

} // namespace

/*
 * AI-generated test block — syntax of if/else (spec 1.1, 1.2, 2.1, 2.4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover the shape of the conditional statement: the spec 2.1 example,
 *   mandatory else (base language without BOOLTHEN), else-if only as a
 *   nested if, then/else placement on logical lines, if is not an
 *   expression, block indentation of each branch, blank lines, comments and
 *   semicolons inside branches. Every expected 2 has a positive control.
 */
namespace {

const FlowCase kIfSyntaxCases[] = {
    // §2.1, §2.4 (A4-001): the spec example, an empty branch written as ().
    // Positive control for the syntax errors below.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99).
    {"SpecExampleIfElseWithUnitBranch",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x < 5 then
    print("prilis malo")
  else
    ()
)IFJ",
     0},
    // §2.4 (A4-002): else is mandatory in the base language (only without
    // BOOLTHEN); if is the last statement. Control: A4-001.
    // Passes vacuously today (RC-PROLOG).
    {"MissingElseAtEndOfBodyIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x < 5 then
    print("a")
)IFJ",
     2},
    // §2.4 (A4-003): else is mandatory (only without BOOLTHEN); a statement
    // aligned with if is not an else. Control: A4-001.
    // Passes vacuously today (RC-PROLOG).
    {"MissingElseBeforeNextStatementIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x < 5 then
    print("a")
  print("b")
)IFJ",
     2},
    // §2.4, §1.2 (A4-004): else stands on its own logical line followed by
    // a line break; `else if` exists only with BOOLTHEN. Control: A4-005.
    // Passes vacuously today (RC-PROLOG).
    {"ElseIfOnOneLineIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x < 5 then
    print("a")
  else if x < 9 then
    print("b")
  else
    print("c")
)IFJ",
     2},
    // §2.4 (A4-005): an else block may hold another if (else-if ladder).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99).
    {"ElseIfLadderAsNestedIfIsAccepted",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x < 5 then
    print("a")
  else
    if x < 9 then
      print("b")
    else
      print("c")
)IFJ",
     0},
    // §2.4, §1.2 (A4-006): a line break outside parentheses ends the
    // logical line, so `if cond` without then is incomplete. Control: A4-001.
    // Passes vacuously today (RC-PROLOG).
    {"ThenOnNextLogicalLineIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x < 5
  then
    print("a")
  else
    ()
)IFJ",
     2},
    // §2.4 (A4-007): the grammar is `if condition then`. Control: A4-001.
    // Passes vacuously today (RC-PROLOG).
    {"DoInsteadOfThenIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x < 5 do
    print("a")
  else
    ()
)IFJ",
     2},
    // §2.4 (A4-008): one if has exactly one else; a second else has no if.
    // Control: A4-001. Passes vacuously today (RC-PROLOG).
    {"TwoElseBranchesAreSyntaxError",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x < 5 then
    print("a")
  else
    print("b")
  else
    print("c")
)IFJ",
     2},
    // §1.2, §2.4 (A4-009): a line break inside parentheses is white space
    // when the next line is indented more than the logical line.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99);
    // then RC-PARENNL.
    {"ParenthesizedConditionSpanningTwoLinesIsAccepted",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if (x <
      5) then
    print("a")
  else
    ()
)IFJ",
     0},
    // §2.4 (A4-011): if cannot be used as an expression (only with CONTROL).
    // Control: A4-001. Passes vacuously today (RC-PROLOG).
    {"IfAsValInitializerIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val x = 3
  val y = if x < 5 then 1 else 2
  print(y)
)IFJ",
     2},
    // §2.4 (A4-012): if is not an expression, so it is no return value.
    // Control: A4-090. Passes vacuously today (RC-PROLOG, RC-EXPR).
    {"IfAsReturnValueIsSyntaxError",
     R"IFJ(def f(x: Int): Int =
  return if x < 5 then 1 else 2
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     2},
    // §2.4, §2.3 (A4-013): a one-line body is an expression or a call, and
    // if is not an expression. Control: OneLineFunctionBodyIsAccepted.
    // Passes vacuously today (RC-PROLOG, RC-ONELINE).
    {"IfAsOneLineFunctionBodyIsSyntaxError",
     R"IFJ(def f(x: Int): Int = if x < 5 then 1 else 2
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     2},
    // §2.3 (A4-013 control): a one-line function body is an expression.
    // Fails today: RC-PROLOG (2); without the prolog RC-ONELINE (2).
    {"OneLineFunctionBodyIsAccepted",
     R"IFJ(def f(x: Int): Int = x + 1
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     0},
    // §1.2, §2.4 (A4-014): each branch only has to be indented more than its
    // header; the two branches need not match each other.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99).
    {"BranchesIndentedDifferentlyAreAccepted",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x < 5 then
    print("a")
  else
        print("b")
)IFJ",
     0},
    // §1.2 (A4-015): all statements of a block have the same indentation.
    // Control: A4-001. Passes vacuously today (RC-PROLOG).
    {"DeeperSecondStatementInBranchIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x < 5 then
    print("a")
      print("b")
  else
    ()
)IFJ",
     2},
    // §1.2, §2.4 (A4-017): empty and whitespace-only lines are ignored and
    // do not change the indentation (plain literal keeps trailing spaces).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99).
    {"BlankAndWhitespaceLinesAroundBranchesAreIgnored",
     "@main def main(): Unit =\n"
     "  val x = 3\n"
     "  if x < 5 then\n"
     "\n"
     "    print(\"a\")\n"
     "      \n"
     "\n"
     "  else\n"
     "   \n"
     "    print(\"b\")\n",
     0},
    // §2.4, §1.2 (A4-018): else aligned with the outer if after a dedent
    // over two levels closing a nested if in the then branch.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99).
    {"NestedIfInThenBranchThenOuterElseIsAccepted",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x < 5 then
    if x < 2 then
      print("a")
    else
      print("b")
  else
    print("c")
)IFJ",
     0},
    // §2.4, §1.2 (A4-019): no separator is needed after the last statement
    // of a block, before the next definition or before the end of file.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2).
    {"FinalIfBeforeNextDefinitionAndAtEofWithoutNewline",
     R"IFJ(def f(x: Int): Unit =
  if x < 5 then
    print("a")
  else
    print("b")
@main def main(): Unit =
  f(3)
  if 1 < 2 then
    ()
  else
    ())IFJ",
     0},
    // §1.2 (A4-020): statements on one logical line are separated by `;`.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99);
    // then RC-SEMI.
    {"SemicolonSeparatedStatementsInBranchAreAccepted",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x < 5 then
    print("a"); print("b")
  else
    print("c")
)IFJ",
     0},
    // §1.1, §1.2 (A4-021): a line comment counts as the end of the line,
    // also after then and else.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99);
    // then RC-COMMENT.
    {"LineCommentAfterThenAndElseIsAccepted",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x < 5 then // mala
    print("a")
  else // velka
    print("b")
)IFJ",
     0},
};

class SpecControlFlowIfSyntaxTest : public ::testing::TestWithParam<FlowCase> {
};

TEST_P(SpecControlFlowIfSyntaxTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecControlFlow, SpecControlFlowIfSyntaxTest,
                         ::testing::ValuesIn(kIfSyntaxCases), flow_case_name);

} // namespace

/*
 * AI-generated test block — conditions of if and while (spec 2.4, 2.5, 5.1).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   A condition must be a relational expression of type Boolean: Int,
 *   String and Unit conditions are error 6, an undefined variable in it is
 *   error 3, parentheses keep it relational. With FUNEXP a call may appear
 *   in a condition: inside a relation it is fine, a call returning Int or
 *   Unit as the whole condition of if or while is error 6.
 *   A non-relational condition is a type error (chapter 6, code 6: type
 *   compatibility in a control construct), not a syntax error.
 */
namespace {

const FlowCase kConditionCases[] = {
    // §2.4 (A4-030): the condition must be Boolean; an Int variable -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99).
    {"IntVariableConditionIsTypeError",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x then
    print("a")
  else
    ()
)IFJ",
     6},
    // §2.4 (A4-031): an arithmetic Int expression is no condition -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99).
    {"ArithmeticConditionIsTypeError",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if x + 1 then
    print("a")
  else
    ()
)IFJ",
     6},
    // §2.4 (A4-032): a String variable is no condition -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99).
    {"StringVariableConditionIsTypeError",
     R"IFJ(@main def main(): Unit =
  val s = "a"
  if s then
    print("a")
  else
    ()
)IFJ",
     6},
    // §2.4 (A4-033): the Unit literal () is no condition -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2).
    {"UnitLiteralConditionIsTypeError",
     R"IFJ(@main def main(): Unit =
  if () then
    print("a")
  else
    ()
)IFJ",
     6},
    // §2.4, §2.2 (A4-037): an undefined variable in the condition -> 3.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2); then RC-PSTATUS (-3).
    {"UndefinedVariableInConditionIsError3",
     R"IFJ(@main def main(): Unit =
  if y < 5 then
    print("a")
  else
    ()
)IFJ",
     3},
    // §2.4, §3 (A4-038): a relational expression in parentheses is still a
    // relational condition of type Boolean.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99).
    {"ParenthesizedRelationalConditionIsAccepted",
     R"IFJ(@main def main(): Unit =
  val x = 3
  if (x < 5) then
    print("a")
  else
    ()
)IFJ",
     0},
    // §2.4, §5.1 FUNEXP (revB B.1): a builtin call inside a relational
    // condition gives a Boolean condition.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99).
    {"BuiltinCallInsideRelationalConditionIsAccepted",
     R"IFJ(@main def main(): Unit =
  val s = "ab"
  if strLength(s) > 1 then
    print("a")
  else
    ()
)IFJ",
     0},
    // §2.5, §5.1 FUNEXP (revB B.1): readInt() is of type Int, so as the
    // whole while condition it is a type error -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-WHILE (2); then
    // RC-EXPR.
    {"IntCallAsWhileConditionIsTypeError",
     R"IFJ(@main def main(): Unit =
  while readInt() do
    print("a")
)IFJ",
     6},
    // §2.4, §5.1 FUNEXP (revB B.1): print returns Unit, so as the whole if
    // condition it is a type error -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2).
    {"UnitCallAsIfConditionIsTypeError",
     R"IFJ(@main def main(): Unit =
  if print("a") then
    print("b")
  else
    ()
)IFJ",
     6},
};

class SpecControlFlowConditionTest : public ::testing::TestWithParam<FlowCase> {
};

TEST_P(SpecControlFlowConditionTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecControlFlow, SpecControlFlowConditionTest,
                         ::testing::ValuesIn(kConditionCases), flow_case_name);

} // namespace

/*
 * AI-generated test block — while loop (spec 2.3 rules 1-3, 2.5).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover the while statement: the minimal loop as positive control, Int,
 *   String and undefined conditions, then/else misuse, do on the next line,
 *   nesting with if, and the result rules: a block ending with a loop is
 *   Unit even when the loop body ends with return or a value, rule 3 does
 *   not apply to an if inside a loop body, return in a loop needs an
 *   explicit return type.
 */
namespace {

const FlowCase kWhileCases[] = {
    // §2.5 (A4-060): `while condition do` and a block, then a statement.
    // Positive control for the syntax errors below.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (var -> 99);
    // then RC-WHILE.
    {"MinimalWhileFollowedByStatementIsAccepted",
     R"IFJ(@main def main(): Unit =
  var i = 0
  while i < 3 do
    i = i + 1
  print(i)
)IFJ",
     0},
    // §2.5 (A4-061): the loop condition is relational; an Int variable -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (var -> 99);
    // then RC-WHILE.
    {"IntVariableWhileConditionIsTypeError",
     R"IFJ(@main def main(): Unit =
  var i = 0
  while i do
    i = i + 1
)IFJ",
     6},
    // §2.5 (A4-062): a String variable as the loop condition -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99);
    // then RC-WHILE.
    {"StringVariableWhileConditionIsTypeError",
     R"IFJ(@main def main(): Unit =
  val s = "a"
  while s do
    print(s)
)IFJ",
     6},
    // §2.5, §2.2 (A4-063): an undefined variable in the loop condition -> 3.
    // Fails today: RC-PROLOG (2); without the prolog RC-WHILE (2); then
    // RC-PSTATUS (-3).
    {"UndefinedVariableInWhileConditionIsError3",
     R"IFJ(@main def main(): Unit =
  while j < 3 do
    print("a")
)IFJ",
     3},
    // §2.5 (A4-067): the grammar is `while condition do`. Control: A4-060.
    // Passes vacuously today (RC-PROLOG, RC-WHILE).
    {"ThenInsteadOfDoIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  var i = 0
  while i < 3 then
    i = i + 1
)IFJ",
     2},
    // §2.5 (A4-068): a loop has no else branch. Control: A4-060.
    // Passes vacuously today (RC-PROLOG, RC-WHILE).
    {"WhileWithElseIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  var i = 0
  while i < 3 do
    i = i + 1
  else
    ()
)IFJ",
     2},
    // §2.4, §2.5 (A4-069): arbitrary nesting, a dedent over two levels
    // before the outer else.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (var -> 99);
    // then RC-WHILE.
    {"IfInsideWhileInsideIfIsAccepted",
     R"IFJ(@main def main(): Unit =
  var i = 0
  if i < 1 then
    while i < 3 do
      if i < 2 then
        print("a")
      else
        print("b")
      i = i + 1
  else
    ()
  print(i)
)IFJ",
     0},
    // §2.3 rule 2, §2.5 (A4-071): a body ending with a loop is Unit even
    // when the loop body ends with return; rule 3 applies only to if -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-WHILE (2).
    {"IntFunctionEndingWithWhileEndingWithReturnIsTypeError",
     R"IFJ(def f(n: Int): Int =
  while n > 0 do
    return n
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     6},
    // §2.3 rule 2, §2.5 (A4-072): an unannotated function ending with a loop
    // infers Unit, so `val r: Int = f(3)` -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (var -> 99);
    // then RC-WHILE.
    {"UnannotatedFunctionEndingWithWhileIsUnitNotInt",
     R"IFJ(def f(n: Int) =
  var i = n
  while i > 0 do
    i = i - 1
@main def main(): Unit =
  val r: Int = f(3)
  print(r)
)IFJ",
     6},
    // §2.3 rule 2, §2.5 (A4-073): an unannotated function ending with a loop
    // infers Unit; a Unit variable is allowed (§1.3).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (var -> 99);
    // then RC-WHILE.
    {"UnannotatedFunctionEndingWithWhileInfersUnit",
     R"IFJ(def f(n: Int) =
  var i = n
  while i > 0 do
    i = i - 1
@main def main(): Unit =
  val r: Unit = f(3)
  print(r)
)IFJ",
     0},
    // §2.3 rule 2 (A4-074): values of the loop body do not become the
    // result of the enclosing block, so f is Unit and `r + 1` -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (var -> 99);
    // then RC-WHILE.
    {"LoopBodyValueIsNotFunctionResult",
     R"IFJ(def f(n: Int) =
  var i = n
  while i > 0 do
    i = i - 1
    5
@main def main(): Unit =
  val r = f(3)
  val z = r + 1
  print(z)
)IFJ",
     6},
    // §2.3 rules 2 and 4, §2.5 (A4-075): an explicit Unit function may end
    // with a loop.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (var -> 99);
    // then RC-WHILE.
    {"ExplicitUnitFunctionEndingWithWhileIsAccepted",
     R"IFJ(def f(n: Int): Unit =
  var i = n
  while i > 0 do
    i = i - 1
@main def main(): Unit =
  f(3)
)IFJ",
     0},
    // §2.3 rule 3, §2.5 (A4-076): rule 3 is about the end of a function
    // body only; an if inside a loop body may have value branches.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (var -> 99);
    // then RC-WHILE.
    {"IfWithValueBranchesInsideLoopBodyIsAccepted",
     R"IFJ(def count(n: Int): Int =
  var i = 0
  while i < n do
    i = i + 1
    if i < 2 then
      1
    else
      2
  i
@main def main(): Unit =
  val r = count(3)
  print(r)
)IFJ",
     0},
    // §2.3, §2.5 (A4-078): return anywhere in the body, also inside a loop,
    // requires an explicit return type -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (var -> 99);
    // then RC-WHILE.
    {"ReturnInsideLoopOfUnannotatedFunctionIsTypeError",
     R"IFJ(def f(n: Int) =
  var i = n
  while i > 0 do
    return i
@main def main(): Unit =
  f(3)
)IFJ",
     6},
    // §2.5, §1.2 (A4-079): a line break ends the logical line, so
    // `while i < 3` without do is incomplete. Control: A4-060.
    // Passes vacuously today (RC-PROLOG, RC-WHILE).
    {"DoOnNextLogicalLineIsSyntaxError",
     R"IFJ(@main def main(): Unit =
  var i = 0
  while i < 3
  do
    i = i + 1
)IFJ",
     2},
};

class SpecControlFlowWhileTest : public ::testing::TestWithParam<FlowCase> {};

TEST_P(SpecControlFlowWhileTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecControlFlow, SpecControlFlowWhileTest,
                         ::testing::ValuesIn(kWhileCases), flow_case_name);

} // namespace

/*
 * AI-generated test block — block result and if at the end of a body (2.3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover spec 2.3 rules 2-4 for a function body that ends with if: all
 *   branches return, value branches in Int and unannotated functions,
 *   explicit Unit discarding values, nested ifs checked recursively,
 *   unreachable code after return, an if in the middle of the body, loops
 *   and definitions or assignments ending a branch, incompatible returns.
 */
namespace {

const FlowCase kBlockResultCases[] = {
    // §2.3 rule 3 (A4-090): an Int function may end with if when all
    // branches end with return. Positive control for the rows below.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2).
    {"IntFunctionEndingWithIfWhoseBranchesReturnIsAccepted",
     R"IFJ(def f(x: Int): Int =
  if x < 5 then
    return 1
  else
    return 2
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     0},
    // §2.3 rule 3, §2.4 (A4-091): an Int function ending with if whose
    // branches end with values matches no bullet of rule 3 -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2).
    {"IntFunctionEndingWithIfWithValueBranchesIsTypeError",
     R"IFJ(def f(x: Int): Int =
  if x < 5 then
    1
  else
    2
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     6},
    // §2.3 rule 3 (A4-092): not all branches end with return -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2).
    {"IntFunctionEndingWithIfWithOneValueBranchIsTypeError",
     R"IFJ(def f(x: Int): Int =
  if x < 5 then
    return 1
  else
    2
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     6},
    // §2.3 rule 3 (A4-094): no return type and Int branches -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2).
    {"UnannotatedFunctionEndingWithIntBranchesIsTypeError",
     R"IFJ(def f(x: Int) =
  if x < 5 then
    1
  else
    2
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     6},
    // §2.3 rules 3 and 4 (A4-095): an explicit Unit function discards the
    // Int and String values of its final if.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2).
    {"ExplicitUnitFunctionDiscardsBranchValues",
     R"IFJ(def f(x: Int): Unit =
  if x < 5 then
    1
  else
    "s"
@main def main(): Unit =
  f(3)
)IFJ",
     0},
    // §2.3 rule 3 (A4-096): a branch ending with another if is checked
    // recursively; here every leaf returns.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2).
    {"NestedIfInThenBranchAllReturnIsAccepted",
     R"IFJ(def f(x: Int): Int =
  if x < 5 then
    if x < 2 then
      return 1
    else
      return 2
  else
    return 3
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     0},
    // §2.3 rule 3 (A4-097): recursively, the inner else ends with a value,
    // not with return -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2).
    {"NestedIfInThenBranchWithValueLeafIsTypeError",
     R"IFJ(def f(x: Int): Int =
  if x < 5 then
    if x < 2 then
      return 1
    else
      2
  else
    return 3
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     6},
    // §2.3 rule 3 (A4-098): the last statement of the branch is (), not
    // return; checks apply to unreachable code too -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2).
    {"BranchEndingWithUnreachableUnitAfterReturnIsTypeError",
     R"IFJ(def f(x: Int): Int =
  if x < 5 then
    return 1
    ()
  else
    return 2
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     6},
    // §2.3 (A4-099): only the last statement written directly in the body
    // decides the result; an if in the middle may mix branch values.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2).
    {"NonFinalIfWithMixedBranchValuesIsAccepted",
     R"IFJ(def f(x: Int): Int =
  if x < 5 then
    1
  else
    "s"
  x
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     0},
    // §2.3 rules 2-3 (A4-101): the then branch ends with a loop (Unit) and
    // not with return in an Int function -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (var -> 99);
    // then RC-WHILE.
    {"IntFunctionThenBranchEndingWithWhileIsTypeError",
     R"IFJ(def f(x: Int): Int =
  var i = x
  if x < 5 then
    while i > 0 do
      i = i - 1
  else
    return 2
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     6},
    // §2.3 rules 2-3 (A4-102): both branches are Unit (a loop, a print
    // call), so the unannotated function infers Unit.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (var -> 99);
    // then RC-WHILE.
    {"UnannotatedFunctionWithLoopAndPrintBranchesInfersUnit",
     R"IFJ(def f(x: Int) =
  var i = x
  if x < 5 then
    while i > 0 do
      i = i - 1
  else
    print("b")
@main def main(): Unit =
  val r: Unit = f(3)
  print(r)
)IFJ",
     0},
    // §2.3 rules 3-4 (A4-103): every returned value must be compatible with
    // the Int return type; "s" is not -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (condition ->
    // 2).
    {"IntFunctionReturningStringInBranchIsTypeError",
     R"IFJ(def f(x: Int): Int =
  if x < 5 then
    return 1
  else
    return "s"
@main def main(): Unit =
  val r = f(3)
  print(r)
)IFJ",
     6},
    // §2.3 rules 2-3 (A4-104): branches ending with a definition and an
    // assignment are Unit, so the unannotated function infers Unit.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (var -> 99).
    {"BranchesEndingWithDefinitionAndAssignmentInferUnit",
     R"IFJ(def f(x: Int) =
  var k = 0
  if x < 5 then
    val y = 1
  else
    k = 2
@main def main(): Unit =
  val r: Unit = f(3)
  print(r)
)IFJ",
     0},
};

class SpecControlFlowBlockResultTest
    : public ::testing::TestWithParam<FlowCase> {};

TEST_P(SpecControlFlowBlockResultTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecControlFlow, SpecControlFlowBlockResultTest,
                         ::testing::ValuesIn(kBlockResultCases),
                         flow_case_name);

} // namespace
