#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "gtest/gtest.h"

#include "bst_test_util.h"

extern "C" {
#include "../src/bst.h"
#include "../src/dynamic_array.h"
#include "../src/parser.h"
#include "../src/semantic.h"
}

/*
 * AI-generated test block — allocation failure injection harness.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Interpose malloc/calloc/realloc/free in this one binary, fail the k-th
 *   allocation inside an armed window and count live blocks; skip cleanly
 *   under sanitizers and wherever the override is not active (valgrind).
 */
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define OOM_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define OOM_SANITIZED 1
#endif
#endif

#ifdef OOM_SANITIZED
TEST(OomFaultTest, SkippedUnderSanitizers) {
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

// fail_at 0 = track only, never fail.
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

class OomFaultTest : public ::testing::Test {
protected:
  void SetUp() override {
    if (!oom::injection_works()) {
      GTEST_SKIP() << "allocator override inactive (e.g. valgrind)";
    }
  }
};

/** Returns a null-terminated copy of @p text for the lexer to borrow. */
std::vector<char> buffer_from(const char *text) {
  return std::vector<char>(text, text + strlen(text) + 1);
}

int compare_int(const void *a, const void *b) {
  const int left = *static_cast<const int *>(a);
  const int right = *static_cast<const int *>(b);
  return (left > right) - (left < right);
}

void *copy_int(const void *value) {
  int *copy = static_cast<int *>(malloc(sizeof(int)));
  if (copy != nullptr) {
    *copy = *static_cast<const int *>(value);
  }
  return copy;
}

void destroy_int(void *value) { free(value); }

// Lexes the first token with the k-th allocation failing. The stream and
// lexer_init run before arming; lexer_destroy runs while still tracking.
struct LexRun {
  LexerStatus status;
  TokenType type;
  bool injected;
  long live;
};

LexRun lex_first_token_failing_at(const char *source, long k) {
  LexRun run = {LEXER_INVALID_ARGUMENT, TOKEN_INVALID, false, 0};
  std::vector<char> buffer = buffer_from(source);
  Lexer lexer;
  if (lexer_init(&lexer, buffer.data()) == LEXER_OK) {
    oom::Arm arm(k);
    run.status = lexer_pls(&lexer);
    run.type = lexer.token.type;
    arm.off();
    run.injected = arm.injected();
    lexer_destroy(&lexer);
    run.live = oom::live;
  }
  return run;
}

struct ParseRun {
  ParserStatus status;
  bool injected;
  long live;
};

// Parses a whole program with the k-th allocation failing. The Program and
// lexer are initialised before arming and destroyed while still tracking.
ParseRun parse_failing_at(const char *source, long k) {
  ParseRun run = {PARSER_INVALID_ARGUMENT, false, 0};
  std::vector<char> buffer = buffer_from(source);
  Lexer lexer;
  Program program;
  if (lexer_init(&lexer, buffer.data()) == LEXER_OK) {
    if (program_init(&program) == PARSER_OK) {
      oom::Arm arm(k);
      run.status = p_program(&lexer, &program);
      arm.off();
      run.injected = arm.injected();
      program_destroy(&program);
      lexer_destroy(&lexer);
      run.live = oom::live;
    } else {
      lexer_destroy(&lexer);
    }
  }
  return run;
}

constexpr const char *kParserSource = "def f(a: Int): Unit =\n"
                                      "  return\n"
                                      "@main\n"
                                      "def main(): Unit =\n"
                                      "  return\n";

// Hand-built `main(): Unit` whose body is
// `[print(1 + 2.5), val x: Double = 1]`; print stays a forward reference.
bool build_semantic_program(Program *program) {
  if (program_init(program) != PARSER_OK) {
    return false;
  }
  FunctionSymbol *main_fn = nullptr;
  FunctionSymbol *print_fn = nullptr;
  if (function_table_get_or_create(&program->functions, "main", &main_fn) !=
          FUNCTION_TABLE_OK ||
      function_table_get_or_create(&program->functions, "print", &print_fn) !=
          FUNCTION_TABLE_OK) {
    return false;
  }
  main_fn->is_defined = true;
  main_fn->is_main = true;
  main_fn->has_explicit_return_type = true;
  main_fn->return_type = type_scalar(TYPE_UNIT);
  SymbolTable *scope = function_symbol_create_local_scope(main_fn);
  main_fn->ast_root = ast_node_create_block(1, 1);
  if (scope == nullptr || main_fn->ast_root == nullptr) {
    return false;
  }
  ASTNode *sum =
      ast_node_create_binary_op(AST_ADD, ast_node_create_int(1, 1, 1),
                                ast_node_create_real(2.5, 1, 1), 1, 1);
  ASTNode *call = ast_node_create_function_call(print_fn, 1, 1);
  if (sum == nullptr || call == nullptr ||
      ast_node_call_append_argument(call, sum) != AST_OK ||
      ast_node_block_append_statement(main_fn->ast_root, call) != AST_OK) {
    return false;
  }
  Symbol *x = nullptr;
  if (symbol_table_define(scope, "x", type_scalar(TYPE_REAL), true, true, &x) !=
      SYMTABLE_OK) {
    return false;
  }
  ASTNode *definition =
      ast_node_create_assign(x, ast_node_create_int(1, 1, 1), 1, 1);
  if (definition == nullptr) {
    return false;
  }
  definition->data.assignment.is_definition = true;
  return ast_node_block_append_statement(main_fn->ast_root, definition) ==
         AST_OK;
}

/// True for the statuses of semantic errors found by the parser (codes 3-6).
bool is_semantic_error(ParserStatus status) {
  return status >= PARSER_UNDEFINED_ERROR && status <= PARSER_TYPE_ERROR;
}

} // namespace

/*
 * AI-generated test block — dynamic_array_reserve allocation failure.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Fail the realloc of a reserve that must grow and check the status and
 *   that the caller's storage, capacity and elements are untouched.
 */
TEST_F(OomFaultTest, DynamicArrayReallocFailureKeepsStorage) {
  void *data = nullptr;
  size_t capacity = 0U;
  ASSERT_EQ(dynamic_array_reserve(&data, &capacity, 3U, sizeof(int)),
            DYNAMIC_ARRAY_OK);
  ASSERT_EQ(capacity, 4U);
  static_cast<int *>(data)[0] = 42;
  void *const before = data;

  DynamicArrayStatus status;
  {
    oom::Arm arm(1);
    status = dynamic_array_reserve(&data, &capacity, 100U, sizeof(int));
  }

  EXPECT_EQ(status, DYNAMIC_ARRAY_OUT_OF_MEMORY);
  EXPECT_EQ(data, before);
  EXPECT_EQ(capacity, 4U);
  EXPECT_EQ(static_cast<int *>(data)[0], 42);
  free(data);
}

/*
 * AI-generated test block — BST allocation failures.
 * Model: Claude (Claude Code).
 * Prompt:
 *   bst_create reports BST_ALLOCATION_FAIL and sets the tree to NULL on
 *   allocation failure; a failed bst_insert must
 *   leave the tree (and its stored copies) unchanged and leak nothing.
 */
TEST_F(OomFaultTest, BstAllocationFailureLeavesTreeUnchanged) {
  BST *failed = nullptr;
  BST_STATUS create_status;
  {
    oom::Arm arm(1);
    create_status = bst_create(compare_int, copy_int, destroy_int, &failed);
  }
  EXPECT_EQ(create_status, BST_ALLOCATION_FAIL);
  EXPECT_EQ(failed, nullptr);

  BST *tree = make_tree(compare_int, copy_int, destroy_int);
  ASSERT_NE(tree, nullptr);
  const int five = 5;
  ASSERT_EQ(bst_insert(tree, &five), BST_OK);
  const void *const stored_five = find_value(tree, &five);
  ASSERT_NE(stored_five, nullptr);

  const int seven = 7;
  long k = 1;
  for (;; ++k) {
    ASSERT_LT(k, kMaxAllocationPoints);
    const void *found;
    BST_STATUS insert_status;
    bool injected;
    long live;
    {
      oom::Arm arm(k);
      insert_status = bst_insert(tree, &seven);
      arm.off();
      injected = arm.injected();
      found = find_value(tree, &seven);
      if (found != nullptr) {
        bst_remove(tree, &seven);
      }
      live = oom::live;
    }
    if (!injected) {
      EXPECT_EQ(insert_status, BST_OK) << "k=" << k;
      EXPECT_NE(found, nullptr) << "insert without failure, k=" << k;
      break;
    }
    EXPECT_EQ(insert_status, BST_ALLOCATION_FAIL) << "k=" << k;
    EXPECT_EQ(found, nullptr) << "k=" << k;
    EXPECT_EQ(find_value(tree, &five), stored_five) << "k=" << k;
    EXPECT_EQ(live, 0) << "k=" << k;
  }
  EXPECT_GT(k, 1) << "bst_insert never allocated";
  bst_destroy(&tree);
}

/*
 * AI-generated test block — symbol table allocation failures.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Walk every allocation of create_root, create_child and define; expect
 *   NULL / SYMTABLE_OUT_OF_MEMORY, no attached child, no binding, no leak.
 */
TEST_F(OomFaultTest, SymbolTableAllocationFailuresAreReported) {
  long k = 1;
  for (;; ++k) {
    ASSERT_LT(k, kMaxAllocationPoints);
    SymbolTable *root;
    bool injected;
    long live;
    {
      oom::Arm arm(k);
      root = symbol_table_create_root();
      arm.off();
      injected = arm.injected();
      symbol_table_destroy(root);
      live = oom::live;
    }
    if (!injected) {
      EXPECT_NE(root, nullptr);
      break;
    }
    EXPECT_EQ(root, nullptr) << "create_root k=" << k;
    EXPECT_EQ(live, 0) << "create_root k=" << k;
  }
  EXPECT_GT(k, 1);

  // A fresh parent per k: a reused one keeps the grown children array, so
  // later k would never reach the child's own allocations.
  for (k = 1;; ++k) {
    ASSERT_LT(k, kMaxAllocationPoints);
    SymbolTable *parent = symbol_table_create_root();
    ASSERT_NE(parent, nullptr);
    SymbolTable *child;
    bool injected;
    size_t child_count;
    {
      oom::Arm arm(k);
      child = symbol_table_create_child(parent);
      arm.off();
      injected = arm.injected();
      child_count = parent->child_count;
    }
    symbol_table_destroy(parent);
    if (!injected) {
      EXPECT_NE(child, nullptr);
      EXPECT_EQ(child_count, 1U);
      break;
    }
    EXPECT_EQ(child, nullptr) << "create_child k=" << k;
    EXPECT_EQ(child_count, 0U) << "create_child k=" << k;
  }
  EXPECT_GT(k, 1);

  SymbolTable *root = symbol_table_create_root();
  ASSERT_NE(root, nullptr);
  for (k = 1;; ++k) {
    ASSERT_LT(k, kMaxAllocationPoints);
    char name[32];
    snprintf(name, sizeof name, "v%ld", k);
    Symbol *symbol = nullptr;
    SymbolTableStatus status;
    bool injected;
    Symbol *bound;
    long live;
    {
      oom::Arm arm(k);
      status = symbol_table_define(root, name, type_scalar(TYPE_INT), true,
                                   false, &symbol);
      arm.off();
      injected = arm.injected();
      bound = symbol_table_lookup_local(root, name);
      live = oom::live;
    }
    if (!injected) {
      EXPECT_EQ(status, SYMTABLE_OK);
      EXPECT_NE(bound, nullptr);
      EXPECT_EQ(bound, symbol);
      break;
    }
    EXPECT_EQ(status, SYMTABLE_OUT_OF_MEMORY) << "define k=" << k;
    EXPECT_EQ(bound, nullptr) << "define k=" << k;
    EXPECT_EQ(live, 0) << "define k=" << k;
  }
  EXPECT_GT(k, 1);
  symbol_table_destroy(root);
}

/*
 * AI-generated test block — function table allocation failures.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Fail get_or_create, add_parameter, add_callee and create_local_scope;
 *   expect FUNCTION_TABLE_OUT_OF_MEMORY / NULL, unchanged counts, no leak.
 */
TEST_F(OomFaultTest, FunctionTableAllocationFailuresAreReported) {
  long k = 1;
  for (;; ++k) {
    ASSERT_LT(k, kMaxAllocationPoints);
    FunctionTable table;
    ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);
    FunctionSymbol *function = nullptr;
    FunctionTableStatus status;
    bool injected;
    size_t count;
    FunctionSymbol *found;
    long live;
    {
      oom::Arm arm(k);
      status = function_table_get_or_create(&table, "f", &function);
      arm.off();
      injected = arm.injected();
      count = table.function_count;
      found = function_table_lookup(&table, "f");
      function_table_destroy(&table);
      live = oom::live;
    }
    if (!injected) {
      EXPECT_EQ(status, FUNCTION_TABLE_OK);
      EXPECT_EQ(count, 1U);
      EXPECT_EQ(found, function);
      break;
    }
    EXPECT_EQ(status, FUNCTION_TABLE_OUT_OF_MEMORY) << "k=" << k;
    EXPECT_EQ(count, 0U) << "k=" << k;
    EXPECT_EQ(found, nullptr) << "k=" << k;
    EXPECT_EQ(live, 0) << "k=" << k;
  }
  EXPECT_GT(k, 1);

  FunctionTable table;
  ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);
  FunctionSymbol *f = nullptr;
  FunctionSymbol *g = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "f", &f), FUNCTION_TABLE_OK);
  ASSERT_EQ(function_table_get_or_create(&table, "g", &g), FUNCTION_TABLE_OK);

  Symbol parameter = {};
  FunctionTableStatus parameter_status;
  {
    oom::Arm arm(1);
    parameter_status = function_symbol_add_parameter(f, &parameter);
  }
  EXPECT_EQ(parameter_status, FUNCTION_TABLE_OUT_OF_MEMORY);
  EXPECT_EQ(f->argument_count, 0U);

  FunctionTableStatus callee_status;
  {
    oom::Arm arm(1);
    callee_status = function_symbol_add_callee(f, g);
  }
  EXPECT_EQ(callee_status, FUNCTION_TABLE_OUT_OF_MEMORY);
  EXPECT_EQ(f->callee_count, 0U);

  SymbolTable *scope;
  {
    oom::Arm arm(1);
    scope = function_symbol_create_local_scope(f);
  }
  EXPECT_EQ(scope, nullptr);
  SymbolTable *retried = function_symbol_create_local_scope(f);
  EXPECT_NE(retried, nullptr);
  EXPECT_EQ(f->local_scope, retried);
  function_table_destroy(&table);
}

/*
 * AI-generated test block — AST allocation failures and ownership.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Constructors return null on allocation failure without leaking; a failed
 *   append returns AST_OUT_OF_MEMORY and the child stays with the caller.
 */
TEST_F(OomFaultTest, AstAllocationFailuresKeepOwnershipWithCaller) {
  ASTNode *literal;
  {
    oom::Arm arm(1);
    literal = ast_node_create_int(1, 1, 1);
  }
  EXPECT_EQ(literal, nullptr);

  long k = 1;
  for (;; ++k) {
    ASSERT_LT(k, kMaxAllocationPoints);
    ASTNode *string;
    bool injected;
    long live;
    {
      oom::Arm arm(k);
      string = ast_node_create_string("abc", 1, 1);
      arm.off();
      injected = arm.injected();
      ast_node_destroy(string);
      live = oom::live;
    }
    if (!injected) {
      EXPECT_NE(string, nullptr);
      break;
    }
    EXPECT_EQ(string, nullptr) << "create_string k=" << k;
    EXPECT_EQ(live, 0) << "create_string k=" << k;
  }
  EXPECT_GT(k, 1);

  ASTNode *block = ast_node_create_block(1, 1);
  ASTNode *statement = ast_node_create_int(7, 2, 3);
  ASSERT_NE(block, nullptr);
  ASSERT_NE(statement, nullptr);
  ASTStatus append_status;
  {
    oom::Arm arm(1);
    append_status = ast_node_block_append_statement(block, statement);
  }
  EXPECT_EQ(append_status, AST_OUT_OF_MEMORY);
  EXPECT_EQ(block->data.block.statement_count, 0U);
  ast_node_destroy(block);
  // Still owned by the caller: readable after the block is gone.
  EXPECT_EQ(statement->kind, AST_CONSTANT_INT);
  EXPECT_EQ(statement->data.int_value, 7);
  ast_node_destroy(statement);

  FunctionTable table;
  ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);
  FunctionSymbol *f = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "f", &f), FUNCTION_TABLE_OK);
  ASTNode *call = ast_node_create_function_call(f, 1, 1);
  ASTNode *argument = ast_node_create_int(8, 1, 3);
  ASSERT_NE(call, nullptr);
  ASSERT_NE(argument, nullptr);
  ASTStatus argument_status;
  {
    oom::Arm arm(1);
    argument_status = ast_node_call_append_argument(call, argument);
  }
  EXPECT_EQ(argument_status, AST_OUT_OF_MEMORY);
  EXPECT_EQ(call->data.call.argument_count, 0U);
  ast_node_destroy(call);
  EXPECT_EQ(argument->kind, AST_CONSTANT_INT);
  EXPECT_EQ(argument->data.int_value, 8);
  ast_node_destroy(argument);
  function_table_destroy(&table);
}

/*
 * AI-generated test block — lexer allocation failure.
 * Model: Claude (Claude Code).
 * Prompt:
 *   A 26-char identifier outgrows the token buffer; every failing allocation
 *   gives LEXER_OUT_OF_MEMORY with a TOKEN_INVALID token and no leak.
 */
TEST_F(OomFaultTest, LexerAllocationFailureIsOutOfMemory) {
  const char *source = "abcdefghijklmnopqrstuvwxyz";
  long k = 1;
  for (;; ++k) {
    ASSERT_LT(k, kMaxAllocationPoints);
    const LexRun run = lex_first_token_failing_at(source, k);
    if (!run.injected) {
      EXPECT_EQ(run.status, LEXER_OK);
      EXPECT_EQ(run.type, TOKEN_IDENTIFIER);
      break;
    }
    EXPECT_EQ(run.status, LEXER_OUT_OF_MEMORY) << "k=" << k;
    EXPECT_EQ(run.type, TOKEN_INVALID) << "k=" << k;
    EXPECT_EQ(run.live, 0) << "k=" << k;
  }
  EXPECT_GT(k, 1);
}

/*
 * AI-generated test block — parser allocation failures.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Fail every allocation of p_program in turn; an allocation failure must
 *   never be reported as a syntax/semantic error, and nothing may leak.
 */
TEST_F(OomFaultTest, ParserAllocationFailureIsNeverSyntaxOrSemanticError) {
  long k = 1;
  for (;; ++k) {
    ASSERT_LT(k, kMaxAllocationPoints);
    const ParseRun run = parse_failing_at(kParserSource, k);
    if (!run.injected) {
      EXPECT_EQ(run.status, PARSER_OK);
      EXPECT_EQ(run.live, 0);
      break;
    }
    EXPECT_NE(run.status, PARSER_OK) << "k=" << k;
    EXPECT_NE(run.status, PARSER_SYNTAX_ERROR) << "k=" << k;
    EXPECT_FALSE(is_semantic_error(run.status)) << "k=" << k;
    EXPECT_EQ(run.live, 0) << "k=" << k;
  }
  EXPECT_GT(k, 1);
}

/*
 * AI-generated test block — lexer OOM status mapping in the parser.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Every allocation failure seen by p_program, also one inside the lexer,
 *   must surface as PARSER_INTERNAL_ERROR (exit code 99).
 */
TEST_F(OomFaultTest, ParserMapsLexerAllocationFailureToInternalError) {
  // Every other allocation failure in parser.c already returns
  // PARSER_INTERNAL_ERROR; IFJ convention (assignment not in repo) is 99.
  long k = 1;
  for (;; ++k) {
    ASSERT_LT(k, kMaxAllocationPoints);
    const ParseRun run = parse_failing_at(kParserSource, k);
    if (!run.injected) {
      EXPECT_EQ(run.status, PARSER_OK);
      break;
    }
    EXPECT_EQ(run.status, PARSER_INTERNAL_ERROR) << "k=" << k;
  }
  EXPECT_GT(k, 1);
}

/*
 * AI-generated test block — semantic allocation failures.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Fail every allocation of semantic_check on a hand-built main with an
 *   implicit Int->Double conversion; each one is SEMANTIC_INTERNAL_ERROR.
 */
TEST_F(OomFaultTest, SemanticAllocationFailureIsInternalError) {
  long k = 1;
  for (;; ++k) {
    ASSERT_LT(k, kMaxAllocationPoints);
    Program program;
    const bool built = build_semantic_program(&program);
    SemanticStatus status = SEMANTIC_OK;
    bool injected = false;
    if (built) {
      oom::Arm arm(k);
      status = semantic_check(&program);
      arm.off();
      injected = arm.injected();
    }
    program_destroy(&program);
    ASSERT_TRUE(built) << "fixture construction failed";
    if (!injected) {
      EXPECT_EQ(status, SEMANTIC_OK);
      break;
    }
    EXPECT_EQ(status, SEMANTIC_INTERNAL_ERROR) << "k=" << k;
  }
  EXPECT_GT(k, 1);
}

#endif // OOM_SANITIZED
