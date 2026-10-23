#ifndef SRC_LEXER_H_
#define SRC_LEXER_H_

#include <stddef.h>
#include <stdio.h>

/*
 * AI-generated code block — lexer token model.
 * Model: gpt-5.6-terra (OpenAI Codex via Hermes Agent).
 * Prompt:
 *   Define the token kinds and token representation required by the DFA in
 *   KA.md: a token owns its lexeme, numeric tokens expose a parsed value, and
 *   source positions are 1-based. This block defines data only, not scanning.
 *   Use ISO C11 and standard-library types.
 */
/** @brief Names every terminal token the lexer may return. */
typedef enum {
  TOKEN_INT,
  TOKEN_DOUBLE,
  TOKEN_STRING,
  TOKEN_IDENTIFIER,
  TOKEN_WILDCARD,
  TOKEN_ASSIGN,
  TOKEN_EQUAL,
  TOKEN_NOT,
  TOKEN_AND,
  TOKEN_NOT_EQUAL,
  TOKEN_ARROW,
  TOKEN_GREATER,
  TOKEN_LESS,
  TOKEN_GREATER_EQUAL,
  TOKEN_LESS_EQUAL,
  TOKEN_GENERATOR,
  TOKEN_MAIN_ANNOTATION,
  TOKEN_SPACE,
  TOKEN_PLUS,
  TOKEN_CONCAT,
  TOKEN_MINUS,
  TOKEN_MULTIPLY,
  TOKEN_DIVIDE,
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_COLON,
  TOKEN_LEFT_BRACKET,
  TOKEN_RIGHT_BRACKET,
  TOKEN_SEMICOLON,
  TOKEN_NEWLINE,
  TOKEN_DOT,
  TOKEN_COMMA,
  TOKEN_UNION,
  TOKEN_OR
} TokenType;

/**
 * @brief Holds the parsed value or owned source spelling of a token.
 * @details `string_value` is owned by a string-like token and must be released
 * with @ref token_destroy; numeric alternatives require no cleanup.
 */
typedef union {
  long long int_value;
  double double_value;
  char *string_value; // (string or identifier) caller must free after
                      // token_destroy
} TokenValue;

/**
 * @brief Represents one lexer result at a 1-based source position.
 * @details The caller owns `value.string_value` for every non-numeric token
 * until it calls @ref token_destroy.
 */
typedef struct {
  TokenType type;
  TokenValue value;
  size_t line;
  size_t column;
} Token;

/*
 * AI-generated code block — lexer state and public API.
 * Model: gpt-5.6-terra (OpenAI Codex via Hermes Agent).
 * Prompt:
 *   Define the statuses, one-byte-lookahead Lexer state, and public API below.
 *   lexer borrows FILE*, lexer_next_token transfers a lexeme to its caller, and
 *   token_destroy releases it. Do not implement DFA transitions in this header.
 *   Use ISO C11 and standard-library types.
 */
/** Reports success, end of input, or the reason tokenization stopped. */
typedef enum {
  LEXER_OK,
  LEXER_EOF,
  LEXER_INVALID_ARGUMENT,
  LEXER_INVALID_TOKEN,
  LEXER_OUT_OF_MEMORY
} LexerStatus;

/** @brief Stores the borrowed input stream and its current one-byte lookahead.
 */
typedef struct {
  FILE *input;
  int lookahead;
  size_t line;
  size_t column;
} Lexer;

/**
 * @brief Initializes a lexer and reads its initial lookahead byte.
 * @param lexer Lexer storage to initialize.
 * @param input Open stream borrowed until @ref lexer_destroy.
 * @return LEXER_OK or LEXER_INVALID_ARGUMENT.
 */
LexerStatus lexer_init(Lexer *lexer, FILE *input);

/**
 * @brief Produces one token or reports the lexical stopping condition.
 * @param lexer Initialized lexer.
 * @param token Output token storage on LEXER_OK.
 * @return LEXER_OK, LEXER_EOF, or a lexer error status.
 */
LexerStatus lexer_pls(Lexer *lexer, Token *token);

/**
 * @brief Invalidates lexer state without closing its borrowed stream.
 * @param lexer Lexer to invalidate; null is accepted.
 */
void lexer_destroy(Lexer *lexer);

/**
 * @brief Releases text owned by a successfully returned non-numeric token.
 * @param token Token to release; null is accepted.
 */
void token_destroy(Token *token);

#endif // SRC_LEXER_H_
