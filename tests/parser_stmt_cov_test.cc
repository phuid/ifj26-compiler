#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "../src/parser.h"
}

/*
 * AI-generated test block — parser statement coverage helpers.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Run the lexer and p_program over in-memory IFJ26 text, optionally with
 *   the k-th allocation failing (malloc interposition as in
 *   tests/oom_fault_test.cc), and report status, injection and live blocks.
 */
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define STMT_COV_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define STMT_COV_SANITIZED 1
#endif
#endif

namespace {
namespace oom {
long fail_at = 0, count = 0, live = 0;
bool armed = false, tracking = false;
bool fails() { return armed && ++count == fail_at; }
} // namespace oom

/// True for the statuses of semantic errors found by the parser (codes 3-6).
bool is_semantic_error(ParserStatus status) {
  return status >= PARSER_UNDEFINED_ERROR && status <= PARSER_TYPE_ERROR;
}
} // namespace

#ifndef STMT_COV_SANITIZED
extern "C" {
void *__libc_malloc(size_t);
void *__libc_calloc(size_t, size_t);
void *__libc_realloc(void *, size_t);
void __libc_free(void *);
}

extern "C" void *malloc(size_t n) {
  if (oom::fails()) {
    return nullptr;
  }
  void *p = __libc_malloc(n);
  if (p != nullptr && oom::tracking) {
    ++oom::live;
  }
  return p;
}

extern "C" void *calloc(size_t a, size_t b) {
  if (oom::fails()) {
    return nullptr;
  }
  void *p = __libc_calloc(a, b);
  if (p != nullptr && oom::tracking) {
    ++oom::live;
  }
  return p;
}

extern "C" void *realloc(void *p, size_t n) {
  if (oom::fails()) {
    return nullptr;
  }
  void *q = __libc_realloc(p, n);
  if (q != nullptr && p == nullptr && oom::tracking) {
    ++oom::live;
  }
  return q;
}

extern "C" void free(void *p) {
  if (p != nullptr && oom::tracking) {
    --oom::live;
  }
  __libc_free(p);
}
#endif

namespace {

/** Returns a null-terminated copy of @p text for the lexer to borrow. */
std::vector<char> buffer_from(const char *text) {
  return std::vector<char>(text, text + strlen(text) + 1);
}

struct ParseRun {
  ParserStatus status;
  bool injected;
  long live;
};

// fail_at == 0 parses without injecting anything. Program and lexer are set
// up before arming and torn down while live blocks are still counted.
ParseRun parse_failing_at(const char *source, long fail_at) {
  ParseRun run = {PARSER_INVALID_ARGUMENT, false, 0};
  std::vector<char> buffer = buffer_from(source);
  Lexer lexer;
  Program program;
  if (lexer_init(&lexer, buffer.data()) == LEXER_OK) {
    if (program_init(&program) == PARSER_OK) {
      oom::count = 0;
      oom::live = 0;
      oom::fail_at = fail_at;
      oom::armed = oom::tracking = fail_at > 0;
      run.status = p_program(&lexer, &program);
      oom::armed = false;
      run.injected = fail_at > 0 && oom::count >= fail_at;
      program_destroy(&program);
      lexer_destroy(&lexer);
      run.live = oom::live;
      oom::tracking = false;
    } else {
      lexer_destroy(&lexer);
    }
  }
  return run;
}

ParserStatus parse(const char *source) {
  return parse_failing_at(source, 0).status;
}

bool injection_works() {
#ifdef STMT_COV_SANITIZED
  return false;
#else
  void *(*volatile alloc)(size_t) = malloc;
  oom::count = 0;
  oom::fail_at = 1;
  oom::armed = true;
  void *p = alloc(8);
  oom::armed = false;
  if (p != nullptr) {
    free(p);
    return false;
  }
  return true;
#endif
}

class ParserStmtCovOomTest : public ::testing::Test {
protected:
  void SetUp() override {
    if (!injection_works()) {
      GTEST_SKIP() << "allocator override inactive (sanitizer or valgrind)";
    }
  }
};

constexpr long kMaxAllocationPoints = 1000;

} // namespace

/*
 * AI-generated test block — lexer failure right after val/var.
 * Model: Claude (Claude Code).
 * Prompt:
 *   A lexical error in the token after `val`/`var` must surface from
 *   p_program as PARSER_LEXER_ERROR (code 1), not as a syntax error.
 */
TEST(ParserStmtCovTest, LexerErrorAfterValOrVarKeywordIsPropagated) {
  const char *const sources[] = {
      "def f() =\n  val$",
      "def f() =\n  var$",
      "def f() =\n  val $\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(source), PARSER_LEXER_ERROR);
  }
}

/*
 * AI-generated test block — malformed variable initializer.
 * Model: Claude (Claude Code).
 * Prompt:
 *   `val/var x [: T] =` with a missing or non-expression initializer must
 *   never be accepted, nor reported as a lexer, semantic or invalid-argument
 *   failure; the exact code is pinned by a DISABLED test below.
 */
// The exact code 2 is pinned by
// ParserStmtCovKnownGapTest.MalformedInitializerIsSyntaxError. The
// extra checks follow from the input: it is lexically valid, has nothing
// semantic to report and p_program receives valid arguments.
TEST(ParserStmtCovTest, MalformedInitializerIsNeverAccepted) {
  const char *const sources[] = {
      "def f() =\n  val x =\n",
      "def f() =\n  val x =",
      "def f() =\n  var x: Int = )\n",
      "def f() =\n  val x = else\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    const ParserStatus status = parse(source);
    EXPECT_NE(status, PARSER_OK);
    EXPECT_NE(status, PARSER_LEXER_ERROR);
    EXPECT_FALSE(is_semantic_error(status));
    EXPECT_NE(status, PARSER_INVALID_ARGUMENT);
  }
}

/*
 * AI-generated test block — allocation failure while defining a local.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Fail each allocation of parsing `val x` in turn: none may be reported
 *   as OK/syntax/semantic, at least one is PARSER_INTERNAL_ERROR (99), and
 *   nothing leaks; without injection the missing `=` is a syntax error.
 */
TEST_F(ParserStmtCovOomTest,
       VariableDefinitionAllocationFailureIsNotSyntaxError) {
  const char *source = "def f() =\n  val x\n";
  bool saw_internal = false;
  long k = 1;
  for (;; ++k) {
    ASSERT_LT(k, kMaxAllocationPoints);
    const ParseRun run = parse_failing_at(source, k);
    if (!run.injected) {
      EXPECT_EQ(run.status, PARSER_SYNTAX_ERROR);
      EXPECT_EQ(run.live, 0);
      break;
    }
    EXPECT_NE(run.status, PARSER_OK) << "k=" << k;
    EXPECT_NE(run.status, PARSER_SYNTAX_ERROR) << "k=" << k;
    EXPECT_FALSE(is_semantic_error(run.status)) << "k=" << k;
    EXPECT_EQ(run.live, 0) << "k=" << k;
    saw_internal = saw_internal || run.status == PARSER_INTERNAL_ERROR;
  }
  EXPECT_TRUE(saw_internal);
  EXPECT_GT(k, 1);
}

/*
 * AI-generated test block — statement behaviour blocked by p_expression.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Record the specified result of statements whose expression part is
 *   never parsed (stub p_expression); enable each once src/ is fixed.
 */
TEST(ParserStmtCovKnownGapTest, MalformedInitializerIsSyntaxError) {
  const char *const sources[] = {
      "def f() =\n  val x =\n",
      "def f() =\n  var x: Int = )\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(source), PARSER_SYNTAX_ERROR);
  }
}

TEST(ParserStmtCovKnownGapTest, DuplicateValInSameScopeIsSemanticError) {
  EXPECT_EQ(parse("def f() =\n  val x = 1\n  val x = 2\n"),
            PARSER_REDEFINITION_ERROR);
  EXPECT_EQ(parse("def f() =\n  var x = 1\n  val x = 2\n"),
            PARSER_REDEFINITION_ERROR);
}

TEST(ParserStmtCovKnownGapTest, ReturnWithValueIsAccepted) {
  // A valid program needs @main (otherwise code 3).
  EXPECT_EQ(parse("def f(): Int =\n  return 1\n"
                  "@main\ndef main(): Unit =\n  return\n"),
            PARSER_OK);
  EXPECT_EQ(parse("def f(a: Int): Int =\n  return a\n"
                  "@main\ndef main(): Unit =\n  return\n"),
            PARSER_OK);
}

TEST(ParserStmtCovKnownGapTest, UndefinedVariableInExpressionIsSemanticError) {
  EXPECT_EQ(parse("def f(): Int =\n  return y\n"), PARSER_UNDEFINED_ERROR);
  EXPECT_EQ(parse("def f() =\n  if y then\n    return\n  else\n    return\n"),
            PARSER_UNDEFINED_ERROR);
}
