/**
 * @file error_trace.c
 * Project: IFJ26 compiler
 * @brief Printing of compile error origins and their propagation chain.
 * @author Vojtěch Ondráček (xondrav00)
 * @note Code in this file was partially generated using an LLM agent.
 */

#include "error_trace.h"

#include <stdarg.h>
#include <stdio.h>

/** Prints "[func]: message\n" to stderr. */
static void print_message(const char *func, const char *fmt, va_list args) {
  /// Print the function name and the formatted message to stderr.
  fprintf(stderr, "[%s]: ", func);

  /// Print the formatted message using the provided format string and
  /// arguments.
  vfprintf(stderr, fmt, args);
  fputc('\n', stderr);
}

void error_trace_origin(size_t line, size_t column, const char *text,
                        const char *func, const char *fmt, ...) {
  /// Print prefix "error: " to stderr
  fputs("error: ", stderr);

  /// Print the line and column if they are not zero
  if (line != 0U) {
    fprintf(stderr, "%zu:%zu ", line, column);
  }
  if (text != NULL && text[0] != '\0') {
    fprintf(stderr, "'%s' ", text);
  }

  /// Print the formatted message using the provided format string and
  /// arguments.
  va_list args;

  /// fmt is the last parameter before the variable arguments
  va_start(args, fmt);
  print_message(func, fmt, args);
  va_end(args);
}

void error_trace_origin_token(const Token *token, const char *func,
                              const char *fmt, ...) {
  /// Print prefix "error: line:column " to stderr
  fprintf(stderr, "error: %zu:%zu ", token->line, token->column);

  /// Print the token's value if it has one, otherwise print the token type
  /// name.
  if (token->type == TOKEN_INT) {
    fprintf(stderr, "'%lld' ", token->value.int_value);
  } else if (token->type == TOKEN_DOUBLE) {
    fprintf(stderr, "'%g' ", token->value.double_value);
  } else if (token->type == TOKEN_NEWLINE ||
             token->value.string_value == NULL) {
    fprintf(stderr, "'%s' ", error_trace_token_type_name(token->type));
  } else {
    fprintf(stderr, "'%s' ", token->value.string_value);
  }

  /// Print the formatted message using the provided format string and
  /// arguments.
  va_list args;
  va_start(args, fmt);
  print_message(func, fmt, args);
  va_end(args);
}

void error_trace_push(const char *func, const char *fmt, ...) {
  /// Print prefix "  in " to stderr
  fputs("  in ", stderr);

  /// Print the formatted message using the provided format string and
  /// arguments.
  va_list args;
  va_start(args, fmt);
  print_message(func, fmt, args);
  va_end(args);
}

const char *error_trace_token_type_name(TokenType type) {
  static const char *const NAMES[] = {[TOKEN_INT] = "integer",
                                      [TOKEN_DOUBLE] = "double",
                                      [TOKEN_STRING] = "string",
                                      [TOKEN_IDENTIFIER] = "identifier",
                                      [TOKEN_WILDCARD] = "_",
                                      [TOKEN_ASSIGN] = "=",
                                      [TOKEN_EQUAL] = "==",
                                      [TOKEN_NOT] = "!",
                                      [TOKEN_AND] = "&&",
                                      [TOKEN_NOT_EQUAL] = "!=",
                                      [TOKEN_ARROW] = "->",
                                      [TOKEN_GREATER] = ">",
                                      [TOKEN_LESS] = "<",
                                      [TOKEN_GREATER_EQUAL] = ">=",
                                      [TOKEN_LESS_EQUAL] = "<=",
                                      [TOKEN_FOR_GENERATOR] = "<-",
                                      [TOKEN_MAIN_ANNOTATION] = "@main",
                                      [TOKEN_INDENT] = "indent",
                                      [TOKEN_DEDENT] = "dedent",
                                      [TOKEN_PLUS] = "+",
                                      [TOKEN_LIST_CONCAT] = "++",
                                      [TOKEN_MINUS] = "-",
                                      [TOKEN_MULTIPLY] = "*",
                                      [TOKEN_DIVIDE] = "/",
                                      [TOKEN_LEFT_PAREN] = "(",
                                      [TOKEN_RIGHT_PAREN] = ")",
                                      [TOKEN_COLON] = ":",
                                      [TOKEN_LEFT_BRACKET] = "[",
                                      [TOKEN_RIGHT_BRACKET] = "]",
                                      [TOKEN_SEMICOLON] = ";",
                                      [TOKEN_NEWLINE] = "newline",
                                      [TOKEN_DOT] = ".",
                                      [TOKEN_COMMA] = ",",
                                      [TOKEN_UNION] = "|",
                                      [TOKEN_OR] = "||",
                                      [TOKEN_KW_DEF] = "def",
                                      [TOKEN_KW_DO] = "do",
                                      [TOKEN_KW_ELSE] = "else",
                                      [TOKEN_KW_IF] = "if",
                                      [TOKEN_KW_THEN] = "then",
                                      [TOKEN_KW_VAL] = "val",
                                      [TOKEN_KW_VAR] = "var",
                                      [TOKEN_KW_WHILE] = "while",
                                      [TOKEN_KW_FOR] = "for",
                                      [TOKEN_KW_RETURN] = "return",
                                      [TOKEN_EOF] = "end of file",
                                      [TOKEN_INVALID] = "invalid token"};

  if ((size_t)type < sizeof(NAMES) / sizeof(NAMES[0]) && NAMES[type] != NULL) {
    return NAMES[type];
  }
  return "token";
}
