/*
 * AI-generated test block — shared rows, runner and AST fixture for the
 * extension tests.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write spec conformance tests for the IFJ26 extensions (spec 5.1-5.10)
 *   and for the missing-else semantics of spec 2.3. The language profile is
 *   the base language plus FUNEXP: FUNEXP tests run, every other extension
 *   waits as DISABLED_ in a fixture Ext<NAME>Test. Black-box cases are whole
 *   programs run through tests/spec_harness.hpp; white-box cases build the AST
 *   by hand like tests/semantic_edge_test.cc, lex single lines or query the
 *   PSA precedence table.
 *
 * The RC-* root-cause IDs in the "Fails today:" comments (RC-EXT-* included)
 * are explained in tests/spec_harness.hpp.
 */
#include <cstdio>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "spec_harness.hpp"

extern "C" {
#include "../src/ast.h"
#include "../src/function_table.h"
#include "../src/keyword.h"
#include "../src/lexer.h"
#include "../src/parser.h"
// psa.h has no include guard and is force-included (C linkage) by the build.
#include "../src/semantic.h"
#include "../src/symtable.h"
}

namespace {

/** One black-box program body with the exit code the spec requires. */
struct ExtCase {
  const char *name;
  const char *source;
  int expected;
};

/** Prints a row as its name in gtest failure messages. */
void PrintTo(const ExtCase &row, std::ostream *os) { *os << row.name; }

/** Uses the row name as the gtest parameter name. */
std::string ext_case_name(const ::testing::TestParamInfo<ExtCase> &info) {
  return info.param.name;
}

/** Compiles the row's program and checks the chapter 6 exit code. */
void expect_spec_exit_code(const ExtCase &row) {
  // Arrange: the row holds the program body; the harness adds the prolog.
  const char *source = row.source;

  // Act
  int code = ifj26_exit_code(source);

  // Assert
  EXPECT_EQ(code, row.expected) << source;
}

using Nodes = std::vector<ASTNode *>;
using Types = std::vector<BaseType>;

constexpr BaseType N = TYPE_UNKNOWN, I = TYPE_INT, R = TYPE_REAL, U = TYPE_UNIT,
                   B = TYPE_BOOL;
constexpr SemanticStatus OK = SEMANTIC_OK, E3 = SEMANTIC_UNDEFINED,
                         E5 = SEMANTIC_BAD_CALL, E6 = SEMANTIC_TYPE_ERROR;

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
  if (n != nullptr) {
    n->data.assignment.is_definition = true;
  }
  return n;
}
ASTNode *ret(ASTNode *v) { return ast_node_create_return(v, 1, 1); }
ASTNode *bin(ASTNodeKind k, ASTNode *l, ASTNode *r) {
  return ast_node_create_binary_op(k, l, r, 1, 1);
}
/** A Boolean value `1 < 2`: the only way to get one in the base language. */
ASTNode *truthy() { return bin(AST_LESS, integer(1), integer(2)); }
/** `if c then t [else e]`; a NULL @p e is an if without else. */
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
bool widened(const ASTNode *n) {
  return n != nullptr && n->kind == AST_IMPLICIT_INT_TO_REAL;
}
/** True when @p t is the scalar type @p b (the unresolved type for
 *  TYPE_UNKNOWN). */
bool has_type(DataType t, BaseType b) { return type_equal(t, type_scalar(b)); }

/** Defines a local `val` (or `var` if !@p constant) in @p f's scope. */
Symbol *var(FunctionSymbol *f, const char *name, BaseType type,
            bool constant = true) {
  Symbol *s = nullptr;
  if (f != nullptr) {
    symbol_table_define(f->local_scope, name, type_scalar(type), true, constant,
                        &s);
  }
  return s;
}
/** Appends @p s to the body of @p f; false when anything is missing. */
bool add(FunctionSymbol *f, ASTNode *s) {
  return f != nullptr && s != nullptr &&
         ast_node_block_append_statement(f->ast_root, s) == AST_OK;
}

/** Owns one Program whose functions and bodies are built by hand. */
class ExtAstFixture : public ::testing::Test {
protected:
  Program program{};
  void SetUp() override { ASSERT_EQ(program_init(&program), PARSER_OK); }
  void TearDown() override { program_destroy(&program); }
  SemanticStatus check() { return semantic_check(&program); }
  /** The function symbol @p name (created on first use, like a call). */
  FunctionSymbol *ref(const char *name) {
    FunctionSymbol *f = nullptr;
    function_table_get_or_create(&program.functions, name, &f);
    return f;
  }
  /** `def name(params...)[: ret] =` with an empty body; NULL on failure. */
  FunctionSymbol *def(const char *name, BaseType ret_type = N,
                      const Types &params = {}, bool is_main = false) {
    FunctionSymbol *f = ref(name);
    if (f == nullptr) {
      return nullptr;
    }
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
  FunctionSymbol *def_main() { return def("main", U, {}, true); }
};

} // namespace

/*
 * AI-generated test block — FUNEXP (spec 5.1), active.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   FUNEXP is part of the team's language profile, so its tests run. Cover
 *   calls as operands of expressions with expression arguments and nesting
 *   (black-box), and the semantic checks of such calls on hand-built ASTs:
 *   types of nested calls, errors inside nested arguments, Int-to-Double
 *   widening of a call result and of an expression argument, inference
 *   order of callees and recursion through a nested argument.
 */
namespace {

class ExtFunexpTest : public ::testing::TestWithParam<ExtCase> {};

const ExtCase kFunexpCases[] = {
    // §5.1 (A7-001): a call is an operand, its arguments are any
    // expressions and calls nest. Positive control for A7-004.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (val -> 99),
    // then RC-STMT-CRASH for the print statement.
    {"NestedBuiltinCallsInsideExpressions",
     R"IFJ(@main def main(): Unit =
  val result = 2 * strLength(subStr("abcd", 1, 3)) + 1
  print(toStr(result + strLength("xy")))
)IFJ",
     0},
    // §5.1, §2.3 (A7-002): nested user calls with an expression argument,
    // called before the definition.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99), then
    // RC-ONELINE for `def inc(x: Int): Int = x + 1`.
    {"UserCallsNestAndReferForward",
     R"IFJ(@main def main(): Unit =
  val v = inc(inc(1) * 2) - inc(0)
  print(v)

def inc(x: Int): Int = x + 1
)IFJ",
     0},
    // §5.1, §3 (A7-003): print returns Unit and `Int + Unit` is a type error.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"UnitCallAsOperandIsError6",
     R"IFJ(@main def main(): Unit =
  val v = 1 + print("a")
)IFJ",
     6},
    // §5.1, §6 (A7-004): a call left open up to EOF is a syntax error (a
    // line break inside parentheses is white space). Control: A7-001.
    // Passes vacuously today (RC-PROLOG); without the prolog RC-EXPR (99).
    {"UnclosedCallInExpressionIsError2",
     R"IFJ(@main def main(): Unit =
  val v = 1 + strLength("a"
)IFJ",
     2},
};

TEST_P(ExtFunexpTest, ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Spec, ExtFunexpTest, ::testing::ValuesIn(kFunexpCases),
                         ext_case_name);

/** Hand-built ASTs for the FUNEXP semantic checks. */
class ExtFunexpAstTest : public ExtAstFixture {};

// §5.1 (A7-121): `val r = 2 * g(readInt() + 1)` with `def g(p: Int): Int`
// is accepted and r is inferred as Int.
TEST_F(ExtFunexpAstTest, CallInsideArithmetic) {
  // Arrange
  FunctionSymbol *m = def_main();
  FunctionSymbol *g = def("g", I, {I});
  Symbol *r = var(m, "r", N);
  Symbol *p = var(g, "p", I);
  ASSERT_NE(r, nullptr);
  ASSERT_NE(p, nullptr);
  ASSERT_TRUE(add(g, ref_var(p)));
  ASTNode *arg = bin(AST_ADD, call(ref("readInt")), integer(1));
  ASSERT_TRUE(add(m, define(r, bin(AST_MULTIPLY, integer(2), call(g, {arg})))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
  EXPECT_TRUE(has_type(r->type.type, I));
}

// §5.1, §3 (A7-122): `val r = 1 + print("a")` is Int + Unit -> 6.
TEST_F(ExtFunexpAstTest, UnitCallAsOperandIsError6) {
  // Arrange
  FunctionSymbol *m = def_main();
  Symbol *r = var(m, "r", N);
  ASSERT_NE(r, nullptr);
  ASTNode *printed = call(ref("print"), {str("a")});
  ASSERT_TRUE(add(m, define(r, bin(AST_ADD, integer(1), printed))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, E6);
}

// §5.1, §4 (A7-123): a bad argument type in a nested call keeps code 5:
// `print(toStr(strLength(5)))`.
TEST_F(ExtFunexpAstTest, NestedBadArgumentTypeIsError5) {
  // Arrange
  FunctionSymbol *m = def_main();
  ASSERT_NE(m, nullptr);
  ASTNode *length = call(ref("strLength"), {integer(5)});
  ASSERT_TRUE(add(m, call(ref("print"), {call(ref("toStr"), {length})})));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, E5);
}

// §5.1, §4 (A7-123): too many arguments of a call inside an expression:
// `val r = 1 + strLength("a", "b")` -> 5.
TEST_F(ExtFunexpAstTest, TooManyArgumentsInExpressionIsError5) {
  // Arrange
  FunctionSymbol *m = def_main();
  Symbol *r = var(m, "r", N);
  ASSERT_NE(r, nullptr);
  ASTNode *two = call(ref("strLength"), {str("a"), str("b")});
  ASSERT_TRUE(add(m, define(r, bin(AST_ADD, integer(1), two))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, E5);
}

// §5.1, §4 (A7-123): an argument for a nullary builtin inside an
// expression: `val r = 1 + readInt(1)` -> 5.
TEST_F(ExtFunexpAstTest, ArgumentOfNullaryBuiltinInExpressionIsError5) {
  // Arrange
  FunctionSymbol *m = def_main();
  Symbol *r = var(m, "r", N);
  ASSERT_NE(r, nullptr);
  ASTNode *read = call(ref("readInt"), {integer(1)});
  ASSERT_TRUE(add(m, define(r, bin(AST_ADD, integer(1), read))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, E5);
}

// §5.1, §6 (A7-123): an undefined function inside an expression:
// `val r = 1 + foo(2)` -> 3.
TEST_F(ExtFunexpAstTest, UndefinedFunctionInExpressionIsError3) {
  // Arrange
  FunctionSymbol *m = def_main();
  Symbol *r = var(m, "r", N);
  ASSERT_NE(r, nullptr);
  ASTNode *foo = call(ref("foo"), {integer(2)});
  ASSERT_TRUE(add(m, define(r, bin(AST_ADD, integer(1), foo))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, E3);
}

// §5.1, §1.3 (A7-124): `val r = 1.5 + readInt()` widens the call result.
TEST_F(ExtFunexpAstTest, IntCallResultWidensInDoubleExpression) {
  // Arrange
  FunctionSymbol *m = def_main();
  Symbol *r = var(m, "r", N);
  ASSERT_NE(r, nullptr);
  ASTNode *sum = bin(AST_ADD, real(1.5), call(ref("readInt")));
  ASSERT_NE(sum, nullptr);
  ASSERT_TRUE(add(m, define(r, sum)));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
  EXPECT_TRUE(has_type(r->type.type, R));
  const ASTNode *right = sum->data.binary_op.right;
  ASSERT_TRUE(widened(right));
  ASSERT_NE(right->data.conversion.operand, nullptr);
  EXPECT_EQ(right->data.conversion.operand->kind, AST_FUNCTION_CALL);
}

// §5.1, §1.3 (A7-125): `h(1 + 2)` with `def h(x: Double): Unit` widens the
// expression argument.
TEST_F(ExtFunexpAstTest, ExpressionArgumentWidensToDoubleParameter) {
  // Arrange
  FunctionSymbol *m = def_main();
  FunctionSymbol *h = def("h", U, {R});
  ASSERT_TRUE(add(h, unit()));
  ASTNode *c = call(h, {bin(AST_ADD, integer(1), integer(2))});
  ASSERT_NE(c, nullptr);
  ASSERT_TRUE(add(m, c));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
  ASSERT_EQ(c->data.call.argument_count, 1U);
  const ASTNode *arg = c->data.call.arguments[0];
  ASSERT_TRUE(widened(arg));
  ASSERT_NE(arg->data.conversion.operand, nullptr);
  EXPECT_EQ(arg->data.conversion.operand->kind, AST_ADD);
}

// §5.1, §2.3 (A7-126): `def a() = b() * 2` is inferred from b even when a
// comes first: `def b() = 3.0` makes a Double, so `val v: Double = a()`
// is accepted.
TEST_F(ExtFunexpAstTest, DoubleCalleeInferredBeforeCallerExpression) {
  // Arrange
  FunctionSymbol *m = def_main();
  Symbol *v = var(m, "v", R);
  FunctionSymbol *a = def("a");
  FunctionSymbol *b = def("b");
  ASSERT_NE(v, nullptr);
  ASSERT_NE(a, nullptr);
  ASSERT_TRUE(add(a, bin(AST_MULTIPLY, call(b), integer(2))));
  ASSERT_TRUE(add(b, real(3.0)));
  ASSERT_TRUE(add(m, define(v, call(a))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
  EXPECT_TRUE(has_type(a->return_type.type, R));
}

// §5.1, §2.3 (A7-126): the same order with `def b() = 3` makes a Int.
TEST_F(ExtFunexpAstTest, IntCalleeInferredBeforeCallerExpression) {
  // Arrange
  FunctionSymbol *m = def_main();
  FunctionSymbol *a = def("a");
  FunctionSymbol *b = def("b");
  ASSERT_NE(a, nullptr);
  ASSERT_TRUE(add(a, bin(AST_MULTIPLY, call(b), integer(2))));
  ASSERT_TRUE(add(b, integer(3)));
  ASSERT_TRUE(add(m, call(a)));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
  EXPECT_TRUE(has_type(a->return_type.type, I));
}

// §5.1, §2.3 (A7-127): direct recursion hidden in a nested argument still
// needs an explicit return type: `def f(x: Int) = print(toStr(f(1)))` -> 6.
TEST_F(ExtFunexpAstTest, RecursionThroughNestedArgumentNeedsAnnotation) {
  // Arrange
  ASSERT_NE(def_main(), nullptr);
  FunctionSymbol *f = def("f", N, {I});
  ASTNode *inner = call(ref("toStr"), {call(f, {integer(1)})});
  ASSERT_TRUE(add(f, call(ref("print"), {inner})));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, E6);
}

} // namespace

/*
 * AI-generated test block — missing else (spec 2.3 rule 3, 5.2), active
 * white-box exception.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Spec 2.3 rule 3 defines a missing else (it behaves as the branch `()`)
 *   only for BOOLTHEN; in the base language else is mandatory by syntax
 *   (spec 2.4), so the semantic checker never gets such an AST from a real
 *   program today. The team already wrote this semantics (semantic.c), so
 *   keep these tests active as white-box tests of that code: build ifs
 *   with else = NULL by hand and cover return types, inference, else-if
 *   chains, errors in the then branch, return and recursion in unannotated
 *   functions and the Boolean condition.
 *
 * Exception to the profile: this is BOOLTHEN behaviour, yet it stays
 * active because it does not contradict the base language (the base
 * parser cannot build these ASTs) and it pins code the team already has.
 */
namespace {

class ExtBoolthenMissingElseTest : public ExtAstFixture {};

// §2.3 rule 3, §5.2 (A7-102): `def f() = if c then {print("x")}`: both
// branches are Unit, so f is inferred as Unit.
TEST_F(ExtBoolthenMissingElseTest, MissingElseWithUnitBranchInfersUnit) {
  // Arrange
  ASSERT_NE(def_main(), nullptr);
  FunctionSymbol *f = def("f");
  ASSERT_NE(f, nullptr);
  ASSERT_TRUE(add(f, if_(truthy(), block({call(ref("print"), {str("x")})}))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
  EXPECT_TRUE(has_type(f->return_type.type, U));
}

// §2.3 rule 3 item 1, rule 4 (A7-104): `def f(): Unit = if c then {1}` is
// accepted, the value of a Unit function is discarded.
TEST_F(ExtBoolthenMissingElseTest, MissingElseInExplicitUnitFunction) {
  // Arrange
  ASSERT_NE(def_main(), nullptr);
  FunctionSymbol *f = def("f", U);
  ASSERT_TRUE(add(f, if_(truthy(), block({integer(1)}))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

// §2.3 rule 3 (A7-105): `def f(): Int = if c then {return 1} else {if c
// then {return 2}}`: the nested if lacks else, so not every path returns.
TEST_F(ExtBoolthenMissingElseTest, ElseIfChainWithoutFinalElseIsError6) {
  // Arrange
  ASSERT_NE(def_main(), nullptr);
  FunctionSymbol *f = def("f", I);
  ASTNode *inner = if_(truthy(), block({ret(integer(2))}));
  ASSERT_TRUE(add(f, if_(truthy(), block({ret(integer(1))}), block({inner}))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, E6);
}

// §2.3 rule 3 (A7-106): the same chain with a final `else {return 3}`.
// Twin of semantic_edge_test NestedIfAllReturn, kept here as the positive
// control of A7-105.
TEST_F(ExtBoolthenMissingElseTest, ElseIfChainAllReturning) {
  // Arrange
  ASSERT_NE(def_main(), nullptr);
  FunctionSymbol *f = def("f", I);
  ASTNode *inner =
      if_(truthy(), block({ret(integer(2))}), block({ret(integer(3))}));
  ASSERT_TRUE(add(f, if_(truthy(), block({ret(integer(1))}), block({inner}))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

// §2.3 rule 3 (A7-107): `def f(): Int = { if c then {return 1}; 2 }`: the
// if is not the last statement, the body ends with an Int.
TEST_F(ExtBoolthenMissingElseTest, MissingElseNotLastStatement) {
  // Arrange
  ASSERT_NE(def_main(), nullptr);
  FunctionSymbol *f = def("f", I);
  ASSERT_NE(f, nullptr);
  ASSERT_TRUE(add(f, if_(truthy(), block({ret(integer(1))}))));
  ASSERT_TRUE(add(f, integer(2)));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
  EXPECT_TRUE(has_type(f->return_type.type, I));
}

// §2.3 rule 3, §2.2 (A7-108): `var k: Int`; `if c then {k = "s"}`: the
// then branch is checked even without else -> 6.
TEST_F(ExtBoolthenMissingElseTest, ErrorInThenOfMissingElseIsReported) {
  // Arrange
  FunctionSymbol *m = def_main();
  Symbol *k = var(m, "k", I, false);
  ASSERT_NE(k, nullptr);
  ASSERT_TRUE(add(m, if_(truthy(), block({assign(k, str("s"))}))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, E6);
}

// §2.3 rule 1 (A7-109): `def f() = if c then {return}`: return needs an
// explicit return type, also inside an if without else.
TEST_F(ExtBoolthenMissingElseTest, ReturnInMissingElseNeedsAnnotation) {
  // Arrange
  ASSERT_NE(def_main(), nullptr);
  FunctionSymbol *f = def("f");
  ASSERT_TRUE(add(f, if_(truthy(), block({ret(nullptr)}))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, E6);
}

// §2.3 (A7-110): `def f() = if c then {f()}`: direct recursion needs an
// explicit return type.
TEST_F(ExtBoolthenMissingElseTest, RecursionInMissingElseNeedsAnnotation) {
  // Arrange
  ASSERT_NE(def_main(), nullptr);
  FunctionSymbol *f = def("f");
  ASSERT_TRUE(add(f, if_(truthy(), block({call(f)}))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, E6);
}

// §2.4, §3 (A7-111): `if 1 then {()}`: the condition must be Boolean.
TEST_F(ExtBoolthenMissingElseTest, MissingElseConditionMustBeBoolean) {
  // Arrange
  FunctionSymbol *m = def_main();
  ASSERT_TRUE(add(m, if_(integer(1), block({unit()}))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, E6);
}

// §2.3 rule 3 (A7-112): `def f() = if c then {()} else {if c then {()}}`:
// every branch, the missing one too, is Unit -> f is Unit.
TEST_F(ExtBoolthenMissingElseTest, NestedMissingElseAllUnitInfersUnit) {
  // Arrange
  ASSERT_NE(def_main(), nullptr);
  FunctionSymbol *f = def("f");
  ASSERT_NE(f, nullptr);
  ASTNode *inner = if_(truthy(), block({unit()}));
  ASSERT_TRUE(add(f, if_(truthy(), block({unit()}), block({inner}))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
  EXPECT_TRUE(has_type(f->return_type.type, U));
}

} // namespace

/*
 * AI-generated test block — BOOLTHEN (spec 5.2), disabled except one PSA
 * guard.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   BOOLTHEN is not in the team's profile, so its cases wait as DISABLED_:
 *   Boolean literals, storing, passing, returning and comparing Booleans,
 *   priorities of ! && ||, else-if alignment, if without else, readBoolean
 *   (black-box), the matching semantic rules on hand-built ASTs and the
 *   PSA cells of ! && ||. Keep active only the guard that unary ! cannot
 *   follow an operand, which holds in every profile.
 *
 * How to enable: once BOOLTHEN is implemented, drop the DISABLED_ prefix
 * from the TEST_P of ExtBoolthenTest and from every TEST_F of
 * ExtBoolthenAstTest and ExtBoolthenPsaTest (ExtBoolthenMissingElseTest
 * above is already active). At the same time flip the base-language
 * tests that forbid it: missing else -> 2 in
 * tests/spec_control_flow_test.cc (A4-002, 003, 004) and the tests that
 * pin Boolean values as errors (semantic_bug_test.cc
 * ReturningBoolFromUnitFunctionIsTypeError, BoolExpressionStatementIs-
 * TypeError; Bool rows of semantic_edge_test.cc and semantic_gap_test.cc;
 * if without else in parser_gap_test.cc).
 */
namespace {

class ExtBoolthenTest : public ::testing::TestWithParam<ExtCase> {};

const ExtCase kBoolthenCases[] = {
    // §5.2 (A7-010): literal true, a Boolean val, parameter, return type, !
    // and toStr(Boolean).
    // Fails today: RC-PROLOG (2); without the prolog RC-ONELINE (2); then
    // RC-EXT-BOOLTHEN (true is an identifier, Booleans cannot be stored).
    {"BooleanIsStoredPassedAndReturned",
     R"IFJ(def neg(b: Boolean): Boolean = !b

@main def main(): Unit =
  val t = true
  val f = neg(t)
  val s = toStr(f)
  print(s)
)IFJ",
     0},
    // §5.2 (A7-011): && binds looser than relations (otherwise `0 && x`
    // -> 6); if without else as the last statement of a Unit function.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"AndBindsLooserThanRelational",
     R"IFJ(@main def main(): Unit =
  val x = readInt()
  val b = x > 0 && x < 10 || x == 42
  if b then
    print("ok")
)IFJ",
     0},
    // §5.2 (A7-012): ! binds tighter than relations, `(!1) < 2` applies !
    // to an Int -> 6 (a wrong priority would give `!(1 < 2)` -> 0).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"NotBindsTighterThanRelational",
     R"IFJ(@main def main(): Unit =
  val b = !1 < 2
)IFJ",
     6},
    // §5.2 (A7-013): operands of logical operators must be Boolean.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"LogicalOperandMustBeBoolean",
     R"IFJ(@main def main(): Unit =
  val b = true && 1
)IFJ",
     6},
    // §5.2 (A7-014): Booleans compare with == and != (parenthesised, no
    // relational chain).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99); then
    // RC-EXT-BOOLTHEN (Bool operand of == is rejected).
    {"BooleansCompareWithEqualityOperators",
     R"IFJ(@main def main(): Unit =
  val e = (1 < 2) == true
  val n = e != false
  print(n)
)IFJ",
     0},
    // §5.2, §3 (A7-015): == needs equal types or two numbers.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"BooleanEqualsIntIsError6",
     R"IFJ(@main def main(): Unit =
  val e = true == 1
)IFJ",
     6},
    // §5.2, §3 (A7-016): < is defined for numbers and strings only.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"BooleanIsNotOrdered",
     R"IFJ(@main def main(): Unit =
  val e = true < false
)IFJ",
     6},
    // §2.3p, §5.2 (A7-017): a missing else is the branch (), so an Int
    // function does not return on every path -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (if condition
    // -> 2), then RC-EXT-BOOLTHEN (else is mandatory in the parser).
    {"MissingElseEndingIntFunctionIsError6",
     R"IFJ(def sign(x: Int): Int =
  if x < 0 then
    return 0 - 1

@main def main(): Unit =
  val s = sign(5)
  print(s)
)IFJ",
     6},
    // §2.3p (A7-018): if without else with a Unit branch infers Unit.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (2), then
    // RC-EXT-BOOLTHEN (else is mandatory in the parser).
    {"MissingElseWithUnitBranchInfersUnit",
     R"IFJ(def warn(x: Int) =
  if x < 0 then
    print("negative")

@main def main(): Unit =
  warn(5)
)IFJ",
     0},
    // §5.2 (A7-019): the else-if example of the spec. Control for A7-020.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"ElseIfChainAlignedWithIf",
     R"IFJ(@main def main(): Unit =
  val x = readInt()
  if x < 0 then
    print("negative")
  else if x == 0 then
    print("zero")
  else
    print("positive")
)IFJ",
     0},
    // §5.2 ("must be aligned with if"), §1.2 (A7-020): else indented like a
    // statement of the then block -> 2. Control: A7-019.
    // Passes vacuously today (RC-PROLOG); without the prolog RC-EXPR (99).
    {"ElseIfNotAlignedIsError2",
     R"IFJ(@main def main(): Unit =
  val x = readInt()
  if x < 0 then
    print("negative")
    else if x == 0 then
    print("zero")
)IFJ",
     2},
    // §5.2 (A7-021): readBoolean is a builtin that feeds a condition.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99); then
    // RC-EXT-BOOLTHEN (readBoolean is not in BUILTINS -> 3).
    {"ReadBooleanFeedsCondition",
     R"IFJ(@main def main(): Unit =
  val b = readBoolean()
  if b then
    print("yes")
  else
    print("no")
)IFJ",
     0},
    // §5.2, §1.3 (A7-022): a Boolean var assigned a relation drives while.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"BooleanVariableDrivesWhile",
     R"IFJ(@main def main(): Unit =
  var go = true
  var i = 0
  while go do
    i = i + 1
    go = i < 3
  print(i)
)IFJ",
     0},
    // §5.2, §2.3 (A7-034): a Boolean return type is inferred from the body.
    // Fails today: RC-PROLOG (2); without the prolog RC-ONELINE (2); then
    // RC-EXT-BOOLTHEN (inferred Bool return type -> 6).
    {"BooleanReturnTypeIsInferred",
     R"IFJ(def isPos(x: Int) = x > 0

@main def main(): Unit =
  val p = isPos(3)
  if p then
    print("pos")
)IFJ",
     0},
};

TEST_P(ExtBoolthenTest, DISABLED_ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Spec, ExtBoolthenTest,
                         ::testing::ValuesIn(kBoolthenCases), ext_case_name);

/** Hand-built ASTs for the BOOLTHEN semantic rules. */
class ExtBoolthenAstTest : public ExtAstFixture {};

// §5.2, §2.3 (A7-113): `def f() = 1 < 2` infers the return type Boolean.
// Fails today: RC-EXT-BOOLTHEN (inferred Bool return type -> 6).
TEST_F(ExtBoolthenAstTest, DISABLED_InferredBooleanReturnType) {
  // Arrange
  ASSERT_NE(def_main(), nullptr);
  FunctionSymbol *f = def("f");
  ASSERT_NE(f, nullptr);
  ASSERT_TRUE(add(f, truthy()));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
  EXPECT_TRUE(has_type(f->return_type.type, B));
}

// §5.2, §1.3 (A7-114): `val b = 1 < 2` stores a Boolean.
// Fails today: RC-EXT-BOOLTHEN (a Bool value cannot be stored -> 6).
TEST_F(ExtBoolthenAstTest, DISABLED_BooleanCanBeStored) {
  // Arrange
  FunctionSymbol *m = def_main();
  Symbol *b = var(m, "b", N);
  ASSERT_NE(b, nullptr);
  ASSERT_TRUE(add(m, define(b, truthy())));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
  EXPECT_TRUE(has_type(b->type.type, B));
}

// §5.2, §1.3 (A7-114): `val b: Boolean = 1 < 2`.
// Fails today: RC-EXT-BOOLTHEN (a Bool value cannot be stored -> 6).
TEST_F(ExtBoolthenAstTest, DISABLED_AnnotatedBooleanCanBeStored) {
  // Arrange
  FunctionSymbol *m = def_main();
  Symbol *b = var(m, "b", B);
  ASSERT_NE(b, nullptr);
  ASSERT_TRUE(add(m, define(b, truthy())));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

// §5.2, §4 (A7-115): `g(1 < 2)` for `def g(a: Boolean): Unit`.
// Fails today: RC-EXT-BOOLTHEN (Bool argument -> 5).
TEST_F(ExtBoolthenAstTest, DISABLED_BooleanCanBePassed) {
  // Arrange
  FunctionSymbol *m = def_main();
  FunctionSymbol *g = def("g", U, {B});
  ASSERT_TRUE(add(g, unit()));
  ASSERT_TRUE(add(m, call(g, {truthy()})));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

// §5.2, §4 (A7-115): `def h(a: Boolean): Unit = g(a)` passes a Boolean
// parameter on. Fails today: RC-EXT-BOOLTHEN (Bool argument -> 5).
TEST_F(ExtBoolthenAstTest, DISABLED_BooleanParameterCanBePassedOn) {
  // Arrange
  FunctionSymbol *m = def_main();
  FunctionSymbol *g = def("g", U, {B});
  FunctionSymbol *h = def("h", U, {B});
  Symbol *a = var(h, "a", B);
  ASSERT_NE(a, nullptr);
  ASSERT_TRUE(add(g, unit()));
  ASSERT_TRUE(add(h, call(g, {ref_var(a)})));
  ASSERT_TRUE(add(m, unit()));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

// §5.2 (A7-116): `if (1 < 2) == (1 < 2) then () else ()`.
// Fails today: RC-EXT-BOOLTHEN (Bool operand of == -> 6).
TEST_F(ExtBoolthenAstTest, DISABLED_BooleansCompareWithEquality) {
  // Arrange
  FunctionSymbol *m = def_main();
  ASTNode *cond = bin(AST_EQUAL, truthy(), truthy());
  ASSERT_TRUE(add(m, if_(cond, block({unit()}), block({unit()}))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

// §5.2, §4 (A7-117): `val s = toStr(1 < 2)`.
// Fails today: RC-EXT-BOOLTHEN (Bool argument of a builtin -> 5).
TEST_F(ExtBoolthenAstTest, DISABLED_ToStrAcceptsBoolean) {
  // Arrange
  FunctionSymbol *m = def_main();
  Symbol *s = var(m, "s", N);
  ASSERT_NE(s, nullptr);
  ASSERT_TRUE(add(m, define(s, call(ref("toStr"), {truthy()}))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

// §5.2, §4 (A7-117): `print(1 < 2)`.
// Fails today: RC-EXT-BOOLTHEN (Bool argument of a builtin -> 5).
TEST_F(ExtBoolthenAstTest, DISABLED_PrintAcceptsBoolean) {
  // Arrange
  FunctionSymbol *m = def_main();
  ASSERT_TRUE(add(m, call(ref("print"), {truthy()})));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

// §5.2, §4 (A7-118): `val b = readBoolean()` is a builtin call.
// Fails today: RC-EXT-BOOLTHEN (readBoolean is not in BUILTINS -> 3).
TEST_F(ExtBoolthenAstTest, DISABLED_ReadBooleanIsBuiltin) {
  // Arrange
  FunctionSymbol *m = def_main();
  Symbol *b = var(m, "b", N);
  ASSERT_NE(b, nullptr);
  ASSERT_TRUE(add(m, define(b, call(ref("readBoolean")))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

// §5.2, §4 (A7-118): `readBoolean(1)` is a bad call (5).
// Fails today: RC-EXT-BOOLTHEN (readBoolean is not in BUILTINS -> 3).
TEST_F(ExtBoolthenAstTest, DISABLED_ReadBooleanTakesNoArgument) {
  // Arrange
  FunctionSymbol *m = def_main();
  ASSERT_TRUE(add(m, call(ref("readBoolean"), {integer(1)})));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, E5);
}

// §2.3 rule 4, §5.2 (A7-120): with BOOLTHEN a Boolean may be returned from a
// Unit function and discarded as a statement. In the base language this
// is disputed and SemanticBugTest pins the opposite, so it stays disabled.
// Passes today.
TEST_F(ExtBoolthenAstTest, DISABLED_BooleanValueOfUnitFunctionIsDiscarded) {
  // Arrange: def f(): Unit = return 1 < 2;  main: 1 < 2; ()
  FunctionSymbol *m = def_main();
  FunctionSymbol *f = def("f", U);
  ASSERT_TRUE(add(f, ret(truthy())));
  ASSERT_TRUE(add(m, truthy()));
  ASSERT_TRUE(add(m, unit()));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

// §5.2, §1.3 (A7-128): a Boolean parameter as a condition. Boolean
// parameters are forbidden in the base language. Passes today.
TEST_F(ExtBoolthenAstTest, DISABLED_BooleanParameterIsCondition) {
  // Arrange: def f(a: Boolean): Unit = if a then () else ()
  ASSERT_NE(def_main(), nullptr);
  FunctionSymbol *f = def("f", U, {B});
  Symbol *a = var(f, "a", B);
  ASSERT_NE(a, nullptr);
  ASSERT_TRUE(add(f, if_(ref_var(a), block({unit()}), block({unit()}))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

// §5.2, §2.3 (A7-129): `def f(): Boolean = 1 < 2`. Boolean return types
// are forbidden in the base language. Passes today.
TEST_F(ExtBoolthenAstTest, DISABLED_ExplicitBooleanReturnType) {
  // Arrange
  ASSERT_NE(def_main(), nullptr);
  ASSERT_TRUE(add(def("f", B), truthy()));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

// §5.2, §2.3 (A7-129): `def f(): Boolean = return 1 < 2`. Passes today.
TEST_F(ExtBoolthenAstTest, DISABLED_ExplicitBooleanReturnStatement) {
  // Arrange
  ASSERT_NE(def_main(), nullptr);
  ASSERT_TRUE(add(def("f", B), ret(truthy())));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

// §5.2, §2.3 (A7-129): `def f(): Boolean = 1` is a type error (6).
// Passes today.
TEST_F(ExtBoolthenAstTest, DISABLED_IntBodyOfBooleanFunctionIsError6) {
  // Arrange
  ASSERT_NE(def_main(), nullptr);
  ASSERT_TRUE(add(def("f", B), integer(1)));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, E6);
}

// §5.2, §5.1 (A7-130): `if f() then () else ()` with `def f(): Boolean`.
// Passes today.
TEST_F(ExtBoolthenAstTest, DISABLED_BooleanCallIsCondition) {
  // Arrange
  FunctionSymbol *m = def_main();
  FunctionSymbol *f = def("f", B);
  ASSERT_TRUE(add(f, truthy()));
  ASSERT_TRUE(add(m, if_(call(f), block({unit()}), block({unit()}))));

  // Act
  SemanticStatus status = check();

  // Assert
  EXPECT_EQ(status, OK);
}

/** Cells of the PSA precedence table for ! && ||. */
class ExtBoolthenPsaTest : public ::testing::Test {};

/** A token of type @p type without a value, for token_to_terminal. */
Token psa_token(TokenType type) {
  Token token = {};
  token.type = type;
  return token;
}

// §5.2, §3 (A7-145): unary ! cannot follow an operand; the cell stays empty
// in every profile. `(` and `)` are no PSA terminals any more (a
// parenthesised subexpression is one operand), so only the operand row is
// left. Passes today: ! has no table terminal (TERMINAL_INVALID) and the
// stack-only operand has no row.
TEST_F(ExtBoolthenPsaTest, NotCannotFollowOperandOrRightParen) {
  // Arrange
  const TerminalType tops[] = {TERMINAL_OPERAND};
  const TerminalType not_terminal = token_to_terminal(psa_token(TOKEN_NOT));

  for (TerminalType top : tops) {
    // Act
    PRECEDENCE cell = get_precedence(top, not_terminal);

    // Assert
    EXPECT_EQ(cell, PREC_INVALID) << "top terminal " << top;
  }
}

// §5.2, §3 (A7-146): unary ! is shifted at the start and after an operator
// (`(` is no PSA terminal any more; inside it the stack starts with `$`).
// Fails today: RC-EXT-BOOLTHEN, ! has no table terminal (the parser takes
// it as a prefix of an operand, outside the table).
TEST_F(ExtBoolthenPsaTest, DISABLED_NotIsShiftedAfterOperatorsAndStart) {
  // Arrange
  const TerminalType tops[] = {TERMINAL_DOLLAR, TERMINAL_MUL, TERMINAL_REL};
  const TerminalType not_terminal = token_to_terminal(psa_token(TOKEN_NOT));

  for (TerminalType top : tops) {
    // Act
    PRECEDENCE cell = get_precedence(top, not_terminal);

    // Assert
    EXPECT_EQ(cell, PREC_TAKE) << "top terminal " << top;
  }
}

// §5.2, §3 (A7-147): && and || bind below relations (|| lowest), so they
// reduce every higher level; at the start of a (parenthesised)
// subexpression the top terminal is `$`, so && is shifted. The operand and
// `)` rows of the old table are gone: operands are stack-only and `(`/`)`
// are no terminals.
TEST_F(ExtBoolthenPsaTest, DISABLED_LogicalOperatorsReduceHigherLevels) {
  // Arrange
  const TerminalType tops[] = {TERMINAL_PLUS, TERMINAL_MUL, TERMINAL_REL};
  const TokenType logical[] = {TOKEN_AND, TOKEN_OR};

  for (TokenType next : logical) {
    const TerminalType input = token_to_terminal(psa_token(next));
    for (TerminalType top : tops) {
      // Act + Assert
      EXPECT_EQ(get_precedence(top, input), PREC_REDUCE)
          << "top " << top << " next " << next;
    }
  }
  EXPECT_EQ(
      get_precedence(TERMINAL_DOLLAR, token_to_terminal(psa_token(TOKEN_AND))),
      PREC_TAKE);
}

} // namespace

/*
 * AI-generated test block — CONTROL (spec 5.3), disabled.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for if expressions (one-line, mixed with blocks, branch
 *   types, Int/Double widening, as a one-line function body) and for loops
 *   (to/until/by, zero literal step -> 10, non-literal step, Int bounds,
 *   immutable and scoped loop variable, guards, layout of a multi-line for,
 *   single generator).
 *
 * How to enable: CONTROL requires BOOLTHEN (spec 5.3), so enable the
 * BOOLTHEN block first; once CONTROL is implemented, drop the DISABLED_
 * prefix from the TEST_P name below.
 */
namespace {

class ExtControlTest : public ::testing::TestWithParam<ExtCase> {};

const ExtCase kControlCases[] = {
    // §5.3 (A7-023): one-line if expression and its mixes with blocks
    // (spec example). Fails today: RC-PROLOG (2); without the prolog
    // RC-EXPR (99); needs RC-EXT-CONTROL.
    {"IfExpressionFormsFromSpec",
     R"IFJ(@main def main(): Unit =
  var x = -5
  val absolute = if x < 0 then 0 - x else x
  val abs2 = if x < 0 then
    0 - x
  else
    x
  val abs3 = if x < 0 then 0 - x else
      x
  val sum = absolute + abs2 + abs3
  print(sum)
)IFJ",
     0},
    // §5.3 (A7-024): both branches of an if expression need one type.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"IfExpressionBranchTypesMustMatch",
     R"IFJ(@main def main(): Unit =
  val x = readInt()
  val a = if x < 0 then 1 else "neg"
)IFJ",
     6},
    // §5.3 ("with implicit conversion") (A7-025): Int and Double branches
    // give Double, which does not fit an Int.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"IfExpressionWidensIntBranchToDouble",
     R"IFJ(@main def main(): Unit =
  val x = readInt()
  val a = if x < 0 then 1 else 2.5
  val b: Int = a
)IFJ",
     6},
    // §5.3, §2.3 (A7-026): an if expression as a one-line function body.
    // Fails today: RC-PROLOG (2); without the prolog RC-ONELINE (2).
    {"IfExpressionAsOneLineFunctionBody",
     R"IFJ(def abs(x: Int): Int = if x < 0 then 0 - x else x

@main def main(): Unit =
  val n = -3
  val a = abs(n)
  print(a)
)IFJ",
     0},
    // §5.3 ("one-line bodies of all branches") (A7-027).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"IfStatementWithOneLineBranches",
     R"IFJ(@main def main(): Unit =
  val x = readInt()
  if x < 0 then print("n") else print("p")
)IFJ",
     0},
    // §5.3 (A7-028): to/by and until, one-line body after do and a block.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-CONTROL (2).
    {"ForToByAndUntilLoops",
     R"IFJ(@main def main(): Unit =
  for i <- 1 to 10 by 2 do print(i)
  for j <- 0 until 3 do
    print(j)
)IFJ",
     0},
    // §5.3, §6 (code 10 "zero step of a range") (A7-029).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-CONTROL (2).
    {"ZeroLiteralStepIsError10",
     R"IFJ(@main def main(): Unit =
  for i <- 1 to 10 by 0 do print(i)
)IFJ",
     10},
    // §5.3 (A7-030): a non-literal step is checked at run time (40), the
    // program compiles. Fails today: RC-PROLOG (2); without the prolog
    // RC-EXPR (99).
    {"NonLiteralStepCompiles",
     R"IFJ(@main def main(): Unit =
  val s = readInt()
  for i <- 1 to 10 by s do print(i)
)IFJ",
     0},
    // §5.3 ("bounds and step are Int expressions, otherwise 6") (A7-031).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-CONTROL (2).
    {"DoubleBoundIsError6",
     R"IFJ(@main def main(): Unit =
  for i <- 1 to 2.5 do print(i)
)IFJ",
     6},
    // §5.3, §6 (A7-032): the loop variable is immutable -> 7.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-CONTROL (2).
    {"LoopVariableIsImmutable",
     R"IFJ(@main def main(): Unit =
  for i <- 1 to 3 do
    i = 5
)IFJ",
     7},
    // §5.3, §2.2 (A7-033): the loop variable is valid only in the body.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-CONTROL (2).
    {"LoopVariableOutsideBodyIsError3",
     R"IFJ(@main def main(): Unit =
  for i <- 1 to 3 do print(i)
  print(i)
)IFJ",
     3},
    // §5.3 ("a guard ... must be Boolean") (A7-035).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-CONTROL (2).
    {"NonBooleanGuardIsError6",
     R"IFJ(@main def main(): Unit =
  for
    i <- 0 until 5
    if i
  do
    print(i)
)IFJ",
     6},
    // §5.3 (A7-036): the multi-line for with two guards from the spec.
    // Control for A7-035b and A7-036b. Fails today: RC-PROLOG (2); without
    // the prolog RC-EXPR (99).
    {"GuardedMultilineForFromSpec",
     R"IFJ(@main def main(): Unit =
  val limit = 10
  for
    i <- 0 until limit
    if i != 3
    if i < 8
  do
    print(i)
)IFJ",
     0},
    // §5.3 ("generator and guards indented deeper than for") (A7-035b).
    // Control: A7-036. Passes vacuously today (RC-PROLOG; without the
    // prolog RC-EXT-CONTROL).
    {"GeneratorNotIndentedIsError2",
     R"IFJ(@main def main(): Unit =
  for
  i <- 0 until 5
  do
    print(i)
)IFJ",
     2},
    // §5.3 ("more generators are not supported") (A7-036b). Control:
    // A7-036. Passes vacuously today (RC-PROLOG; RC-EXT-CONTROL).
    {"SecondGeneratorIsError2",
     R"IFJ(@main def main(): Unit =
  for
    i <- 0 until 2
    j <- 0 until 2
  do
    print(i)
)IFJ",
     2},
};

TEST_P(ExtControlTest, DISABLED_ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Spec, ExtControlTest,
                         ::testing::ValuesIn(kControlCases), ext_case_name);

} // namespace

/*
 * AI-generated test block — VALMEM (spec 5.4), disabled.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for member access on values: the spec chain
 *   number.floor.toInt, result types of length and floor, an Int literal
 *   and a parenthesised expression as the receiver, chains across types.
 *
 * How to enable: once VALMEM is implemented, drop the DISABLED_ prefix
 * from the TEST_P name below.
 */
namespace {

class ExtValmemTest : public ::testing::TestWithParam<ExtCase> {};

const ExtCase kValmemCases[] = {
    // §5.4 (A7-037): number.floor.toInt and toString from the spec.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"MemberChainFromSpec",
     R"IFJ(@main def main(): Unit =
  val number = 3.7
  val k = number.floor.toInt
  val s = k.toString
  print(s)
)IFJ",
     0},
    // §5.4 (table: length -> Int) (A7-038).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"LengthIsInt",
     R"IFJ(@main def main(): Unit =
  val l = "abc".length
  val s: String = l
)IFJ",
     6},
    // §5.4 (floor -> Double) (A7-039).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"FloorStaysDouble",
     R"IFJ(@main def main(): Unit =
  val d = 2.5.floor
  val i: Int = d
)IFJ",
     6},
    // §5.4 ("any value has toString"), §1.1 (A7-040): `5.` is not a Double
    // literal here. Fails today: RC-PROLOG (2); without the prolog
    // RC-EXT-VALMEM-LEX (lexer rejects `5.t` -> 1).
    {"IntLiteralReceiver",
     R"IFJ(@main def main(): Unit =
  val s = 5.toString
  print(s)
)IFJ",
     0},
    // §5.4 (A7-041): `expression.id`.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"ParenthesizedExpressionReceiver",
     R"IFJ(@main def main(): Unit =
  val s = (1 + 2).toString
  print(s)
)IFJ",
     0},
    // §5.4 ("chaining is supported") (A7-042): String -> Int -> String ->
    // Int. Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"ChainAcrossReceiverTypes",
     R"IFJ(@main def main(): Unit =
  val n = "abcd".length.toString.length
  print(n)
)IFJ",
     0},
};

TEST_P(ExtValmemTest, DISABLED_ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Spec, ExtValmemTest, ::testing::ValuesIn(kValmemCases),
                         ext_case_name);

} // namespace

/*
 * AI-generated test block — ALGEBDT (spec 5.5), disabled.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for union types and aliases (widening into a member,
 *   non-member assignment, operand without match, alias redefinition,
 *   undefined type), case classes (construction, attribute read, argument
 *   count and type, redefinition, duplicate attribute), match (spec
 *   example, non-exhaustive -> 10, branch types, binder scope, guard type)
 *   and readIntMaybe/readDoubleMaybe; plus the PSA cells that end an
 *   expression before => and match.
 *
 * How to enable: once ALGEBDT is implemented, drop the DISABLED_ prefix
 * from the TEST_P name of ExtAlgebdtTest and from the TEST_F of
 * ExtAlgebdtPsaTest below.
 */
namespace {

class ExtAlgebdtTest : public ::testing::TestWithParam<ExtCase> {};

const ExtCase kAlgebdtCases[] = {
    // §5.5 (A7-043): alias, assigning another member, Int widened to the
    // Double member (spec `foo(5)`). Fails today: RC-PROLOG (2); without
    // the prolog RC-EXT-SYNTAX (2).
    {"UnionAliasAndWideningFromSpec",
     R"IFJ(type Scalar = Int | Double | String

def show(x: Double | String): String =
  return toStr(x)

@main def main(): Unit =
  var anything: Scalar = 5
  anything = "hello"
  val a = show(5)
  print(a)
)IFJ",
     0},
    // §5.5, §1.3 (A7-044): Double is not a member of Int | String.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"NonMemberAssignmentIsError6",
     R"IFJ(@main def main(): Unit =
  var a: Int | String = 1
  a = 2.5
)IFJ",
     6},
    // §5.5, §3 (A7-045): a union value is no operand without match.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"UnionOperandWithoutMatchIsError6",
     R"IFJ(@main def main(): Unit =
  var a: Int | String = 1
  val b = a + 1
)IFJ",
     6},
    // §5.5, §6 (4 "redefinition of a type") (A7-047).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"AliasRedefinitionIsError4",
     R"IFJ(type T = Int | String
type T = Int | Double

@main def main(): Unit =
  print(1)
)IFJ",
     4},
    // §5.5, §6 (3 "undefined type") (A7-048).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"UndefinedTypeNameIsError3",
     R"IFJ(@main def main(): Unit =
  var a: Scalar = 1
)IFJ",
     3},
    // §5.5 (A7-049): construct a case class and read an attribute.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"CaseClassConstructAndRead",
     R"IFJ(case class Point(x: Int, y: Int)

@main def main(): Unit =
  val p = Point(10, 20)
  val px = p.x
  print(px)
)IFJ",
     0},
    // §5.5, §6 (5) (A7-050): wrong number of constructor arguments.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"ConstructorArgumentCountIsError5",
     R"IFJ(case class Point(x: Int, y: Int)

@main def main(): Unit =
  val p = Point(10)
)IFJ",
     5},
    // §5.5, §6 (5) (A7-051): wrong type of a constructor argument.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"ConstructorArgumentTypeIsError5",
     R"IFJ(case class Point(x: Int, y: Int)

@main def main(): Unit =
  val p = Point(10, "a")
)IFJ",
     5},
    // §5.5, §6 (4) (A7-052): a case class defined twice.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"CaseClassRedefinitionIsError4",
     R"IFJ(case class Point(x: Int, y: Int)
case class Point(a: Int)

@main def main(): Unit =
  print(1)
)IFJ",
     4},
    // §5.5, §2.3 (A7-052b): attributes are parameters, names are unique.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"DuplicateAttributeIsError4",
     R"IFJ(case class Point(x: Int, x: Int)

@main def main(): Unit =
  print(1)
)IFJ",
     4},
    // §5.5, §6 (10 "non-exhaustive match") (A7-053): literals over Int
    // never cover every value. Fails today: RC-PROLOG (2); without the
    // prolog RC-STMT-CRASH (`x match` statement -> SIGSEGV, -1).
    {"MatchWithoutDefaultIsError10",
     R"IFJ(def name(x: Int): String =
  x match
    case 0 => "zero"
    case 1 => "one"

@main def main(): Unit =
  val s = name(1)
  print(s)
)IFJ",
     10},
    // §5.5 (A7-054): literal, typed pattern with a guard, constructor
    // pattern and _ (spec example). Fails today: RC-PROLOG (2); without the
    // prolog RC-EXT-SYNTAX (2).
    {"MatchFromSpec",
     R"IFJ(case class Point(x: Int, y: Int)

def pointToStr(x: Int, y: Int) =
  val ptA = toStr(x)
  val ptB = toStr(y)
  "point " + ptA + ":" + ptB

def describe(value: Int | String | Point): String =
  value match
    case 0 => "zero"
    case n: Int if n > 0 => "positive"
    case text: String => text
    case Point(a, b) => pointToStr(a, b)
    case _ => "negative"

@main def main(): Unit =
  val d = describe(5)
  print(d)
)IFJ",
     0},
    // §5.5 ("all branches must be type compatible") (A7-055).
    // Fails today: RC-PROLOG (2); without the prolog RC-STMT-CRASH (-1).
    {"IncompatibleCaseBodiesAreError6",
     R"IFJ(def name(x: Int): String =
  x match
    case 0 => "zero"
    case _ => 1

@main def main(): Unit =
  val s = name(1)
  print(s)
)IFJ",
     6},
    // §5.5, §2.2 (A7-055b): a pattern binder is valid only in its case.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"PatternBinderScopedToItsCase",
     R"IFJ(def f(x: Int | String): Int =
  x match
    case n: Int => n
    case _ => n

@main def main(): Unit =
  val r = f(1)
  print(r)
)IFJ",
     3},
    // §5.5 ("optional guard if boolean-expression") (A7-055c): the typed
    // pattern picks a member of a union, the guard `n` is an Int.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"NonBooleanCaseGuardIsError6",
     R"IFJ(def f(x: Int | String): Int =
  x match
    case n: Int if n => n
    case _ => 0

@main def main(): Unit =
  val r = f(1)
  print(r)
)IFJ",
     6},
    // §5.5 (A7-056): readIntMaybe returns Int | Unit.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"ReadIntMaybeReturnsUnion",
     R"IFJ(@main def main(): Unit =
  val m: Int | Unit = readIntMaybe()
  print(m)
)IFJ",
     0},
    // §5.5 (A7-057): Int | Unit does not fit an Int.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"ReadIntMaybeIsNotInt",
     R"IFJ(@main def main(): Unit =
  val m: Int = readIntMaybe()
)IFJ",
     6},
    // §5.5, §4 (A7-058): readDoubleMaybe takes no argument.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"ReadDoubleMaybeTakesNoArgument",
     R"IFJ(@main def main(): Unit =
  val m = readDoubleMaybe(1)
)IFJ",
     5},
};

TEST_P(ExtAlgebdtTest, DISABLED_ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Spec, ExtAlgebdtTest,
                         ::testing::ValuesIn(kAlgebdtCases), ext_case_name);

/** PSA cells for the tokens that end an expression in ALGEBDT/LAMBDAS. */
class ExtAlgebdtPsaTest : public ::testing::Test {};

// §5.5, §5.7, §3 (A7-148): `=>` (after a guard or a lambda head) and
// `match` end an expression like `$`, so a pending operator reduces before
// them. This is a design choice of the team's PSA, not mandated by the spec.
// Fails today: token_to_terminal maps both to TERMINAL_INVALID.
TEST_F(ExtAlgebdtPsaTest, DISABLED_ArrowAndMatchEndExpression) {
  // Arrange
  const TokenType enders[] = {TOKEN_ARROW, TOKEN_KW_MATCH};

  for (TokenType next : enders) {
    Token token = {};
    token.type = next;

    // Act
    const TerminalType input = token_to_terminal(token);
    PRECEDENCE cell = get_precedence(TERMINAL_PLUS, input);

    // Assert
    EXPECT_EQ(input, TERMINAL_DOLLAR) << "next token " << next;
    EXPECT_EQ(cell, PREC_REDUCE) << "next token " << next;
  }
}

} // namespace

/*
 * AI-generated test block — INTERPOLATEDS (spec 5.6), disabled.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for interpolated strings: $id, ${expression}, $$, the
 *   multi-line form, undefined names and type errors inside, a plain string
 *   that is not interpolated and an escaped quote.
 *
 * How to enable: once INTERPOLATEDS is implemented, drop the DISABLED_
 * prefix from the TEST_P name below.
 */
namespace {

class ExtInterpolatedsTest : public ::testing::TestWithParam<ExtCase> {};

const ExtCase kInterpolatedsCases[] = {
    // §5.6 (A7-059): $id, ${expression}, $$ and the multi-line form with a
    // literal quote. Fails today: RC-PROLOG (2); without the prolog
    // RC-EXPR (99).
    {"InterpolationFromSpec",
     R"IFJ(@main def main(): Unit =
  val name = "IFJ"
  val n = 26
  val a = s"$name${n + 1} costs $$0"
  print(a)
  val b = s"""He said "hello", $name.
Second line."""
  print(b)
)IFJ",
     0},
    // §5.6, §2.2 (A7-060): an undefined $id -> 3.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"UndefinedDollarIdIsError3",
     R"IFJ(@main def main(): Unit =
  val a = s"value $x"
)IFJ",
     3},
    // §5.6, §2.2 (A7-061): an undefined name inside ${...} -> 3.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"UndefinedNameInsideBracesIsError3",
     R"IFJ(@main def main(): Unit =
  val a = s"value ${y + 1}"
)IFJ",
     3},
    // §5.6, §3 (A7-062): a type error inside ${...} -> 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"TypeErrorInsideBracesIsError6",
     R"IFJ(@main def main(): Unit =
  val a = s"value ${"a" - 1}"
)IFJ",
     6},
    // §5.6, §1.1 (A7-063): without the s prefix $... is plain text.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"PlainStringIsNotInterpolated",
     R"IFJ(@main def main(): Unit =
  val a = "$undefinedName costs $$"
  print(a)
)IFJ",
     0},
    // §5.6 ("a double quote is written as \"") (A7-065).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"EscapedQuoteInInterpolatedString",
     R"IFJ(@main def main(): Unit =
  val name = "IFJ"
  val a = s"say \"$name\""
  print(a)
)IFJ",
     0},
};

TEST_P(ExtInterpolatedsTest, DISABLED_ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Spec, ExtInterpolatedsTest,
                         ::testing::ValuesIn(kInterpolatedsCases),
                         ext_case_name);

} // namespace

/*
 * AI-generated test block — LAMBDAS (spec 5.7), disabled.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for function values: function types, lambdas with
 *   expression and block bodies, named functions as values, parameters and
 *   results of function type, return in a lambda block (10), untyped
 *   parameters, argument count, type mismatch, nullary lambdas, immutable
 *   and duplicate parameters.
 *
 * How to enable: once LAMBDAS (with FUNEXP) is implemented, drop the
 * DISABLED_ prefix from the TEST_P name below.
 */
namespace {

class ExtLambdasTest : public ::testing::TestWithParam<ExtCase> {};

const ExtCase kLambdasCases[] = {
    // §5.7 (A7-066): (T1, T2) => R, a lambda, a named function as a value,
    // function parameter and result, a lambda block without return.
    // Fails today: RC-PROLOG (2); without the prolog RC-ONELINE (2).
    {"FunctionValuesFromSpec",
     R"IFJ(def foo(x: Int) = x + 5

def apply(f: (Int) => Int, v: Int): Int = f(v)

def mk(): (Int) => Int =
  return (a: Int) =>
    val b = a * 2
    b + 1

@main def main(): Unit =
  val add: (Int, Int) => Int =
    (left: Int, right: Int) => left + right
  val addFive: (Int) => Int = foo
  val x = add(10, 8)
  val y = addFive(x)
  val g = mk()
  val z = apply(g, y)
  print(z)
)IFJ",
     0},
    // §5.7, §6 (10 "return not allowed") (A7-067).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"ReturnInLambdaBlockIsError10",
     R"IFJ(@main def main(): Unit =
  val f: (Int) => Int = (a: Int) =>
    val b = a + 1
    return b
  val r = f(1)
  print(r)
)IFJ",
     10},
    // §5.7 ("type and name of every parameter") (A7-069). Control: A7-072.
    // Passes vacuously today (RC-PROLOG; without the prolog RC-EXT-SYNTAX).
    {"UntypedLambdaParameterIsError2",
     R"IFJ(@main def main(): Unit =
  val f: (Int) => Int = (a) => a + 1
)IFJ",
     2},
    // §5.7, §6 (5) (A7-070): wrong argument count of a function value.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"FunctionValueArgumentCountIsError5",
     R"IFJ(@main def main(): Unit =
  val add: (Int, Int) => Int = (l: Int, r: Int) => l + r
  val x = add(1)
)IFJ",
     5},
    // §5.7, §6 (6) (A7-071): the lambda does not match the declared type.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"LambdaTypeMismatchIsError6",
     R"IFJ(@main def main(): Unit =
  val f: (Int) => Int = (s: String) => 1
)IFJ",
     6},
    // §5.7 ("a function without parameters has type () => R") (A7-072).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"NullaryLambda",
     R"IFJ(@main def main(): Unit =
  val k: () => Int = () => 42
  val v = k()
  print(v)
)IFJ",
     0},
    // §5.7, §6 (7) (A7-073): a lambda parameter is immutable.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"LambdaParameterIsImmutable",
     R"IFJ(@main def main(): Unit =
  val f: (Int) => Unit = (a: Int) =>
    a = 1
  f(2)
)IFJ",
     7},
    // §5.7, §6 (4) (A7-074): duplicate lambda parameter names.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"DuplicateLambdaParameterIsError4",
     R"IFJ(@main def main(): Unit =
  val f: (Int, Int) => Int = (a: Int, a: Int) => a
)IFJ",
     4},
};

TEST_P(ExtLambdasTest, DISABLED_ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Spec, ExtLambdasTest,
                         ::testing::ValuesIn(kLambdasCases), ext_case_name);

} // namespace

/*
 * AI-generated test block — LISTS (spec 5.8), disabled.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for immutable lists: List.range and indexing, the
 *   member functions, element type compatibility and widening, the empty
 *   list with and without an expected type, a list as a for source, ++,
 *   head type, takeWhile/filter with lambdas, (1 to 5).toList, generic
 *   functions (forbidden) and nested list types.
 *
 * How to enable: once LISTS is implemented, drop the DISABLED_ prefix from
 * the TEST_P name below (rows using for or lambdas also need CONTROL or
 * LAMBDAS).
 */
namespace {

class ExtListsTest : public ::testing::TestWithParam<ExtCase> {};

const ExtCase kListsCases[] = {
    // §5.8 (A7-075): List.range and indexing.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"RangeAndIndexFromSpec",
     R"IFJ(@main def main(): Unit =
  val lst = List.range(1, 10)
  val x = lst(2)
  print(x)
)IFJ",
     0},
    // §5.8 (A7-076): drop, take, slice, head, tail, toString.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"ListMembers",
     R"IFJ(@main def main(): Unit =
  val lst = List(1, 2, 3, 4)
  val a = lst.drop(1)
  val b = a.take(2)
  val c = lst.slice(1, 3)
  val h = lst.head
  val t = lst.tail
  val s = c.toString
  print(s)
)IFJ",
     0},
    // §5.8 ("all elements must have a compatible type") (A7-077).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"HeterogeneousListIsError6",
     R"IFJ(@main def main(): Unit =
  val l = List(1, "a")
)IFJ",
     6},
    // §5.8, §1.3 (A7-078): Int and Double elements give a Double list.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"IntAndDoubleElementsGiveDoubleList",
     R"IFJ(@main def main(): Unit =
  val l = List(1, 2.5)
  val d: Double = l(1)
  print(d)
)IFJ",
     0},
    // §5.8 ("an empty list needs an expected type"), §6 (6) (A7-079).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"UntypedEmptyListIsError6",
     R"IFJ(@main def main(): Unit =
  val e = List()
)IFJ",
     6},
    // §5.8 (A7-079b): an empty list with an expected type.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"TypedEmptyList",
     R"IFJ(@main def main(): Unit =
  val e: List[Int] = List()
  val s = e.toString
  print(s)
)IFJ",
     0},
    // §5.8, §5.3 (A7-080): a list as the source of a for generator.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-CONTROL (2).
    {"ListAsForGeneratorSource",
     R"IFJ(@main def main(): Unit =
  for v <- List(1, 2) do print(v)
)IFJ",
     0},
    // §5.8 (A7-081): ++ concatenates lists.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"ConcatenateLists",
     R"IFJ(@main def main(): Unit =
  val c = List(1) ++ List(2, 3)
  print(c)
)IFJ",
     0},
    // §5.8 (A7-081b): ++ of incompatible element types.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"ConcatenateDifferentElementTypesIsError6",
     R"IFJ(@main def main(): Unit =
  val c = List(1) ++ List("a")
)IFJ",
     6},
    // §5.8 (A7-081c): head has the element type.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"HeadHasElementType",
     R"IFJ(@main def main(): Unit =
  val h: String = List(1).head
)IFJ",
     6},
    // §5.8 (with LAMBDAS and BOOLTHEN) (A7-082): takeWhile and filter.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXPR (99).
    {"TakeWhileAndFilterWithLambdas",
     R"IFJ(@main def main(): Unit =
  val foo = List(1, 2, 6, 89, 20, 1)
  val goo = foo.takeWhile((x: Int) => x < 10)
  val f = foo.filter((x: Int) => x > 5)
  print(goo)
)IFJ",
     0},
    // §5.8 (A7-083): (1 to 5).toList and toString.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"RangeToListAndToString",
     R"IFJ(@main def main(): Unit =
  val itemsList: List[Int] = (1 to 5).toList
  val lstStr = itemsList.toString
  print(lstStr)
)IFJ",
     0},
    // §5.8 ("own generic functions are forbidden"), §6 (2) (A7-084).
    // Control: A7-075. Passes vacuously today (RC-PROLOG; without the
    // prolog RC-EXT-SYNTAX).
    {"GenericFunctionIsError2",
     R"IFJ(def first[T](l: List[T]): T = l.head

@main def main(): Unit =
  print(1)
)IFJ",
     2},
    // §5.8 ("T is a concrete type of the implemented extensions")
    // (A7-086): a nested list type.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"NestedListType",
     R"IFJ(@main def main(): Unit =
  val n: List[List[Int]] = List(List(1), List(2, 3))
  print(n)
)IFJ",
     0},
};

TEST_P(ExtListsTest, DISABLED_ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Spec, ExtListsTest, ::testing::ValuesIn(kListsCases),
                         ext_case_name);

} // namespace

/*
 * AI-generated test block — EXTKW (spec 5.9), disabled.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for extension methods: the three forms from the spec
 *   visible before their definition, one name twice for a type (4) and
 *   for different types, plain calls (3), argument count and type (5), the
 *   immutable receiver (7), two receivers and an extension inside a
 *   function (2).
 *
 * How to enable: once EXTKW is implemented, drop the DISABLED_ prefix from
 * the TEST_P name below.
 */
namespace {

class ExtExtkwTest : public ::testing::TestWithParam<ExtCase> {};

const ExtCase kExtkwCases[] = {
    // §5.9 (A7-087): parameterless, empty list and one parameter, used
    // before the extension block. Fails today: RC-PROLOG (2); without the
    // prolog RC-EXPR (99).
    {"MethodsFromSpecVisibleBeforeDefinition",
     R"IFJ(@main def main(): Unit =
  val x: Int = 6
  val y = x.twice
  val z = x.foo()
  val w = x.plus(12)
  print(w)

extension (value: Int)
  def twice: Int = value * 2
  def foo(): String = "foo"
  def plus(other: Int): Int = value + other
)IFJ",
     0},
    // §5.9 ("not defined twice for one type"), §6 (4) (A7-088).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"SameMethodTwiceForTypeIsError4",
     R"IFJ(extension (value: Int)
  def twice: Int = value * 2

extension (n: Int)
  def twice: Int = n + n

@main def main(): Unit =
  print(1)
)IFJ",
     4},
    // §5.9 (A7-089): one method name for different receiver types.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"SameMethodNameForDifferentTypes",
     R"IFJ(extension (value: Int)
  def twice: Int = value * 2

extension (text: String)
  def twice: String = text + text

@main def main(): Unit =
  val n = 3
  val a = n.twice
  val b = "ab".twice
  print(b)
)IFJ",
     0},
    // §5.9 ("called only by member notation"), §6 (3) (A7-090): outside
    // member notation the name plus denotes no function, which is code 3
    // ("undefined function") of chapter 6.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"PlainCallOfExtensionIsError3",
     R"IFJ(extension (value: Int)
  def plus(other: Int): Int = value + other

@main def main(): Unit =
  val w = plus(6, 12)
)IFJ",
     3},
    // §5.9, §6 (5) (A7-091): wrong argument count.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"ExtensionArgumentCountIsError5",
     R"IFJ(extension (value: Int)
  def plus(other: Int): Int = value + other

@main def main(): Unit =
  val x = 6
  val w = x.plus(1, 2)
)IFJ",
     5},
    // §5.9, §6 (5) (A7-092): wrong argument type.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"ExtensionArgumentTypeIsError5",
     R"IFJ(extension (value: Int)
  def plus(other: Int): Int = value + other

@main def main(): Unit =
  val x = 6
  val w = x.plus("a")
)IFJ",
     5},
    // §5.9 ("the receiver has the form of a parameter"), §6 (7) (A7-093).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"ReceiverIsImmutable",
     R"IFJ(extension (value: Int)
  def bump: Int =
    value = 1
    value

@main def main(): Unit =
  print(1)
)IFJ",
     7},
    // §5.9 ("a single explicitly typed parameter") (A7-094). Control:
    // A7-089. Passes vacuously today (RC-PROLOG; RC-EXT-SYNTAX).
    {"TwoReceiversAreError2",
     R"IFJ(extension (a: Int, b: Int)
  def sum: Int = a + b

@main def main(): Unit =
  print(1)
)IFJ",
     2},
    // §5.9 ("at the top level") (A7-095). Control: A7-089.
    // Passes vacuously today (RC-PROLOG; without the prolog p_statement 2).
    {"ExtensionInsideFunctionIsError2",
     R"IFJ(@main def main(): Unit =
  extension (value: Int)
    def twice: Int = value * 2
  print(1)
)IFJ",
     2},
};

TEST_P(ExtExtkwTest, DISABLED_ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Spec, ExtExtkwTest, ::testing::ValuesIn(kExtkwCases),
                         ext_case_name);

} // namespace

/*
 * AI-generated test block — PARAMLISTS (spec 5.10), disabled.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Black-box cases for functions without a parameter list and with several
 *   lists: the spec program, unique names across lists (4), number of
 *   lists and of arguments in any list (5), the empty list cannot be
 *   omitted and a parameterless function takes no parentheses (5), use in
 *   expressions, immutable parameters (7), recursion needs a type (6) and
 *   @main with a parameter in a second list (5).
 *
 * How to enable: once PARAMLISTS is implemented, drop the DISABLED_ prefix
 * from the TEST_P name below.
 */
namespace {

class ExtParamlistsTest : public ::testing::TestWithParam<ExtCase> {};

const ExtCase kParamlistsCases[] = {
    // §5.10 (A7-096): the spec example without its prolog line.
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"ProgramFromSpec",
     R"IFJ(def answer: Int = 42
def answerWithParens(): Int = 42
def add(a: Int)(b: Int)(c: Int): Int = a + b + c
def offset()(value: Int): Int = value + 1

@main def run(): Unit =
  val x = answer
  val y = answerWithParens()
  val sum = add(x)(y)(1)
  val result = offset()(sum)
  print(result)
)IFJ",
     0},
    // §5.10 ("unique across all lists, otherwise 4") (A7-097).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"DuplicateNameAcrossListsIsError4",
     R"IFJ(def add(a: Int)(a: Int): Int = a

@main def main(): Unit =
  print(1)
)IFJ",
     4},
    // §5.10 ("wrong number of lists ... 5") (A7-098).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"MissingArgumentListIsError5",
     R"IFJ(def add(a: Int)(b: Int)(c: Int): Int = a + b + c

@main def main(): Unit =
  val s = add(1)(2)
)IFJ",
     5},
    // §5.10 ("wrong argument count in any list") (A7-099).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"WrongCountInInnerListIsError5",
     R"IFJ(def add(a: Int)(b: Int)(c: Int): Int = a + b + c

@main def main(): Unit =
  val s = add(1)(2, 3)(4)
)IFJ",
     5},
    // §5.10 ("an empty list cannot be omitted") (A7-100).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"OmittedEmptyListIsError5",
     R"IFJ(def offset()(value: Int): Int = value + 1

@main def main(): Unit =
  val s = offset(5)
)IFJ",
     5},
    // §5.10 ("called only by its name without parentheses") (A7-138).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"ParameterlessCalledWithParensIsError5",
     R"IFJ(def answer: Int = 42

@main def main(): Unit =
  val s = answer()
)IFJ",
     5},
    // §5.10 ("def foo(): Int = 42 is still called as foo()") (A7-139): a
    // missing parameter list is the more specific code 5 of §5.10 ("wrong
    // number of lists ... error 5"), not 3.
    // Fails today: RC-PROLOG (2); without the prolog RC-ONELINE (2).
    {"EmptyListCalledWithoutParensIsError5",
     R"IFJ(def answerWithParens(): Int = 42

@main def main(): Unit =
  val s = answerWithParens
)IFJ",
     5},
    // §5.10 ("parameterless functions may be part of an expression even
    // without FUNEXP") (A7-140).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"ParameterlessCallInsideExpression",
     R"IFJ(def answer: Int = 42

@main def main(): Unit =
  val z = answer + 1
  print(z)
)IFJ",
     0},
    // §5.10 ("all parameters are immutable"), §6 (7) (A7-141).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"SecondListParameterIsImmutable",
     R"IFJ(def f(a: Int)(b: Int): Int =
  b = 1
  a

@main def main(): Unit =
  print(1)
)IFJ",
     7},
    // §5.10 ("rules for recursion stay the same"), §2.3 (A7-142).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"RecursiveParameterlessNeedsTypeIsError6",
     R"IFJ(def count = count

@main def main(): Unit =
  print(1)
)IFJ",
     6},
    // §5.10, §2 ("the entry function has no parameter" -> 5) (A7-143).
    // Fails today: RC-PROLOG (2); without the prolog RC-EXT-SYNTAX (2).
    {"MainWithSecondListParameterIsError5",
     R"IFJ(@main def run()(x: Int): Unit =
  print(x)
)IFJ",
     5},
};

TEST_P(ExtParamlistsTest, DISABLED_ExitCodeMatchesSpec) {
  expect_spec_exit_code(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Spec, ExtParamlistsTest,
                         ::testing::ValuesIn(kParamlistsCases), ext_case_name);

} // namespace

/*
 * AI-generated test block — lexer tokens of the extensions, active.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   The lexer already knows the tokens of every extension. Lex typical
 *   extension lines (member chains, match and case class, interpolated
 *   strings, lambdas and function types, lists and ranges, extension and
 *   parameter list headers) and check the token kinds, and check that the
 *   member and builtin names of the extensions stay plain identifiers.
 *
 * In the base language `|`, `[` and `]` are unknown characters (spec 1.1,
 * error 1). These rows pin the team's decision to lex the tokens of every
 * extension in every profile, like the existing lexer tests do.
 */
namespace {

/** An expected token; @c text is checked only when it is not NULL. */
struct Tok {
  TokenType type;
  const char *text;
};
Tok T(TokenType type, const char *text = nullptr) { return {type, text}; }
Tok Id(const char *text) { return T(TOKEN_IDENTIFIER, text); }

/** One source line with the tokens and final status the lexer must give. */
struct LexLine {
  const char *input;
  std::vector<Tok> tokens;
  LexerStatus status;
};

struct LexCase {
  const char *name;
  std::vector<LexLine> lines;
};

/** Prints a row as its name in gtest failure messages. */
void PrintTo(const LexCase &row, std::ostream *os) { *os << row.name; }

std::string lex_case_name(const ::testing::TestParamInfo<LexCase> &info) {
  return info.param.name;
}

/** Tokens of @p input up to the first non-OK status. */
struct Lexed {
  std::vector<TokenType> types;
  std::vector<std::string> texts;
  LexerStatus status = LEXER_INVALID_ARGUMENT;
};

bool owns_text(TokenType type) {
  return type == TOKEN_IDENTIFIER || type == TOKEN_STRING;
}

Lexed lex_line(const std::string &input) {
  Lexed out;
  std::vector<char> buffer(input.c_str(), input.c_str() + input.size() + 1);
  Lexer lexer;
  if (lexer_init(&lexer, buffer.data()) == LEXER_OK) {
    while (out.types.size() < 1000U &&
           (out.status = lexer_pls(&lexer)) == LEXER_OK) {
      out.types.push_back(lexer.token.type);
      out.texts.push_back(
          owns_text(lexer.token.type) ? lexer.token.value.string_value : "");
    }
    lexer_destroy(&lexer);
  }
  return out;
}

void expect_line(const LexLine &line) {
  // Act
  Lexed got = lex_line(line.input);

  // Assert
  EXPECT_EQ(got.status, line.status);
  ASSERT_EQ(got.types.size(), line.tokens.size());
  for (size_t i = 0U; i < line.tokens.size(); i += 1U) {
    SCOPED_TRACE(i);
    EXPECT_EQ(got.types[i], line.tokens[i].type);
    if (line.tokens[i].text != nullptr) {
      EXPECT_EQ(got.texts[i], line.tokens[i].text);
    }
  }
}

class ExtLexerTest : public ::testing::TestWithParam<LexCase> {};

const Tok kLp = T(TOKEN_LEFT_PAREN), kRp = T(TOKEN_RIGHT_PAREN),
          kDot = T(TOKEN_DOT), kColon = T(TOKEN_COLON), kComma = T(TOKEN_COMMA),
          kArrow = T(TOKEN_ARROW), kInt = T(TOKEN_KW_INT), kNum = T(TOKEN_INT),
          kReal = T(TOKEN_DOUBLE), kAssign = T(TOKEN_ASSIGN),
          kList = T(TOKEN_KW_LIST), kLb = T(TOKEN_LEFT_BRACKET),
          kRb = T(TOKEN_RIGHT_BRACKET);
constexpr LexerStatus kEof = LEXER_EOF;

const LexCase kExtLexerCases[] = {
    // §5.4 (A7-132): member chains are DOT and identifiers after any
    // receiver; `5 .toString` with a space is INT DOT ID.
    {"MemberChainsLexAsDotAndIdentifier",
     {{"n.floor.toInt", {Id("n"), kDot, Id("floor"), kDot, Id("toInt")}, kEof},
      {"\"abc\".length", {T(TOKEN_STRING), kDot, Id("length")}, kEof},
      {"(1.2345).toString", {kLp, kReal, kRp, kDot, Id("toString")}, kEof},
      {"1.5.toString", {kReal, kDot, Id("toString")}, kEof},
      {"1.5d.toString", {kReal, kDot, Id("toString")}, kEof},
      {"1e3.toInt", {kReal, kDot, Id("toInt")}, kEof},
      {"5 .toString", {kNum, kDot, Id("toString")}, kEof}}},
    // §5.5 (A7-133): match, case class and union type lines (a spaced
    // union alias is also in lexer_edge_test UnionTypeAlias; here `Int|Unit`
    // without spaces).
    {"MatchAndCaseClassLines",
     {{"case Point(a, b) => f(a, b)",
       {T(TOKEN_KW_CASE), Id("Point"), kLp, Id("a"), kComma, Id("b"), kRp,
        kArrow, Id("f"), kLp, Id("a"), kComma, Id("b"), kRp},
       kEof},
      {"case n: Int if n > 0 => \"p\"",
       {T(TOKEN_KW_CASE), Id("n"), kColon, kInt, T(TOKEN_KW_IF), Id("n"),
        T(TOKEN_GREATER), kNum, kArrow, T(TOKEN_STRING)},
       kEof},
      {"case class Point(x: Int, y: Int)",
       {T(TOKEN_KW_CASE), T(TOKEN_KW_CLASS), Id("Point"), kLp, Id("x"), kColon,
        kInt, kComma, Id("y"), kColon, kInt, kRp},
       kEof},
      {"value match", {Id("value"), T(TOKEN_KW_MATCH)}, kEof},
      {"Int|Unit", {kInt, T(TOKEN_UNION), T(TOKEN_KW_UNIT)}, kEof}}},
    // §5.6, §1.1 (A7-134): s"..." is the identifier s and a string with a
    // literal $; `\$` is no escape sequence -> lexical error (1) (a plain
    // string with `\$` is also in lexer_edge_test EscapeDollar).
    {"InterpolatedStringLexemes",
     {{"s\"\"\"He said \"hello\", $name.\"\"\"",
       {Id("s"), T(TOKEN_STRING, "\"\"\"He said \"hello\", $name.\"\"\"")},
       kEof},
      {"s\"a\\\"b $n\"", {Id("s"), T(TOKEN_STRING, "\"a\\\"b $n\"")}, kEof},
      {"s\"a\\$b\"", {Id("s")}, LEXER_INVALID_TOKEN}}},
    // §5.7 (A7-135): function types and lambdas.
    {"LambdaAndFunctionTypeLines",
     {{"val f: (Int, Int) => Int = (a: Int, b: Int) => a + b",
       {T(TOKEN_KW_VAL), Id("f"), kColon,        kLp,    kInt,
        kComma,          kInt,    kRp,           kArrow, kInt,
        kAssign,         kLp,     Id("a"),       kColon, kInt,
        kComma,          Id("b"), kColon,        kInt,   kRp,
        kArrow,          Id("a"), T(TOKEN_PLUS), Id("b")},
       kEof},
      {"() => 42", {kLp, kRp, kArrow, kNum}, kEof},
      {"(a:Int)=>a",
       {kLp, Id("a"), kColon, kInt, kRp, kArrow, Id("a")},
       kEof}}},
    // §5.8, §5.3 (A7-136): lists, ++, List[T], ranges and the generator;
    // to/until/by stay identifiers. `List[Int]` and `for i <- 1 to 10` are
    // also in lexer_edge_test (ListType, ForGenerator); here the nested
    // type, ++ and ranges as operands. The generator is written with
    // spaces: glued `x<-1` is `x < -1` in the base language.
    {"ListAndRangeLines",
     {{"List.range(1, 10)",
       {kList, kDot, Id("range"), kLp, kNum, kComma, kNum, kRp},
       kEof},
      {"a ++ List(1)",
       {Id("a"), T(TOKEN_LIST_CONCAT), kList, kLp, kNum, kRp},
       kEof},
      {"List[List[Int]]", {kList, kLb, kList, kLb, kInt, kRb, kRb}, kEof},
      {"(1 to 5).toList",
       {kLp, kNum, Id("to"), kNum, kRp, kDot, Id("toList")},
       kEof},
      {"10 to 1 by -1",
       {kNum, Id("to"), kNum, Id("by"), T(TOKEN_MINUS), kNum},
       kEof},
      {"i <- 0 until n",
       {Id("i"), T(TOKEN_FOR_GENERATOR), kNum, Id("until"), Id("n")},
       kEof}}},
    // §5.9, §5.10 (A7-137): extension headers and several parameter lists.
    {"ExtensionAndParamListHeaders",
     {{"extension (value: Int)",
       {T(TOKEN_KW_EXTENSION), kLp, Id("value"), kColon, kInt, kRp},
       kEof},
      {"def twice: Int = value * 2",
       {T(TOKEN_KW_DEF), Id("twice"), kColon, kInt, kAssign, Id("value"),
        T(TOKEN_MULTIPLY), kNum},
       kEof},
      {"def add(a: Int)(b: Int): Int",
       {T(TOKEN_KW_DEF), Id("add"), kLp, Id("a"), kColon, kInt, kRp, kLp,
        Id("b"), kColon, kInt, kRp, kColon, kInt},
       kEof},
      {"offset()(sum)", {Id("offset"), kLp, kRp, kLp, Id("sum"), kRp}, kEof}}},
};

TEST_P(ExtLexerTest, TokensMatchSpec) {
  for (const LexLine &line : GetParam().lines) {
    SCOPED_TRACE(line.input);
    expect_line(line);
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, ExtLexerTest,
                         ::testing::ValuesIn(kExtLexerCases), lex_case_name);

// §1.1, §5 (A7-144): builtin and member names of the extensions are not
// keywords: keyword_lookup finds nothing and the lexer gives identifiers.
TEST(ExtKeywordTest, ExtensionMemberNamesAreIdentifiers) {
  // Arrange
  const char *names[] = {"readBoolean", "readIntMaybe", "readDoubleMaybe",
                         "toString",    "length",       "floor",
                         "toInt",       "head",         "tail",
                         "drop",        "take",         "slice",
                         "range",       "toList",       "filter",
                         "takeWhile",   "dropWhile",    "yield"};

  for (const char *name : names) {
    SCOPED_TRACE(name);
    // Act
    const Keyword *keyword = keyword_lookup(name);
    Lexed got = lex_line(name);

    // Assert
    EXPECT_EQ(keyword, nullptr);
    EXPECT_EQ(got.status, LEXER_EOF);
    ASSERT_EQ(got.types.size(), 1U);
    EXPECT_EQ(got.types[0], TOKEN_IDENTIFIER);
    EXPECT_EQ(got.texts[0], name);
  }
}

} // namespace
