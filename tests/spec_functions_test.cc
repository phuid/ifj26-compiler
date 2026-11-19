/*
 * AI-generated test block — black-box spec tests of IFJ26 functions
 *   (spec chapter 2 and 2.3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write black-box conformance tests for IFJ26 functions from the
 *   approved catalog A3: the entry function @main, the shape of a
 *   definition, parameters, return type and value, recursion, calls and
 *   arguments. Group the programs in TEST_P tables by topic, give every
 *   case a readable name and a spec section comment, and mark cases that
 *   fail today with their root cause. Failing tests stay enabled.
 */

#include <ostream>
#include <string>

#include "gtest/gtest.h"

#include "spec_harness.h"

namespace {

/** One whole IFJ26 program (without the prolog) and its expected exit code. */
struct FunctionCase {
  const char *name;
  const char *source;
  int expected;
};

/** Prints a row by its name instead of a byte dump in failure messages. */
void PrintTo(const FunctionCase &row, std::ostream *out) { *out << row.name; }

/** Gives every table row a readable gtest name. */
std::string case_name(const ::testing::TestParamInfo<FunctionCase> &info) {
  return std::string(info.param.name);
}

/** Runs the program of one row and checks its exit code. */
void expect_exit_code(const FunctionCase &row) {
  // Act
  const int code = ifj26_exit_code(row.source);
  // Assert
  EXPECT_EQ(code, row.expected) << row.source;
}

} // namespace

/*
 * AI-generated test block — the factorial example program of spec
 *   chapter 2.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Run the factorial example of spec chapter 2 verbatim (comments in
 *   ASCII) through the whole front end and expect exit code 0.
 */

// §2: the factorial example program of the spec is valid (comments
//   rewritten to ASCII) [A3-001].
// Fails today: RC-PROLOG, the prolog import is a syntax error (2);
//   without the prolog RC-EXPR (expression parser stub)
//   gives 99, then RC-WHILE, RC-COMMENT.
TEST(SpecFunctionsExampleTest, SpecFactorialExampleVerbatim) {
  // Arrange
  const char *source = R"(def factorial(n: Int): Int =
  var result = 1
  var i: Int = n // uvedeni typu je nepovinne
  while i > 1 do
    result = result * i
    i = i - 1
  result // posledni vyraz v tele funkce urci navratovou hodnotu

@main def runFoo(): Unit =
  val n = readInt()
  val result = factorial(n)
  print(result)
  print("\n")
)";
  // Act
  const int code = ifj26_exit_code(source);
  // Assert
  EXPECT_EQ(code, 0) << source;
}

/*
 * AI-generated test block — the entry function (@main): name, count,
 *   parameters and Unit result.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of spec chapter 2 entry function rules: any name, a
 *   missing or second @main, parameters, a non Unit result, discarded
 *   return values and recursion of @main.
 */

namespace {

const FunctionCase kMainCases[] = {
    // §2: the entry function may have any valid identifier as its name
    //   [A3-005].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"EntryFunctionNameIsAnyIdentifier",
     R"(@main def _Start_2(): Unit =
  return
)",
     0},
    // §2: a missing entry function is error 3; the name main alone is not
    //   enough [A3-006].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"MissingMainIsError3",
     R"(def main(): Unit =
  return
)",
     3},
    // §2: more than one entry function is error 4 [A3-007].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"TwoMainFunctionsAreError4",
     R"(@main def first(): Unit =
  return
@main def second(): Unit =
  return
)",
     4},
    // §2, ch. 6: a parameter of the @main function is error 5 [A3-008].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"MainWithParameterIsError5",
     R"(@main def run(x: Int): Unit =
  return
)",
     5},
    // §2: an @main return type other than Unit is error 6 [A3-009].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"MainWithIntReturnTypeIsError6",
     R"(@main def run(): Int =
  return 0
)",
     6},
    // §2, §2.3: an @main without annotation whose body infers Int is error
    //   6 [A3-010].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"MainWithInferredIntIsError6",
     R"(@main def run() =
  1
)",
     6},
    // §2, §2.3: an explicit Unit @main discards the value of return (rule
    //   4) [A3-012].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"UnitMainDiscardsReturnValue",
     R"(@main def run(): Unit =
  return 1
)",
     0},
    // §2, §2.3: a directly recursive @main must state its return type, else
    //   error 6 [A3-013].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"RecursiveMainWithoutTypeIsError6",
     R"(@main def run() =
  run()
)",
     6},
    // §2.3, §2: a plain function with the name of the entry function is a
    //   redefinition, error 4 [A3-030].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"FunctionNamedLikeMainFunctionIsError4",
     R"(@main def run(): Unit =
  return
def run(): Unit =
  return
)",
     4},
};

} // namespace

class SpecFunctionsMainTest : public ::testing::TestWithParam<FunctionCase> {};

TEST_P(SpecFunctionsMainTest, ExitCodeFollowsSpec) {
  expect_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecFunctions, SpecFunctionsMainTest,
                         ::testing::ValuesIn(kMainCases), case_name);

/*
 * AI-generated test block — the shape of a function definition: one-line
 *   and multi-line forms.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of spec 2.3 definition forms: one-line bodies (call,
 *   expression, inferred type, @main), bodies that are not an expression
 *   or call, headers over two lines and overloading.
 */

namespace {

const FunctionCase kDefinitionCases[] = {
    // §2.3: a one-line function body may be a call [A3-014].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-ONELINE (one-line body is not
    //   parsed) gives 2.
    {"OneLineFunctionWithCallBody",
     R"(def greet(): Unit = print("hi")
@main def run(): Unit =
  greet()
)",
     0},
    // §2.3: a one-line function body may be an expression; its value is the
    //   result [A3-015].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-ONELINE (one-line body is not
    //   parsed) gives 2.
    {"OneLineFunctionWithConcatenationBody",
     R"(def greet(name: String): String = "hi " + name
@main def run(): Unit =
  val s = greet("bob")
  print(s)
)",
     0},
    // §2.3, §2: the one-line form is allowed for the @main function too
    //   [A3-016].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-ONELINE (one-line body is not
    //   parsed) gives 2.
    {"OneLineMainFunction",
     R"(@main def run(): Unit = print("x")
)",
     0},
    // §2.3: a one-line function may omit the return type; it is inferred
    //   (Int) [A3-017].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-ONELINE (one-line body is not
    //   parsed) gives 2.
    {"OneLineFunctionInfersReturnType",
     R"(def answer() = 42
@main def run(): Unit =
  val a: Int = answer()
  print(a)
)",
     0},
    // §2.3: a one-line body is an expression or call only; a definition is
    //   error 2 [A3-018].
    // Passes today only by accident: RC-PROLOG gives the expected 2.
    //   Without the prolog RC-ONELINE gives 2 as well;
    //   OneLineFunctionWithCallBody is the positive control.
    {"OneLineBodyCannotBeDefinition",
     R"(def f(): Unit = val x = 1
@main def run(): Unit =
  f()
)",
     2},
    // §2.3: a one-line body is an expression or call only; a def is error 2
    //   [A3-076].
    // Passes today only by accident: RC-PROLOG gives the expected 2.
    //   Without the prolog RC-ONELINE gives 99; OneLineFunctionWithCallBody
    //   is the positive control.
    {"DefinitionAsOneLineBodyIsError2",
     R"(def f() = def g(): Unit =
  return
@main def run(): Unit =
  return
)",
     2},
    // §1.2, §2.3: a header may span two physical lines inside parentheses;
    //   the body indent is measured from the first one [A3-077].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"HeaderSpanningTwoPhysicalLines",
     R"(def f(a: Int,
      b: Int): Unit =
  return
@main def run(): Unit =
  return
)",
     0},
    // §2.3: overloading is not supported; two functions with one name are
    //   error 4 [A3-029].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"OverloadingByParameterTypeIsError4",
     R"(def f(a: Int): Unit =
  return
def f(a: String): Unit =
  return
@main def run(): Unit =
  return
)",
     4},
};

} // namespace

class SpecFunctionsDefinitionTest
    : public ::testing::TestWithParam<FunctionCase> {};

TEST_P(SpecFunctionsDefinitionTest, ExitCodeFollowsSpec) {
  expect_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecFunctions, SpecFunctionsDefinitionTest,
                         ::testing::ValuesIn(kDefinitionCases), case_name);

/*
 * AI-generated test block — function parameters: types, trailing comma,
 *   immutability, uniqueness.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of spec 2.3 parameters: several parameters, every base
 *   type, a trailing comma, assignment to a parameter (error 7) and
 *   duplicate parameter names (error 4).
 */

namespace {

const FunctionCase kParameterCases[] = {
    // §2.3: multi-line form with two comma separated id : type parameters
    //   [A3-019].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"MultiLineFunctionWithTwoParameters",
     R"(def show(a: Int, b: String): Unit =
  print(a)
  print(b)
@main def run(): Unit =
  show(1, "x")
)",
     0},
    // §2.3, §1.3: parameters may have every base type including Unit
    //   [A3-020].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"ParametersOfEveryBaseType",
     R"(def f(a: Int, b: Double, c: String, d: Unit): Unit =
  return
@main def run(): Unit =
  f(1, 2.5, "s", ())
)",
     0},
    // §2.3, §1.2: a trailing comma before ) on its own line is allowed in
    //   parameters [A3-021].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"MultiLineParametersWithTrailingComma",
     R"(def f(
  a: Int,
  b: Int,
  ): Unit =
  return
@main def run(): Unit =
  return
)",
     0},
    // §2.3, ch. 6: parameters are immutable; assignment in a nested branch
    //   is error 7 [A3-024].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2, then RC-ASSIGN.
    {"AssignmentToParameterInBranchIsError7",
     R"(def f(n: Int): Unit =
  if n > 0 then
    n = 0
  else
    ()
@main def run(): Unit =
  return
)",
     7},
    // §2.3, ch. 6: checks hold in unreachable code; assignment to a
    //   parameter after return is error 7 [A3-025].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1), then RC-ASSIGN.
    {"AssignmentToParameterAfterReturnIsError7",
     R"(def f(n: Int): Unit =
  return
  n = 0
@main def run(): Unit =
  return
)",
     7},
    // §2.3, §2.2: two parameters with the same name are a redefinition,
    //   error 4 [A3-078].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"DuplicateParameterNameIsError4",
     R"(def f(a: Int, a: String): Unit =
  return
@main def run(): Unit =
  return
)",
     4},
};

} // namespace

class SpecFunctionsParameterTest
    : public ::testing::TestWithParam<FunctionCase> {};

TEST_P(SpecFunctionsParameterTest, ExitCodeFollowsSpec) {
  expect_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecFunctions, SpecFunctionsParameterTest,
                         ::testing::ValuesIn(kParameterCases), case_name);

/*
 * AI-generated test block — return type and return value rules 1-4 of
 *   spec 2.3.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of spec 2.3 return rules: inferred types, bare return,
 *   the result of a block, explicit Unit discarding values, compatibility
 *   of returned values and checks in unreachable code.
 */

namespace {

const FunctionCase kReturnCases[] = {
    // §2.3: a not annotated function ending with an expression infers its
    //   type (Int) [A3-027].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 99.
    {"NotAnnotatedFunctionEndingWithExpression",
     R"(def f() =
  val a = 1
  a + 1
@main def run(): Unit =
  val x: Int = f()
  print(x)
)",
     0},
    // §2.3, §2.2: an inferred String result assigned to an Int variable is
    //   error 6 [A3-028].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"InferredResultIncompatibleWithAnnotationIsError6",
     R"(def f() =
  "s"
@main def run(): Unit =
  val x: Int = f()
  print(x)
)",
     6},
    // §2.3: a return anywhere in the body requires the return type, else
    //   error 6 [A3-037]. Chapter 6 lists a forbidden return under 10, but
    //   §2.3 names error 6 for exactly these rules, so 6 is expected.
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"BareReturnWithoutReturnTypeIsError6",
     R"(def f() =
  return
@main def run(): Unit =
  return
)",
     6},
    // §2.3: a nested return in the body requires the return type, else
    //   error 6 [A3-038] (§2.3 names 6, not the generic 10 of chapter 6).
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"NestedReturnWithoutReturnTypeIsError6",
     R"(def f(x: Int) =
  if x > 0 then
    return 1
  else
    return 2
@main def run(): Unit =
  return
)",
     6},
    // §2.3: a bare return is allowed only with an explicit Unit return
    //   type, else error 6 [A3-039] (§2.3 names 6, not the generic 10).
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"BareReturnInIntFunctionIsError6",
     R"(def f(): Int =
  return
@main def run(): Unit =
  return
)",
     6},
    // §2.3: a bare return from a nested branch of an explicit Unit function
    //   is valid [A3-040].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"BareReturnInBranchOfUnitFunction",
     R"(def f(x: Int): Unit =
  if x > 0 then
    return
  else
    print(x)
@main def run(): Unit =
  return
)",
     0},
    // §2.3: a block ending with an expression has its value (rule 2)
    //   [A3-041].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 99.
    {"BlockEndingWithExpressionGivesValue",
     R"(def f(): Int =
  val a = 1
  a
@main def run(): Unit =
  val x = f()
  print(x)
)",
     0},
    // §2.3: a body ending with a definition yields () and is incompatible
    //   with Int, error 6 [A3-042].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 99.
    {"IntBodyEndingWithDefinitionIsError6",
     R"(def f(): Int =
  val a = 1
@main def run(): Unit =
  return
)",
     6},
    // §2.3: a body ending with an assignment yields () and is incompatible
    //   with Int, error 6 [A3-043].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 99, then RC-ASSIGN.
    {"IntBodyEndingWithAssignmentIsError6",
     R"(def f(): Int =
  var a = 1
  a = 2
@main def run(): Unit =
  return
)",
     6},
    // §2.3, §1.3: a not annotated body ending with an assignment infers
    //   Unit [A3-045].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 99, then RC-ASSIGN.
    {"NotAnnotatedBodyEndingWithAssignmentInfersUnit",
     R"(def f() =
  var a = 1
  a = 2
@main def run(): Unit =
  val u: Unit = f()
  print(u)
)",
     0},
    // §2.3: an explicit Unit function discards the value of return (rule 4)
    //   [A3-046].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"UnitFunctionDiscardsReturnValue",
     R"(def f(): Unit =
  return 5
@main def run(): Unit =
  return
)",
     0},
    // §2.3: an explicit Unit function discards the value of its last
    //   expression (rule 4) [A3-047].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"UnitFunctionDiscardsLastValue",
     R"(def f(): Unit =
  "ignored"
@main def run(): Unit =
  return
)",
     0},
    // §2.3, §3: a discarded return value is still type checked; Int +
    //   String is error 6 [A3-048].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"UnitFunctionStillChecksDiscardedValue",
     R"(def f(): Unit =
  return 1 + "a"
@main def run(): Unit =
  return
)",
     6},
    // §2.3: a discarded return value is still checked; a call with a wrong
    //   argument count is error 5 [A3-049].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"UnitFunctionStillChecksDiscardedCall",
     R"(def g(): Int =
  return 1
def f(): Unit =
  return g(1)
@main def run(): Unit =
  return
)",
     5},
    // §2.3: every returned value must match the return type; String in Int
    //   is error 6 [A3-050].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"ReturnOfStringInIntFunctionIsError6",
     R"(def f(): Int =
  return "s"
@main def run(): Unit =
  return
)",
     6},
    // §2.3, §1.3: a returned Int widens to a Double return type [A3-051].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"ReturnOfIntInDoubleFunction",
     R"(def f(): Double =
  return 1
@main def run(): Unit =
  val d = f()
  print(d)
)",
     0},
    // §2.3, §1.3: a last value of type Double in an Int function is error 6
    //   [A3-052].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"DoubleLastValueInIntFunctionIsError6",
     R"(def f(): Int =
  1.5
@main def run(): Unit =
  return
)",
     6},
    // §2.3: checks hold in unreachable code; an incompatible return after
    //   return is error 6 [A3-053].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"UnreachableIncompatibleReturnIsError6",
     R"(def f(): Int =
  return 1
  return "s"
@main def run(): Unit =
  return
)",
     6},
    // §2.3: an unreachable last statement of type Unit is still the
    //   implicit value, error 6 [A3-054].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"UnreachableUnitLastStatementIsError6",
     R"(def f(): Int =
  return 1
  print("x")
@main def run(): Unit =
  return
)",
     6},
    // §2.3, §2.2: checks hold in unreachable code; an undefined variable
    //   there is error 3 [A3-055]. The function is Unit so that the
    //   unreachable statement adds no other error.
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a term
    //   crashes) gives a crash (-1).
    {"UnreachableUndefinedVariableIsError3",
     R"(def f(): Unit =
  return
  print(y)
@main def run(): Unit =
  return
)",
     3},
    // §2.3: return ends the function from a branch nested in a loop
    //   [A3-056].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 99, then RC-WHILE.
    {"ReturnFromBranchInsideLoop",
     R"(def find(n: Int): Int =
  var i = 0
  while i < n do
    if i == 2 then
      return i
    else
      i = i + 1
  return -1
@main def run(): Unit =
  val r = find(5)
  print(r)
)",
     0},
    // §2.3: a String returned from a branch inside a loop of an Int
    //   function is error 6 [A3-057].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 99, then RC-WHILE.
    {"IncompatibleReturnInsideLoopIsError6",
     R"(def find(n: Int): Int =
  var i = 0
  while i < n do
    if i == 2 then
      return "two"
    else
      i = i + 1
  return -1
@main def run(): Unit =
  return
)",
     6},
};

} // namespace

class SpecFunctionsReturnTest : public ::testing::TestWithParam<FunctionCase> {
};

TEST_P(SpecFunctionsReturnTest, ExitCodeFollowsSpec) {
  expect_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecFunctions, SpecFunctionsReturnTest,
                         ::testing::ValuesIn(kReturnCases), case_name);

/*
 * AI-generated test block — direct, indirect and mutual recursion.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of spec 2.3 recursion: mutual recursion with types,
 *   direct and indirect recursion without a return type (error 6) and a
 *   not annotated caller of a recursive function.
 */

namespace {

const FunctionCase kRecursionCases[] = {
    // §2.3: mutual recursion is supported when both return types are stated
    //   [A3-033].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"MutualRecursionWithTypes",
     R"(def even(n: Int): Int =
  if n == 0 then
    return 1
  else
    val m = n - 1
    return odd(m)
def odd(n: Int): Int =
  if n == 0 then
    return 0
  else
    val m = n - 1
    return even(m)
@main def run(): Unit =
  val r = even(4)
  print(r)
)",
     0},
    // §2.3: indirect recursion requires the return type of every function
    //   on the cycle, else error 6 [A3-034].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"IndirectRecursionWithoutTypeIsError6",
     R"(def ping(n: Int): Unit =
  pong(n)
def pong(n: Int) =
  ping(n)
@main def run(): Unit =
  return
)",
     6},
    // §2.3: direct recursion without a return type is error 6 [A3-035].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"DirectRecursionWithoutTypeIsError6",
     R"(def loop(n: Int) =
  loop(n)
@main def run(): Unit =
  return
)",
     6},
    // §2.3: direct recursion with a stated return type is valid [A3-036].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"DirectRecursionWithType",
     R"(def loop(n: Int): Int =
  loop(n)
@main def run(): Unit =
  return
)",
     0},
    // §2.3: a not annotated caller of a recursive function is not on the
    //   cycle; its type is inferred [A3-080].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"NotAnnotatedCallerOfRecursiveFunction",
     R"(def fact(n: Int): Int =
  if n < 2 then
    return 1
  else
    val m = n - 1
    val r = fact(m)
    return n * r
def wrapper() =
  fact(5)
@main def run(): Unit =
  val x: Int = wrapper()
  print(x)
)",
     0},
};

} // namespace

class SpecFunctionsRecursionTest
    : public ::testing::TestWithParam<FunctionCase> {};

TEST_P(SpecFunctionsRecursionTest, ExitCodeFollowsSpec) {
  expect_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecFunctions, SpecFunctionsRecursionTest,
                         ::testing::ValuesIn(kRecursionCases), case_name);

/*
 * AI-generated test block — calls and arguments: forward calls, terms,
 *   argument count and types, call positions.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of spec 2.3 calls: calls before the definition,
 *   variables as arguments, wrong argument count or type (error 5), Int to
 *   Double widening and calls as assignment, return value, last statement
 *   and statement.
 */

namespace {

const FunctionCase kCallCases[] = {
    // §2.3: a function may be called before its lexical definition
    //   [A3-031].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"CallBeforeDefinitionWithArgument",
     R"(@main def run(): Unit =
  later(1)
def later(x: Int): Unit =
  print(x)
)",
     0},
    // §2.3: a call before the definition infers the type of a later not
    //   annotated function [A3-032].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 99.
    {"CallBeforeDefinitionInfersForward",
     R"(@main def run(): Unit =
  val x = later()
  print(x)
def later() =
  7
)",
     0},
    // §2.3: arguments are terms; variable identifiers are terms [A3-060].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"VariablesAsArguments",
     R"(def f(a: Int, b: String): Unit =
  print(b)
@main def run(): Unit =
  val x = 1
  val s = "s"
  f(x, s)
)",
     0},
    // §2.3, ch. 6: the argument count must match the signature; too few is
    //   error 5 [A3-061].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"TooFewArgumentsIsError5",
     R"(def f(a: Int, b: Int): Unit =
  return
@main def run(): Unit =
  f(1)
)",
     5},
    // §2.3, ch. 6: the argument count must match the signature; too many is
    //   error 5 [A3-062].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"TooManyArgumentsIsError5",
     R"(def f(a: Int): Unit =
  return
@main def run(): Unit =
  f(1, 2)
)",
     5},
    // §2.3, ch. 6: argument types must match the signature; String for Int
    //   is error 5 [A3-063].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"StringArgumentForIntParameterIsError5",
     R"(def f(a: Int): Unit =
  return
@main def run(): Unit =
  f("1")
)",
     5},
    // §2.3, §1.3: an Int argument widens to a Double parameter [A3-064].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"IntArgumentWidensToDoubleParameter",
     R"(def f(a: Double): Unit =
  print(a)
@main def run(): Unit =
  f(1)
)",
     0},
    // §2.3, §1.3: there is no Double to Int conversion; such an argument is
    //   error 5 [A3-065]. §1.3 says 6 in general, but §4 and chapter 6 give
    //   the more specific 5 for a wrong argument type.
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH (statement starting with a
    //   term crashes) gives a crash (-1).
    {"DoubleArgumentForIntParameterIsError5",
     R"(def f(a: Int): Unit =
  return
@main def run(): Unit =
  f(1.5)
)",
     5},
    // §2.3, ch. 6: a variable inferred as Double passed to an Int parameter
    //   is error 5 [A3-066] (argument type: the more specific 5, not 6).
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 99.
    {"InferredDoubleVariableForIntParameterIsError5",
     R"(def f(a: Int): Unit =
  return
@main def run(): Unit =
  val d = 2.5
  f(d)
)",
     5},
    // §2.3: a call may be the right-hand side of an assignment [A3-067].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2, then RC-ASSIGN.
    {"CallAsAssignmentRightHandSide",
     R"(def name(): String =
  return "b"
@main def run(): Unit =
  var s = "a"
  s = name()
  print(s)
)",
     0},
    // §2.3, §2.2: assigning an Int call result to a String variable is
    //   error 6 [A3-068].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2, then RC-ASSIGN.
    {"CallAsAssignmentWithOtherTypeIsError6",
     R"(def count(): Int =
  return 1
@main def run(): Unit =
  var s = "a"
  s = count()
  print(s)
)",
     6},
    // §2.3: a call may be the value of return [A3-069].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"CallAsReturnValue",
     R"(def g(): Int =
  return 1
def f(): Int =
  return g()
@main def run(): Unit =
  val x = f()
  print(x)
)",
     0},
    // §2.3: returning a Unit call result from an Int function is error 6
    //   [A3-070].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"ReturnOfUnitCallInIntFunctionIsError6",
     R"(def f(): Int =
  return print("x")
@main def run(): Unit =
  return
)",
     6},
    // §2.3: a call as the last statement gives the return value [A3-071].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"CallAsLastStatementGivesValue",
     R"(def g(): Int =
  return 1
def f(): Int =
  g()
@main def run(): Unit =
  val x = f()
  print(x)
)",
     0},
    // §2.3: a String call as the last statement of an Int function is error
    //   6 [A3-072].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"CallAsLastStatementWithOtherTypeIsError6",
     R"(def g(): String =
  return "s"
def f(): Int =
  g()
@main def run(): Unit =
  return
)",
     6},
    // §2.3: a call used as a statement discards its result [A3-073].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR (expression parser stub)
    //   gives 2.
    {"CallStatementDiscardsResult",
     R"(def g(): Int =
  return 1
@main def run(): Unit =
  g()
  print("ok")
)",
     0},
};

} // namespace

class SpecFunctionsCallTest : public ::testing::TestWithParam<FunctionCase> {};

TEST_P(SpecFunctionsCallTest, ExitCodeFollowsSpec) {
  expect_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecFunctions, SpecFunctionsCallTest,
                         ::testing::ValuesIn(kCallCases), case_name);
