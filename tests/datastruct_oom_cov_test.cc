#include <cstddef>
#include <cstdlib>

#include "gtest/gtest.h"

extern "C" {
#include "../src/ast.h"
#include "../src/function_table.h"
#include "../src/symtable.h"
}

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
#define DS_OOM_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define DS_OOM_SANITIZED 1
#endif
#endif

#ifdef DS_OOM_SANITIZED
TEST(DatastructOomCovTest, SkippedUnderSanitizers) {
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

// Nothing in this namespace allocates. Outside an Arm window the override
// only forwards to glibc, which keeps gtest (and the build-time
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

// Fails the k-th allocation after construction and counts live blocks until
// destruction. Inside the window: no gtest macros, printf or std::string.
struct Arm {
  explicit Arm(long k) {
    count = 0;
    fail_at = k;
    live = 0;
    armed = tracking = true;
  }
  void off() { armed = false; }
  // True when the k-th allocation was attempted, i.e. a failure was injected.
  bool injected() const { return fail_at > 0 && count >= fail_at; }
  ~Arm() { armed = tracking = false; }
};

bool injection_works() {
  void *(*volatile alloc)(size_t) = malloc;
  void *p;
  {
    Arm arm(1);
    p = alloc(8);
  }
  if (p != nullptr) {
    free(p);
    return false;
  }
  return true;
}
} // namespace oom

// Upper bound on the number of allocation points walked by one loop.
constexpr long kMaxAllocationPoints = 1000;

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
 * AI-generated test block — AST constructors under allocation failure.
 * Model: Claude (Claude Code).
 * Prompt:
 *   For every AST constructor not yet covered by oom_fault_test.cc (real,
 *   unit, variable ref, call, assign, if, binary op) fail each allocation
 *   in turn: per ast.h the call returns null, leaks nothing and leaves every
 *   passed child owned by the caller; the first non-failing k succeeds.
 */
class DatastructOomCovTest : public ::testing::Test {
protected:
  void SetUp() override {
    if (!oom::injection_works()) {
      GTEST_SKIP() << "allocator override inactive (e.g. valgrind)";
    }
    scope = symbol_table_create_root();
    ASSERT_NE(scope, nullptr);
    ASSERT_EQ(symbol_table_define(scope, "x", TYPE_INT, true, false, &x),
              SYMTABLE_OK);
    ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);
    ASSERT_EQ(function_table_get_or_create(&table, "f", &f), FUNCTION_TABLE_OK);
  }
  void TearDown() override {
    function_table_destroy(&table);
    symbol_table_destroy(scope);
  }

  SymbolTable *scope = nullptr;
  Symbol *x = nullptr;
  FunctionTable table = {};
  FunctionSymbol *f = nullptr;
};

struct ConstructRun {
  ASTNode *node;
  bool injected;
  long live;
};

// Runs one constructor with the k-th allocation failing. `make` must only
// call the constructor (no gtest macros inside the window).
template <typename Make> ConstructRun construct_failing_at(long k, Make make) {
  ConstructRun run = {nullptr, false, 0};
  oom::Arm arm(k);
  run.node = make();
  arm.off();
  run.injected = arm.injected();
  run.live = oom::live;
  return run;
}

// Walks k = 1, 2, ... until the constructor no longer hits an injected
// failure. Every failing k must yield null without a live block; the first
// non-failing k must yield a node of `expected` kind. Returns that k.
template <typename Make>
long walk_leaf(const char *what, ASTNodeKind expected, Make make) {
  for (long k = 1; k < kMaxAllocationPoints; ++k) {
    ConstructRun run = construct_failing_at(k, make);
    if (!run.injected) {
      EXPECT_NE(run.node, nullptr) << what;
      if (run.node != nullptr) {
        EXPECT_EQ(run.node->kind, expected) << what;
        ast_node_destroy(run.node);
      }
      return k;
    }
    EXPECT_EQ(run.node, nullptr) << what << " k=" << k;
    EXPECT_EQ(run.live, 0) << what << " k=" << k;
    ast_node_destroy(run.node);
  }
  ADD_FAILURE() << what << " never succeeded";
  return kMaxAllocationPoints;
}

TEST_F(DatastructOomCovTest, LeafConstructorsReturnNullWithoutLeaking) {
  EXPECT_GT(walk_leaf("create_real", AST_CONSTANT_REAL,
                      [] { return ast_node_create_real(2.5, 1U, 1U); }),
            1);
  EXPECT_GT(walk_leaf("create_unit", AST_CONSTANT_UNIT,
                      [] { return ast_node_create_unit(1U, 1U); }),
            1);
  EXPECT_GT(
      walk_leaf("create_variable_ref", AST_VARIABLE_REF,
                [this] { return ast_node_create_variable_ref(x, 1U, 1U); }),
      1);
  EXPECT_GT(
      walk_leaf("create_function_call", AST_FUNCTION_CALL,
                [this] { return ast_node_create_function_call(f, 1U, 1U); }),
      1);
}

// Children for the composite constructors, built before the armed window
// so that freeing any of them inside it drives the live counter negative.
struct Children {
  ASTNode *value;
  ASTNode *condition;
  ASTNode *then_branch;
  ASTNode *else_branch;
  ASTNode *left;
  ASTNode *right;
};

Children make_children(Symbol *x) {
  Children c = {
      ast_node_create_int(1, 1U, 5U), ast_node_create_variable_ref(x, 2U, 4U),
      ast_node_create_block(2U, 9U),  ast_node_create_block(3U, 9U),
      ast_node_create_int(2, 4U, 1U), ast_node_create_real(0.5, 4U, 5U)};
  return c;
}

void destroy_children(Children &c) {
  ast_node_destroy(c.value);
  ast_node_destroy(c.condition);
  ast_node_destroy(c.then_branch);
  ast_node_destroy(c.else_branch);
  ast_node_destroy(c.left);
  ast_node_destroy(c.right);
  c = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}

// Same walk as walk_leaf, with fresh children per k. On failure the
// children must be intact and are freed by the caller; on success `adopt`
// checks the node and clears the children it now owns.
template <typename Make, typename Adopt>
long walk_composite(const char *what, Symbol *x, Make make, Adopt adopt) {
  for (long k = 1; k < kMaxAllocationPoints; ++k) {
    Children c = make_children(x);
    EXPECT_TRUE(c.value != nullptr && c.condition != nullptr &&
                c.then_branch != nullptr && c.else_branch != nullptr &&
                c.left != nullptr && c.right != nullptr)
        << what;
    if (c.value == nullptr || c.condition == nullptr ||
        c.then_branch == nullptr || c.else_branch == nullptr ||
        c.left == nullptr || c.right == nullptr) {
      destroy_children(c);
      return kMaxAllocationPoints;
    }
    ConstructRun run = construct_failing_at(k, [&] { return make(c); });
    if (!run.injected) {
      EXPECT_NE(run.node, nullptr) << what;
      if (run.node != nullptr) {
        adopt(run.node, c);
        ast_node_destroy(run.node);
      }
      destroy_children(c);
      return k;
    }
    EXPECT_EQ(run.node, nullptr) << what << " k=" << k;
    EXPECT_EQ(run.live, 0) << what << " k=" << k;
    // Children were not adopted: they are intact and the caller frees them.
    EXPECT_EQ(c.value->kind, AST_CONSTANT_INT) << what << " k=" << k;
    EXPECT_EQ(c.value->data.int_value, 1) << what << " k=" << k;
    EXPECT_EQ(c.condition->data.variable.symbol, x) << what << " k=" << k;
    EXPECT_EQ(c.then_branch->kind, AST_BLOCK) << what << " k=" << k;
    EXPECT_EQ(c.else_branch->kind, AST_BLOCK) << what << " k=" << k;
    EXPECT_EQ(c.left->data.int_value, 2) << what << " k=" << k;
    EXPECT_DOUBLE_EQ(c.right->data.real_value, 0.5) << what << " k=" << k;
    destroy_children(c);
  }
  ADD_FAILURE() << what << " never succeeded";
  return kMaxAllocationPoints;
}

TEST_F(DatastructOomCovTest, CompositeConstructorsKeepChildrenWithCaller) {
  EXPECT_GT(walk_composite(
                "create_assign", x,
                [this](Children &c) {
                  return ast_node_create_assign(x, c.value, 1U, 1U);
                },
                [this](ASTNode *node, Children &c) {
                  EXPECT_EQ(node->kind, AST_ASSIGN);
                  EXPECT_EQ(node->data.assignment.target, x);
                  EXPECT_EQ(node->data.assignment.value, c.value);
                  c.value = nullptr;
                }),
            1);
  EXPECT_GT(walk_composite(
                "create_if", x,
                [](Children &c) {
                  return ast_node_create_if(c.condition, c.then_branch,
                                            c.else_branch, 2U, 1U);
                },
                [](ASTNode *node, Children &c) {
                  EXPECT_EQ(node->kind, AST_IF);
                  EXPECT_EQ(node->data.if_statement.condition, c.condition);
                  EXPECT_EQ(node->data.if_statement.then_branch, c.then_branch);
                  EXPECT_EQ(node->data.if_statement.else_branch, c.else_branch);
                  c.condition = c.then_branch = c.else_branch = nullptr;
                }),
            1);
  EXPECT_GT(walk_composite(
                "create_binary_op", x,
                [](Children &c) {
                  return ast_node_create_binary_op(AST_ADD, c.left, c.right, 4U,
                                                   3U);
                },
                [](ASTNode *node, Children &c) {
                  EXPECT_EQ(node->kind, AST_ADD);
                  EXPECT_EQ(node->data.binary_op.left, c.left);
                  EXPECT_EQ(node->data.binary_op.right, c.right);
                  c.left = c.right = nullptr;
                }),
            1);
}

} // namespace

#endif // DS_OOM_SANITIZED
