#include "gtest/gtest.h"
#include <string>

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
  FunctionSymbol *def(const char *name, BaseType explicit_ret = TYPE_UNKNOWN,
                      std::initializer_list<BaseType> params = {}) {
    FunctionSymbol *f = ref(name);
    f->is_defined = true;
    SymbolTable *scope = function_symbol_create_local_scope(f);
    int index = 0;
    for (BaseType p : params) {
      std::string name = "p" + std::to_string(index++);
      Symbol *symbol = nullptr;
      symbol_table_define(scope, name.c_str(), type_scalar(p), true, true,
                          &symbol);
      function_symbol_add_parameter(f, symbol);
    }
    if (explicit_ret != TYPE_UNKNOWN) {
      f->return_type.type = type_scalar(explicit_ret);
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

  Symbol *param(FunctionSymbol *f, const char *name, BaseType type) {
    Symbol *s = nullptr;
    symbol_table_define(f->local_scope, name, type_scalar(type), true, true,
                        &s);
    return s;
  }

  Symbol *var(FunctionSymbol *f, const char *name, BaseType annotated,
              bool constant) {
    Symbol *s = nullptr;
    symbol_table_define(f->local_scope, name, type_scalar(annotated), true,
                        constant, &s);
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
  EXPECT_TRUE(type_is_scalar(g->return_type.type, TYPE_INT));
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
  EXPECT_TRUE(type_is_scalar(c->return_type.type, TYPE_STRING));
  EXPECT_TRUE(type_is_scalar(v->type.type, TYPE_STRING));
  EXPECT_TRUE(type_is_scalar(b->return_type.type, TYPE_STRING));
  EXPECT_TRUE(type_is_scalar(a->return_type.type, TYPE_STRING));
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
  EXPECT_TRUE(type_is_scalar(r->type.type, TYPE_INT));
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

TEST_F(SemanticTest, BareReturnInUnitFunctionIsOk) {
  def_main();
  FunctionSymbol *f = def("f", TYPE_UNIT);
  ASTNode *r = ret(nullptr);
  add(f, r);
  EXPECT_EQ(check(), SEMANTIC_OK);
  /// The semantic pass attaches an implicit Unit constant.
  ASSERT_NE(r->data.return_statement.value, nullptr);
  EXPECT_EQ(r->data.return_statement.value->kind, AST_CONSTANT_UNIT);
}

TEST_F(SemanticTest, BareReturnInIntFunctionIsError6) {
  def_main();
  FunctionSymbol *f = def("f", TYPE_INT);
  add(f, ret(nullptr));
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
  EXPECT_TRUE(type_is_scalar(n->type.type, TYPE_INT));
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
  EXPECT_TRUE(type_is_scalar(x->type.type, TYPE_REAL));
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

TEST_F(SemanticTest, IfAsLastStatementYieldsCommonValue) {
  // def f() =
  //   if 1 < 2 then 1 else 2.5    (if is an expression with CONTROL)
  def_main();
  FunctionSymbol *f = def("f");
  add(f, if_(bin(AST_LESS, integer(1), integer(2)), block({integer(1)}),
             block({real(2.5)})));
  ASSERT_EQ(check(), SEMANTIC_OK);
  EXPECT_TRUE(type_is_scalar(f->return_type.type, TYPE_REAL));
  /// The branch value is widened and then wrapped in an implicit return.
  ASTNode *then_tail =
      f->ast_root->data.block.statements[0]
          ->data.if_statement.then_branch->data.block.statements[0];
  ASSERT_EQ(then_tail->kind, AST_RETURN);
  EXPECT_EQ(then_tail->data.return_statement.value->kind,
            AST_IMPLICIT_INT_TO_REAL);
}

TEST_F(SemanticTest, IfWithIncompatibleBranchValuesIsError6) {
  // def f(): Int =
  //   if 1 < 2 then 1 else "a"
  def_main();
  FunctionSymbol *f = def("f", TYPE_INT);
  add(f, if_(bin(AST_LESS, integer(1), integer(2)), block({integer(1)}),
             block({str("a")})));
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
  EXPECT_TRUE(type_is_scalar(f->return_type.type, TYPE_UNIT));
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

TEST_F(SemanticTest, BooleanValuesCanBeStoredAndCombined) {
  // val b = readBoolean() && 1 < 2;  val c = !b
  FunctionSymbol *m = def_main();
  Symbol *b = var(m, "b", TYPE_UNKNOWN, true);
  Symbol *c = var(m, "c", TYPE_UNKNOWN, true);
  add(m, define(b, bin(AST_AND, call(ref("readBoolean")),
                       bin(AST_LESS, integer(1), integer(2)))));
  add(m, define(c, ast_node_create_not(ref_var(b), 1, 1)));
  ASSERT_EQ(check(), SEMANTIC_OK);
  EXPECT_TRUE(type_is_scalar(b->type.type, TYPE_BOOL));
  EXPECT_TRUE(type_is_scalar(c->type.type, TYPE_BOOL));
}

TEST_F(SemanticTest, ChainedRelationalOperatorsAreError6) {
  // 1 < 2 < 3
  FunctionSymbol *m = def_main();
  add(m, bin(AST_LESS, bin(AST_LESS, integer(1), integer(2)), integer(3)));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticTest, ListLiteralWidensElementsAndIndexes) {
  // val l = List(1, 2.5);  val x = l(0)
  FunctionSymbol *m = def_main();
  Symbol *l = var(m, "l", TYPE_UNKNOWN, true);
  Symbol *x = var(m, "x", TYPE_UNKNOWN, true);
  ASTNode *list = ast_node_create_list_constructor(1, 1);
  ast_node_call_append_argument(list, integer(1));
  ast_node_call_append_argument(list, real(2.5));
  add(m, define(l, list));
  add(m,
      define(x, ast_node_create_list_indexing(ref_var(l), integer(0), 1, 1)));
  ASSERT_EQ(check(), SEMANTIC_OK);
  EXPECT_TRUE(type_equal(l->type.type, type_list(type_scalar(TYPE_REAL))));
  EXPECT_TRUE(type_is_scalar(x->type.type, TYPE_REAL));
}

TEST_F(SemanticTest, EmptyListNeedsExpectedType) {
  // val e = List()       -> 6
  FunctionSymbol *m = def_main();
  Symbol *e = var(m, "e", TYPE_UNKNOWN, true);
  add(m, define(e, ast_node_create_list_constructor(1, 1)));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticTest, EmptyListTakesAnnotatedType) {
  // val e: List[Int] = List()
  FunctionSymbol *m = def_main();
  Symbol *e = nullptr;
  symbol_table_define(m->local_scope, "e", type_list(type_scalar(TYPE_INT)),
                      true, true, &e);
  add(m, define(e, ast_node_create_list_constructor(1, 1)));
  EXPECT_EQ(check(), SEMANTIC_OK);
}

TEST_F(SemanticTest, EmptyListPropagatesThroughListPreservingBuiltins) {
  // val e: List[String] = List().drop(1).tail().take(2).slice(0, 1)
  FunctionSymbol *m = def_main();
  Symbol *e = nullptr;
  symbol_table_define(m->local_scope, "e", type_list(type_scalar(TYPE_STRING)),
                      true, true, &e);

  ASTNode *value = call(ref("list$drop"),
                        {ast_node_create_list_constructor(1, 1), integer(1)});
  value = call(ref("list$tail"), {value});
  value = call(ref("list$take"), {value, integer(2)});
  value = call(ref("list$slice"), {value, integer(0), integer(1)});
  add(m, define(e, value));

  ASSERT_EQ(check(), SEMANTIC_OK);
  EXPECT_TRUE(type_equal(value->inferred_type.type,
                         type_list(type_scalar(TYPE_STRING))));
}

namespace {
/// Last statement of a block.
ASTNode *last(ASTNode *block) {
  return block->data.block.statements[block->data.block.statement_count - 1U];
}
/// True for `return ()`.
bool is_unit_return(ASTNode *node) {
  return node->kind == AST_RETURN &&
         node->data.return_statement.value != nullptr &&
         node->data.return_statement.value->kind == AST_CONSTANT_UNIT;
}
} // namespace

TEST_F(SemanticTest, EmptyUnitBodyGetsImplicitUnitReturn) {
  FunctionSymbol *m = def_main();
  ASSERT_EQ(check(), SEMANTIC_OK);
  ASSERT_EQ(m->ast_root->data.block.statement_count, 1U);
  EXPECT_TRUE(is_unit_return(last(m->ast_root)));
}

TEST_F(SemanticTest, UnitBodyKeepsLastStatementAndReturnsUnit) {
  // @main def main(): Unit =
  //   val x: Int = 0         -> followed by an implicit `return ()`
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", TYPE_INT, true);
  add(m, define(x, integer(0)));

  ASSERT_EQ(check(), SEMANTIC_OK);
  ASSERT_EQ(m->ast_root->data.block.statement_count, 2U);
  EXPECT_EQ(m->ast_root->data.block.statements[0]->kind, AST_ASSIGN);
  EXPECT_TRUE(is_unit_return(last(m->ast_root)));
}

TEST_F(SemanticTest, IntBodyTailValueIsWrappedInReturn) {
  def_main();
  FunctionSymbol *f = def("f", TYPE_INT);
  add(f, integer(7));

  ASSERT_EQ(check(), SEMANTIC_OK);
  ASSERT_EQ(f->ast_root->data.block.statement_count, 1U);
  ASTNode *r = last(f->ast_root);
  ASSERT_EQ(r->kind, AST_RETURN);
  EXPECT_EQ(r->data.return_statement.value->kind, AST_CONSTANT_INT);
  EXPECT_TRUE(type_is_scalar(r->inferred_type.type, TYPE_INT));
}

TEST_F(SemanticTest, WidenedTailValueIsWrappedInReturn) {
  def_main();
  FunctionSymbol *f = def("f", TYPE_REAL);
  add(f, integer(7));

  ASSERT_EQ(check(), SEMANTIC_OK);
  ASTNode *r = last(f->ast_root);
  ASSERT_EQ(r->kind, AST_RETURN);
  EXPECT_EQ(r->data.return_statement.value->kind, AST_IMPLICIT_INT_TO_REAL);
}

TEST_F(SemanticTest, InferredBodyTailValueIsWrappedInReturn) {
  def_main();
  FunctionSymbol *g = def("g");
  add(g, integer(2));

  ASSERT_EQ(check(), SEMANTIC_OK);
  ASTNode *r = last(g->ast_root);
  ASSERT_EQ(r->kind, AST_RETURN);
  EXPECT_EQ(r->data.return_statement.value->kind, AST_CONSTANT_INT);
}

TEST_F(SemanticTest, ExplicitTailReturnIsNotDuplicated) {
  def_main();
  FunctionSymbol *f = def("f", TYPE_INT);
  add(f, ret(integer(1)));

  ASSERT_EQ(check(), SEMANTIC_OK);
  ASSERT_EQ(f->ast_root->data.block.statement_count, 1U);
  EXPECT_EQ(last(f->ast_root)->data.return_statement.value->kind,
            AST_CONSTANT_INT);
}

TEST_F(SemanticTest, TailIfStatementGetsReturnInEachBranch) {
  // def f(): Int =
  //   if 1 < 2 then { 1 } else { return 2 }
  def_main();
  FunctionSymbol *f = def("f", TYPE_INT);
  add(f, if_(bin(AST_LESS, integer(1), integer(2)), block({integer(1)}),
             block({ret(integer(2))})));

  ASSERT_EQ(check(), SEMANTIC_OK);
  ASTNode *i = last(f->ast_root);
  ASSERT_EQ(i->kind, AST_IF);
  ASTNode *then_tail = last(i->data.if_statement.then_branch);
  ASSERT_EQ(then_tail->kind, AST_RETURN);
  EXPECT_EQ(then_tail->data.return_statement.value->kind, AST_CONSTANT_INT);
  ASSERT_EQ(i->data.if_statement.else_branch->data.block.statement_count, 1U);
}

TEST_F(SemanticTest, TailIfStatementInUnitFunctionReturnsUnitInBranches) {
  // A non-block branch is wrapped in a block together with `return ()`.
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", TYPE_INT, false);
  add(m, if_(bin(AST_LESS, integer(1), integer(2)), assign(x, integer(1)),
             block({assign(x, integer(2))})));

  ASSERT_EQ(check(), SEMANTIC_OK);
  ASTNode *i = last(m->ast_root);
  ASSERT_EQ(i->kind, AST_IF);
  ASTNode *then_b = i->data.if_statement.then_branch;
  ASSERT_EQ(then_b->kind, AST_BLOCK);
  ASSERT_EQ(then_b->data.block.statement_count, 2U);
  EXPECT_EQ(then_b->data.block.statements[0]->kind, AST_ASSIGN);
  EXPECT_TRUE(is_unit_return(last(then_b)));
  EXPECT_TRUE(is_unit_return(last(i->data.if_statement.else_branch)));
}

TEST_F(SemanticTest, TrailingIfWithoutElseInUnitFunctionIsOk) {
  // def k(): Unit =
  //   if 1 < 2 then { val y: Int = 1 }      (BOOLTHEN, no else)
  def_main();
  FunctionSymbol *k = def("k", TYPE_UNIT);
  Symbol *y = var(k, "y", TYPE_INT, true);
  add(k, if_(bin(AST_LESS, integer(1), integer(2)),
             block({define(y, integer(1))}), nullptr));

  ASSERT_EQ(check(), SEMANTIC_OK);
  ASSERT_EQ(k->ast_root->data.block.statement_count, 2U);
  EXPECT_EQ(k->ast_root->data.block.statements[0]->kind, AST_IF);
  EXPECT_TRUE(is_unit_return(last(k->ast_root)));
}

TEST_F(SemanticTest, TrailingIfWithoutElseInInferredFunctionIsUnit) {
  // def k() =
  //   if 1 < 2 then { print(1) }   -> both branches (), inferred Unit
  def_main();
  FunctionSymbol *k = def("k");
  add(k, if_(bin(AST_LESS, integer(1), integer(2)),
             block({call(ref("print"), {integer(1)})}), nullptr));

  ASSERT_EQ(check(), SEMANTIC_OK);
  EXPECT_TRUE(type_is_scalar(k->return_type.type, TYPE_UNIT));
}

TEST_F(SemanticTest, TrailingIfWithoutElseInIntFunctionIsError6) {
  // def f(): Int =
  //   if 1 < 2 then { 1 }          -> missing else counts as ()
  def_main();
  FunctionSymbol *f = def("f", TYPE_INT);
  add(f,
      if_(bin(AST_LESS, integer(1), integer(2)), block({integer(1)}), nullptr));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticTest, TrailingIfStatementWidensEachBranchToReturnType) {
  // def f(): Double =
  //   if 1 < 2 then { 1 } else { 2 }   -> conversion inside each branch
  def_main();
  FunctionSymbol *f = def("f", TYPE_REAL);
  add(f, if_(bin(AST_LESS, integer(1), integer(2)), block({integer(1)}),
             block({integer(2)})));

  ASSERT_EQ(check(), SEMANTIC_OK);
  ASTNode *i = last(f->ast_root);
  ASSERT_EQ(i->kind, AST_IF);
  for (ASTNode *branch :
       {i->data.if_statement.then_branch, i->data.if_statement.else_branch}) {
    ASTNode *r = last(branch);
    ASSERT_EQ(r->kind, AST_RETURN);
    EXPECT_EQ(r->data.return_statement.value->kind, AST_IMPLICIT_INT_TO_REAL);
  }
}
