#include "parser.h"

#include <stdbool.h>
#include <string.h>

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
  LexerStatus status = lexer_pls(lexer, &lookahead);

  if (status != LEXER_OK) {
    return parser_status_from_lexer(status);
  }

  if (lookahead.type != TOKEN_IDENTIFIER) {
    token_destroy(&lookahead);
    return PARSER_SYNTAX_ERROR;
  }

  if (function_table_add(&program->functions, lookahead.value.string_value) !=
      FUNCTION_TABLE_OK) {
    token_destroy(&lookahead);
    return PARSER_INTERNAL_ERROR;
  }
  token_destroy(&lookahead);

  p_args(lexer, program);

  int indent_level = 0;
  for (;;) {
    status = lexer_pls(lexer, &lookahead);

    if (status != LEXER_OK) {
      return parser_status_from_lexer(status);
    }

    // Consume all pending layout tokens before consuming any new raw token.
    if (lookahead.type == TOKEN_INDENT) {
      indent_level += 1;
    } else {
      break;
    }

    token_destroy(&lookahead);
  }

  if (indent_level <= 0) {
    return PARSER_SYNTAX_ERROR;
  }

  p_code_block(lexer, program);

  for (; indent_level > 0; indent_level--) {
    status = lexer_pls(lexer, &lookahead);

    if (status != LEXER_OK) {
      return parser_status_from_lexer(status);
    }

    // Consume all pending layout tokens before consuming any new raw token.
    if (lookahead.type != TOKEN_DEDENT) {
      return PARSER_SYNTAX_ERROR;
    }

    token_destroy(&lookahead);
  }

  return PARSER_OK;
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
