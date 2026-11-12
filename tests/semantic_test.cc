#include "gtest/gtest.h"

extern "C" {
#include "../src/ast.h"
#include "../src/function_table.h"
#include "../src/parser.h"
#include "../src/semantic.h"
#include "../src/symtable.h"
}

#include <initializer_list>

namespace {

/** Builds programs by hand (the expression parser is not finished yet). */
class SemanticTest : public ::testing::Test {
protected:
  Program program{};

  void SetUp() override { ASSERT_EQ(program_init(&program), PARSER_OK); }
  void TearDown() override { program_destroy(&program); }

  /// Defines `def name(params...)[: ret] =` with an empty body block.
  FunctionSymbol *def(const char *name, DataType explicit_ret = TYPE_UNKNOWN,
                      std::initializer_list<DataType> params = {}) {
    FunctionSymbol *f = ref(name);
    f->is_defined = true;
    function_symbol_create_local_scope(f);
    for (DataType p : params) {
      function_symbol_add_parameter(f, p);
    }
    if (explicit_ret != TYPE_UNKNOWN) {
      f->return_type.type = explicit_ret;
      f->has_explicit_return_type = true;
    }
    f->ast_root = ast_node_create_block(1, 1);
    return f;
  }

  FunctionSymbol *def_main() {
    FunctionSymbol *f = def("main", TYPE_UNIT);
    f->is_main = true;
    return f;
  }

  /// A (possibly forward / undefined) function reference.
  FunctionSymbol *ref(const char *name) {
    FunctionSymbol *f = nullptr;
    function_table_get_or_create(&program.functions, name, &f);
    return f;
  }

  Symbol *param(FunctionSymbol *f, const char *name, DataType type) {
    Symbol *s = nullptr;
    symbol_table_define(f->local_scope, name, type, true, true, &s);
    return s;
  }

  Symbol *var(FunctionSymbol *f, const char *name, DataType annotated,
              bool constant) {
    Symbol *s = nullptr;
    symbol_table_define(f->local_scope, name, annotated, true, constant, &s);
    return s;
  }

  static void add(FunctionSymbol *f, ASTNode *statement) {
    ASSERT_EQ(ast_node_block_append_statement(f->ast_root, statement), AST_OK);
  }

  static ASTNode *call(FunctionSymbol *callee,
                       std::initializer_list<ASTNode *> args = {}) {
    ASTNode *c = ast_node_create_function_call(callee, 1, 1);
    for (ASTNode *a : args) {
      ast_node_call_append_argument(c, a);
    }
    return c;
  }

  static ASTNode *integer(long long v) { return ast_node_create_int(v, 1, 1); }
  static ASTNode *real(double v) { return ast_node_create_real(v, 1, 1); }
  static ASTNode *str(const char *v) { return ast_node_create_string(v, 1, 1); }
  static ASTNode *ref_var(Symbol *s) {
    return ast_node_create_variable_ref(s, 1, 1);
  }
  static ASTNode *define(Symbol *s, ASTNode *value) {
    ASTNode *n = ast_node_create_assign(s, value, 1, 1);
    n->data.assignment.is_definition = true;
    return n;
  }
  static ASTNode *assign(Symbol *s, ASTNode *value) {
    return ast_node_create_assign(s, value, 1, 1);
  }
  static ASTNode *ret(ASTNode *value) {
    return ast_node_create_return(value, 1, 1);
  }
  static ASTNode *bin(ASTNodeKind kind, ASTNode *l, ASTNode *r) {
    return ast_node_create_binary_op(kind, l, r, 1, 1);
  }
  static ASTNode *block(std::initializer_list<ASTNode *> statements) {
    ASTNode *b = ast_node_create_block(1, 1);
    for (ASTNode *s : statements) {
      ast_node_block_append_statement(b, s);
    }
    return b;
  }
  static ASTNode *if_(ASTNode *cond, ASTNode *then_b, ASTNode *else_b) {
    return ast_node_create_if(cond, then_b, else_b, 1, 1);
  }

  SemanticStatus check() { return semantic_check(&program); }
};

} // namespace

TEST_F(SemanticTest, InfersReturnTypeOfForwardCalledFunction) {
  // @main def main(): Unit =
  //   val x: Double = g()      // g defined later, Int widened to Double
  // def g() = 1
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", TYPE_REAL, true);
  add(m, define(x, call(ref("g"))));
  FunctionSymbol *g = def("g");
  add(g, integer(1));

  ASSERT_EQ(check(), SEMANTIC_OK);
  EXPECT_EQ(g->return_type.type, TYPE_INT);
  ASTNode *init = m->ast_root->data.block.statements[0]->data.assignment.value;
  EXPECT_EQ(init->kind, AST_IMPLICIT_INT_TO_REAL);
}

TEST_F(SemanticTest, ResolvesChainInTopologicalOrder) {
  // main -> a -> b -> c, none of a, b, c annotated; c yields String.
  FunctionSymbol *m = def_main();
  add(m, call(ref("a")));
  FunctionSymbol *a = def("a");
  add(a, call(ref("b")));
  FunctionSymbol *b = def("b");
  Symbol *v = var(b, "v", TYPE_UNKNOWN, true);
  add(b, define(v, call(ref("c"))));
  add(b, ref_var(v));
  FunctionSymbol *c = def("c");
  add(c, str("hi"));

  ASSERT_EQ(check(), SEMANTIC_OK);
  EXPECT_EQ(c->return_type.type, TYPE_STRING);
  EXPECT_EQ(v->type.type, TYPE_STRING);
  EXPECT_EQ(b->return_type.type, TYPE_STRING);
  EXPECT_EQ(a->return_type.type, TYPE_STRING);
}

TEST_F(SemanticTest, DirectRecursionWithoutReturnTypeIsError6) {
  // def f(n: Int) =
  //   f(n)
  //   n
  def_main();
  FunctionSymbol *f = def("f", TYPE_UNKNOWN, {TYPE_INT});
  Symbol *n = param(f, "n", TYPE_INT);
  add(f, call(f, {ref_var(n)}));
  add(f, ref_var(n));

  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticTest, DirectRecursionWithReturnTypeIsOk) {
  def_main();
  FunctionSymbol *f = def("f", TYPE_INT, {TYPE_INT});
  Symbol *n = param(f, "n", TYPE_INT);
  Symbol *r = var(f, "r", TYPE_UNKNOWN, true);
  add(f, define(r, call(f, {ref_var(n)})));
  add(f, bin(AST_ADD, ref_var(r), integer(1)));

  ASSERT_EQ(check(), SEMANTIC_OK);
  EXPECT_EQ(r->type.type, TYPE_INT);
}

TEST_F(SemanticTest, IndirectRecursionThroughAnnotatedFunctionsIsError6) {
  // a: Int -> b, b: Int -> a, a -> c, c (no type) -> b.
  // c lies on the cycle a -> c -> b -> a, a naive single-DFS "gray node"
  // check finds only the a<->b cycle and misses c.
  def_main();
  FunctionSymbol *a = def("a", TYPE_INT);
  FunctionSymbol *b = def("b", TYPE_INT);
  FunctionSymbol *c = def("c");
  add(a, call(b));
  add(a, call(c));
  add(a, integer(1));
  add(b, call(a));
  add(c, call(b));

  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticTest, MutualRecursionWithAllAnnotatedIsOk) {
  def_main();
  FunctionSymbol *a = def("a", TYPE_INT);
  FunctionSymbol *b = def("b", TYPE_INT);
  FunctionSymbol *c = def("c", TYPE_INT);
  add(a, call(b));
  add(a, call(c));
  add(b, call(a));
  add(c, call(b));

  EXPECT_EQ(check(), SEMANTIC_OK);
}

TEST_F(SemanticTest, ReturnWithoutReturnTypeIsError6) {
  def_main();
  FunctionSymbol *f = def("f");
  add(f, ret(integer(1)));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticTest, UndefinedFunctionIsError3) {
  FunctionSymbol *m = def_main();
  add(m, call(ref("nope")));
  EXPECT_EQ(check(), SEMANTIC_UNDEFINED);
}

TEST_F(SemanticTest, MissingMainIsError3) {
  def("f");
  add(ref("f"), integer(1));
  EXPECT_EQ(check(), SEMANTIC_UNDEFINED);
}

TEST_F(SemanticTest, WrongArgumentCountOfForwardCallIsError5) {
  FunctionSymbol *m = def_main();
  add(m, call(ref("g"), {integer(1)}));
  FunctionSymbol *g = def("g", TYPE_UNKNOWN, {TYPE_INT, TYPE_INT});
  add(g, integer(1));
  EXPECT_EQ(check(), SEMANTIC_BAD_CALL);
}

TEST_F(SemanticTest, WrongArgumentTypeIsError5) {
  FunctionSymbol *m = def_main();
  add(m, call(ref("g"), {real(1.5)}));
  FunctionSymbol *g = def("g", TYPE_UNKNOWN, {TYPE_INT});
  add(g, integer(1));
  EXPECT_EQ(check(), SEMANTIC_BAD_CALL);
}

TEST_F(SemanticTest, BuiltinsHaveSignatures) {
  FunctionSymbol *m = def_main();
  Symbol *n = var(m, "n", TYPE_UNKNOWN, true);
  add(m, define(n, call(ref("readInt"))));
  add(m, call(ref("print"), {ref_var(n)}));
  ASSERT_EQ(check(), SEMANTIC_OK);
  EXPECT_EQ(n->type.type, TYPE_INT);
}

TEST_F(SemanticTest, BuiltinWithWrongArgumentIsError5) {
  FunctionSymbol *m = def_main();
  add(m, call(ref("strLength"), {integer(1)}));
  EXPECT_EQ(check(), SEMANTIC_BAD_CALL);
}

TEST_F(SemanticTest, AssignmentToValIsError7) {
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", TYPE_UNKNOWN, true);
  add(m, define(x, integer(1)));
  add(m, assign(x, integer(2)));
  EXPECT_EQ(check(), SEMANTIC_OTHER);
}

TEST_F(SemanticTest, AssignmentOfOtherTypeIsError6) {
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", TYPE_UNKNOWN, false);
  add(m, define(x, integer(1)));
  add(m, assign(x, str("a")));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticTest, DoubleToIntIsError6) {
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", TYPE_INT, true);
  add(m, define(x, real(1.5)));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticTest, MixedArithmeticWidensToDouble) {
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", TYPE_UNKNOWN, true);
  add(m, define(x, bin(AST_MULTIPLY, integer(2), real(1.5))));
  ASSERT_EQ(check(), SEMANTIC_OK);
  EXPECT_EQ(x->type.type, TYPE_REAL);
}

TEST_F(SemanticTest, StringPlusIntIsError6) {
  FunctionSymbol *m = def_main();
  add(m, bin(AST_ADD, str("a"), integer(1)));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticTest, NonBooleanConditionIsError6) {
  FunctionSymbol *m = def_main();
  add(m, if_(integer(1), block({integer(1)}), block({integer(2)})));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticTest, IfAsLastStatementWithValuesIsError6) {
  // def f(): Int =
  //   if 1 < 2 then 1 else 2      (if is not an expression)
  def_main();
  FunctionSymbol *f = def("f", TYPE_INT);
  add(f, if_(bin(AST_LESS, integer(1), integer(2)), block({integer(1)}),
             block({integer(2)})));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticTest, IfAsLastStatementWithReturnsIsOk) {
  def_main();
  FunctionSymbol *f = def("f", TYPE_REAL);
  add(f, if_(bin(AST_LESS, integer(1), real(2.0)), block({ret(integer(1))}),
             block({ret(real(2.0))})));
  EXPECT_EQ(check(), SEMANTIC_OK);
}

TEST_F(SemanticTest, IfWithUnitBranchesInfersUnit) {
  def_main();
  FunctionSymbol *f = def("f");
  add(f, if_(bin(AST_EQUAL, str("a"), str("b")),
             block({call(ref("print"), {str("x")})}),
             block({ast_node_create_unit(1, 1)})));
  ASSERT_EQ(check(), SEMANTIC_OK);
  EXPECT_EQ(f->return_type.type, TYPE_UNIT);
}

TEST_F(SemanticTest, MainReturningIntIsError6) {
  FunctionSymbol *m = ref("main");
  m->is_defined = true;
  m->is_main = true;
  function_symbol_create_local_scope(m);
  m->ast_root = ast_node_create_block(1, 1);
  add(m, integer(1));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticTest, RedefinitionOfBuiltinIsError4) {
  // def print(x: Int) = x
  def_main();
  FunctionSymbol *p = def("print", TYPE_UNKNOWN, {TYPE_INT});
  Symbol *x = param(p, "x", TYPE_INT);
  add(p, ref_var(x));
  EXPECT_EQ(check(), SEMANTIC_REDEFINITION);
}
