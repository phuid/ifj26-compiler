/*
 * AI-generated test block — shared table row and runner for built-ins.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write black-box spec conformance tests for the IFJ26 built-in functions
 *   (spec 4, with 1.3, 2.2, 2.3 and chapter 6). Each case is a whole program
 *   body run through tests/spec_harness.h and checked against the exit code
 *   of spec chapter 6. Group the cases into TEST_P tables per built-in and
 *   per topic, give every row a readable name and a comment with the spec
 *   rule and, when it fails today, the root cause.
 */

/*
 * Root causes named in the "Fails today" comments (revB section D; RC-ASSIGN
 * as in revA section D and spec_program_structure_test.cc):
 *   RC-PROLOG     parser.c p_program: the prolog import ifj26.* is not
 *                 parsed, so every program with the prolog gives 2.
 *   RC-EXPR       parser.c p_expression is a stub: val/var x = e gives 99,
 *                 return e gives 2.
 *   RC-STMT-CRASH parser.c p_statement/p_code_block: a statement starting
 *                 with an identifier (a call) is a NULL statement and
 *                 crashes (harness -1).
 *   RC-ASSIGN     parser.c p_statement: no production for id = expression.
 */
#include <ostream>
#include <string>

#include "gtest/gtest.h"

#include "spec_harness.h"

namespace {

/** One black-box program body with the exit code the spec requires. */
struct BuiltinCase {
  const char *name;
  const char *source;
  int expected;
};

/** Prints the row name instead of a byte dump in gtest messages. */
void PrintTo(const BuiltinCase &row, std::ostream *os) { *os << row.name; }

/** Uses the row name as the gtest parameter name. */
std::string
builtin_case_name(const ::testing::TestParamInfo<BuiltinCase> &info) {
  return info.param.name;
}

/** Compiles the row's program and checks the chapter 6 exit code. */
void expect_spec_exit_code(const BuiltinCase &row) {
  // Arrange: the row holds the program body; the harness adds the prolog.
  const char *source = row.source;

  // Act
  int code = ifj26_exit_code(source);

  // Assert
  EXPECT_EQ(code, row.expected) << source;
}

} // namespace

/*
 * AI-generated test block — print (spec 4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover print(value: Any): Unit: a correct call, every base type and a
 *   Unit variable as the argument, a missing and an extra argument.
 */
namespace {

const BuiltinCase kPrintCases[] = {
    // §4 (A6-001): print(value: Any): Unit takes one term of any type.
    // Positive control.
    // Fails today: RC-PROLOG (2); without the prolog RC-STMT-CRASH
    // (call statement -> -1).
    {"StringLiteralArgumentIsAccepted",
     R"IFJ(@main def run(): Unit =
  print("ahoj")
)IFJ",
     0},
    // §4, §1.1 (A6-002): Any is a value of any type; () is the Unit
    // literal and a Unit variable is a term too.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"AnyAcceptsIntDoubleStringAndUnit",
     R"IFJ(@main def run(): Unit =
  val u = ()
  print(1)
  print(2.5)
  print("s")
  print(())
  print(u)
)IFJ",
     0},
    // §4 (A6-003): a wrong number of arguments of a built-in function
    // is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-STMT-CRASH
    // (call statement -> -1).
    {"MissingArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  print()
)IFJ",
     5},
    // §4 (A6-004): print takes exactly one value; a second argument is
    // error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-STMT-CRASH
    // (call statement -> -1).
    {"ExtraArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  print("a", "b")
)IFJ",
     5},
};

class SpecBuiltinsPrintTest : public ::testing::TestWithParam<BuiltinCase> {};

TEST_P(SpecBuiltinsPrintTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsPrintTest,
                         ::testing::ValuesIn(kPrintCases), builtin_case_name);

} // namespace

/*
 * AI-generated test block — readLine (spec 4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover readLine(): String: a correct call and an argument passed to the
 *   parameterless function.
 */
namespace {

const BuiltinCase kReadLineCases[] = {
    // §4 (A6-010): readLine(): String. Positive control.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"ResultInitializesStringValue",
     R"IFJ(@main def run(): Unit =
  val s: String = readLine()
  print(s)
)IFJ",
     0},
    // §4 (A6-011): readLine has no parameters; any argument is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ArgumentToParameterlessFunctionIsError5",
     R"IFJ(@main def run(): Unit =
  val s = readLine("x")
)IFJ",
     5},
};

class SpecBuiltinsReadLineTest : public ::testing::TestWithParam<BuiltinCase> {
};

TEST_P(SpecBuiltinsReadLineTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsReadLineTest,
                         ::testing::ValuesIn(kReadLineCases),
                         builtin_case_name);

} // namespace

/*
 * AI-generated test block — readInt (spec 4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover readInt(): Int: a correct call and an argument passed to the
 *   parameterless function.
 */
namespace {

const BuiltinCase kReadIntCases[] = {
    // §4 (A6-020): readInt(): Int. Positive control. Twin of
    // InitializerMayBeCall in spec_program_structure_test.cc; this one
    // also pins the stated Int type.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"ResultInitializesIntValue",
     R"IFJ(@main def run(): Unit =
  val n: Int = readInt()
  print(n)
)IFJ",
     0},
    // §4 (A6-022): readInt has no parameters; any argument is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ArgumentToParameterlessFunctionIsError5",
     R"IFJ(@main def run(): Unit =
  val n = readInt(1)
)IFJ",
     5},
};

class SpecBuiltinsReadIntTest : public ::testing::TestWithParam<BuiltinCase> {};

TEST_P(SpecBuiltinsReadIntTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsReadIntTest,
                         ::testing::ValuesIn(kReadIntCases), builtin_case_name);

} // namespace

/*
 * AI-generated test block — readDouble (spec 4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover readDouble(): Double: a correct call and an argument passed to
 *   the parameterless function.
 */
namespace {

const BuiltinCase kReadDoubleCases[] = {
    // §4 (A6-030): readDouble(): Double. Positive control.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"ResultInitializesDoubleValue",
     R"IFJ(@main def run(): Unit =
  val d: Double = readDouble()
  print(d)
)IFJ",
     0},
    // §4 (A6-031): readDouble has no parameters; any argument is error
    // 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ArgumentToParameterlessFunctionIsError5",
     R"IFJ(@main def run(): Unit =
  val d = readDouble(1.5)
)IFJ",
     5},
};

class SpecBuiltinsReadDoubleTest
    : public ::testing::TestWithParam<BuiltinCase> {};

TEST_P(SpecBuiltinsReadDoubleTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsReadDoubleTest,
                         ::testing::ValuesIn(kReadDoubleCases),
                         builtin_case_name);

} // namespace

/*
 * AI-generated test block — toStr (spec 4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover toStr(value: Any): String: every base type as the argument, a
 *   missing and an extra argument.
 */
namespace {

const BuiltinCase kToStrCases[] = {
    // §4 (A6-040): toStr(value: Any): String accepts Int, Double,
    // String and Unit. Positive control.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"AnyAcceptsIntDoubleStringAndUnit",
     R"IFJ(@main def run(): Unit =
  val a = toStr(1)
  val b = toStr(2.5)
  val c = toStr("x")
  val d = toStr(())
  print(a)
)IFJ",
     0},
    // §4 (A6-041): a wrong number of arguments is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"MissingArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val s = toStr()
)IFJ",
     5},
    // §4 (A6-042): a wrong number of arguments is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ExtraArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val s = toStr(1, 2)
)IFJ",
     5},
};

class SpecBuiltinsToStrTest : public ::testing::TestWithParam<BuiltinCase> {};

TEST_P(SpecBuiltinsToStrTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsToStrTest,
                         ::testing::ValuesIn(kToStrCases), builtin_case_name);

} // namespace

/*
 * AI-generated test block — toInt (spec 1.3, 4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover toInt(value: Double): Int: a correct call, an Int literal and an
 *   Int variable widened to Double, wrong argument counts and types.
 */
namespace {

const BuiltinCase kToIntCases[] = {
    // §4 (A6-050): toInt(value: Double): Int. Positive control.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"DoubleLiteralArgumentIsAccepted",
     R"IFJ(@main def run(): Unit =
  val n: Int = toInt(3.7)
  print(n)
)IFJ",
     0},
    // §1.3, §4 (A6-051): the implicit Int -> Double conversion also
    // applies to an argument, so toInt(Int) is 0.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"IntLiteralWidensToDoubleParameter",
     R"IFJ(@main def run(): Unit =
  val n = toInt(3)
  print(n)
)IFJ",
     0},
    // §1.3, §2.3 (A6-052): an Int identifier is a term and widens to
    // the Double parameter -> 0.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"IntVariableWidensToDoubleParameter",
     R"IFJ(@main def run(): Unit =
  val i = 3
  val n = toInt(i)
  print(n)
)IFJ",
     0},
    // §4 (A6-053): a wrong number of arguments is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"MissingArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val n = toInt()
)IFJ",
     5},
    // §4 (A6-054): a wrong number of arguments is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ExtraArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val n = toInt(1.5, 2.5)
)IFJ",
     5},
    // §4 (A6-055): a term of a wrong type in a parameter of a built-in
    // function is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"StringArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val n = toInt("1")
)IFJ",
     5},
    // §4 (A6-056): Unit is not a Double; a wrong argument type is error
    // 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"UnitArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val n = toInt(())
)IFJ",
     5},
};

class SpecBuiltinsToIntTest : public ::testing::TestWithParam<BuiltinCase> {};

TEST_P(SpecBuiltinsToIntTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsToIntTest,
                         ::testing::ValuesIn(kToIntCases), builtin_case_name);

} // namespace

/*
 * AI-generated test block — strLength (spec 4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover strLength(value: String): Int: a correct call, wrong argument
 *   counts, an Int literal and a Double variable as the argument.
 */
namespace {

const BuiltinCase kStrLengthCases[] = {
    // §4 (A6-060): strLength(value: String): Int. Positive control.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"StringLiteralArgumentIsAccepted",
     R"IFJ(@main def run(): Unit =
  val n: Int = strLength("abc")
  print(n)
)IFJ",
     0},
    // §4 (A6-061): a wrong number of arguments is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"MissingArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val n = strLength()
)IFJ",
     5},
    // §4 (A6-062): a wrong number of arguments is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ExtraArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val n = strLength("a", "b")
)IFJ",
     5},
    // §4, §1.3 (A6-063): the only implicit conversion is Int -> Double,
    // so an Int argument for String is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"IntArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val n = strLength(1)
)IFJ",
     5},
    // §4 (A6-064): a Double identifier in a String parameter is error
    // 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"DoubleVariableArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val d = 1.5
  val n = strLength(d)
)IFJ",
     5},
};

class SpecBuiltinsStrLengthTest : public ::testing::TestWithParam<BuiltinCase> {
};

TEST_P(SpecBuiltinsStrLengthTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsStrLengthTest,
                         ::testing::ValuesIn(kStrLengthCases),
                         builtin_case_name);

} // namespace

/*
 * AI-generated test block — subStr (spec 4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover subStr(value: String, start: Int, end: Int): String: a correct
 *   call, wrong argument counts and a wrong type in each parameter,
 *   including Double for Int (code 5 by the leader decision).
 */
namespace {

const BuiltinCase kSubStrCases[] = {
    // §4 (A6-070): subStr(value: String, start: Int, end: Int): String.
    // Positive control.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"ThreeCorrectArgumentsAreAccepted",
     R"IFJ(@main def run(): Unit =
  val s: String = subStr("abcd", 1, 3)
  print(s)
)IFJ",
     0},
    // §4 (A6-071): a wrong number of arguments is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"MissingEndArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val s = subStr("abcd", 1)
)IFJ",
     5},
    // §4 (A6-072): a wrong number of arguments is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"FourthArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val s = subStr("abcd", 1, 3, 4)
)IFJ",
     5},
    // §4 (A6-073): an Int in the String parameter value is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"IntValueArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val s = subStr(1, 1, 3)
)IFJ",
     5},
    // §4 (A6-074): a String in the Int parameter start is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"StringStartArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val s = subStr("abcd", "1", 3)
)IFJ",
     5},
    // §4 (A6-075): a String in the Int parameter end is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"StringEndArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val s = subStr("abcd", 1, "3")
)IFJ",
     5},
    // §4, ch. 6 (A6-076): a Double in the Int parameter start is error
    // 5 (leader decision: the specific §4 rule wins over the general
    // §1.3 code 6).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"DoubleStartArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val s = subStr("abcd", 1.0, 3)
)IFJ",
     5},
    // §4, ch. 6 (A6-077): a Double identifier in the Int parameter end
    // is error 5 (leader decision, as A6-076).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"DoubleVariableEndArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val e = 3.0
  val s = subStr("abcd", 1, e)
)IFJ",
     5},
};

class SpecBuiltinsSubStrTest : public ::testing::TestWithParam<BuiltinCase> {};

TEST_P(SpecBuiltinsSubStrTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsSubStrTest,
                         ::testing::ValuesIn(kSubStrCases), builtin_case_name);

} // namespace

/*
 * AI-generated test block — ord (spec 4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover ord(value: String, index: Int): Int: a correct call, wrong
 *   argument counts and a wrong type in each parameter, including Double
 *   for Int (code 5 by the leader decision).
 */
namespace {

const BuiltinCase kOrdCases[] = {
    // §4 (A6-080): ord(value: String, index: Int): Int. Positive
    // control.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"StringAndIndexAreAccepted",
     R"IFJ(@main def run(): Unit =
  val c: Int = ord("A", 0)
  print(c)
)IFJ",
     0},
    // §4 (A6-081): a wrong number of arguments is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"MissingIndexIsError5",
     R"IFJ(@main def run(): Unit =
  val c = ord("A")
)IFJ",
     5},
    // §4 (A6-082): a wrong number of arguments is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ExtraArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val c = ord("A", 0, 1)
)IFJ",
     5},
    // §4 (A6-083): an Int in the String parameter value is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"IntValueArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val c = ord(65, 0)
)IFJ",
     5},
    // §4 (A6-084): a String in the Int parameter index is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"StringIndexArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val c = ord("A", "0")
)IFJ",
     5},
    // §4, ch. 6 (A6-085): a Double in the Int parameter index is error
    // 5 (leader decision, as A6-076).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"DoubleIndexArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val c = ord("A", 0.0)
)IFJ",
     5},
};

class SpecBuiltinsOrdTest : public ::testing::TestWithParam<BuiltinCase> {};

TEST_P(SpecBuiltinsOrdTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsOrdTest,
                         ::testing::ValuesIn(kOrdCases), builtin_case_name);

} // namespace

/*
 * AI-generated test block — chr (spec 4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover chr(code: Int): String: a correct call, wrong argument counts,
 *   String and Double literals and a String variable as the argument.
 */
namespace {

const BuiltinCase kChrCases[] = {
    // §4 (A6-090): chr(code: Int): String. Positive control.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"IntLiteralArgumentIsAccepted",
     R"IFJ(@main def run(): Unit =
  val s: String = chr(65)
  print(s)
)IFJ",
     0},
    // §4 (A6-091): a wrong number of arguments is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"MissingArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val s = chr()
)IFJ",
     5},
    // §4 (A6-092): a wrong number of arguments is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ExtraArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val s = chr(65, 66)
)IFJ",
     5},
    // §4 (A6-093): a String in the Int parameter code is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"StringArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val s = chr("A")
)IFJ",
     5},
    // §4, ch. 6 (A6-094): a Double in the Int parameter code is error 5
    // (leader decision, as A6-076).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"DoubleArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val s = chr(65.0)
)IFJ",
     5},
    // §2.3, §4 (A6-103): a term may be an identifier; a String variable
    // in the Int parameter is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"StringVariableArgumentIsError5",
     R"IFJ(@main def run(): Unit =
  val t = "A"
  val s = chr(t)
)IFJ",
     5},
};

class SpecBuiltinsChrTest : public ::testing::TestWithParam<BuiltinCase> {};

TEST_P(SpecBuiltinsChrTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsChrTest,
                         ::testing::ValuesIn(kChrCases), builtin_case_name);

} // namespace

/*
 * AI-generated test block — built-in results in init, assignment and return.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Check that the result type of each built-in function is used
 *   statically: val initialization, var assignment, return, the implicit
 *   result of a function body and inferred return types (spec 1.3, 2.2,
 *   2.3 rules 1-4), plus calls standing alone as statements.
 */
namespace {

const BuiltinCase kResultCases[] = {
    // §1.3, §4 (A6-006): print returns Unit and Unit may be the type of
    // a variable. Positive control.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"PrintResultInUnitValueIsAccepted",
     R"IFJ(@main def run(): Unit =
  val u: Unit = print(1)
  print(u)
)IFJ",
     0},
    // §2.2, §4 (A6-005): print returns Unit; initializing an Int value
    // with it is error 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"PrintResultInIntValueIsError6",
     R"IFJ(@main def run(): Unit =
  val x: Int = print(1)
)IFJ",
     6},
    // §2.2 (A6-012): a String result does not initialize an Int value
    // -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ReadLineResultInIntValueIsError6",
     R"IFJ(@main def run(): Unit =
  val n: Int = readLine()
)IFJ",
     6},
    // §1.3 (A6-013): the only implicit conversion is Int -> Double;
    // String -> Double is error 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ReadLineResultInDoubleValueIsError6",
     R"IFJ(@main def run(): Unit =
  val d: Double = readLine()
)IFJ",
     6},
    // §2.2 (A6-014): assigning a value of another type to a var is
    // error 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-ASSIGN.
    {"ReadLineResultAssignedToIntVarIsError6",
     R"IFJ(@main def run(): Unit =
  var n = 1
  n = readLine()
)IFJ",
     6},
    // §2.2 (A6-023): an Int result does not initialize a String value
    // -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ReadIntResultInStringValueIsError6",
     R"IFJ(@main def run(): Unit =
  val s: String = readInt()
)IFJ",
     6},
    // §2.3 rule 4 (A6-024): a returned value must be compatible with
    // the return type -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (return
    // value -> 2), then RC-STMT-CRASH.
    {"ReadIntResultReturnedFromStringFunctionIsError6",
     R"IFJ(def f(): String =
  return readInt()
@main def run(): Unit =
  val s = f()
  print(s)
)IFJ",
     6},
    // §2.2, §1.3 (A6-033): the type of a var is fixed and Double never
    // narrows to Int -> 6. Twin of DoubleCallAssignedToIntVarIsError6
    // in spec_program_structure_test.cc with a built-in instead of a
    // user function.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-ASSIGN.
    {"ReadDoubleResultAssignedToIntVarIsError6",
     R"IFJ(@main def run(): Unit =
  var n = 0
  n = readDouble()
)IFJ",
     6},
    // §2.2 (A6-043): a String result does not initialize an Int value
    // -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ToStrResultInIntValueIsError6",
     R"IFJ(@main def run(): Unit =
  val n: Int = toStr(1)
)IFJ",
     6},
    // §2.3 rule 2 (A6-044): f infers String from its last call; using
    // it as Int is error 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-STMT-CRASH
    // (call statement -> -1).
    {"ToStrInferredFunctionResultInIntValueIsError6",
     R"IFJ(def f() =
  toStr(1)
@main def run(): Unit =
  val n: Int = f()
)IFJ",
     6},
    // §1.3 (A6-057): the Int result of toInt widens to a Double value.
    // Positive control.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"ToIntResultWidensToDoubleValue",
     R"IFJ(@main def run(): Unit =
  val d: Double = toInt(1.5)
  print(d)
)IFJ",
     0},
    // §2.2 (A6-058): an Int result does not initialize a String value
    // -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ToIntResultInStringValueIsError6",
     R"IFJ(@main def run(): Unit =
  val s: String = toInt(1.5)
)IFJ",
     6},
    // §2.2 (A6-065): an Int result does not initialize a String value
    // -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"StrLengthResultInStringValueIsError6",
     R"IFJ(@main def run(): Unit =
  val s: String = strLength("a")
)IFJ",
     6},
    // §2.3 rules 2 and 4 (A6-066): the implicitly returned Int is not
    // compatible with String -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-STMT-CRASH
    // (call statement -> -1).
    {"StrLengthImplicitResultOfStringFunctionIsError6",
     R"IFJ(def f(): String =
  strLength("a")
@main def run(): Unit =
  val s = f()
  print(s)
)IFJ",
     6},
    // §2.2 (A6-078): a String result does not initialize an Int value
    // -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"SubStrResultInIntValueIsError6",
     R"IFJ(@main def run(): Unit =
  val n: Int = subStr("abcd", 1, 3)
)IFJ",
     6},
    // §2.2 (A6-086): an Int result does not initialize a String value
    // -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"OrdResultInStringValueIsError6",
     R"IFJ(@main def run(): Unit =
  val s: String = ord("A", 0)
)IFJ",
     6},
    // §2.2 (A6-095): a String result does not initialize an Int value
    // -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"ChrResultInIntValueIsError6",
     R"IFJ(@main def run(): Unit =
  val n: Int = chr(65)
)IFJ",
     6},
    // §2.3 (A6-104): a call may stand alone as a statement and its
    // result is discarded. Positive control.
    // Fails today: RC-PROLOG (2); without the prolog RC-STMT-CRASH
    // (call statement -> -1).
    {"CallsAsStatementsDiscardTheirResult",
     R"IFJ(@main def run(): Unit =
  readInt()
  toStr(1)
  strLength("ab")
  print("")
)IFJ",
     0},
    // §2.3 rules 1 and 4, §1.3 (A6-107): return of an Int result from
    // Int and (widened) Double functions. Positive control.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (return
    // value -> 2), then RC-STMT-CRASH.
    {"IntResultsReturnedFromIntAndDoubleFunctions",
     R"IFJ(def f(): Int =
  return strLength("ab")
def g(): Double =
  return ord("a", 0)
@main def run(): Unit =
  val a = f()
  val b = g()
  print(b)
)IFJ",
     0},
    // §2.3 rule 4 (A6-108): a returned String is not compatible with
    // Int -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (return
    // value -> 2), then RC-STMT-CRASH.
    {"ToStrResultReturnedFromIntFunctionIsError6",
     R"IFJ(def f(): Int =
  return toStr(1)
@main def run(): Unit =
  val a = f()
  print(a)
)IFJ",
     6},
    // §2.3 rule 2, §4, ch. 6 (A6-109): f infers Double from readDouble;
    // d is Double and passing it to the Int parameter of chr is error 5
    // (leader decision, as A6-076). Variant of the catalog case, which
    // repeated A6-044.
    // Fails today: RC-PROLOG (2); without the prolog RC-STMT-CRASH
    // (call statement -> -1).
    {"ReadDoubleInferredResultAsIntArgumentIsError5",
     R"IFJ(def f() =
  readDouble()
@main def run(): Unit =
  val d = f()
  val s = chr(d)
)IFJ",
     5},
};

class SpecBuiltinsResultTest : public ::testing::TestWithParam<BuiltinCase> {};

TEST_P(SpecBuiltinsResultTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsResultTest,
                         ::testing::ValuesIn(kResultCases), builtin_case_name);

} // namespace

/*
 * AI-generated test block — case-sensitive built-in names (spec 1, 6).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Check that built-in names are case-sensitive: Print and readint are
 *   undefined functions (code 3).
 */
namespace {

const BuiltinCase kNameCases[] = {
    // §1, ch. 6 (A6-100): identifiers are case-sensitive; Print is an
    // undefined function -> 3.
    // Fails today: RC-PROLOG (2); without the prolog RC-STMT-CRASH
    // (call statement -> -1).
    {"CapitalizedPrintIsUndefinedError3",
     R"IFJ(@main def run(): Unit =
  Print("a")
)IFJ",
     3},
    // §1, ch. 6 (A6-101): identifiers are case-sensitive; readint is an
    // undefined function -> 3. The call stands alone as a statement, so
    // no inferred type depends on it.
    // Fails today: RC-PROLOG (2); without the prolog RC-STMT-CRASH
    // (call statement -> -1).
    {"LowercaseReadintIsUndefinedError3",
     R"IFJ(@main def run(): Unit =
  readint()
)IFJ",
     3},
};

class SpecBuiltinsNameTest : public ::testing::TestWithParam<BuiltinCase> {};

TEST_P(SpecBuiltinsNameTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsNameTest,
                         ::testing::ValuesIn(kNameCases), builtin_case_name);

} // namespace

/*
 * AI-generated test block — static checks of built-in calls (spec 2.3, 6).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Check that a bad built-in call is reported even in unreachable code
 *   after return and in a function that is never called (code 5).
 */
namespace {

const BuiltinCase kStaticCheckCases[] = {
    // §2.3 (unreachable code), ch. 6 (all type checks are static)
    // (A6-115): a bad call after return is still error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"BadCallInUnreachableCodeIsError5",
     R"IFJ(@main def run(): Unit =
  return
  val n = strLength(1)
)IFJ",
     5},
    // ch. 6 (A6-116): all type checks are static, so a bad call in a
    // function that is never called is error 5.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99).
    {"BadCallInUncalledFunctionIsError5",
     R"IFJ(def unused(): Unit =
  val n = strLength(1)
@main def run(): Unit =
  return
)IFJ",
     5},
};

class SpecBuiltinsStaticCheckTest
    : public ::testing::TestWithParam<BuiltinCase> {};

TEST_P(SpecBuiltinsStaticCheckTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsStaticCheckTest,
                         ::testing::ValuesIn(kStaticCheckCases),
                         builtin_case_name);

} // namespace

/*
 * AI-generated test block — runtime conditions of built-ins (spec 4, 6).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Check that conditions which the spec defines as runtime behaviour
 *   (subStr start > end -> 40, ord out of range -> -1, chr outside 0..127)
 *   are not compile errors: the program compiles with 0.
 */
namespace {

const BuiltinCase kRuntimeConditionCases[] = {
    // §4, ch. 6 (A6-079): start > end ends with runtime error 40, it is
    // not a compile error -> 0.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"SubStrStartAfterEndCompiles",
     R"IFJ(@main def run(): Unit =
  val s = subStr("abcd", 3, 1)
  print(s)
)IFJ",
     0},
    // §4 (A6-087): ord returns -1 at runtime for an empty string or an
    // index out of range, it is not a compile error -> 0.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"OrdOnEmptyStringAndOutOfRangeIndexCompiles",
     R"IFJ(@main def run(): Unit =
  val c = ord("", 5)
  print(c)
)IFJ",
     0},
    // §4 (A6-096): a code outside 0..127 fails at runtime
    // (instruction), not at compile time -> 0.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val/var
    // initializer -> 99), then RC-STMT-CRASH.
    {"ChrOutOfAsciiRangeCompiles",
     R"IFJ(@main def run(): Unit =
  val s = chr(200)
  print(s)
)IFJ",
     0},
};

class SpecBuiltinsRuntimeConditionTest
    : public ::testing::TestWithParam<BuiltinCase> {};

TEST_P(SpecBuiltinsRuntimeConditionTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(SpecBuiltins, SpecBuiltinsRuntimeConditionTest,
                         ::testing::ValuesIn(kRuntimeConditionCases),
                         builtin_case_name);

} // namespace
