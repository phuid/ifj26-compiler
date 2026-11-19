#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "../src/parser.h"
#include "../src/psa.h"
#include "../src/semantic.h"
}

namespace {

Terminal MakeTerminal(TerminalType type) {
  Terminal terminal = {};
  terminal.type = type;
  terminal.token.type = TOKEN_INVALID;
  terminal.node = nullptr;
  return terminal;
}

/** Relation of stack top @p top and the terminal of input token @p next. */
PRECEDENCE PrecedenceOf(TerminalType top, TokenType next) {
  Token token = {};
  token.type = next;
  return get_precedence(top, token_to_terminal(token));
}

} // namespace

/*
 * AI-generated test block — TerminalStack descriptor reset and status codes.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover what psa_test.cc / psa_cov_test.cc leave open for the implemented
 *   psa.h API: terminal_stack_init must reset every field of a descriptor
 *   that already holds values (deterministically, not on stack garbage),
 *   and a drained stack must keep rejecting pop/peek without underflow.
 */
// Ported to the psa.h of ee9d509: pop was removed without a replacement, peek
// became stack_get_top_terminal and a stack is drained by
// terminal_stack_cleanup_nodes.
TEST(ExprEdgeTest, InitResetsEveryFieldOfAUsedDescriptor) {
  Terminal storage[2] = {MakeTerminal(TERMINAL_PLUS),
                         MakeTerminal(TERMINAL_MUL)};
  TerminalStack stack = {storage, 2U, 2U};

  ASSERT_EQ(terminal_stack_init(&stack), PSA_OK);

  EXPECT_EQ(stack.data, nullptr);
  ASSERT_EQ(stack.size, 0U); // the lookup below must not index old storage
  EXPECT_EQ(stack.capacity, 0U);
  EXPECT_EQ(storage[0].type, TERMINAL_PLUS); // old storage is not touched
  EXPECT_EQ(storage[1].type, TERMINAL_MUL);
  Terminal out = MakeTerminal(TERMINAL_INVALID);
  EXPECT_EQ(stack_get_top_terminal(&stack, &out), PSA_INVALID_ARGUMENT);
  EXPECT_EQ(out.type, TERMINAL_INVALID);
}

TEST(ExprEdgeTest, DrainedStackKeepsRejectingTopLookup) {
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PSA_OK);
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_DOLLAR)), PSA_OK);
  ASSERT_EQ(terminal_stack_cleanup_nodes(&stack), PSA_OK);
  Terminal out = MakeTerminal(TERMINAL_INVALID);

  for (int attempt = 0; attempt < 3; ++attempt) {
    SCOPED_TRACE(attempt);
    out = MakeTerminal(TERMINAL_INVALID);
    EXPECT_EQ(stack_get_top_terminal(&stack, &out), PSA_INVALID_ARGUMENT);
    EXPECT_EQ(out.type, TERMINAL_INVALID);
    EXPECT_EQ(stack.size, 0U);
  }
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_OR)), PSA_OK);
  ASSERT_EQ(stack_get_top_terminal(&stack, &out), PSA_OK);
  EXPECT_EQ(out.type, TERMINAL_OR);
  EXPECT_EQ(terminal_stack_cleanup_nodes(&stack), PSA_OK);
}

/*
 * AI-generated test block — shared token groups for get_precedence.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Group the expression tokens of IFJ26 (spec ch. 3 operator table plus the
 *   LISTS `++` operator, 5.8) the way psa.c token_to_terminal classifies
 *   them, so precedence rows can be written as (stack top, group, relation).
 */
namespace {

struct TokenGroup {
  const char *name;
  const TokenType *tokens;
  size_t count;
};

const TokenType kMulTokens[] = {TOKEN_MULTIPLY, TOKEN_DIVIDE};
// Binary `-` only; unary minus is handled by the driver (p_expression).
const TokenType kAddTokens[] = {TOKEN_PLUS, TOKEN_MINUS, TOKEN_LIST_CONCAT};
// Spec 3 level 4 (TERMINAL_REL) and level 5 (TERMINAL_EQUAL).
const TokenType kRelTokens[] = {TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_GREATER,
                                TOKEN_GREATER_EQUAL};
const TokenType kEqTokens[] = {TOKEN_EQUAL, TOKEN_NOT_EQUAL};
// Tokens that start an operand are no table terminals (TERMINAL_INVALID);
// `(` opens a parenthesised operand, which the driver parses as a whole.
const TokenType kOperandTokens[] = {TOKEN_INT, TOKEN_DOUBLE, TOKEN_STRING,
                                    TOKEN_IDENTIFIER, TOKEN_LEFT_PAREN};
// Tokens psa.c maps to TERMINAL_DOLLAR (end of the expression); `)` ends a
// parenthesised operand.
const TokenType kEndTokens[] = {
    TOKEN_EOF,   TOKEN_RIGHT_PAREN, TOKEN_NEWLINE, TOKEN_SEMICOLON,
    TOKEN_COMMA, TOKEN_COLON,       TOKEN_DEDENT,  TOKEN_KW_THEN,
    TOKEN_KW_DO, TOKEN_KW_ELSE};
// Table rows (stack tops) plus the stack-only operand marker.
const TerminalType kTableTops[] = {
    TERMINAL_MUL,    TERMINAL_DIV,    TERMINAL_PLUS,  TERMINAL_MINUS,
    TERMINAL_CONCAT, TERMINAL_REL,    TERMINAL_EQUAL, TERMINAL_AND,
    TERMINAL_OR,     TERMINAL_TO,     TERMINAL_UNTIL, TERMINAL_BY,
    TERMINAL_DOLLAR, TERMINAL_OPERAND};

#define EXPR_EDGE_GROUP(array) {#array, array, sizeof(array) / sizeof(array[0])}
const TokenGroup kMul = EXPR_EDGE_GROUP(kMulTokens);
const TokenGroup kAdd = EXPR_EDGE_GROUP(kAddTokens);
const TokenGroup kRel = EXPR_EDGE_GROUP(kRelTokens);
const TokenGroup kEq = EXPR_EDGE_GROUP(kEqTokens);
const TokenGroup kOperand = EXPR_EDGE_GROUP(kOperandTokens);
const TokenGroup kEnd = EXPR_EDGE_GROUP(kEndTokens);
#undef EXPR_EDGE_GROUP

struct RowCell {
  const TokenGroup *group;
  PRECEDENCE expected;
};

/** Checks the relation of (top, t) == cell.expected for every t in a group. */
template <size_t N>
void ExpectRow(TerminalType top, const RowCell (&cells)[N]) {
  for (const RowCell &cell : cells) {
    for (size_t i = 0; i < cell.group->count; ++i) {
      const TokenType next = cell.group->tokens[i];
      EXPECT_EQ(PrecedenceOf(top, next), cell.expected)
          << "top " << top << ", " << cell.group->name << " token " << next;
    }
  }
}

} // namespace

/*
 * AI-generated test block — get_precedence cells that must stay empty.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Pin the "no relation" cells of the operator-precedence table that the
 *   spec fixes regardless of the team's table design: an operand or `)`
 *   followed by an operand or `(`, an unclosed `(` at the end of input, and
 *   markers that are not terminals (HANDLE, INVALID) on the stack top.
 *   Statement-only tokens never relate to any stack top.
 */
// Ported to the psa.h of ee9d509: parentheses are no table terminals any more
// (TERMINAL_LPAREN/RPAREN and PREC_EQUAL were removed), so operand tokens and
// `(` have no relation with any top, and the unclosed `(` is checked through
// p_program in ExprEdgeTest.UnclosedLeftParenAtExpressionEndIsSyntaxError.
// Cells already pinned by psa_test.cc / psa_cov_test.cc are left out.
TEST(ExprEdgeTest, OperandTokensHaveNoRelationWithAnyTop) {
  for (const TerminalType top : kTableTops) {
    for (const TokenType next : kOperandTokens) {
      EXPECT_EQ(PrecedenceOf(top, next), PREC_INVALID)
          << "top " << top << ", next " << next;
    }
  }
}

TEST(ExprEdgeTest, MarkersOnStackTopHaveNoRelationWithAnyToken) {
  const TokenGroup *groups[] = {&kMul, &kAdd, &kRel, &kEq, &kOperand, &kEnd};
  for (const TerminalType top :
       {TERMINAL_HANDLE, TERMINAL_INVALID, TERMINAL_OPERAND}) {
    for (const TokenGroup *group : groups) {
      for (size_t i = 0; i < group->count; ++i) {
        EXPECT_EQ(PrecedenceOf(top, group->tokens[i]), PREC_INVALID)
            << "top " << top << ", token " << group->tokens[i];
      }
    }
  }
}

TEST(ExprEdgeTest, StatementOnlyTokensHaveNoRelation) {
  const TokenType foreign[] = {
      TOKEN_KW_VAL,     TOKEN_KW_VAR,    TOKEN_KW_WHILE,     TOKEN_KW_RETURN,
      TOKEN_KW_INT,     TOKEN_KW_DOUBLE, TOKEN_KW_STRING,    TOKEN_KW_UNIT,
      TOKEN_KW_BOOLEAN, TOKEN_UNION,     TOKEN_FOR_GENERATOR};
  for (const TerminalType top : kTableTops) {
    for (const TokenType next : foreign) {
      EXPECT_EQ(PrecedenceOf(top, next), PREC_INVALID)
          << "top " << top << ", next " << next;
    }
  }
}

/*
 * AI-generated test block — complete precedence table (known gap).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write the whole operator-precedence table of IFJ26 row by row (stack
 *   top terminal x input token group) from spec ch. 3 (unary - > * / > + -
 *   > relational; * / + - left associative) and LISTS 5.8 (`++` has the
 *   priority of `+`, left associative). Disabled: get_precedence is a stub.
 */
// What must hold (spec 3, table "od nejvyšší priority"; 5.8 for `++`):
//  * top of higher or equal level than the input operator -> REDUCE (left
//    associativity for equal levels), lower level -> TAKE;
//  * every end token (also `)`) reduces every operator; $ takes every one.
// Ported to the psa.h of ee9d509: the rows of `(`, `)` and operands were
// removed with their terminals (operands and parentheses are parsed by the
// driver, not looked up in the table); REL x REL/EQUAL, the logical and range
// operators are pinned by psa_cov_test.cc; unary minus is the driver's.
TEST(ExprEdgeKnownGapTest, DISABLED_MultiplicativeRowsReduceEveryOperator) {
  const RowCell row[] = {{&kMul, PREC_REDUCE},
                         {&kAdd, PREC_REDUCE},
                         {&kRel, PREC_REDUCE},
                         {&kEq, PREC_REDUCE},
                         {&kEnd, PREC_REDUCE}};
  ExpectRow(TERMINAL_MUL, row);
  ExpectRow(TERMINAL_DIV, row);
}

TEST(ExprEdgeKnownGapTest, DISABLED_AdditiveRowsShiftOnlyMultiplicative) {
  // `++` (TERMINAL_CONCAT) shares the row of `+` (spec 5.8).
  const RowCell row[] = {{&kMul, PREC_TAKE},
                         {&kAdd, PREC_REDUCE},
                         {&kRel, PREC_REDUCE},
                         {&kEq, PREC_REDUCE},
                         {&kEnd, PREC_REDUCE}};
  ExpectRow(TERMINAL_PLUS, row);
  ExpectRow(TERMINAL_MINUS, row);
  ExpectRow(TERMINAL_CONCAT, row);
}

TEST(ExprEdgeKnownGapTest, DISABLED_RelationalRowShiftsArithmetic) {
  const RowCell row[] = {
      {&kMul, PREC_TAKE}, {&kAdd, PREC_TAKE}, {&kEnd, PREC_REDUCE}};
  ExpectRow(TERMINAL_REL, row);
  ExpectRow(TERMINAL_EQUAL, row);
}

TEST(ExprEdgeKnownGapTest, DISABLED_DollarRowShiftsEveryExpressionStart) {
  // $E with any operator shifts: the top terminal is $ even after the first
  // operand was pushed; a leading operator is rejected by the driver.
  const RowCell row[] = {{&kMul, PREC_TAKE},
                         {&kAdd, PREC_TAKE},
                         {&kRel, PREC_TAKE},
                         {&kEq, PREC_TAKE}};
  ExpectRow(TERMINAL_DOLLAR, row);
}

/*
 * AI-generated test block — whole-program expression helpers.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Parse in-memory IFJ26 programs with p_program and print the AST of one
 *   expression as a fully parenthesised string, e.g. "(a + (b * c))", so
 *   precedence and associativity can be asserted once p_expression exists.
 */
namespace {

// Programs in this file have no `import ifj26.*` prolog, like every other
// parser test: p_program does not parse it yet (ParserBugTest.
// ProgramWithPrologIsAccepted). Set this to "import ifj26.*\n" after that fix.
const char *const kProlog = "";
// The parser itself reports a missing @main (code 3, spec 2), so every
// program starts with this entry function (kEntryLines lines) before f.
const char *const kEntry = "@main\ndef entry(): Unit =\n  ()\n";
constexpr size_t kEntryLines = 3U;
const char *const kHeader =
    "def f(a: Int, b: Int, c: Int, d: Int, x: Double, s: String) =\n";
const char *const kUnitHeader =
    "def f(a: Int, b: Int, c: Int, d: Int, x: Double, s: String): Unit =\n";

/** Prolog and entry function followed by @p functions. */
std::string ProgramSource(const std::string &functions) {
  return std::string(kProlog) + kEntry + functions;
}

std::string InitializerSource(const std::string &expression) {
  return ProgramSource(kHeader + std::string("  val r = ") + expression + "\n");
}

std::string ConditionSource(const std::string &expression) {
  return ProgramSource(kUnitHeader + std::string("  if ") + expression +
                       " then\n    ()\n  else\n    ()\n");
}

ParserStatus ParseInto(const std::string &source, Program *program) {
  std::vector<char> buffer(source.c_str(), source.c_str() + source.size() + 1);
  Lexer lexer;
  ParserStatus status = PARSER_INTERNAL_ERROR;
  if (lexer_init(&lexer, buffer.data()) == LEXER_OK) {
    status = p_program(&lexer, program);
    lexer_destroy(&lexer);
  }
  return status;
}

class ParsedProgram {
public:
  explicit ParsedProgram(const std::string &source) {
    status_ = program_init(&program_) == PARSER_OK
                  ? ParseInto(source, &program_)
                  : PARSER_INTERNAL_ERROR;
  }
  ~ParsedProgram() { program_destroy(&program_); }
  ParsedProgram(const ParsedProgram &) = delete;
  ParsedProgram &operator=(const ParsedProgram &) = delete;

  ParserStatus status() const { return status_; }
  Program *program() { return &program_; }

  /** First statement of function f, or null when it is missing. */
  const ASTNode *FirstStatement() const {
    FunctionSymbol *f = function_table_lookup(&program_.functions, "f");
    if (f == nullptr || f->ast_root == nullptr ||
        f->ast_root->kind != AST_BLOCK ||
        f->ast_root->data.block.statement_count == 0U) {
      return nullptr;
    }
    return f->ast_root->data.block.statements[0];
  }
  /** Value of `val r = ...` (InitializerSource), or null. */
  const ASTNode *Initializer() const {
    const ASTNode *s = FirstStatement();
    return s != nullptr && s->kind == AST_ASSIGN ? s->data.assignment.value
                                                 : nullptr;
  }
  /** Condition of `if ... then` (ConditionSource), or null. */
  const ASTNode *Condition() const {
    const ASTNode *s = FirstStatement();
    return s != nullptr && s->kind == AST_IF ? s->data.if_statement.condition
                                             : nullptr;
  }

private:
  Program program_;
  ParserStatus status_;
};

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

std::string Sexpr(const ASTNode *node);

std::string CallSexpr(const ASTNode *node) {
  const FunctionSymbol *function = node->data.call.function;
  std::string text = function != nullptr ? function->name : "<null fn>";
  text += "(";
  for (size_t i = 0; i < node->data.call.argument_count; ++i) {
    text += (i == 0U ? "" : ", ") + Sexpr(node->data.call.arguments[i]);
  }
  return text + ")";
}

std::string LeafSexpr(const ASTNode *node) {
  char buffer[64];
  switch (node->kind) {
  case AST_CONSTANT_INT:
    return std::to_string(node->data.int_value);
  case AST_CONSTANT_REAL:
    snprintf(buffer, sizeof(buffer), "%g", node->data.real_value);
    return buffer;
  case AST_CONSTANT_STRING: {
    // The lexer keeps the quotes of the literal (LexerTest pins it).
    const std::string text = node->data.string_value;
    return !text.empty() && text[0] == '"' ? text : "\"" + text + "\"";
  }
  case AST_CONSTANT_UNIT:
    return "()";
  case AST_VARIABLE_REF:
    return node->data.variable.symbol != nullptr
               ? node->data.variable.symbol->name
               : "<null var>";
  case AST_FUNCTION_CALL:
    return CallSexpr(node);
  default:
    return "<kind " + std::to_string(node->kind) + ">";
  }
}

/** Fully parenthesised text of an expression AST (parentheses add no node).
 */
std::string Sexpr(const ASTNode *node) {
  if (node == nullptr) {
    return "<null>";
  }
  const char *op = OperatorSpelling(node->kind);
  if (op == nullptr) {
    return LeafSexpr(node);
  }
  return "(" + Sexpr(node->data.binary_op.left) + " " + op + " " +
         Sexpr(node->data.binary_op.right) + ")";
}

struct ShapeCase {
  const char *expression;
  const char *shape;
};

template <size_t N> void ExpectInitializerShapes(const ShapeCase (&cases)[N]) {
  for (const ShapeCase &c : cases) {
    SCOPED_TRACE(c.expression);
    ParsedProgram p(InitializerSource(c.expression));
    ASSERT_EQ(p.status(), PARSER_OK);
    EXPECT_EQ(Sexpr(p.Initializer()), c.shape);
  }
}

template <size_t N>
void ExpectStatus(const char *const (&expressions)[N],
                  std::string (*wrap)(const std::string &),
                  ParserStatus expected) {
  for (const char *expression : expressions) {
    SCOPED_TRACE(expression);
    EXPECT_EQ(ParsedProgram(wrap(expression)).status(), expected);
  }
}

} // namespace

/*
 * AI-generated test block — precedence and associativity (known gap).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   For every level of the spec ch. 3 operator table check through p_program
 *   that the AST groups operators by priority, that `* /` and `+ -` are left
 *   associative, that parentheses override priority without adding a node,
 *   and that each operator node carries the operator's line and column.
 *   Disabled: p_expression is a stub (parser.c:487).
 */
TEST(ExprEdgeKnownGapTest, DISABLED_MultiplicativeBindsTighterThanAdditive) {
  const ShapeCase cases[] = {
      {"a + b * c", "(a + (b * c))"},
      {"a * b + c", "((a * b) + c)"},
      {"a - b / c", "(a - (b / c))"},
      {"a / b - c * d", "((a / b) - (c * d))"},
      {"a + b * c - d", "((a + (b * c)) - d)"},
  };
  ExpectInitializerShapes(cases);
}

TEST(ExprEdgeKnownGapTest, DISABLED_SameLevelOperatorsAssociateLeft) {
  const ShapeCase cases[] = {
      {"a - b - c", "((a - b) - c)"},           {"a - b + c", "((a - b) + c)"},
      {"a + b - c", "((a + b) - c)"},           {"a / b * c", "((a / b) * c)"},
      {"a * b / c", "((a * b) / c)"},           {"a / b / c", "((a / b) / c)"},
      {"a - b - c - d", "(((a - b) - c) - d)"},
  };
  ExpectInitializerShapes(cases);
}

TEST(ExprEdgeKnownGapTest, DISABLED_ParenthesesOverridePriorityWithoutNode) {
  const ShapeCase cases[] = {
      {"(a + b) * c", "((a + b) * c)"},
      {"a * (b + c)", "(a * (b + c))"},
      {"a - (b - c)", "(a - (b - c))"},
      {"a / (b / c)", "(a / (b / c))"},
      {"(a)", "a"},
      {"((a))", "a"},
      {"(1)", "1"},
      {"((a + b))", "(a + b)"},
      {"(a + (b * (c - d)))", "(a + (b * (c - d)))"},
  };
  ExpectInitializerShapes(cases);
}

TEST(ExprEdgeKnownGapTest, DISABLED_LiteralOperandsKeepTheirValues) {
  // Shapes only; operand types are the semantic pass's business.
  const ShapeCase cases[] = {
      {"7 - 2.5", "(7 - 2.5)"},
      {"x * 0.5 + 1", "((x * 0.5) + 1)"},
      {"\"ab\" + s", "(\"ab\" + s)"},
      {"0 / 0", "(0 / 0)"}, // division by zero is a run-time error only
  };
  ExpectInitializerShapes(cases);
}

TEST(ExprEdgeKnownGapTest, DISABLED_OperatorNodeCarriesOperatorPosition) {
  // "  val r = a * b + c": `*` at column 13, `+` at column 17 (ast.h:269).
  ParsedProgram p(InitializerSource("a * b + c"));
  ASSERT_EQ(p.status(), PARSER_OK);
  const ASTNode *root = p.Initializer();
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->kind, AST_ADD);
  EXPECT_EQ(root->line, 2U + kEntryLines);
  EXPECT_EQ(root->column, 17U);
  const ASTNode *left = root->data.binary_op.left;
  ASSERT_NE(left, nullptr);
  ASSERT_EQ(left->kind, AST_MULTIPLY);
  EXPECT_EQ(left->line, 2U + kEntryLines);
  EXPECT_EQ(left->column, 13U);
}

/*
 * AI-generated test block — relational operators (known gap).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Relational operators have the lowest priority (spec ch. 3, levels 4-5)
 *   and no associativity: same-level chains are syntax errors; mixed-level
 *   chains must be rejected by parser or semantic pass ("Řetězení relačních
 *   operátorů není povoleno"). Use `if` conditions, where Boolean is legal.
 */
TEST(ExprEdgeKnownGapTest, DISABLED_RelationalOperatorsBindLoosest) {
  const ShapeCase cases[] = {
      {"a < b", "(a < b)"},
      {"a <= b", "(a <= b)"},
      {"a > b", "(a > b)"},
      {"a >= b", "(a >= b)"},
      {"a == b", "(a == b)"},
      {"a != b", "(a != b)"},
      {"a + b < c * d", "((a + b) < (c * d))"},
      {"a - 1 == b / 2", "((a - 1) == (b / 2))"},
      {"(a + b) != c", "((a + b) != c)"},
      {"s <= \"z\"", "(s <= \"z\")"},
  };
  for (const ShapeCase &c : cases) {
    SCOPED_TRACE(c.expression);
    ParsedProgram p(ConditionSource(c.expression));
    ASSERT_EQ(p.status(), PARSER_OK);
    EXPECT_EQ(Sexpr(p.Condition()), c.shape);
  }
}

// Spec 3: levels 4 and 5 are "bez asociativity" -> no table relation -> 2.
// Passes vacuously today (no `if` condition is parsed at all); it becomes
// meaningful together with DISABLED_RelationalOperatorsBindLoosest.
TEST(ExprEdgeKnownGapTest, DISABLED_SameLevelRelationalChainIsSyntaxError) {
  const char *const chains[] = {"a < b < c",    "a <= b >= c", "a > b > c",
                                "a == b == c",  "a != b == c", "a == b != c",
                                "a < b < c < d"};
  ExpectStatus(chains, ConditionSource, PARSER_SYNTAX_ERROR);
}

// The exit code of a mixed-level chain is open (spec Nejasnosti 6: 2 or 6),
// but the program must not be accepted by the parser + semantic pipeline.
TEST(ExprEdgeKnownGapTest, DISABLED_MixedLevelRelationalChainIsRejected) {
  const char *const chains[] = {"a < b == c", "a == b < c", "a != b >= c"};
  {
    ParsedProgram control(ConditionSource("a < b"));
    ASSERT_EQ(control.status(), PARSER_OK);
    ASSERT_EQ(semantic_check(control.program()), SEMANTIC_OK);
  }
  for (const char *chain : chains) {
    SCOPED_TRACE(chain);
    ParsedProgram p(ConditionSource(chain));
    if (p.status() == PARSER_OK) {
      EXPECT_NE(semantic_check(p.program()), SEMANTIC_OK);
    }
  }
}

/*
 * AI-generated test block — unary minus (known gap).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Unary minus has the highest priority and right associativity (spec 3);
 *   the AST has no negation kind yet, so check only acceptance and the kind
 *   of the root operator, whatever node represents `-operand`.
 */
TEST(ExprEdgeKnownGapTest, DISABLED_UnaryMinusIsAcceptedEverywhere) {
  const char *const expressions[] = {"-a",       "- - a",  "-(-a)",  "-x",
                                     "-(a + b)", "a - -b", "a * -b", "-a * -b",
                                     "(-a)",     "-1"};
  ExpectStatus(expressions, InitializerSource, PARSER_OK);
}

TEST(ExprEdgeKnownGapTest, DISABLED_UnaryMinusBindsTighterThanBinary) {
  struct Case {
    const char *expression;
    ASTNodeKind root;
    const char *untouched; // binary operand that must stay a plain leaf
  };
  const Case cases[] = {
      {"-a * b", AST_MULTIPLY, nullptr},
      {"a * -b", AST_MULTIPLY, "a"},
      {"-a + b", AST_ADD, nullptr},
      {"a - -b", AST_SUBTRACT, "a"},
      {"a / -b", AST_DIVIDE, "a"},
      {"-(a + b) * c", AST_MULTIPLY, nullptr},
  };
  for (const Case &c : cases) {
    SCOPED_TRACE(c.expression);
    ParsedProgram p(InitializerSource(c.expression));
    ASSERT_EQ(p.status(), PARSER_OK);
    const ASTNode *root = p.Initializer();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->kind, c.root);
    if (c.untouched != nullptr) {
      EXPECT_EQ(Sexpr(root->data.binary_op.left), c.untouched);
    }
  }
  ParsedProgram cond(ConditionSource("-a < b"));
  ASSERT_EQ(cond.status(), PARSER_OK);
  ASSERT_NE(cond.Condition(), nullptr);
  EXPECT_EQ(cond.Condition()->kind, AST_LESS);
}

/*
 * AI-generated test block — malformed expressions (known gap).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Unbalanced parentheses, two operands without an operator, an operator
 *   at the start or end of an expression, two binary operators in a row and
 *   an unfinished expression continued on the next logical line are syntax
 *   errors (code 2); `()` alone is the Unit literal (spec 1.1, 2.1).
 */
TEST(ExprEdgeKnownGapTest, DISABLED_UnbalancedParenthesesAreSyntaxError) {
  const char *const expressions[] = {"(a + b",  "a + b)", "((a)", "(a))",
                                     ")a(",     "(",      ")",    "a * (b + c",
                                     "(a +) b", "(()"};
  ExpectStatus(expressions, InitializerSource, PARSER_SYNTAX_ERROR);
}

// Replaces the get_precedence cells `(` x end token, whose terminal was
// removed in ee9d509: a `(` still open when the expression ends at `then`,
// `do`, `;` or the end of the line is a syntax error.
TEST(ExprEdgeTest, UnclosedLeftParenAtExpressionEndIsSyntaxError) {
  const char *const conditions[] = {"(a < b", "((a < b)", "(a + b < c"};
  ExpectStatus(conditions, ConditionSource, PARSER_SYNTAX_ERROR);
  const char *const sources[] = {
      "def f(a: Int): Unit =\n  while (a < 1 do\n    ()\n",
      "def f(a: Int): Unit =\n  val r = (a + 1; val q = a\n",
      "def f(a: Int): Unit =\n  val r = (a\n  val q = a\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(ParsedProgram(ProgramSource(source)).status(),
              PARSER_SYNTAX_ERROR);
  }
}

TEST(ExprEdgeKnownGapTest, DISABLED_EmptyParenthesesAreTheUnitLiteral) {
  const char *const sources[] = {
      "def f() =\n  val u: Unit = ()\n",
      "def f() =\n  val u: Unit = ( )\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    ParsedProgram p(ProgramSource(source));
    ASSERT_EQ(p.status(), PARSER_OK);
    EXPECT_EQ(Sexpr(p.Initializer()), "()");
  }
  ParsedProgram statement(ProgramSource("def f(): Unit =\n  ()\n"));
  ASSERT_EQ(statement.status(), PARSER_OK);
  ASSERT_NE(statement.FirstStatement(), nullptr);
  EXPECT_EQ(statement.FirstStatement()->kind, AST_CONSTANT_UNIT);
}

// `(a) (b)` is left out: LISTS (spec 5.8) indexes with call syntax, so it is
// the indexing of `(a)` by `b`, a syntactically valid expression.
TEST(ExprEdgeKnownGapTest, DISABLED_AdjacentOperandsAreSyntaxError) {
  const char *const expressions[] = {"a b",   "1 2",   "a 1",     "\"t\" s",
                                     "1.5 x", "(a) b", "a + b c", "a b + c"};
  ExpectStatus(expressions, InitializerSource, PARSER_SYNTAX_ERROR);
}

TEST(ExprEdgeKnownGapTest, DISABLED_MisplacedBinaryOperatorIsSyntaxError) {
  const char *const expressions[] = {
      "* a",      "/ a",   "== a",  "< a",      "++ a",    "a *",
      "a +",      "a -",   "a <",   "a ++",     "a + * b", "a * / b",
      "a < == b", "(* a)", "(a *)", "a + (/ b)"};
  ExpectStatus(expressions, InitializerSource, PARSER_SYNTAX_ERROR);
}

// Spec 1.2: "Neukončený výraz nemůže pokračovat na dalším logickém řádku".
TEST(ExprEdgeKnownGapTest, DISABLED_UnfinishedExpressionCannotContinue) {
  const char *const expressions[] = {"a +\n    b", "a *\n  b", "(a + b) -\n"};
  ExpectStatus(expressions, InitializerSource, PARSER_SYNTAX_ERROR);
}

// Spec 1.2: LF inside parentheses is whitespace, but the next physical line
// must be indented more than the logical line (here 2 spaces).
TEST(ExprEdgeKnownGapTest, DISABLED_LineBreakInsideParenthesesIsWhitespace) {
  const ShapeCase cases[] = {
      {"(a +\n    b)", "(a + b)"},
      {"(a\n   + b)", "(a + b)"},
      {"(a +\n   (b *\n      c))", "(a + (b * c))"},
  };
  ExpectInitializerShapes(cases);
  const char *const badly_indented[] = {"(a +\n  b)", "(a +\nb)"};
  ExpectStatus(badly_indented, InitializerSource, PARSER_SYNTAX_ERROR);
}

/*
 * AI-generated test block — where an expression ends (known gap).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   An expression must stop, without consuming it, at NEWLINE, end of input,
 *   `then`, the `)` or `,` of a call, and the DEDENT closing a block.
 */
// Crashes today (SIGSEGV): an expression statement yields a NULL node that
// p_code_block dereferences, see ParserGapKnownGapTest.
// DISABLED_ExpressionStatementIsAccepted (parser.c:870).
TEST(ExprEdgeKnownGapTest, DISABLED_ExpressionStopsAtEveryTerminator) {
  const char *const sources[] = {
      "def f(a: Int) =\n  val r = a + 1\n  val q = r * 2\n", // NEWLINE
      "def f(a: Int) =\n  val r = a + 1",                    // EOF
      "def f(a: Int): Int =\n  return a * 2\n",              // NEWLINE
      "def f(a: Int): Int =\n  return a * 2",                // EOF
      "def f(a: Int) =\n  if a < 1 then\n    val r = a + 1\n  else\n"
      "    val q = a - 1\n  ()\n", // DEDENT
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(ParsedProgram(ProgramSource(source)).status(), PARSER_OK);
  }
  ParsedProgram call(ProgramSource("def f(a: Int) =\n  g(a + 1, a * 2)\n"
                                   "def g(x: Int, y: Int) =\n  x\n"));
  ASSERT_EQ(call.status(), PARSER_OK);
  EXPECT_EQ(Sexpr(call.FirstStatement()), "g((a + 1), (a * 2))");
}

/*
 * AI-generated test block — function calls inside expressions (known gap).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   FUNEXP (spec 5.1, implemented by p_operand/p_call_args): a call with 0,
 *   1 or more arguments is an operand of any operator, arguments are whole
 *   expressions and calls nest; malformed calls are syntax errors.
 */
TEST(ExprEdgeKnownGapTest, DISABLED_CallsAreOperands) {
  const ShapeCase cases[] = {
      {"g() + 1", "(g() + 1)"},
      {"1 + g(a)", "(1 + g(a))"},
      {"g(a, b, c) * 2", "(g(a, b, c) * 2)"},
      {"g(h(a), h(h(b)))", "g(h(a), h(h(b)))"},
      {"g(a + b * c)", "g((a + (b * c)))"},
      {"(g(a) - 1) / 2", "((g(a) - 1) / 2)"},
      {"g((a + b)) * c", "(g((a + b)) * c)"},
      {"g(a) * g(b) + g(c)", "((g(a) * g(b)) + g(c))"},
      {"-g(a) * 0 + g(-a)", nullptr},
  };
  for (const ShapeCase &c : cases) {
    SCOPED_TRACE(c.expression);
    ParsedProgram p(InitializerSource(c.expression));
    ASSERT_EQ(p.status(), PARSER_OK);
    if (c.shape != nullptr) {
      EXPECT_EQ(Sexpr(p.Initializer()), c.shape);
    }
  }
}

TEST(ExprEdgeKnownGapTest, DISABLED_MalformedCallInExpressionIsSyntaxError) {
  // `g(a,)`: a trailing comma needs a line break before `)` (spec 1.2).
  const char *const expressions[] = {"g(a",    "g(a,) + 1", "g(, a)",
                                     "g(a b)", "g(a +)",    "1 + g(",
                                     "g(a))",  "g(a,, b)"};
  ExpectStatus(expressions, InitializerSource, PARSER_SYNTAX_ERROR);
}

/*
 * AI-generated test block — long chains and deep nesting (known gap).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Long operator chains and deeply nested parentheses must parse (the PSA
 *   stack grows) into a left-deep resp. right-deep tree of the right size.
 */
namespace {

constexpr size_t kChainOperands = 2000U;
constexpr size_t kNestingDepth = 500U;

/** Follows @p left (or right) children through @p kind nodes; returns depth.
 */
size_t SpineDepth(const ASTNode *node, ASTNodeKind kind, bool left) {
  size_t depth = 0U;
  while (node != nullptr && node->kind == kind) {
    const ASTNode *leaf =
        left ? node->data.binary_op.right : node->data.binary_op.left;
    if (leaf == nullptr || leaf->kind != AST_VARIABLE_REF) {
      return depth;
    }
    node = left ? node->data.binary_op.left : node->data.binary_op.right;
    ++depth;
  }
  return node != nullptr && node->kind == AST_VARIABLE_REF ? depth : 0U;
}

} // namespace

TEST(ExprEdgeKnownGapTest, DISABLED_LongChainBuildsLeftDeepTree) {
  std::string chain = "a";
  for (size_t i = 1; i < kChainOperands; ++i) {
    chain += " - a";
  }
  ParsedProgram p(InitializerSource(chain));
  ASSERT_EQ(p.status(), PARSER_OK);
  EXPECT_EQ(SpineDepth(p.Initializer(), AST_SUBTRACT, true),
            kChainOperands - 1U);
}

TEST(ExprEdgeKnownGapTest, DISABLED_DeepNestingBuildsRightDeepTree) {
  std::string nested = "a";
  for (size_t i = 0; i < kNestingDepth; ++i) {
    nested = "a / (" + nested + ")";
  }
  ParsedProgram p(InitializerSource(nested));
  ASSERT_EQ(p.status(), PARSER_OK);
  EXPECT_EQ(SpineDepth(p.Initializer(), AST_DIVIDE, false), kNestingDepth);

  const std::string parens =
      std::string(kNestingDepth, '(') + "a" + std::string(kNestingDepth, ')');
  ParsedProgram q(InitializerSource(parens));
  ASSERT_EQ(q.status(), PARSER_OK);
  EXPECT_EQ(Sexpr(q.Initializer()), "a");
  ParsedProgram unbalanced(InitializerSource("(" + parens));
  EXPECT_EQ(unbalanced.status(), PARSER_SYNTAX_ERROR);
}

/*
 * AI-generated test block — operator cannot start a statement.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Spec 1.2: "Logický řádek nemůže začínat operátorem." A statement line
 *   that starts with a binary-only operator or `)` is a syntax error today
 *   (p_statement default branch); unary `-`, `!` and `+` are left out.
 */
TEST(ExprEdgeTest, BinaryOperatorCannotStartStatement) {
  const char *const lines[] = {"* a",  "/ a", "== a", "!= a", "< a",
                               "<= a", "> a", ">= a", "++ a", ") a"};
  for (const char *line : lines) {
    SCOPED_TRACE(line);
    const std::string source =
        ProgramSource("def f(a: Int) =\n  " + std::string(line) + "\n");
    EXPECT_EQ(ParsedProgram(source).status(), PARSER_SYNTAX_ERROR);
  }
}
