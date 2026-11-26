/**
 * @file lexer_dfa.c
 * Project: IFJ26 compiler
 * @brief Declarative DFA tables for the IFJ26 lexer.
 * @author Vojtěch Ondráček (xondrav00)
 * @author Šimon Hrouda (xhrouds00)
 * @note Code in this file was partially generated using an LLM agent based on
 *       specifikace.txt.
 */

#include "lexer_dfa.h"

#include <stddef.h>

/** @brief Names the character predicates that occur in KA.md transitions. */
typedef enum {
  MATCH_CHAR,
  MATCH_DIGIT,
  MATCH_NONZERO_DIGIT,
  MATCH_LETTER,
  MATCH_C,
  MATCH_M
} MatchKind;

/** @brief One edge leaving the state denoted by its containing table row. */
typedef struct {
  MatchKind match;
  int character; /**< Literal value used only by MATCH_CHAR. */
  LexerState to;
} Transition;

typedef struct {
  LexerState state;
  TokenType token_type;
} FinalState;

/** STATE_S currently has 26 edges; all other rows are shorter. */
#define MAX_TRANSITIONS_PER_STATE 26U

typedef struct {
  Transition items[MAX_TRANSITIONS_PER_STATE];
  size_t count;
} StateTransitions;

#define TRANSITION(match_kind, literal, destination)                           \
  {match_kind, literal, destination}
#define TRANSITION_ROW(...)                                                    \
  {.items = {__VA_ARGS__},                                                     \
   .count = sizeof((Transition[]){__VA_ARGS__}) / sizeof(Transition)}

static bool is_digit(int character) {
  return character >= '0' && character <= '9';
}

static bool is_letter(int character) {
  return (character >= 'A' && character <= 'Z') ||
         (character >= 'a' && character <= 'z');
}

/** C: ASCII except LF, double quote, and backslash. */
static bool is_str(int character) {
  return character >= 0 && character <= 127 && character != '\n' &&
         character != '"' && character != '\\';
}

/** M: ASCII except double quote. */
static bool is_str_multiline(int character) {
  return character >= 0 && character <= 127 && character != '"';
}

static bool matches(const Transition *transition, int character) {
  switch (transition->match) {
  case MATCH_CHAR:
    return character == transition->character;
  case MATCH_DIGIT:
    return is_digit(character);
  case MATCH_NONZERO_DIGIT:
    return is_digit(character) && character != '0';
  case MATCH_LETTER:
    return is_letter(character);
  case MATCH_C:
    return is_str(character);
  case MATCH_M:
    return is_str_multiline(character);
  }
  return false;
}

/**
 * The first index chooses a source state; the second chooses one of its edges.
 * A new edge is added only to the row of its source state. TRANSITION_ROW
 * derives count from that row, so no parallel offset/count table can drift.
 */
static const StateTransitions transitions_by_state[STATE_COUNT] = {
    [STATE_S] = TRANSITION_ROW(TRANSITION(MATCH_CHAR, '0', STATE_INT0),
                               TRANSITION(MATCH_NONZERO_DIGIT, 0, STATE_INT),
                               TRANSITION(MATCH_CHAR, '.', STATE_PERIOD),
                               TRANSITION(MATCH_CHAR, '"', STATE_Q1),
                               TRANSITION(MATCH_LETTER, 0, STATE_IDENTIFIER),
                               TRANSITION(MATCH_CHAR, '_', STATE_UNDERSCORE),
                               TRANSITION(MATCH_CHAR, '=', STATE_ASSIGN),
                               TRANSITION(MATCH_CHAR, '!', STATE_NOT),
                               TRANSITION(MATCH_CHAR, '&', STATE_AND_PREFIX),
                               TRANSITION(MATCH_CHAR, '>', STATE_GREATER),
                               TRANSITION(MATCH_CHAR, '<', STATE_LESS),
                               TRANSITION(MATCH_CHAR, '@', STATE_AT),
                               TRANSITION(MATCH_CHAR, '+', STATE_PLUS),
                               TRANSITION(MATCH_CHAR, '-', STATE_MINUS),
                               TRANSITION(MATCH_CHAR, '*', STATE_MULTIPLY),
                               TRANSITION(MATCH_CHAR, '/', STATE_DIVIDE),
                               TRANSITION(MATCH_CHAR, '(', STATE_LEFT_PAREN),
                               TRANSITION(MATCH_CHAR, ')', STATE_RIGHT_PAREN),
                               TRANSITION(MATCH_CHAR, ':', STATE_COLON),
                               TRANSITION(MATCH_CHAR, '[', STATE_LEFT_BRACKET),
                               TRANSITION(MATCH_CHAR, ']', STATE_RIGHT_BRACKET),
                               TRANSITION(MATCH_CHAR, ';', STATE_SEMICOLON),
                               TRANSITION(MATCH_CHAR, '\n', STATE_NEWLINE),
                               TRANSITION(MATCH_CHAR, ',', STATE_COMMA),
                               TRANSITION(MATCH_CHAR, '|', STATE_UNION)),
    [STATE_INT0] =
        TRANSITION_ROW(TRANSITION(MATCH_DIGIT, 0, STATE_DOUBLE_STARTS_ZERO),
                       TRANSITION(MATCH_CHAR, '_', STATE_U_DOUBLE_STARTS_ZERO),
                       TRANSITION(MATCH_CHAR, '.', STATE_NUM_DOT),
                       TRANSITION(MATCH_CHAR, 'e', STATE_EXP),
                       TRANSITION(MATCH_CHAR, 'E', STATE_EXP)),
    [STATE_DOUBLE_STARTS_ZERO] =
        TRANSITION_ROW(TRANSITION(MATCH_DIGIT, 0, STATE_DOUBLE_STARTS_ZERO),
                       TRANSITION(MATCH_CHAR, '.', STATE_NUM_DOT),
                       TRANSITION(MATCH_CHAR, 'e', STATE_EXP),
                       TRANSITION(MATCH_CHAR, 'E', STATE_EXP),
                       TRANSITION(MATCH_CHAR, '_', STATE_U_DOUBLE_STARTS_ZERO)),
    [STATE_U_DOUBLE_STARTS_ZERO] =
        TRANSITION_ROW(TRANSITION(MATCH_CHAR, '_', STATE_U_DOUBLE_STARTS_ZERO),
                       TRANSITION(MATCH_DIGIT, 0, STATE_DOUBLE_STARTS_ZERO)),
    [STATE_INT] = TRANSITION_ROW(TRANSITION(MATCH_DIGIT, 0, STATE_INT),
                                 TRANSITION(MATCH_CHAR, '_', STATE_U_INT),
                                 TRANSITION(MATCH_CHAR, '.', STATE_NUM_DOT),
                                 TRANSITION(MATCH_CHAR, 'e', STATE_EXP),
                                 TRANSITION(MATCH_CHAR, 'E', STATE_EXP)),
    [STATE_U_INT] = TRANSITION_ROW(TRANSITION(MATCH_CHAR, '_', STATE_U_INT),
                                   TRANSITION(MATCH_DIGIT, 0, STATE_INT)),
    [STATE_NUM_DOT] = TRANSITION_ROW(TRANSITION(MATCH_DIGIT, 0, STATE_DOUBLE)),
    [STATE_PERIOD] = TRANSITION_ROW(TRANSITION(MATCH_DIGIT, 0, STATE_DOUBLE)),
    [STATE_DOUBLE] =
        TRANSITION_ROW(TRANSITION(MATCH_DIGIT, 0, STATE_DOUBLE),
                       TRANSITION(MATCH_CHAR, '_', STATE_U_DOUBLE),
                       TRANSITION(MATCH_CHAR, 'e', STATE_EXP),
                       TRANSITION(MATCH_CHAR, 'E', STATE_EXP),
                       TRANSITION(MATCH_CHAR, 'd', STATE_DOUBLE_FINAL),
                       TRANSITION(MATCH_CHAR, 'D', STATE_DOUBLE_FINAL)),
    [STATE_U_DOUBLE] =
        TRANSITION_ROW(TRANSITION(MATCH_CHAR, '_', STATE_U_DOUBLE),
                       TRANSITION(MATCH_DIGIT, 0, STATE_DOUBLE)),
    [STATE_EXP] = TRANSITION_ROW(TRANSITION(MATCH_DIGIT, 0, STATE_E_DOUBLE),
                                 TRANSITION(MATCH_CHAR, '+', STATE_S_EXP),
                                 TRANSITION(MATCH_CHAR, '-', STATE_S_EXP)),
    [STATE_S_EXP] = TRANSITION_ROW(TRANSITION(MATCH_DIGIT, 0, STATE_E_DOUBLE)),
    [STATE_E_DOUBLE] =
        TRANSITION_ROW(TRANSITION(MATCH_CHAR, '_', STATE_E_U_DOUBLE),
                       TRANSITION(MATCH_DIGIT, 0, STATE_E_DOUBLE),
                       TRANSITION(MATCH_CHAR, 'd', STATE_DOUBLE_FINAL),
                       TRANSITION(MATCH_CHAR, 'D', STATE_DOUBLE_FINAL)),
    [STATE_E_U_DOUBLE] =
        TRANSITION_ROW(TRANSITION(MATCH_CHAR, '_', STATE_E_U_DOUBLE),
                       TRANSITION(MATCH_DIGIT, 0, STATE_E_DOUBLE)),
    [STATE_Q1] = TRANSITION_ROW(TRANSITION(MATCH_C, 0, STATE_IN),
                                TRANSITION(MATCH_CHAR, '\\', STATE_ESC),
                                TRANSITION(MATCH_CHAR, '"', STATE_STR_EMPTY)),
    [STATE_STR_EMPTY] =
        TRANSITION_ROW(TRANSITION(MATCH_CHAR, '"', STATE_IN_MULTILINE)),
    [STATE_IN] = TRANSITION_ROW(TRANSITION(MATCH_C, 0, STATE_IN),
                                TRANSITION(MATCH_CHAR, '\\', STATE_ESC),
                                TRANSITION(MATCH_CHAR, '"', STATE_STR_NORMAL)),
    [STATE_ESC] = TRANSITION_ROW(TRANSITION(MATCH_CHAR, '\\', STATE_IN),
                                 TRANSITION(MATCH_CHAR, '"', STATE_IN),
                                 TRANSITION(MATCH_CHAR, 'n', STATE_IN),
                                 TRANSITION(MATCH_CHAR, 'r', STATE_IN),
                                 TRANSITION(MATCH_CHAR, 't', STATE_IN)),
    [STATE_IN_MULTILINE] =
        TRANSITION_ROW(TRANSITION(MATCH_M, 0, STATE_IN_MULTILINE),
                       TRANSITION(MATCH_CHAR, '"', STATE_Q1_MULTI)),
    [STATE_Q1_MULTI] =
        TRANSITION_ROW(TRANSITION(MATCH_CHAR, '"', STATE_Q2_MULTI),
                       TRANSITION(MATCH_M, 0, STATE_IN_MULTILINE)),
    [STATE_Q2_MULTI] =
        TRANSITION_ROW(TRANSITION(MATCH_CHAR, '"', STATE_STR_MULTILINE),
                       TRANSITION(MATCH_M, 0, STATE_IN_MULTILINE)),
    /* Quotes after the closing """ extend the literal (Scala: """a"""" is a").
     */
    [STATE_STR_MULTILINE] =
        TRANSITION_ROW(TRANSITION(MATCH_CHAR, '"', STATE_STR_MULTILINE)),
    [STATE_UNDERSCORE] =
        TRANSITION_ROW(TRANSITION(MATCH_LETTER, 0, STATE_IDENTIFIER),
                       TRANSITION(MATCH_DIGIT, 0, STATE_IDENTIFIER),
                       TRANSITION(MATCH_CHAR, '_', STATE_IDENTIFIER)),
    [STATE_IDENTIFIER] =
        TRANSITION_ROW(TRANSITION(MATCH_LETTER, 0, STATE_IDENTIFIER),
                       TRANSITION(MATCH_DIGIT, 0, STATE_IDENTIFIER),
                       TRANSITION(MATCH_CHAR, '_', STATE_IDENTIFIER)),
    [STATE_ASSIGN] = TRANSITION_ROW(TRANSITION(MATCH_CHAR, '=', STATE_EQUAL),
                                    TRANSITION(MATCH_CHAR, '>', STATE_ARROW)),
    [STATE_AND_PREFIX] = TRANSITION_ROW(TRANSITION(MATCH_CHAR, '&', STATE_AND)),
    [STATE_NOT] = TRANSITION_ROW(TRANSITION(MATCH_CHAR, '=', STATE_NOT_EQUAL)),
    [STATE_GREATER] =
        TRANSITION_ROW(TRANSITION(MATCH_CHAR, '=', STATE_GREATER_EQUAL)),
    [STATE_LESS] = TRANSITION_ROW(TRANSITION(MATCH_CHAR, '=', STATE_LESS_EQUAL),
                                  TRANSITION(MATCH_CHAR, '-', STATE_GENERATOR)),
    [STATE_AT] = TRANSITION_ROW(TRANSITION(MATCH_CHAR, 'm', STATE_AT_M)),
    [STATE_AT_M] = TRANSITION_ROW(TRANSITION(MATCH_CHAR, 'a', STATE_AT_MA)),
    [STATE_AT_MA] = TRANSITION_ROW(TRANSITION(MATCH_CHAR, 'i', STATE_AT_MAI)),
    [STATE_AT_MAI] = TRANSITION_ROW(TRANSITION(MATCH_CHAR, 'n', STATE_AT_MAIN)),
    [STATE_PLUS] = TRANSITION_ROW(TRANSITION(MATCH_CHAR, '+', STATE_CONCAT)),
    [STATE_NEWLINE] =
        TRANSITION_ROW(TRANSITION(MATCH_CHAR, ' ', STATE_NEWLINE_SPACES),
                       TRANSITION(MATCH_CHAR, '\t', STATE_NEWLINE_TABS),
                       TRANSITION(MATCH_CHAR, '\n', STATE_NEWLINE)),
    [STATE_NEWLINE_SPACES] =
        TRANSITION_ROW(TRANSITION(MATCH_CHAR, ' ', STATE_NEWLINE_SPACES),
                       TRANSITION(MATCH_CHAR, '\t', STATE_NEWLINE_TABS),
                       TRANSITION(MATCH_CHAR, '\n', STATE_NEWLINE)),
    [STATE_NEWLINE_TABS] =
        TRANSITION_ROW(TRANSITION(MATCH_CHAR, ' ', STATE_NEWLINE_TABS),
                       TRANSITION(MATCH_CHAR, '\t', STATE_NEWLINE_TABS),
                       TRANSITION(MATCH_CHAR, '\n', STATE_NEWLINE)),
    [STATE_UNION] = TRANSITION_ROW(TRANSITION(MATCH_CHAR, '|', STATE_OR)),
};

#undef TRANSITION_ROW
#undef TRANSITION

static const FinalState final_states[] = {
    {STATE_INT0, TOKEN_INT},
    {STATE_INT, TOKEN_INT},
    {STATE_DOUBLE, TOKEN_DOUBLE},
    {STATE_E_DOUBLE, TOKEN_DOUBLE},
    {STATE_DOUBLE_FINAL, TOKEN_DOUBLE},
    {STATE_STR_EMPTY, TOKEN_STRING},
    {STATE_STR_NORMAL, TOKEN_STRING},
    {STATE_STR_MULTILINE, TOKEN_STRING},
    {STATE_IDENTIFIER, TOKEN_IDENTIFIER},
    {STATE_UNDERSCORE, TOKEN_WILDCARD},
    {STATE_ASSIGN, TOKEN_ASSIGN},
    {STATE_EQUAL, TOKEN_EQUAL},
    {STATE_NOT, TOKEN_NOT},
    {STATE_AND, TOKEN_AND},
    {STATE_NOT_EQUAL, TOKEN_NOT_EQUAL},
    {STATE_ARROW, TOKEN_ARROW},
    {STATE_GREATER, TOKEN_GREATER},
    {STATE_LESS, TOKEN_LESS},
    {STATE_GREATER_EQUAL, TOKEN_GREATER_EQUAL},
    {STATE_LESS_EQUAL, TOKEN_LESS_EQUAL},
    {STATE_GENERATOR, TOKEN_FOR_GENERATOR},
    {STATE_AT_MAIN, TOKEN_MAIN_ANNOTATION},
    {STATE_PLUS, TOKEN_PLUS},
    {STATE_CONCAT, TOKEN_LIST_CONCAT},
    {STATE_MINUS, TOKEN_MINUS},
    {STATE_MULTIPLY, TOKEN_MULTIPLY},
    {STATE_DIVIDE, TOKEN_DIVIDE},
    {STATE_LEFT_PAREN, TOKEN_LEFT_PAREN},
    {STATE_RIGHT_PAREN, TOKEN_RIGHT_PAREN},
    {STATE_COLON, TOKEN_COLON},
    {STATE_LEFT_BRACKET, TOKEN_LEFT_BRACKET},
    {STATE_RIGHT_BRACKET, TOKEN_RIGHT_BRACKET},
    {STATE_SEMICOLON, TOKEN_SEMICOLON},
    {STATE_NEWLINE, TOKEN_NEWLINE},
    {STATE_NEWLINE_SPACES, TOKEN_NEWLINE},
    {STATE_PERIOD, TOKEN_DOT},
    {STATE_COMMA, TOKEN_COMMA},
    {STATE_UNION, TOKEN_UNION},
    {STATE_OR, TOKEN_OR},
};

bool lexer_dfa_next_state(LexerState from, int character, LexerState *to) {
  const StateTransitions row = transitions_by_state[from];
  size_t index;

  for (index = 0U; index < row.count; index += 1U) {
    if (matches(&row.items[index], character)) {
      *to = row.items[index].to;
      return true;
    }
  }
  return false;
}

bool lexer_dfa_final_token(LexerState state, TokenType *token_type) {
  size_t index;

  for (index = 0U; index < sizeof(final_states) / sizeof(final_states[0]);
       index += 1U) {
    if (final_states[index].state == state) {
      *token_type = final_states[index].token_type;
      return true;
    }
  }
  return false;
}
