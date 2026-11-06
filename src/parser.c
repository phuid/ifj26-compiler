/**
 * @file parser.c
 * @brief Syntactic analysis of IFJ26 source code.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was partially generated using an LLM agent based on
 *       specifikace.txt.
 */

#include "parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Translates one lexer result to the parser status boundary.
 * @param status Lexer result returned by @ref lexer_pls.
 * @return PARSER_OK only for LEXER_OK; all other results are parser outcomes.
 */
static ParserStatus parser_status_from_lexer(LexerStatus status) {
  if (status == LEXER_OK) {
    return PARSER_OK;
  }
  if (status == LEXER_INVALID_ARGUMENT) {
    return PARSER_INVALID_ARGUMENT;
  }
  return PARSER_LEXER_ERROR;
}

/// TODO: Implement the grammar productions for code blocks
static ParserStatus p_code_block(Lexer *lexer, Program *program,
                                 Token *first_token);

/** Initializes the owned FunctionTable that anchors every parsed function. */
ParserStatus program_init(Program *program) {
  if (program == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }
  if (function_table_init(&program->functions) != FUNCTION_TABLE_OK) {
    return PARSER_INTERNAL_ERROR;
  }
  program->initialized = true;
  return PARSER_OK;
}

/** Releases the complete semantic output without touching the borrowed lexer.
 */
void program_destroy(Program *program) {
  if (program == NULL || !program->initialized) {
    return;
  }

  function_table_destroy(&program->functions);
  program->initialized = false;
}

static ParserStatus p_args(Lexer *lexer, Program *program) {
  if (lexer == NULL || program == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  // Parse brackets and arguments

  return PARSER_OK;
}

/**
 * @brief Consumes one contiguous lexer-emitted INDENT delta.
 * @details The first token that is not TOKEN_INDENT remains initialized in
 * @p lookahead and transfers to the caller. Every consumed indentation token
 * is released here.
 * @param lexer Initialized lexer producing layout tokens.
 * @param lookahead Output storage receiving the first non-INDENT token.
 * @param indent_delta Output number of consumed TOKEN_INDENT tokens.
 * @return PARSER_OK, PARSER_INVALID_ARGUMENT, PARSER_INTERNAL_ERROR, or a
 * translated lexer failure.
 */
static ParserStatus parser_consume_indent_delta(Lexer *lexer, Token *lookahead,
                                                size_t *indent_delta) {
  LexerStatus lexer_status;

  if (lexer == NULL || lookahead == NULL || indent_delta == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }
  *indent_delta = 0U;
  while (true) {
    lexer_status = lexer_pls(lexer, lookahead);
    if (lexer_status != LEXER_OK) {
      return parser_status_from_lexer(lexer_status);
    }
    if (lookahead->type != TOKEN_INDENT) {
      return PARSER_OK;
    }
    if (*indent_delta == SIZE_MAX) {
      token_destroy(lookahead);
      return PARSER_INTERNAL_ERROR;
    }
    *indent_delta += 1U;
    token_destroy(lookahead);
  }
}

/**
 * @brief Consumes exactly one matching DEDENT delta after a nested construct.
 * @param lexer Initialized lexer producing layout tokens.
 * @param indent_delta Number of TOKEN_DEDENT tokens required.
 * @return PARSER_OK when all requested DEDENTs occur in order, a syntax error
 * for another token, or a translated lexer failure.
 */
static ParserStatus parser_consume_dedents(Lexer *lexer, size_t indent_delta) {
  Token lookahead;
  LexerStatus lexer_status;

  if (lexer == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }
  while (indent_delta > 0U) {
    lexer_status = lexer_pls(lexer, &lookahead);
    if (lexer_status != LEXER_OK) {
      return parser_status_from_lexer(lexer_status);
    }
    if (lookahead.type != TOKEN_DEDENT) {
      token_destroy(&lookahead);
      return PARSER_SYNTAX_ERROR;
    }
    token_destroy(&lookahead);
    indent_delta -= 1U;
  }
  return PARSER_OK;
}

/**
 * @brief Parses a function after the top-level parser consumed its @c def.
 * @details This intentionally remains a grammar stub. The future rule will
 * call @ref lexer_pls itself to consume the function name, parameters, type,
 * and body instead of receiving a shared parser lookahead buffer.
 * @param lexer Initialized lexer positioned after @c def.
 * @return PARSER_OK until the function grammar is specified.
 */
static ParserStatus p_function(Lexer *lexer, Program *program) {
  if (lexer == NULL || program == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  Token lookahead;
  LexerStatus lexer_status = lexer_pls(lexer, &lookahead);
  ParserStatus parser_status;

  if (lexer_status != LEXER_OK) {
    return parser_status_from_lexer(lexer_status);
  }

  /// Get the function name and add it to the program's function table.
  if (lookahead.type != TOKEN_IDENTIFIER ||
      !identifier_is_valid(lookahead.value.string_value)) {
    token_destroy(&lookahead);
    return PARSER_SYNTAX_ERROR;
  }

  FunctionSymbol *function;
  if (function_table_get_or_create(&program->functions,
                                   lookahead.value.string_value,
                                   &function) != FUNCTION_TABLE_OK) {
    token_destroy(&lookahead);
    return PARSER_INTERNAL_ERROR;
  }
  token_destroy(&lookahead);

  /// Parse the function's argument list
  p_args(lexer, program);

  /// Get the return type and verify it is a known type
  lexer_status = lexer_pls(lexer, &lookahead);
  if (lexer_status != LEXER_OK) {
    return parser_status_from_lexer(lexer_status);
  }

  if (lookahead.type == TOKEN_COLON) {
    token_destroy(&lookahead);
    lexer_status = lexer_pls(lexer, &lookahead);
    if (lexer_status != LEXER_OK) {
      return parser_status_from_lexer(lexer_status);
    }

    if (lookahead.type != TOKEN_IDENTIFIER) {
      token_destroy(&lookahead);
      return PARSER_SYNTAX_ERROR;
    }

    DataType return_type = type_from_identifier(lookahead.value.string_value);
    if (return_type == TYPE_UNKNOWN) {
      token_destroy(&lookahead);
      return PARSER_SYNTAX_ERROR;
    }

    type_entity_resolve(&function->return_type, return_type);
    token_destroy(&lookahead);
  }

  /// Get the indent and parse the function's body
  size_t indent_delta;

  parser_status = parser_consume_indent_delta(lexer, &lookahead, &indent_delta);
  if (parser_status != PARSER_OK) {
    return parser_status;
  }
  if (indent_delta == 0U) {
    token_destroy(&lookahead);
    return PARSER_SYNTAX_ERROR;
  }

  /// Parse the function body as a code block
  parser_status = p_code_block(lexer, program, &lookahead);
  if (parser_status != PARSER_OK) {
    return parser_status;
  }

  // Consume the matching DEDENT tokens after the function body
  return parser_consume_dedents(lexer, indent_delta);
}

/**
 * @brief Parses an @c main function after its annotation was consumed.
 * @details The concrete grammar remains deliberately undefined for now.
 * @param lexer Initialized lexer positioned after @c @main.
 * @return PARSER_OK until the main-function grammar is specified.
 */
static ParserStatus p_main_function(Lexer *lexer, Program *program) {
  if (lexer == NULL || program == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  // TODO

  return PARSER_OK;
}

/**
 * @brief Consumes a program directly from its lexer.
 * @details Each future recursive production owns its own @ref lexer_pls call;
 * no shared parser lookahead exists between productions.
 * @param lexer Initialized lexer positioned at the next top-level token.
 * @param program Initialized output owner for the parsed program.
 * @return PARSER_OK at end of input or the first lexer/parser failure.
 */
ParserStatus p_program(Lexer *lexer, Program *program) {
  bool main_function_found = false;
  LexerStatus lexer_status;

  if (lexer == NULL || program == NULL || !program->initialized) {
    return PARSER_INVALID_ARGUMENT;
  }

  /** Consume each top-level token directly from the lexer. */
  for (;;) {
    Token token;
    ParserStatus status;

    lexer_status = lexer_pls(lexer, &token);
    if (lexer_status == LEXER_EOF) {
      return PARSER_OK;
    }
    if (lexer_status != LEXER_OK) {
      return parser_status_from_lexer(lexer_status);
    }

    /** Dispatch only the currently sketched top-level constructs. */
    if (token.type == TOKEN_IDENTIFIER &&
        strcmp(token.value.string_value, "def") == 0) {
      status = p_function(lexer, program);
    } else if (token.type == TOKEN_MAIN_ANNOTATION) {
      if (main_function_found) {
        status = PARSER_SYNTAX_ERROR;
      } else {
        main_function_found = true;
        status = p_main_function(lexer, program);
      }
    } else {
      status = PARSER_SYNTAX_ERROR;
    }

    /** Release the token after the selected production processed its kind. */
    token_destroy(&token);
    if (status != PARSER_OK) {
      return status;
    }
  }
}
