/*
 * AI-generated test block — shared rows and runners for the chapter 6 exit
 *   code tests.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write spec conformance tests for the IFJ26 exit codes of spec chapter
 *   6 from the approved catalog A6: the black-box programs the catalog
 *   keeps in this file (syntax errors, an undefined call and the
 *   precedence of code 7 over 6), with positive controls, and white-box
 *   tests of the status mapping in tests/spec_harness.h
 *   (ifj26_code_from_parser, ifj26_code_from_semantic,
 *   ifj26_code_from_wait, the prolog switch and the lexer -> parser ->
 *   code chain). Use TEST_P tables with readable row names, a spec
 *   comment over every row and the root cause of each case that fails
 *   today. Failing tests stay enabled.
 */
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <ostream>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

#include "gtest/gtest.h"

#include "spec_harness.h"

namespace {

/** One program body (the harness adds the prolog) and its exit code. */
struct ErrorCodeCase {
  const char *name;
  const char *source;
  int expected;
};

/** Prints a row by its name instead of a byte dump. */
void PrintTo(const ErrorCodeCase &row, std::ostream *out) { *out << row.name; }

/** Uses the row name as the gtest parameter name. */
std::string
error_code_case_name(const ::testing::TestParamInfo<ErrorCodeCase> &info) {
  return info.param.name;
}

/** Name of the variable that makes the harness skip the prolog. */
constexpr const char *kNoPrologVariable = "IFJ26_HARNESS_NO_PROLOG";

/**
 * Saves IFJ26_HARNESS_NO_PROLOG on construction and restores it on
 * destruction, so a test may change it without affecting the others.
 */
class NoPrologVariableGuard {
public:
  NoPrologVariableGuard() {
    const char *value = getenv(kNoPrologVariable);
    was_set_ = value != nullptr;
    if (was_set_) {
      saved_ = value;
    }
  }

  ~NoPrologVariableGuard() {
    if (was_set_) {
      setenv(kNoPrologVariable, saved_.c_str(), 1);
    } else {
      unsetenv(kNoPrologVariable);
    }
  }

  NoPrologVariableGuard(const NoPrologVariableGuard &) = delete;
  NoPrologVariableGuard &operator=(const NoPrologVariableGuard &) = delete;

private:
  bool was_set_ = false;
  std::string saved_;
};

} // namespace

/*
 * AI-generated test block — black-box programs for chapter 6 codes 0, 2,
 *   3 and 7.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Run the approved A6 programs of spec chapter 6 kept in this file
 *   through the harness: a minimal valid program as the positive control,
 *   syntax errors (missing = in the header, a function without a body,
 *   else indented deeper than if, with an aligned-else twin), a call of an
 *   undefined function and the precedence of code 7 over 6 for writes of
 *   a value of the wrong type to a val or a parameter.
 */
namespace {

const ErrorCodeCase kProgramCases[] = {
    // §2, ch. 6 (A6-K0): the smallest valid program compiles with 0.
    //   Positive control for every expected 2 in this file.
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"MinimalValidProgramIsCode0",
     R"(@main def run(): Unit =
  return
)",
     0},
    // §2.3, ch. 6 code 2 (A6-133): the = between the header and the body of
    //   a function is mandatory.
    // Passes today only because of RC-PROLOG (every program gives 2), see
    //   MinimalValidProgramIsCode0; passes without the prolog too.
    {"MissingEqualsInFunctionHeaderIsCode2",
     R"(@main def run(): Unit
  print(1)
)",
     2},
    // §2.1, ch. 6 codes 2 and 99 (A6-136): a function whose body is
    //   missing (end of input right after =) is a syntax error, never 99.
    // Passes today only because of RC-PROLOG (every program gives 2), see
    //   MinimalValidProgramIsCode0; passes without the prolog too.
    {"FunctionWithoutBodyAtEndOfInputIsCode2", R"(@main def run(): Unit =)", 2},
    // §2.4, ch. 6 code 2 (A6-138): else stands on its own line indented
    //   exactly like its if; a deeper else is a syntax error.
    // Passes today only because of RC-PROLOG (every program gives 2), see
    //   MinimalValidProgramIsCode0; passes without the prolog too.
    {"ElseIndentedDeeperThanIfIsCode2",
     R"(@main def run(): Unit =
  if 1 < 2 then
    print(1)
    else
    print(2)
)",
     2},
    // §2.4, ch. 6 code 0 (A6-138 twin): the same program with else aligned
    //   with its if is valid; positive control for the row above.
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"ElseAlignedWithIfIsCode0",
     R"(@main def run(): Unit =
  if 1 < 2 then
    print(1)
  else
    print(2)
)",
     0},
    // ch. 6 code 3 (A6-140): a call of an undefined function in a
    //   user-defined name (spec_builtins_test CapitalizedPrintIsUndefined-
    //   Error3 covers a misspelled built-in name).
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   passes without the prolog.
    {"CallOfUndefinedFunctionIsCode3",
     R"(@main def run(): Unit =
  foo(1)
)",
     3},
    // §2.2, ch. 6 codes 6 and 7 (A6-182): writing a String to a val is
    //   code 7, which is more specific than the type error 6.
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-ASSIGN (no production for `id = expr`)
    //   gives 2.
    {"WrongTypeAssignedToValIsCode7",
     R"(@main def run(): Unit =
  val x = 1
  x = "a"
)",
     7},
    // §2.3, ch. 6 codes 6 and 7 (A6-184): parameters are not modifiable,
    //   so writing a String to an Int parameter is code 7, not 6.
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2);
    //   without the prolog RC-ASSIGN (no production for `id = expr`)
    //   gives 2.
    {"WrongTypeAssignedToParameterIsCode7",
     R"(def f(n: Int): Unit =
  n = "a"
@main def run(): Unit =
  f(1)
)",
     7},
};

} // namespace

class SpecErrorCodesProgramTest
    : public ::testing::TestWithParam<ErrorCodeCase> {};

TEST_P(SpecErrorCodesProgramTest, ExitCodeFollowsChapter6) {
  // Arrange
  const ErrorCodeCase &row = GetParam();
  // Act
  const int code = ifj26_exit_code(row.source);
  // Assert
  EXPECT_EQ(code, row.expected) << row.source;
}
INSTANTIATE_TEST_SUITE_P(SpecErrorCodes, SpecErrorCodesProgramTest,
                         ::testing::ValuesIn(kProgramCases),
                         error_code_case_name);

/*
 * AI-generated test block — black-box sources with a wrong number of
 *   prologs.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Compile sources through ifj26_exit_code_raw so that the harness never
 *   adds or drops a prolog: an empty source (no prolog, no @main) and a
 *   program with the prolog written twice are syntax errors (2); the same
 *   program with the prolog written once is the positive control (0).
 */
namespace {

/** A source built from @c prologs copies of the prolog and a body. */
struct RawSourceCase {
  const char *name;
  int prologs;
  const char *body;
  int expected;
};

/** Prints a row by its name instead of a byte dump. */
void PrintTo(const RawSourceCase &row, std::ostream *out) { *out << row.name; }

std::string
raw_source_case_name(const ::testing::TestParamInfo<RawSourceCase> &info) {
  return info.param.name;
}

/** Joins the prolog copies and the body of one row into one source. */
std::string raw_source(const RawSourceCase &row) {
  std::string source;
  for (int i = 0; i < row.prologs; ++i) {
    source += kIfj26Prolog;
  }
  return source + row.body;
}

const RawSourceCase kRawSourceCases[] = {
    // §2, ch. 6 code 2 (A6-131): an empty source lacks the mandatory
    //   prolog, so it is a syntax error (2), not a missing @main (3).
    // Fails today: RC-NOPROLOG-OK, the missing prolog is accepted and the
    //   empty program gets 3 (missing @main, reported by the parser).
    {"EmptySourceWithoutPrologIsCode2", 0, "", 2},
    // §2, ch. 6 code 2 (A6-134): the prolog appears exactly once; a second
    //   import is a syntax error.
    // Passes today only because of RC-PROLOG (the first import is already
    //   a syntax error), see PrologWrittenOnceIsCode0.
    {"PrologWrittenTwiceIsCode2", 2, R"(@main def run(): Unit =
  return
)",
     2},
    // §2, ch. 6 code 0: the prolog written exactly once before a valid
    //   program; positive control for the rows above in both modes.
    // Fails today: RC-PROLOG, the prolog import is a syntax error (2).
    {"PrologWrittenOnceIsCode0", 1, R"(@main def run(): Unit =
  return
)",
     0},
};

} // namespace

class SpecErrorCodesRawSourceTest
    : public ::testing::TestWithParam<RawSourceCase> {};

TEST_P(SpecErrorCodesRawSourceTest, ExitCodeFollowsChapter6) {
  // Arrange
  const std::string source = raw_source(GetParam());
  // Act
  const int code = ifj26_exit_code_raw(source.c_str());
  // Assert
  EXPECT_EQ(code, GetParam().expected) << source;
}
INSTANTIATE_TEST_SUITE_P(SpecErrorCodes, SpecErrorCodesRawSourceTest,
                         ::testing::ValuesIn(kRawSourceCases),
                         raw_source_case_name);

/*
 * AI-generated test block — white-box mapping of ParserStatus and
 *   SemanticStatus to chapter 6 codes.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Check ifj26_code_from_parser for every ParserStatus value and an
 *   unknown one, ifj26_code_from_semantic for every SemanticStatus value,
 *   and pin the last enum values so that a new status cannot slip past
 *   the mapping unnoticed (A6-230, A6-231, A6-232).
 */
namespace {

struct ParserMapCase {
  const char *name;
  ParserStatus status;
  int expected;
};

/** Prints a row by its name instead of a byte dump. */
void PrintTo(const ParserMapCase &row, std::ostream *out) { *out << row.name; }

std::string
parser_map_case_name(const ::testing::TestParamInfo<ParserMapCase> &info) {
  return info.param.name;
}

// ch. 6 (A6-230): parser statuses map to chapter 6 codes.
const ParserMapCase kParserMapCases[] = {
    // ch. 6 code 0: a parsed program goes on to the semantic check.
    {"OkIsCode0", PARSER_OK, 0},
    // ch. 6 code 1: every lexer error is folded into PARSER_LEXER_ERROR.
    {"LexerErrorIsCode1", PARSER_LEXER_ERROR, 1},
    // ch. 6 code 2: syntax error.
    {"SyntaxErrorIsCode2", PARSER_SYNTAX_ERROR, 2},
    // ch. 6 code 3: undefined or not yet defined variable.
    {"UndefinedErrorIsCode3", PARSER_UNDEFINED_ERROR, 3},
    // ch. 6 code 4: redefinition of a function, variable or parameter.
    {"RedefinitionErrorIsCode4", PARSER_REDEFINITION_ERROR, 4},
    // ch. 6 code 5: wrong parameters (e.g. of @main).
    {"BadCallErrorIsCode5", PARSER_BAD_CALL_ERROR, 5},
    // ch. 6 code 6: type compatibility or type inference.
    {"TypeErrorIsCode6", PARSER_TYPE_ERROR, 6},
    // ch. 6 code 99: internal error not caused by the input program.
    {"InvalidArgumentIsCode99", PARSER_INVALID_ARGUMENT, 99},
    // ch. 6 code 99: internal error (e.g. allocation failure).
    {"InternalErrorIsCode99", PARSER_INTERNAL_ERROR, 99},
    // ch. 6 code 99: a value without an enumerator is never a success or an
    //   input error. 7 lies inside the value range of ParserStatus (0-127),
    //   so the cast is well defined.
    {"UnknownStatusIsCode99",
     static_cast<ParserStatus>(PARSER_TYPE_ERROR + 1), 99},
};

struct SemanticMapCase {
  const char *name;
  SemanticStatus status;
  int expected;
};

/** Prints a row by its name instead of a byte dump. */
void PrintTo(const SemanticMapCase &row, std::ostream *out) {
  *out << row.name;
}

std::string
semantic_map_case_name(const ::testing::TestParamInfo<SemanticMapCase> &info) {
  return info.param.name;
}

// ch. 6 (A6-232): every semantic status is its chapter 6 exit code.
const SemanticMapCase kSemanticMapCases[] = {
    // ch. 6 code 0: no error.
    {"OkIsCode0", SEMANTIC_OK, 0},
    // ch. 6 code 3: undefined function or variable, missing @main.
    {"UndefinedIsCode3", SEMANTIC_UNDEFINED, 3},
    // ch. 6 code 4: redefinition of a function or variable.
    {"RedefinitionIsCode4", SEMANTIC_REDEFINITION, 4},
    // ch. 6 code 5: wrong number or type of arguments, parameter of @main.
    {"BadCallIsCode5", SEMANTIC_BAD_CALL, 5},
    // ch. 6 code 6: type compatibility or type inference.
    {"TypeErrorIsCode6", SEMANTIC_TYPE_ERROR, 6},
    // ch. 6 code 7: write to a val or a parameter (SEMANTIC_OTHER).
    {"ImmutableWriteIsCode7", SEMANTIC_OTHER, 7},
    // ch. 6 code 99: internal error.
    {"InternalErrorIsCode99", SEMANTIC_INTERNAL_ERROR, 99},
};

} // namespace

class SpecErrorCodesParserMapTest
    : public ::testing::TestWithParam<ParserMapCase> {};

TEST_P(SpecErrorCodesParserMapTest, ParserStatusMapsToChapter6Code) {
  // Arrange
  const ParserMapCase &row = GetParam();
  // Act
  const int code = ifj26_code_from_parser(row.status);
  // Assert
  EXPECT_EQ(code, row.expected) << row.name;
}
INSTANTIATE_TEST_SUITE_P(SpecErrorCodes, SpecErrorCodesParserMapTest,
                         ::testing::ValuesIn(kParserMapCases),
                         parser_map_case_name);

class SpecErrorCodesSemanticMapTest
    : public ::testing::TestWithParam<SemanticMapCase> {};

TEST_P(SpecErrorCodesSemanticMapTest, SemanticStatusIsChapter6Code) {
  // Arrange
  const SemanticMapCase &row = GetParam();
  // Act
  const int code = ifj26_code_from_semantic(row.status);
  // Assert
  EXPECT_EQ(code, row.expected) << row.name;
}
INSTANTIATE_TEST_SUITE_P(SpecErrorCodes, SpecErrorCodesSemanticMapTest,
                         ::testing::ValuesIn(kSemanticMapCases),
                         semantic_map_case_name);

// ch. 6 (A6-231): a new ParserStatus value breaks the build already, because
//   ifj26_code_from_parser names every enumerator under -Wswitch-enum as an
//   error. LexerStatus (mapped in src/parser.c) and SemanticStatus (cast in
//   the harness) have no such guard: these asserts pin their last values,
//   which catches an insertion before the last value or a renumbering, but
//   not a value appended after it.
TEST(SpecErrorCodesEnumTest, StatusEnumsHaveNoUnmappedValues) {
  // Arrange, Act: nothing runs; the checks happen at compile time.
  // Assert
  static_assert(PARSER_INVALID_ARGUMENT == 100,
                "ParserStatus changed: review ifj26_code_from_parser");
  static_assert(LEXER_OUT_OF_MEMORY == 4,
                "LexerStatus changed: review parser_status_from_lexer");
  static_assert(SEMANTIC_INTERNAL_ERROR == 99,
                "SemanticStatus changed: review ifj26_code_from_semantic");
}

/*
 * AI-generated test block — white-box mapping of a child's wait status.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Fork children that exit with 0, 5, 99 and the parser-semantic marker
 *   100, crash with SIGSEGV or abort(), or are stopped by the time limit
 *   signal SIGALRM, and check that ifj26_code_from_wait gives the exit
 *   code, IFJ26_PARSER_SEMANTIC, IFJ26_CRASH or IFJ26_TIMEOUT (A6-233).
 */
namespace {

/** How the forked child ends. */
enum class ChildEnd { kExit, kSegfault, kAbort, kAlarm };

struct WaitMapCase {
  const char *name;
  ChildEnd end;
  int exit_code;
  int expected;
};

/** Prints a row by its name instead of a byte dump. */
void PrintTo(const WaitMapCase &row, std::ostream *out) { *out << row.name; }

std::string
wait_map_case_name(const ::testing::TestParamInfo<WaitMapCase> &info) {
  return info.param.name;
}

/**
 * Ends the child as the row says; never returns. The child first gets the
 * same reset as a harness child (default signal actions, no core dump).
 */
[[noreturn]] void end_child(const WaitMapCase &row) {
  ifj26_prepare_child();
  switch (row.end) {
  case ChildEnd::kSegfault:
    raise(SIGSEGV);
    break;
  case ChildEnd::kAbort:
    abort();
  case ChildEnd::kAlarm:
    raise(SIGALRM);
    break;
  case ChildEnd::kExit:
    break;
  }
  _exit(row.exit_code);
}

/** Forks a child that ends as the row says; returns its waitpid status. */
int wait_status_of_child(const WaitMapCase &row) {
  fflush(stdout);
  fflush(stderr);
  const pid_t child = fork();
  if (child == 0) {
    end_child(row);
  }
  int wait_status = 0;
  if (child < 0 || waitpid(child, &wait_status, 0) != child) {
    return -1;
  }
  return wait_status;
}

// Harness codes (tests/spec_harness.h): an exit status is the chapter 6
//   code, 100 marks the folded parser status, a signal is a crash and
//   SIGALRM (the time limit) is a timeout.
const WaitMapCase kWaitMapCases[] = {
    // ch. 6 code 0: a normal exit passes its code through.
    {"ExitZeroIsCode0", ChildEnd::kExit, 0, 0},
    // ch. 6 code 5: a normal exit passes its code through.
    {"ExitFiveIsCode5", ChildEnd::kExit, 5, 5},
    // ch. 6 code 99: a normal exit passes its code through.
    {"ExitNinetyNineIsCode99", ChildEnd::kExit, 99, 99},
    // Harness code: exit 100 is the marker of the folded parser status.
    {"ExitMarkerIsParserSemantic", ChildEnd::kExit, kIfj26ChildParserSemantic,
     IFJ26_PARSER_SEMANTIC},
    // Harness code: a crashing compiler is IFJ26_CRASH.
    {"SegfaultIsCrash", ChildEnd::kSegfault, 0, IFJ26_CRASH},
    // Harness code: an aborting compiler is IFJ26_CRASH.
    {"AbortIsCrash", ChildEnd::kAbort, 0, IFJ26_CRASH},
    // Harness code: SIGALRM of the time limit is IFJ26_TIMEOUT.
    {"AlarmIsTimeout", ChildEnd::kAlarm, 0, IFJ26_TIMEOUT},
};

} // namespace

class SpecErrorCodesWaitMapTest : public ::testing::TestWithParam<WaitMapCase> {
};

TEST_P(SpecErrorCodesWaitMapTest, WaitStatusMapsToHarnessCode) {
  // Arrange
  const WaitMapCase &row = GetParam();
  const int wait_status = wait_status_of_child(row);
  ASSERT_NE(wait_status, -1) << "fork or waitpid failed";
  // Act
  const int code = ifj26_code_from_wait(wait_status);
  // Assert
  EXPECT_EQ(code, row.expected) << row.name;
}
INSTANTIATE_TEST_SUITE_P(SpecErrorCodes, SpecErrorCodesWaitMapTest,
                         ::testing::ValuesIn(kWaitMapCases),
                         wait_map_case_name);

/*
 * AI-generated test block — the IFJ26_HARNESS_NO_PROLOG switch and the
 *   prolog the harness prepends.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Check that only the exact value "1" of IFJ26_HARNESS_NO_PROLOG makes
 *   the harness skip the prolog, that kIfj26Prolog is the prolog of spec
 *   chapter 2 and that ifj26_exit_code equals ifj26_exit_code_raw of the
 *   source with the prolog, or without it when the switch is set. Restore
 *   the variable after each test (A6-234, A6-235).
 */
namespace {

struct PrologSwitchCase {
  const char *name;
  const char *value; // nullptr = variable not set
  bool skips;
};

/** Prints a row by its name instead of a byte dump. */
void PrintTo(const PrologSwitchCase &row, std::ostream *out) {
  *out << row.name;
}

std::string
prolog_switch_case_name(const ::testing::TestParamInfo<PrologSwitchCase> &i) {
  return i.param.name;
}

// §2: the spec-correct default keeps the prolog; only the exact value "1"
//   switches it off (harness rule, tests/spec_harness.h).
const PrologSwitchCase kPrologSwitchCases[] = {
    // Harness rule: no variable, the prolog is added.
    {"UnsetKeepsProlog", nullptr, false},
    // Harness rule: exactly "1" skips the prolog.
    {"ExactOneSkipsProlog", "1", true},
    // Harness rule: any other value keeps the prolog.
    {"ZeroKeepsProlog", "0", false},
    // Harness rule: any other value keeps the prolog.
    {"EmptyKeepsProlog", "", false},
    // Harness rule: any other value keeps the prolog.
    {"TrueKeepsProlog", "true", false},
    // Harness rule: a prefix match is not enough.
    {"TenKeepsProlog", "10", false},
    // Harness rule: no trimming of white space.
    {"LeadingSpaceKeepsProlog", " 1", false},
};

/** A body without @main: its code differs with and without the prolog. */
constexpr const char *kBodyWithoutMain = "def f(): Unit =\n  return\n";

} // namespace

class SpecErrorCodesPrologSwitchTest
    : public ::testing::TestWithParam<PrologSwitchCase> {};

TEST_P(SpecErrorCodesPrologSwitchTest, OnlyExactOneSkipsProlog) {
  // Arrange
  NoPrologVariableGuard guard;
  const PrologSwitchCase &row = GetParam();
  if (row.value == nullptr) {
    unsetenv(kNoPrologVariable);
  } else {
    setenv(kNoPrologVariable, row.value, 1);
  }
  // Act
  const bool skips = ifj26_harness_skips_prolog();
  // Assert
  EXPECT_EQ(skips, row.skips) << row.name;
}
INSTANTIATE_TEST_SUITE_P(SpecErrorCodes, SpecErrorCodesPrologSwitchTest,
                         ::testing::ValuesIn(kPrologSwitchCases),
                         prolog_switch_case_name);

// §2 (A6-235): the harness prolog is exactly the mandatory import line.
TEST(SpecErrorCodesHarnessTest, PrologConstantIsSpecImportLine) {
  // Arrange
  const char *spec_prolog = "import ifj26.*\n";
  // Act
  const char *harness_prolog = kIfj26Prolog;
  // Assert
  EXPECT_STREQ(harness_prolog, spec_prolog);
}

// §2 (A6-235): by default ifj26_exit_code compiles the prolog plus the body.
TEST(SpecErrorCodesHarnessTest, ExitCodePrependsPrologByDefault) {
  // Arrange
  NoPrologVariableGuard guard;
  unsetenv(kNoPrologVariable);
  const std::string with_prolog = std::string(kIfj26Prolog) + kBodyWithoutMain;
  const int expected = ifj26_exit_code_raw(with_prolog.c_str());
  ASSERT_NE(expected, ifj26_exit_code_raw(kBodyWithoutMain))
      << "the body must tell the two sources apart";
  // Act
  const int code = ifj26_exit_code(kBodyWithoutMain);
  // Assert
  EXPECT_EQ(code, expected);
}

// §2 (A6-235): with IFJ26_HARNESS_NO_PROLOG=1 the body is compiled as is.
TEST(SpecErrorCodesHarnessTest, ExitCodeSkipsPrologWhenDisabled) {
  // Arrange
  NoPrologVariableGuard guard;
  setenv(kNoPrologVariable, "1", 1);
  const std::string with_prolog = std::string(kIfj26Prolog) + kBodyWithoutMain;
  const int expected = ifj26_exit_code_raw(kBodyWithoutMain);
  ASSERT_NE(expected, ifj26_exit_code_raw(with_prolog.c_str()))
      << "the body must tell the two sources apart";
  // Act
  const int code = ifj26_exit_code(kBodyWithoutMain);
  // Assert
  EXPECT_EQ(code, expected);
}

/*
 * AI-generated test block — the forked run and the lexer -> parser ->
 *   code chain.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Check that compiling in a forked child gives the same code as
 *   compiling in this process for sources that do not crash (A6-236), and
 *   that LEXER_INVALID_ARGUMENT reaches the harness as
 *   PARSER_INVALID_ARGUMENT and code 99 (A6-237).
 */
namespace {

struct ForkedRunCase {
  const char *name;
  const char *source;
};

/** Prints a row by its name instead of a byte dump. */
void PrintTo(const ForkedRunCase &row, std::ostream *out) { *out << row.name; }

std::string
forked_run_case_name(const ::testing::TestParamInfo<ForkedRunCase> &info) {
  return info.param.name;
}

// Sources compiled raw (no prolog) that end without a crash today; each
//   gives a different code. Harness rule: the fork changes no result.
const ForkedRunCase kForkedRunCases[] = {
    // Harness code 3 (missing @main, from the parser).
    {"MissingMain", "def f(): Unit =\n  return\n"},
    // Harness code 1 (a tab in the indentation, from the lexer).
    {"TabInIndentation", "@main def run(): Unit =\n\treturn\n"},
    // Harness code 0 (a valid program without the prolog, RC-NOPROLOG-OK).
    {"MinimalMain", "@main def run(): Unit =\n  return\n"},
    // Harness code 4 (redefinition of a built-in, from the semantic check).
    {"RedefinedPrint",
     "def print(): Unit =\n  return\n@main def run(): Unit =\n  return\n"},
    // Harness code 4 (a second @main is a redefinition, from the parser).
    {"TwoMainFunctions",
     "@main def a(): Unit =\n  return\n@main def b(): Unit =\n  return\n"},
};

} // namespace

class SpecErrorCodesForkedRunTest
    : public ::testing::TestWithParam<ForkedRunCase> {};

TEST_P(SpecErrorCodesForkedRunTest, ForkedRunMatchesInProcessRun) {
  // Arrange: the forked run first, so a crash or a hang of the compiler
  //   fails this test instead of the whole binary.
  const char *source = GetParam().source;
  const int forked = ifj26_exit_code_raw(source);
  ASSERT_NE(forked, IFJ26_CRASH) << "in-process run skipped:\n" << source;
  ASSERT_NE(forked, IFJ26_TIMEOUT) << "in-process run skipped:\n" << source;
  // Act
  const int in_process = ifj26_exit_code_in_process(source);
  // Assert
  EXPECT_EQ(forked, in_process) << source;
}
INSTANTIATE_TEST_SUITE_P(SpecErrorCodes, SpecErrorCodesForkedRunTest,
                         ::testing::ValuesIn(kForkedRunCases),
                         forked_run_case_name);

// ch. 6 code 99 (A6-237): a lexer without an input stream is an internal
//   error (LEXER_INVALID_ARGUMENT), never an input error of the program.
//   The parser half alone is ParserCovApiTest.DestroyedLexerIsInvalidArgument
//   (tests/parser_cov_test.cc); new here is the whole chain from the lexer
//   status to the harness exit code.
TEST(SpecErrorCodesLexerChainTest, LexerInvalidArgumentIsInternalError99) {
  // Arrange
  std::unique_ptr<FILE, int (*)(FILE *)> stream(tmpfile(), fclose);
  ASSERT_NE(stream, nullptr);
  fputs("@main def run(): Unit =\n  return\n", stream.get());
  rewind(stream.get());
  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream.get()), LEXER_OK);
  lexer.input = nullptr;
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  // Act
  const ParserStatus status = p_program(&lexer, &program);
  const int code = ifj26_code_from_parser(status);
  program_destroy(&program);
  lexer_destroy(&lexer);
  // Assert
  EXPECT_EQ(status, PARSER_INVALID_ARGUMENT);
  EXPECT_EQ(code, 99);
}
