/**
 * @file psa.h
 * Project: IFJ26 compiler
 * @brief Precedence stack and reduction helpers for expression parsing.
 * @author Šimon Hrouda (xhrouds00)
 */

#ifndef SRC_PSA_H
#define SRC_PSA_H

#include <stddef.h>

#include "ast.h"
#include "lexer.h"

typedef enum PRECEDENCE {
  PREC_TAKE,
  PREC_REDUCE,
  PREC_INVALID,
  PREC_INTERNAL_ERROR
} PRECEDENCE;

typedef enum PSA_STATUS {
  PSA_OK,
  PSA_INVALID_ARGUMENT,
  PSA_SYNTAX_ERROR,
  PSA_INTERNAL_ERROR
} PsaStatus;

typedef enum {
  // Terminals that can be looked up in the precedence table
  TERMINAL_MUL,    // *
  TERMINAL_DIV,    // /
  TERMINAL_PLUS,   // +
  TERMINAL_MINUS,  // -
  TERMINAL_CONCAT, // ++
  TERMINAL_REL,    // <, <=, >, >=
  TERMINAL_EQUAL,  // ==, !=
  TERMINAL_AND,    // &&
  TERMINAL_OR,     // ||
  TERMINAL_TO,     // to
  TERMINAL_UNTIL,  // until
  TERMINAL_BY,     // by
  TERMINAL_DOLLAR, // $ stack bottom / end of expression delimiter
  TERMINAL_COUNT,  // = 13 (dimension of precedence table)

  // Stack-only symbols (never looked up in table)
  TERMINAL_OPERAND, // any subexpression (variable, literal, function call, AST
                    // NODE)
  TERMINAL_HANDLE,  // '<' reduction handle marker
  TERMINAL_INVALID  // sentinel for syntax errors
} TerminalType;

typedef struct {
  TerminalType type;
  Token token;   // Filled for operators/delimiters, TOKEN_INVALID for
                 // TERMINAL_OPERAND
  ASTNode *node; // Filled for TERMINAL_OPERAND, NULL for operators/delimiters
} Terminal;

typedef struct {
  Terminal *data;
  size_t size;
  size_t capacity;
} TerminalStack;

/** Maps a lexer token to its precedence-table terminal without consuming it. */
TerminalType token_to_terminal(Token token);

/** Returns the table relation for two terminals, or PREC_INVALID out of range.
 */
PRECEDENCE get_precedence(TerminalType stack, TerminalType input);

PsaStatus terminal_stack_init(TerminalStack *stack);
PsaStatus terminal_stack_push(TerminalStack *stack, Terminal terminal);
PsaStatus stack_get_top_terminal(TerminalStack *stack, Terminal *terminal);
PsaStatus terminal_stack_insert_at(TerminalStack *stack, size_t index,
                                   Terminal terminal);
PsaStatus stack_insert_handle_after_top_terminal(TerminalStack *stack);

/** Reduces the top handle/operand/operator/operand sequence into one AST node.
 */
PsaStatus psa_reduce(TerminalStack *stack);

/** Destroys all operand ASTs still owned by the stack and empties the stack. */
PsaStatus terminal_stack_cleanup_nodes(TerminalStack *stack);

#endif // SRC_PSA_H
