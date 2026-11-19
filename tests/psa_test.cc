#include <cstdlib>

#include "gtest/gtest.h"

extern "C" {
#include "../src/psa.h"
}

namespace {

Terminal MakeTerminal(TerminalType type, ASTNode *node = nullptr) {
  Terminal terminal = {};
  terminal.type = type;
  terminal.token.type = TOKEN_INVALID;
  terminal.node = node;
  return terminal;
}

Token MakeToken(TokenType type) {
  Token token = {};
  token.type = type;
  return token;
}

} // namespace

/*
 * AI-generated test block — TerminalStack.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover NULL arguments, the empty stack, push order and growth over many
 *   pushes, and the top-terminal lookup that skips operands and handles.
 *   Storage is released by terminal_stack_cleanup_nodes.
 */
TEST(PsaTerminalStackTest, NullArgumentsAndEmptyStackAreRejected) {
  Terminal terminal = MakeTerminal(TERMINAL_PLUS);
  EXPECT_EQ(terminal_stack_init(nullptr), PSA_INVALID_ARGUMENT);
  EXPECT_EQ(terminal_stack_push(nullptr, terminal), PSA_INVALID_ARGUMENT);
  EXPECT_EQ(stack_get_top_terminal(nullptr, &terminal), PSA_INVALID_ARGUMENT);
  EXPECT_EQ(terminal_stack_cleanup_nodes(nullptr), PSA_INVALID_ARGUMENT);
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PSA_OK);
  EXPECT_EQ(stack.data, nullptr);
  EXPECT_EQ(stack.capacity, 0U);
  EXPECT_EQ(stack_get_top_terminal(&stack, &terminal), PSA_INVALID_ARGUMENT);
  EXPECT_EQ(terminal.type, TERMINAL_PLUS);
  ASSERT_EQ(terminal_stack_push(&stack, terminal), PSA_OK);
  EXPECT_EQ(stack_get_top_terminal(&stack, nullptr), PSA_INVALID_ARGUMENT);
  EXPECT_EQ(stack.size, 1U);
  EXPECT_EQ(terminal_stack_cleanup_nodes(&stack), PSA_OK);
  EXPECT_EQ(stack.data, nullptr);
  EXPECT_EQ(stack.size, 0U);
}

TEST(PsaTerminalStackTest, GrowsAndKeepsTerminalsInPushOrder) {
  constexpr size_t kPushCount = 1000U;
  constexpr size_t kTypeCount = static_cast<size_t>(TERMINAL_COUNT);
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PSA_OK);
  for (size_t i = 0; i < kPushCount; ++i) {
    const auto type = static_cast<TerminalType>(i % kTypeCount);
    ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(type)), PSA_OK);
    ASSERT_GE(stack.capacity, stack.size);
    Terminal top;
    ASSERT_EQ(stack_get_top_terminal(&stack, &top), PSA_OK);
    ASSERT_EQ(top.type, type);
  }
  ASSERT_EQ(stack.size, kPushCount);
  for (size_t i = 0; i < kPushCount; ++i) {
    ASSERT_EQ(stack.data[i].type, static_cast<TerminalType>(i % kTypeCount));
  }
  EXPECT_EQ(terminal_stack_cleanup_nodes(&stack), PSA_OK);
}

TEST(PsaTerminalStackTest, TopTerminalSkipsOperandsAndHandles) {
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PSA_OK);
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_DOLLAR)), PSA_OK);
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_HANDLE)), PSA_OK);
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_OPERAND)),
            PSA_OK);
  Terminal top = MakeTerminal(TERMINAL_INVALID);
  ASSERT_EQ(stack_get_top_terminal(&stack, &top), PSA_OK);
  EXPECT_EQ(top.type, TERMINAL_DOLLAR);
  EXPECT_EQ(stack.size, 3U);
  EXPECT_EQ(terminal_stack_cleanup_nodes(&stack), PSA_OK);

  // A stack of operands and handles only has no top terminal.
  ASSERT_EQ(terminal_stack_init(&stack), PSA_OK);
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_HANDLE)), PSA_OK);
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_OPERAND)),
            PSA_OK);
  EXPECT_EQ(stack_get_top_terminal(&stack, &top), PSA_INVALID_ARGUMENT);
  EXPECT_EQ(terminal_stack_cleanup_nodes(&stack), PSA_OK);
}

/*
 * AI-generated test block — get_precedence.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Operand tokens are not table terminals and stack-only symbols have no
 *   relation; `*` binds tighter than `+` and both are left associative.
 */
TEST(PsaPrecedenceTest, OperandsAndStackOnlySymbolsHaveNoRelation) {
  EXPECT_EQ(token_to_terminal(MakeToken(TOKEN_INT)), TERMINAL_INVALID);
  EXPECT_EQ(token_to_terminal(MakeToken(TOKEN_IDENTIFIER)), TERMINAL_INVALID);
  EXPECT_EQ(token_to_terminal(MakeToken(TOKEN_LEFT_PAREN)), TERMINAL_INVALID);
  EXPECT_EQ(get_precedence(TERMINAL_OPERAND, TERMINAL_PLUS), PREC_INVALID);
  EXPECT_EQ(get_precedence(TERMINAL_PLUS, TERMINAL_OPERAND), PREC_INVALID);
  EXPECT_EQ(get_precedence(TERMINAL_HANDLE, TERMINAL_DOLLAR), PREC_INVALID);
  EXPECT_EQ(get_precedence(TERMINAL_INVALID, TERMINAL_INVALID), PREC_INVALID);
}

TEST(PsaPrecedenceTest, MultiplicativeFirstAndLeftAssociative) {
  struct Case {
    TerminalType top;
    TokenType next;
    PRECEDENCE expected;
  };
  const Case cases[] = {{TERMINAL_PLUS, TOKEN_MULTIPLY, PREC_TAKE},
                        {TERMINAL_MUL, TOKEN_PLUS, PREC_REDUCE},
                        {TERMINAL_PLUS, TOKEN_MINUS, PREC_REDUCE},
                        {TERMINAL_MUL, TOKEN_DIVIDE, PREC_REDUCE}};
  for (const Case &c : cases) {
    EXPECT_EQ(get_precedence(c.top, token_to_terminal(MakeToken(c.next))),
              c.expected)
        << "top " << c.top << ", next " << c.next;
  }
}
