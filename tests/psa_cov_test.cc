#include <cstdint>
#include <cstdlib>

#include "gtest/gtest.h"

extern "C" {
#include "../src/psa.h"
}

namespace {

Terminal MakeTerminal(TerminalType type, ASTNode *node = nullptr,
                      TokenType token_type = TOKEN_INVALID) {
  Terminal terminal = {};
  terminal.type = type;
  terminal.token.type = token_type;
  terminal.node = node;
  return terminal;
}

Token MakeToken(TokenType type, const char *text = nullptr) {
  Token token = {};
  token.type = type;
  token.value.string_value = const_cast<char *>(text);
  return token;
}

} // namespace

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
  Terminal storage = MakeTerminal(TERMINAL_DOLLAR);
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
  EXPECT_EQ(terminal_stack_push(&frozen.stack, MakeTerminal(TERMINAL_PLUS)),
            PSA_INTERNAL_ERROR);
  EXPECT_EQ(
      terminal_stack_insert_at(&frozen.stack, 0U, MakeTerminal(TERMINAL_PLUS)),
      PSA_INTERNAL_ERROR);
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
    EXPECT_EQ(terminal_stack_push(&frozen.stack, MakeTerminal(TERMINAL_MUL)),
              PSA_INTERNAL_ERROR);
    frozen.Expect(size, size);
  }
}

/*
 * AI-generated test block — TerminalStack value semantics.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Push stores the whole Terminal (type, token and node pointer); the top
 *   terminal lookup leaves the stack as it is; insert_at shifts the rest;
 *   a cleaned-up stack accepts new pushes.
 */
TEST(PsaCovTerminalStackValueTest, PushKeepsNodePointerAndTopKeepsStack) {
  ASTNode fake_node = {};
  ASTNode *const node = &fake_node;
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PSA_OK);
  ASSERT_EQ(terminal_stack_push(
                &stack, MakeTerminal(TERMINAL_PLUS, nullptr, TOKEN_PLUS)),
            PSA_OK);
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_OPERAND, node)),
            PSA_OK);

  Terminal out = MakeTerminal(TERMINAL_INVALID);
  ASSERT_EQ(stack_get_top_terminal(&stack, &out), PSA_OK);
  EXPECT_EQ(out.type, TERMINAL_PLUS);
  EXPECT_EQ(out.token.type, TOKEN_PLUS);
  ASSERT_EQ(stack_get_top_terminal(&stack, &out), PSA_OK);
  EXPECT_EQ(stack.size, 2U);
  EXPECT_EQ(stack.data[1].type, TERMINAL_OPERAND);
  EXPECT_EQ(stack.data[1].node, node);

  // The node is not owned by the test's AST, so release only the storage.
  free(stack.data);
}

TEST(PsaCovTerminalStackValueTest, InsertAtShiftsAndRejectsOutOfRange) {
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PSA_OK);
  EXPECT_EQ(terminal_stack_insert_at(nullptr, 0U, MakeTerminal(TERMINAL_MUL)),
            PSA_INVALID_ARGUMENT);
  EXPECT_EQ(terminal_stack_insert_at(&stack, 1U, MakeTerminal(TERMINAL_MUL)),
            PSA_INVALID_ARGUMENT);
  ASSERT_EQ(terminal_stack_insert_at(&stack, 0U, MakeTerminal(TERMINAL_OR)),
            PSA_OK);
  ASSERT_EQ(terminal_stack_insert_at(&stack, 0U, MakeTerminal(TERMINAL_DOLLAR)),
            PSA_OK);
  ASSERT_EQ(terminal_stack_insert_at(&stack, 1U, MakeTerminal(TERMINAL_AND)),
            PSA_OK);
  ASSERT_EQ(stack.size, 3U);
  EXPECT_EQ(stack.data[0].type, TERMINAL_DOLLAR);
  EXPECT_EQ(stack.data[1].type, TERMINAL_AND);
  EXPECT_EQ(stack.data[2].type, TERMINAL_OR);
  EXPECT_EQ(terminal_stack_cleanup_nodes(&stack), PSA_OK);
}

TEST(PsaCovTerminalStackValueTest, HandleIsInsertedAfterTopTerminal) {
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PSA_OK);
  EXPECT_EQ(stack_insert_handle_after_top_terminal(nullptr),
            PSA_INVALID_ARGUMENT);
  // Without any terminal there is nothing to put the handle after.
  EXPECT_EQ(stack_insert_handle_after_top_terminal(&stack), PSA_SYNTAX_ERROR);
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_DOLLAR)), PSA_OK);
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_OPERAND)),
            PSA_OK);
  ASSERT_EQ(stack_insert_handle_after_top_terminal(&stack), PSA_OK);
  ASSERT_EQ(stack.size, 3U);
  EXPECT_EQ(stack.data[0].type, TERMINAL_DOLLAR);
  EXPECT_EQ(stack.data[1].type, TERMINAL_HANDLE);
  EXPECT_EQ(stack.data[2].type, TERMINAL_OPERAND);
  // An operator with no operand after it (`a + * b`) cannot open a handle.
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_PLUS)), PSA_OK);
  EXPECT_EQ(stack_insert_handle_after_top_terminal(&stack), PSA_SYNTAX_ERROR);
  EXPECT_EQ(stack.size, 4U);
  EXPECT_EQ(terminal_stack_cleanup_nodes(&stack), PSA_OK);
}

TEST(PsaCovTerminalStackValueTest, CleanedUpStackAcceptsNewTerminals) {
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PSA_OK);
  for (int round = 0; round < 2; ++round) {
    SCOPED_TRACE(round);
    ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_DOLLAR)),
              PSA_OK);
    ASTNode *node = ast_node_create_int(round, 1U, 1U);
    ASSERT_NE(node, nullptr);
    ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_OPERAND, node)),
              PSA_OK);
    // cleanup destroys the owned operand AST and empties the stack.
    ASSERT_EQ(terminal_stack_cleanup_nodes(&stack), PSA_OK);
    EXPECT_EQ(stack.data, nullptr);
    EXPECT_EQ(stack.size, 0U);
    EXPECT_EQ(stack.capacity, 0U);
  }
}

/*
 * AI-generated test block — psa_reduce.
 * Model: Claude (Claude Code).
 * Prompt:
 *   `< E op E` reduces to one operand holding a binary AST node; anything
 *   else is a syntax error that leaves the stack untouched.
 */
TEST(PsaCovReduceTest, HandleOperandOperatorOperandBecomesBinaryNode) {
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PSA_OK);
  ASTNode *left = ast_node_create_int(1, 1U, 1U);
  ASTNode *right = ast_node_create_int(2, 1U, 5U);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  Terminal op = MakeTerminal(TERMINAL_REL, nullptr, TOKEN_LESS_EQUAL);
  op.token.line = 1U;
  op.token.column = 3U;
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_DOLLAR)), PSA_OK);
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_HANDLE)), PSA_OK);
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_OPERAND, left)),
            PSA_OK);
  ASSERT_EQ(terminal_stack_push(&stack, op), PSA_OK);
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_OPERAND, right)),
            PSA_OK);

  ASSERT_EQ(psa_reduce(&stack), PSA_OK);
  ASSERT_EQ(stack.size, 2U);
  EXPECT_EQ(stack.data[0].type, TERMINAL_DOLLAR);
  ASSERT_EQ(stack.data[1].type, TERMINAL_OPERAND);
  ASTNode *node = stack.data[1].node;
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->kind, AST_LESS_EQUAL);
  EXPECT_EQ(node->data.binary_op.left, left);
  EXPECT_EQ(node->data.binary_op.right, right);
  EXPECT_EQ(node->line, 1U);
  EXPECT_EQ(node->column, 3U);
  EXPECT_EQ(terminal_stack_cleanup_nodes(&stack), PSA_OK);
}

TEST(PsaCovReduceTest, MalformedHandlesAreSyntaxErrors) {
  EXPECT_EQ(psa_reduce(nullptr), PSA_INVALID_ARGUMENT);
  TerminalStack stack;
  ASSERT_EQ(terminal_stack_init(&stack), PSA_OK);
  ASTNode fake_node = {};
  // Too short.
  ASSERT_EQ(terminal_stack_push(&stack, MakeTerminal(TERMINAL_HANDLE)), PSA_OK);
  ASSERT_EQ(
      terminal_stack_push(&stack, MakeTerminal(TERMINAL_OPERAND, &fake_node)),
      PSA_OK);
  EXPECT_EQ(psa_reduce(&stack), PSA_SYNTAX_ERROR);
  // `< E E E` has no operator.
  ASSERT_EQ(
      terminal_stack_push(&stack, MakeTerminal(TERMINAL_OPERAND, &fake_node)),
      PSA_OK);
  ASSERT_EQ(
      terminal_stack_push(&stack, MakeTerminal(TERMINAL_OPERAND, &fake_node)),
      PSA_OK);
  EXPECT_EQ(psa_reduce(&stack), PSA_SYNTAX_ERROR);
  EXPECT_EQ(stack.size, 4U);
  // `< E op E` whose operator token is not a binary operator.
  stack.data[2] = MakeTerminal(TERMINAL_PLUS, nullptr, TOKEN_COMMA);
  EXPECT_EQ(psa_reduce(&stack), PSA_SYNTAX_ERROR);
  EXPECT_EQ(stack.size, 4U);
  // The fake nodes are not heap ASTs, so release only the storage.
  free(stack.data);
}

/*
 * AI-generated test block — token_to_terminal and the precedence table.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Check how tokens map to precedence terminals (delimiters act as `$`,
 *   operand tokens are no terminals) and the relations between arithmetic,
 *   relational, logical and range operators. Tokens that cannot appear in an
 *   expression have no relation with any stack top.
 */
namespace {

const TokenType kOperandTokens[] = {TOKEN_INT, TOKEN_DOUBLE, TOKEN_STRING,
                                    TOKEN_IDENTIFIER, TOKEN_LEFT_PAREN};
const TokenType kDelimiterTokens[] = {
    TOKEN_EOF,   TOKEN_RIGHT_PAREN, TOKEN_NEWLINE, TOKEN_SEMICOLON,
    TOKEN_COMMA, TOKEN_COLON,       TOKEN_DEDENT,  TOKEN_KW_THEN,
    TOKEN_KW_DO, TOKEN_KW_ELSE,     TOKEN_KW_IF};
const TerminalType kArithmeticTerminals[] = {TERMINAL_PLUS, TERMINAL_MINUS,
                                             TERMINAL_MUL, TERMINAL_DIV};

} // namespace

TEST(PsaCovPrecedenceTest, TokensMapToTheirTerminals) {
  const struct {
    TokenType token;
    TerminalType terminal;
  } cases[] = {
      {TOKEN_PLUS, TERMINAL_PLUS},
      {TOKEN_MINUS, TERMINAL_MINUS},
      {TOKEN_MULTIPLY, TERMINAL_MUL},
      {TOKEN_DIVIDE, TERMINAL_DIV},
      {TOKEN_LIST_CONCAT, TERMINAL_CONCAT},
      {TOKEN_LESS, TERMINAL_REL},
      {TOKEN_GREATER, TERMINAL_REL},
      {TOKEN_LESS_EQUAL, TERMINAL_REL},
      {TOKEN_GREATER_EQUAL, TERMINAL_REL},
      {TOKEN_EQUAL, TERMINAL_EQUAL},
      {TOKEN_NOT_EQUAL, TERMINAL_EQUAL},
      {TOKEN_AND, TERMINAL_AND},
      {TOKEN_OR, TERMINAL_OR},
  };
  for (const auto &c : cases) {
    EXPECT_EQ(token_to_terminal(MakeToken(c.token)), c.terminal) << c.token;
  }
  for (const TokenType end : kDelimiterTokens) {
    EXPECT_EQ(token_to_terminal(MakeToken(end)), TERMINAL_DOLLAR) << end;
  }
  for (const TokenType operand : kOperandTokens) {
    EXPECT_EQ(token_to_terminal(MakeToken(operand)), TERMINAL_INVALID)
        << operand;
  }
  // Range operators are identifiers recognised by their spelling.
  EXPECT_EQ(token_to_terminal(MakeToken(TOKEN_IDENTIFIER, "to")), TERMINAL_TO);
  EXPECT_EQ(token_to_terminal(MakeToken(TOKEN_IDENTIFIER, "until")),
            TERMINAL_UNTIL);
  EXPECT_EQ(token_to_terminal(MakeToken(TOKEN_IDENTIFIER, "by")), TERMINAL_BY);
  EXPECT_EQ(token_to_terminal(MakeToken(TOKEN_IDENTIFIER, "toward")),
            TERMINAL_INVALID);
}

TEST(PsaCovPrecedenceTest, NonExpressionTokensHaveNoRelation) {
  const TokenType foreign[] = {TOKEN_KW_DEF,   TOKEN_KW_IMPORT,
                               TOKEN_KW_CLASS, TOKEN_MAIN_ANNOTATION,
                               TOKEN_NOT,      TOKEN_INVALID};
  const TerminalType tops[] = {TERMINAL_DOLLAR, TERMINAL_PLUS, TERMINAL_OPERAND,
                               TERMINAL_HANDLE, TERMINAL_INVALID};
  for (const TerminalType top : tops) {
    for (const TokenType next : foreign) {
      EXPECT_EQ(get_precedence(top, token_to_terminal(MakeToken(next))),
                PREC_INVALID)
          << "top " << top << ", next " << next;
    }
  }
}

TEST(PsaCovPrecedenceTest, OperatorsAreReducedAtDelimiters) {
  for (const TerminalType op :
       {TERMINAL_MUL, TERMINAL_DIV, TERMINAL_PLUS, TERMINAL_MINUS,
        TERMINAL_CONCAT, TERMINAL_REL, TERMINAL_EQUAL, TERMINAL_AND,
        TERMINAL_OR, TERMINAL_TO, TERMINAL_UNTIL, TERMINAL_BY}) {
    EXPECT_EQ(get_precedence(op, TERMINAL_DOLLAR), PREC_REDUCE) << op;
    EXPECT_EQ(get_precedence(TERMINAL_DOLLAR, op), PREC_TAKE) << op;
  }
  EXPECT_EQ(get_precedence(TERMINAL_DOLLAR, TERMINAL_DOLLAR), PREC_INVALID);
}

TEST(PsaCovPrecedenceTest, RelationalOperatorsBindLooserThanArithmetic) {
  for (const TerminalType rel : {TERMINAL_REL, TERMINAL_EQUAL}) {
    for (const TerminalType op : kArithmeticTerminals) {
      EXPECT_EQ(get_precedence(op, rel), PREC_REDUCE)
          << "top " << op << ", next " << rel;
      EXPECT_EQ(get_precedence(rel, op), PREC_TAKE)
          << "top " << rel << ", next " << op;
    }
  }
  // Comparisons of one level are not associative: `a < b < c` is an error.
  EXPECT_EQ(get_precedence(TERMINAL_REL, TERMINAL_REL), PREC_INVALID);
  EXPECT_EQ(get_precedence(TERMINAL_EQUAL, TERMINAL_EQUAL), PREC_INVALID);
  // `<`-level binds tighter than `==`-level, which binds tighter than && / ||.
  EXPECT_EQ(get_precedence(TERMINAL_REL, TERMINAL_EQUAL), PREC_REDUCE);
  EXPECT_EQ(get_precedence(TERMINAL_EQUAL, TERMINAL_REL), PREC_TAKE);
  EXPECT_EQ(get_precedence(TERMINAL_EQUAL, TERMINAL_AND), PREC_REDUCE);
  EXPECT_EQ(get_precedence(TERMINAL_AND, TERMINAL_OR), PREC_REDUCE);
  EXPECT_EQ(get_precedence(TERMINAL_OR, TERMINAL_AND), PREC_TAKE);
}

TEST(PsaCovPrecedenceTest, OutOfRangeTerminalsHaveNoRelation) {
  EXPECT_EQ(get_precedence(TERMINAL_COUNT, TERMINAL_PLUS), PREC_INVALID);
  EXPECT_EQ(get_precedence(TERMINAL_PLUS, TERMINAL_COUNT), PREC_INVALID);
}
