#include "gtest/gtest.h"

extern "C" {
#include "../src/ast.h"
#include "../src/function_table.h"
#include "../src/parser.h"
#include "../src/semantic.h"
#include "../src/symtable.h"
}

#include <string>
#include <vector>

namespace {

using Nodes = std::vector<ASTNode *>;
using Types = std::vector<BaseType>;

// AST builders; programs are built by hand at position 1:1.
ASTNode *integer(long long v) { return ast_node_create_int(v, 1, 1); }
ASTNode *real(double v) { return ast_node_create_real(v, 1, 1); }
ASTNode *str(const char *v) { return ast_node_create_string(v, 1, 1); }
ASTNode *unit() { return ast_node_create_unit(1, 1); }
ASTNode *ref_var(Symbol *s) { return ast_node_create_variable_ref(s, 1, 1); }
ASTNode *assign(Symbol *s, ASTNode *v) {
  return ast_node_create_assign(s, v, 1, 1);
}
ASTNode *ret(ASTNode *v) { return ast_node_create_return(v, 1, 1); }
ASTNode *bin(ASTNodeKind k, ASTNode *l, ASTNode *r) {
  return ast_node_create_binary_op(k, l, r, 1, 1);
}
ASTNode *if_(ASTNode *c, ASTNode *t, ASTNode *e = nullptr) {
  return ast_node_create_if(c, t, e, 1, 1);
}
ASTNode *truthy() {
  return bin(AST_LESS, integer(1), integer(2));
} // a Bool value
ASTNode *define(Symbol *s, ASTNode *v) {
  ASTNode *n = assign(s, v);
  n->data.assignment.is_definition = true;
  return n;
}
template <class Append>
ASTNode *append_all(ASTNode *n, const Nodes &items, Append append) {
  for (ASTNode *item : items)
    append(n, item);
  return n;
}
ASTNode *block(const Nodes &xs) {
  return append_all(ast_node_create_block(1, 1), xs,
                    ast_node_block_append_statement);
}
ASTNode *call(FunctionSymbol *f, const Nodes &args = {}) {
  return append_all(ast_node_create_function_call(f, 1, 1), args,
                    ast_node_call_append_argument);
}
/// A literal of type @p t (Bool is `1 < 2`); NULL for TYPE_UNKNOWN.
ASTNode *value(BaseType t) {
  return t == TYPE_INT      ? integer(1)
         : t == TYPE_REAL   ? real(1.5)
         : t == TYPE_STRING ? str("s")
         : t == TYPE_UNIT   ? unit()
         : t == TYPE_BOOL   ? truthy()
                            : nullptr;
}
bool widened(const ASTNode *n) { return n->kind == AST_IMPLICIT_INT_TO_REAL; }
Symbol *var(FunctionSymbol *f, const char *name, BaseType type,
            bool constant = true) {
  Symbol *s = nullptr;
  symbol_table_define(f->local_scope, name, type_scalar(type), true, constant,
                      &s);
  return s;
}
void add(FunctionSymbol *f, ASTNode *s) {
  ASSERT_EQ(ast_node_block_append_statement(f->ast_root, s), AST_OK);
}
ASTNode *stmt(FunctionSymbol *f, size_t i) {
  return f->ast_root->data.block.statements[i];
}
/// True when @p t is the scalar type @p b (the unresolved type for
/// TYPE_UNKNOWN).
bool has_type(DataType t, BaseType b) { return type_equal(t, type_scalar(b)); }

constexpr BaseType N = TYPE_UNKNOWN, I = TYPE_INT, R = TYPE_REAL,
                   S = TYPE_STRING, U = TYPE_UNIT, B = TYPE_BOOL;
constexpr SemanticStatus OK = SEMANTIC_OK, E3 = SEMANTIC_UNDEFINED,
                         E4 = SEMANTIC_REDEFINITION, E5 = SEMANTIC_BAD_CALL,
                         E6 = SEMANTIC_TYPE_ERROR, E7 = SEMANTIC_OTHER;

/// Owns one Program; @p Base is ::testing::Test or
/// ::testing::TestWithParam<Row>.
template <class Base> class Fixture : public Base {
protected:
  Program program{};
  void SetUp() override { ASSERT_EQ(program_init(&program), PARSER_OK); }
  void TearDown() override { program_destroy(&program); }
  SemanticStatus check() { return semantic_check(&program); }
  FunctionSymbol *ref(const char *name) {
    FunctionSymbol *f = nullptr;
    function_table_get_or_create(&program.functions, name, &f);
    return f;
  }
  /// `def name(params...)[: ret] =` with an empty body; `@main` if @p is_main.
  FunctionSymbol *def(const char *name, BaseType ret_type = N,
                      const Types &params = {}, bool is_main = false) {
    FunctionSymbol *f = ref(name);
    f->is_defined = true;
    f->is_main = is_main;
    function_symbol_create_local_scope(f);
    for (size_t i = 0; i < params.size(); ++i) {
      function_symbol_add_parameter(
          f, var(f, ("p" + std::to_string(i)).c_str(), params[i]));
    }
    f->has_explicit_return_type = ret_type != N;
    if (ret_type != N)
      f->return_type.type = type_scalar(ret_type);
    f->ast_root = ast_node_create_block(1, 1);
    return f;
  }
  FunctionSymbol *def_main() { return def("main", U, {}, true); }
};

using SemanticGapTest = Fixture<::testing::Test>;
template <class Row> using TableTest = Fixture<::testing::TestWithParam<Row>>;
} // namespace

/*
 * AI-generated test block — binary operator typing.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   One table {op, left, right, status, result}; the Int side of a mixed
 *   Int/Double pair must be wrapped in AST_IMPLICIT_INT_TO_REAL.
 */
struct OpCase {
  ASTNodeKind op;
  BaseType left, right;
  SemanticStatus status;
  BaseType result;
};
const OpCase kOps[] = {
    {AST_SUBTRACT, I, I, OK, I},  {AST_DIVIDE, I, I, OK, I},
    {AST_DIVIDE, R, I, OK, R},    {AST_SUBTRACT, I, R, OK, R},
    {AST_LESS, I, R, OK, B},      {AST_LESS_EQUAL, I, R, OK, B},
    {AST_GREATER, I, R, OK, B},   {AST_GREATER_EQUAL, R, I, OK, B},
    {AST_EQUAL, I, R, OK, B},     {AST_NOT_EQUAL, I, I, OK, B},
    {AST_ADD, S, S, OK, S},       {AST_LESS, S, S, OK, B},
    {AST_EQUAL, U, U, OK, B},     {AST_SUBTRACT, S, S, E6, N},
    {AST_MULTIPLY, S, S, E6, N},  {AST_DIVIDE, S, S, E6, N},
    {AST_LESS, U, U, E6, N},      {AST_EQUAL, I, S, E6, N},
    {AST_ADD, R, S, E6, N},       {AST_LESS, B, I, E6, N},
    {AST_ADD, I, B, E6, N},       {AST_EQUAL, B, B, OK, B},
    {AST_NOT_EQUAL, S, S, OK, B},
};
class OperatorTable : public TableTest<OpCase> {};
TEST_P(OperatorTable, TypesOperands) {
  const OpCase &c = GetParam();
  FunctionSymbol *m = def_main();
  ASTNode *e = bin(c.op, value(c.left), value(c.right));
  bool in_if = c.result == B; // a Bool is only usable as a condition
  add(m, in_if ? if_(e, block({unit()})) : e);
  ASSERT_EQ(check(), c.status);
  if (c.status != OK)
    return;
  ASTNode *n = in_if ? stmt(m, 0)->data.if_statement.condition : stmt(m, 0);
  EXPECT_TRUE(has_type(n->inferred_type.type, c.result));
  EXPECT_EQ(widened(n->data.binary_op.left), c.left == I && c.right == R);
  EXPECT_EQ(widened(n->data.binary_op.right), c.right == I && c.left == R);
}
INSTANTIATE_TEST_SUITE_P(SemanticGap, OperatorTable, ::testing::ValuesIn(kOps));

/*
 * AI-generated test block — builtin signatures.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   One table {builtin, argument types, status, return type}; a referenced
 *   builtin gets its full signature and an Int argument to Double is widened.
 */
struct BuiltinCase {
  const char *name;
  Types args;
  SemanticStatus status;
  BaseType result;
};
const BuiltinCase kBuiltins[] = {
    {"readLine", {}, OK, S},      {"readDouble", {}, OK, R},
    {"toInt", {R}, OK, I},        {"toInt", {I}, OK, I},
    {"subStr", {S, I, I}, OK, S}, {"ord", {S, I}, OK, I},
    {"chr", {I}, OK, S},          {"print", {R}, OK, U},
    {"print", {U}, OK, U},        {"toStr", {I}, OK, S},
    {"toStr", {U}, OK, S},        {"print", {B}, OK, U},
    {"toStr", {B}, OK, S},        {"print", {}, E5, N},
    {"print", {I, I}, E5, N},     {"readInt", {I}, E5, N},
    {"subStr", {S, I}, E5, N},    {"ord", {S, R}, E5, N},
    {"chr", {S}, E5, N},          {"strLength", {S}, OK, I},
    {"readInt", {}, OK, I},
};
class BuiltinTable : public TableTest<BuiltinCase> {};
TEST_P(BuiltinTable, ChecksCallAgainstSignature) {
  const BuiltinCase &c = GetParam();
  FunctionSymbol *m = def_main();
  Symbol *v = var(m, "v", N);
  Nodes args;
  for (BaseType t : c.args)
    args.push_back(value(t));
  ASTNode *cl = call(ref(c.name), args);
  add(m, define(v, cl));
  ASSERT_EQ(check(), c.status);
  if (c.status != OK)
    return;
  FunctionSymbol *b = ref(c.name);
  EXPECT_TRUE(b->is_defined && b->is_builtin && b->has_explicit_return_type);
  EXPECT_TRUE(has_type(b->return_type.type, c.result));
  EXPECT_TRUE(has_type(v->type.type, c.result));
  ASSERT_EQ(b->argument_count, c.args.size());
  for (size_t i = 0; i < c.args.size(); ++i) {
    DataType p = b->parameters[i]->type.type;
    bool widen = c.args[i] == I && has_type(p, R);
    EXPECT_TRUE(has_type(p, c.args[i]) || has_type(p, TYPE_ANY) || widen) << i;
    EXPECT_EQ(widened(cl->data.call.arguments[i]), widen) << i;
  }
}
INSTANTIATE_TEST_SUITE_P(SemanticGap, BuiltinTable,
                         ::testing::ValuesIn(kBuiltins));

/*
 * AI-generated test block — coercion; Bool only where Bool is expected.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   One table {where, target, value, status, widened} over definition,
 *   assignment, return (N = bare), call argument and last body value.
 */
enum Where { DEFINE, ASSIGN, RETURN, ARG, LAST };
struct CoercionCase {
  Where where;
  BaseType target, value;
  SemanticStatus status;
  bool widen;
};
const CoercionCase kCoercions[] = {
    {DEFINE, R, I, OK, true},  {DEFINE, I, S, E6, false},
    {DEFINE, N, U, OK, false}, {DEFINE, N, B, OK, false},
    {ASSIGN, R, I, OK, true},  {ASSIGN, I, R, E6, false},
    {ASSIGN, I, B, E6, false}, {RETURN, R, I, OK, true},
    {RETURN, U, N, OK, false}, {RETURN, I, N, E6, false},
    {RETURN, I, B, E6, false}, {RETURN, U, I, OK, false},
    {RETURN, U, S, OK, false}, // Unit: value evaluated and discarded (rule 4)
    {ARG, R, I, OK, true},     {ARG, I, B, E5, false},
    {LAST, R, I, OK, true},    {LAST, U, I, OK, false},
    {LAST, S, I, E6, false},   {LAST, I, B, E6, false},
    {LAST, N, B, OK, false},
};
class CoercionTable : public TableTest<CoercionCase> {};
TEST_P(CoercionTable, ConvertsOrRejectsValue) {
  const CoercionCase &c = GetParam();
  FunctionSymbol *m = def_main();
  BaseType f_ret = c.where == RETURN || c.where == LAST ? c.target : U;
  FunctionSymbol *f =
      def("f", f_ret, c.where == ARG ? Types{c.target} : Types{});
  if (c.where == DEFINE || c.where == ASSIGN) {
    Symbol *x = var(m, "x", c.target, false);
    add(m, define(x, value(c.where == DEFINE ? c.value : c.target)));
    if (c.where == ASSIGN)
      add(m, assign(x, value(c.value)));
  }
  if (c.where == RETURN || c.where == LAST)
    add(f, c.where == RETURN ? ret(value(c.value)) : value(c.value));
  if (c.where == ARG)
    add(m, call(f, {value(c.value)}));
  ASSERT_EQ(check(), c.status);
  if (c.status != OK)
    return;
  ASTNode *n = c.where == DEFINE   ? stmt(m, 0)->data.assignment.value
               : c.where == ASSIGN ? stmt(m, 1)->data.assignment.value
               : c.where == RETURN ? stmt(f, 0)->data.return_statement.value
               : c.where == ARG    ? stmt(m, 0)->data.call.arguments[0]
                                   : stmt(f, 0);
  // The last value of a non-Unit body is wrapped in an implicit return.
  if (n != nullptr && c.where == LAST && n->kind == AST_RETURN)
    n = n->data.return_statement.value;
  if (n != nullptr)
    EXPECT_EQ(widened(n), c.widen);
}
INSTANTIATE_TEST_SUITE_P(SemanticGap, CoercionTable,
                         ::testing::ValuesIn(kCoercions));

/*
 * AI-generated test block — function body checks and inferred return types.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   One table {annotation, body of an uncalled f, status, return type}: BAD_IF
 *   results, empty/nested bodies, nested self-calls, a second main, 7 and 3.
 */
struct BodyCase {
  BaseType annotation;
  void (*body)(FunctionSymbol *);
  SemanticStatus status;
  BaseType result;
};
const BodyCase kBodies[] = {
    // A final if yields the common value of its branches; a returning branch
    // fits any value (CONTROL), but a Unit branch does not fit Int.
    {I,
     [](FunctionSymbol *f) {
       add(f, if_(truthy(), block({ret(integer(1))}), block({unit()})));
     },
     E6, N},
    {I,
     [](FunctionSymbol *f) {
       add(f, if_(truthy(), block({integer(2)}), block({ret(integer(1))})));
     },
     OK, I},
    {I,
     [](FunctionSymbol *f) { add(f, if_(truthy(), block({ret(integer(1))}))); },
     E6, N},
    {I,
     [](FunctionSymbol *f) {
       add(f, ret(integer(1)));
       add(f, if_(truthy(), block({unit()})));
     },
     E6, N},
    {N, [](FunctionSymbol *f) { add(f, if_(truthy(), block({integer(1)}))); },
     E6, N},
    {U,
     [](FunctionSymbol *f) {
       add(f, if_(truthy(), block({ret(nullptr)}), block({integer(1)})));
     },
     OK, U},
    // Empty, definition-terminated and nested bodies.
    {N, [](FunctionSymbol *) {}, OK, U},
    {I, [](FunctionSymbol *) {}, E6, N},
    {N, [](FunctionSymbol *f) { add(f, define(var(f, "x", N), integer(1))); },
     OK, U},
    {I, [](FunctionSymbol *f) { add(f, define(var(f, "x", N), integer(1))); },
     E6, N},
    {N,
     [](FunctionSymbol *f) { add(f, block({integer(1), block({str("s")})})); },
     OK, S},
    {N,
     [](FunctionSymbol *f) {
       add(f, integer(1));
       add(f, block({}));
     },
     OK, U},
    {N,
     [](FunctionSymbol *f) {
       add(f, integer(1));
       add(f, unit());
     },
     OK, U},
    {N, [](FunctionSymbol *f) { add(f, bin(AST_ADD, integer(1), real(2.5))); },
     OK, R},
    {I, [](FunctionSymbol *f) { add(f, block({block({ret(integer(1))})})); },
     OK, I},
    // A self-call of a not-annotated function is found wherever it is nested.
    {N,
     [](FunctionSymbol *f) {
       add(f, if_(bin(AST_LESS, call(f), integer(1)), block({unit()})));
     },
     E6, N},
    {N,
     [](FunctionSymbol *f) {
       add(f, if_(truthy(), block({unit()}), block({block({call(f)})})));
     },
     E6, N},
    {N,
     [](FunctionSymbol *f) {
       Symbol *r = var(f, "r", N);
       add(f, define(r, call(f)));
       add(f, ref_var(r));
     },
     E6, N},
    // A second @main is 4, assigning a parameter is 7, `val x = x` is 3.
    {U, [](FunctionSymbol *f) { f->is_main = true; }, E4, N},
    {U,
     [](FunctionSymbol *f) {
       Symbol *n = var(f, "n", I);
       function_symbol_add_parameter(f, n);
       add(f, assign(n, integer(2)));
     },
     E7, N},
    {U,
     [](FunctionSymbol *f) {
       Symbol *x = var(f, "x", N);
       add(f, define(x, ref_var(x)));
     },
     E3, N},
    // An error nested in a return value, an if condition or a left operand is
    // reported unchanged (3), not replaced by the enclosing node's 6 checks.
    {I, [](FunctionSymbol *f) { add(f, ret(ref_var(var(f, "x", N)))); }, E3, N},
    {U,
     [](FunctionSymbol *f) {
       add(f, if_(ref_var(var(f, "x", N)), block({unit()})));
     },
     E3, N},
    {U,
     [](FunctionSymbol *f) {
       Symbol *x = var(f, "x", N);
       add(f, define(x, bin(AST_ADD, ref_var(x), integer(1))));
     },
     E3, N},
    // A conversion node already in the AST has type Double (ast.h).
    {N,
     [](FunctionSymbol *f) {
       add(f, ast_node_create_implicit_int_to_real(integer(1)));
     },
     OK, R},
};
class BodyTable : public TableTest<BodyCase> {};
TEST_P(BodyTable, InfersOrRejectsResult) {
  const BodyCase &c = GetParam();
  def_main();
  FunctionSymbol *f = def("f", c.annotation);
  c.body(f);
  ASSERT_EQ(check(), c.status);
  if (c.status == OK)
    EXPECT_TRUE(has_type(f->return_type.type, c.result));
}
INSTANTIATE_TEST_SUITE_P(SemanticGap, BodyTable, ::testing::ValuesIn(kBodies));

/*
 * AI-generated test block — program-level rules, call graph and memoization.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   NULL/uninitialized program is 99, main with a parameter 5, a<->b cycle 6,
 *   a diamond is no cycle, a function called twice is checked only once.
 */
TEST(SemanticGapEntryTest, NullOrUninitializedProgramIsInternalError) {
  Program uninitialized{};
  EXPECT_EQ(semantic_check(nullptr), SEMANTIC_INTERNAL_ERROR);
  EXPECT_EQ(semantic_check(&uninitialized), SEMANTIC_INTERNAL_ERROR);
}

TEST_F(SemanticGapTest, MainWithParameterIsError5) {
  var(def("main", U, {I}, true), "x", I);
  EXPECT_EQ(check(), SEMANTIC_BAD_CALL);
}

TEST_F(SemanticGapTest, IndirectRecursionWithoutAnnotationsIsError6) {
  def_main();
  FunctionSymbol *a = def("a"), *b = def("b");
  add(a, call(b));
  add(b, call(a));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticGapTest, DiamondCallGraphIsNotACycle) {
  FunctionSymbol *m = def_main();
  FunctionSymbol *a = def("a"), *b = def("b"), *c = def("c"), *d = def("d");
  add(m, call(a));
  add(a, call(b));
  add(a, call(c));
  add(b, call(d));
  add(c, call(d));
  add(d, real(1.5));
  ASSERT_EQ(check(), SEMANTIC_OK);
  for (FunctionSymbol *f : {a, b, c, d})
    EXPECT_TRUE(has_type(f->return_type.type, R));
}

TEST_F(SemanticGapTest, FunctionCalledTwiceIsCheckedOnce) {
  // def g() = { val x: Double = 1; x }   called from main and from h.
  FunctionSymbol *m = def_main();
  FunctionSymbol *h = def("h"), *g = def("g");
  Symbol *x = var(g, "x", R);
  add(g, define(x, integer(1)));
  add(g, ref_var(x));
  add(m, call(g));
  add(m, call(h));
  add(h, call(g));
  ASSERT_EQ(check(), SEMANTIC_OK);
  EXPECT_EQ(g->check_state, FUNCTION_CHECKED);
  EXPECT_TRUE(has_type(h->return_type.type, R));
  ASTNode *v = stmt(g, 0)->data.assignment.value;
  ASSERT_TRUE(widened(v));
  EXPECT_EQ(v->data.conversion.operand->kind, AST_CONSTANT_INT);
}

/*
 * AI-generated test block — pass order and first-error reporting.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Errors follow the semantic.h pass order (builtins, undefined/main,
 *   return/cycle, typing); an error in a callee checked on demand is reported.
 */
TEST_F(SemanticGapTest, ErrorInOnDemandCalleeIsReported) {
  // main (first in the table) calls not-annotated g, whose body is "a" + 1,
  // and only then assigns to a `val` (7). g is checked at the call, so its 6
  // is the first error; checking g only after main would report 7.
  FunctionSymbol *m = def_main();
  FunctionSymbol *g = def("g");
  Symbol *k = var(m, "k", I);
  add(m, call(g));
  add(m, define(k, integer(1)));
  add(m, assign(k, integer(2)));
  add(g, bin(AST_ADD, str("a"), integer(1)));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticGapTest, BuiltinRedefinitionIsReportedBeforeMissingMain) {
  def("print", U);
  EXPECT_EQ(check(), SEMANTIC_REDEFINITION);
}

TEST_F(SemanticGapTest, UndefinedCalleeIsReportedBeforeMissingAnnotation) {
  def_main();
  FunctionSymbol *f = def("f");
  add(f, call(ref("undef")));
  add(f, ret(nullptr));
  EXPECT_EQ(check(), SEMANTIC_UNDEFINED);
}

TEST_F(SemanticGapTest, UndefinedFunctionIsReportedBeforeTypeErrors) {
  FunctionSymbol *m = def_main();
  add(m, bin(AST_ADD, str("a"), integer(1)));
  FunctionSymbol *g = def("g", U);
  add(g, call(ref("nope")));
  EXPECT_EQ(check(), SEMANTIC_UNDEFINED);
}

/*
 * AI-generated test block — cycle detection behind an acyclic caller.
 * Model: Claude (Claude Code).
 * Prompt:
 *   a -> b -> c -> b with only c annotated: a is not on a cycle but b is,
 *   so the program is 6 even though a is queried first.
 */
TEST_F(SemanticGapTest, CycleBehindAcyclicCallerIsError6) {
  def_main();
  FunctionSymbol *a = def("a"), *b = def("b"), *c = def("c", I);
  add(a, call(b));
  add(b, call(c));
  add(c, call(b));
  add(c, integer(1));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

/*
 * AI-generated test block — malformed program without a function body.
 * Model: Claude (Claude Code).
 * Prompt:
 *   A defined function whose ast_root is NULL is an internal error (99),
 *   not a crash, like the NULL/uninitialized program case.
 */
TEST_F(SemanticGapTest, DefinedFunctionWithoutBodyIsInternalError) {
  def_main();
  FunctionSymbol *f = def("f", U);
  ast_node_destroy(f->ast_root);
  f->ast_root = nullptr;
  EXPECT_EQ(check(), SEMANTIC_INTERNAL_ERROR);
}
