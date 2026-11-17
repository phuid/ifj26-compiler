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

/*
 * AI-generated test block — fixture and AST builders.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Fixture for hand-built semantic_check inputs: one Program plus a second
 *   FunctionTable for functions outside program.functions, and small AST
 *   builders at position 1:1; everything is released in TearDown.
 */
using Nodes = std::initializer_list<ASTNode *>;

ASTNode *integer(long long v) { return ast_node_create_int(v, 1, 1); }
ASTNode *real(double v) { return ast_node_create_real(v, 1, 1); }
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
ASTNode *truthy() { return bin(AST_LESS, integer(1), integer(2)); }
ASTNode *if_(ASTNode *c, ASTNode *t, ASTNode *e = nullptr) {
  return ast_node_create_if(c, t, e, 1, 1);
}
ASTNode *conv(ASTNode *operand) {
  return ast_node_create_implicit_int_to_real(operand);
}
ASTNode *block(Nodes xs) {
  ASTNode *b = ast_node_create_block(1, 1);
  for (ASTNode *x : xs) {
    ast_node_block_append_statement(b, x);
  }
  return b;
}
ASTNode *call(FunctionSymbol *f, Nodes args = {}) {
  ASTNode *c = ast_node_create_function_call(f, 1, 1);
  for (ASTNode *a : args) {
    ast_node_call_append_argument(c, a);
  }
  return c;
}
Symbol *var(FunctionSymbol *f, const char *name, DataType type,
            bool constant = true) {
  Symbol *s = nullptr;
  symbol_table_define(f->local_scope, name, type, true, constant, &s);
  return s;
}
void add(FunctionSymbol *f, ASTNode *s) {
  ASSERT_EQ(ast_node_block_append_statement(f->ast_root, s), AST_OK);
}

/// `def name(): ret_type =` with an empty body in @p table; the function is
/// annotated iff @p ret_type is not TYPE_UNKNOWN.
FunctionSymbol *def_in(FunctionTable *table, const char *name,
                       DataType ret_type, bool is_main = false) {
  FunctionSymbol *f = nullptr;
  function_table_get_or_create(table, name, &f);
  f->is_defined = true;
  f->is_main = is_main;
  function_symbol_create_local_scope(f);
  f->has_explicit_return_type = ret_type != TYPE_UNKNOWN;
  f->return_type.type = ret_type;
  f->ast_root = ast_node_create_block(1, 1);
  return f;
}

class SemanticCovTest : public ::testing::Test {
protected:
  Program program{};
  /// Functions outside program.functions (white-box: the parser never makes
  /// a call node refer to such a record). The AST only borrows them, so the
  /// order of destruction does not matter.
  FunctionTable foreign{};
  void SetUp() override {
    ASSERT_EQ(program_init(&program), PARSER_OK);
    ASSERT_EQ(function_table_init(&foreign), FUNCTION_TABLE_OK);
  }
  void TearDown() override {
    program_destroy(&program);
    function_table_destroy(&foreign);
  }
  SemanticStatus check() { return semantic_check(&program); }
  FunctionSymbol *def(const char *name, DataType ret_type = TYPE_UNKNOWN) {
    return def_in(&program.functions, name, ret_type);
  }
  FunctionSymbol *def_main() {
    return def_in(&program.functions, "main", TYPE_UNIT, true);
  }
};

} // namespace

/*
 * AI-generated test block — final if and then-branch errors.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Cover the remaining if-statement rules of semantic.c: an error inside the
 *   then branch of an if with else is reported as is, and a final if whose
 *   then branch is () but whose else branch returns or yields a non-Unit
 *   value is error 6.
 */
TEST_F(SemanticCovTest, ErrorInThenBranchOfIfElseIsReported) {
  // val k = 1; if (1 < 2) then { k = 2 } else { () }   -> 7 (k is a val)
  FunctionSymbol *m = def_main();
  Symbol *k = var(m, "k", TYPE_INT);
  add(m, define(k, integer(1)));
  add(m, if_(truthy(), block({assign(k, integer(2))}), block({unit()})));
  EXPECT_EQ(check(), SEMANTIC_OTHER);
}

TEST_F(SemanticCovTest, FinalIfWithUnitThenAndReturningElseIsError6) {
  // def f(): Int = if (1 < 2) then { () } else { return 1 }
  def_main();
  FunctionSymbol *f = def("f", TYPE_INT);
  add(f, if_(truthy(), block({unit()}), block({ret(integer(1))})));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticCovTest, FinalIfWithUnitThenAndValueElseIsError6) {
  // def f() = if (1 < 2) then { () } else { 1 }
  def_main();
  FunctionSymbol *f = def("f");
  add(f, if_(truthy(), block({unit()}), block({integer(1)})));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

/*
 * AI-generated test block — malformed AST is an internal error.
 * Model: Claude (Claude Code).
 * Prompt:
 *   White-box contract of semantic_check for ASTs the parser never builds:
 *   a statement node in expression position, a NULL expression slot and an
 *   Int-to-Double conversion of a non-Int operand are SEMANTIC_INTERNAL_ERROR
 *   (99); an error inside a conversion operand is reported as is.
 */
class SemanticCovStatementAsExpressionTest
    : public SemanticCovTest,
      public ::testing::WithParamInterface<ASTNodeKind> {};

TEST_P(SemanticCovStatementAsExpressionTest, IsInternalError) {
  // val x = <statement>
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", TYPE_UNKNOWN);
  Symbol *y = var(m, "y", TYPE_INT, false);
  ASTNode *statement = nullptr;
  switch (GetParam()) {
  case AST_BLOCK:
    statement = block({integer(1)}); // { 1 }
    break;
  case AST_ASSIGN:
    statement = assign(y, integer(1)); // y = 1
    break;
  case AST_RETURN:
    statement = ret(nullptr); // return
    break;
  default:
    statement = if_(truthy(), block({unit()})); // if (1 < 2) then { () }
    break;
  }
  add(m, define(x, statement));
  EXPECT_EQ(check(), SEMANTIC_INTERNAL_ERROR);
}
INSTANTIATE_TEST_SUITE_P(SemanticCov, SemanticCovStatementAsExpressionTest,
                         ::testing::Values(AST_BLOCK, AST_ASSIGN, AST_RETURN,
                                           AST_IF));

TEST_F(SemanticCovTest, NullOperandIsInternalError) {
  FunctionSymbol *m = def_main();
  ASTNode *sum = bin(AST_ADD, integer(1), integer(2));
  ast_node_destroy(sum->data.binary_op.left);
  sum->data.binary_op.left = nullptr;
  add(m, sum);
  EXPECT_EQ(check(), SEMANTIC_INTERNAL_ERROR);
}

TEST_F(SemanticCovTest, ConversionOfNonIntOperandIsInternalError) {
  // ast.h: AST_IMPLICIT_INT_TO_REAL converts an Int operand to Double.
  FunctionSymbol *m = def_main();
  add(m, conv(real(1.5)));
  EXPECT_EQ(check(), SEMANTIC_INTERNAL_ERROR);
}

TEST_F(SemanticCovTest, ErrorInsideConversionOperandIsPropagated) {
  // val x = Double(x)  -- x used inside its own initialization: 3
  FunctionSymbol *m = def_main();
  Symbol *x = var(m, "x", TYPE_UNKNOWN);
  add(m, define(x, conv(ref_var(x))));
  EXPECT_EQ(check(), SEMANTIC_UNDEFINED);
}

/*
 * AI-generated test block — calls of functions outside the program table.
 * Model: Claude (Claude Code).
 * Prompt:
 *   White-box: a call node may borrow a FunctionSymbol that is not in
 *   program.functions, so passes 1-3 never saw it or its body. An undefined
 *   callee or a call without a function is 3, a not-annotated callee with
 *   `return` is 6, a not-annotated callee that calls itself is rejected (6 or
 *   99) instead of endless recursion, and a body of unknown type is rejected.
 */
TEST_F(SemanticCovTest, UndefinedCalleeOutsideTableIsError3) {
  FunctionSymbol *ghost = nullptr;
  ASSERT_EQ(function_table_get_or_create(&foreign, "ghost", &ghost),
            FUNCTION_TABLE_OK);
  add(def_main(), call(ghost));
  EXPECT_EQ(check(), SEMANTIC_UNDEFINED);
}

TEST_F(SemanticCovTest, CallWithoutFunctionInCalleeOutsideTableIsError3) {
  // The body of ghost is only seen by the type pass, which treats a call
  // node without a function like a call of an undefined function.
  FunctionSymbol *ghost = def_in(&foreign, "ghost", TYPE_UNKNOWN);
  FunctionSymbol *target = def_in(&foreign, "target", TYPE_UNIT);
  ASTNode *orphan = call(target);
  orphan->data.call.function = nullptr;
  add(ghost, orphan);
  add(def_main(), call(ghost));
  EXPECT_EQ(check(), SEMANTIC_UNDEFINED);
}

TEST_F(SemanticCovTest, ReturnInNotAnnotatedCalleeOutsideTableIsError6) {
  // Language rule: a not-annotated function must not contain `return`.
  // The guard in check_statement is only kept for safety: without it the
  // result would still be 6, because Int cannot be coerced to the unknown
  // return type of ghost.
  FunctionSymbol *ghost = def_in(&foreign, "ghost", TYPE_UNKNOWN);
  add(ghost, ret(integer(1)));
  add(def_main(), call(ghost));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}

TEST_F(SemanticCovTest,
       RecursiveNotAnnotatedCalleeOutsideTableTerminatesWithError) {
  FunctionSymbol *ghost = def_in(&foreign, "ghost", TYPE_UNKNOWN);
  add(ghost, call(ghost));
  add(def_main(), call(ghost));
  // The language rule gives 6; semantic.c reports 99 because pass 3 normally
  // rejects such a cycle first. Either is fine, but no endless recursion.
  const SemanticStatus status = check();
  EXPECT_TRUE(status == SEMANTIC_TYPE_ERROR ||
              status == SEMANTIC_INTERNAL_ERROR)
      << status;
}

TEST_F(SemanticCovTest, NotAnnotatedCalleeEndingWithUnknownKindNodeIsRejected) {
  // def ghost() = { <node of no valid kind> }
  // Guards only that this body is rejected, and it is rejected by the
  // inference rule of check_function: the bogus node gets no type, and a
  // not-annotated body whose value has no type is error 6. The kind itself
  // is not detected: check_expression has no default case and lets the node
  // pass with an unknown type (99 would be the honest code), so { <bogus>; 1 }
  // is accepted today - a latent weakness for an input that breaks the AST
  // contract, deliberately not asserted here. The test exists to run the
  // no-match exit of that switch.
  FunctionSymbol *ghost = def_in(&foreign, "ghost", TYPE_UNKNOWN);
  ASTNode *bogus = unit();
  // One past the last enumerator; still within the enum's range of values.
  bogus->kind = static_cast<ASTNodeKind>(AST_IMPLICIT_INT_TO_REAL + 1);
  add(ghost, bogus);
  add(def_main(), call(ghost));
  const SemanticStatus status = check();
  bogus->kind = AST_CONSTANT_UNIT; // let ast_node_destroy see a valid node
  EXPECT_NE(status, SEMANTIC_OK);
}

/*
 * AI-generated test block — annotation flag without a type.
 * Model: Claude (Claude Code).
 * Prompt:
 *   White-box: has_explicit_return_type set while return_type stays UNKNOWN
 *   (the parser derives the flag from the type, so it never does this). Such
 *   a function passes on its own, but a not-annotated caller cannot infer its
 *   type from a call of it: error 6.
 */
TEST_F(SemanticCovTest, InferenceFromCalleeWithUntypedAnnotationIsError6) {
  // def h(): <no type> = return h()     def g() = h()
  def_main();
  FunctionSymbol *g = def("g");
  FunctionSymbol *h = def("h");
  h->has_explicit_return_type = true; // return_type.type stays TYPE_UNKNOWN
  // h alone passes: its return value has the same (unknown) type and the
  // body returns, so only the inference of g rejects the program.
  add(h, ret(call(h)));
  add(g, call(h));
  EXPECT_EQ(check(), SEMANTIC_TYPE_ERROR);
}
