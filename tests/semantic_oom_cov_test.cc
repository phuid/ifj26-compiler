#include <cstddef>
#include <cstdlib>

#include "gtest/gtest.h"

extern "C" {
#include "../src/ast.h"
#include "../src/function_table.h"
#include "../src/parser.h"
#include "../src/semantic.h"
#include "../src/symtable.h"
}

#include <initializer_list>

/*
 * AI-generated test block — allocation failure injection harness.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Reuse the oom_fault_test.cc approach in this binary: interpose
 *   malloc/calloc/realloc/free, fail the k-th allocation inside an armed
 *   window and count live blocks; skip under sanitizers and wherever the
 *   override is not active (valgrind).
 */
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define SEMANTIC_OOM_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define SEMANTIC_OOM_SANITIZED 1
#endif
#endif

#ifdef SEMANTIC_OOM_SANITIZED
TEST(SemanticOomCovTest, SkippedUnderSanitizers) {
  GTEST_SKIP() << "malloc override is incompatible with sanitizers";
}
#else

extern "C" {
void *__libc_malloc(size_t);
void *__libc_calloc(size_t, size_t);
void *__libc_realloc(void *, size_t);
void __libc_free(void *);
}

namespace {

// Nothing in this namespace allocates. Outside the armed/tracking window the
// override only forwards to glibc, which keeps gtest (and the build-time
// gtest_discover_tests run of this binary) unaffected.
namespace oom {
long fail_at = 0, count = 0, live = 0;
bool armed = false, tracking = false;

bool fails() {
  if (!armed) {
    return false;
  }
  return ++count == fail_at;
}

// False where the override is not active (e.g. under valgrind).
bool injection_works() {
  void *(*volatile alloc)(size_t) = malloc;
  count = 0;
  fail_at = 1;
  armed = true;
  void *p = alloc(8);
  armed = false;
  if (p != nullptr) {
    free(p);
    return false;
  }
  return true;
}
} // namespace oom

// Upper bound on the number of allocation points walked by the loop.
constexpr long kMaxAllocationPoints = 100;

} // namespace

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

namespace {

/*
 * AI-generated test block — call-graph allocation failure in nested nodes.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Fail each call-graph edge allocation of a program whose new edges are
 *   found inside the first of two call arguments, an if condition, a then
 *   branch and a left operand; each failure must be SEMANTIC_INTERNAL_ERROR
 *   and leave no live block after program_destroy.
 */
using Nodes = std::initializer_list<ASTNode *>;

ASTNode *integer(long long v) { return ast_node_create_int(v, 1, 1); }
ASTNode *block(Nodes xs) {
  ASTNode *b = ast_node_create_block(1, 1);
  for (ASTNode *x : xs) {
    ast_node_block_append_statement(b, x);
  }
  return b;
}
ASTNode *call(FunctionSymbol *f, Nodes args = {}) {
  ASTNode *c = ast_node_create_function_call(f, 1, 1);
  for (ASTNode *a : args) {
    ast_node_call_append_argument(c, a);
  }
  return c;
}
ASTNode *less(ASTNode *l, ASTNode *r) {
  return ast_node_create_binary_op(AST_LESS, l, r, 1, 1);
}
ASTNode *if_(ASTNode *c, ASTNode *t) {
  return ast_node_create_if(c, t, nullptr, 1, 1);
}

/// Annotated `def name(): ret =` with an empty body.
FunctionSymbol *def(Program *p, const char *name, BaseType ret,
                    bool is_main = false) {
  FunctionSymbol *f = nullptr;
  function_table_get_or_create(&p->functions, name, &f);
  f->is_defined = true;
  f->is_main = is_main;
  function_symbol_create_local_scope(f);
  f->has_explicit_return_type = true;
  f->return_type.type = type_scalar(ret);
  f->ast_root = ast_node_create_block(1, 1);
  return f;
}
void add(FunctionSymbol *f, ASTNode *s) {
  ast_node_block_append_statement(f->ast_root, s);
}

// Every function is annotated and no builtin or Int->Double conversion is
// used, so the only allocations of semantic_check are call-graph edges:
//   @main def main(): Unit = ()     def g(): Int = 1     def u(): Unit = ()
//   def q(a: Int, b: Int): Unit = ()
//   def h1(): Unit = ()             def h2(): Unit = ()
//   def f1(): Unit = if (g() < 1) then { () }   -- if condition, left operand
//   def f2(): Unit = if (1 < 2) then { u() }    -- then branch
//   def f3(): Unit = { q(1, 1); u(); h1(); h2(); q(g(), 1) }
// In f3, q is already an edge, so the fifth new edge is g, found in the first
// of two arguments (the loop over the arguments must stop after it fails).
void build(Program *p) {
  def(p, "main", TYPE_UNIT, true);
  FunctionSymbol *g = def(p, "g", TYPE_INT);
  FunctionSymbol *u = def(p, "u", TYPE_UNIT);
  FunctionSymbol *q = def(p, "q", TYPE_UNIT);
  FunctionSymbol *h1 = def(p, "h1", TYPE_UNIT);
  FunctionSymbol *h2 = def(p, "h2", TYPE_UNIT);
  add(g, integer(1));
  for (const char *name : {"a", "b"}) {
    Symbol *parameter = nullptr;
    symbol_table_define(q->local_scope, name, type_scalar(TYPE_INT), true, true,
                        &parameter);
    function_symbol_add_parameter(q, parameter);
  }
  add(def(p, "f1", TYPE_UNIT), if_(less(call(g), integer(1)), block({})));
  add(def(p, "f2", TYPE_UNIT),
      if_(less(integer(1), integer(2)), block({call(u)})));
  FunctionSymbol *f3 = def(p, "f3", TYPE_UNIT);
  for (ASTNode *s : {call(q, {integer(1), integer(1)}), call(u), call(h1),
                     call(h2), call(q, {call(g), integer(1)})}) {
    add(f3, s);
  }
}

struct CheckRun {
  ParserStatus init;
  SemanticStatus status;
  bool injected;
  long live;
};

// Builds the program, fails the k-th allocation of semantic_check and
// destroys the program. Live blocks are counted from program_init to
// program_destroy. Inside the window: no gtest macros, printf or std::string.
CheckRun run_failing_at(long k) {
  CheckRun run = {PARSER_OK, SEMANTIC_OK, false, 0};
  Program program;
  oom::live = 0;
  oom::tracking = true;
  run.init = program_init(&program);
  if (run.init == PARSER_OK) {
    build(&program);
    oom::count = 0;
    oom::fail_at = k;
    oom::armed = true;
    run.status = semantic_check(&program);
    oom::armed = false;
    run.injected = oom::count >= k;
    program_destroy(&program);
  }
  oom::tracking = false;
  run.live = oom::live;
  return run;
}

TEST(SemanticOomCovTest, NestedCallGraphAllocationFailureIsInternalError) {
  if (!oom::injection_works()) {
    GTEST_SKIP() << "allocator override inactive (e.g. valgrind)";
  }
  long k = 1;
  for (; k < kMaxAllocationPoints; ++k) {
    const CheckRun run = run_failing_at(k);
    ASSERT_EQ(run.init, PARSER_OK) << "k=" << k;
    EXPECT_EQ(run.live, 0) << "k=" << k;
    if (!run.injected) {
      EXPECT_EQ(run.status, SEMANTIC_OK);
      break;
    }
    EXPECT_EQ(run.status, SEMANTIC_INTERNAL_ERROR) << "k=" << k;
  }
  ASSERT_LT(k, kMaxAllocationPoints);
  // f1, f2 and f3 each allocate their callee array at least once. The fifth
  // edge of f3 (g, inside the call argument) needs a realloc only because
  // dynamic arrays start with capacity 4 (dynamic_array.c), so whether that
  // nested failure is reached depends on that growth policy; it is not
  // asserted here.
  EXPECT_GE(k, 4);
}

} // namespace
#endif
