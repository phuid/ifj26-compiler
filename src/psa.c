/**
 * @file psa.c
 * Project: IFJ26 compiler
 * @brief Precedence stack and reduction helpers for expression parsing.
 * @author Šimon Hrouda (xhrouds00)
 * @note psa_reduce was partially generated using an LLM agent based on
 * psa_spec.md. Model used: Kimi-k3.1-pro.
 */

#include "psa.h"

#include "ast.h"
#include "dynamic_array.h"
#include "error_trace.h"
#include "lexer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

TerminalType token_to_terminal(Token token) {
  switch (token.type) {
  case TOKEN_PLUS:
    return TERMINAL_PLUS;
  case TOKEN_MINUS:
    return TERMINAL_MINUS;
  case TOKEN_MULTIPLY:
    return TERMINAL_MUL;
  case TOKEN_DIVIDE:
    return TERMINAL_DIV;
  case TOKEN_LIST_CONCAT:
    return TERMINAL_CONCAT;
  case TOKEN_AND:
    return TERMINAL_AND;
  case TOKEN_OR:
    return TERMINAL_OR;

  // Relational operators all share one precedence level
  case TOKEN_LESS:
  case TOKEN_GREATER:
  case TOKEN_LESS_EQUAL:
  case TOKEN_GREATER_EQUAL:
    return TERMINAL_REL;
  case TOKEN_EQUAL:
  case TOKEN_NOT_EQUAL:
    return TERMINAL_EQUAL;

  // Delimiters that mark the end of an expression -> act as '$'
  case TOKEN_EOF:
  case TOKEN_RIGHT_PAREN:
  case TOKEN_NEWLINE:
  case TOKEN_SEMICOLON:
  case TOKEN_COMMA:
  case TOKEN_COLON:
  case TOKEN_DEDENT:
  /// `=` ends the target of an assignment (`x = ...`). Only p_statement
  /// accepts it after an expression; everywhere else the caller expects a
  /// different delimiter and reports a syntax error.
  case TOKEN_ASSIGN:
  case TOKEN_KW_THEN:
  case TOKEN_KW_DO:
  case TOKEN_KW_ELSE:
  /// A for-loop guard (`for i <- xs if i > 0 do`) ends the preceding
  /// generator source or guard. Unlike the other delimiters, `if` can start
  /// a statement, so callers that are not for-loop headers must reject it
  /// (e.g. p_if after a one-line branch).
  case TOKEN_KW_IF:
    return TERMINAL_DOLLAR;

  case TOKEN_IDENTIFIER:
    /// if identifier name matches to/until/by operators, return their
    /// respective terminal types
    if (token.value.string_value != NULL) {
      if (strcmp(token.value.string_value, "to") == 0) {
        return TERMINAL_TO;
      } else if (strcmp(token.value.string_value, "until") == 0) {
        return TERMINAL_UNTIL;
      } else if (strcmp(token.value.string_value, "by") == 0) {
        return TERMINAL_BY;
      }
    }
    return TERMINAL_INVALID;

  default:
    // This also applies to Tokens with which operands start, since they are not
    // included in the precedence table and will be handled in the "expecting
    // operand" state of the parser.
    return TERMINAL_INVALID;
  }
}

#define T PREC_TAKE
#define R PREC_REDUCE
#define X PREC_INVALID

static const PRECEDENCE precedence_table[TERMINAL_COUNT][TERMINAL_COUNT] = {
    /* Stack \ Input: *  /  +  -  ++ <= == && || to un by $  */
    /* *          */ {R, R, R, R, R, R, R, R, R, R, R, R, R},
    /* /          */ {R, R, R, R, R, R, R, R, R, R, R, R, R},
    /* +          */ {T, T, R, R, R, R, R, R, R, R, R, R, R},
    /* -          */ {T, T, R, R, R, R, R, R, R, R, R, R, R},
    /* ++         */ {T, T, R, R, R, R, R, R, R, R, R, R, R},
    /* <=         */ {T, T, T, T, T, X, R, R, R, R, R, R, R},
    /* ==         */ {T, T, T, T, T, T, X, R, R, R, R, R, R},
    /* &&         */ {T, T, T, T, T, T, T, R, R, R, R, R, R},
    /* ||         */ {T, T, T, T, T, T, T, T, R, R, R, R, R},
    /* to         */ {T, T, T, T, T, T, T, T, T, R, R, R, R},
    /* until      */ {T, T, T, T, T, T, T, T, T, T, R, R, R},
    /* by         */ {T, T, T, T, T, T, T, T, T, T, T, R, R},
    /* $          */ {T, T, T, T, T, T, T, T, T, T, T, T, X},
};
#undef T
#undef R
#undef X

/**
 * AI-generated code block — precedence stack implementation.
 * Model: gpt-5.6-luna
 * Prompt:
"""
generate these terminalStack handling functions
PsaStatus terminal_stack_init(TerminalStack *stack);
PsaStatus terminal_stack_push(TerminalStack *stack, Terminal terminal);
PsaStatus terminal_stack_peek(TerminalStack *stack, Terminal *terminal);

use the types from psa.h and function from dynamic_array.c for allocation
"""

peek was later replaced by terminal_stack_get_top_terminal, by hand
 */

PsaStatus terminal_stack_init(TerminalStack *stack) {
  if (stack == NULL) {
    ERROR_INTERNAL("null terminal stack");
    return PSA_INVALID_ARGUMENT;
  }

  stack->data = NULL;
  stack->size = 0U;
  stack->capacity = 0U;
  return PSA_OK;
}

static PsaStatus terminal_stack_destroy(TerminalStack *stack) {
  if (stack == NULL) {
    ERROR_INTERNAL("null terminal stack");
    return PSA_INVALID_ARGUMENT;
  }

  free(stack->data);
  stack->data = NULL;
  stack->size = 0U;
  stack->capacity = 0U;
  return PSA_OK;
}

PsaStatus terminal_stack_push(TerminalStack *stack, Terminal terminal) {
  if (stack == NULL) {
    ERROR_INTERNAL("null terminal stack");
    return PSA_INVALID_ARGUMENT;
  }
  if (stack->size == SIZE_MAX) {
    ERROR_INTERNAL("terminal stack size overflow");
    return PSA_INTERNAL_ERROR;
  }
  if (dynamic_array_reserve((void **)&stack->data, &stack->capacity,
                            stack->size + 1U,
                            sizeof(*stack->data)) != DYNAMIC_ARRAY_OK) {
    ERROR_INTERNAL("failed to grow the terminal stack");
    return PSA_INTERNAL_ERROR;
  }

  stack->data[stack->size] = terminal;
  stack->size += 1U;
  return PSA_OK;
}

PsaStatus stack_get_top_terminal(TerminalStack *stack, Terminal *terminal) {
  if (stack == NULL || terminal == NULL || stack->size == 0U) {
    ERROR_INTERNAL("null or empty terminal stack");
    return PSA_INVALID_ARGUMENT;
  }
  for (size_t i = stack->size; i > 0U; i--) {
    TerminalType type = stack->data[i - 1U].type;
    if (type != TERMINAL_OPERAND && type != TERMINAL_HANDLE) {
      *terminal = stack->data[i - 1U];
      return PSA_OK;
    }
  }
  ERROR_INTERNAL("no terminal on the PSA stack");
  return PSA_INVALID_ARGUMENT;
}

PsaStatus terminal_stack_insert_at(TerminalStack *stack, size_t index,
                                   Terminal terminal) {
  if (stack == NULL || index > stack->size) {
    ERROR_INTERNAL("invalid terminal stack insert position");
    return PSA_INVALID_ARGUMENT;
  }
  if (stack->size == SIZE_MAX) {
    ERROR_INTERNAL("terminal stack size overflow");
    return PSA_INTERNAL_ERROR;
  }
  if (dynamic_array_reserve((void **)&stack->data, &stack->capacity,
                            stack->size + 1U,
                            sizeof(*stack->data)) != DYNAMIC_ARRAY_OK) {
    ERROR_INTERNAL("failed to grow the terminal stack");
    return PSA_INTERNAL_ERROR;
  }

  for (size_t i = stack->size; i > index; i--) {
    stack->data[i] = stack->data[i - 1U];
  }

  stack->data[index] = terminal;
  stack->size += 1U;
  return PSA_OK;
}

PsaStatus stack_insert_handle_after_top_terminal(TerminalStack *stack) {
  // 1. Find index of top terminal
  size_t idx;

  if (stack == NULL) {
    ERROR_INTERNAL("null terminal stack");
    return PSA_INVALID_ARGUMENT;
  }

  idx = stack->size;
  for (size_t i = stack->size; i > 0U; i--) {
    TerminalType type = stack->data[i - 1U].type;
    if (type != TERMINAL_OPERAND && type != TERMINAL_HANDLE) {
      idx = i; // Insert immediately after this terminal
      break;
    }
  }
  if (idx == stack->size) {
    ERROR_ORIGIN_AT(
        0U, 0U, NULL,
        "syntax error: no operator to take on the expression stack");
    return PSA_SYNTAX_ERROR;
  }

  // 2. Shift elements right and insert TERMINAL_HANDLE at idx
  Terminal handle = {
      .type = TERMINAL_HANDLE, .token = {.type = TOKEN_INVALID}, .node = NULL};
  return terminal_stack_insert_at(stack, idx, handle);
}

PRECEDENCE get_precedence(TerminalType stack, TerminalType input) {
  if (stack >= TERMINAL_COUNT || input >= TERMINAL_COUNT) {
    return PREC_INVALID;
  }

  return precedence_table[stack][input];
}

static PsaStatus terminal_to_binary_op(Terminal terminal,
                                       ASTNodeKind *return_kind) {
  if (return_kind == NULL || terminal.type >= TERMINAL_COUNT) {
    ERROR_INTERNAL("null terminal or output");
    return PSA_INVALID_ARGUMENT;
  }

  /// to, until and by are only distinguishable by their terminal type, since
  /// the identifier name is not owned by the terminal. Other operators are
  /// distinguished by their token type, which is preserved in the terminal.
  switch (terminal.type) {
  case TERMINAL_TO:
    *return_kind = AST_TO;
    return PSA_OK;
  case TERMINAL_UNTIL:
    *return_kind = AST_UNTIL;
    return PSA_OK;
  case TERMINAL_BY:
    *return_kind = AST_BY;
    return PSA_OK;
  default:
    switch (terminal.token.type) {
    case TOKEN_PLUS:
      *return_kind = AST_ADD;
      return PSA_OK;
    case TOKEN_MINUS:
      *return_kind = AST_SUBTRACT;
      return PSA_OK;
    case TOKEN_MULTIPLY:
      *return_kind = AST_MULTIPLY;
      return PSA_OK;
    case TOKEN_DIVIDE:
      *return_kind = AST_DIVIDE;
      return PSA_OK;
    case TOKEN_LIST_CONCAT:
      *return_kind = AST_LIST_CONCAT;
      return PSA_OK;
    case TOKEN_EQUAL:
      *return_kind = AST_EQUAL;
      return PSA_OK;
    case TOKEN_NOT_EQUAL:
      *return_kind = AST_NOT_EQUAL;
      return PSA_OK;
    case TOKEN_LESS:
      *return_kind = AST_LESS;
      return PSA_OK;
    case TOKEN_LESS_EQUAL:
      *return_kind = AST_LESS_EQUAL;
      return PSA_OK;
    case TOKEN_GREATER:
      *return_kind = AST_GREATER;
      return PSA_OK;
    case TOKEN_GREATER_EQUAL:
      *return_kind = AST_GREATER_EQUAL;
      return PSA_OK;
    case TOKEN_AND:
      *return_kind = AST_AND;
      return PSA_OK;
    case TOKEN_OR:
      *return_kind = AST_OR;
      return PSA_OK;
    default:
      return PSA_INVALID_ARGUMENT; // Invalid token for binary operation
    }
  }
}

PsaStatus psa_reduce(TerminalStack *stack) {
  // Reduction pattern must be exactly 4 items: '<' OPERAND op OPERAND
  if (stack == NULL) {
    ERROR_INTERNAL("null terminal stack");
    return PSA_INVALID_ARGUMENT;
  }
  if (stack->size < 4U) {
    ERROR_ORIGIN_AT(0U, 0U, NULL,
                    "syntax error: missing operand of a binary operator");
    return PSA_SYNTAX_ERROR;
  }

  size_t top_idx = stack->size - 1U;
  Terminal right = stack->data[top_idx];
  Terminal op = stack->data[top_idx - 1U];
  Terminal left = stack->data[top_idx - 2U];
  Terminal handle = stack->data[top_idx - 3U];

  if (handle.type != TERMINAL_HANDLE || left.type != TERMINAL_OPERAND ||
      op.type == TERMINAL_OPERAND || right.type != TERMINAL_OPERAND) {
    ERROR_ORIGIN_AT(op.token.line, op.token.column, NULL,
                    "syntax error: binary operator is missing an operand");
    return PSA_SYNTAX_ERROR;
  }

  if (left.node == NULL || right.node == NULL) {
    ERROR_INTERNAL("operand of a binary operator has no AST node");
    return PSA_INTERNAL_ERROR;
  }

  ASTNodeKind op_kind;
  if (terminal_to_binary_op(op, &op_kind) != PSA_OK) {
    ERROR_ORIGIN_AT(op.token.line, op.token.column, NULL,
                    "syntax error: not a binary operator");
    return PSA_SYNTAX_ERROR;
  }

  ASTNode *bin_node = ast_node_create_binary_op(op_kind, left.node, right.node,
                                                op.token.line, op.token.column);
  if (bin_node == NULL) {
    ERROR_INTERNAL("failed to allocate a binary operator node");
    return PSA_INTERNAL_ERROR;
  }

  // Pop '<', left, op, right (4 items)
  stack->size -= 4U;

  // Push resulting evaluated operand
  Terminal result = {.type = TERMINAL_OPERAND,
                     .token = {.type = TOKEN_INVALID,
                               .line = op.token.line,
                               .column = op.token.column},
                     .node = bin_node};
  PsaStatus status = terminal_stack_push(stack, result);
  if (status != PSA_OK) {
    ast_node_destroy(bin_node);
  }
  return status;
}

PsaStatus terminal_stack_cleanup_nodes(TerminalStack *stack) {
  if (stack == NULL) {
    ERROR_INTERNAL("null terminal stack");
    return PSA_INVALID_ARGUMENT;
  }
  for (size_t i = 0; i < stack->size; i++) {
    if (stack->data[i].type == TERMINAL_OPERAND &&
        stack->data[i].node != NULL) {
      ast_node_destroy(stack->data[i].node);
      stack->data[i].node = NULL;
    }
  }
  return terminal_stack_destroy(stack);
}
