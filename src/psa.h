/**
 * @file psa.h
 * @brief Public precedence stack interface and terminal representation.
 * @author Simon Hrouda
 */

#include "lexer.h"
#include "parser.h"

#ifndef SRC_PSA_H
#define SRC_PSA_H

typedef enum PRECEDENCE {
  PREC_TAKE,
  PREC_REDUCE,
  PREC_EQUAL,
  PREC_INVALID,
  PREC_INTERNAL_ERROR
} PRECEDENCE;

typedef enum {
  // Terminals that can be looked up in the precedence table
  TERMINAL_PLUS,    // +
  TERMINAL_MINUS,   // -
  TERMINAL_MUL,     // *
  TERMINAL_DIV,     // /
  TERMINAL_CONCAT,  // ++
  TERMINAL_REL,     // <, <=, >, >=
  TERMINAL_EQUAL,   // ==, !=
  TERMINAL_LPAREN,  // (
  TERMINAL_RPAREN,  // )
  TERMINAL_AND,     // &&
  TERMINAL_OR,      // ||
  TERMINAL_OPERAND, // any subexpression (variable, literal, function call, AST
                    // node)
  TERMINAL_DOLLAR,  // $ stack bottom / end of expression delimiter
  TERMINAL_COUNT,   // = 10 (dimension of precedence table)

  // Stack-only symbols (never looked up in table)
  TERMINAL_HANDLE, // '<' reduction handle marker
  TERMINAL_INVALID // sentinel for syntax errors
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

#endif // SRC_PSA_H