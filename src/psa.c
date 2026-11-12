/**
 * @file psa.c
 * @brief Precedence stack implementation for operator precedence parsing.
 * @author Simon Hrouda
 */

#include "psa.h"

#include "dynamic_array.h"
#include <stdint.h>

static TerminalType token_to_terminal(TokenType token) {
  switch (token) {
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

  // Relational operators all share one precedence level
  case TOKEN_LESS:
  case TOKEN_GREATER:
  case TOKEN_LESS_EQUAL:
  case TOKEN_GREATER_EQUAL:
    return TERMINAL_REL;
  case TOKEN_EQUAL:
  case TOKEN_NOT_EQUAL:
    return TERMINAL_EQUAL;
  case TOKEN_LEFT_PAREN:
    return TERMINAL_LPAREN;
  case TOKEN_RIGHT_PAREN:
    return TERMINAL_RPAREN;

  // Operands
  case TOKEN_INT:
  case TOKEN_DOUBLE:
  case TOKEN_STRING:
  case TOKEN_IDENTIFIER:
    return TERMINAL_OPERAND;

  // Delimiters that mark the end of an expression -> act as '$'
  case TOKEN_EOF:
  case TOKEN_NEWLINE:
  case TOKEN_SEMICOLON:
  case TOKEN_COMMA:
  case TOKEN_COLON:
  case TOKEN_RIGHT_BRACKET:
  case TOKEN_DEDENT:
  case TOKEN_KW_THEN:
  case TOKEN_KW_DO:
  case TOKEN_KW_ELSE:
    return TERMINAL_DOLLAR;

  default:
    return TERMINAL_INVALID;
  }
}

#define T PREC_TAKE
#define R PREC_REDUCE
#define E PREC_EQUAL
#define I PREC_INVALID

static const PRECEDENCE precedence_table[TERMINAL_COUNT][TERMINAL_COUNT] = {
    /* Stack \ Input: +  -  *  /  ++ <= == (  )  && || i  $  */
    /* +          */ {I, I, I, I, I, I, I, I, I, I, I, I, I},
    /* -          */ {I, I, I, I, I, I, I, I, I, I, I, I, I},
    /* *          */ {I, I, I, I, I, I, I, I, I, I, I, I, I},
    /* /          */ {I, I, I, I, I, I, I, I, I, I, I, I, I},
    /* ++         */ {I, I, I, I, I, I, I, I, I, I, I, I, I},
    /* <=         */ {I, I, I, I, I, I, I, I, I, I, I, I, I},
    /* ==         */ {I, I, I, I, I, I, I, I, I, I, I, I, I},
    /* (          */ {I, I, I, I, I, I, I, I, I, I, I, I, I},
    /* )          */ {I, I, I, I, I, I, I, I, I, I, I, I, I},
    /* &&         */ {I, I, I, I, I, I, I, I, I, I, I, I, I},
    /* ||         */ {I, I, I, I, I, I, I, I, I, I, I, I, I},
    /* i          */ {I, I, I, I, I, I, I, I, I, I, I, I, I},
    /* $          */ {I, I, I, I, I, I, I, I, I, I, I, I, I},
};
#undef T
#undef R
#undef E
#undef I

/**
 * AI-generated code block — precedence stack implementation.
 * Model: gpt-5.6-luna
 * Prompt:
"""
generate these terminalStack handling functions
ParserStatus terminal_stack_init(TerminalStack *stack);
ParserStatus terminal_stack_push(TerminalStack *stack, Terminal terminal);
ParserStatus terminal_stack_pop(TerminalStack *stack, Terminal *terminal);
ParserStatus terminal_stack_peek(TerminalStack *stack, Terminal *terminal);

use the types from psa.h and function from dynamic_array.c for allocation
"""

peek was later replaced by terminal_stack_get_top_terminal, by hand
 */
ParserStatus terminal_stack_init(TerminalStack *stack) {
  if (stack == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  stack->data = NULL;
  stack->size = 0U;
  stack->capacity = 0U;
  return PARSER_OK;
}

ParserStatus terminal_stack_destroy(TerminalStack *stack) {
  if (stack == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  free(stack->data);
  stack->data = NULL;
  stack->size = 0U;
  stack->capacity = 0U;
  return PARSER_OK;
}

ParserStatus terminal_stack_push(TerminalStack *stack, Terminal terminal) {
  if (stack == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }
  if (stack->size == SIZE_MAX) {
    return PARSER_INTERNAL_ERROR;
  }
  if (dynamic_array_reserve((void **)&stack->data, &stack->capacity,
                            stack->size + 1U,
                            sizeof(*stack->data)) != DYNAMIC_ARRAY_OK) {
    return PARSER_INTERNAL_ERROR;
  }

  stack->data[stack->size] = terminal;
  stack->size += 1U;
  return PARSER_OK;
}

// TODO: should pop remove the terminal top, or the top terminal (not operand)?
ParserStatus terminal_stack_pop(TerminalStack *stack, Terminal *terminal) {
  if (stack == NULL || terminal == NULL || stack->size == 0U) {
    return PARSER_INVALID_ARGUMENT;
  }

  stack->size -= 1U;
  *terminal = stack->data[stack->size];
  return PARSER_OK;
}

ParserStatus stack_get_top_terminal(TerminalStack *stack, Terminal *terminal) {
  if (stack == NULL || stack->size == 0U) {
    return PARSER_INVALID_ARGUMENT;
  }
  for (size_t i = stack->size - 1; i >= 0U; i--) {
    TerminalType type = stack->data[i].type;
    if (type != TERMINAL_OPERAND && type != TERMINAL_HANDLE) {
      *terminal = stack->data[i];
      return PARSER_OK;
    }
  }
  return PARSER_INVALID_ARGUMENT;
}

ParserStatus terminal_stack_insert_at(TerminalStack *stack, size_t index,
                                      Terminal terminal) {
  if (stack == NULL || index > stack->size) {
    return PARSER_INVALID_ARGUMENT;
  }
  if (dynamic_array_reserve((void **)&stack->data, &stack->capacity,
                            stack->size + 1U,
                            sizeof(*stack->data)) != DYNAMIC_ARRAY_OK) {
    return PARSER_INTERNAL_ERROR;
  }

  // Shift elements to the right to make space for the new terminal
  for (size_t i = stack->size; i > index; i--) {
    stack->data[i] = stack->data[i - 1U];
  }

  stack->data[index] = terminal;
  stack->size += 1U;
  return PARSER_OK;
}

ParserStatus stack_insert_handle_after_top_terminal(TerminalStack *stack) {
  // 1. Find index of top terminal
  size_t idx = stack->size;
  for (size_t i = stack->size; i > 0U; i--) {
    TerminalType type = stack->data[i - 1U].type;
    if (type != TERMINAL_OPERAND && type != TERMINAL_HANDLE) {
      idx = i; // Insert immediately after this terminal
      break;
    }
  }

  // 2. Shift elements right and insert TERMINAL_HANDLE at idx
  Terminal handle = {.type = TERMINAL_HANDLE, .node = NULL};
  return terminal_stack_insert_at(stack, idx, handle);
}

PRECEDENCE get_precedence(TerminalType stackTerminal, TokenType input) {
  TerminalType inputTerminal = token_to_terminal(input);
  if (stackTerminal >= TERMINAL_COUNT || inputTerminal >= TERMINAL_COUNT) {
    return PREC_INTERNAL_ERROR;
  }

  return precedence_table[stackTerminal][inputTerminal];
}
