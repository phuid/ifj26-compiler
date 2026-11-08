/**
 * @file lexer_dfa.h
 * @brief Internal declarative DFA interface for lexical analysis.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was partially generated using an LLM agent based on
 *       specifikace.txt.
 */

#ifndef SRC_LEXER_DFA_H_
#define SRC_LEXER_DFA_H_

#include <stdbool.h>

#include "lexer.h"

/*
 * AI-generated code block — declarative IFJ26 lexical DFA.
 * Model: gpt-5.6-terra (OpenAI Codex via Hermes Agent).
 * Prompt:
 *   Represent the user-supplied KA.md as explicit transition and final-state
 *   tables. A transition may match a literal character or exactly the named
 *   KA character sets DIGIT, nonzero DIGIT, LETTER, C, and M. Expose only
 *   lookup functions; keep the tables static. Do not add implicit/default or
 *   rejection transitions. Use ISO C11.
 */
/**
 * @brief Names the states of the lexical finite automaton from KA.md.
 *
 * @details This is an internal representation. Parser-facing token kinds stay
 * in @ref TokenType and are obtained only from accepting states.
 */
typedef enum {
  STATE_S,
  STATE_INT0,
  STATE_INT,
  STATE_U_INT,
  STATE_NUM_DOT,
  STATE_PERIOD,
  STATE_DOUBLE,
  STATE_U_DOUBLE,
  STATE_EXP,
  STATE_S_EXP,
  STATE_E_DOUBLE,
  STATE_E_U_DOUBLE,
  STATE_DOUBLE_FINAL,
  STATE_Q1,
  STATE_STR_EMPTY,
  STATE_IN,
  STATE_ESC,
  STATE_STR_NORMAL,
  STATE_IN_MULTILINE,
  STATE_Q1_MULTI,
  STATE_Q2_MULTI,
  STATE_STR_MULTILINE,
  STATE_UNDERSCORE,
  STATE_IDENTIFIER,
  STATE_ASSIGN,
  STATE_EQUAL,
  STATE_AND_PREFIX,
  STATE_AND,
  STATE_NOT,
  STATE_NOT_EQUAL,
  STATE_ARROW,
  STATE_GREATER,
  STATE_LESS,
  STATE_GREATER_EQUAL,
  STATE_LESS_EQUAL,
  STATE_GENERATOR,
  STATE_AT,
  STATE_AT_M,
  STATE_AT_MA,
  STATE_AT_MAI,
  STATE_AT_MAIN,
  STATE_PLUS,
  STATE_CONCAT,
  STATE_MINUS,
  STATE_MULTIPLY,
  STATE_DIVIDE,
  STATE_LEFT_PAREN,
  STATE_RIGHT_PAREN,
  STATE_COLON,
  STATE_LEFT_BRACKET,
  STATE_RIGHT_BRACKET,
  STATE_SEMICOLON,
  STATE_NEWLINE,
  STATE_NEWLINE_SPACES,
  STATE_NEWLINE_TABS,
  STATE_COMMA,
  STATE_UNION,
  STATE_OR,
  STATE_COUNT
} LexerState;

/**
 * @brief Finds an explicit KA.md transition without consuming input.
 * @param from Current DFA state.
 * @param character Lookahead character to match.
 * @param to Receives the target state if a transition exists.
 * @return True if and only if one explicit transition matched.
 */
bool lexer_dfa_next_state(LexerState from, int character, LexerState *to);

/**
 * @brief Maps one accepting DFA state to the parser-facing token type.
 * @param state DFA state to query.
 * @param token_type Receives the token type if the state is accepting.
 * @return True if @p state is final, otherwise false.
 */
bool lexer_dfa_final_token(LexerState state, TokenType *token_type);

#endif // SRC_LEXER_DFA_H_
