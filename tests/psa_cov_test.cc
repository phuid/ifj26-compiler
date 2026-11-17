#include <cstdint>
#include <cstdlib>

#include "gtest/gtest.h"
// psa.h has no include guard and is force-included (C linkage) by the build.

/*
 * AI-generated test block — TerminalStack growth limits.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Cover terminal_stack_push when the stack cannot grow: size already at
 *   SIZE_MAX and a capacity whose byte size would overflow size_t. Set the
 *   public fields directly (no allocation happens) and check that the push
 *   fails and leaves size, capacity, storage and the stored element intact.
 */
namespace {

// The stack borrows one local Terminal as its storage and claims a huge
// size/capacity. This is safe only because of the dynamic_array.h contract
// ("arithmetic overflow is reported instead of wrapping"): the overflow of
// new_capacity * sizeof(Terminal) is detected before realloc is called, so
// realloc never sees the local address and nothing is written. If that
// contract breaks, realloc gets a stack address and the binary crashes, which
// still reports the regression.
struct FrozenStack {
  Terminal storage = {TERMINAL_DOLLAR, nullptr};
  TerminalStack stack = {&storage, 0U, 0U};

  void Expect(size_t size, size_t capacity) const {
    EXPECT_EQ(stack.data, &storage);
    EXPECT_EQ(stack.size, size);
    EXPECT_EQ(stack.capacity, capacity);
    EXPECT_EQ(storage.type, TERMINAL_DOLLAR);
    EXPECT_EQ(storage.node, nullptr);
  }
};

} // namespace

TEST(PsaCovTerminalStackLimitTest, PushAtMaximumSizeFailsWithoutChangingStack) {
  FrozenStack frozen;
  frozen.stack.size = SIZE_MAX;
  frozen.stack.capacity = SIZE_MAX;
  const Terminal terminal = {TERMINAL_PLUS, nullptr};
  EXPECT_EQ(terminal_stack_push(&frozen.stack, terminal),
            PARSER_INTERNAL_ERROR);
  frozen.Expect(SIZE_MAX, SIZE_MAX);
}

TEST(PsaCovTerminalStackLimitTest,
     PushFailsWhenGrownCapacityWouldOverflowBytes) {
  // SIZE_MAX / sizeof(Terminal): doubling exceeds the byte limit;
  // SIZE_MAX - 1: doubling would wrap, so the new capacity is `required`.
  // Both are rejected by dynamic_array_reserve before realloc (see above).
  const size_t sizes[] = {SIZE_MAX / sizeof(Terminal), SIZE_MAX - 1U};
  for (const size_t size : sizes) {
    SCOPED_TRACE(size);
    FrozenStack frozen;
    frozen.stack.size = size;
    frozen.stack.capacity = size;
    const Terminal terminal = {TERMINAL_MUL, nullptr};
    EXPECT_EQ(terminal_stack_push(&frozen.stack, terminal),
              PARSER_INTERNAL_ERROR);
    frozen.Expect(size, size);
  }
}

/*
 * AI-generated test block — TerminalStack value semantics.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Push stores the whole Terminal (type and node pointer, which the stack
 *   neither owns nor dereferences); peek leaves the stack as it is; an
 *   emptied stack accepts new pushes. Storage comes from realloc: free() it.
 */
TEST(PsaCovTerminalStackValueTest, PushKeepsNodePointerAndPeekKeepsTerminal) {
  ASTNode fake_node = {};
  ASTNode *const node = &fake_node;
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PARSER_OK);
  const Terminal terminal = {TERMINAL_OPERAND, node};
  ASSERT_EQ(terminal_stack_push(&stack, terminal), PARSER_OK);

  Terminal out = {TERMINAL_INVALID, nullptr};
  ASSERT_EQ(terminal_stack_peek(&stack, &out), PARSER_OK);
  EXPECT_EQ(out.type, TERMINAL_OPERAND);
  EXPECT_EQ(out.node, node);
  ASSERT_EQ(terminal_stack_peek(&stack, &out), PARSER_OK);
  EXPECT_EQ(stack.size, 1U);

  out = {TERMINAL_INVALID, nullptr};
  ASSERT_EQ(terminal_stack_pop(&stack, &out), PARSER_OK);
  EXPECT_EQ(out.type, TERMINAL_OPERAND);
  EXPECT_EQ(out.node, node);
  EXPECT_EQ(stack.size, 0U);
  free(stack.data);
}

TEST(PsaCovTerminalStackValueTest, EmptiedStackAcceptsNewTerminals) {
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PARSER_OK);
  Terminal out;
  for (int round = 0; round < 2; ++round) {
    SCOPED_TRACE(round);
    ASSERT_EQ(terminal_stack_push(&stack, {TERMINAL_DOLLAR, nullptr}),
              PARSER_OK);
    ASSERT_EQ(terminal_stack_push(&stack, {TERMINAL_LPAREN, nullptr}),
              PARSER_OK);
    ASSERT_EQ(terminal_stack_pop(&stack, &out), PARSER_OK);
    EXPECT_EQ(out.type, TERMINAL_LPAREN);
    ASSERT_EQ(terminal_stack_pop(&stack, &out), PARSER_OK);
    EXPECT_EQ(out.type, TERMINAL_DOLLAR);
    EXPECT_EQ(stack.size, 0U);
  }
  free(stack.data);
}

/*
 * AI-generated test block — get_precedence table.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Check the operator-precedence relation (stack-top terminal vs. input
 *   token) for parentheses, operands, expression delimiters and relational
 *   operators, grouping tokens as psa.c's token_to_terminal does. Tokens that
 *   cannot appear in an expression have no relation with any stack top.
 *   Leave out choices the team has not made yet.
 */
// Deliberately not tested (open team decisions, specifikace.txt has no
// precedence table): precedence/associativity of `++` and associativity of
// relational operators; whether the `)` closing a call argument is handled by
// the ($, `)`) table entry or by the driver loop; whether `=`, `=>` or `if`
// may occur inside p_expression; TerminalType has no terminal for `!`, `&&`,
// `||` or unary `-` although parser.c/the lexer produce them
// (token_to_terminal maps them to TERMINAL_INVALID); whether `[` may start an
// operand.
namespace {

struct PrecCase {
  TerminalType top;
  TokenType next;
  PRECEDENCE expected;
};

void ExpectRelations(const PrecCase *cases, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    EXPECT_EQ(get_precedence(cases[i].top, cases[i].next), cases[i].expected)
        << "top " << cases[i].top << ", next " << cases[i].next;
  }
}

const TokenType kOperandTokens[] = {TOKEN_INT, TOKEN_DOUBLE, TOKEN_STRING,
                                    TOKEN_IDENTIFIER};
const TokenType kRelationalTokens[] = {TOKEN_EQUAL,      TOKEN_NOT_EQUAL,
                                       TOKEN_LESS,       TOKEN_GREATER,
                                       TOKEN_LESS_EQUAL, TOKEN_GREATER_EQUAL};
const TokenType kDelimiterTokens[] = {
    TOKEN_EOF,   TOKEN_NEWLINE,       TOKEN_SEMICOLON, TOKEN_COMMA,
    TOKEN_COLON, TOKEN_RIGHT_BRACKET, TOKEN_DEDENT,    TOKEN_KW_THEN,
    TOKEN_KW_DO, TOKEN_KW_ELSE};
const TerminalType kArithmeticTerminals[] = {TERMINAL_PLUS, TERMINAL_MINUS,
                                             TERMINAL_MUL, TERMINAL_DIV};

} // namespace

// Guard test: with today's stub (always PREC_INVALID) it passes trivially and
// adds no coverage; it becomes meaningful once the precedence table is
// implemented (default -> TERMINAL_INVALID, HANDLE/INVALID tops).
TEST(PsaCovPrecedenceTest, NonExpressionTokensHaveNoRelation) {
  const TokenType foreign[] = {TOKEN_KW_DEF, TOKEN_KW_IMPORT, TOKEN_KW_CLASS,
                               TOKEN_MAIN_ANNOTATION, TOKEN_INVALID};
  const TerminalType tops[] = {TERMINAL_DOLLAR, TERMINAL_PLUS,
                               TERMINAL_LPAREN, TERMINAL_OPERAND,
                               TERMINAL_HANDLE, TERMINAL_INVALID};
  for (const TerminalType top : tops) {
    for (const TokenType next : foreign) {
      EXPECT_EQ(get_precedence(top, next), PREC_INVALID)
          << "top " << top << ", next " << next;
    }
  }
}

// BUG: psa.c:110 – get_precedence is an unfinished stub (psa.h @todo) that
// always returns PREC_INVALID; expectations follow the usual IFJ precedence
// table, not specifikace.txt (which has none).
TEST(PsaCovPrecedenceTest, DISABLED_ParenthesesMatchAndNestAroundOperators) {
  const PrecCase cases[] = {
      {TERMINAL_LPAREN, TOKEN_RIGHT_PAREN, PREC_EQUAL},
      {TERMINAL_LPAREN, TOKEN_LEFT_PAREN, PREC_TAKE},
      {TERMINAL_LPAREN, TOKEN_INT, PREC_TAKE},
      {TERMINAL_LPAREN, TOKEN_PLUS, PREC_TAKE},
      {TERMINAL_PLUS, TOKEN_LEFT_PAREN, PREC_TAKE},
      {TERMINAL_MUL, TOKEN_LEFT_PAREN, PREC_TAKE},
      {TERMINAL_PLUS, TOKEN_RIGHT_PAREN, PREC_REDUCE},
      {TERMINAL_OPERAND, TOKEN_RIGHT_PAREN, PREC_REDUCE},
      {TERMINAL_RPAREN, TOKEN_MULTIPLY, PREC_REDUCE},
      {TERMINAL_RPAREN, TOKEN_RIGHT_PAREN, PREC_REDUCE},
      {TERMINAL_LPAREN, TOKEN_EOF, PREC_INVALID},
  };
  ExpectRelations(cases, sizeof(cases) / sizeof(cases[0]));
}

// BUG: psa.c:110 – get_precedence is an unfinished stub (psa.h @todo) that
// always returns PREC_INVALID; expectations follow the usual IFJ precedence
// table, not specifikace.txt (which has none).
TEST(PsaCovPrecedenceTest, DISABLED_OperandsAreShiftedAndReducedAtDelimiters) {
  for (const TokenType operand : kOperandTokens) {
    EXPECT_EQ(get_precedence(TERMINAL_DOLLAR, operand), PREC_TAKE) << operand;
    for (const TerminalType op : kArithmeticTerminals) {
      EXPECT_EQ(get_precedence(op, operand), PREC_TAKE)
          << "top " << op << ", next " << operand;
    }
  }
  EXPECT_EQ(get_precedence(TERMINAL_DOLLAR, TOKEN_LEFT_PAREN), PREC_TAKE);
  for (const TokenType end : kDelimiterTokens) {
    EXPECT_EQ(get_precedence(TERMINAL_OPERAND, end), PREC_REDUCE) << end;
    EXPECT_EQ(get_precedence(TERMINAL_RPAREN, end), PREC_REDUCE) << end;
    EXPECT_EQ(get_precedence(TERMINAL_MINUS, end), PREC_REDUCE) << end;
  }
}

// BUG: psa.c:110 – get_precedence is an unfinished stub (psa.h @todo) that
// always returns PREC_INVALID; expectations follow the usual IFJ precedence
// table, not specifikace.txt (which has none).
TEST(PsaCovPrecedenceTest,
     DISABLED_RelationalOperatorsBindLooserThanArithmetic) {
  for (const TokenType rel : kRelationalTokens) {
    for (const TerminalType op : kArithmeticTerminals) {
      EXPECT_EQ(get_precedence(op, rel), PREC_REDUCE)
          << "top " << op << ", next " << rel;
    }
    EXPECT_EQ(get_precedence(TERMINAL_OPERAND, rel), PREC_REDUCE) << rel;
  }
  const TokenType after_rel[] = {TOKEN_PLUS,   TOKEN_MINUS, TOKEN_MULTIPLY,
                                 TOKEN_DIVIDE, TOKEN_INT,   TOKEN_LEFT_PAREN};
  for (const TokenType next : after_rel) {
    EXPECT_EQ(get_precedence(TERMINAL_REL, next), PREC_TAKE) << next;
  }
}
