#include <cstdlib>

#include "gtest/gtest.h"
// psa.h has no include guard and is force-included (C linkage) by the build.

/*
 * AI-generated test block — TerminalStack.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover NULL arguments, the empty stack, LIFO order with peek and growth
 *   over many pushes. Storage comes from realloc, so tests free() it.
 */
TEST(PsaTerminalStackTest, NullArgumentsAndEmptyStackAreRejected) {
  Terminal terminal = {TERMINAL_PLUS, nullptr};
  EXPECT_EQ(terminal_stack_init(nullptr), PARSER_INVALID_ARGUMENT);
  EXPECT_EQ(terminal_stack_push(nullptr, terminal), PARSER_INVALID_ARGUMENT);
  EXPECT_EQ(terminal_stack_pop(nullptr, &terminal), PARSER_INVALID_ARGUMENT);
  EXPECT_EQ(terminal_stack_peek(nullptr, &terminal), PARSER_INVALID_ARGUMENT);
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PARSER_OK);
  EXPECT_EQ(stack.data, nullptr);
  EXPECT_EQ(stack.capacity, 0U);
  EXPECT_EQ(terminal_stack_pop(&stack, &terminal), PARSER_INVALID_ARGUMENT);
  EXPECT_EQ(terminal_stack_peek(&stack, &terminal), PARSER_INVALID_ARGUMENT);
  EXPECT_EQ(terminal.type, TERMINAL_PLUS);
  ASSERT_EQ(terminal_stack_push(&stack, terminal), PARSER_OK);
  EXPECT_EQ(terminal_stack_pop(&stack, nullptr), PARSER_INVALID_ARGUMENT);
  EXPECT_EQ(terminal_stack_peek(&stack, nullptr), PARSER_INVALID_ARGUMENT);
  EXPECT_EQ(stack.size, 1U);
  free(stack.data);
}

TEST(PsaTerminalStackTest, GrowsAndReturnsTerminalsInLifoOrder) {
  constexpr size_t kPushCount = 1000U;
  constexpr size_t kTypeCount = static_cast<size_t>(TERMINAL_INVALID);
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PARSER_OK);
  for (size_t i = 0; i < kPushCount; ++i) {
    Terminal terminal = {static_cast<TerminalType>(i % kTypeCount), nullptr};
    ASSERT_EQ(terminal_stack_push(&stack, terminal), PARSER_OK);
    ASSERT_GE(stack.capacity, stack.size);
  }
  Terminal out;
  for (size_t i = kPushCount; i > 0U; --i) {
    const auto expected = static_cast<TerminalType>((i - 1U) % kTypeCount);
    ASSERT_EQ(terminal_stack_peek(&stack, &out), PARSER_OK);
    ASSERT_EQ(out.type, expected);
    ASSERT_EQ(stack.size, i);
    ASSERT_EQ(terminal_stack_pop(&stack, &out), PARSER_OK);
    ASSERT_EQ(out.type, expected);
  }
  EXPECT_EQ(terminal_stack_pop(&stack, &out), PARSER_INVALID_ARGUMENT);
  free(stack.data);
}

/*
 * AI-generated test block — get_precedence.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Adjacent operands and `)` before `(` have no relation; `*` over `+`
 *   and left associativity await the real table (stub returns INVALID).
 */
TEST(PsaPrecedenceTest, AdjacentOperandsHaveNoRelation) {
  EXPECT_EQ(get_precedence(TERMINAL_OPERAND, TOKEN_INT), PREC_INVALID);
  EXPECT_EQ(get_precedence(TERMINAL_OPERAND, TOKEN_IDENTIFIER), PREC_INVALID);
  EXPECT_EQ(get_precedence(TERMINAL_RPAREN, TOKEN_LEFT_PAREN), PREC_INVALID);
}

// Needs get_precedence implementation (stub returns PREC_INVALID).
TEST(PsaPrecedenceTest, DISABLED_MultiplicativeFirstAndLeftAssociative) {
  struct Case { TerminalType top; TokenType next; PRECEDENCE expected; };
  const Case cases[] = {{TERMINAL_PLUS, TOKEN_MULTIPLY, PREC_TAKE},
                        {TERMINAL_MUL, TOKEN_PLUS, PREC_REDUCE},
                        {TERMINAL_PLUS, TOKEN_MINUS, PREC_REDUCE},
                        {TERMINAL_MUL, TOKEN_DIVIDE, PREC_REDUCE}};
  for (const Case &c : cases) {
    EXPECT_EQ(get_precedence(c.top, c.next), c.expected)
        << "top " << c.top << ", next " << c.next;
  }
}
