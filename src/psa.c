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

PRECEDENCE
get_precedence(TerminalType stackTerminal, TokenType input) {
  (void)(input);

  return PREC_INVALID;
}