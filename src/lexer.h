/**
 * @file lexer.h
 * @brief Public lexer interface and token representation.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was partially generated using an LLM agent based on
 *       specifikace.txt.
 */

#ifndef SRC_LEXER_H_
#define SRC_LEXER_H_

#include <stdbool.h>
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
  TOKEN_FOR_GENERATOR,
  TOKEN_MAIN_ANNOTATION,
  TOKEN_INDENT,
  TOKEN_DEDENT,
  TOKEN_PLUS,
  TOKEN_LIST_CONCAT,
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
  TOKEN_OR,
  /* Keyword tokens produced from TOKEN_IDENTIFIER by the keyword table
   * (keyword.h). Like identifiers, they own their keyword spelling. */
  TOKEN_KW_DEF,
  TOKEN_KW_DO,
  TOKEN_KW_DOUBLE,
  TOKEN_KW_ELSE,
  TOKEN_KW_IF,
  TOKEN_KW_IMPORT,
  TOKEN_KW_INT,
  TOKEN_KW_RETURN,
  TOKEN_KW_STRING,
  TOKEN_KW_THEN,
  TOKEN_KW_UNIT,
  TOKEN_KW_VAL,
  TOKEN_KW_VAR,
  TOKEN_KW_WHILE,
  TOKEN_KW_BOOLEAN,
  TOKEN_KW_TRUE,
  TOKEN_KW_FALSE,
  TOKEN_KW_CASE,
  TOKEN_KW_CLASS,
  TOKEN_KW_EXTENSION,
  TOKEN_KW_FOR,
  TOKEN_KW_LIST,
  TOKEN_KW_MATCH,
  TOKEN_KW_TYPE,
  /** End of input; set by @ref lexer_pls when it returns LEXER_EOF. */
  TOKEN_EOF,
  /** No token loaded yet (after @ref lexer_init) or the last @ref lexer_pls
   * failed. Not a lexical error by itself; errors are reported through
   * LEXER_INVALID_TOKEN. */
  TOKEN_INVALID
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

/**
 * @brief Stores the borrowed input stream, its one-byte lookahead and the
 * current lookahead token.
 * @details `token` is owned by the lexer. It is valid after @ref lexer_pls
 * returned LEXER_OK and is replaced (its text released) by the next call to
 * @ref lexer_pls or by @ref lexer_destroy.
 */
typedef struct {
  char *input_str;
  size_t input_str_pos;

  int lookahead;
  Token token;
  size_t line;
  size_t column;
  size_t current_indent;
  ptrdiff_t pending_indent_delta;
  /// If false, a NEWLINE does not change the indentation: no INDENT or DEDENT
  /// is emitted and @c current_indent is unchanged. The parser clears it right
  /// before reading the token after `(` / `[` and sets it again right before
  /// reading the token after the matching `)` / `]`. Defaults to true.
} Lexer;

/**
 * @brief Initializes a lexer and reads its initial lookahead byte.
 * @param lexer Lexer storage to initialize.
 * @param input_str Borrowed, null-terminated string of characters to tokenize.
 * @return LEXER_OK or LEXER_INVALID_ARGUMENT.
 */
LexerStatus lexer_init(Lexer *lexer, char *input_str);

/**
 * @brief Adjusts the current indentation level.
 * @details A negative delta is rejected when it would make
 * @c current_indent negative. Positive overflow of the @c size_t counter is
 * rejected as well. Pending INDENT/DEDENT tokens are not changed.
 * @param lexer Initialized lexer.
 * @param indent_delta Signed indentation adjustment.
 * @return LEXER_OK, or LEXER_INVALID_ARGUMENT for an invalid adjustment.
 */
LexerStatus lexer_change_current_indent(Lexer *lexer, ptrdiff_t indent_delta);

/**
 * @brief Replaces the lexer's lookahead token with the next token.
 * @details The previous `lexer->token` is released first. On LEXER_OK the new
 * token is available in `lexer->token`, on LEXER_EOF it is @ref TOKEN_EOF and
 * on an error status it is @ref TOKEN_INVALID.
 * @param lexer Initialized lexer.
 * @return LEXER_OK, LEXER_EOF, or a lexer error status.
 */
LexerStatus lexer_pls(Lexer *lexer);

/**
 * @brief Releases the lookahead token and invalidates lexer state without
 * closing its borrowed stream.
 * @param lexer Lexer to invalidate; null is accepted.
 */
void lexer_destroy(Lexer *lexer);

/**
 * @brief Releases text owned by a successfully returned non-numeric token.
 * @param token Token to release; null is accepted.
 */
void token_destroy(Token *token);

#endif // SRC_LEXER_H_
