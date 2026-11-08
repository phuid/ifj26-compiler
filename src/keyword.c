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

/** All 22 IFJ26 keywords. Whether `(` may follow is decided by the parser. */
static const Keyword keywords[] = {
    {"def", TOKEN_KW_DEF},         {"do", TOKEN_KW_DO},
    {"Double", TOKEN_KW_DOUBLE},   {"else", TOKEN_KW_ELSE},
    {"if", TOKEN_KW_IF},           {"import", TOKEN_KW_IMPORT},
    {"Int", TOKEN_KW_INT},         {"return", TOKEN_KW_RETURN},
    {"String", TOKEN_KW_STRING},   {"then", TOKEN_KW_THEN},
    {"Unit", TOKEN_KW_UNIT},       {"val", TOKEN_KW_VAL},
    {"var", TOKEN_KW_VAR},         {"while", TOKEN_KW_WHILE},
    {"Boolean", TOKEN_KW_BOOLEAN}, {"case", TOKEN_KW_CASE},
    {"class", TOKEN_KW_CLASS},     {"extension", TOKEN_KW_EXTENSION},
    {"for", TOKEN_KW_FOR},         {"List", TOKEN_KW_LIST},
    {"match", TOKEN_KW_MATCH},     {"type", TOKEN_KW_TYPE},
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
