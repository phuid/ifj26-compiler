/*
 * AI-generated test block — fixture and AST builders for white-box spec
 *   tests of the semantic pass.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write white-box conformance tests of semantic_check from the approved
 *   catalogs A2-A6 (variables, functions, control flow, expressions and
 *   builtins). Build every Program and AST by hand through the ast,
 *   function_table and symtable APIs; copy the builders of
 *   semantic_edge_test.cc (they live in an anonymous namespace there) and
 *   skip every case an existing tests/semantic_* test already covers.
 */
// The RC-* root-cause IDs of the "Fails today:" comments are listed in
// tests/spec_harness.hpp.
#include <cstddef>
#include <ostream>
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
                         E5 = SEMANTIC_BAD_CALL, E6 = SEMANTIC_TYPE_ERROR,
                         E7 = SEMANTIC_OTHER;

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
ASTNode *if_(ASTNode *c, ASTNode *t, ASTNode *e) {
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

/// Defines @p name in @p scope the way the parser does: before its
/// initialization, with the annotated type or TYPE_UNKNOWN.
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
/// Appends @p s to the body of @p f; a failed append is a non-fatal test
/// failure (usable from void helpers and row lambdas), @p s is then freed.
bool add(FunctionSymbol *f, ASTNode *s) {
  if (ast_node_block_append_statement(f->ast_root, s) != AST_OK) {
    ADD_FAILURE() << "cannot append a statement to " << f->name;
    ast_node_destroy(s);
    return false;
  }
  return true;
}
ASTNode *stmt(const FunctionSymbol *f, size_t i) {
  return f->ast_root->data.block.statements[i];
}
/// Statement @p i of the block @p b.
ASTNode *in_block(const ASTNode *b, size_t i) {
  return b->data.block.statements[i];
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
  f->return_type = type_scalar(ret_type);
  f->ast_root = ast_node_create_block(1, 1);
  return f;
}

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
    return "Boolean";
  default:
    return "None";
  }
}

/**
 * Owns one Program built by hand, shaped as the parser builds it, with two
 * simplifications that semantic_check cannot see: a one-line body
 * `def g() = e` is a block holding the single expression e (the parser keeps
 * the bare expression; for the semantics both are equivalent), and the child
 * scopes of if branches are created only where a branch defines a variable
 * (the semantic pass never reads scopes).
 */
class SpecSemanticWhiteboxTest : public ::testing::Test {
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
class SpecSemanticWhiteboxTableTest
    : public SpecSemanticWhiteboxTest,
      public ::testing::WithParamInterface<Row> {};

/// Gives every row with a `name` member a readable gtest name.
template <class Row>
std::string row_name(const ::testing::TestParamInfo<Row> &info) {
  return std::string(info.param.name);
}

} // namespace

/*
 * AI-generated test block — typed definitions and assignments (spec 1.3,
 *   2.2).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Every annotated definition and every assignment of a literal of type
 *   Int/Double/String/Unit/Boolean that the existing tables skip: only the
 *   same type or Int to Double is accepted, otherwise 6; a Boolean value is
 *   never stored (catalog A2-092, A2-093). Boolean as the annotated type of
 *   a variable is not tested: its exit code is unresolved (C-revA-4).
 */
namespace {

struct StoreRow {
  BaseType target; ///< Annotated (definition) or current (assignment) type.
  BaseType value;  ///< Type of the stored literal.
  SemanticStatus status;
};

void PrintTo(const StoreRow &row, std::ostream *out) {
  *out << type_name(row.target) << " <- " << type_name(row.value);
}

std::string store_name(const ::testing::TestParamInfo<StoreRow> &info) {
  return std::string(type_name(info.param.target)) + "From" +
         type_name(info.param.value);
}

// Rows R<-I and I<-S are in semantic_gap_test.cc (CoercionTable), I<-R in
// semantic_test.cc (DoubleToIntIsError6) and U<-I in semantic_edge_test.cc
// (UnitAnnotationRejectsIntInitializer); they are not repeated here.
const StoreRow kDefinitions[] = {
    // §2.2: an Int variable accepts an Int initializer [A2-092].
    {I, I, OK},
    // §1.3: Unit is not convertible to Int [A2-092].
    {I, U, E6},
    // §1.3, §5.2: Boolean is not convertible to Int [A2-092].
    {I, B, E6},
    // §2.2: a Double variable accepts a Double initializer [A2-092].
    {R, R, OK},
    // §1.3: String is not convertible to Double [A2-092].
    {R, S, E6},
    // §1.3: Unit is not convertible to Double [A2-092].
    {R, U, E6},
    // §1.3, §5.2: Boolean is not convertible to Double [A2-092].
    {R, B, E6},
    // §1.3: Int is not convertible to String [A2-092].
    {S, I, E6},
    // §1.3: Double is not convertible to String [A2-092].
    {S, R, E6},
    // §2.2: a String variable accepts a String initializer [A2-092].
    {S, S, OK},
    // §1.3: Unit is not convertible to String [A2-092].
    {S, U, E6},
    // §1.3, §5.2: Boolean is not convertible to String [A2-092].
    {S, B, E6},
    // §1.3: Double is not convertible to Unit [A2-092].
    {U, R, E6},
    // §1.3: String is not convertible to Unit [A2-092].
    {U, S, E6},
    // §2.2: a Unit variable accepts () [A2-092].
    {U, U, OK},
    // §1.3, §5.2: Boolean is not convertible to Unit [A2-092].
    {U, B, E6},
};

// Rows R<-I, I<-R, I<-B and I<-S are in semantic_gap_test.cc (CoercionTable)
// and semantic_test.cc; they are not repeated here.
const StoreRow kAssignments[] = {
    // §2.2: an Int variable accepts an Int value [A2-093].
    {I, I, OK},
    // §2.2: the type of a variable never changes; Unit -> Int is 6 [A2-093].
    {I, U, E6},
    // §2.2: a Double variable accepts a Double value [A2-093].
    {R, R, OK},
    // §2.2: String is not assignable to Double [A2-093].
    {R, S, E6},
    // §2.2: Unit is not assignable to Double [A2-093].
    {R, U, E6},
    // §1.3, §5.2: Boolean is not convertible to Double [A2-093].
    {R, B, E6},
    // §2.2: Int is not assignable to String [A2-093].
    {S, I, E6},
    // §2.2: Double is not assignable to String [A2-093].
    {S, R, E6},
    // §2.2: a String variable accepts a String value [A2-093].
    {S, S, OK},
    // §2.2: Unit is not assignable to String [A2-093].
    {S, U, E6},
    // §1.3, §5.2: Boolean is not convertible to String [A2-093].
    {S, B, E6},
    // §2.2: Int is not assignable to Unit [A2-093].
    {U, I, E6},
    // §2.2: Double is not assignable to Unit [A2-093].
    {U, R, E6},
    // §2.2: String is not assignable to Unit [A2-093].
    {U, S, E6},
    // §2.2: a Unit variable accepts () [A2-093].
    {U, U, OK},
    // §1.3, §5.2: Boolean is not convertible to Unit [A2-093].
    {U, B, E6},
};

} // namespace

class SpecSemanticWhiteboxDefinitionTest
    : public SpecSemanticWhiteboxTableTest<StoreRow> {};

TEST_P(SpecSemanticWhiteboxDefinitionTest, AcceptsOnlySameTypeOrIntToDouble) {
  // Arrange: val x: <target> = <literal of value type>
  const StoreRow &row = GetParam();
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", row.target);
  add(m, define(x, value(row.value)));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, row.status);
  if (status == OK) {
    EXPECT_FALSE(widened(stmt(m, 0)->data.assignment.value));
  }
}
INSTANTIATE_TEST_SUITE_P(SpecSemanticWhitebox,
                         SpecSemanticWhiteboxDefinitionTest,
                         ::testing::ValuesIn(kDefinitions), store_name);

class SpecSemanticWhiteboxAssignmentTest
    : public SpecSemanticWhiteboxTableTest<StoreRow> {};

TEST_P(SpecSemanticWhiteboxAssignmentTest, AcceptsOnlySameTypeOrIntToDouble) {
  // Arrange: var x: <target> = <literal of target>; x = <literal of value>
  const StoreRow &row = GetParam();
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", row.target, false);
  add(m, define(x, value(row.target)));
  add(m, assign(x, value(row.value)));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, row.status);
  if (status == OK) {
    EXPECT_FALSE(widened(stmt(m, 1)->data.assignment.value));
  }
}
INSTANTIATE_TEST_SUITE_P(SpecSemanticWhitebox,
                         SpecSemanticWhiteboxAssignmentTest,
                         ::testing::ValuesIn(kAssignments), store_name);

/*
 * AI-generated test block — inferred variable types and use before
 *   definition (spec 1.3, 2.2).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   An inferred type comes from the annotated type of the source variable,
 *   the conversion wraps a variable on the right-hand side, a use of an
 *   annotated variable inside its own initializer expression is 3, and a
 *   type inferred from
 *   a user function result never changes (catalog A2-094, A2-098, A2-101).
 */

// §1.3, §2.2: an inferred type is the declared type of the source variable,
//   Int to Double is inserted around the right-hand side [A2-094].
TEST_F(SpecSemanticWhiteboxTest,
       AnnotatedDoubleTypePropagatesThroughInference) {
  // Arrange: val d: Double = 1; val y = d; val i = 2; var z = y; z = i
  FunctionSymbol *m = def_main();
  Symbol *d = var(m, "d", R), *y = var(m, "y", N), *i = var(m, "i", N);
  Symbol *z = var(m, "z", N, false);
  add(m, define(d, integer(1)));
  add(m, define(y, ref_var(d)));
  add(m, define(i, integer(2)));
  add(m, define(z, ref_var(y)));
  add(m, assign(z, ref_var(i)));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  EXPECT_TRUE(has_type(y->type, R));
  EXPECT_TRUE(has_type(z->type, R));
  EXPECT_TRUE(has_type(i->type, I));
  EXPECT_TRUE(widened(stmt(m, 0)->data.assignment.value));
  EXPECT_FALSE(widened(stmt(m, 1)->data.assignment.value));
  ASTNode *last = stmt(m, 4)->data.assignment.value;
  ASSERT_TRUE(widened(last));
  EXPECT_EQ(last->data.conversion.operand->kind, AST_VARIABLE_REF);
}

// §2.2: using a variable before its definition is error 3, also inside its
//   own annotated initializer; the parser defines x (with type Int) before
//   it parses `1 + x`, so the operand refers to the same symbol [A2-098].
// Fails today: RC-ANNOT-USE, the operand x already has its annotated type
//   Int, use before definition is detected only by TYPE_UNKNOWN (returns 0)
TEST_F(SpecSemanticWhiteboxTest, AnnotatedUseInOwnInitializerOperandIsError3) {
  // Arrange: val x: Int = 1 + x
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", I);
  add(m, define(x, bin(AST_ADD, integer(1), ref_var(x))));
  // Act
  const SemanticStatus status = check();
  // Assert
  EXPECT_EQ(status, E3);
}

// §1.3, §2.2: `var u = g()` takes the inferred Unit of g and keeps it, so
//   `u = 1` is error 6 [A2-101].
TEST_F(SpecSemanticWhiteboxTest,
       UnitInferredFromUserFunctionRejectsIntAssignment) {
  // Arrange: def g() = print("a") (single-expression block); var u = g();
  //   u = 1
  FunctionSymbol *m = def_main();
  FunctionSymbol *g = def("g");
  add(g, call(ref("print"), {str("a")}));
  Symbol *u = var(m, "u", N, false);
  add(m, define(u, call(g)));
  add(m, assign(u, integer(1)));
  // Act
  const SemanticStatus status = check();
  // Assert
  EXPECT_EQ(status, E6);
  EXPECT_TRUE(has_type(u->type, U));
}

/*
 * AI-generated test block — bodies of an uncalled function f (spec 2.3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   One table {name, annotation of f, body of f, status, inferred type of a
 *   not-annotated f}:
 *   return values, calls of other functions, parameters, unreachable code
 *   and rule 3 for an if ending the body, nested ifs and branches ending
 *   with a definition, assignment, call or variable; only combinations the
 *   existing body tables miss (catalogs A3-101..125, A4-110..127, A6-204,
 *   A6-206).
 */
namespace {

struct BodyRow {
  const char *name;
  BaseType annotation; ///< Return type of f (N = not annotated).
  void (*body)(Program *, FunctionSymbol *);
  SemanticStatus status;
  BaseType result; ///< Return type of f after the check (N = not checked).
};

void PrintTo(const BodyRow &row, std::ostream *out) { *out << row.name; }

const BodyRow kBodies[] = {
    // §2.3: g is indirectly recursive through f, the edge f -> g is only in
    //   unreachable code, which is still checked (6) [A3-101].
    {"CycleOnlyThroughUnreachableCallIsError6", I,
     [](Program *p, FunctionSymbol *f) {
       FunctionSymbol *g = def(p, "g");
       add(f, ret(integer(1)));
       add(f, call(g));
       add(f, integer(1));
       add(g, call(f));
     },
     E6, N},
    // §2.3: the value of return in a Unit function is evaluated, so a bad
    //   call in it is 5 [A3-102].
    {"DiscardedReturnValueOfUnitFunctionIsChecked", U,
     [](Program *p, FunctionSymbol *f) {
       FunctionSymbol *g = def(p, "g", I);
       add(g, integer(1));
       add(f, ret(call(g, {integer(1)})));
     },
     E5, N},
    // §2.3: return of a forward call of a not-annotated Int function from a
    //   String function is 6 [A3-105].
    {"ReturnOfForwardCallOfOtherTypeIsError6", S,
     [](Program *p, FunctionSymbol *f) {
       FunctionSymbol *g = def(p, "g");
       add(f, ret(call(g)));
       add(g, integer(5));
     },
     E6, N},
    // §2.3, §4: return of print (Unit) from an Int function is 6 [A3-106].
    {"ReturningUnitCallFromIntFunctionIsError6", I,
     [](Program *p, FunctionSymbol *f) {
       add(f, ret(call(ref(p, "print"), {str("x")})));
     },
     E6, N},
    // §2.3: a parameter is read-only, also inside a branch (7) [A3-111].
    {"ParameterAssignmentInBranchIsError7", U,
     [](Program *, FunctionSymbol *f) {
       Symbol *n = var(f, "n", I);
       function_symbol_add_parameter(f, n);
       add(f, if_(truthy(), block({assign(n, integer(0))}), block({unit()})));
     },
     E7, N},
    // §2.3: a parameter is read-only, also in unreachable code (7) [A3-112].
    {"ParameterAssignmentAfterReturnIsError7", U,
     [](Program *, FunctionSymbol *f) {
       Symbol *n = var(f, "n", I);
       function_symbol_add_parameter(f, n);
       add(f, ret(nullptr));
       add(f, assign(n, integer(0)));
     },
     E7, N},
    // §2.3 rule 3: only the last statement decides, an if in the middle may
    //   have branches of different types [A4-110, same as A3-114].
    {"NonFinalIfMayHaveMismatchedBranchValues", I,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(), block({integer(1)}), block({str("s")})));
       add(f, integer(2));
     },
     OK, N},
    // §2.3: a String parameter as argument for an Int parameter is 5
    //   [A3-118].
    {"CallerParameterOfWrongTypeAsArgumentIsError5", U,
     [](Program *p, FunctionSymbol *f) {
       FunctionSymbol *g = def(p, "g", U, {I});
       add(g, unit());
       Symbol *s = var(f, "s", S);
       function_symbol_add_parameter(f, s);
       add(f, call(g, {ref_var(s)}));
     },
     E5, N},
    // §2.3: a bare return is allowed only in a Unit function (6) [A3-119].
    {"BareReturnInDoubleFunctionIsError6", R,
     [](Program *, FunctionSymbol *f) { add(f, ret(nullptr)); }, E6, N},
    // §2.3: a bare return is allowed only in a Unit function (6) [A3-119].
    {"BareReturnInStringFunctionIsError6", S,
     [](Program *, FunctionSymbol *f) { add(f, ret(nullptr)); }, E6, N},
    // §1.3, §2.3: Double never narrows to the Int result (6) [A3-120].
    {"ReturnOfDoubleFromIntFunctionIsError6", I,
     [](Program *, FunctionSymbol *f) { add(f, ret(real(1.5))); }, E6, N},
    // §1.3, §2.3: Unit is not an Int result (6) [A3-120].
    {"ReturnOfUnitFromIntFunctionIsError6", I,
     [](Program *, FunctionSymbol *f) { add(f, ret(unit())); }, E6, N},
    // §2.3: a call of an undefined function in unreachable code is 3
    //   [A3-122]; the 3 already comes from the function table pass (an
    //   undefined callee), so this row alone does not show that unreachable
    //   code is walked (UnreachableCallWithExtraArgumentIsError5 does).
    {"UnreachableCallOfUndefinedFunctionIsError3", I,
     [](Program *p, FunctionSymbol *f) {
       add(f, ret(integer(1)));
       add(f, call(ref(p, "missing")));
       add(f, integer(1));
     },
     E3, N},
    // §2.3: a call with a wrong argument count in unreachable code is 5
    //   [A3-122].
    {"UnreachableCallWithExtraArgumentIsError5", I,
     [](Program *p, FunctionSymbol *f) {
       FunctionSymbol *g = def(p, "g", U);
       add(g, unit());
       add(f, ret(integer(1)));
       add(f, call(g, {integer(1)}));
       add(f, integer(1));
     },
     E5, N},
    // §2.3 rule 3: the else branch ends with (), not with return (6)
    //   [A3-123].
    {"ElseBranchWithStatementAfterReturnIsError6", I,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(), block({ret(integer(1))}),
                  block({ret(integer(2)), unit()})));
     },
     E6, N},
    // §2.3 rule 3: a branch ending with a definition has the value (),
    //   the not-annotated f infers Unit [A3-124].
    {"NotAnnotatedFinalIfWithDefinitionBranchInfersUnit", N,
     [](Program *, FunctionSymbol *f) {
       SymbolTable *then_scope = symbol_table_create_child(f->local_scope);
       add(f, if_(truthy(),
                  block({define(var_in(then_scope, "x", N), integer(1))}),
                  block({unit()})));
     },
     OK, U},
    // §2.3 rule 3: in an Int function a branch ending with an assignment
    //   has the value () (6) [A3-125].
    {"FinalIfWithAssignmentBranchInIntFunctionIsError6", I,
     [](Program *, FunctionSymbol *f) {
       Symbol *k = var(f, "k", I, false);
       add(f, define(k, integer(0)));
       add(f, if_(truthy(), block({ret(integer(1))}),
                  block({assign(k, integer(2))})));
     },
     E6, N},
    // §2.3 rule 3 applies recursively to an if ending the then branch
    //   [A4-111].
    {"NestedIfInThenBranchAllReturn", I,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(),
                  block({if_(truthy(), block({ret(integer(1))}),
                             block({ret(integer(2))}))}),
                  block({ret(integer(3))})));
     },
     OK, N},
    // §2.3 rule 3: one leaf of a three-level if ends with () (6) [A4-112b].
    {"ThreeLevelNestedIfOneUnitLeaf", I,
     [](Program *, FunctionSymbol *f) {
       ASTNode *inner =
           if_(truthy(), block({ret(integer(1))}), block({unit()}));
       ASTNode *middle =
           if_(truthy(), block({ret(integer(3))}), block({inner}));
       add(f, if_(truthy(), block({middle}), block({ret(integer(4))})));
     },
     E6, N},
    // §2.3 rule 3: the then branch ends with an unreachable (), not with
    //   return (6) [A4-113].
    {"BranchEndingWithUnreachableUnitAfterReturn", I,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(), block({ret(integer(1)), unit()}),
                  block({ret(integer(2))})));
     },
     E6, N},
    // §2.3 rule 3: the last statement of the branch is a return [A4-114].
    {"BranchWithTwoReturnsEndsWithReturn", I,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(), block({ret(integer(1)), ret(integer(2))}),
                  block({ret(integer(3))})));
     },
     OK, N},
    // §2.3 rules 2-3: branches ending with a definition and an assignment
    //   are both (), f infers Unit [A4-115].
    {"BranchesEndingWithDefinitionAndAssignmentInferUnit", N,
     [](Program *, FunctionSymbol *f) {
       Symbol *k = var(f, "k", N, false);
       SymbolTable *then_scope = symbol_table_create_child(f->local_scope);
       Symbol *y = var_in(then_scope, "y", N);
       add(f, define(k, integer(0)));
       add(f, if_(truthy(), block({define(y, integer(1))}),
                  block({assign(k, integer(2))})));
     },
     OK, U},
    // §2.3 rule 3: a branch ending with a call of a function that infers
    //   Unit [A4-116].
    {"BranchEndingWithCallOfInferredUnitFunction", N,
     [](Program *p, FunctionSymbol *f) {
       FunctionSymbol *g = def(p, "g");
       add(g, call(ref(p, "print"), {str("x")}));
       add(f, if_(truthy(), block({call(g)}), block({unit()})));
     },
     OK, U},
    // §2.3 rule 3: a branch ending with a call of a function that infers
    //   Int cannot be the result of a final if (6) [A4-117].
    {"BranchEndingWithCallOfInferredIntFunction", N,
     [](Program *p, FunctionSymbol *f) {
       FunctionSymbol *g = def(p, "g");
       add(g, integer(1));
       add(f, if_(truthy(), block({call(g)}), block({unit()})));
     },
     E6, N},
    // §1.3, §2.3 rule 3: a Unit variable as the value of a branch [A4-118].
    {"BranchEndingWithUnitVariable", N,
     [](Program *, FunctionSymbol *f) {
       Symbol *u = var(f, "u", N);
       add(f, define(u, unit()));
       add(f, if_(truthy(), block({ref_var(u)}), block({unit()})));
     },
     OK, U},
    // §2.3 rules 3-4: return of a String from an Int function in a final if
    //   (6) [A4-119].
    {"FinalIfReturningStringInIntFunction", I,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(), block({ret(integer(1))}), block({ret(str("s"))})));
     },
     E6, N},
    // §2.3: the last statement directly in the body decides, also when it is
    //   unreachable after a returning if (6) [A4-122].
    {"ValueAfterReturningIfDecidesResult", I,
     [](Program *, FunctionSymbol *f) {
       add(f,
           if_(truthy(), block({ret(integer(1))}), block({ret(integer(2))})));
       add(f, str("s"));
     },
     E6, N},
    // §2.3: a body ending with an assignment has the value (), f infers
    //   Unit [A4-125a].
    {"AssignmentTerminatedBodyInfersUnit", N,
     [](Program *, FunctionSymbol *f) {
       Symbol *x = var(f, "x", N, false);
       add(f, define(x, integer(1)));
       add(f, assign(x, integer(2)));
     },
     OK, U},
    // §2.3: an Int body ending with an assignment has the value () (6)
    //   [A4-125b].
    {"AssignmentTerminatedIntBodyIsError6", I,
     [](Program *, FunctionSymbol *f) {
       Symbol *x = var(f, "x", N, false);
       add(f, define(x, integer(1)));
       add(f, assign(x, integer(2)));
     },
     E6, N},
    // §2.3 rules 3-4: an explicit Unit function discards a returned String
    //   [A4-126].
    {"ExplicitUnitFinalIfReturningString", U,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(), block({ret(str("s"))}), block({unit()})));
     },
     OK, N},
    // §2.3: a non-final if is skipped, the last Double infers Double
    //   [A4-127].
    {"NonFinalIfThenDoubleValueInfersDouble", N,
     [](Program *, FunctionSymbol *f) {
       add(f, if_(truthy(), block({integer(1)}), block({str("s")})));
       add(f, real(1.5));
     },
     OK, R},
    // §2.3 rule 4, §4: toStr returns String, not Int (6) [A6-204].
    {"ReturnedToStrFromIntFunctionIsError6", I,
     [](Program *p, FunctionSymbol *f) {
       add(f, ret(call(ref(p, "toStr"), {integer(1)})));
     },
     E6, N},
    // §2.3 rules 2 and 4, §4: strLength returns Int, not String (6)
    //   [A6-206].
    {"LastStrLengthInStringFunctionIsError6", S,
     [](Program *p, FunctionSymbol *f) {
       add(f, call(ref(p, "strLength"), {str("a")}));
     },
     E6, N},
};

} // namespace

class SpecSemanticWhiteboxBodyTest
    : public SpecSemanticWhiteboxTableTest<BodyRow> {};

TEST_P(SpecSemanticWhiteboxBodyTest, InfersOrRejectsResult) {
  // Arrange
  const BodyRow &row = GetParam();
  def_main();
  FunctionSymbol *f = def("f", row.annotation);
  row.body(&program, f);
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, row.status);
  if (status == OK && row.result != N) {
    EXPECT_TRUE(has_type(f->return_type, row.result));
  }
}
INSTANTIATE_TEST_SUITE_P(SpecSemanticWhitebox, SpecSemanticWhiteboxBodyTest,
                         ::testing::ValuesIn(kBodies), row_name<BodyRow>);

/*
 * AI-generated test block — rules of the entry function (spec 2, 2.3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   One table {name, annotation of @main, body of @main, status}. Criterion:
 *   a row is here only if a rule of @main changes the outcome (the Unit
 *   result of @main, @main is not implicitly annotated, return or recursion
 *   in @main); a case that only moves an existing test of an uncalled f into
 *   @main is left out (A4-120 and A6-211 for that reason) (catalog A3-115,
 *   A3-116, A3-126, A6-210).
 */
namespace {

struct MainRow {
  const char *name;
  BaseType annotation; ///< Return type of @main (N = not annotated).
  void (*body)(Program *, FunctionSymbol *);
  SemanticStatus status;
};

void PrintTo(const MainRow &row, std::ostream *out) { *out << row.name; }

const MainRow kMains[] = {
    // §2, §2.3 rule 4: an explicitly Unit @main may return a value, it is
    //   discarded and the Unit result rule of @main still holds [A3-115].
    {"MainExplicitUnitDiscardsReturnValue", U,
     [](Program *, FunctionSymbol *m) { add(m, ret(integer(1))); }, OK},
    // §2, §2.3: @main is not implicitly annotated, so a recursive @main
    //   without `: Unit` is 6 (with `: Unit` it is accepted, see
    //   SemanticEdgeTest.MainMayBeCalledRecursivelyAndFromOtherFunctions)
    //   [A3-116].
    {"NotAnnotatedRecursiveMainIsError6", N,
     [](Program *, FunctionSymbol *m) { add(m, call(m)); }, E6},
    // §2, §2.3: @main is not implicitly annotated, so return in a @main
    //   without `: Unit` is 6 [A3-126].
    {"NotAnnotatedMainWithReturnIsError6", N,
     [](Program *, FunctionSymbol *m) {
       add(m, if_(truthy(), block({ret(nullptr)}), block({ret(nullptr)})));
     },
     E6},
    // §2, §4: the result of @main must be Unit; a not-annotated @main ending
    //   with readInt infers Int (6) [A6-210].
    {"NotAnnotatedMainEndingWithReadIntIsError6", N,
     [](Program *p, FunctionSymbol *m) { add(m, call(ref(p, "readInt"))); },
     E6},
};

} // namespace

class SpecSemanticWhiteboxMainTest
    : public SpecSemanticWhiteboxTableTest<MainRow> {};

TEST_P(SpecSemanticWhiteboxMainTest, ChecksEntryFunctionRules) {
  // Arrange
  const MainRow &row = GetParam();
  FunctionSymbol *m = def("main", row.annotation, {}, true);
  row.body(&program, m);
  // Act
  const SemanticStatus status = check();
  // Assert
  EXPECT_EQ(status, row.status);
}
INSTANTIATE_TEST_SUITE_P(SpecSemanticWhitebox, SpecSemanticWhiteboxMainTest,
                         ::testing::ValuesIn(kMains), row_name<MainRow>);

/*
 * AI-generated test block — statements in the body of @main (spec 2.2,
 *   2.3, 2.4, 4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   One table {name, statements of `@main def main(): Unit`, status}; @main
 *   is only the Unit-typed container of the statements, as in the existing
 *   semantic tests: calls with inferred results checked against annotations
 *   and parameters, conditions must be Boolean, builtin results stored,
 *   unreachable and else-branch calls are checked (catalog A3-107, A3-108,
 *   A3-110, A3-117, A4-123b, A4-130, A6-202, A6-212, A6-213, A6-215).
 */
namespace {

struct StatementRow {
  const char *name;
  void (*body)(Program *, FunctionSymbol *);
  SemanticStatus status;
};

void PrintTo(const StatementRow &row, std::ostream *out) { *out << row.name; }

const StatementRow kStatements[] = {
    // §2.2, §2.3: the inferred Int of g does not fit `val s: String` (6)
    //   [A3-107].
    {"InferredCalleeTypeCheckedAgainstAnnotation",
     [](Program *p, FunctionSymbol *m) {
       FunctionSymbol *g = def(p, "g");
       add(g, integer(1));
       add(m, define(var(m, "s", S), call(g)));
     },
     E6},
    // §2.3: an inferred Double result as argument for an Int parameter is 5
    //   [A3-108].
    {"InferredDoubleResultAsIntArgumentIsError5",
     [](Program *p, FunctionSymbol *m) {
       FunctionSymbol *g = def(p, "g");
       add(g, real(1.5));
       FunctionSymbol *h = def(p, "h", U, {I});
       add(h, unit());
       Symbol *d = var(m, "d", N);
       add(m, define(d, call(g)));
       add(m, call(h, {ref_var(d)}));
     },
     E5},
    // §2.2, §2.3: `s = name()` with a String result keeps s a String
    //   [A3-110].
    {"StringCallAsAssignmentRightHandSide",
     [](Program *p, FunctionSymbol *m) {
       FunctionSymbol *name = def(p, "name", S);
       add(name, str("s"));
       Symbol *s = var(m, "s", N, false);
       add(m, define(s, str("a")));
       add(m, assign(s, call(name)));
     },
     OK},
    // §2.2, §2.3: `s = name()` with an Int result into a String is 6
    //   [A3-110].
    {"IntCallAsAssignmentRightHandSideIsError6",
     [](Program *p, FunctionSymbol *m) {
       FunctionSymbol *name = def(p, "name", I);
       add(name, integer(1));
       Symbol *s = var(m, "s", N, false);
       add(m, define(s, str("a")));
       add(m, assign(s, call(name)));
     },
     E6},
    // §2.3: two arguments for a function with one parameter is 5 [A3-117].
    {"TooManyArgumentsToUnaryFunctionIsError5",
     [](Program *p, FunctionSymbol *m) {
       FunctionSymbol *g = def(p, "g", U, {I});
       add(g, unit());
       add(m, call(g, {integer(1), integer(2)}));
     },
     E5},
    // §2.4: the condition is a call with an Int result, not a Boolean (6)
    //   [A4-123b].
    {"CallConditionIsError6",
     [](Program *p, FunctionSymbol *m) {
       FunctionSymbol *g = def(p, "g", I);
       add(g, integer(1));
       add(m, if_(call(g), block({unit()}), block({unit()})));
     },
     E6},
    // §2.4: the condition of an if nested in a then branch is checked (6)
    //   [A4-130].
    {"NestedConditionChecked",
     [](Program *, FunctionSymbol *m) {
       add(m, if_(truthy(),
                  block({if_(integer(1), block({unit()}), block({unit()}))}),
                  block({unit()})));
     },
     E6},
    // §2.2, §4: readLine returns String, an Int variable cannot take it (6)
    //   [A6-202].
    {"AssignedReadLineToIntVarIsError6",
     [](Program *p, FunctionSymbol *m) {
       Symbol *x = var(m, "x", I, false);
       add(m, define(x, integer(1)));
       add(m, assign(x, call(ref(p, "readLine"))));
     },
     E6},
    // §4: a String identifier as the Int argument of chr is 5 [A6-212].
    {"IdentifierArgumentOfWrongTypeIsError5",
     [](Program *p, FunctionSymbol *m) {
       Symbol *t = var(m, "t", N);
       add(m, define(t, str("A")));
       add(m, call(ref(p, "chr"), {ref_var(t)}));
     },
     E5},
    // §6: all checks are static, a bad builtin call after return is 5
    //   [A6-213].
    {"BadBuiltinCallAfterReturnIsError5",
     [](Program *p, FunctionSymbol *m) {
       add(m, ret(nullptr));
       add(m, call(ref(p, "strLength"), {integer(1)}));
     },
     E5},
    // §4: a bad builtin call in the else branch is 5 [A6-215].
    {"BadBuiltinCallInElseBranchIsError5",
     [](Program *p, FunctionSymbol *m) {
       add(m, if_(truthy(), block({unit()}),
                  block({call(ref(p, "chr"), {str("a")})})));
     },
     E5},
};

} // namespace

class SpecSemanticWhiteboxStatementTest
    : public SpecSemanticWhiteboxTableTest<StatementRow> {};

TEST_P(SpecSemanticWhiteboxStatementTest, ChecksStatementsOfUnitMain) {
  // Arrange
  const StatementRow &row = GetParam();
  FunctionSymbol *m = def_main();
  row.body(&program, m);
  // Act
  const SemanticStatus status = check();
  // Assert
  EXPECT_EQ(status, row.status);
}
INSTANTIATE_TEST_SUITE_P(SpecSemanticWhitebox,
                         SpecSemanticWhiteboxStatementTest,
                         ::testing::ValuesIn(kStatements),
                         row_name<StatementRow>);

/*
 * AI-generated test block — inferred types and conversions placed by the
 *   function rules (spec 1.3, 2.2, 2.3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   The cases of catalog A3 that also inspect the AST or the symbols after
 *   the check: the inferred return type of a forward callee, the Int to
 *   Double conversion of an argument, inference through a Double parameter
 *   (A3-104, A3-109, A3-121).
 */

// §2.3: return of a forward call infers the callee from its body (Int)
//   [A3-104].
TEST_F(SpecSemanticWhiteboxTest, ReturnOfForwardCallInfersCallee) {
  // Arrange: def f(): Int = return g()      def g() = 5
  //   (bodies are single-expression blocks)
  def_main();
  FunctionSymbol *f = def("f", I);
  FunctionSymbol *g = def("g");
  add(f, ret(call(g)));
  add(g, integer(5));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  EXPECT_TRUE(has_type(g->return_type, I));
  EXPECT_TRUE(
      has_type(stmt(f, 0)->data.return_statement.value->inferred_type, I));
}

// §1.3, §2.3: an inferred Int result passed to a Double parameter is widened
//   [A3-109].
TEST_F(SpecSemanticWhiteboxTest, InferredIntResultWidensForDoubleParameter) {
  // Arrange: def g() = 1; def h(x: Double): Unit = (); val d = g(); h(d)
  //   (bodies of g and h are single-expression blocks)
  FunctionSymbol *m = def_main();
  FunctionSymbol *g = def("g");
  add(g, integer(1));
  FunctionSymbol *h = def("h", U, {R});
  add(h, unit());
  Symbol *d = var(m, "d", N);
  add(m, define(d, call(g)));
  add(m, call(h, {ref_var(d)}));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  EXPECT_TRUE(has_type(d->type, I));
  EXPECT_TRUE(widened(stmt(m, 1)->data.call.arguments[0]));
}

// §1.3, §2.3: g(x: Double) = x infers Double, which does not fit an Int
//   variable (6) [A3-121].
TEST_F(SpecSemanticWhiteboxTest, InferenceThroughDoubleParameter) {
  // Arrange: def g(x: Double) = x;  val i: Int = g(1)
  //   (body of g is a single-expression block)
  FunctionSymbol *m = def_main();
  FunctionSymbol *g = def("g", N, {R});
  add(g, ref_var(g->parameters[0]));
  add(m, define(var(m, "i", I), call(g, {integer(1)})));
  // Act
  const SemanticStatus status = check();
  // Assert
  EXPECT_EQ(status, E6);
  EXPECT_TRUE(has_type(g->return_type, R));
}

/*
 * AI-generated test block — types of if nodes and conversions in nested
 *   branches (spec 2.3, 2.4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   A statement if is Unit with a Boolean condition, and a Double function
 *   widens every Int return of a nested final if but not the Double one
 *   (catalog A4-124, A4-128).
 */

// §2.3 rule 2, §2.4: a statement if is Unit, its condition is Boolean
//   [A4-124].
TEST_F(SpecSemanticWhiteboxTest, IfIsUnitAndConditionIsBoolean) {
  // Arrange: if 1 < 2.5 then 1 else "s"
  FunctionSymbol *m = def_main();
  add(m, if_(bin(AST_LESS, integer(1), real(2.5)), block({integer(1)}),
             block({str("s")})));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  ASTNode *node = stmt(m, 0);
  EXPECT_TRUE(has_type(node->inferred_type, U));
  EXPECT_TRUE(
      has_type(node->data.if_statement.condition->inferred_type, B));
  EXPECT_TRUE(
      has_type(node->data.if_statement.then_branch->inferred_type, U));
}

// §1.3, §2.3 rules 3-4: every Int return of a nested final if in a Double
//   function is widened, the Double one is not [A4-128].
TEST_F(SpecSemanticWhiteboxTest, DoubleFunctionWidensReturnInNestedIf) {
  // Arrange: def f(): Double =
  //   if t then { if t then return 1 else return 2.5 } else return 3
  def_main();
  FunctionSymbol *f = def("f", R);
  ASTNode *inner =
      if_(truthy(), block({ret(integer(1))}), block({ret(real(2.5))}));
  add(f, if_(truthy(), block({inner}), block({ret(integer(3))})));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  ASTNode *r1 = in_block(inner->data.if_statement.then_branch, 0);
  ASTNode *r2 = in_block(inner->data.if_statement.else_branch, 0);
  ASTNode *r3 = in_block(stmt(f, 0)->data.if_statement.else_branch, 0);
  EXPECT_TRUE(widened(r1->data.return_statement.value));
  EXPECT_FALSE(widened(r2->data.return_statement.value));
  EXPECT_TRUE(widened(r3->data.return_statement.value));
}

/*
 * AI-generated test block — where the Int to Double conversion is placed
 *   (spec 1.3, 3).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   The conversion wraps a whole Int subexpression (an Int quotient stays
 *   integer division), is not added twice around a mixed sum, and is placed
 *   for definitions, assignments, return values, last values and relational
 *   operands (catalog A5-W10..W16).
 */

// §1.3, §3: `val x: Double = 7 / 2` widens the whole Int quotient, the
//   division itself stays Int [A5-W10].
TEST_F(SpecSemanticWhiteboxTest, DoubleInitWidensWholeIntQuotient) {
  // Arrange
  FunctionSymbol *m = def_main();
  add(m, define(var(m, "x", R), bin(AST_DIVIDE, integer(7), integer(2))));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  ASTNode *v = stmt(m, 0)->data.assignment.value;
  ASSERT_TRUE(widened(v));
  EXPECT_TRUE(has_type(v->inferred_type, R));
  ASTNode *q = v->data.conversion.operand;
  EXPECT_EQ(q->kind, AST_DIVIDE);
  EXPECT_TRUE(has_type(q->inferred_type, I));
  EXPECT_FALSE(widened(q->data.binary_op.left));
  EXPECT_FALSE(widened(q->data.binary_op.right));
}

// §3: in `(1 + 2) < 2.5` the Int sum is widened as a whole [A5-W11].
//   Intended overlap with the parser-level twin RelationWidensWholeIntSum in
//   spec_parser_whitebox_test.cc (precedence there, semantics here).
TEST_F(SpecSemanticWhiteboxTest, RelationalWidensIntSumAsWhole) {
  // Arrange
  FunctionSymbol *m = def_main();
  add(m, if_(bin(AST_LESS, bin(AST_ADD, integer(1), integer(2)), real(2.5)),
             block({unit()}), block({unit()})));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  ASTNode *c = stmt(m, 0)->data.if_statement.condition;
  EXPECT_TRUE(has_type(c->inferred_type, B));
  ASSERT_TRUE(widened(c->data.binary_op.left));
  ASTNode *sum = c->data.binary_op.left->data.conversion.operand;
  EXPECT_EQ(sum->kind, AST_ADD);
  EXPECT_TRUE(has_type(sum->inferred_type, I));
  EXPECT_FALSE(widened(c->data.binary_op.right));
}

// §1.3, §2.3 rule 4: `def f(): Double = return 1 + 2` widens the sum
//   [A5-W12].
TEST_F(SpecSemanticWhiteboxTest, DoubleReturnWidensIntSum) {
  // Arrange
  def_main();
  FunctionSymbol *f = def("f", R);
  add(f, ret(bin(AST_ADD, integer(1), integer(2))));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  ASTNode *v = stmt(f, 0)->data.return_statement.value;
  ASSERT_TRUE(widened(v));
  EXPECT_EQ(v->data.conversion.operand->kind, AST_ADD);
}

// §3: `val x: Double = 1 + 2.5` is already Double, only the Int operand is
//   widened [A5-W13].
TEST_F(SpecSemanticWhiteboxTest, MixedSumIsNotWrappedAgain) {
  // Arrange
  FunctionSymbol *m = def_main();
  add(m, define(var(m, "x", R), bin(AST_ADD, integer(1), real(2.5))));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  ASTNode *v = stmt(m, 0)->data.assignment.value;
  EXPECT_EQ(v->kind, AST_ADD);
  EXPECT_TRUE(widened(v->data.binary_op.left));
  EXPECT_FALSE(widened(v->data.binary_op.right));
}

// §1.3, §2.2: `x = 2 * 3` into a Double variable widens the product
//   [A5-W14].
TEST_F(SpecSemanticWhiteboxTest, DoubleAssignmentWidensIntProduct) {
  // Arrange: var x: Double = 0.0; x = 2 * 3
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", R, false);
  add(m, define(x, real(0.0)));
  add(m, assign(x, bin(AST_MULTIPLY, integer(2), integer(3))));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  ASTNode *v = stmt(m, 1)->data.assignment.value;
  ASSERT_TRUE(widened(v));
  EXPECT_EQ(v->data.conversion.operand->kind, AST_MULTIPLY);
}

// §1.3, §2.3 rule 2: `def f(): Double = 1 + 2` widens the last value
//   [A5-W15].
TEST_F(SpecSemanticWhiteboxTest, DoubleLastValueWidensIntSum) {
  // Arrange
  def_main();
  FunctionSymbol *f = def("f", R);
  add(f, bin(AST_ADD, integer(1), integer(2)));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  ASTNode *v = tail_value(stmt(f, 0));
  ASSERT_TRUE(widened(v));
  EXPECT_EQ(v->data.conversion.operand->kind, AST_ADD);
}

// §3: in `(1 / 2) / 2.0` only the inner Int quotient is widened, it stays
//   an integer division [A5-W16]. Intended overlap with the parser-level
//   twin DivisionChainWidensLeftQuotient in spec_parser_whitebox_test.cc.
TEST_F(SpecSemanticWhiteboxTest, LeftNestedQuotientWidensOnlyInner) {
  // Arrange: val x = 1 / 2 / 2.0
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", N);
  add(m, define(x, bin(AST_DIVIDE, bin(AST_DIVIDE, integer(1), integer(2)),
                       real(2.0))));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  EXPECT_TRUE(has_type(x->type, R));
  ASTNode *v = stmt(m, 0)->data.assignment.value;
  EXPECT_EQ(v->kind, AST_DIVIDE);
  ASSERT_TRUE(widened(v->data.binary_op.left));
  ASTNode *inner = v->data.binary_op.left->data.conversion.operand;
  EXPECT_EQ(inner->kind, AST_DIVIDE);
  EXPECT_FALSE(widened(inner->data.binary_op.left));
  EXPECT_FALSE(widened(inner->data.binary_op.right));
}

/*
 * AI-generated test block — results and arguments of builtin functions
 *   (spec 4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   A builtin result stored into an incompatible annotated variable is 6,
 *   wrong builtin arguments missing from the existing tables are 5 (catalog
 *   A6-200, A6-201; strLength(Int) is SemanticTest.BuiltinWithWrongArgument
 *   IsError5 and is not repeated).
 */
namespace {

struct BuiltinRow {
  const char *name;    ///< gtest name of the row.
  const char *builtin; ///< Called builtin function.
  Types arguments;
  BaseType annotation; ///< Type of `v` in `val v: T = builtin(...)`.
};

void PrintTo(const BuiltinRow &row, std::ostream *out) { *out << row.name; }

const BuiltinRow kResults[] = {
    // §4: print returns Unit, not Int [A6-200].
    {"PrintIntoInt", "print", {I}, I},
    // §4: readInt returns Int, not String [A6-200].
    {"ReadIntIntoString", "readInt", {}, S},
    // §4: toStr returns String, not Int [A6-200].
    {"ToStrIntoInt", "toStr", {I}, I},
    // §4: toInt returns Int, not String [A6-200].
    {"ToIntIntoString", "toInt", {R}, S},
    // §4: subStr returns String, not Int [A6-200].
    {"SubStrIntoInt", "subStr", {S, I, I}, I},
    // §4: ord returns Int, not String [A6-200].
    {"OrdIntoString", "ord", {S, I}, S},
    // §4: readLine returns String, not Double [A6-200].
    {"ReadLineIntoDouble", "readLine", {}, R},
    // §4: readDouble returns Double, not String [A6-200].
    {"ReadDoubleIntoString", "readDouble", {}, S},
    // §4: readLine returns String, not Unit [A6-200].
    {"ReadLineIntoUnit", "readLine", {}, U},
};

const BuiltinRow kBadArguments[] = {
    // §4: the second argument of subStr is an Int, not a String [A6-201].
    {"SubStrWithStringStart", "subStr", {S, S, I}, N},
    // §4: the first argument of subStr is a String, not Unit [A6-201].
    {"SubStrOfUnit", "subStr", {U, I, I}, N},
    // §4: ord takes two arguments, none is 5 [A6-201].
    {"OrdWithoutArguments", "ord", {}, N},
};

Nodes literals(const Types &types) {
  Nodes nodes;
  for (BaseType t : types) {
    nodes.push_back(value(t));
  }
  return nodes;
}

} // namespace

class SpecSemanticWhiteboxBuiltinResultTest
    : public SpecSemanticWhiteboxTableTest<BuiltinRow> {};

TEST_P(SpecSemanticWhiteboxBuiltinResultTest, WrongResultTypeIsError6) {
  // Arrange: val v: <annotation> = <builtin>(<literals>)
  const BuiltinRow &row = GetParam();
  FunctionSymbol *m = def_main();
  Symbol *v = var(m, "v", row.annotation);
  add(m, define(v, call(ref(row.builtin), literals(row.arguments))));
  // Act
  const SemanticStatus status = check();
  // Assert
  EXPECT_EQ(status, E6);
}
INSTANTIATE_TEST_SUITE_P(SpecSemanticWhitebox,
                         SpecSemanticWhiteboxBuiltinResultTest,
                         ::testing::ValuesIn(kResults), row_name<BuiltinRow>);

class SpecSemanticWhiteboxBuiltinArgumentTest
    : public SpecSemanticWhiteboxTableTest<BuiltinRow> {};

TEST_P(SpecSemanticWhiteboxBuiltinArgumentTest, WrongArgumentIsError5) {
  // Arrange: <builtin>(<literals>) as a statement
  const BuiltinRow &row = GetParam();
  FunctionSymbol *m = def_main();
  add(m, call(ref(row.builtin), literals(row.arguments)));
  // Act
  const SemanticStatus status = check();
  // Assert
  EXPECT_EQ(status, E5);
}
INSTANTIATE_TEST_SUITE_P(SpecSemanticWhitebox,
                         SpecSemanticWhiteboxBuiltinArgumentTest,
                         ::testing::ValuesIn(kBadArguments),
                         row_name<BuiltinRow>);

/*
 * AI-generated test block — builtin results through conversions and
 *   inference, registered signatures (spec 1.3, 2.3, 4).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Builtin Int results widened for a Double variable, return and last
 *   value, a type inferred from a builtin result, the exact signatures of
 *   all ten builtins of chapter 4 and the type of a builtin call node
 *   (catalog A6-203, A6-205, A6-207, A6-208, A6-209, A6-217, A6-218).
 */

// §1.3, §4: `x = readInt()` into a Double variable is widened [A6-203].
TEST_F(SpecSemanticWhiteboxTest, AssignedReadIntToDoubleVarIsWidened) {
  // Arrange: var x: Double = 1.0; x = readInt()
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", R, false);
  add(m, define(x, real(1.0)));
  add(m, assign(x, call(ref("readInt"))));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  EXPECT_TRUE(widened(stmt(m, 1)->data.assignment.value));
}

// §1.3, §2.3 rule 4, §4: `return ord("a", 0)` from a Double function is
//   widened [A6-205].
TEST_F(SpecSemanticWhiteboxTest, ReturnedOrdFromDoubleFunctionIsWidened) {
  // Arrange
  def_main();
  FunctionSymbol *f = def("f", R);
  add(f, ret(call(ref("ord"), {str("a"), integer(0)})));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  EXPECT_TRUE(widened(stmt(f, 0)->data.return_statement.value));
}

// §1.3, §2.3 rule 2, §4: a last readInt() of a Double function is widened
//   [A6-207].
TEST_F(SpecSemanticWhiteboxTest, LastReadIntInDoubleFunctionIsWidened) {
  // Arrange
  def_main();
  FunctionSymbol *f = def("f", R);
  add(f, call(ref("readInt")));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  EXPECT_TRUE(widened(tail_value(stmt(f, 0))));
}

// §2.3, §4: f ending with readLine() infers String, which does not fit an
//   Int variable (6) [A6-208].
TEST_F(SpecSemanticWhiteboxTest, InferredStringFromReadLineRejectsIntUse) {
  // Arrange: def f() = readLine();  val v: Int = f()
  //   (body of f is a single-expression block)
  FunctionSymbol *m = def_main();
  FunctionSymbol *f = def("f");
  add(f, call(ref("readLine")));
  add(m, define(var(m, "v", I), call(f)));
  // Act
  const SemanticStatus status = check();
  // Assert
  EXPECT_EQ(status, E6);
  EXPECT_TRUE(has_type(f->return_type, S));
}

// §1.3, §2.3, §4: f ending with toInt(1.5) infers Int, which widens for a
//   Double variable [A6-209].
TEST_F(SpecSemanticWhiteboxTest, InferredIntFromToIntWidensToDouble) {
  // Arrange: def f() = toInt(1.5);  val v: Double = f()
  //   (body of f is a single-expression block)
  FunctionSymbol *m = def_main();
  FunctionSymbol *f = def("f");
  add(f, call(ref("toInt"), {real(1.5)}));
  add(m, define(var(m, "v", R), call(f)));
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  EXPECT_TRUE(has_type(f->return_type, I));
  EXPECT_TRUE(widened(stmt(m, 0)->data.assignment.value));
}

namespace {

struct Signature {
  const char *name;
  BaseType result;
  Types parameters;
};

/// Chapter 4 of the spec; Any is TYPE_ANY.
const Signature kSignatures[] = {
    {"print", U, {TYPE_ANY}}, {"readLine", S, {}},      {"readInt", I, {}},
    {"readDouble", R, {}},    {"toStr", S, {TYPE_ANY}}, {"toInt", I, {R}},
    {"strLength", I, {S}},    {"subStr", S, {S, I, I}}, {"ord", I, {S, I}},
    {"chr", S, {I}},
};

} // namespace

// §4: every builtin is registered with exactly the signature of chapter 4
//   [A6-217].
TEST_F(SpecSemanticWhiteboxTest, RegisteredSignaturesMatchChapter4Exactly) {
  // Arrange: main references all ten names
  add(def_main(), unit());
  for (const Signature &sig : kSignatures) {
    ref(sig.name);
  }
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  for (const Signature &sig : kSignatures) {
    FunctionSymbol *f = ref(sig.name);
    EXPECT_TRUE(f->is_builtin) << sig.name;
    EXPECT_TRUE(f->has_explicit_return_type) << sig.name;
    EXPECT_TRUE(has_type(f->return_type, sig.result)) << sig.name;
    ASSERT_EQ(f->argument_count, sig.parameters.size()) << sig.name;
    for (size_t i = 0; i < sig.parameters.size(); ++i) {
      EXPECT_TRUE(has_type(f->parameters[i]->type, sig.parameters[i]))
          << sig.name << i;
    }
  }
}

// §4: a call node of subStr gets the builtin result type String [A6-218].
TEST_F(SpecSemanticWhiteboxTest, CallNodeGetsBuiltinReturnType) {
  // Arrange: subStr("a", 0, 1) as a statement
  FunctionSymbol *m = def_main();
  ASTNode *c = call(ref("subStr"), {str("a"), integer(0), integer(1)});
  add(m, c);
  add(m, unit());
  // Act
  const SemanticStatus status = check();
  // Assert
  ASSERT_EQ(status, OK);
  EXPECT_TRUE(has_type(c->inferred_type, S));
}
