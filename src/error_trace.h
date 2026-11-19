/**
 * @file error_trace.h
 * @brief Prints a compile error and the chain of functions it propagates
 * through.
 * @author Vojtech Ondracek
 * @note Code in this file was partially generated using an LLM agent.
 *
 * @details The function that detects an error prints its *origin* with
 * @ref ERROR_ORIGIN, @ref ERROR_ORIGIN_AT or @ref ERROR_INTERNAL. Every caller
 * that propagates the error further prints one context line with
 * @ref ERROR_TRACE. Lines are written to stderr immediately, so they appear
 * origin first, outermost caller last:
 *
 *     error: 7:9 'x' [p_variable_init_statement]: redefinition of variable 'x'
 *       in [p_statement]: while parsing var definition
 *       in [p_code_block]: while parsing a statement in function 'foo'
 */

#ifndef SRC_ERROR_TRACE_H_
#define SRC_ERROR_TRACE_H_

#include <stddef.h>
#include <stdio.h>

#include "lexer.h"

/** Prints an error origin; @p line 0 means unknown position. */
void error_trace_origin(size_t line, size_t column, const char *text,
                        const char *func, const char *fmt, ...);

/** Prints an error origin at @p token (position and spelling). */
void error_trace_origin_token(const Token *token, const char *func,
                              const char *fmt, ...);

/** Prints a context line of a propagating error. */
void error_trace_push(const char *func, const char *fmt, ...);

/** Returns a printable name of a token type, e.g. "=" or "identifier". */
const char *error_trace_token_type_name(TokenType type);

/** Error at the token @p token. */
#define ERROR_ORIGIN(token, ...)                                               \
  error_trace_origin_token((token), __func__, __VA_ARGS__)

/** Error at a line/column, with optional source @p text (may be null). */
#define ERROR_ORIGIN_AT(line, column, text, ...)                               \
  error_trace_origin((line), (column), (text), __func__, __VA_ARGS__)

/** Internal compiler error (failed allocation, API misuse). */
#define ERROR_INTERNAL(...)                                                    \
  (fputs("internal ", stderr),                                                 \
   error_trace_origin(0U, 0U, NULL, __func__, __VA_ARGS__))

/** Context line: what the current function was doing when the error came. */
#define ERROR_TRACE(...) error_trace_push(__func__, __VA_ARGS__)

#endif // SRC_ERROR_TRACE_H_
