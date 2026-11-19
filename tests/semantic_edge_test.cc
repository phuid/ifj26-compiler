/*
 * AI-generated test block — fixture and AST builders for semantic edge cases.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover the semantic edge cases of IFJ26 that the existing semantic tests
 *   miss: every error code 3-7 with positive and negative cases, scopes,
 *   inference chains, recursion, main rules, all builtins and every operator
 *   type combination. ASTs are built by hand because the expression parser is
 *   a stub; whole-program cases wait as DISABLED known-gap tests.
 */
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "../src/ast.h"
#include "../src/function_table.h"
#include "../src/parser.h"
#include "../src/semantic.h"
#include "../src/symtable.h"
}

namespace {

using Nodes = std::vector<ASTNode *>;
using Types = std::vector<BaseType>;

constexpr BaseType N = TYPE_UNKNOWN, I = TYPE_INT, R = TYPE_REAL,
                   S = TYPE_STRING, U = TYPE_UNIT, B = TYPE_BOOL;
constexpr SemanticStatus OK = SEMANTIC_OK, E3 = SEMANTIC_UNDEFINED,
                         E4 = SEMANTIC_REDEFINITION, E5 = SEMANTIC_BAD_CALL,
                         E6 = SEMANTIC_TYPE_ERROR, E7 = SEMANTIC_OTHER;

ASTNode *integer(long long v) { return ast_node_create_int(v, 1, 1); }
ASTNode *real(double v) { return ast_node_create_real(v, 1, 1); }
ASTNode *str(const char *v) { return ast_node_create_string(v, 1, 1); }
ASTNode *unit() { return ast_node_create_unit(1, 1); }
ASTNode *ref_var(Symbol *s) { return ast_node_create_variable_ref(s, 1, 1); }
ASTNode *assign(Symbol *s, ASTNode *v) {
  return ast_node_create_assign(s, v, 1, 1);
}
ASTNode *define(Symbol *s, ASTNode *v) {
  ASTNode *n = assign(s, v);
  n->data.assignment.is_definition = true;
  return n;
}
ASTNode *ret(ASTNode *v) { return ast_node_create_return(v, 1, 1); }
ASTNode *bin(ASTNodeKind k, ASTNode *l, ASTNode *r) {
  return ast_node_create_binary_op(k, l, r, 1, 1);
}
/// A Boolean value: the only way to get one in the base language.
ASTNode *truthy() { return bin(AST_LESS, integer(1), integer(2)); }
ASTNode *if_(ASTNode *c, ASTNode *t, ASTNode *e = nullptr) {
  return ast_node_create_if(c, t, e, 1, 1);
}
ASTNode *block(const Nodes &xs) {
  ASTNode *b = ast_node_create_block(1, 1);
  for (ASTNode *x : xs) {
    ast_node_block_append_statement(b, x);
  }
  return b;
}
ASTNode *call(FunctionSymbol *f, const Nodes &args = {}) {
  ASTNode *c = ast_node_create_function_call(f, 1, 1);
  for (ASTNode *a : args) {
    ast_node_call_append_argument(c, a);
  }
  return c;
}
/// A literal of type @p t (Bool is `1 < 2`).
ASTNode *value(BaseType t) {
  switch (t) {
  case TYPE_INT:
    return integer(1);
  case TYPE_REAL:
    return real(1.5);
  case TYPE_STRING:
    return str("s");
  case TYPE_UNIT:
    return unit();
  default:
    return truthy();
  }
}
bool widened(const ASTNode *n) { return n->kind == AST_IMPLICIT_INT_TO_REAL; }
/// True when @p t is the scalar type @p b (the unresolved type for
/// TYPE_UNKNOWN).
bool has_type(DataType t, BaseType b) { return type_equal(t, type_scalar(b)); }
/// The value of @p n, seen through the implicit `return` that semantic_check
/// wraps around the last value of a non-Unit body.
ASTNode *tail_value(ASTNode *n) {
  return n->kind == AST_RETURN ? n->data.return_statement.value : n;
}

Symbol *var_in(SymbolTable *scope, const char *name, BaseType type,
               bool constant = true) {
  Symbol *s = nullptr;
  symbol_table_define(scope, name, type_scalar(type), true, constant, &s);
  return s;
}
Symbol *var(FunctionSymbol *f, const char *name, BaseType type,
            bool constant = true) {
  return var_in(f->local_scope, name, type, constant);
}
void add(FunctionSymbol *f, ASTNode *s) {
  ASSERT_EQ(ast_node_block_append_statement(f->ast_root, s), AST_OK);
}
ASTNode *stmt(const FunctionSymbol *f, size_t i) {
  return f->ast_root->data.block.statements[i];
}

FunctionSymbol *ref(Program *program, const char *name) {
  FunctionSymbol *f = nullptr;
  function_table_get_or_create(&program->functions, name, &f);
  return f;
}
/// `def name(params...)[: ret] =` with an empty body; `@main` if @p is_main.
FunctionSymbol *def(Program *program, const char *name, BaseType ret_type = N,
                    const Types &params = {}, bool is_main = false) {
  FunctionSymbol *f = ref(program, name);
  f->is_defined = true;
  f->is_main = is_main;
  function_symbol_create_local_scope(f);
  for (size_t i = 0; i < params.size(); ++i) {
    function_symbol_add_parameter(
        f, var(f, ("p" + std::to_string(i)).c_str(), params[i]));
  }
  f->has_explicit_return_type = ret_type != N;
  f->return_type.type = type_scalar(ret_type);
  f->ast_root = ast_node_create_block(1, 1);
  return f;
}

/** Owns one Program built by hand. */
class SemanticEdgeTest : public ::testing::Test {
protected:
  Program program{};
  void SetUp() override { ASSERT_EQ(program_init(&program), PARSER_OK); }
  void TearDown() override { program_destroy(&program); }
  SemanticStatus check() { return semantic_check(&program); }
  FunctionSymbol *ref(const char *name) { return ::ref(&program, name); }
  FunctionSymbol *def(const char *name, BaseType ret_type = N,
                      const Types &params = {}, bool is_main = false) {
    return ::def(&program, name, ret_type, params, is_main);
  }
  FunctionSymbol *def_main() { return def("main", U, {}, true); }
};

template <class Row>
class SemanticEdgeTableTest : public SemanticEdgeTest,
                              public ::testing::WithParamInterface<Row> {};

const char *type_name(BaseType t) {
  switch (t) {
  case TYPE_INT:
    return "Int";
  case TYPE_REAL:
    return "Double";
  case TYPE_STRING:
    return "String";
  case TYPE_UNIT:
    return "Unit";
  case TYPE_BOOL:
    return "Bool";
  default:
    return "None";
  }
}

} // namespace

/*
 * AI-generated test block — exit codes of the semantic pass.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   SemanticStatus values must be the compiler exit codes of spec chapter 6.
 */
TEST(SemanticEdgeCodeTest, StatusValuesMatchSpecExitCodes) {
  EXPECT_EQ(static_cast<int>(SEMANTIC_OK), 0);
  EXPECT_EQ(static_cast<int>(SEMANTIC_UNDEFINED), 3);
  EXPECT_EQ(static_cast<int>(SEMANTIC_REDEFINITION), 4);
  EXPECT_EQ(static_cast<int>(SEMANTIC_BAD_CALL), 5);
  EXPECT_EQ(static_cast<int>(SEMANTIC_TYPE_ERROR), 6);
  EXPECT_EQ(static_cast<int>(SEMANTIC_OTHER), 7);
  EXPECT_EQ(static_cast<int>(SEMANTIC_INTERNAL_ERROR), 99);
}

/*
 * AI-generated test block — full binary operator type matrix.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Every binary operator against every pair of Int, Double, String, Unit and
 *   Bool, judged by an oracle written from the spec 3 tables; the pairs
 *   already in semantic_gap_test.cc (OperatorTable) are skipped.
 */
namespace {

struct OpRow {
  ASTNodeKind op;
  BaseType left, right;
};

bool is_numeric(BaseType t) { return t == I || t == R; }
bool is_arithmetic(ASTNodeKind k) {
  return k == AST_ADD || k == AST_SUBTRACT || k == AST_MULTIPLY ||
         k == AST_DIVIDE;
}

/// Result type of `left op right` per spec 3 and 5.2, or N for error 6.
BaseType spec_result(const OpRow &r) {
  const bool equality = r.op == AST_EQUAL || r.op == AST_NOT_EQUAL;
  if (r.left == B || r.right == B) {
    // BOOLTHEN (spec 5.2): two Booleans compare with == and != only.
    return equality && r.left == B && r.right == B ? B : N;
  }
  if (is_numeric(r.left) && is_numeric(r.right)) {
    if (!is_arithmetic(r.op)) {
      return B;
    }
    return r.left == R || r.right == R ? R : I;
  }
  if (r.left == S && r.right == S) {
    if (!is_arithmetic(r.op)) {
      return B; // lexicographic comparison
    }
    return r.op == AST_ADD ? S : N; // only + concatenates
  }
  return equality && r.left == U && r.right == U ? B : N;
}

/// Pairs covered by OperatorTable in semantic_gap_test.cc.
const OpRow kCoveredOps[] = {
    {AST_SUBTRACT, I, I},  {AST_DIVIDE, I, I},        {AST_DIVIDE, R, I},
    {AST_SUBTRACT, I, R},  {AST_LESS, I, R},          {AST_LESS_EQUAL, I, R},
    {AST_GREATER, I, R},   {AST_GREATER_EQUAL, R, I}, {AST_EQUAL, I, R},
    {AST_NOT_EQUAL, I, I}, {AST_ADD, S, S},           {AST_LESS, S, S},
    {AST_EQUAL, U, U},     {AST_SUBTRACT, S, S},      {AST_MULTIPLY, S, S},
    {AST_DIVIDE, S, S},    {AST_LESS, U, U},          {AST_EQUAL, I, S},
    {AST_ADD, R, S},       {AST_LESS, B, I},          {AST_ADD, I, B},
    {AST_EQUAL, B, B},     {AST_NOT_EQUAL, S, S},
};

std::vector<OpRow> uncovered_op_rows() {
  const ASTNodeKind ops[] = {AST_ADD,      AST_SUBTRACT,      AST_MULTIPLY,
                             AST_DIVIDE,   AST_LESS,          AST_LESS_EQUAL,
                             AST_GREATER,  AST_GREATER_EQUAL, AST_EQUAL,
                             AST_NOT_EQUAL};
  const BaseType types[] = {I, R, S, U, B};
  std::vector<OpRow> rows;
  for (ASTNodeKind op : ops) {
    for (BaseType l : types) {
      for (BaseType r : types) {
        bool covered = false;
        for (const OpRow &c : kCoveredOps) {
          covered |= c.op == op && c.left == l && c.right == r;
        }
        if (!covered) {
          rows.push_back({op, l, r});
        }
      }
    }
  }
  return rows;
}

const char *op_name(ASTNodeKind op) {
  switch (op) {
  case AST_ADD:
    return "Add";
  case AST_SUBTRACT:
    return "Sub";
  case AST_MULTIPLY:
    return "Mul";
  case AST_DIVIDE:
    return "Div";
  case AST_LESS:
    return "Less";
  case AST_LESS_EQUAL:
    return "LessEqual";
  case AST_GREATER:
    return "Greater";
  case AST_GREATER_EQUAL:
    return "GreaterEqual";
  case AST_EQUAL:
    return "Equal";
  case AST_NOT_EQUAL:
    return "NotEqual";
  default:
    return "Other";
  }
}

std::string op_row_name(const ::testing::TestParamInfo<OpRow> &info) {
  const OpRow &r = info.param;
  return std::string(op_name(r.op)) + "_" + type_name(r.left) + "_" +
         type_name(r.right);
}

} // namespace

class SemanticEdgeOperatorTest : public SemanticEdgeTableTest<OpRow> {};

TEST_P(SemanticEdgeOperatorTest, FollowsSpecOperatorTable) {
  const OpRow &r = GetParam();
  const BaseType expected = spec_result(r);
  FunctionSymbol *m = def_main();
  ASTNode *e = bin(r.op, value(r.left), value(r.right));
  const bool in_if = expected == B; // a Bool result is used as a condition
  add(m, in_if ? if_(e, block({unit()}), block({unit()})) : e);

  ASSERT_EQ(check(), expected == N ? E6 : OK);
  if (expected == N) {
    return;
  }
  ASTNode *n = in_if ? stmt(m, 0)->data.if_statement.condition : stmt(m, 0);
  EXPECT_TRUE(has_type(n->inferred_type.type, expected));
  EXPECT_EQ(widened(n->data.binary_op.left), r.left == I && r.right == R);
  EXPECT_EQ(widened(n->data.binary_op.right), r.right == I && r.left == R);
}
INSTANTIATE_TEST_SUITE_P(SemanticEdge, SemanticEdgeOperatorTest,
                         ::testing::ValuesIn(uncovered_op_rows()), op_row_name);

/*
 * AI-generated test block — nested expressions and operand errors.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Int-to-Double conversion wraps a whole Int subexpression, a type error of
 *   an outer operator after a valid inner one is 6, and an error in the right
 *   operand is reported unchanged.
 */
TEST_F(SemanticEdgeTest, NestedExpressionWidensWholeSubexpression) {
  // val x = (1 + 2) * 2.5
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", N);
  add(m, define(x, bin(AST_MULTIPLY, bin(AST_ADD, integer(1), integer(2)),
                       real(2.5))));

  ASSERT_EQ(check(), OK);
  EXPECT_TRUE(has_type(x->type.type, R));
  ASTNode *product = stmt(m, 0)->data.assignment.value;
  ASSERT_TRUE(widened(product->data.binary_op.left));
  ASTNode *sum = product->data.binary_op.left->data.conversion.operand;
  EXPECT_EQ(sum->kind, AST_ADD);
  EXPECT_TRUE(has_type(sum->inferred_type.type, I));
  EXPECT_FALSE(widened(sum->data.binary_op.left));
  EXPECT_FALSE(widened(product->data.binary_op.right));
}

TEST_F(SemanticEdgeTest, ChainedConcatenationWithIntIsError6) {
  // "a" + "b" + 1      (no automatic conversion to String, spec 3)
  add(def_main(), bin(AST_ADD, bin(AST_ADD, str("a"), str("b")), integer(1)));
  EXPECT_EQ(check(), E6);
}

TEST_F(SemanticEdgeTest, ChainedIntSumWithStringIsError6) {
  // 1 + 2 + "s"
  add(def_main(), bin(AST_ADD, bin(AST_ADD, integer(1), integer(2)), str("s")));
  EXPECT_EQ(check(), E6);
}

TEST_F(SemanticEdgeTest, RightOperandErrorIsReported) {
  // val x = 1 + x      (x used inside its own initialization)
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", N);
  add(m, define(x, bin(AST_ADD, integer(1), ref_var(x))));
  EXPECT_EQ(check(), E3);
}

/*
 * AI-generated test block — if conditions and branches.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   A condition of type Double, String or Unit is error 6 (Int is covered),
 *   and an error inside the else branch is reported unchanged.
 */
class SemanticEdgeConditionTest : public SemanticEdgeTableTest<BaseType> {};

TEST_P(SemanticEdgeConditionTest, NonBooleanConditionIsError6) {
  FunctionSymbol *m = def_main();
  add(m, if_(value(GetParam()), block({unit()}), block({unit()})));
  EXPECT_EQ(check(), E6);
}
INSTANTIATE_TEST_SUITE_P(SemanticEdge, SemanticEdgeConditionTest,
                         ::testing::Values(R, S, U),
                         [](const ::testing::TestParamInfo<BaseType> &info) {
                           return std::string(type_name(info.param));
                         });

TEST_F(SemanticEdgeTest, ErrorInElseBranchIsReported) {
  // val k = 1; if 1 < 2 then () else k = 2      -> 7
  FunctionSymbol *m = def_main();
  Symbol *k = var(m, "k", I);
  add(m, define(k, integer(1)));
  add(m, if_(truthy(), block({unit()}), block({assign(k, integer(2))})));
  EXPECT_EQ(check(), E7);
}

/*
 * AI-generated test block — variables, inference and immutability.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Spec 2.2: the inferred type never changes, inference follows a chain of
 *   variables, Unit is a valid variable type, a wrong-typed write to a val or
 *   a parameter is 7 (more specific than 6, spec 6), a use before the
 *   definition is 3 and a shadowing binding keeps its own type.
 */
TEST_F(SemanticEdgeTest, VarInferredIntRejectsLaterDoubleAssignment) {
  // var x = 1; x = 2.5
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", N, false);
  add(m, define(x, integer(1)));
  add(m, assign(x, real(2.5)));
  EXPECT_EQ(check(), E6);
  EXPECT_TRUE(has_type(x->type.type, I));
}

TEST_F(SemanticEdgeTest, VarInferredDoubleWidensLaterIntAssignment) {
  // var d = 1.5; d = 1
  FunctionSymbol *m = def_main();
  Symbol *d = var(m, "d", N, false);
  add(m, define(d, real(1.5)));
  add(m, assign(d, integer(1)));
  ASSERT_EQ(check(), OK);
  EXPECT_TRUE(has_type(d->type.type, R));
  EXPECT_TRUE(widened(stmt(m, 1)->data.assignment.value));
}

TEST_F(SemanticEdgeTest, InferenceChainThroughVariables) {
  // val a = 1; val b = a; val c = b * 2.5
  FunctionSymbol *m = def_main();
  Symbol *a = var(m, "a", N), *b = var(m, "b", N), *c = var(m, "c", N);
  add(m, define(a, integer(1)));
  add(m, define(b, ref_var(a)));
  add(m, define(c, bin(AST_MULTIPLY, ref_var(b), real(2.5))));

  ASSERT_EQ(check(), OK);
  EXPECT_TRUE(has_type(a->type.type, I));
  EXPECT_TRUE(has_type(b->type.type, I));
  EXPECT_TRUE(has_type(c->type.type, R));
  EXPECT_FALSE(widened(stmt(m, 1)->data.assignment.value));
  EXPECT_TRUE(widened(stmt(m, 2)->data.assignment.value->data.binary_op.left));
}

TEST_F(SemanticEdgeTest, UnitVariablesAreUsable) {
  // val u = (); var w: Unit = print("x"); w = u; if u == w then () else ()
  FunctionSymbol *m = def_main();
  Symbol *u = var(m, "u", N), *w = var(m, "w", U, false);
  add(m, define(u, unit()));
  add(m, define(w, call(ref("print"), {str("x")})));
  add(m, assign(w, ref_var(u)));
  add(m, if_(bin(AST_EQUAL, ref_var(u), ref_var(w)), block({unit()}),
             block({unit()})));
  ASSERT_EQ(check(), OK);
  EXPECT_TRUE(has_type(u->type.type, U));
}

TEST_F(SemanticEdgeTest, UnitAnnotationRejectsIntInitializer) {
  // val u: Unit = 1     (only Int -> Double is implicit, spec 1.3)
  FunctionSymbol *m = def_main();
  add(m, define(var(m, "u", U), integer(1)));
  EXPECT_EQ(check(), E6);
}

TEST_F(SemanticEdgeTest, AssigningWrongTypeToValIsError7) {
  // val k = 1; k = "s"      7 and 6 both apply, 7 is more specific
  FunctionSymbol *m = def_main();
  Symbol *k = var(m, "k", N);
  add(m, define(k, integer(1)));
  add(m, assign(k, str("s")));
  EXPECT_EQ(check(), E7);
}

TEST_F(SemanticEdgeTest, AssigningWrongTypeToParameterIsError7) {
  // def f(n: Int): Unit = n = "s"
  def_main();
  FunctionSymbol *f = def("f", U, {I});
  Symbol *n = f->parameters[0];
  add(f, assign(n, str("s")));
  EXPECT_EQ(check(), E7);
}

TEST_F(SemanticEdgeTest, UseBeforeDefinitionInSameBlockIsError3) {
  // print(x); val x = 1     (also: the error of an argument is kept, not 5)
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", N);
  add(m, call(ref("print"), {ref_var(x)}));
  add(m, define(x, integer(1)));
  EXPECT_EQ(check(), E3);
}

TEST_F(SemanticEdgeTest, ShadowedVariableInBranchKeepsOwnType) {
  // val x = 1
  // if 1 < 2 then { val x = "s"; x + "t" } else ()
  // x + 1
  FunctionSymbol *m = def_main();
  Symbol *outer = var(m, "x", N);
  SymbolTable *then_scope = symbol_table_create_child(m->local_scope);
  ASSERT_NE(then_scope, nullptr);
  Symbol *inner = var_in(then_scope, "x", N);
  ASSERT_NE(inner, outer);
  add(m, define(outer, integer(1)));
  add(m, if_(truthy(),
             block({define(inner, str("s")),
                    bin(AST_ADD, ref_var(inner), str("t"))}),
             block({unit()})));
  add(m, bin(AST_ADD, ref_var(outer), integer(1)));

  ASSERT_EQ(check(), OK);
  EXPECT_TRUE(has_type(outer->type.type, I));
  EXPECT_TRUE(has_type(inner->type.type, S));
  EXPECT_TRUE(has_type(stmt(m, 2)->inferred_type.type, I));
}

/*
 * AI-generated test block — function bodies, return and unreachable code.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   One table {annotation, body of an uncalled f, status, return type}:
 *   unreachable code after return is still checked (spec 2.3), rule 3 is
 *   applied recursively to an if nested in a branch, return nested in a
 *   not-annotated function, explicit Unit discards branch values.
 */
namespace {

struct BodyRow {
  const char *name;
  BaseType annotation;
  void (*body)(Program *, FunctionSymbol *);
  SemanticStatus status;
  BaseType result;
};

const BodyRow kBodies[] = {
    // Unreachable code after `return` is checked like any other code.
    {"UnreachableStringAfterReturn", I,
     [](Program *, FunctionSymbol *f) {
       add(f, ret(integer(1)));
       add(f, str("x"));
     },
     E6, N},
    {"UnreachableReturnOfString", I,
     [](Program *, FunctionSymbol *f) {
       add(f, ret(integer(1)));
       add(f, ret(str("x")));
     },
     E6, N},
    {"UnreachableValAssignment", I,
     [](Program *, FunctionSymbol *f) {
       Symbol *k = var(f, "k", N);
       add(f, ret(integer(1)));
       add(f, define(k, integer(1)));
       add(f, assign(k, integer(2)));
     },
     E7, N},
    {"UnreachableCompatibleValue", I,
     [](Program *, FunctionSymbol *f) {
       add(f, ret(integer(1)));
       add(f, integer(2));
     },
     OK, I},
    // The last call yields Unit, which is not an Int.
    {"IntBodyEndingWithUnitCall", I,
     [](Program *p, FunctionSymbol *f) {
       add(f, call(ref(p, "print"), {str("x")}));
     },
     E6, N},
    // Rule 3 is checked recursively in an if ending a branch.
    {"NestedIfAllReturn", I,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(), block({ret(integer(1))}),
                  block({if_(truthy(), block({ret(integer(2))}),
                             block({ret(integer(3))}))})));
     },
     OK, I},
    {"NestedIfWithUnitBranchInIntFunction", I,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(), block({ret(integer(1))}),
                  block({if_(truthy(), block({ret(integer(2))}),
                             block({unit()}))})));
     },
     E6, N},
    {"NestedIfAllUnitInfersUnit", N,
     [](Program *p, FunctionSymbol *f) {
       add(f, if_(truthy(), block({unit()}),
                  block({if_(truthy(), block({unit()}),
                             block({call(ref(p, "print"), {str("x")})}))})));
     },
     OK, U},
    {"NestedIfWithIntBranchCannotInfer", N,
     [](Program *, FunctionSymbol *f) {
       add(f,
           if_(truthy(), block({unit()}),
               block({if_(truthy(), block({unit()}), block({integer(1)}))})));
     },
     E6, N},
    {"EmptyBranchCountsAsUnit", N,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(), block({}), block({unit()})));
     },
     OK, U},
    {"NestedBlockReturnInBranch", I,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(), block({ret(integer(1))}),
                  block({block({ret(integer(2))})})));
     },
     OK, I},
    // Explicit Unit: the value of every branch is evaluated and discarded.
    {"ExplicitUnitDiscardsBranchValues", U,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(), block({integer(1)}), block({str("s")})));
     },
     OK, U},
    // `return` anywhere requires an explicit return type.
    {"NestedReturnWithoutAnnotation", N,
     [](Program *, FunctionSymbol *f) {
       add(f,
           if_(truthy(), block({unit()}), block({block({ret(integer(1))})})));
     },
     E6, N},
    {"NestedBareReturnWithoutAnnotation", N,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(), block({ret(nullptr)}), block({unit()})));
     },
     E6, N},
};

} // namespace

class SemanticEdgeBodyTest : public SemanticEdgeTableTest<BodyRow> {};

TEST_P(SemanticEdgeBodyTest, InfersOrRejectsResult) {
  const BodyRow &row = GetParam();
  def_main();
  FunctionSymbol *f = def("f", row.annotation);
  row.body(&program, f);
  ASSERT_EQ(check(), row.status);
  if (row.status == OK) {
    EXPECT_TRUE(has_type(f->return_type.type, row.result));
  }
}
INSTANTIATE_TEST_SUITE_P(SemanticEdge, SemanticEdgeBodyTest,
                         ::testing::ValuesIn(kBodies),
                         [](const ::testing::TestParamInfo<BodyRow> &info) {
                           return std::string(info.param.name);
                         });

/*
 * AI-generated test block — Int-to-Double conversion of returned values.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   In a Double function the conversion is placed on the innermost last
 *   value of nested blocks, around a last call of an Int function and on the
 *   Int `return` of a final if only.
 */
TEST_F(SemanticEdgeTest, DoubleFunctionWidensInnermostLastValue) {
  // def f(): Double = { { 1 } }
  def_main();
  FunctionSymbol *f = def("f", R);
  add(f, block({block({integer(1)})}));
  ASSERT_EQ(check(), OK);
  ASTNode *outer = stmt(f, 0);
  ASSERT_EQ(outer->kind, AST_BLOCK);
  ASTNode *inner = outer->data.block.statements[0];
  ASSERT_EQ(inner->kind, AST_BLOCK);
  ASTNode *last = tail_value(inner->data.block.statements[0]);
  ASSERT_TRUE(widened(last));
  EXPECT_EQ(last->data.conversion.operand->kind, AST_CONSTANT_INT);
}

TEST_F(SemanticEdgeTest, DoubleFunctionWidensLastCallOfIntFunction) {
  // def g(): Int = 1      def f(): Double = g()
  def_main();
  FunctionSymbol *g = def("g", I);
  add(g, integer(1));
  FunctionSymbol *f = def("f", R);
  add(f, call(g));
  ASSERT_EQ(check(), OK);
  ASTNode *last = tail_value(stmt(f, 0));
  ASSERT_TRUE(widened(last));
  EXPECT_EQ(last->data.conversion.operand->kind, AST_FUNCTION_CALL);
}

TEST_F(SemanticEdgeTest, DoubleFunctionWidensReturnInFinalIf) {
  // def f(): Double = if 1 < 2 then return 1 else return 2.5
  def_main();
  FunctionSymbol *f = def("f", R);
  add(f, if_(truthy(), block({ret(integer(1))}), block({ret(real(2.5))})));
  ASSERT_EQ(check(), OK);
  ASTNode *node = stmt(f, 0);
  ASTNode *then_branch = node->data.if_statement.then_branch;
  ASTNode *else_branch = node->data.if_statement.else_branch;
  ASTNode *then_ret = then_branch->data.block.statements[0];
  ASTNode *else_ret = else_branch->data.block.statements[0];
  EXPECT_TRUE(widened(then_ret->data.return_statement.value));
  EXPECT_FALSE(widened(else_ret->data.return_statement.value));
}

/*
 * AI-generated test block — recursion and the call graph.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   A not-annotated caller of a recursive annotated function is not on a
 *   cycle and infers its type; a self-call inside a call argument is a cycle;
 *   @main may be called recursively and from other functions.
 */
TEST_F(SemanticEdgeTest, NotAnnotatedCallerOfRecursiveFunctionInfersType) {
  // def f(n: Int): Int = { val r = f(n); r }     def g() = f(1)
  FunctionSymbol *m = def_main();
  FunctionSymbol *f = def("f", I, {I});
  Symbol *n = f->parameters[0];
  Symbol *r = var(f, "r", N);
  add(f, define(r, call(f, {ref_var(n)})));
  add(f, ref_var(r));
  FunctionSymbol *g = def("g");
  add(g, call(f, {integer(1)}));
  add(m, call(g));
  ASSERT_EQ(check(), OK);
  EXPECT_TRUE(has_type(g->return_type.type, I));
}

TEST_F(SemanticEdgeTest, SelfCallInsideCallArgumentIsCycle) {
  // def f() = print(f())
  def_main();
  FunctionSymbol *f = def("f");
  add(f, call(ref("print"), {call(f)}));
  EXPECT_EQ(check(), E6);
}

TEST_F(SemanticEdgeTest, MainMayBeCalledRecursivelyAndFromOtherFunctions) {
  // @main def main(): Unit = main()      def g(): Unit = main()
  FunctionSymbol *m = def_main();
  add(m, call(m));
  FunctionSymbol *g = def("g", U);
  add(g, call(m));
  EXPECT_EQ(check(), OK);
}

/*
 * AI-generated test block — the entry function.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Spec 2: @main is found by the annotation, not the name; an explicit
 *   non-Unit type is 6; an inferred Unit is accepted.
 */
TEST_F(SemanticEdgeTest, ExplicitIntMainIsError6) {
  FunctionSymbol *m = def("main", I, {}, true);
  add(m, integer(1));
  EXPECT_EQ(check(), E6);
}

TEST_F(SemanticEdgeTest, EntryFunctionNameIsArbitrary) {
  // @main def runFoo(): Unit = ()      def main(): Int = 1
  add(def("runFoo", U, {}, true), unit());
  add(def("main", I), integer(1));
  EXPECT_EQ(check(), OK);
}

TEST_F(SemanticEdgeTest, FunctionNamedMainWithoutAnnotationIsError3) {
  add(def("main", U), unit());
  EXPECT_EQ(check(), E3);
}

TEST_F(SemanticEdgeTest, NotAnnotatedMainEndingWithPrintIsOk) {
  // @main def main() = print("x")
  FunctionSymbol *m = def("main", N, {}, true);
  add(m, call(ref("print"), {str("x")}));
  ASSERT_EQ(check(), OK);
  EXPECT_TRUE(has_type(m->return_type.type, U));
}

/*
 * AI-generated test block — user function arguments.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Every parameter type Int/Double/String/Unit against every argument type
 *   Int/Double/String/Unit/Bool (pairs of the existing tests skipped): only
 *   the same type or Int for Double is accepted, otherwise 5 (spec 4).
 */
namespace {

struct ArgRow {
  BaseType parameter, argument;
};

std::vector<ArgRow> uncovered_arg_rows() {
  std::vector<ArgRow> rows;
  for (BaseType p : {I, R, S, U}) {
    for (BaseType a : {I, R, S, U, B}) {
      bool covered =
          (p == R && a == I) || (p == I && a == B) || (p == I && a == R);
      if (!covered) {
        rows.push_back({p, a});
      }
    }
  }
  return rows;
}

} // namespace

class SemanticEdgeArgumentTest : public SemanticEdgeTableTest<ArgRow> {};

TEST_P(SemanticEdgeArgumentTest, AcceptsOnlyCompatibleArgument) {
  const ArgRow &row = GetParam();
  FunctionSymbol *m = def_main();
  FunctionSymbol *g = def("g", U, {row.parameter});
  add(g, unit());
  add(m, call(g, {value(row.argument)}));
  EXPECT_EQ(check(), row.parameter == row.argument ? OK : E5);
}
INSTANTIATE_TEST_SUITE_P(SemanticEdge, SemanticEdgeArgumentTest,
                         ::testing::ValuesIn(uncovered_arg_rows()),
                         [](const ::testing::TestParamInfo<ArgRow> &info) {
                           return std::string(type_name(info.param.parameter)) +
                                  "_" + type_name(info.param.argument);
                         });

TEST_F(SemanticEdgeTest, TooManyArgumentsToParameterlessFunctionIsError5) {
  FunctionSymbol *m = def_main();
  FunctionSymbol *g = def("g", U);
  add(g, unit());
  add(m, call(g, {integer(1)}));
  EXPECT_EQ(check(), E5);
}

/*
 * AI-generated test block — builtin functions.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   The argument counts and types of spec chapter 4 not yet in BuiltinTable,
 *   the builtin return type enforced on an annotated variable, and a user
 *   definition of each builtin name other than print is error 4.
 */
namespace {

struct BuiltinRow {
  const char *name;
  Types arguments;
  BaseType annotation; ///< Type of the variable the result initializes.
  SemanticStatus status;
};

const BuiltinRow kBuiltins[] = {
    {"readLine", {I}, N, E5},     {"readDouble", {S}, N, E5},
    {"readInt", {}, R, OK},       {"readDouble", {}, I, E6},
    {"readLine", {}, I, E6},      {"toStr", {}, N, E5},
    {"toStr", {S, S}, N, E5},     {"toStr", {R}, S, OK},
    {"toStr", {S}, S, OK},        {"print", {S}, U, OK},
    {"print", {I}, N, OK},        {"toInt", {S}, N, E5},
    {"toInt", {U}, N, E5},        {"toInt", {}, N, E5},
    {"toInt", {R, R}, N, E5},     {"toInt", {R}, R, OK},
    {"strLength", {}, N, E5},     {"strLength", {R}, N, E5},
    {"strLength", {U}, N, E5},    {"strLength", {S, S}, N, E5},
    {"strLength", {S}, S, E6},    {"subStr", {I, I, I}, N, E5},
    {"subStr", {S, R, I}, N, E5}, {"subStr", {S, I, R}, N, E5},
    {"subStr", {S, I, S}, N, E5}, {"subStr", {S, I, I, I}, N, E5},
    {"subStr", {}, N, E5},        {"ord", {I, I}, N, E5},
    {"ord", {S}, N, E5},          {"ord", {S, I, I}, N, E5},
    {"ord", {S, S}, N, E5},       {"ord", {S, I}, R, OK},
    {"chr", {R}, N, E5},          {"chr", {}, N, E5},
    {"chr", {I, I}, N, E5},       {"chr", {U}, N, E5},
    {"chr", {I}, I, E6},
};

std::string builtin_row_name(const ::testing::TestParamInfo<BuiltinRow> &info) {
  return std::string(info.param.name) + "_" + std::to_string(info.index);
}

} // namespace

class SemanticEdgeBuiltinTest : public SemanticEdgeTableTest<BuiltinRow> {};

TEST_P(SemanticEdgeBuiltinTest, ChecksSpecSignature) {
  const BuiltinRow &row = GetParam();
  FunctionSymbol *m = def_main();
  Symbol *v = var(m, "v", row.annotation);
  Nodes args;
  for (BaseType t : row.arguments) {
    args.push_back(value(t));
  }
  add(m, define(v, call(ref(row.name), args)));
  ASSERT_EQ(check(), row.status);
  if (row.status != OK) {
    return;
  }
  ASTNode *init = stmt(m, 0)->data.assignment.value;
  FunctionSymbol *builtin = ref(row.name);
  bool widen = row.annotation == R && has_type(builtin->return_type.type, I);
  EXPECT_EQ(widened(init), widen);
}
INSTANTIATE_TEST_SUITE_P(SemanticEdge, SemanticEdgeBuiltinTest,
                         ::testing::ValuesIn(kBuiltins), builtin_row_name);

class SemanticEdgeBuiltinRedefinitionTest
    : public SemanticEdgeTableTest<const char *> {};

TEST_P(SemanticEdgeBuiltinRedefinitionTest, UserDefinitionIsError4) {
  // def <builtin>(): Unit = ()
  def_main();
  add(def(GetParam(), U), unit());
  EXPECT_EQ(check(), E4);
}
INSTANTIATE_TEST_SUITE_P(
    SemanticEdge, SemanticEdgeBuiltinRedefinitionTest,
    ::testing::Values("readLine", "readInt", "readDouble", "toStr", "toInt",
                      "strLength", "subStr", "ord", "chr"),
    [](const ::testing::TestParamInfo<const char *> &info) {
      return std::string(info.param);
    });

/*
 * AI-generated test block — whole-program semantic outcomes (known gaps).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Real source through p_program and semantic_check for the scope rules of
 *   spec 2.2 and the spec example program. They need the expression parser
 *   (p_expression is a stub) and while support, so they stay DISABLED.
 */
namespace {

/** Parses @p source into its own Program and releases the buffer. */
struct ParsedProgram {
  Program program{};
  ParserStatus status = PARSER_INTERNAL_ERROR;

  explicit ParsedProgram(const char *source) {
    if (program_init(&program) != PARSER_OK) {
      return;
    }
    std::vector<char> buffer(source, source + strlen(source) + 1);
    Lexer lexer;
    if (lexer_init(&lexer, buffer.data()) == LEXER_OK) {
      status = p_program(&lexer, &program);
      lexer_destroy(&lexer);
    }
  }
  ~ParsedProgram() { program_destroy(&program); }
};

} // namespace

// Every source starts with the prolog required by spec 2 (not parsed yet).
TEST(SemanticEdgeKnownGapTest, DISABLED_SpecFactorialExampleIsAccepted) {
  ParsedProgram p("import ifj26.*\n"
                  "\n"
                  "def factorial(n: Int): Int =\n"
                  "  var result = 1\n"
                  "  var i: Int = n\n"
                  "  while i > 1 do\n"
                  "    result = result * i\n"
                  "    i = i - 1\n"
                  "  result\n"
                  "\n"
                  "@main def runFoo(): Unit =\n"
                  "  val n = readInt()\n"
                  "  val result = factorial(n)\n"
                  "  print(result)\n"
                  "  print(\"\\n\")\n");
  ASSERT_EQ(p.status, PARSER_OK);
  EXPECT_EQ(semantic_check(&p.program), OK);
}

TEST(SemanticEdgeKnownGapTest, DISABLED_ShadowingInBranchIsAccepted) {
  ParsedProgram p("import ifj26.*\n"
                  "@main def main(): Unit =\n"
                  "  val x = 1\n"
                  "  if x < 2 then\n"
                  "    val x = \"s\"\n"
                  "    print(x)\n"
                  "  else\n"
                  "    print(x)\n");
  ASSERT_EQ(p.status, PARSER_OK);
  EXPECT_EQ(semantic_check(&p.program), OK);
}

TEST(SemanticEdgeKnownGapTest, DISABLED_BranchVariableIsNotVisibleAfterIf) {
  // spec 2.2/2.4: the branch is its own scope -> error 3
  ParsedProgram p("import ifj26.*\n"
                  "@main def main(): Unit =\n"
                  "  if 1 < 2 then\n"
                  "    val y = 1\n"
                  "  else\n"
                  "    ()\n"
                  "  print(y)\n");
  EXPECT_EQ(p.status, PARSER_UNDEFINED_ERROR);
}

TEST(SemanticEdgeKnownGapTest, DISABLED_UseBeforeLaterDefinitionIsRejected) {
  // spec 2.2: visible only from the definition on -> error 3
  ParsedProgram p("import ifj26.*\n"
                  "@main def main(): Unit =\n"
                  "  print(x)\n"
                  "  val x = 1\n");
  EXPECT_EQ(p.status, PARSER_UNDEFINED_ERROR);
}

TEST(SemanticEdgeKnownGapTest, DISABLED_WritesToValAndParameterAreError7) {
  const char *sources[] = {
      "import ifj26.*\n@main def main(): Unit =\n  val x = 1\n  x = 2\n",
      "import ifj26.*\ndef f(n: Int): Unit =\n  n = 2\n"
      "@main def main(): Unit =\n  f(1)\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    ParsedProgram p(source);
    ASSERT_EQ(p.status, PARSER_OK);
    EXPECT_EQ(semantic_check(&p.program), E7);
  }
}
