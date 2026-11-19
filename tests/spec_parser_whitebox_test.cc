/*
 * AI-generated test block — helpers for white-box spec tests of the parser.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write white-box spec conformance tests for the IFJ26 parser. Parse
 *   spec-correct programs (with the `import ifj26.*` prolog) from a tmpfile
 *   through program_init, lexer_init and p_program, optionally run
 *   semantic_check, then inspect the FunctionTable, the scope tree of each
 *   function and the AST. Run the same pipeline first in a forked child so
 *   that a crash or a hang fails one test instead of the whole binary, and
 *   report environment failures and statuses by name.
 */
#include <cstdio>
#include <ostream>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

#include "gtest/gtest.h"

extern "C" {
#include "../src/ast.h"
#include "../src/function_table.h"
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/semantic.h"
#include "../src/symtable.h"
}

#include "spec_harness.h"

namespace {

/** Spec-correct entry function appended to every parsed program. */
const char *const kMainFunction = "@main\ndef main(): Unit =\n  ()\n";

/**
 * Prepends the mandatory prolog (spec 2) unless IFJ26_HARNESS_NO_PROLOG=1,
 * which shows the root cause hidden behind RC-PROLOG.
 */
std::string WithProlog(const std::string &body) {
  if (ifj26_harness_skips_prolog()) {
    return body;
  }
  return std::string(kIfj26Prolog) + body;
}

const char *ParserStatusName(ParserStatus status) {
  switch (status) {
  case PARSER_OK:
    return "PARSER_OK";
  case PARSER_SYNTAX_ERROR:
    return "PARSER_SYNTAX_ERROR";
  case PARSER_SEMANTIC_ERROR:
    return "PARSER_SEMANTIC_ERROR";
  case PARSER_LEXER_ERROR:
    return "PARSER_LEXER_ERROR";
  case PARSER_INVALID_ARGUMENT:
    return "PARSER_INVALID_ARGUMENT";
  case PARSER_INTERNAL_ERROR:
    return "PARSER_INTERNAL_ERROR";
  }
  return "<unknown ParserStatus>";
}

const char *SemanticStatusName(SemanticStatus status) {
  switch (status) {
  case SEMANTIC_OK:
    return "SEMANTIC_OK";
  case SEMANTIC_UNDEFINED:
    return "SEMANTIC_UNDEFINED";
  case SEMANTIC_REDEFINITION:
    return "SEMANTIC_REDEFINITION";
  case SEMANTIC_BAD_CALL:
    return "SEMANTIC_BAD_CALL";
  case SEMANTIC_TYPE_ERROR:
    return "SEMANTIC_TYPE_ERROR";
  case SEMANTIC_OTHER:
    return "SEMANTIC_OTHER";
  case SEMANTIC_INTERNAL_ERROR:
    return "SEMANTIC_INTERNAL_ERROR";
  }
  return "<unknown SemanticStatus>";
}

/**
 * Parses @p source into @p program. @p io_ok becomes false when the test
 * environment (tmpfile/fputs) fails, so that is never blamed on the parser.
 */
ParserStatus ParseInto(const std::string &source, Program *program,
                       bool *io_ok) {
  FILE *stream = tmpfile();
  *io_ok = stream != nullptr && fputs(source.c_str(), stream) != EOF;
  if (!*io_ok) {
    if (stream != nullptr) {
      fclose(stream);
    }
    return PARSER_INTERNAL_ERROR;
  }
  rewind(stream);
  Lexer lexer;
  ParserStatus status = PARSER_INTERNAL_ERROR;
  if (lexer_init(&lexer, stream) == LEXER_OK) {
    status = p_program(&lexer, program);
    lexer_destroy(&lexer);
  }
  fclose(stream);
  return status;
}

/**
 * Runs parse (+ semantic_check) in a forked child with the harness time
 * limit; true when the child was killed by a signal (crash or hang).
 */
bool PipelineDiesInChild(const std::string &source, bool run_semantic) {
  fflush(stdout);
  fflush(stderr);
  pid_t child = fork();
  if (child < 0) {
    return false;
  }
  if (child == 0) {
    alarm(kIfj26TimeLimitSeconds);
    Program program{};
    bool io_ok = false;
    if (program_init(&program) == PARSER_OK &&
        ParseInto(source, &program, &io_ok) == PARSER_OK && run_semantic) {
      semantic_check(&program);
    }
    _exit(0);
  }
  int wait_status = 0;
  return waitpid(child, &wait_status, 0) == child && WIFSIGNALED(wait_status);
}

} // namespace

/*
 * AI-generated test block — parsed program wrapper.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Own one Program parsed from a body (prolog added), optionally checked by
 *   semantic_check, with accessors for function f, its statements, the
 *   initializer of a val and the condition of an if.
 */
namespace {

/** A program body (after the prolog) parsed with p_program. */
class ParsedProgram {
public:
  ParsedProgram(const std::string &body, bool run_semantic)
      : source_(WithProlog(body)) {
    died_ = PipelineDiesInChild(source_, run_semantic);
    if (died_ || program_init(&program_) != PARSER_OK) {
      return;
    }
    status_ = ParseInto(source_, &program_, &io_ok_);
    if (status_ == PARSER_OK && run_semantic) {
      semantic_ = semantic_check(&program_);
    }
  }
  ~ParsedProgram() { program_destroy(&program_); }
  ParsedProgram(const ParsedProgram &) = delete;
  ParsedProgram &operator=(const ParsedProgram &) = delete;

  bool io_ok() const { return io_ok_; }
  bool died() const { return died_; }
  ParserStatus status() const { return status_; }
  SemanticStatus semantic() const { return semantic_; }
  const std::string &source() const { return source_; }

  /** Function f of the program, or null. */
  FunctionSymbol *F() const {
    return function_table_lookup(&program_.functions, "f");
  }
  /** Statement @p index of the body of f, or null when it is missing. */
  ASTNode *Statement(size_t index) const {
    FunctionSymbol *f = F();
    if (f == nullptr || f->ast_root == nullptr ||
        f->ast_root->kind != AST_BLOCK ||
        f->ast_root->data.block.statement_count <= index) {
      return nullptr;
    }
    return f->ast_root->data.block.statements[index];
  }
  /** Value of the `val` in statement @p index, or null. */
  ASTNode *Initializer(size_t index = 0U) const {
    ASTNode *s = Statement(index);
    return s != nullptr && s->kind == AST_ASSIGN ? s->data.assignment.value
                                                 : nullptr;
  }
  /** Condition of the `if` in the first statement, or null. */
  ASTNode *Condition() const {
    ASTNode *s = Statement(0U);
    return s != nullptr && s->kind == AST_IF ? s->data.if_statement.condition
                                             : nullptr;
  }

private:
  std::string source_;
  Program program_{};
  bool io_ok_ = false;
  bool died_ = false;
  ParserStatus status_ = PARSER_INTERNAL_ERROR;
  SemanticStatus semantic_ = SEMANTIC_INTERNAL_ERROR;
};

/** Variable @p name defined directly in @p scope (not in an ancestor). */
Symbol *Local(const SymbolTable *scope, const char *name) {
  return scope != nullptr ? symbol_table_lookup_local(scope, name) : nullptr;
}

} // namespace

/** Asserts a written, non-crashing program that p_program accepts. */
#define ASSERT_PARSED(p)                                                       \
  do {                                                                         \
    ASSERT_TRUE((p).io_ok() || (p).died()) << "tmpfile/fputs failed";          \
    ASSERT_FALSE((p).died())                                                   \
        << "pipeline killed by a signal (crash or hang) in the forked "        \
           "pre-flight:\n"                                                     \
        << (p).source();                                                       \
    ASSERT_EQ((p).status(), PARSER_OK)                                         \
        << ParserStatusName((p).status()) << "\n"                              \
        << (p).source();                                                       \
  } while (0)

/** ASSERT_PARSED plus semantic_check == SEMANTIC_OK. */
#define ASSERT_CHECKED(p)                                                      \
  do {                                                                         \
    ASSERT_PARSED(p);                                                          \
    ASSERT_EQ((p).semantic(), SEMANTIC_OK)                                     \
        << SemanticStatusName((p).semantic()) << "\n"                          \
        << (p).source();                                                       \
  } while (0)

/*
 * AI-generated test block — expression AST printer with conversions.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Copy the Sexpr printer of tests/expr_edge_test.cc (it lives in an
 *   anonymous namespace there), extend it with d(...) for
 *   AST_IMPLICIT_INT_TO_REAL and print Double literals with a decimal point
 *   so that 3.0 differs from 3.
 */
namespace {

const char *OperatorSpelling(ASTNodeKind kind) {
  switch (kind) {
  case AST_ADD:
    return "+";
  case AST_SUBTRACT:
    return "-";
  case AST_MULTIPLY:
    return "*";
  case AST_DIVIDE:
    return "/";
  case AST_LESS:
    return "<";
  case AST_LESS_EQUAL:
    return "<=";
  case AST_GREATER:
    return ">";
  case AST_GREATER_EQUAL:
    return ">=";
  case AST_EQUAL:
    return "==";
  case AST_NOT_EQUAL:
    return "!=";
  default:
    return nullptr;
  }
}

/** %g with ".0" appended to integral values: 3.0 -> "3.0", 0.5 -> "0.5". */
std::string RealSpelling(double value) {
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%g", value);
  std::string text = buffer;
  if (text.find_first_of(".eni") == std::string::npos) {
    text += ".0";
  }
  return text;
}

std::string LeafSexpr(const ASTNode *node) {
  switch (node->kind) {
  case AST_CONSTANT_INT:
    return std::to_string(node->data.int_value);
  case AST_CONSTANT_REAL:
    return RealSpelling(node->data.real_value);
  case AST_CONSTANT_STRING:
    return "\"" + std::string(node->data.string_value) + "\"";
  case AST_CONSTANT_UNIT:
    return "()";
  case AST_VARIABLE_REF:
    return node->data.variable.symbol != nullptr
               ? node->data.variable.symbol->name
               : "<null var>";
  default:
    return "<kind " + std::to_string(node->kind) + ">";
  }
}

/**
 * Fully parenthesised text of an expression AST; d(e) marks an inserted
 * Int->Double conversion of e. Parentheses of the source add no node.
 */
std::string Sexpr(const ASTNode *node) {
  if (node == nullptr) {
    return "<null>";
  }
  if (node->kind == AST_IMPLICIT_INT_TO_REAL) {
    return "d(" + Sexpr(node->data.conversion.operand) + ")";
  }
  const char *op = OperatorSpelling(node->kind);
  if (op == nullptr) {
    return LeafSexpr(node);
  }
  return "(" + Sexpr(node->data.binary_op.left) + " " + op + " " +
         Sexpr(node->data.binary_op.right) + ")";
}

/** Header A of catalog A5: `val r [: Double] = <expression>` in f. */
std::string InitializerSource(const std::string &expression, bool as_double) {
  return std::string("def f(a: Int, b: Int, x: Double, s: String) =\n") +
         "  val r" + (as_double ? ": Double" : "") + " = " + expression + "\n" +
         kMainFunction;
}

/** Header B of catalog A5: `if <expression> then () else ()` in f. */
std::string ConditionSource(const std::string &expression) {
  return std::string("def f(a: Int, b: Int, x: Double): Unit =\n") + "  if " +
         expression + " then\n    ()\n  else\n    ()\n" + kMainFunction;
}

} // namespace

/*
 * AI-generated test block — definitions next to branch scopes (spec 2.2).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Through p_program and semantic_check: a val in the enclosing block after
 *   val/var of the same name in both if branches is accepted and every scope
 *   holds its own symbol with the constant flag of val/var and its own type;
 *   one more definition in the enclosing block is a redefinition (code 4),
 *   reported by the parser or by semantic_check (A2-100, reworked after the
 *   check of the file).
 */
namespace {

/** f defines y in the then branch (val), else branch (var) and after if. */
const char *const kBranchesThenParent = "def f(a: Int): Unit =\n"
                                        "  if a < 1 then\n"
                                        "    val y = 1\n"
                                        "  else\n"
                                        "    var y = \"s\"\n"
                                        "  val y = 2.0\n";

/** Asserts one parser-made symbol y: its constant flag and type. */
void ExpectY(const SymbolTable *scope, bool constant, DataType type) {
  Symbol *y = Local(scope, "y");
  ASSERT_NE(y, nullptr);
  EXPECT_EQ(y->constant, constant);
  EXPECT_EQ(y->type.type, type);
}

} // namespace

// §2.2: a sub-block may shadow, val is immutable and var mutable; §2.4: both
// branches are separate scopes, so their `y` does not block a later `y` in
// the enclosing block (A2-100).
// Fails today: RC-PROLOG (status SYNTAX at `import`); without the prolog
// RC-EXPR (stub condition is NULL -> SYNTAX).
TEST(SpecParserWhiteboxDefinitionTest,
     ParentDefinitionAfterBranchDefinitionsIsAllowed) {
  // Arrange
  std::string body = std::string(kBranchesThenParent) + kMainFunction;

  // Act
  ParsedProgram p(body, true);

  // Assert
  ASSERT_CHECKED(p);
  FunctionSymbol *f = p.F();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->local_scope, nullptr);
  ASSERT_EQ(f->local_scope->child_count, 2U);
  ExpectY(f->local_scope->children[0], true, TYPE_INT);
  ExpectY(f->local_scope->children[1], false, TYPE_STRING);
  ExpectY(f->local_scope, true, TYPE_REAL);
}

// §2.2: "Opakovaná definice ve stejném bloku vede na chybu 4" - a second
// `y` in the enclosing block is a redefinition even after the branch ones;
// the spec does not say which pass reports it (A2-100).
// Fails today: RC-PROLOG (status SYNTAX at `import`); without the prolog
// RC-EXPR (stub condition is NULL -> SYNTAX).
TEST(SpecParserWhiteboxDefinitionTest,
     SecondParentDefinitionAfterBranchesIsRedefinition) {
  // Arrange
  std::string body =
      std::string(kBranchesThenParent) + "  var y = 3.0\n" + kMainFunction;

  // Act
  ParsedProgram p(body, true);

  // Assert
  ASSERT_TRUE(p.io_ok() || p.died()) << "tmpfile/fputs failed";
  ASSERT_FALSE(p.died()) << p.source();
  bool by_parser = p.status() == PARSER_SEMANTIC_ERROR;
  bool by_semantic =
      p.status() == PARSER_OK && p.semantic() == SEMANTIC_REDEFINITION;
  EXPECT_TRUE(by_parser || by_semantic)
      << ParserStatusName(p.status()) << " / "
      << SemanticStatusName(p.semantic()) << "\n"
      << p.source();
}

/*
 * AI-generated test block — scope tree that p_program builds for if/while.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Parse spec-correct programs with p_program and check the scope tree of
 *   function f: each if branch and the while body is its own child scope of
 *   the enclosing block, nested ifs give nested scopes, and the variables of
 *   the branch stay out of the enclosing scope (A4-140..142, with prolog).
 */

// §2.4: "Obě větve vytvářejí samostatné rozsahy platnosti" - each branch
// gets its own child scope, so `x` may be defined in both (A4-140).
// Fails today: RC-PROLOG (status SYNTAX at `import`); without the prolog
// RC-EXPR (stub condition is NULL -> SYNTAX).
TEST(SpecParserWhiteboxScopeTest, IfBranchesGetSeparateChildScopes) {
  // Arrange
  std::string body = "def f(a: Int): Unit =\n  if a < 1 then\n    val x = 1\n"
                     "  else\n    val x = \"s\"\n";

  // Act
  ParsedProgram p(body + kMainFunction, false);

  // Assert
  ASSERT_PARSED(p);
  FunctionSymbol *f = p.F();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->local_scope, nullptr);
  ASSERT_EQ(f->local_scope->child_count, 2U);
  Symbol *in_then = Local(f->local_scope->children[0], "x");
  Symbol *in_else = Local(f->local_scope->children[1], "x");
  ASSERT_NE(in_then, nullptr);
  ASSERT_NE(in_else, nullptr);
  EXPECT_NE(in_then, in_else);
  EXPECT_EQ(Local(f->local_scope, "x"), nullptr);
}

// §2.4: a nested if inside the then branch opens scopes nested in the
// then-scope of the outer if (A4-141).
// Fails today: RC-PROLOG (status SYNTAX at `import`); without the prolog
// RC-EXPR (stub condition is NULL -> SYNTAX).
TEST(SpecParserWhiteboxScopeTest, NestedIfScopesAreNested) {
  // Arrange
  std::string body = "def f(a: Int): Unit =\n  if a < 1 then\n"
                     "    if a < 0 then\n      val x = 1\n    else\n"
                     "      ()\n  else\n    ()\n";

  // Act
  ParsedProgram p(body + kMainFunction, false);

  // Assert
  ASSERT_PARSED(p);
  FunctionSymbol *f = p.F();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->local_scope, nullptr);
  ASSERT_EQ(f->local_scope->child_count, 2U);
  SymbolTable *outer_then = f->local_scope->children[0];
  ASSERT_EQ(outer_then->child_count, 2U);
  EXPECT_NE(Local(outer_then->children[0], "x"), nullptr);
  EXPECT_EQ(Local(outer_then, "x"), nullptr);
  EXPECT_EQ(f->local_scope->children[1]->child_count, 0U);
}

// §2.5: "Tělo tvoří samostatný rozsah platnosti" - the while body is one
// child scope and its `x` is not visible in the enclosing block (A4-142).
// Fails today: RC-PROLOG (status SYNTAX at `import`); without the prolog
// RC-WHILE (`while` has no production in p_statement -> SYNTAX), then
// RC-EXPR.
TEST(SpecParserWhiteboxScopeTest, WhileBodyGetsChildScope) {
  // Arrange
  std::string body =
      "def f(a: Int): Unit =\n  while a < 1 do\n    val x = 1\n  ()\n";

  // Act
  ParsedProgram p(body + kMainFunction, false);

  // Assert
  ASSERT_PARSED(p);
  FunctionSymbol *f = p.F();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->local_scope, nullptr);
  ASSERT_EQ(f->local_scope->child_count, 1U);
  EXPECT_NE(Local(f->local_scope->children[0], "x"), nullptr);
  EXPECT_EQ(Local(f->local_scope, "x"), nullptr);
}

/*
 * AI-generated test block — conversion placement proves precedence.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Parse expressions with p_program, run semantic_check and compare the
 *   expression AST printed with d(...) for each Int->Double conversion.
 *   A conversion wraps a whole Int subtree only when the grouping of spec 3
 *   (priority and left associativity) is right (A5-W30..W35, W39, active
 *   and with the prolog as review revB requires), plus a whole Int value
 *   widened for a Double target and an Int-only control without conversion.
 */
namespace {

/** Where the expression of a row stands in function f. */
enum class Place { kInitializer, kDoubleInitializer, kCondition };

/** One expression with its expected AST shape after semantic_check. */
struct ShapeCase {
  const char *name;
  Place place;
  const char *expression;
  const char *shape;
  DataType type;
};

/** Prints a row as its name instead of a byte dump in failure messages. */
void PrintTo(const ShapeCase &row, std::ostream *os) { *os << row.name; }

std::string ShapeCaseName(const ::testing::TestParamInfo<ShapeCase> &info) {
  return info.param.name;
}

std::string ShapeSource(const ShapeCase &row) {
  if (row.place == Place::kCondition) {
    return ConditionSource(row.expression);
  }
  return InitializerSource(row.expression,
                           row.place == Place::kDoubleInitializer);
}

class SpecParserWhiteboxConversionShapeTest
    : public ::testing::TestWithParam<ShapeCase> {};

const ShapeCase kShapeCases[] = {
    // §3: `/` (priority 2) binds before `+` (3); §1.3: the conversion wraps
    // the whole Int quotient, so 1 / 2 stays integer division (A5-W30).
    // Fails today: RC-PROLOG; without the prolog RC-EXPR (val -> INTERNAL),
    // then RC-PSA.
    {"QuotientIsWidenedAfterIntegerDivision", Place::kDoubleInitializer,
     "1 / 2 + 0.5", "(d((1 / 2)) + 0.5)", TYPE_REAL},
    // §3: `*` binds before `+`; (1 + 2) * 3.0 would print
    // (d((1 + 2)) * 3.0) (A5-W31).
    // Fails today: RC-PROLOG; without the prolog RC-EXPR, then RC-PSA.
    {"ProductBindsBeforeWideningSum", Place::kDoubleInitializer, "1 + 2 * 3.0",
     "(d(1) + (d(2) * 3.0))", TYPE_REAL},
    // §3: `/` is left associative; the right grouping 1 / (2 / 2.0) would
    // print (d(1) / (d(2) / 2.0)) (A5-W32).
    // Fails today: RC-PROLOG; without the prolog RC-EXPR, then RC-PSA.
    {"DivisionChainWidensLeftQuotient", Place::kInitializer, "1 / 2 / 2.0",
     "(d((1 / 2)) / 2.0)", TYPE_REAL},
    // §3: `*` and `/` share priority 2 and associate left; every Int leaf
    // next to a Double is widened on its own (A5-W33).
    // Fails today: RC-PROLOG; without the prolog RC-EXPR, then RC-PSA.
    {"MixedProductQuotientWidensEachIntLeaf", Place::kInitializer,
     "2.0 * 3 / 2", "((2.0 * d(3)) / d(2))", TYPE_REAL},
    // §3: `+` (3) binds before `<` (4); the Int sum is widened as a whole
    // (A5-W34).
    // Fails today: RC-PROLOG; without the prolog RC-EXPR (NULL condition ->
    // SYNTAX), then RC-PSA.
    {"RelationWidensWholeIntSum", Place::kCondition, "a + b < x",
     "(d((a + b)) < x)", TYPE_BOOL},
    // §3: `a < b + x` is a < (b + x); a is widened, b inside the sum
    // (A5-W35).
    // Fails today: RC-PROLOG; without the prolog RC-EXPR (NULL condition ->
    // SYNTAX), then RC-PSA.
    {"RelationWidensLeftLeafAndRightSumLeaf", Place::kCondition, "a < b + x",
     "(d(a) < (d(b) + x))", TYPE_BOOL},
    // §3: `-` is left associative; the Int difference a - b is widened as a
    // whole (A5-W39).
    // Fails today: RC-PROLOG; without the prolog RC-EXPR, then RC-PSA.
    {"SubtractionChainWidensLeftDifference", Place::kDoubleInitializer,
     "a - b - x", "(d((a - b)) - x)", TYPE_REAL},
    // §1.3 + §2.2: an Int value is compatible with a declared Double; the
    // whole Int difference is widened once, at the top (check of the file).
    // Fails today: RC-PROLOG; without the prolog RC-EXPR, then RC-PSA.
    {"DoubleTargetWidensWholeIntDifference", Place::kDoubleInitializer, "a - b",
     "d((a - b))", TYPE_REAL},
    // §3: + - * over two Int values give Int; no conversion anywhere
    // (control row, check of the file).
    // Fails today: RC-PROLOG; without the prolog RC-EXPR, then RC-PSA.
    {"IntOnlyExpressionHasNoConversion", Place::kInitializer, "a + b * 2",
     "(a + (b * 2))", TYPE_INT},
};

const ASTNode *ShapedExpression(const ShapeCase &row, const ParsedProgram &p) {
  return row.place == Place::kCondition ? p.Condition() : p.Initializer();
}

} // namespace

TEST_P(SpecParserWhiteboxConversionShapeTest, ConversionsFollowGrouping) {
  // Arrange
  const ShapeCase &row = GetParam();

  // Act
  ParsedProgram p(ShapeSource(row), true);

  // Assert
  ASSERT_CHECKED(p);
  const ASTNode *expression = ShapedExpression(row, p);
  ASSERT_NE(expression, nullptr) << p.source();
  EXPECT_EQ(Sexpr(expression), row.shape) << p.source();
  EXPECT_EQ(expression->inferred_type.type, row.type) << p.source();
}

INSTANTIATE_TEST_SUITE_P(SpecParserWhitebox,
                         SpecParserWhiteboxConversionShapeTest,
                         ::testing::ValuesIn(kShapeCases), ShapeCaseName);

/*
 * AI-generated test block — unary minus through the whole pipeline.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Parse unary minus with p_program and check with semantic_check that the
 *   negation stays in the AST, keeps the operand type and is widened as a
 *   whole when a Double is needed (A5-W36..W38, active and with the prolog
 *   as review revB requires; W40 dropped as a duplicate of A5-025).
 */
namespace {

/**
 * Fails when the unary minus was dropped and only its operand @p operand is
 * left. The AST has no unary node yet (RC-UNARY), so any representation
 * other than the bare operand counts as kept.
 */
void ExpectNegationKept(const ASTNode *negation, const char *operand) {
  ASSERT_NE(negation, nullptr);
  EXPECT_NE(negation->kind, AST_IMPLICIT_INT_TO_REAL) << Sexpr(negation);
  EXPECT_NE(Sexpr(negation), operand) << "unary minus vanished from the AST";
}

} // namespace

// §3: unary operators apply to numbers only and do not change the type:
// -x is Double, -a is Int (A5-W36).
// Fails today: RC-PROLOG; without the prolog RC-EXPR (val -> INTERNAL), then
// RC-PSA and RC-UNARY (no terminal, AST node or type rule for unary minus).
TEST(SpecParserWhiteboxUnaryMinusTest, UnaryMinusKeepsOperandType) {
  // Arrange
  std::string body = "def f(a: Int, b: Int, x: Double, s: String) =\n"
                     "  val r = -x\n  val q = -a\n";

  // Act
  ParsedProgram p(body + kMainFunction, true);

  // Assert
  ASSERT_CHECKED(p);
  FunctionSymbol *f = p.F();
  ASSERT_NE(f, nullptr);
  Symbol *r = Local(f->local_scope, "r");
  Symbol *q = Local(f->local_scope, "q");
  ASSERT_NE(r, nullptr);
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(r->type.type, TYPE_REAL);
  EXPECT_EQ(q->type.type, TYPE_INT);
  ExpectNegationKept(p.Initializer(0U), "x");
  ExpectNegationKept(p.Initializer(1U), "a");
}

// §3 + §1.3: -a stays Int, so a Double target widens the negation as a
// whole: the conversion is above the unary node (A5-W37).
// Fails today: RC-PROLOG; without the prolog RC-EXPR, then RC-PSA, RC-UNARY.
TEST(SpecParserWhiteboxUnaryMinusTest, NegatedIntIsWidenedAsWhole) {
  // Arrange
  std::string source = InitializerSource("-a", true);

  // Act
  ParsedProgram p(source, true);

  // Assert
  ASSERT_CHECKED(p);
  const ASTNode *value = p.Initializer();
  ASSERT_NE(value, nullptr);
  ASSERT_EQ(value->kind, AST_IMPLICIT_INT_TO_REAL) << Sexpr(value);
  const ASTNode *negation = value->data.conversion.operand;
  ExpectNegationKept(negation, "a");
  ASSERT_NE(negation, nullptr);
  EXPECT_EQ(negation->inferred_type.type, TYPE_INT);
}

// §3: unary `-` (priority 1) binds before `<` (4); the Int negation is the
// widened left operand (A5-W38).
// Fails today: RC-PROLOG; without the prolog RC-EXPR (NULL condition ->
// SYNTAX), then RC-PSA, RC-UNARY.
TEST(SpecParserWhiteboxUnaryMinusTest, UnaryMinusOperandOfRelationIsWidened) {
  // Arrange
  std::string source = ConditionSource("-a < x");

  // Act
  ParsedProgram p(source, true);

  // Assert
  ASSERT_CHECKED(p);
  const ASTNode *condition = p.Condition();
  ASSERT_NE(condition, nullptr);
  ASSERT_EQ(condition->kind, AST_LESS) << Sexpr(condition);
  const ASTNode *left = condition->data.binary_op.left;
  ASSERT_NE(left, nullptr);
  ASSERT_EQ(left->kind, AST_IMPLICIT_INT_TO_REAL) << Sexpr(condition);
  const ASTNode *negation = left->data.conversion.operand;
  ExpectNegationKept(negation, "a");
  ASSERT_NE(negation, nullptr);
  EXPECT_EQ(negation->inferred_type.type, TYPE_INT);
}
