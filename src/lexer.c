#include "lexer.h"
#include "lexer_dfa.h"

#include <stdbool.h>
#include <stdlib.h>

/*
 * AI-generated code block — generic table-driven lexer runner.
 * Model: gpt-5.6-terra (OpenAI Codex via Hermes Agent).
 * Prompt:
 *   Implement only the generic runner for the declarative IFJ26 DFA in
 *   lexer_dfa.c. Lexer owns one lookahead character. Repeatedly either follow
 *   an explicit transition by consuming lookahead, finish the current token
 *   from the final-state TokenType without consuming lookahead, or return a
 *   lexical error. Preserve 1-based positions and the lexer.h ownership
 *   contract. Do not encode language transitions in this file. Use ISO C11.
 */
/**
 * @brief Owns the in-progress spelling of one token.
 *
 * @details The builder borrows @ref Lexer. Its text is transferred to a
 * text-valued Token on success, or freed by the caller on any error path.
 */
typedef struct {
  Lexer *lexer;
  char *text;
  size_t length;
  size_t capacity;
} TokenBuilder;

/**
 * @brief Initializes one token builder and records the token start position.
 * @param builder Builder storage to initialize.
 * @param lexer Lexer that owns the current lookahead.
 * @param token Output token whose source position is initialized.
 * @return LEXER_OK on success, or LEXER_OUT_OF_MEMORY if allocation fails.
 */
static LexerStatus token_builder_init(TokenBuilder *builder, Lexer *lexer,
                                      Token *token) {
  char *text = malloc(16U);
  if (text == NULL) {
    return LEXER_OUT_OF_MEMORY;
  }

  *builder = (TokenBuilder){
      .lexer = lexer, .text = text, .length = 0U, .capacity = 16U};
  token->line = lexer->line;
  token->column = lexer->column;

  return LEXER_OK;
}

/**
 * @brief Releases untransferred token text and resets builder ownership.
 * @param builder Builder to destroy; null is not accepted.
 */
static void token_builder_destroy(TokenBuilder *builder) {
  free(builder->text);
  builder->text = NULL;
  builder->length = 0U;
  builder->capacity = 0U;
}

/**
 * @brief Appends one consumed source character and preserves NUL termination.
 * @param builder In-progress token storage.
 * @param character Character already accepted by the DFA.
 * @return LEXER_OK, or LEXER_OUT_OF_MEMORY without changing ownership.
 */
static LexerStatus append_char(TokenBuilder *builder, int character) {
  char *resized;

  /// Reallocates the builder's text to double size buffer if necessary.
  if (builder->length + 1U >= builder->capacity) {
    size_t new_capacity = builder->capacity * 2U;
    resized = realloc(builder->text, new_capacity);
    if (resized == NULL) {
      return LEXER_OUT_OF_MEMORY;
    }
    builder->text = resized;
    builder->capacity = new_capacity;
  }

  /// Appends the character and maintains a NUL terminator.
  builder->text[builder->length] = (char)character;
  builder->length += 1U;
  builder->text[builder->length] = '\0';
  return LEXER_OK;
}

/**
 * @brief Consumes the current lookahead after the DFA selected a transition.
 * @details Appends lookahead to the builder, updates the 1-based position, and
 * replaces lookahead with the next byte from the caller-owned input stream.
 * @param builder Token under construction.
 * @return LEXER_OK or LEXER_OUT_OF_MEMORY.
 */
static LexerStatus consume(TokenBuilder *builder) {
  Lexer *lexer = builder->lexer;
  LexerStatus status = append_char(builder, lexer->lookahead);

  if (status != LEXER_OK) {
    return status;
  }

  /// Move the position
  if (lexer->lookahead == '\n') {
    lexer->line += 1U;
    lexer->column = 1U;
  } else {
    lexer->column += 1U;
  }

  /// Read new lookahead
  lexer->lookahead = fgetc(lexer->input);
  return LEXER_OK;
}

/**
 * @brief Reports whether a token type owns @c value.string_value.
 * @param type Token kind to inspect.
 * @return True for non-numeric tokens, false for TOKEN_INT and TOKEN_DOUBLE.
 */
static bool token_owns_text(TokenType type) {
  return type != TOKEN_INT && type != TOKEN_DOUBLE;
}

/**
 * @brief Transfers a completed builder into a public token.
 * @details Numeric spellings are normalized by removing separators and a final
 * d/D suffix before conversion. All other token types take ownership of the
 * preserved source spelling.
 * @param builder Completed token spelling.
 * @param token Output token.
 * @param type Token type obtained from the final-state table.
 * @return LEXER_OK, LEXER_INVALID_TOKEN, or LEXER_OUT_OF_MEMORY.
 */
static LexerStatus finish_token(TokenBuilder *builder, Token *token,
                                TokenType type) {
  char *end;
  size_t read_index;
  size_t write_index = 0U;

  token->type = type;

  if (type == TOKEN_INT || type == TOKEN_DOUBLE) {

    /// Normalize numeric spellings by removing separators
    for (read_index = 0U; read_index < builder->length; read_index += 1U) {
      if (builder->text[read_index] != '_') {
        builder->text[write_index] = builder->text[read_index];
        write_index += 1U;
      }
    }
    builder->text[write_index] = '\0';

    /// Remove a final d/D suffix from double literals.
    if (type == TOKEN_DOUBLE && write_index > 0U &&
        (builder->text[write_index - 1U] == 'd' ||
         builder->text[write_index - 1U] == 'D')) {
      builder->text[write_index - 1U] = '\0';
    }

    /// Convert to numeric value
    if (type == TOKEN_INT) {
      token->value.int_value = strtoll(builder->text, &end, 10);
    } else {
      token->value.double_value = strtod(builder->text, &end);
    }

    /// Verify the whole text was converted
    if (*end != '\0') {
      return LEXER_INVALID_TOKEN;
    }

    free(builder->text);
    builder->text = NULL;
    return LEXER_OK;
  }

  token->value.string_value = builder->text;
  builder->text = NULL;
  return LEXER_OK;
}

/**
 * @brief Initializes a lexer over a caller-owned input stream.
 * @param lexer Storage to initialize.
 * @param input Open stream borrowed for the lexer lifetime.
 * @return LEXER_OK, or LEXER_INVALID_ARGUMENT for a null argument.
 */
LexerStatus lexer_init(Lexer *lexer, FILE *input) {
  if (lexer == NULL || input == NULL) {
    return LEXER_INVALID_ARGUMENT;
  }

  lexer->input = input;
  lexer->lookahead = fgetc(input);
  lexer->line = 1U;
  lexer->column = 1U;
  return LEXER_OK;
}

/**
 * @brief Produces one maximal token by executing the declarative DFA.
 * @details A non-matching lookahead is not consumed when the current state is
 * accepting, so it becomes the first character examined for the next token.
 * @param lexer Initialized lexer with one lookahead character.
 * @param token Output storage receiving type, value, and start position.
 * @return LEXER_OK, LEXER_EOF, LEXER_INVALID_TOKEN, or an argument/allocation
 * error.
 */
LexerStatus lexer_pls(Lexer *lexer, Token *token) {
  TokenBuilder builder;
  LexerState state = STATE_S;
  LexerState next_state;
  TokenType token_type;
  LexerStatus status;

  if (lexer == NULL || token == NULL || lexer->input == NULL) {
    return LEXER_INVALID_ARGUMENT;
  }
  if (lexer->lookahead == EOF) {
    return LEXER_EOF;
  }

  token_builder_init(&builder, lexer, token);

  /// Follow the DFA until no transition matches the current lookahead.
  while (lexer_dfa_next_state(state, lexer->lookahead, &next_state)) {
    status = consume(&builder);
    if (status != LEXER_OK) {
      token_builder_destroy(&builder);
      return status;
    }
    state = next_state;
  }

  /// If the state is not final return invalid token
  if (!lexer_dfa_final_token(state, &token_type)) {
    token_builder_destroy(&builder);
    return LEXER_INVALID_TOKEN;
  }

  status = finish_token(&builder, token, token_type);
  token_builder_destroy(&builder);
  return status;
}

/**
 * @brief Invalidates a lexer without closing its borrowed stream.
 * @param lexer Lexer to invalidate; null is accepted.
 */
void lexer_destroy(Lexer *lexer) {
  if (lexer == NULL) {
    return;
  }
  lexer->input = NULL;
  lexer->lookahead = EOF;
  lexer->line = 0U;
  lexer->column = 0U;
}

/**
 * @brief Releases text owned by a successfully returned non-numeric token.
 * @param token Token to clean up; null is accepted.
 */
void token_destroy(Token *token) {
  if (token != NULL && token_owns_text(token->type)) {
    free(token->value.string_value);
    token->value.string_value = NULL;
  }
}
