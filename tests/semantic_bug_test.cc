#include "gtest/gtest.h"

extern "C" {
#include "../src/ast.h"
#include "../src/function_table.h"
#include "../src/parser.h"
#include "../src/semantic.h"
#include "../src/symtable.h"
}

/*
 * AI-generated test block — semantic spec deviations found by bug hunting.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write GoogleTest cases, with ASTs built by hand like semantic_test.cc,
 *   for semantic_check behaviour that contradicts specifikace-jazyka.pdf:
 *   an annotated variable may read itself in its own initializer
 *   (spec 2.2, error 3), and a Boolean value is accepted as the returned
 *   value or as a discarded statement of a Unit function although without
 *   BOOLTHEN it may only be used directly in an if/while condition
 *   (spec 3, error 6). These tests are expected to fail until fixed.
 */
namespace {

/** Builds the AST shapes the parser produces for small programs. */
class SemanticBugTest : public ::testing::Test {
protected:
  Program program{};

  void SetUp() override { ASSERT_EQ(program_init(&program), PARSER_OK); }
  void TearDown() override { program_destroy(&program); }

  /// Defines `def name()[: ret] =` with an empty body block.
  FunctionSymbol *def(const char *name, BaseType explicit_ret) {
    FunctionSymbol *f = nullptr;
    function_table_get_or_create(&program.functions, name, &f);
    f->is_defined = true;
    function_symbol_create_local_scope(f);
    f->return_type.type = type_scalar(explicit_ret);
    f->has_explicit_return_type = true;
    f->ast_root = ast_node_create_block(1, 1);
    return f;
  }

  FunctionSymbol *def_main() {
    FunctionSymbol *f = def("main", TYPE_UNIT);
    f->is_main = true;
    return f;
  }

  /// Defines a local the way p_variable_init_statement does: before the
  /// initializer is parsed and already marked initialized.
  static Symbol *var(FunctionSymbol *f, const char *name, BaseType annotated) {
    Symbol *s = nullptr;
    symbol_table_define(f->local_scope, name, type_scalar(annotated), true, true,
                        &s);
    return s;
  }

  static void add(FunctionSymbol *f, ASTNode *statement) {
    ASSERT_EQ(ast_node_block_append_statement(f->ast_root, statement), AST_OK);
  }

  static ASTNode *integer(long long v) { return ast_node_create_int(v, 1, 1); }
  static ASTNode *ref_var(Symbol *s) {
    return ast_node_create_variable_ref(s, 1, 1);
  }
  static ASTNode *define(Symbol *s, ASTNode *value) {
    ASTNode *n = ast_node_create_assign(s, value, 1, 1);
    n->data.assignment.is_definition = true;
    return n;
  }
  static ASTNode *ret(ASTNode *value) {
    return ast_node_create_return(value, 1, 1);
  }
  static ASTNode *less(ASTNode *l, ASTNode *r) {
    return ast_node_create_binary_op(AST_LESS, l, r, 1, 1);
  }

  SemanticStatus check() { return semantic_check(&program); }
};

} // namespace

TEST_F(SemanticBugTest, AnnotatedSelfReferenceInInitializerIsUndefined) {
  // @main def main(): Unit =
  //   val x: Int = x
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", TYPE_INT);
  add(m, define(x, ref_var(x)));

  EXPECT_EQ(check(), SEMANTIC_UNDEFINED);
}

TEST_F(SemanticBugTest, ReturningBoolFromUnitFunctionIsAccepted) {
  // def f(): Unit = return 1 < 2      (BOOLTHEN, spec 5.2: a Boolean may be
  //                                    returned; spec 2.3 rule 4: discarded)
  def_main();
  FunctionSymbol *f = def("f", TYPE_UNIT);
  add(f, ret(less(integer(1), integer(2))));

  EXPECT_EQ(check(), SEMANTIC_OK);
}

TEST_F(SemanticBugTest, BoolExpressionStatementIsAccepted) {
  // @main def main(): Unit =
  //   1 < 2                           (BOOLTHEN, spec 5.2: Boolean is a full
  //   ()                               type, not only an if/while condition)
  FunctionSymbol *m = def_main();
  add(m, less(integer(1), integer(2)));
  add(m, ast_node_create_unit(1, 1));

  EXPECT_EQ(check(), SEMANTIC_OK);
}
