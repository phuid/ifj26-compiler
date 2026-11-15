/**
 * @file keyword.h
 * @brief Static keyword table used by the lexer to turn identifiers into
 * keyword tokens.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was partially generated using an LLM agent based on
 *       specifikace.txt.
 */

#ifndef SRC_KEYWORD_H_
#define SRC_KEYWORD_H_

#include <stdbool.h>

#include "lexer.h"
#include "type.h"

/** @brief One row of the static keyword table. */
typedef struct {
  const char *spelling;
  TokenType type;
} Keyword;

/**
 * @brief Finds a keyword by a linear scan of the static table.
 * @param spelling Null-terminated identifier spelling; matched exactly and
 * case-sensitively.
 * @return The matching table row, or null for an ordinary identifier or null
 * input.
 */
const Keyword *keyword_lookup(const char *spelling);

/**
 * @brief Maps a type-name keyword token to its semantic type.
 * @param type Token kind to inspect.
 * @return TYPE_INT, TYPE_REAL, TYPE_STRING, TYPE_UNIT or TYPE_BOOL for the
 * matching TOKEN_KW_* kind, otherwise TYPE_UNKNOWN.
 * @note List types are returned as TYPE_UNKNOWN
 */
BaseType keyword_to_type(TokenType type);

#endif // SRC_KEYWORD_H_
