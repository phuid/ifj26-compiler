/*
 * AI-generated test block — black-box spec tests of the IFJ26 program
 *   structure (spec chapters 1.3, 2, 2.1 and 2.2).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write black-box conformance tests for the IFJ26 program structure from
 *   the approved catalog A2: the prolog, the top level of a program,
 *   blocks, val/var definitions and their initialization, type inference
 *   and compatibility, assignment, visibility and shadowing, and
 *   redefinition. Group the programs in TEST_P tables by topic, give every
 *   case a readable name and a spec section comment, and mark cases that
 *   fail today with their root cause. Failing tests stay enabled.
 */

/*
 * Root causes named in the "Fails today" comments (see revA section D):
 *   RC-PROLOG    parser.c p_program: the prolog import ifj26.* is not parsed
 *                (import -> 2) and its absence is not checked.
 *   RC-EXPR      parser.c p_expression is a stub: the expression is not
 *                consumed, val/var x = e gives 99, an if condition or
 *                return e gives 2.
 *   RC-CRASH     parser.c p_statement/p_code_block: a statement starting
 *                with a term (call, literal, (), id) is a NULL statement and
 *                crashes (harness -1).
 *   RC-ASSIGN    parser.c p_statement: no production for id = expression.
 *   RC-WHILE     parser.c p_statement: the while production is commented
 *                out.
 *   RC-PSTATUS   parser.h ParserStatus: redefinition, undefined variable and
 *                similar errors share PARSER_SEMANTIC_ERROR (harness -3), so
 *                codes 3/4/5 cannot be told apart.
 *   RC-COMMENT   lexer_dfa.c/lexer.c: // and nested block comments are not
 *                implemented (/ and * are operators).
 *   RC-LEADWS    lexer.c: whitespace at the start of the file is a lexical
 *                error (1).
 *   RC-TOPNL     parser.c p_program: leading empty lines (NEWLINE) on the
 *                top level are not skipped (2).
 *   RC-ANNOT-USE parser.c p_variable_init_statement + semantic.c: the symbol
 *                is defined with its stated type before the initializer, so
 *                a use before the definition of an annotated variable is not
 *                error 3.
 */

#include <ostream>
#include <string>

#include "gtest/gtest.h"

#include "spec_harness.hpp"

namespace {

/** One IFJ26 program and the exit code that spec chapter 6 expects. */
struct StructureCase {
  const char *name;
  const char *source;
  int expected;
};

/** Prints the case name instead of the raw bytes in gtest messages. */
void PrintTo(const StructureCase &row, std::ostream *out) { *out << row.name; }

/** Gives every table row a readable gtest name. */
std::string case_name(const ::testing::TestParamInfo<StructureCase> &info) {
  return std::string(info.param.name);
}

/** Runs a program body (the harness adds the prolog) and checks its code. */
void expect_exit_code(const StructureCase &row) {
  // Act
  const int code = ifj26_exit_code(row.source);
  // Assert
  EXPECT_EQ(code, row.expected) << row.source;
}

/** Runs a whole source exactly as written and checks its exit code. */
void expect_raw_exit_code(const StructureCase &row) {
  // Act
  const int code = ifj26_exit_code_raw(row.source);
  // Assert
  EXPECT_EQ(code, row.expected) << row.source;
}

} // namespace

/*
 * AI-generated test block — the mandatory prolog import ifj26.*.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of the spec chapter 2 prolog on whole sources (no
 *   harness prolog): the exact form, whitespace and comments around and
 *   inside it, and a prolog that is not one logical line of its own.
 */

namespace {

const StructureCase kPrologCases[] = {
    // §2: the prolog followed directly by a plain function definition and
    //   then a Unit @main is a valid program; the positive control of the
    //   error-2 prolog cases [A2-001, variant: the bare prolog + @main is
    //   spec_lexical_test CorrectPrologTwinIsAccepted].
    // Fails today: RC-PROLOG, the import is a syntax error (2).
    {"PrologFollowedByPlainFunctionIsAccepted",
     "import ifj26.*\ndef f(): Unit =\n  return\n"
     "@main def main(): Unit =\n  return\n",
     0},
    // §2: any whitespace may separate the prolog tokens; a tab inside a line
    //   is whitespace [A2-002].
    // Fails today: RC-PROLOG, the import is a syntax error (2).
    {"TabsBetweenPrologTokensAreAccepted",
     "import\tifj26.*\n@main def main(): Unit =\n  return\n", 0},
    // §2, §1.2: only whitespace and comments may precede the prolog; empty
    //   lines are ignored [A2-003].
    // Fails today: RC-TOPNL and RC-PROLOG, the leading NEWLINE and the
    //   import are syntax errors (2).
    {"BlankLinesBeforePrologAreIgnored",
     "\n\nimport ifj26.*\n@main def main(): Unit =\n  return\n", 0},
    // §2, §1.2: a line of spaces before the prolog is whitespace [A2-004].
    // Fails today: RC-LEADWS, leading spaces are a lexical error (1); then
    //   RC-PROLOG.
    {"SpaceOnlyLineBeforePrologIsIgnored",
     "   \n\nimport ifj26.*\n@main def main(): Unit =\n  return\n", 0},
    // §2, §1.1: a line comment may precede the prolog [A2-005].
    // Fails today: RC-COMMENT and RC-PROLOG, / and import are syntax errors
    //   (2).
    {"LineCommentBeforePrologIsAllowed",
     "// hlavicka\nimport ifj26.*\n@main def main(): Unit =\n  return\n", 0},
    // §2, §1.1: a block comment with a line feed before the prolog counts as
    //   one end of line [A2-006].
    // Fails today: RC-COMMENT and RC-PROLOG, / and import are syntax errors
    //   (2).
    {"MultiLineBlockCommentBeforePrologIsAllowed",
     "/* a\n   b */\nimport ifj26.*\n@main def main(): Unit =\n  return\n", 0},
    // §2, §1.1: a block comment without a line feed inside the prolog is
    //   one whitespace character [A2-007].
    // Fails today: RC-COMMENT and RC-PROLOG, the import is a syntax error
    //   (2).
    {"BlockCommentInsidePrologIsWhitespace",
     "import /* x */ ifj26.*\n@main def main(): Unit =\n  return\n", 0},
    // §2, §1.1: a line comment after the prolog ends its line [A2-008].
    // Fails today: RC-COMMENT and RC-PROLOG, the import is a syntax error
    //   (2).
    {"LineCommentAfterPrologIsAllowed",
     "import ifj26.* // prolog\n@main def main(): Unit =\n  return\n", 0},
    // §1.2, §2: a logical line ends with LF outside parentheses and each
    //   logical line holds one prolog, so import alone on a line is error 2;
    //   §1.2 limits the "any whitespace between tokens" of §2 (Scala, which
    //   accepts it, is no measure here) [A2-012].
    // Passes today only by accident: RC-PROLOG rejects every import with 2;
    //   positive control PrologFollowedByPlainFunctionIsAccepted.
    {"PrologSplitOverTwoLinesIsError2",
     "import\nifj26.*\n@main def main(): Unit =\n  return\n", 0},
    // §2, §1.2: a semicolon separates statements only; the prolog needs its
    //   own logical line [A2-013].
    // Passes today only by accident: RC-PROLOG rejects every import with 2;
    //   positive control PrologFollowedByPlainFunctionIsAccepted.
    {"SemicolonAfterPrologIsError2",
     "import ifj26.*; @main def main(): Unit =\n  return\n", 2},
};

} // namespace

class SpecProgramStructurePrologTest
    : public ::testing::TestWithParam<StructureCase> {};

TEST_P(SpecProgramStructurePrologTest, ExitCodeFollowsSpec) {
  expect_raw_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecProgramStructure, SpecProgramStructurePrologTest,
                         ::testing::ValuesIn(kPrologCases), case_name);

/*
 * AI-generated test block — the top level of a program after the prolog.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of spec chapter 2: only function definitions follow
 *   the prolog, a missing @main is error 3, and blank or comment-only
 *   lines between the definitions are ignored.
 */

namespace {

const StructureCase kTopLevelCases[] = {
    // §2, §1.2: blank and comment-only lines between the prolog and the
    //   definitions and between definitions are ignored [A2-009].
    // Fails today: RC-PROLOG, the import is a syntax error (2); then
    //   RC-COMMENT and RC-TOPNL.
    {"BlankAndCommentLinesBetweenDefinitionsAreIgnored",
     "import ifj26.*\n\n// c\n\ndef f(): Unit =\n  return\n\n"
     "@main def main(): Unit =\n  return\n",
     0},
    // §2: a prolog without any function is syntactically fine, but the
    //   missing @main is error 3 [A2-010].
    // Fails today: RC-PROLOG, the import is a syntax error (2).
    {"PrologWithoutFunctionsIsError3", "import ifj26.*\n", 3},
    // §2: only function definitions follow the prolog; a variable
    //   definition on the top level is error 2 [A2-014].
    // Passes today only by accident: RC-PROLOG rejects every import with 2;
    //   positive control PrologFollowedByPlainFunctionIsAccepted.
    {"TopLevelVariableDefinitionIsError2",
     "import ifj26.*\nval x = 1\n@main def main(): Unit =\n  return\n", 2},
};

} // namespace

class SpecProgramStructureTopLevelTest
    : public ::testing::TestWithParam<StructureCase> {};

TEST_P(SpecProgramStructureTopLevelTest, ExitCodeFollowsSpec) {
  expect_raw_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecProgramStructure, SpecProgramStructureTopLevelTest,
                         ::testing::ValuesIn(kTopLevelCases), case_name);

namespace {

const StructureCase kTopLevelBodyCases[] = {
    // §2, §1.2: empty lines between two function definitions are ignored;
    //   unlike BlankAndCommentLinesBetweenDefinitionsAreIgnored it needs no
    //   comments [A2-015].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2); passes
    //   without the prolog.
    {"BlankLinesBetweenFunctionsAreIgnored",
     R"(@main def main(): Unit =
  return


def f(): Unit =
  return
)",
     0},
};

} // namespace

class SpecProgramStructureTopLevelBodyTest
    : public ::testing::TestWithParam<StructureCase> {};

TEST_P(SpecProgramStructureTopLevelBodyTest, ExitCodeFollowsSpec) {
  expect_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecProgramStructure,
                         SpecProgramStructureTopLevelBodyTest,
                         ::testing::ValuesIn(kTopLevelBodyCases), case_name);

/*
 * AI-generated test block — blocks and the statements they contain.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of spec 2.1: a block always has a body (empty then,
 *   else and while bodies are error 2), () is a valid body, and a block
 *   holds every kind of statement, standalone expressions and statements
 *   separated by semicolons.
 */

namespace {

const StructureCase kBlockCases[] = {
    // §2.1, §1.1: a block body may consist of the Unit literal () alone
    //   [A2-085].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH gives crash (-1).
    {"UnitLiteralAsWholeBodyIsAccepted",
     R"(@main def main(): Unit =
  ()
)",
     0},
    // §2.1, §1.2: a block must always have a body; an empty then block is
    //   error 2 [A2-086].
    // Passes today only by accident: RC-PROLOG gives 2 (without the prolog
    //   RC-EXPR gives 2); positive control VariableIsVisibleInBothBranches.
    {"EmptyThenBlockIsError2",
     R"(@main def main(): Unit =
  if 1 < 2 then
  else
    ()
)",
     2},
    // §2.1, §1.2: an empty else block at the end of the file is error 2
    //   [A2-087].
    // Passes today only by accident: RC-PROLOG gives 2 (without the prolog
    //   RC-EXPR gives 2); positive control VariableIsVisibleInBothBranches.
    {"EmptyElseBlockAtEndOfFileIsError2",
     R"(@main def main(): Unit =
  if 1 < 2 then
    ()
  else
)",
     2},
    // §2.1, §2.5: a while body that is not indented more is empty, error 2
    //   [A2-088].
    // Passes today only by accident: RC-PROLOG gives 2 (without the prolog
    //   RC-EXPR gives 99); positive control LoopBodyVariableEndsWithLoop.
    {"EmptyWhileBodyIsError2",
     R"(@main def main(): Unit =
  var i = 0
  while i < 1 do
  print(i)
)",
     2},
    // §2.1: one block holds definitions, an assignment, calls, if, while,
    //   an expression and (); return in a function body [A2-089].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 2; then RC-ASSIGN, RC-WHILE.
    {"EveryStatementKindInOneBlockIsAccepted",
     R"(def inc(n: Int): Int =
  return n + 1
@main def main(): Unit =
  val a = 1
  var b = 2
  b = inc(a)
  print(b)
  if a < b then
    print(a)
  else
    ()
  while b < 3 do
    b = b + 1
  a + b
  ()
)",
     0},
    // §2.1, §2.3: a standalone expression is a statement; a Unit function
    //   discards its value [A2-090].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"StandaloneExpressionsAreStatements",
     R"(@main def main(): Unit =
  val x = 1
  x
  "abc"
  2.5
  x * 2
)",
     0},
    // §1.2, §2.1: several statements on one logical line are separated by
    //   semicolons [A2-091].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"SemicolonSeparatedStatementsOnOneLine",
     R"(@main def main(): Unit =
  val a = 1; var b = 2; b = a; print(b)
)",
     0},
};

} // namespace

class SpecProgramStructureBlockTest
    : public ::testing::TestWithParam<StructureCase> {};

TEST_P(SpecProgramStructureBlockTest, ExitCodeFollowsSpec) {
  expect_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecProgramStructure, SpecProgramStructureBlockTest,
                         ::testing::ValuesIn(kBlockCases), case_name);

/*
 * AI-generated test block — val/var definitions and their initialization.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of spec 1.3 and 2.2: the basic types as variable
 *   types, a mandatory initializer, an initializer that must be
 *   compatible with the stated type (only Int widens to Double) and an
 *   initializer that is a call.
 */

namespace {

const StructureCase kDefinitionCases[] = {
    // §1.3, §2.2: Int, Double, String and Unit are variable types; () is the
    //   Unit literal and print takes any value [A2-016].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"AllBasicTypesAreVariableTypes",
     R"(@main def main(): Unit =
  val a: Int = 1
  val b: Double = 2.5
  val c: String = "s"
  val d: Unit = ()
  print(a)
  print(b)
  print(c)
  print(d)
)",
     0},
    // §1.3, §2.2: the only implicit conversion Int -> Double applies to an
    //   initializer [A2-017].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"IntInitializerWidensToDouble",
     R"(@main def main(): Unit =
  val d: Double = 1
  print(d)
)",
     0},
    // §1.3, §2.2: a Double value where an Int is expected is error 6
    //   [A2-020].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"DoubleInitializerForIntIsError6",
     R"(@main def main(): Unit =
  val i: Int = 1.5
)",
     6},
    // §1.3, §3: Int never converts to String, error 6 [A2-024].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"IntInitializerForStringIsError6",
     R"(@main def main(): Unit =
  val s: String = 1
)",
     6},
    // §2.2, ch. 6: an initializer incompatible with the stated type is
    //   error 6 [A2-025].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"StringInitializerForIntIsError6",
     R"(@main def main(): Unit =
  val i: Int = "1"
)",
     6},
    // §1.3, §2.2: Int is not compatible with Unit, error 6 [A2-026].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"IntInitializerForUnitIsError6",
     R"(@main def main(): Unit =
  val u: Unit = 1
)",
     6},
    // §1.3, §2.2: Unit does not convert to Int, error 6 [A2-027].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"UnitInitializerForIntIsError6",
     R"(@main def main(): Unit =
  val i: Int = ()
)",
     6},
    // §2.2: the initialization is mandatory; val without = is error 2
    //   [A2-038].
    // Passes today only by accident: RC-PROLOG gives 2 (without the prolog
    //   the parser legitimately gives 2); positive control
    //   ValWithInitializerIsAccepted.
    {"ValWithoutInitializerIsError2",
     R"(@main def main(): Unit =
  val x: Int
  print(1)
)",
     2},
    // §2.2: positive twin of ValWithoutInitializerIsError2 [A2-038 twin].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"ValWithInitializerIsAccepted",
     R"(@main def main(): Unit =
  val x: Int = 1
  print(1)
)",
     0},
    // §2.2: an annotated var without an initializer is error 2 [A2-039].
    // Passes today only by accident: RC-PROLOG gives 2 (without the prolog
    //   the parser legitimately gives 2); positive control
    //   AnnotatedVarWithInitializerIsAccepted.
    {"AnnotatedVarWithoutInitializerIsError2",
     R"(@main def main(): Unit =
  var y: String
  y = "a"
)",
     2},
    // §2.2: positive twin of AnnotatedVarWithoutInitializerIsError2
    //   [A2-039 twin].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"AnnotatedVarWithInitializerIsAccepted",
     R"(@main def main(): Unit =
  var y: String = "b"
  y = "a"
)",
     0},
    // §2.2: the initializer may be a function call [A2-040].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"InitializerMayBeCall",
     R"(@main def main(): Unit =
  val n = readInt()
  print(n)
)",
     0},
};

} // namespace

class SpecProgramStructureDefinitionTest
    : public ::testing::TestWithParam<StructureCase> {};

TEST_P(SpecProgramStructureDefinitionTest, ExitCodeFollowsSpec) {
  expect_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecProgramStructure,
                         SpecProgramStructureDefinitionTest,
                         ::testing::ValuesIn(kDefinitionCases), case_name);

/*
 * AI-generated test block — inferred variable types and their
 *   compatibility with a stated type.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of spec 2.2 and 2.3: the type of a variable is
 *   inferred from its initializer, also through calls of functions whose
 *   return type is inferred, and must be compatible with a stated type;
 *   a recursive function without a return type has no inferable type.
 */

namespace {

const StructureCase kInferenceCases[] = {
    // §2.2, §1.3: i is inferred as Int and widens into a Double variable
    //   [A2-019].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"InferredIntWidensToDoubleVariable",
     R"(@main def main(): Unit =
  val i = 3
  val d: Double = i
  print(d)
)",
     0},
    // §2.2, §1.3: d is inferred as Double; Double where Int is expected is
    //   error 6 [A2-021].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"InferredDoubleForIntIsError6",
     R"(@main def main(): Unit =
  val d = 2.0
  val i: Int = d
)",
     6},
    // §1.3, §2.2: Unit is a variable type inferred from print(): Unit
    //   [A2-028].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"UnitInferredFromPrintCall",
     R"(@main def main(): Unit =
  val u = print("a")
  print(u)
)",
     0},
    // §2.3, §1.3: g infers Double from its body; Double where Int is
    //   expected is error 6 [A2-034].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH gives crash (-1).
    {"InferredDoubleResultForIntIsError6",
     R"(def g() =
  2.5
@main def main(): Unit =
  val i: Int = g()
)",
     6},
    // §2.3, §2.2: a body ending with a definition has the result (), so g
    //   infers Unit; the stated Int is incompatible, error 6 [A2-205].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"InferredUnitResultForIntIsError6",
     R"(def g() =
  val a = 1
@main def main(): Unit =
  val i: Int = g()
)",
     6},
    // §2.3, §2.2: Int is inferred through the chain g -> h; the stated
    //   String is incompatible, error 6 [A2-206].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH gives crash (-1).
    {"ChainedInferredIntForStringIsError6",
     R"(def g() =
  h()
def h() =
  1
@main def main(): Unit =
  val s: String = g()
)",
     6},
    // §2.3: direct recursion (even only in the else branch; the checks hold
    //   for unreachable code too) requires an explicitly stated return type;
    //   breaking this rule is type error 6, already at the definition of g
    //   [A2-207].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 2.
    {"RecursionInElseBranchWithoutTypeIsError6",
     R"(def g() =
  if 1 < 2 then
    ()
  else
    g()
@main def main(): Unit =
  val x = g()
)",
     6},
    // §2.2, §1.3: positive twin of ChainedInferredIntForStringIsError6; the
    //   inferred Int widens to the stated Double [A2-208].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH gives crash (-1).
    {"ChainedInferredIntWidensToDouble",
     R"(def g() =
  h()
def h() =
  1
@main def main(): Unit =
  val d: Double = g()
  print(d)
)",
     0},
};

} // namespace

class SpecProgramStructureInferenceTest
    : public ::testing::TestWithParam<StructureCase> {};

TEST_P(SpecProgramStructureInferenceTest, ExitCodeFollowsSpec) {
  expect_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecProgramStructure,
                         SpecProgramStructureInferenceTest,
                         ::testing::ValuesIn(kInferenceCases), case_name);

/*
 * AI-generated test block — assignment to variables.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of spec 2.2 assignment: the left side must be a
 *   modifiable variable (a val is error 7), the type of a variable never
 *   changes (another type is error 6, Int widens to Double) and the right
 *   side may be an expression or a call.
 */

namespace {

const StructureCase kAssignmentCases[] = {
    // §1.3, §2.2: Int widens to Double in an assignment too [A2-018].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"IntAssignedToDoubleVarWidens",
     R"(@main def main(): Unit =
  var d: Double = 1.5
  d = 2
  print(d)
)",
     0},
    // §2.2: the type of a variable never changes; a Double into an Int var
    //   is error 6 [A2-022].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"DoubleAssignedToIntVarIsError6",
     R"(@main def main(): Unit =
  var i = 1
  i = 2.0
)",
     6},
    // §3, §2.2: Int * Double is Double, which does not fit an Int var,
    //   error 6 [A2-023].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"DoubleExpressionAssignedToIntVarIsError6",
     R"(@main def main(): Unit =
  var i = 1
  i = i * 1.0
)",
     6},
    // §2.2, §1.3: u is inferred as Unit; assigning an Int is error 6
    //   [A2-029].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"IntAssignedToUnitVarIsError6",
     R"(@main def main(): Unit =
  var u = ()
  u = 1
)",
     6},
    // §2.2: the right side may be a call; its Double result into an Int
    //   var is error 6 [A2-033].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 2; then RC-ASSIGN.
    {"DoubleCallAssignedToIntVarIsError6",
     R"(def half(): Double =
  return 0.5
@main def main(): Unit =
  var i = 0
  i = half()
)",
     6},
    // §2.2: s is inferred as String from readLine(); assigning an Int is
    //   error 6 [A2-037].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"IntAssignedToInferredStringVarIsError6",
     R"(@main def main(): Unit =
  var s = readLine()
  s = 1
)",
     6},
    // §2.2: an assignment to a previously defined var is valid [A2-043].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"AssignmentToVarIsAccepted",
     R"(@main def main(): Unit =
  var x = 1
  x = 2
  print(x)
)",
     0},
    // §2.2, ch. 6: a val of the enclosing block is visible in a branch but
    //   not modifiable, error 7 [A2-045].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"AssignmentToOuterValInBranchIsError7",
     R"(@main def main(): Unit =
  val x = 1
  if x < 2 then
    x = 5
  else
    ()
)",
     7},
    // §2.2: assigning a value of another type is error 6 [A2-049].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"StringAssignedToIntVarIsError6",
     R"(@main def main(): Unit =
  var x = 1
  x = "a"
)",
     6},
    // §2.2: id = expression-or-call accepts a call on the right side
    //   [A2-052].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"CallResultAssignedToVar",
     R"(@main def main(): Unit =
  var s = ""
  s = readLine()
  print(s)
)",
     0},
    // §1.3, §2.2: print() returns Unit, which does not fit an Int var,
    //   error 6 [A2-053].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"UnitCallAssignedToIntVarIsError6",
     R"(@main def main(): Unit =
  var x = 1
  x = print("a")
)",
     6},
    // §1.3, §2.2: an Int variable on the right side widens into a Double
    //   var [A2-054].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"IntVariableAssignedToDoubleVarWidens",
     R"(@main def main(): Unit =
  var d = 1.5
  val i = 2
  d = i
  print(d)
)",
     0},
};

} // namespace

class SpecProgramStructureAssignmentTest
    : public ::testing::TestWithParam<StructureCase> {};

TEST_P(SpecProgramStructureAssignmentTest, ExitCodeFollowsSpec) {
  expect_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecProgramStructure,
                         SpecProgramStructureAssignmentTest,
                         ::testing::ValuesIn(kAssignmentCases), case_name);

/*
 * AI-generated test block — visibility of variables and shadowing.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of spec 2.2: a variable is visible from its
 *   definition to the end of its block and in all sub-blocks, a sub-block
 *   may shadow it, the outer one applies again after the sub-block, and a
 *   use before the definition, in its own initializer or outside the
 *   block or function is error 3.
 */

namespace {

const StructureCase kScopeCases[] = {
    // §2.2: after the shadowing sub-block the outer val applies again, so
    //   the write is error 7 [A2-048].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    // Positive control: AssignmentToShadowingVarInBranchIsAccepted.
    {"OuterValIsBackAfterShadowingBlockIsError7",
     R"(@main def main(): Unit =
  val x = 1
  if x < 2 then
    var x = 2
    x = 3
  else
    ()
  x = 4
)",
     7},
    // §2.2: positive twin of OuterValIsBackAfterShadowingBlockIsError7
    //   without the last write; x = 3 targets the inner var, so the 7 there
    //   comes only from the write after the sub-block [A2-048 twin].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"AssignmentToShadowingVarInBranchIsAccepted",
     R"(@main def main(): Unit =
  val x = 1
  if x < 2 then
    var x = 2
    x = 3
  else
    ()
)",
     0},
    // §2.2: an inner String val does not change the type of the outer Int
    //   var [A2-050].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"ShadowingDoesNotChangeOuterVarType",
     R"(@main def main(): Unit =
  var x = 1
  if x < 2 then
    val x = "s"
    print(x)
  else
    ()
  x = 2
)",
     0},
    // §2.2: inside the branch x is the inner String var, so assigning an
    //   Int is error 6 [A2-051].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"IntAssignedToShadowingStringVarIsError6",
     R"(@main def main(): Unit =
  var x = 1
  if x < 2 then
    var x = "s"
    x = 2
  else
    ()
)",
     6},
    // §2.2: a variable is visible in all sub-blocks of its block; positive
    //   control of the empty branch cases [A2-055].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"VariableIsVisibleInBothBranches",
     R"(@main def main(): Unit =
  val x = 1
  if x < 2 then
    print(x)
  else
    print(x)
)",
     0},
    // §2.2: a var is visible and writable in a nested sub-block [A2-056].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN.
    {"VariableIsVisibleInNestedBranch",
     R"(@main def main(): Unit =
  var x = 1
  if x < 2 then
    if x < 3 then
      x = 2
      print(x)
    else
      ()
  else
    ()
)",
     0},
    // §2.2: with nested shadowing (Int, Double, String) every level sees
    //   its nearest definition; the innermost String level is checked by a
    //   String variable, the Double level by
    //   InnerDoubleShadowForIntIsError6 [A2-062].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"NestedShadowingSeesNearestDefinition",
     R"(@main def main(): Unit =
  val x = 1
  if x < 2 then
    val x = 2.5
    if x < 3.0 then
      val x = "s"
      val s: String = x
    else
      val y: Double = x
    val z: Double = x
  else
    ()
  val w: Int = x
)",
     0},
    // §2.2, §1.3: negative twin of NestedShadowingSeesNearestDefinition;
    //   in the branch x is the inner Double, which does not fit an Int,
    //   error 6 [A2-062 twin].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"InnerDoubleShadowForIntIsError6",
     R"(@main def main(): Unit =
  val x = 1
  if x < 2 then
    val x = 2.5
    val i: Int = x
  else
    ()
)",
     6},
    // §2.2: using a variable before its definition is error 3 [A2-074].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH gives crash (-1); then RC-PSTATUS.
    {"UseBeforeDefinitionIsError3",
     R"(@main def main(): Unit =
  print(x)
  val x = 1
)",
     3},
    // §2.2: a local variable is visible only in its own block, not in
    //   another function [A2-076].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-PSTATUS.
    {"LocalOfOtherFunctionIsError3",
     R"(def f(): Unit =
  val a = 1
  print(a)
@main def main(): Unit =
  f()
  print(a)
)",
     3},
    // §2.3, §2.2: a parameter is a local variable of its own function
    //   [A2-077].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH gives crash (-1); then RC-PSTATUS.
    {"ParameterOfOtherFunctionIsError3",
     R"(def f(p: Int): Unit =
  print(p)
@main def main(): Unit =
  f(1)
  print(p)
)",
     3},
    // §2.2: a variable is visible only after its definition, not in its
    //   own initializer; this is "use of a variable before its definition"
    //   (§2.2, ch. 6 code 3), the missing type is only its consequence, so
    //   3 and not 6 [A2-078].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99.
    {"UseInOwnInitializerIsError3",
     R"(@main def main(): Unit =
  val x = x + 1
)",
     3},
    // §2.2: the same with a stated type [A2-079].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ANNOT-USE (0).
    {"UseInOwnAnnotatedInitializerIsError3",
     R"(@main def main(): Unit =
  val x: Int = x
)",
     3},
    // §2.2: a use in a condition before the definition is error 3
    //   [A2-080].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 2; then RC-PSTATUS.
    {"UseInConditionBeforeDefinitionIsError3",
     R"(@main def main(): Unit =
  if y < 1 then
    ()
  else
    ()
  val y = 1
)",
     3},
    // §2.2, ch. 6: an undefined variable on the right side of an
    //   assignment is error 3 [A2-081].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-ASSIGN, RC-PSTATUS.
    {"UndefinedVariableInAssignmentIsError3",
     R"(@main def main(): Unit =
  var x = 1
  x = z
)",
     3},
    // §2.2, ch. 6: an undefined variable as a call argument is error 3
    //   [A2-082].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-CRASH gives crash (-1); then RC-PSTATUS.
    {"UndefinedVariableAsArgumentIsError3",
     R"(@main def main(): Unit =
  print(z)
)",
     3},
};

} // namespace

class SpecProgramStructureScopeTest
    : public ::testing::TestWithParam<StructureCase> {};

TEST_P(SpecProgramStructureScopeTest, ExitCodeFollowsSpec) {
  expect_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecProgramStructure, SpecProgramStructureScopeTest,
                         ::testing::ValuesIn(kScopeCases), case_name);

/*
 * AI-generated test block — redefinition of variables.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box tests of spec 2.2: a second definition of a name in the same
 *   block is error 4, also after a shadowing sub-block or after a
 *   semicolon, while a variable of an ended loop body may be defined anew.
 */

namespace {

const StructureCase kRedefinitionCases[] = {
    // §2.2: a repeated definition in the same block is error 4 [A2-063].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-PSTATUS.
    {"ValRedefinedInSameBlockIsError4",
     R"(@main def main(): Unit =
  val x = 1
  val x = 2
)",
     4},
    // §2.2: redefining a var as a val of another type is error 4 [A2-064].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-PSTATUS.
    {"VarRedefinedAsValInSameBlockIsError4",
     R"(@main def main(): Unit =
  var x = 1
  val x = "s"
)",
     4},
    // §2.2: a sub-block with its own val x in between changes nothing; the
    //   second outer val x is error 4 [A2-066].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-PSTATUS.
    {"RedefinitionAfterShadowingBlockIsError4",
     R"(@main def main(): Unit =
  val x = 1
  if x < 2 then
    val x = 2
    print(x)
  else
    ()
  val x = 3
)",
     4},
    // §2.2, §1.2: statements separated by ; are in the same block, error 4
    //   [A2-068].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-PSTATUS.
    {"RedefinitionAfterSemicolonIsError4",
     R"(@main def main(): Unit =
  val x = 1; val x = 2
)",
     4},
    // §2.5, §2.2: k of the loop body ends with the loop; a new k of another
    //   type is no redefinition [A2-070].
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-EXPR gives 99; then RC-WHILE, RC-ASSIGN.
    {"LoopBodyVariableEndsWithLoop",
     R"(@main def main(): Unit =
  var i = 0
  while i < 1 do
    val k = 1
    i = i + k
  val k = "s"
  print(k)
)",
     0},
};

} // namespace

class SpecProgramStructureRedefinitionTest
    : public ::testing::TestWithParam<StructureCase> {};

TEST_P(SpecProgramStructureRedefinitionTest, ExitCodeFollowsSpec) {
  expect_exit_code(GetParam());
}
INSTANTIATE_TEST_SUITE_P(SpecProgramStructure,
                         SpecProgramStructureRedefinitionTest,
                         ::testing::ValuesIn(kRedefinitionCases), case_name);
