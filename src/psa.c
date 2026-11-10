#include "psa.h"

#include "dynamic_array.h"
#include <stdint.h>

ParserStatus terminal_stack_init(TerminalStack *stack) {
  if (stack == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

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

ParserStatus terminal_stack_pop(TerminalStack *stack, Terminal *terminal) {
  if (stack == NULL || terminal == NULL || stack->size == 0U) {
    return PARSER_INVALID_ARGUMENT;
  }

  stack->size -= 1U;
  *terminal = stack->data[stack->size];
  return PARSER_OK;
}

ParserStatus terminal_stack_peek(TerminalStack *stack, Terminal *terminal) {
  if (stack == NULL || terminal == NULL || stack->size == 0U) {
    return PARSER_INVALID_ARGUMENT;
  }

  *terminal = stack->data[stack->size - 1U];
  return PARSER_OK;
}

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
  case TOKEN_EQUAL:
  case TOKEN_NOT_EQUAL:
  case TOKEN_LESS:
  case TOKEN_GREATER:
  case TOKEN_LESS_EQUAL:
  case TOKEN_GREATER_EQUAL:
    return TERMINAL_REL;

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

PRECEDENCE
get_precedence(TerminalType stackTerminal, TokenType input) {
  (void)(input);

  return PREC_INVALID;
}