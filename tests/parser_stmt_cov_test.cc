#include <cstddef>
#include <cstdio>

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

FILE *stream_from(const char *text) {
  FILE *stream = tmpfile();
  if (stream != nullptr) {
    fputs(text, stream);
    rewind(stream);
  }
  return stream;
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
  FILE *stream = stream_from(source);
  if (stream == nullptr) {
    return run;
  }
  Lexer lexer;
  Program program;
  if (lexer_init(&lexer, stream) == LEXER_OK) {
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
  fclose(stream);
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
// Today every input yields PARSER_INTERNAL_ERROR (99), which is wrong (see
// the BUG below); the exact code 2 is pinned by
// ParserStmtCovKnownGapTest.DISABLED_MalformedInitializerIsSyntaxError. The
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
    EXPECT_NE(status, PARSER_SEMANTIC_ERROR);
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
    EXPECT_NE(run.status, PARSER_SEMANTIC_ERROR) << "k=" << k;
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
// BUG: parser.c:560-574 – the p_expression stub yields a NULL initializer,
// ast_node_create_assign rejects it, so every `val/var x = ...` returns
// PARSER_INTERNAL_ERROR (99) instead of PARSER_SYNTAX_ERROR (2).
TEST(ParserStmtCovKnownGapTest, DISABLED_MalformedInitializerIsSyntaxError) {
  const char *const sources[] = {
      "def f() =\n  val x =\n",
      "def f() =\n  var x: Int = )\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(source), PARSER_SYNTAX_ERROR);
  }
}

// BUG: parser.c:572 – the first initializer already fails with 99, so the
// redefinition (PARSER_SEMANTIC_ERROR, code 3) is never reached.
TEST(ParserStmtCovKnownGapTest,
     DISABLED_DuplicateValInSameScopeIsSemanticError) {
  EXPECT_EQ(parse("def f() =\n  val x = 1\n  val x = 2\n"),
            PARSER_SEMANTIC_ERROR);
  EXPECT_EQ(parse("def f() =\n  var x = 1\n  val x = 2\n"),
            PARSER_SEMANTIC_ERROR);
}

// BUG: parser.c:605 – the p_expression stub does not consume the return
// value, so p_code_block then finds no NEWLINE (parser.c:884) and returns
// PARSER_SYNTAX_ERROR (2) instead of PARSER_OK.
TEST(ParserStmtCovKnownGapTest, DISABLED_ReturnWithValueIsAccepted) {
  EXPECT_EQ(parse("def f(): Int =\n  return 1\n"), PARSER_OK);
  EXPECT_EQ(parse("def f(a: Int): Int =\n  return a\n"), PARSER_OK);
}

// BUG: parser.c:486-501 – the p_expression stub never resolves identifiers
// in a scope, so an undefined variable is not reported as code 3 (today 2).
TEST(ParserStmtCovKnownGapTest,
     DISABLED_UndefinedVariableInExpressionIsSemanticError) {
  EXPECT_EQ(parse("def f(): Int =\n  return y\n"), PARSER_SEMANTIC_ERROR);
  EXPECT_EQ(parse("def f() =\n  if y then\n    return\n  else\n    return\n"),
            PARSER_SEMANTIC_ERROR);
}
