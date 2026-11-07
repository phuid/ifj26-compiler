/**
 * @file keyword.c
 * @brief Static keyword table and its lookup functions.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was partially generated using an LLM agent based on
 *       specifikace.txt.
 */

#include "keyword.h"

#include <stddef.h>
#include <string.h>

/**
 * All 22 IFJ26 keywords. Keywords whose following expression or pattern may
 * start with `(` have a dedicated *_PAREN token; all others reject `(`.
 */
static const Keyword keywords[] = {
    {"def", TOKEN_KW_DEF, TOKEN_INVALID},
    {"do", TOKEN_KW_DO, TOKEN_INVALID},
    {"Double", TOKEN_KW_DOUBLE, TOKEN_INVALID},
    {"else", TOKEN_KW_ELSE, TOKEN_INVALID},
    {"if", TOKEN_KW_IF, TOKEN_KW_IF_PAREN},
    {"import", TOKEN_KW_IMPORT, TOKEN_INVALID},
    {"Int", TOKEN_KW_INT, TOKEN_INVALID},
    {"return", TOKEN_KW_RETURN, TOKEN_KW_RETURN_PAREN},
    {"String", TOKEN_KW_STRING, TOKEN_INVALID},
    {"then", TOKEN_KW_THEN, TOKEN_INVALID},
    {"Unit", TOKEN_KW_UNIT, TOKEN_INVALID},
    {"val", TOKEN_KW_VAL, TOKEN_INVALID},
    {"var", TOKEN_KW_VAR, TOKEN_INVALID},
    {"while", TOKEN_KW_WHILE, TOKEN_KW_WHILE_PAREN},
    {"Boolean", TOKEN_KW_BOOLEAN, TOKEN_INVALID},
    {"case", TOKEN_KW_CASE, TOKEN_KW_CASE_PAREN},
    {"class", TOKEN_KW_CLASS, TOKEN_INVALID},
    {"extension", TOKEN_KW_EXTENSION, TOKEN_INVALID},
    {"for", TOKEN_KW_FOR, TOKEN_KW_FOR_PAREN},
    {"List", TOKEN_KW_LIST, TOKEN_KW_LIST_PAREN},
    {"match", TOKEN_KW_MATCH, TOKEN_INVALID},
    {"type", TOKEN_KW_TYPE, TOKEN_INVALID},
};

const Keyword *keyword_lookup(const char *spelling) {
  size_t index;

  if (spelling == NULL) {
    return NULL;
  }
  for (index = 0U; index < sizeof(keywords) / sizeof(*keywords); index += 1U) {
    if (strcmp(spelling, keywords[index].spelling) == 0) {
      return &keywords[index];
    }
  }
  return NULL;
}

DataType keyword_to_type(TokenType type) {
  switch (type) {
  case TOKEN_KW_INT:
    return TYPE_INT;
  case TOKEN_KW_DOUBLE:
    return TYPE_REAL;
  case TOKEN_KW_STRING:
    return TYPE_STRING;
  case TOKEN_KW_UNIT:
    return TYPE_UNIT;
  case TOKEN_KW_BOOLEAN:
    return TYPE_BOOL;
  default:
    return TYPE_UNKNOWN;
  }
}
