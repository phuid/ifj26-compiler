#include <cfloat>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#if defined(__APPLE__)
#include <malloc/malloc.h>
#endif

extern "C" {
#include "../src/ast.h"
#include "../src/function_table.h"
#include "../src/symtable.h"
#include "../src/type.h"
}

/*
 * AI-generated test block — shared helpers for data-model edge cases.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Edge cases for the IFJ26 data model (ast.c, type.c, function_table.c)
 *   that the existing suites skip: positions and inferred types of every
 *   constructor, extreme payloads, heap balance of destroy, deep trees,
 *   dependency-graph shapes, and function-table lifecycle. Only add what
 *   is missing; known bugs are already reported elsewhere.
 */
namespace {

#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(memory_sanitizer)
#define MODEL_EDGE_SANITIZED 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define MODEL_EDGE_SANITIZED 1
#endif

/** Bytes currently allocated by malloc, or SIZE_MAX when not measurable. */
size_t HeapBytesInUse() {
#if defined(__APPLE__) && !defined(MODEL_EDGE_SANITIZED)
  malloc_statistics_t stats;
  malloc_zone_statistics(nullptr, &stats);
  return stats.size_in_use;
#else
  // glibc keeps freed chunks in tcache as "in use"; no exact counter here.
  return SIZE_MAX;
#endif
}

#define SKIP_WITHOUT_HEAP_COUNTER()                                            \
  do {                                                                         \
    if (HeapBytesInUse() == SIZE_MAX) {                                        \
      GTEST_SKIP() << "no exact heap counter on this platform";                \
    }                                                                          \
  } while (0)

/** Runs @p body and returns the heap growth it left behind. */
template <typename Body> long long HeapGrowth(Body body) {
  const size_t before = HeapBytesInUse();
  body();
  const size_t after = HeapBytesInUse();
  return static_cast<long long>(after) - static_cast<long long>(before);
}

ASTNode *Int(long long value) { return ast_node_create_int(value, 1U, 1U); }

/** Defines parameter @p name in the local scope of @p fn and appends it. */
Symbol *AddParameter(FunctionSymbol *fn, const std::string &name,
                     BaseType base) {
  SymbolTable *scope = function_symbol_create_local_scope(fn);
  Symbol *symbol = nullptr;
  if (scope == nullptr ||
      symbol_table_define(scope, name.c_str(), type_scalar(base), true, true,
                          &symbol) != SYMTABLE_OK ||
      function_symbol_add_parameter(fn, symbol) != FUNCTION_TABLE_OK) {
    return nullptr;
  }
  return symbol;
}

/** Owns a root scope with symbol `x` and a function table with `f`. */
class ModelFixture : public ::testing::Test {
protected:
  void SetUp() override {
    scope = symbol_table_create_root();
    ASSERT_NE(scope, nullptr);
    ASSERT_EQ(symbol_table_define(scope, "x", type_scalar(TYPE_INT), true,
                                  false, &x),
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
  FunctionTable table{};
  FunctionSymbol *f = nullptr;
};

} // namespace

/*
 * AI-generated test block — every constructor stores kind, position, type.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Table over all AST constructors: kind, line/column at the boundaries
 *   0, 1 and SIZE_MAX are stored verbatim, literals get their concrete
 *   type, all other nodes start TYPE_UNKNOWN, and no constructor creates a
 *   hidden dependency edge (empty dependents, symbol type untouched).
 */
namespace {

struct Position {
  size_t line;
  size_t column;
};

struct CtorCase {
  const char *name;
  ASTNodeKind kind;
  BaseType type;
  ASTNode *(*make)(Symbol *x, FunctionSymbol *f, Position at);
};

const CtorCase kCtorCases[] = {
    {"int", AST_CONSTANT_INT, TYPE_INT,
     [](Symbol *, FunctionSymbol *, Position at) {
       return ast_node_create_int(0, at.line, at.column);
     }},
    {"real", AST_CONSTANT_REAL, TYPE_REAL,
     [](Symbol *, FunctionSymbol *, Position at) {
       return ast_node_create_real(0.0, at.line, at.column);
     }},
    {"string", AST_CONSTANT_STRING, TYPE_STRING,
     [](Symbol *, FunctionSymbol *, Position at) {
       return ast_node_create_string("s", at.line, at.column);
     }},
    {"unit", AST_CONSTANT_UNIT, TYPE_UNIT,
     [](Symbol *, FunctionSymbol *, Position at) {
       return ast_node_create_unit(at.line, at.column);
     }},
    {"variable_ref", AST_VARIABLE_REF, TYPE_UNKNOWN,
     [](Symbol *x, FunctionSymbol *, Position at) {
       return ast_node_create_variable_ref(x, at.line, at.column);
     }},
    {"call", AST_FUNCTION_CALL, TYPE_UNKNOWN,
     [](Symbol *, FunctionSymbol *f, Position at) {
       return ast_node_create_function_call(f, at.line, at.column);
     }},
    {"block", AST_BLOCK, TYPE_UNKNOWN,
     [](Symbol *, FunctionSymbol *, Position at) {
       return ast_node_create_block(at.line, at.column);
     }},
    {"assign", AST_ASSIGN, TYPE_UNKNOWN,
     [](Symbol *x, FunctionSymbol *, Position at) {
       return ast_node_create_assign(x, Int(1), at.line, at.column);
     }},
    {"return", AST_RETURN, TYPE_UNKNOWN,
     [](Symbol *, FunctionSymbol *, Position at) {
       return ast_node_create_return(nullptr, at.line, at.column);
     }},
    {"if", AST_IF, TYPE_UNKNOWN,
     [](Symbol *, FunctionSymbol *, Position at) {
       return ast_node_create_if(Int(1), ast_node_create_block(1U, 1U),
                                 nullptr, at.line, at.column);
     }},
    {"not_equal", AST_NOT_EQUAL, TYPE_UNKNOWN,
     [](Symbol *, FunctionSymbol *, Position at) {
       return ast_node_create_binary_op(AST_NOT_EQUAL, Int(1), Int(2), at.line,
                                        at.column);
     }},
    {"int_to_real", AST_IMPLICIT_INT_TO_REAL, TYPE_REAL,
     [](Symbol *, FunctionSymbol *, Position at) {
       return ast_node_create_implicit_int_to_real(
           ast_node_create_int(1, at.line, at.column));
     }},
};

class AstConstructorEdgeTest : public ModelFixture,
                               public ::testing::WithParamInterface<CtorCase> {
};

} // namespace

TEST_P(AstConstructorEdgeTest, StoresKindPositionAndInitialType) {
  const CtorCase &c = GetParam();
  const Position positions[] = {
      {0U, 0U}, {1U, 1U}, {SIZE_MAX, SIZE_MAX}, {SIZE_MAX - 1U, 7U}};
  for (const Position &at : positions) {
    ASTNode *node = c.make(x, f, at);
    ASSERT_NE(node, nullptr) << c.name << " @" << at.line;
    EXPECT_EQ(node->kind, c.kind) << c.name;
    EXPECT_EQ(node->line, at.line) << c.name;
    EXPECT_EQ(node->column, at.column) << c.name;
    EXPECT_TRUE(type_equal(node->inferred_type.type, type_scalar(c.type)))
        << c.name;
    EXPECT_EQ(node->inferred_type.dependents, nullptr) << c.name;
    EXPECT_EQ(node->inferred_type.dependent_count, 0U) << c.name;
    EXPECT_FALSE(node->inferred_type.resolving) << c.name;
    ast_node_destroy(node);
  }
  EXPECT_TRUE(type_equal(x->type.type, type_scalar(TYPE_INT)));
  EXPECT_EQ(x->type.dependent_count, 0U);
  EXPECT_TRUE(type_equal(f->return_type.type, type_scalar(TYPE_UNKNOWN)));
  EXPECT_EQ(f->return_type.dependent_count, 0U);
}

INSTANTIATE_TEST_SUITE_P(
    AllConstructors, AstConstructorEdgeTest, ::testing::ValuesIn(kCtorCases),
    [](const ::testing::TestParamInfo<CtorCase> &info) {
      return std::string(info.param.name);
    });

/*
 * AI-generated test block — extreme literal payloads and rejected kinds.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Real literals keep NaN, infinities, -0.0, the smallest subnormal and
 *   DBL_MAX bit-exactly; string literals copy long and non-ASCII text up to
 *   the first NUL; create_binary_op rejects every non-binary kind not yet
 *   covered (including an out-of-range enum value) and keeps the operands.
 */
TEST(AstPayloadEdgeTest, RealLiteralKeepsSpecialValues) {
  const double values[] = {std::nan(""), INFINITY, -INFINITY,
                           -0.0,         DBL_MAX,  std::nextafter(0.0, 1.0)};
  for (double value : values) {
    ASTNode *node = ast_node_create_real(value, 1U, 1U);
    ASSERT_NE(node, nullptr);
    const double stored = node->data.real_value;
    if (std::isnan(value)) {
      EXPECT_TRUE(std::isnan(stored));
    } else {
      EXPECT_EQ(stored, value);
      EXPECT_EQ(std::signbit(stored), std::signbit(value)) << value;
    }
    EXPECT_TRUE(type_equal(node->inferred_type.type, type_scalar(TYPE_REAL)));
    ast_node_destroy(node);
  }
}

TEST(AstPayloadEdgeTest, StringLiteralCopiesLongAndNonAsciiText) {
  const std::string long_text(100000U, 'q');
  const char utf8[] = "\xC5\xBElu\xC5\xA5ou\xC4\x8Dk\xC3\xBD k\xC5\xAF\xC5\x88";
  const char raw_escapes[] = "a\\n\\t\\\"\\x41";
  const char embedded_nul[] = {'a', 'b', '\0', 'c', '\0'};
  for (const char *text :
       {long_text.c_str(), utf8, raw_escapes, &embedded_nul[0]}) {
    ASTNode *node = ast_node_create_string(text, 2U, 3U);
    ASSERT_NE(node, nullptr);
    EXPECT_NE(node->data.string_value, text);
    EXPECT_STREQ(node->data.string_value, text);
    ast_node_destroy(node);
  }
}

TEST(AstPayloadEdgeTest, BinaryOpRejectsRemainingNonBinaryKinds) {
  ASTNode *left = Int(4);
  ASTNode *right = Int(5);
  ASSERT_TRUE(left != nullptr && right != nullptr);
  for (ASTNodeKind kind :
       {AST_CONSTANT_REAL, AST_CONSTANT_STRING, AST_CONSTANT_UNIT,
        AST_VARIABLE_REF, AST_FUNCTION_CALL, static_cast<ASTNodeKind>(-1),
        static_cast<ASTNodeKind>(AST_IMPLICIT_INT_TO_REAL + 1)}) {
    EXPECT_EQ(ast_node_create_binary_op(kind, left, right, 1U, 1U), nullptr)
        << kind;
  }
  EXPECT_EQ(left->data.int_value, 4);
  EXPECT_EQ(right->data.int_value, 5);
  ast_node_destroy(right);
  ast_node_destroy(left);
}

/*
 * AI-generated test block — destroy releases every AST-owned payload.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Measure net heap growth (exact malloc zone counter, macOS only; skipped
 *   elsewhere and under sanitizers) around create+destroy of every node
 *   kind, empty and grown argument/statement arrays, and a mixed tree.
 *   Net growth must be zero. The inferred_type dependents leak is a known
 *   bug (oom_fault_test.cc) and is not exercised here.
 */
TEST_F(ModelFixture, AstDestroyLeavesNoHeapGrowthForAnyKind) {
  SKIP_WITHOUT_HEAP_COUNTER();
  std::vector<long long> growth;
  for (const CtorCase &c : kCtorCases) {
    growth.push_back(HeapGrowth([&] {
      ast_node_destroy(c.make(x, f, Position{1U, 1U}));
    }));
  }
  for (size_t i = 0U; i < growth.size(); i++) {
    EXPECT_EQ(growth[i], 0) << kCtorCases[i].name;
  }
}

TEST_F(ModelFixture, AstDestroyFreesGrownArgumentAndStatementArrays) {
  SKIP_WITHOUT_HEAP_COUNTER();
  constexpr int kCount = 1000;
  bool appended = true;
  const long long growth = HeapGrowth([&] {
    ASTNode *call = ast_node_create_function_call(f, 1U, 1U);
    ASTNode *block = ast_node_create_block(1U, 1U);
    for (int i = 0; i < kCount; i++) {
      appended &= ast_node_call_append_argument(
                      call, ast_node_create_string("arg", 1U, 1U)) == AST_OK;
      appended &= ast_node_block_append_statement(
                      block, ast_node_create_unit(1U, 1U)) == AST_OK;
    }
    appended &= ast_node_block_append_statement(block, call) == AST_OK;
    ast_node_destroy(block);
  });
  EXPECT_TRUE(appended);
  EXPECT_EQ(growth, 0);
}

TEST_F(ModelFixture, AstDestroyFreesEveryChildOfMixedTree) {
  SKIP_WITHOUT_HEAP_COUNTER();
  const long long growth = HeapGrowth([&] {
    ASTNode *call = ast_node_create_function_call(f, 1U, 1U);
    ast_node_call_append_argument(call, ast_node_create_string("a", 1U, 1U));
    ASTNode *sum = ast_node_create_binary_op(
        AST_ADD, ast_node_create_implicit_int_to_real(Int(1)),
        ast_node_create_real(2.5, 1U, 1U), 1U, 1U);
    ASTNode *then_block = ast_node_create_block(1U, 1U);
    ast_node_block_append_statement(then_block,
                                    ast_node_create_assign(x, sum, 1U, 1U));
    ASTNode *else_block = ast_node_create_block(1U, 1U);
    ast_node_block_append_statement(else_block,
                                    ast_node_create_return(call, 1U, 1U));
    ASTNode *cond = ast_node_create_binary_op(
        AST_GREATER_EQUAL, ast_node_create_variable_ref(x, 1U, 1U),
        ast_node_create_string("b", 1U, 1U), 1U, 1U);
    ast_node_destroy(ast_node_create_if(cond, then_block, else_block, 1U, 1U));
  });
  EXPECT_EQ(growth, 0);
}

/*
 * AI-generated test block — deep and wide trees.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Long realistic shapes: a left-deep `1 + 1 + ... + 1` chain and an
 *   else-if ladder of depth 10000 are built and destroyed without crashing;
 *   10000 arguments and statements keep source order.
 */
TEST(AstDepthEdgeTest, LeftDeepBinaryChainDestroys) {
  constexpr int kDepth = 10000;
  ASTNode *chain = Int(0);
  for (int i = 1; i <= kDepth; i++) {
    ASTNode *next = ast_node_create_binary_op(AST_ADD, chain, Int(i), 1U, 1U);
    ASSERT_NE(next, nullptr) << i;
    chain = next;
  }
  ASTNode *leftmost = chain;
  for (int i = 0; i < kDepth; i++) {
    EXPECT_EQ(leftmost->data.binary_op.right->data.int_value, kDepth - i);
    leftmost = leftmost->data.binary_op.left;
  }
  EXPECT_EQ(leftmost->kind, AST_CONSTANT_INT);
  EXPECT_EQ(leftmost->data.int_value, 0);
  ast_node_destroy(chain);
}

TEST(AstDepthEdgeTest, DeepElseIfLadderDestroys) {
  constexpr int kDepth = 10000;
  ASTNode *ladder = ast_node_create_block(1U, 1U);
  ASSERT_NE(ladder, nullptr);
  for (int i = 0; i < kDepth; i++) {
    ASTNode *cond = ast_node_create_binary_op(AST_LESS, Int(i), Int(0), 1U, 1U);
    ASTNode *next = ast_node_create_if(cond, ast_node_create_block(1U, 1U),
                                       ladder, static_cast<size_t>(i), 1U);
    ASSERT_NE(next, nullptr) << i;
    ladder = next;
  }
  EXPECT_EQ(ladder->line, static_cast<size_t>(kDepth - 1));
  ast_node_destroy(ladder);
}

TEST(AstDepthEdgeTest, WideBlockAndCallKeepOrder) {
  constexpr long long kCount = 10000;
  FunctionTable table{};
  FunctionSymbol *g = nullptr;
  ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);
  ASSERT_EQ(function_table_get_or_create(&table, "g", &g), FUNCTION_TABLE_OK);
  ASTNode *call = ast_node_create_function_call(g, 1U, 1U);
  ASTNode *block = ast_node_create_block(1U, 1U);
  ASSERT_TRUE(call != nullptr && block != nullptr);
  for (long long i = 0; i < kCount; i++) {
    ASSERT_EQ(ast_node_call_append_argument(call, Int(i)), AST_OK);
    ASSERT_EQ(ast_node_block_append_statement(block, Int(-i)), AST_OK);
  }
  ASSERT_EQ(call->data.call.argument_count, static_cast<size_t>(kCount));
  ASSERT_EQ(block->data.block.statement_count, static_cast<size_t>(kCount));
  EXPECT_GE(call->data.call.argument_capacity, static_cast<size_t>(kCount));
  for (long long i = 0; i < kCount; i++) {
    EXPECT_EQ(call->data.call.arguments[i]->data.int_value, i);
    EXPECT_EQ(block->data.block.statements[i]->data.int_value, -i);
  }
  ast_node_destroy(call);
  ast_node_destroy(block);
  function_table_destroy(&table);
}

/*
 * AI-generated test block — type graph edge cases.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover the type dependency graph shapes the other suites skip: self
 *   edge on a resolved entity, TYPE_UNKNOWN on a resolved entity, a
 *   pre-resolved dependent of the same type, depend on a resolved source
 *   that reaches stored edges (and their conflicts), fan-in, conflict
 *   position among siblings, a 1000-node ring, a ring with a conflicting
 *   exit, a 10000-node chain, and destroy in the middle of a chain.
 */
namespace {

/** Owns @c n initialised entities and destroys them at scope exit. */
class Graph {
public:
  explicit Graph(size_t n) : nodes_(n) {
    for (TypeEntity &node : nodes_) {
      type_entity_init(&node);
    }
  }
  ~Graph() {
    for (TypeEntity &node : nodes_) {
      type_entity_destroy(&node);
    }
  }
  Graph(const Graph &) = delete;
  Graph &operator=(const Graph &) = delete;
  TypeEntity *operator[](size_t index) { return &nodes_[index]; }
  size_t size() const { return nodes_.size(); }
  bool AnyResolving() const {
    for (const TypeEntity &node : nodes_) {
      if (node.resolving) {
        return true;
      }
    }
    return false;
  }

private:
  std::vector<TypeEntity> nodes_;
};

} // namespace

TEST(TypeGraphEdgeTest, SelfEdgeOnResolvedEntityStoresNoEdge) {
  Graph g(1U);
  ASSERT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_UNIT)), TYPE_RESOLVE_OK);
  EXPECT_EQ(type_entity_depend(g[0], g[0]), TYPE_RESOLVE_OK);
  EXPECT_EQ(g[0]->dependent_count, 0U);
  EXPECT_TRUE(type_equal(g[0]->type, type_scalar(TYPE_UNIT)));
}

TEST(TypeGraphEdgeTest, UnknownOnResolvedEntityIsRejectedAndKeepsType) {
  Graph g(2U);
  ASSERT_EQ(type_entity_depend(g[0], g[1]), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_STRING)),
            TYPE_RESOLVE_OK);
  EXPECT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_UNKNOWN)),
            TYPE_RESOLVE_INVALID_ARGUMENT);
  EXPECT_TRUE(type_equal(g[0]->type, type_scalar(TYPE_STRING)));
  EXPECT_TRUE(type_equal(g[1]->type, type_scalar(TYPE_STRING)));
  EXPECT_EQ(g[0]->dependent_count, 1U);
}

TEST(TypeGraphEdgeTest, PreResolvedDependentOfSameTypeIsNoConflict) {
  Graph g(3U);
  ASSERT_EQ(type_entity_depend(g[0], g[1]), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_depend(g[1], g[2]), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(g[1], type_scalar(TYPE_INT)), TYPE_RESOLVE_OK);
  EXPECT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_INT)), TYPE_RESOLVE_OK);
  EXPECT_TRUE(type_equal(g[2]->type, type_scalar(TYPE_INT)));
  EXPECT_FALSE(g.AnyResolving());
}

TEST(TypeGraphEdgeTest, DependOnResolvedSourceFollowsStoredEdges) {
  // `val y = x` recorded late, while `val z = y` was already waiting on y.
  Graph g(4U);
  ASSERT_EQ(type_entity_depend(g[1], g[2]), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_depend(g[2], g[3]), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_REAL)), TYPE_RESOLVE_OK);
  EXPECT_EQ(type_entity_depend(g[0], g[1]), TYPE_RESOLVE_OK);
  EXPECT_TRUE(type_equal(g[3]->type, type_scalar(TYPE_REAL)));
  EXPECT_EQ(g[0]->dependent_count, 0U);
}

TEST(TypeGraphEdgeTest, DependOnResolvedSourceReportsTransitiveConflict) {
  Graph g(3U);
  ASSERT_EQ(type_entity_depend(g[1], g[2]), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(g[2], type_scalar(TYPE_STRING)),
            TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_INT)), TYPE_RESOLVE_OK);
  EXPECT_EQ(type_entity_depend(g[0], g[1]), TYPE_RESOLVE_CONFLICT);
  EXPECT_TRUE(type_equal(g[2]->type, type_scalar(TYPE_STRING)));
  EXPECT_FALSE(g.AnyResolving());
}

TEST(TypeGraphEdgeTest, FanInAcceptsSameTypeAndRejectsOther) {
  // Three sources (e.g. three `return`s) feed one dependent.
  Graph g(4U);
  for (size_t source = 0U; source < 3U; source++) {
    ASSERT_EQ(type_entity_depend(g[source], g[3]), TYPE_RESOLVE_OK);
  }
  EXPECT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_INT)), TYPE_RESOLVE_OK);
  EXPECT_EQ(type_entity_resolve(g[1], type_scalar(TYPE_INT)), TYPE_RESOLVE_OK);
  EXPECT_EQ(type_entity_resolve(g[2], type_scalar(TYPE_REAL)),
            TYPE_RESOLVE_CONFLICT);
  EXPECT_TRUE(type_equal(g[3]->type, type_scalar(TYPE_INT)));
  EXPECT_FALSE(g.AnyResolving());
}

TEST(TypeGraphEdgeTest, ConflictIsReportedWhateverSiblingPosition) {
  for (size_t conflicting = 0U; conflicting < 3U; conflicting++) {
    Graph g(4U);
    for (size_t child = 1U; child <= 3U; child++) {
      ASSERT_EQ(type_entity_depend(g[0], g[child]), TYPE_RESOLVE_OK);
    }
    ASSERT_EQ(type_entity_resolve(g[conflicting + 1U], type_scalar(TYPE_BOOL)),
              TYPE_RESOLVE_OK);
    EXPECT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_INT)),
              TYPE_RESOLVE_CONFLICT)
        << "conflicting sibling " << conflicting;
    EXPECT_TRUE(type_equal(g[conflicting + 1U]->type, type_scalar(TYPE_BOOL)));
    EXPECT_FALSE(g.AnyResolving());
  }
}

TEST(TypeGraphEdgeTest, LongRingResolvesEveryNode) {
  constexpr size_t kCount = 1000U;
  Graph g(kCount);
  for (size_t i = 0U; i < kCount; i++) {
    ASSERT_EQ(type_entity_depend(g[i], g[(i + 1U) % kCount]), TYPE_RESOLVE_OK);
  }
  ASSERT_EQ(type_entity_resolve(g[kCount / 2U], type_scalar(TYPE_STRING)),
            TYPE_RESOLVE_OK);
  for (size_t i = 0U; i < kCount; i++) {
    EXPECT_TRUE(type_equal(g[i]->type, type_scalar(TYPE_STRING))) << i;
  }
  EXPECT_FALSE(g.AnyResolving());
  EXPECT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_STRING)),
            TYPE_RESOLVE_OK);
  EXPECT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_INT)),
            TYPE_RESOLVE_CONFLICT);
}

TEST(TypeGraphEdgeTest, RingWithConflictingExitClearsResolvingFlags) {
  // 0 -> 1 -> 2 -> 0 ring, 2 -> 3 exit already typed Int.
  Graph g(4U);
  ASSERT_EQ(type_entity_resolve(g[3], type_scalar(TYPE_INT)), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_depend(g[0], g[1]), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_depend(g[1], g[2]), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_depend(g[2], g[0]), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_depend(g[2], g[3]), TYPE_RESOLVE_OK);
  EXPECT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_REAL)),
            TYPE_RESOLVE_CONFLICT);
  EXPECT_TRUE(type_equal(g[3]->type, type_scalar(TYPE_INT)));
  EXPECT_FALSE(g.AnyResolving());
}

TEST(TypeGraphEdgeTest, LongChainResolvesToTheEnd) {
  constexpr size_t kCount = 10000U;
  Graph g(kCount);
  for (size_t i = 0U; i + 1U < kCount; i++) {
    ASSERT_EQ(type_entity_depend(g[i], g[i + 1U]), TYPE_RESOLVE_OK);
  }
  ASSERT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_UNIT)), TYPE_RESOLVE_OK);
  EXPECT_TRUE(type_equal(g[kCount - 1U]->type, type_scalar(TYPE_UNIT)));
  EXPECT_TRUE(type_equal(g[kCount / 2U]->type, type_scalar(TYPE_UNIT)));
  EXPECT_FALSE(g.AnyResolving());
}

TEST(TypeGraphEdgeTest, DestroyInMiddleCutsPropagation) {
  Graph g(3U);
  ASSERT_EQ(type_entity_depend(g[0], g[1]), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_depend(g[1], g[2]), TYPE_RESOLVE_OK);
  type_entity_destroy(g[1]);
  EXPECT_EQ(g[1]->dependent_count, 0U);
  EXPECT_EQ(g[1]->dependents, nullptr);
  ASSERT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_INT)), TYPE_RESOLVE_OK);
  EXPECT_TRUE(type_equal(g[2]->type, type_scalar(TYPE_UNKNOWN)));
  EXPECT_TRUE(type_equal(g[0]->type, type_scalar(TYPE_INT)));
}

TEST(TypeGraphEdgeTest, DestroyedSourceLeavesDependentTypes) {
  Graph g(3U);
  ASSERT_EQ(type_entity_depend(g[0], g[1]), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_depend(g[1], g[2]), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(g[0], type_scalar(TYPE_BOOL)), TYPE_RESOLVE_OK);
  type_entity_destroy(g[0]);
  EXPECT_TRUE(type_equal(g[0]->type, type_scalar(TYPE_UNKNOWN)));
  EXPECT_TRUE(type_equal(g[1]->type, type_scalar(TYPE_BOOL)));
  EXPECT_TRUE(type_equal(g[2]->type, type_scalar(TYPE_BOOL)));
  EXPECT_EQ(type_entity_resolve(g[1], type_scalar(TYPE_INT)),
            TYPE_RESOLVE_CONFLICT);
}

/*
 * AI-generated test block — function table names, signatures, lifecycle.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Function-table edge cases the other suites skip: empty and very long
 *   names, one record per name (spec 2.3: no overloading, so a second
 *   get_or_create with a different arity reuses the record), errors reset
 *   the out parameter, 0/1/many parameters including TYPE_ANY/TYPE_UNIT,
 *   independent callee lists for mutual recursion, destroy idempotence and
 *   reuse, and a zero-heap-growth destroy of a fully populated table.
 */
TEST_F(ModelFixture, EmptyAndLongNamesAreDistinctRecords) {
  const std::string long_name(10000U, 'n');
  FunctionSymbol *empty = nullptr;
  FunctionSymbol *longest = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "", &empty),
            FUNCTION_TABLE_OK);
  ASSERT_EQ(function_table_get_or_create(&table, long_name.c_str(), &longest),
            FUNCTION_TABLE_OK);
  ASSERT_TRUE(empty != nullptr && longest != nullptr);
  EXPECT_NE(empty, f);
  EXPECT_NE(empty, longest);
  EXPECT_STREQ(empty->name, "");
  EXPECT_EQ(std::string(longest->name), long_name);
  EXPECT_EQ(function_table_lookup(&table, ""), empty);
  EXPECT_EQ(function_table_lookup(&table, long_name.c_str()), longest);
  const std::string one_shorter(long_name.size() - 1U, 'n');
  EXPECT_EQ(function_table_lookup(&table, one_shorter.c_str()), nullptr);
  EXPECT_EQ(function_table_lookup(&table, "f "), nullptr);
  EXPECT_EQ(table.function_count, 3U);
}

TEST_F(ModelFixture, SameNameNeverCreatesAnOverload) {
  ASSERT_NE(AddParameter(f, "a", TYPE_INT), nullptr);
  Symbol *second = AddParameter(f, "b", TYPE_STRING);
  ASSERT_NE(second, nullptr);
  f->is_defined = true;
  f->has_explicit_return_type = true;
  ASSERT_EQ(type_entity_resolve(&f->return_type, type_scalar(TYPE_REAL)),
            TYPE_RESOLVE_OK);

  FunctionSymbol *again = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "f", &again),
            FUNCTION_TABLE_OK);
  EXPECT_EQ(again, f);
  EXPECT_EQ(table.function_count, 1U);
  ASSERT_EQ(again->argument_count, 2U);
  EXPECT_EQ(again->parameters[1], second);
  EXPECT_TRUE(type_is_scalar(again->parameters[1]->type.type, TYPE_STRING));
  EXPECT_TRUE(again->is_defined);
  EXPECT_TRUE(type_equal(again->return_type.type, type_scalar(TYPE_REAL)));
}

TEST_F(ModelFixture, FailedGetOrCreateResetsOutParameter) {
  FunctionSymbol *out = f;
  EXPECT_EQ(function_table_get_or_create(nullptr, "g", &out),
            FUNCTION_TABLE_INVALID_ARGUMENT);
  EXPECT_EQ(out, nullptr);
  out = f;
  EXPECT_EQ(function_table_get_or_create(&table, nullptr, &out),
            FUNCTION_TABLE_INVALID_ARGUMENT);
  EXPECT_EQ(out, nullptr);
  EXPECT_EQ(table.function_count, 1U);
}

TEST_F(ModelFixture, ParameterListsOfZeroOneAndManyTypes) {
  FunctionSymbol *none = nullptr;
  FunctionSymbol *one = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "none", &none),
            FUNCTION_TABLE_OK);
  ASSERT_EQ(function_table_get_or_create(&table, "one", &one),
            FUNCTION_TABLE_OK);
  EXPECT_EQ(none->argument_count, 0U);
  EXPECT_EQ(none->parameters, nullptr);
  Symbol *only = AddParameter(one, "p", TYPE_ANY);
  ASSERT_NE(only, nullptr);
  ASSERT_EQ(one->argument_count, 1U);
  EXPECT_EQ(one->parameters[0], only);
  EXPECT_TRUE(type_is_scalar(one->parameters[0]->type.type, TYPE_ANY));
  const BaseType all[] = {TYPE_UNIT, TYPE_BOOL, TYPE_ANY,   TYPE_UNKNOWN,
                          TYPE_INT,  TYPE_REAL, TYPE_STRING};
  for (size_t round = 0U; round < 100U; round++) {
    for (size_t k = 0U; k < 7U; k++) {
      const std::string name = "p" + std::to_string(round * 7U + k);
      ASSERT_NE(AddParameter(f, name, all[k]), nullptr) << name;
    }
  }
  ASSERT_EQ(f->argument_count, 700U);
  EXPECT_GE(f->parameter_capacity, 700U);
  for (size_t i = 0U; i < f->argument_count; i++) {
    EXPECT_STREQ(f->parameters[i]->name, ("p" + std::to_string(i)).c_str());
    EXPECT_TRUE(type_equal(f->parameters[i]->type.type,
                           type_scalar(all[i % 7U])))
        << i;
  }
  EXPECT_EQ(none->argument_count, 0U);
}

TEST_F(ModelFixture, MutualRecursionKeepsSeparateCalleeLists) {
  FunctionSymbol *g = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "g", &g), FUNCTION_TABLE_OK);
  ASSERT_EQ(function_symbol_add_callee(f, g), FUNCTION_TABLE_OK);
  ASSERT_EQ(function_symbol_add_callee(g, f), FUNCTION_TABLE_OK);
  ASSERT_EQ(function_symbol_add_callee(g, g), FUNCTION_TABLE_OK);
  ASSERT_EQ(function_symbol_add_callee(f, g), FUNCTION_TABLE_OK);
  ASSERT_EQ(f->callee_count, 1U);
  ASSERT_EQ(g->callee_count, 2U);
  EXPECT_EQ(f->callees[0], g);
  EXPECT_EQ(g->callees[0], f);
  EXPECT_EQ(g->callees[1], g);
}

TEST(FunctionTableEdgeTest, DestroyIsIdempotentAndTableIsReusable) {
  FunctionTable table{};
  FunctionSymbol *old_fn = nullptr;
  ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);
  ASSERT_EQ(function_table_get_or_create(&table, "old", &old_fn),
            FUNCTION_TABLE_OK);
  function_table_destroy(&table);
  EXPECT_EQ(table.functions, nullptr);
  EXPECT_EQ(table.function_count, 0U);
  EXPECT_EQ(table.function_capacity, 0U);
  function_table_destroy(&table);
  EXPECT_EQ(function_table_lookup(&table, "old"), nullptr);
  FunctionSymbol *fresh = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "fresh", &fresh),
            FUNCTION_TABLE_OK);
  EXPECT_EQ(function_table_lookup(&table, "fresh"), fresh);
  EXPECT_EQ(table.function_count, 1U);
  function_table_destroy(&table);
}

namespace {

/** Fills @p fn with every owned part: params, callees, scope, AST, deps. */
bool PopulateFunction(FunctionSymbol *fn, FunctionSymbol *callee) {
  bool ok = AddParameter(fn, "param", TYPE_INT) != nullptr;
  ok &= function_symbol_add_callee(fn, callee) == FUNCTION_TABLE_OK;
  SymbolTable *scope = function_symbol_create_local_scope(fn);
  Symbol *local = nullptr;
  ok &= scope != nullptr &&
        symbol_table_define(scope, "local", type_unknown(), true, false,
                            &local) == SYMTABLE_OK;
  ok &= local != nullptr && type_entity_depend(&fn->return_type,
                                               &local->type) == TYPE_RESOLVE_OK;
  ASTNode *body = ast_node_create_block(1U, 1U);
  ok &= ast_node_block_append_statement(
            body, ast_node_create_return(ast_node_create_variable_ref(local, 2U,
                                                                      5U),
                                         2U, 3U)) == AST_OK;
  ok &= function_symbol_set_ast_root(fn, body) == FUNCTION_TABLE_OK;
  return ok;
}

} // namespace

TEST(FunctionTableEdgeTest, DestroyOfPopulatedTableLeavesNoHeapGrowth) {
  SKIP_WITHOUT_HEAP_COUNTER();
  bool populated = true;
  const long long growth = HeapGrowth([&] {
    FunctionTable table{};
    function_table_init(&table);
    FunctionSymbol *fns[3] = {nullptr, nullptr, nullptr};
    const char *names[] = {"main", "helper", "a_rather_long_function_name"};
    for (size_t i = 0U; i < 3U; i++) {
      populated &= function_table_get_or_create(&table, names[i], &fns[i]) ==
                   FUNCTION_TABLE_OK;
    }
    for (size_t i = 0U; i < 3U && populated; i++) {
      populated &= PopulateFunction(fns[i], fns[(i + 1U) % 3U]);
    }
    function_table_destroy(&table);
  });
  EXPECT_TRUE(populated);
  EXPECT_EQ(growth, 0);
}
