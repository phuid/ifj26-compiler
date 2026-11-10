#include "lexer.h"
#include "parser.h"

typedef enum PRECEDENCE {
  PREC_INVALID,
  PREC_TAKE,
  PREC_REDUCE,
  PREC_EQUAL
} PRECEDENCE;

typedef enum {
  TERMINAL_PLUS,    // +
  TERMINAL_MINUS,   // -
  TERMINAL_MUL,     // *
  TERMINAL_DIV,     // /
  TERMINAL_CONCAT,  // ++
  TERMINAL_REL,     // <, >, ==, !=, ...
  TERMINAL_LPAREN,  // (
  TERMINAL_RPAREN,  // )
  TERMINAL_OPERAND, // variables, literals, function calls, ...
  TERMINAL_DOLLAR,  // $ stack bottom / end of input
  TERMINAL_HANDLE,  // < reduction handle
  TERMINAL_INVALID
} TerminalType;

typedef struct {
  TerminalType type;
  ASTNode *node;
} Terminal;

typedef struct {
  Terminal *data;
  size_t size;
  size_t capacity;
} TerminalStack;

ParserStatus terminal_stack_init(TerminalStack *stack);
ParserStatus terminal_stack_push(TerminalStack *stack, Terminal terminal);
ParserStatus terminal_stack_pop(TerminalStack *stack, Terminal *terminal);
ParserStatus terminal_stack_peek(TerminalStack *stack, Terminal *terminal);

/**
 * @todo implement precedence table for operators in the language, see
 */
PRECEDENCE get_precedence(TerminalType stackTerminal, TokenType input);
