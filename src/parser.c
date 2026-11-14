/**
 * @file parser.c
 * @brief Syntactic analysis of IFJ26 source code.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was partially generated using an LLM agent based on
 *       specifikace.txt.
 */

#include "parser.h"

#include "ast.h"
#include "function_table.h"
#include "keyword.h"
#include "lexer.h"
#include "psa.h"
#include "symtable.h"
#include "type.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static ParserStatus parser_consume_indent_delta(Lexer *lexer,
                                                size_t *indent_delta);
static ParserStatus parser_consume_dedents(Lexer *lexer, size_t indent_delta);
static ParserStatus p_code_block(Lexer *lexer, Program *program,
                                 FunctionSymbol *function, SymbolTable *scope,
                                 ASTNode **return_node);

static ParserStatus p_expression(Lexer *lexer, Program *program,
                                 FunctionSymbol *function, SymbolTable *scope,
                                 ASTNode **return_node,
                                 bool consume_newlines_indents,
                                 ptrdiff_t *local_indent);

static ParserStatus p_statement(Lexer *lexer, Program *program,
                                FunctionSymbol *function, SymbolTable *scope,
                                ASTNode **return_node);

/**
 * @brief Translates one lexer result to the parser status boundary.
 * @param status Lexer result returned by @ref lexer_pls.
 * @return PARSER_OK only for LEXER_OK; all other results are parser outcomes.
 */
static ParserStatus parser_status_from_lexer(LexerStatus status) {
  if (status == LEXER_OK || status == LEXER_EOF) {
    return PARSER_OK;
  }
  if (status == LEXER_INVALID_ARGUMENT) {
    return PARSER_INVALID_ARGUMENT;
  }
  return PARSER_LEXER_ERROR;
}

/**
 * @brief Translates a type resolution result to the parser status boundary.
 * @param status Result of @ref type_entity_resolve or @ref type_entity_depend.
 * @return PARSER_OK on success, PARSER_SEMANTIC_ERROR on a type conflict,
 * otherwise an invalid-argument or internal error.
 */
static ParserStatus parser_status_from_type(TypeResolveStatus status) {
  switch (status) {
  case TYPE_RESOLVE_OK:
    return PARSER_OK;
  case TYPE_RESOLVE_CONFLICT:
    return PARSER_SEMANTIC_ERROR;
  case TYPE_RESOLVE_INVALID_ARGUMENT:
    return PARSER_INVALID_ARGUMENT;
  default:
    return PARSER_INTERNAL_ERROR;
  }
}

/** Translates a precedence-stack helper result to the parser status boundary.
 */
static ParserStatus parser_status_from_psa(PsaStatus status) {
  switch (status) {
  case PSA_OK:
    return PARSER_OK;
  case PSA_INVALID_ARGUMENT:
    return PARSER_INVALID_ARGUMENT;
  case PSA_SYNTAX_ERROR:
    return PARSER_SYNTAX_ERROR;
  default:
    return PARSER_INTERNAL_ERROR;
  }
}

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

/**
 * @brief Replaces @c lexer->token with the next token.
 * @details End of input is not an error here: @c lexer->token becomes
 * @ref TOKEN_EOF and the production that does not expect it reports a syntax
 * error.
 * @param lexer Initialized lexer.
 * @return PARSER_OK on a new token or end of input, otherwise a translated
 * lexer failure.
 */
static ParserStatus parser_advance(Lexer *lexer) {
  LexerStatus status = lexer_pls(lexer);
  if (status == LEXER_EOF) {
    return PARSER_OK;
  }
  return parser_status_from_lexer(status);
}

/**
 * @brief Checks that the current token is @p expected and consumes it.
 * @param lexer Initialized lexer.
 * @param expected Token type that must be the current token.
 * @return PARSER_OK, PARSER_SYNTAX_ERROR for another token, or a translated
 * lexer failure.
 */
static ParserStatus parser_expect_and_consume(Lexer *lexer,
                                              TokenType expected) {
  if (lexer == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  ParserStatus status;

  if (lexer->token.type != expected) {
    return PARSER_SYNTAX_ERROR;
  }

  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  return PARSER_OK;
}

/**
 * @brief Consumes any number of newlines, optionally requiring at least one.
 * @details If @p required is true, the first token must be a newline or a
 * syntax error is reported. If @p consume_indent_dedent is true, every consumed
 * newline is followed by consuming all pending layout tokens (indent/dedent).
 * @param lexer Initialized lexer.
 * @param required If true, at least one newline must be present.
 * @param consume_indent_dedent If true, every consumed newline is followed by
 * consuming all pending layout tokens (indent/dedent).
 * @param indent_delta Optional indentation level updated while consuming
 * layout tokens. If non-NULL, the updated level must remain positive. If it
 * doesnt, a syntax error is reported.
 * @return PARSER_OK on success or PARSER_SYNTAX_ERROR when the indentation
 * level turns <= 0, otherwise a translated lexer failure.
 */
static ParserStatus parser_consume_newlines(Lexer *lexer, bool required,
                                            bool consume_indent_dedent,
                                            ptrdiff_t *indent_delta) {
  ParserStatus status = PARSER_OK;

  if (lexer == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  /// Consume newlines until the first non-newline token is reached.
  if (lexer->token.type == TOKEN_NEWLINE) {
    status = parser_advance(lexer);
    /// If the newline is required but not present, report a syntax error.
  } else if (required) {
    /// If the token is EOF, it counts as newline if it is required
    if (lexer->token.type != TOKEN_EOF) {
      status = PARSER_SYNTAX_ERROR;
    }
  }

  /// Consume indent/dedent tokens if requested
  if (consume_indent_dedent) {
    while (status == PARSER_OK && (lexer->token.type == TOKEN_INDENT ||
                                   lexer->token.type == TOKEN_DEDENT)) {
      if (indent_delta != NULL) {
        if (lexer->token.type == TOKEN_INDENT) {
          if (*indent_delta == PTRDIFF_MAX) {
            return PARSER_INTERNAL_ERROR;
          }
          (*indent_delta)++;
        } else {
          if (*indent_delta == PTRDIFF_MIN) {
            return PARSER_INTERNAL_ERROR;
          }
          (*indent_delta)--;
        }
        if (*indent_delta <= 0) {
          return PARSER_SYNTAX_ERROR;
        }
      }
      status = parser_advance(lexer);
    }
  }

  return status;
}

/*
 * Lookahead rule shared by every p_* production (see specifikace.txt):
 *   entry: lexer->token is the first token of the production,
 *   exit (PARSER_OK): lexer->token is the first token the production did not
 *   process. A production advances with parser_advance only after it has
 * processed the current token.
 *
 * First token consumption rule: The first token of each p_* function
 * is always cosumed by the function itself, not by the caller. The caller may
 * load the token and decide which p_function to call based on it, but has to
 * always pass the first token to the production, and the function consumes it
 * before processing the rest of the tokens.
 *
 * Scope passing rule: Every production that may define or resolve local
 * variables receives the lexical scope it operates in as @c scope. A function
 * body uses the function's local scope (which already holds its parameters);
 * a nested block (e.g. an if branch) receives a child scope created with
 * symbol_table_create_child, so its definitions shadow but do not leak out.
 *
 * Indentation in parenthesis: When parsing a parenthesized expression, the
 * indentation level is tracked separately from the current indentation level.
 * All physical lines inside parentheses must be indented more than the line
 * with the opening parenthesis.
 * This is tracked with the @c indentation_level pointer passed to
 * parser_consume_newlines.
 * When the closing parenthesis is reached, the indentation must be reset with
 * lexer_change_current_indent(lexer, (-1) * local_indent) to the level of the
 * opening parenthesis.
 * Possible problems:
  if ( (bool) &&
          ( bool
            ) // valid code, but the indent in nested parentheses must be
 tracked separately
    ) then statement;
 */

/**
 * @brief Parses an optional type annotation `: Type`.
 * @details Entry: @c lexer->token is the possible @c :. Without @c : nothing
 * is consumed. Exit: the token after the type identifier.
 * @param lexer Initialized lexer.
 * @param type_entity Type entity resolved to the annotated type.
 * @param required If true, a type annotation is required.
 * @param consume_newlines_indents If true, any number of newlines and layout
 * tokens is consumed.
 * @param indentation_level If @p consume_newlines_indents is true, the current
 * indentation level is updated. Pass NULL if @p consume_newlines_indents is
 * false.
 * @return PARSER_OK, PARSER_SYNTAX_ERROR, or a translated lexer failure.
 */
static ParserStatus p_type(Lexer *lexer, TypeEntity *type_entity, bool required,
                           bool consume_newlines_indents,
                           ptrdiff_t *indentation_level) {
  ParserStatus status;

  if (lexer == NULL || type_entity == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }
  if (consume_newlines_indents && indentation_level == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }
  if (!consume_newlines_indents && indentation_level != NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  /// Check for `:`
  if (lexer->token.type != TOKEN_COLON) {
    if (required) {
      return PARSER_SYNTAX_ERROR;
    }
    return PARSER_OK;
  }

  /// Consume the `:`
  status = parser_expect_and_consume(lexer, TOKEN_COLON);
  if (status != PARSER_OK) {
    return status;
  }

  if (consume_newlines_indents) {
    /// Consume any number of newlines before the type
    status = parser_consume_newlines(lexer, false, true, indentation_level);
    if (status != PARSER_OK) {
      return status;
    }
  }

  /// Expect a type-name keyword and resolve its type.
  DataType type = keyword_to_type(lexer->token.type);
  if (type == TYPE_UNKNOWN) {
    return PARSER_SYNTAX_ERROR;
  }
  status = parser_status_from_type(type_entity_resolve(type_entity, type));
  if (status != PARSER_OK) {
    return status;
  }

  return parser_advance(lexer);
}

static ParserStatus p_call_args(Lexer *lexer, Program *program,
                                FunctionSymbol *function, SymbolTable *scope,
                                ASTNode *call_node) {
  if (lexer == NULL || program == NULL || function == NULL || scope == NULL ||
      call_node == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  /// Consume the opening parenthesis
  ParserStatus status = parser_expect_and_consume(lexer, TOKEN_LEFT_PAREN);
  if (status != PARSER_OK) {
    return status;
  }

  ptrdiff_t local_indent = 0;

  /// Consume any newlines after the opening parenthesis
  status = parser_consume_newlines(lexer, false, true, &local_indent);

  if (status != PARSER_OK) {
    return status;
  }

  /// Parse the function call arguments
  if (lexer->token.type != TOKEN_RIGHT_PAREN) {
    while (true) {
      if (lexer->token.type == TOKEN_EOF) {
        return PARSER_SYNTAX_ERROR;
      }

      ASTNode *arg_node = NULL;
      status = p_expression(lexer, program, function, scope, &arg_node, true,
                            &local_indent);
      if (status != PARSER_OK) {
        return status;
      }

      /// Add the argument node to the call node's argument list
      if (ast_node_call_append_argument(call_node, arg_node) != AST_OK) {
        ast_node_destroy(arg_node);
        return PARSER_INTERNAL_ERROR;
      }

      /// Consume any number of newlines before the next comma or closing
      /// parenthesis
      status = parser_consume_newlines(lexer, false, true, &local_indent);
      if (status != PARSER_OK) {
        return status;
      }

      /// Expect a comma or closing parenthesis
      if (lexer->token.type == TOKEN_RIGHT_PAREN) {
        break;
      }
      status = parser_expect_and_consume(lexer, TOKEN_COMMA);
      if (status != PARSER_OK) {
        return status;
      }

      /// Consume any number of newlines before the next argument
      bool has_newline = lexer->token.type == TOKEN_NEWLINE;
      status = parser_consume_newlines(lexer, false, true, &local_indent);
      if (status != PARSER_OK) {
        return status;
      }

      /// A trailing comma is allowed only when followed by a newline and the
      /// closing parenthesis.
      if (has_newline && lexer->token.type == TOKEN_RIGHT_PAREN) {
        break;
      }
    }
  }

  /// Reset the indentation level to what it was before the opening parenthesis
  status = parser_status_from_lexer(
      lexer_change_current_indent(lexer, (-1) * local_indent));
  if (status != PARSER_OK) {
    return status;
  }

  return parser_expect_and_consume(lexer, TOKEN_RIGHT_PAREN);
}

static ParserStatus p_if(Lexer *lexer, Program *program,
                         FunctionSymbol *function, SymbolTable *scope,
                         ASTNode **return_node, bool consume_newlines_indents,
                         ptrdiff_t *local_indent) {
  if (lexer == NULL || program == NULL || scope == NULL ||
      return_node == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }
  if (consume_newlines_indents && local_indent == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  /// Remember the position of the 'if' keyword for the AST node
  size_t if_line = lexer->token.line;
  size_t if_column = lexer->token.column;

  /// Consume the 'if' keyword
  ParserStatus status = parser_expect_and_consume(lexer, TOKEN_KW_IF);
  if (status != PARSER_OK) {
    return status;
  }

  /// Inside parentheses: Consume any number of layout tokens after the 'if'
  /// keyword
  if (consume_newlines_indents) {
    status = parser_consume_newlines(lexer, false, true, local_indent);
    if (status != PARSER_OK) {
      return status;
    }
  }

  /// Parse the boolean expression
  ASTNode *condition_node = NULL;
  status = p_expression(lexer, program, function, scope, &condition_node,
                        consume_newlines_indents, local_indent);
  if (status != PARSER_OK) {
    ast_node_destroy(condition_node);
    return status;
  }

  /// Inside parentheses: Consume any number of layout tokens after the boolean
  /// expression
  if (consume_newlines_indents) {
    status = parser_consume_newlines(lexer, false, true, local_indent);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      return status;
    }
  }

  /// Parse the 'then' keyword
  status = parser_expect_and_consume(lexer, TOKEN_KW_THEN);
  if (status != PARSER_OK) {
    ast_node_destroy(condition_node);
    return status;
  }

  /// Inside parentheses: Consume any number of layout tokens after the 'then'
  /// keyword
  if (consume_newlines_indents) {
    status = parser_consume_newlines(lexer, false, true, local_indent);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      return status;
    }
  }

  ASTNode *then_branch_node = NULL;
  /// If there are no newlines after the 'then' keyword, parse the 'then' branch
  /// as a single statement
  if (lexer->token.type != TOKEN_NEWLINE) {

    /// Create a new child scope for the 'then' block
    SymbolTable *then_scope = symbol_table_create_child(scope);
    if (then_scope == NULL) {
      ast_node_destroy(condition_node);
      return PARSER_INTERNAL_ERROR;
    }

    status =
        p_statement(lexer, program, function, then_scope, &then_branch_node);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      return status;
    }

    /// Consume any number of newlines after the 'then' statement
    /// also consume any indents/dedents if inside parentheses
    status = parser_consume_newlines(lexer, false, consume_newlines_indents,
                                     local_indent);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      return status;
    }

  } else {
    /// If there are newlines after the 'then' keyword, parse the 'then' branch
    /// as a code block

    /// Consume any number of newlines
    status = parser_consume_newlines(lexer, false, false, NULL);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      return status;
    }

    /// Count indentation level for the 'then' branch
    size_t indent_delta = 0;
    status = parser_consume_indent_delta(lexer, &indent_delta);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      return status;
    }

    /// create a new child scope for the 'then' block
    SymbolTable *then_scope = symbol_table_create_child(scope);
    if (then_scope == NULL) {
      ast_node_destroy(condition_node);
      return PARSER_INTERNAL_ERROR;
    }

    /// Parse the 'then' branch
    ASTNode *then_branch_node = NULL;
    status =
        p_code_block(lexer, program, function, then_scope, &then_branch_node);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      return status;
    }

    /// Consume any number of newlines after the 'then' branch
    status = parser_consume_newlines(lexer, false, false, NULL);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      return status;
    }

    /// Consume dedents that match the indentation of the 'then' branch
    status = parser_consume_dedents(lexer, indent_delta);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      return status;
    }
  }

  /// If there is no 'else' branch, create the if node and return
  if (lexer->token.type != TOKEN_KW_ELSE) {
    ASTNode *if_node = ast_node_create_if(condition_node, then_branch_node,
                                          NULL, if_line, if_column);
    if (if_node == NULL) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      return PARSER_INTERNAL_ERROR;
    }
    *return_node = if_node;
    return PARSER_OK;
  }

  /// Consume the 'else' keyword
  status = parser_expect_and_consume(lexer, TOKEN_KW_ELSE);
  if (status != PARSER_OK) {
    ast_node_destroy(condition_node);
    ast_node_destroy(then_branch_node);
    return status;
  }

  /// Inside parentheses: Consume any number of layout tokens after the 'then'
  /// keyword
  if (consume_newlines_indents) {
    status = parser_consume_newlines(lexer, false, true, local_indent);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      return status;
    }
  }

  ASTNode *else_branch_node = NULL;
  /// If there are no newlines after the 'else' keyword, parse the 'else' branch
  /// as a single statement
  if (lexer->token.type != TOKEN_NEWLINE) {

    /// Create a new child scope for the 'else' block
    SymbolTable *else_scope = symbol_table_create_child(scope);
    if (else_scope == NULL) {
      ast_node_destroy(condition_node);
      return PARSER_INTERNAL_ERROR;
    }

    status =
        p_statement(lexer, program, function, else_scope, &else_branch_node);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      ast_node_destroy(else_branch_node);
      return status;
    }

    /// Consume any number of newlines after the 'else' statement
    /// also consume any indents/dedents if inside parentheses
    status = parser_consume_newlines(lexer, false, consume_newlines_indents,
                                     local_indent);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      ast_node_destroy(else_branch_node);
      return status;
    }

  } else {
    /// If there are newlines after the 'else' keyword, parse the 'else' branch
    /// as a code block

    /// Consume any number of newlines
    status = parser_consume_newlines(lexer, false, false, NULL);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      return status;
    }

    /// Count indentation level for the 'else' branch
    size_t indent_delta = 0;
    status = parser_consume_indent_delta(lexer, &indent_delta);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      return status;
    }

    /// Create a new child scope for the 'else' block
    SymbolTable *else_scope = symbol_table_create_child(scope);
    if (else_scope == NULL) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      return PARSER_INTERNAL_ERROR;
    }

    /// Parse the 'else' branch
    ASTNode *else_branch_node = NULL;
    status =
        p_code_block(lexer, program, function, else_scope, &else_branch_node);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      ast_node_destroy(else_branch_node);
      return status;
    }

    /// Consume any number of newlines after the 'else' branch
    status = parser_consume_newlines(lexer, false, false, NULL);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      ast_node_destroy(else_branch_node);
      return status;
    }

    /// Consume dedents that match the indentation of the 'else' branch
    status = parser_consume_dedents(lexer, indent_delta);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      ast_node_destroy(else_branch_node);
      return status;
    }
  }

  ASTNode *if_node = ast_node_create_if(condition_node, then_branch_node,
                                        else_branch_node, if_line, if_column);
  if (if_node == NULL) {
    ast_node_destroy(condition_node);
    ast_node_destroy(then_branch_node);
    ast_node_destroy(else_branch_node);
    return PARSER_INTERNAL_ERROR;
  }

  *return_node = if_node;

  return PARSER_OK;
}

/**
@note This function was altered by an LLM agent to handle nested parentheses
and unit parsing in accordance with psa_spec.md. Model used: Kimi-k3.1-pro.
*/
static ParserStatus p_operand(Lexer *lexer, Program *program,
                              FunctionSymbol *function, SymbolTable *scope,
                              ASTNode **return_node,
                              bool consume_newlines_indents,
                              ptrdiff_t *local_indent) {
  if (lexer == NULL || program == NULL || scope == NULL ||
      return_node == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  ParserStatus status;
  switch (lexer->token.type) {
  case TOKEN_INT:
    *return_node = ast_node_create_int(lexer->token.value.int_value,
                                       lexer->token.line, lexer->token.column);
    if (*return_node == NULL) {
      return PARSER_INTERNAL_ERROR;
    }
    return parser_advance(lexer);

  case TOKEN_DOUBLE:
    *return_node = ast_node_create_real(lexer->token.value.double_value,
                                        lexer->token.line, lexer->token.column);
    if (*return_node == NULL) {
      return PARSER_INTERNAL_ERROR;
    }
    return parser_advance(lexer);

  case TOKEN_STRING:
    *return_node =
        ast_node_create_string(lexer->token.value.string_value,
                               lexer->token.line, lexer->token.column);
    if (*return_node == NULL) {
      return PARSER_INTERNAL_ERROR;
    }
    return parser_advance(lexer);

  case TOKEN_KW_TRUE:
    *return_node =
        ast_node_create_bool(true, lexer->token.line, lexer->token.column);
    if (*return_node == NULL) {
      return PARSER_INTERNAL_ERROR;
    }
    return parser_advance(lexer);

  case TOKEN_KW_FALSE:
    *return_node =
        ast_node_create_bool(false, lexer->token.line, lexer->token.column);
    if (*return_node == NULL) {
      return PARSER_INTERNAL_ERROR;
    }
    return parser_advance(lexer);

  case TOKEN_LEFT_PAREN: {
    size_t paren_line = lexer->token.line;
    size_t paren_column = lexer->token.column;

    status = parser_advance(lexer);
    if (status != PARSER_OK) {
      return status;
    }

    if (lexer->token.type == TOKEN_RIGHT_PAREN) {
      *return_node = ast_node_create_unit(paren_line, paren_column);
      if (*return_node == NULL) {
        return PARSER_INTERNAL_ERROR;
      }
      return parser_advance(lexer);
    }

    ptrdiff_t nested_indent = 0;
    status = parser_consume_newlines(lexer, false, true, &nested_indent);
    if (status != PARSER_OK) {
      return status;
    }

    status = p_expression(lexer, program, function, scope, return_node, true,
                          &nested_indent);
    if (status != PARSER_OK) {
      return status;
    }
    if (*return_node == NULL) {
      return PARSER_SYNTAX_ERROR;
    }

    status = parser_consume_newlines(lexer, false, true, &nested_indent);
    if (status != PARSER_OK) {
      ast_node_destroy(*return_node);
      *return_node = NULL;
      return status;
    }

    status = parser_status_from_lexer(
        lexer_change_current_indent(lexer, (-1) * nested_indent));
    if (status != PARSER_OK) {
      ast_node_destroy(*return_node);
      *return_node = NULL;
      return status;
    }

    status = parser_expect_and_consume(lexer, TOKEN_RIGHT_PAREN);
    if (status != PARSER_OK) {
      ast_node_destroy(*return_node);
      *return_node = NULL;
      return status;
    }

    return PARSER_OK;
  }

  case TOKEN_IDENTIFIER:
    const char *token_name = lexer->token.value.string_value;
    size_t line = lexer->token.line;
    size_t column = lexer->token.column;
    size_t name_length = strlen(token_name) + 1U;
    char *name = malloc(name_length);
    if (name == NULL) {
      return PARSER_INTERNAL_ERROR;
    }
    memcpy(name, token_name, name_length);

    status = parser_advance(lexer);
    if (status != PARSER_OK) {
      free(name);
      return status;
    }

    if (lexer->token.type == TOKEN_LEFT_PAREN) {
      /// Function call

      FunctionSymbol *function = NULL;
      if (function_table_get_or_create(&program->functions, name, &function) !=
          FUNCTION_TABLE_OK) {
        free(name);
        return PARSER_INTERNAL_ERROR;
      }
      free(name);

      ASTNode *call_node =
          ast_node_create_function_call(function, line, column);
      if (call_node == NULL) {
        return PARSER_INTERNAL_ERROR;
      }

      /// Parse the function call arguments
      status = p_call_args(lexer, program, function, scope, call_node);
      if (status != PARSER_OK) {
        ast_node_destroy(call_node);
        return status;
      }
      status = parser_status_from_type(type_entity_depend(
          &function->return_type, &call_node->inferred_type));
      if (status != PARSER_OK) {
        ast_node_destroy(call_node);
        return status;
      }
      *return_node = call_node;
      return PARSER_OK;
    } else {
      /// Variable reference
      Symbol *symbol = symbol_table_resolve(scope, name);
      free(name);
      if (symbol == NULL) {
        return PARSER_SEMANTIC_ERROR;
      }
      ASTNode *variable_node =
          ast_node_create_variable_ref(symbol, line, column);
      if (variable_node == NULL) {
        return PARSER_INTERNAL_ERROR;
      }
      status = parser_status_from_type(
          type_entity_depend(&symbol->type, &variable_node->inferred_type));
      if (status != PARSER_OK) {
        ast_node_destroy(variable_node);
        return status;
      }

      *return_node = variable_node;
      return PARSER_OK;
    }
    break;

  case TOKEN_KW_IF:
    return p_if(lexer, program, function, scope, return_node,
                consume_newlines_indents, local_indent);

  default:
    return PARSER_SYNTAX_ERROR;
  }
}

// Parses an implicit return expression on the function header line.
// Entry: the first token of the expression. Exit: the token after the
// expression.

/**
 * @brief Parses one expression using operator-precedence analysis.
 * @details Entry: @c lexer->token is the first token of the expression. On
 * success, the expression's owned AST is returned and @c lexer->token is the
 * first unconsumed delimiter. On failure, the caller retains no AST
 * ownership.
 * @param lexer Initialized lexer with its current lookahead token loaded.
 * @param program Program owning functions referenced by the expression.
 * @param scope Lexical scope used to resolve variable references.
 * @param return_node Output receiving the owned expression AST on success.
 * @param consume_newlines_indents Whether multiline expression layout may be
 * consumed.
 * @param local_indent Nested indentation depth; required when
 * @p consume_newlines_indents is true.
 * @return PARSER_OK or the first syntax, semantic, lexer, argument, internal,
 * or precedence-stack error.
 * @note This function was generated by an LLM agent based on psa_spec.md
 * Model used: Kimi-k3.1-pro.
 */
static ParserStatus p_expression(Lexer *lexer, Program *program,
                                 FunctionSymbol *function, SymbolTable *scope,
                                 ASTNode **return_node,
                                 bool consume_newlines_indents,
                                 ptrdiff_t *local_indent) {
  if (lexer == NULL || program == NULL || scope == NULL ||
      return_node == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }
  if (consume_newlines_indents && local_indent == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }
  /// Do not transfer a partial AST to the caller if parsing fails.
  *return_node = NULL;

  /// Stack initialization consumes no input. PSA helpers report PsaStatus, so
  /// translate their results at the parser boundary.
  TerminalStack stack;
  ParserStatus status = parser_status_from_psa(terminal_stack_init(&stack));
  if (status != PARSER_OK) {
    return status;
  }

  /// Push the stack-bottom dollar sentinel. It is not a real token and leaves
  /// the first token of the expression untouched.
  Terminal bottom = {
      .type = TERMINAL_DOLLAR, .token = {.type = TOKEN_INVALID}, .node = NULL};
  status = parser_status_from_psa(terminal_stack_push(&stack, bottom));
  if (status != PARSER_OK) {
    terminal_stack_cleanup_nodes(&stack);
    return status;
  }

  /// Start by expecting an operand. Alternating this state distinguishes
  /// unary operators from binary operators and keeps operands out of the
  /// table lookup.
  bool expecting_operand = true;

  while (true) {
    if (expecting_operand) {
      /// Before an operand, consume newlines and layout only in constructs
      /// that permit multiline expressions, such as parentheses and call
      /// arguments.
      if (consume_newlines_indents) {
        status = parser_consume_newlines(lexer, false, true, local_indent);
        if (status != PARSER_OK) {
          terminal_stack_cleanup_nodes(&stack);
          return status;
        }
      }

      TokenType unary_op = TOKEN_INVALID;
      size_t unary_line = 0;
      size_t unary_column = 0;

      /// `+`, `-`, and `!` have their prefix meaning only while an operand is
      /// expected. Save the operator and its source position before consuming
      /// it.
      if (lexer->token.type == TOKEN_MINUS || lexer->token.type == TOKEN_PLUS ||
          lexer->token.type == TOKEN_NOT) {
        unary_op = lexer->token.type;
        unary_line = lexer->token.line;
        unary_column = lexer->token.column;

        /// Consume the unary operator now; its operand must be available
        /// before the corresponding AST wrapper can be created.
        status = parser_advance(lexer);
        if (status != PARSER_OK) {
          terminal_stack_cleanup_nodes(&stack);
          return status;
        }

        /// Layout between a unary operator and its operand is governed by the
        /// same multiline-expression rule as layout before the operator.
        if (consume_newlines_indents) {
          status = parser_consume_newlines(lexer, false, true, local_indent);
          if (status != PARSER_OK) {
            terminal_stack_cleanup_nodes(&stack);
            return status;
          }
        }
      }

      /// Delegate one primary operand to p_operand. It consumes that operand
      /// and any nested syntax it owns, leaving the next operator or
      /// delimiter as the current lookahead token.
      ASTNode *operand_node = NULL;
      status = p_operand(lexer, program, function, scope, &operand_node,
                         consume_newlines_indents, local_indent);
      if (status != PARSER_OK) {
        ast_node_destroy(operand_node);
        terminal_stack_cleanup_nodes(&stack);
        return status;
      }
      if (operand_node == NULL) {
        terminal_stack_cleanup_nodes(&stack);
        return PARSER_SYNTAX_ERROR;
      }

      /// Wrap the now-complete operand in the saved unary operation. Unary
      /// `+` needs no wrapper because it does not change the operand's value.
      if (unary_op == TOKEN_NOT) {
        ASTNode *not_node =
            ast_node_create_not(operand_node, unary_line, unary_column);
        if (not_node == NULL) {
          ast_node_destroy(operand_node);
          terminal_stack_cleanup_nodes(&stack);
          return PARSER_INTERNAL_ERROR;
        }
        operand_node = not_node;
      } else if (unary_op == TOKEN_MINUS) {
        /// The AST has no unary-minus kind, so represent negation as `0 - x`.
        ASTNode *zero_node = ast_node_create_int(0, unary_line, unary_column);
        if (zero_node == NULL) {
          ast_node_destroy(operand_node);
          terminal_stack_cleanup_nodes(&stack);
          return PARSER_INTERNAL_ERROR;
        }
        ASTNode *neg_node = ast_node_create_binary_op(
            AST_SUBTRACT, zero_node, operand_node, unary_line, unary_column);
        if (neg_node == NULL) {
          ast_node_destroy(zero_node);
          ast_node_destroy(operand_node);
          terminal_stack_cleanup_nodes(&stack);
          return PARSER_INTERNAL_ERROR;
        }
        operand_node = neg_node;
      }

      /// Store the completed operand as a nonterminal stack item. A
      /// successful push transfers AST ownership to the stack; no input is
      /// consumed here.
      Terminal operand_terminal = {.type = TERMINAL_OPERAND,
                                   .token = {.type = TOKEN_INVALID},
                                   .node = operand_node};
      status =
          parser_status_from_psa(terminal_stack_push(&stack, operand_terminal));
      if (status != PARSER_OK) {
        ast_node_destroy(operand_node);
        terminal_stack_cleanup_nodes(&stack);
        return status;
      }

      /// A complete operand is on the stack, so the next token must be either
      /// a binary operator or a delimiter that ends the expression.
      expecting_operand = false;
    } else {
      /// After an operand, permitted layout may precede a binary operator or
      /// the expression's final delimiter.
      if (consume_newlines_indents) {
        status = parser_consume_newlines(lexer, false, true, local_indent);
        if (status != PARSER_OK) {
          terminal_stack_cleanup_nodes(&stack);
          return status;
        }
      }

      /// Find the nearest terminal below any completed operands and handles;
      /// this only inspects the stack and consumes no input.
      Terminal top_terminal;
      status =
          parser_status_from_psa(stack_get_top_terminal(&stack, &top_terminal));
      if (status != PARSER_OK) {
        terminal_stack_cleanup_nodes(&stack);
        return status;
      }

      /// Classify the lookahead without consuming it. Operators retain their
      /// terminal kinds, while tokens that can end an expression map to `$`.
      TerminalType input_terminal = token_to_terminal(lexer->token.type);

      /// Two dollar symbols mean that no more operators can be taken. The
      /// current delimiter deliberately remains unconsumed for the caller.
      if (top_terminal.type == TERMINAL_DOLLAR &&
          input_terminal == TERMINAL_DOLLAR) {
        /// Success requires exactly the bottom sentinel and one complete AST;
        /// any operators or extra operands would make the expression
        /// incomplete.
        if (stack.size == 2U && stack.data[1].type == TERMINAL_OPERAND &&
            stack.data[1].node != NULL) {
          *return_node = stack.data[1].node;
          /// Transfer ownership of the result before destroying all ASTs
          /// still owned by the stack.
          stack.data[1].node = NULL;
          terminal_stack_cleanup_nodes(&stack);
          return PARSER_OK;
        }
        terminal_stack_cleanup_nodes(&stack);
        return PARSER_SYNTAX_ERROR;
      }

      /// Reject tokens that are neither binary operators nor valid expression
      /// delimiters, and stack-only symbols that cannot index the table. The
      /// offending lookahead is left for the caller's diagnostic context.
      if (input_terminal == TERMINAL_INVALID ||
          top_terminal.type >= TERMINAL_COUNT) {
        terminal_stack_cleanup_nodes(&stack);
        return PARSER_SYNTAX_ERROR;
      }

      /// The precedence relation, rather than the token alone, decides
      /// whether to take the operator or reduce the expression already on the
      /// stack.
      PRECEDENCE precedence = get_precedence(top_terminal.type, input_terminal);
      switch (precedence) {
      case PREC_TAKE: {
        /// TAKE opens a new reduction handle and delays combining the left
        /// operand until the incoming operator has its right operand.
        status = parser_status_from_psa(
            stack_insert_handle_after_top_terminal(&stack));
        if (status != PARSER_OK) {
          terminal_stack_cleanup_nodes(&stack);
          return status;
        }

        /// Copy the operator and its source position onto the stack before
        /// advancing, because advancing replaces the lexer's lookahead token.
        Terminal operator_terminal = {
            .type = input_terminal, .token = lexer->token, .node = NULL};
        status = parser_status_from_psa(
            terminal_stack_push(&stack, operator_terminal));
        if (status != PARSER_OK) {
          terminal_stack_cleanup_nodes(&stack);
          return status;
        }

        /// Consume only the binary operator. The following token is retained
        /// as lookahead and must produce its required right operand next.
        status = parser_advance(lexer);
        if (status != PARSER_OK) {
          terminal_stack_cleanup_nodes(&stack);
          return status;
        }

        expecting_operand = true;
        break;
      }

      case PREC_REDUCE:
        /// REDUCE combines the completed handle on the stack but deliberately
        /// does not advance: the same lookahead is compared with the new top
        /// terminal until it can be taken or can finish the expression.
        status = parser_status_from_psa(psa_reduce(&stack));
        if (status != PARSER_OK) {
          terminal_stack_cleanup_nodes(&stack);
          return status;
        }
        break;

      case PREC_INVALID:
      default:
        /// No valid precedence relation exists, so consuming the lookahead
        /// would only hide the syntax error. Discard owned ASTs and report
        /// it.
        terminal_stack_cleanup_nodes(&stack);
        return PARSER_SYNTAX_ERROR;
      }
    }
  }
}
static ParserStatus p_variable_init_statement(Lexer *lexer, Program *program,
                                              bool constant,
                                              FunctionSymbol *function,
                                              SymbolTable *scope,
                                              ASTNode **return_node) {
  ParserStatus status;

  if (lexer == NULL || program == NULL || function == NULL || scope == NULL ||
      return_node == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  /// Consume the 'val' or 'var' keyword
  status =
      parser_expect_and_consume(lexer, constant ? TOKEN_KW_VAL : TOKEN_KW_VAR);
  if (status != PARSER_OK) {
    return status;
  }

  /// Expect an identifier for the variable name
  if (lexer->token.type != TOKEN_IDENTIFIER) {
    return PARSER_SYNTAX_ERROR;
  }

  /// Add the variable to the current lexical scope
  Symbol *var_symbol;
  switch (symbol_table_define(scope, lexer->token.value.string_value,
                              TYPE_UNKNOWN, true, constant, &var_symbol)) {
  case SYMTABLE_OK:
    break;
  case SYMTABLE_DUPLICATE:
    return PARSER_SEMANTIC_ERROR;
  default:
    return PARSER_INTERNAL_ERROR;
  }

  /// Consume the variable name
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  /// Parse the optional type annotation for the variable
  status = p_type(lexer, &var_symbol->type, false, false, NULL);
  if (status != PARSER_OK) {
    return status;
  }

  /// Consume the assignment operator
  status = parser_expect_and_consume(lexer, TOKEN_ASSIGN);
  if (status != PARSER_OK) {
    return status;
  }

  /// Parse the expression for the variable's initial value
  ASTNode *expression_node = NULL;
  status = p_expression(lexer, program, function, scope, &expression_node,
                        false, NULL);
  if (status != PARSER_OK) {
    return status;
  }

  /// Resolve the variable's type based on the expression's inferred type or
  /// create a dependency
  if (expression_node->inferred_type.type != TYPE_UNKNOWN) {
    status = parser_status_from_type(type_entity_resolve(
        &var_symbol->type, expression_node->inferred_type.type));
  } else {
    status = parser_status_from_type(
        type_entity_depend(&expression_node->inferred_type, &var_symbol->type));
  }
  if (status != PARSER_OK) {
    ast_node_destroy(expression_node);
    return status;
  }

  /// Create an assignment node for the variable initialization
  ASTNode *assignment_node = ast_node_create_assign(
      var_symbol, expression_node, lexer->token.line, lexer->token.column);
  if (assignment_node == NULL) {
    return PARSER_INTERNAL_ERROR;
  }

  /// Return the assignment node to the caller
  *return_node = assignment_node;

  return PARSER_OK;
}

static ParserStatus p_return_statement(Lexer *lexer, Program *program,
                                       FunctionSymbol *function,
                                       SymbolTable *scope,
                                       ASTNode **return_node) {
  ParserStatus status;

  if (lexer == NULL || program == NULL || function == NULL || scope == NULL ||
      return_node == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  /// Consume the 'return' keyword
  status = parser_expect_and_consume(lexer, TOKEN_KW_RETURN);
  if (status != PARSER_OK) {
    return status;
  }

  /// Parse the optional return expression
  ASTNode *expression_node = NULL;
  if (lexer->token.type != TOKEN_NEWLINE && lexer->token.type != TOKEN_DEDENT &&
      lexer->token.type != TOKEN_EOF) {
    status = p_expression(lexer, program, function, scope, &expression_node,
                          false, NULL);
    if (status != PARSER_OK) {
      ast_node_destroy(expression_node);
      return status;
    }

    /// If the function's return type is unit, discard the expression and set
    /// it to null
    if (function->return_type.type == TYPE_UNIT) {
      ast_node_destroy(expression_node);
      expression_node = NULL;
    }
  }

  /// Create a return statement node
  ASTNode *return_node_local = ast_node_create_return(
      expression_node, lexer->token.line, lexer->token.column);
  if (return_node_local == NULL) {
    ast_node_destroy(expression_node);
    return PARSER_INTERNAL_ERROR;
  }

  /// The function's return type is dependent on the return node's inferred
  /// type
  type_entity_depend(&return_node_local->inferred_type, &function->return_type);

  /// Resolve the return statement's inferred type based on the expression's
  /// inferred type or create a dependency
  if (expression_node != NULL) {
    if (expression_node->inferred_type.type != TYPE_UNKNOWN) {
      status = parser_status_from_type(
          type_entity_resolve(&return_node_local->inferred_type,
                              expression_node->inferred_type.type));
    } else {
      status = parser_status_from_type(type_entity_depend(
          &expression_node->inferred_type, &return_node_local->inferred_type));
    }
    /// If there is no expression, resolve the return statement's inferred
    /// type to unit
  } else {
    status = parser_status_from_type(
        type_entity_resolve(&return_node_local->inferred_type, TYPE_UNIT));
  }
  if (status != PARSER_OK) {
    ast_node_destroy(return_node_local);
    return status;
  }

  /// Return the return statement node to the caller
  *return_node = return_node_local;

  return PARSER_OK;
}

static ParserStatus p_statement(Lexer *lexer, Program *program,
                                FunctionSymbol *function, SymbolTable *scope,
                                ASTNode **return_node) {
  if (lexer == NULL || program == NULL || scope == NULL ||
      return_node == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  switch (lexer->token.type) {
  case TOKEN_KW_IF:
    return p_if(lexer, program, function, scope, return_node, false, NULL);
    //  TODO: Implement these statement productions, then uncomment their
    //  cases here. case TOKEN_KW_WHILE:
    //    return p_while_statement(lexer, program, function, scope,
    //    return_node); break;
    //  case TOKEN_KW_FOR:
    //    return p_for_statement(lexer, program, function, scope,
    //    return_node); break;
    //  case TOKEN_KW_LIST:
    //    return p_list_statement(lexer, program, function, scope,
    //    return_node); break;
  case TOKEN_KW_VAL:
    return p_variable_init_statement(lexer, program, true, function, scope,
                                     return_node);
  case TOKEN_KW_VAR:
    return p_variable_init_statement(lexer, program, false, function, scope,
                                     return_node);
  case TOKEN_KW_RETURN:
    return p_return_statement(lexer, program, function, scope, return_node);

  /// Expression
  case TOKEN_IDENTIFIER:
  case TOKEN_LEFT_PAREN:
  case TOKEN_INT:
  case TOKEN_DOUBLE:
  case TOKEN_STRING:
  case TOKEN_NOT:
  case TOKEN_KW_TRUE:
  case TOKEN_KW_FALSE:
  case TOKEN_MINUS:
    return p_expression(lexer, program, function, scope, return_node, false,
                        NULL);
    break;
  default:
    return PARSER_SYNTAX_ERROR;
  }
}

// TODO: Implement p_code_block and p_expression, then uncomment these
// declarations and their calls in p_function.
//
// Parses an indented code block.
// Entry: the first token of the block (after INDENT). Exit: the DEDENT
// closing the block.
static ParserStatus p_code_block(Lexer *lexer, Program *program,
                                 FunctionSymbol *function, SymbolTable *scope,
                                 ASTNode **return_node) {
  ParserStatus status;

  if (lexer == NULL || program == NULL || function == NULL || scope == NULL ||
      return_node == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  /// Expect an INDENT token to start the code block
  if (lexer->token.type != TOKEN_INDENT) {
    return PARSER_SYNTAX_ERROR;
  }

  size_t indent_delta;

  status = parser_consume_indent_delta(lexer, &indent_delta);
  if (status != PARSER_OK) {
    return status;
  }
  if (indent_delta == 0U) {
    return PARSER_SYNTAX_ERROR;
  }

  /// Create a new block node to hold the statements in the code block
  ASTNode *block_node =
      ast_node_create_block(lexer->token.line, lexer->token.column);
  if (block_node == NULL) {
    return PARSER_INTERNAL_ERROR;
  }

  /// Loop over statements until a DEDENT or EOF is reached.
  while (lexer->token.type != TOKEN_DEDENT && lexer->token.type != TOKEN_EOF) {

    /// Skip any semicolons or newlines before the next statement.
    while (lexer->token.type == TOKEN_SEMICOLON) {
      status = parser_expect_and_consume(lexer, TOKEN_SEMICOLON);
      if (status != PARSER_OK) {
        ast_node_destroy(block_node);
        return status;
      }

      status = parser_consume_newlines(lexer, false, false, NULL);
      if (status != PARSER_OK) {
        ast_node_destroy(block_node);
        return status;
      }
    }

    if (lexer->token.type == TOKEN_INVALID ||
        lexer->token.type == TOKEN_KW_DEF ||
        lexer->token.type == TOKEN_INDENT) {
      ast_node_destroy(block_node);
      return PARSER_SYNTAX_ERROR;
    }

    /// The separators may have been the last tokens of the block.
    if (lexer->token.type == TOKEN_DEDENT || lexer->token.type == TOKEN_EOF) {
      break;
    }

    /// Parse the next statement and append it to the block.
    ASTNode *statement = NULL;
    status = p_statement(lexer, program, function, scope, &statement);
    if (status != PARSER_OK) {
      ast_node_destroy(statement);
      ast_node_destroy(block_node);
      return status;
    }

    /// A statement ending with its own nested block (e.g. if) has already
    /// consumed the NEWLINE preceding the block's DEDENT, so no further
    /// newline follows it.
    bool ends_with_block =
        statement->kind == AST_IF; // FIXME: add other statement kinds that
                                   // end with a block (while, for, etc?)

    /// Update the block's inferred type based on the return inferred type
    if (statement->kind == AST_RETURN) {
      if (statement->inferred_type.type != TYPE_UNKNOWN) {
        status = parser_status_from_type(type_entity_resolve(
            &block_node->inferred_type, statement->inferred_type.type));
      } else {
        status = parser_status_from_type(type_entity_depend(
            &statement->inferred_type, &block_node->inferred_type));
      }
      if (status != PARSER_OK) {
        ast_node_destroy(statement);
        ast_node_destroy(block_node);
        return status;
      }
    }

    ASTStatus ast_status;
    ast_status = ast_node_block_append_statement(block_node, statement);
    if (ast_status != AST_OK) {
      ast_node_destroy(statement);
      ast_node_destroy(block_node);
      return PARSER_INTERNAL_ERROR;
    }

    /// Consume at least one newline after the statement, or report a syntax
    /// error. Statements ending with a nested block only allow extra
    /// newlines.
    status = parser_consume_newlines(lexer, !ends_with_block, false, NULL);
    if (status != PARSER_OK) {
      ast_node_destroy(block_node);
      return status;
    }
  }

  /// Consume same number of DEDENT tokens as the INDENT tokens that started
  /// the block.
  status = parser_consume_dedents(lexer, indent_delta);
  if (status != PARSER_OK) {
    ast_node_destroy(block_node);
    return status;
  }

  /// Hand the block over to the caller only on success, so the caller never
  /// receives an already destroyed node.
  *return_node = block_node;

  return PARSER_OK;
}

/**
 * @brief Parses a parameter list `( [id {, id}] )`.
 * @details Entry: @c lexer->token is @c (. Exit: the token after @c ).
 * @param lexer Initialized lexer.
 * @param program Program owning the function.
 * @param function Function whose local scope receives the parameters.
 * @return PARSER_OK, a syntax/internal error, or a translated lexer failure.
 */
static ParserStatus p_def_args(Lexer *lexer, Program *program,
                               FunctionSymbol *function) {
  ParserStatus status;

  if (lexer == NULL || program == NULL || function == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  /// Consume the opening parenthesis
  status = parser_expect_and_consume(lexer, TOKEN_LEFT_PAREN);
  if (status != PARSER_OK) {
    return status;
  }

  /// Create the function's local scope for its parameters
  SymbolTable *local_scope = function_symbol_create_local_scope(function);
  if (local_scope == NULL) {
    return PARSER_INTERNAL_ERROR;
  }

  /// Start the indentation level at 0 for the line where the opening
  /// parenthesis was found. The indentation level is increased by 1 for each
  /// INDENT token and decreased by 1 for each DEDENT token. The function
  /// fails if the indentation level goes negative (syntax error).
  ptrdiff_t local_indent = 0U;

  /// Consume any number of newlines before the first parameter
  status = parser_consume_newlines(lexer, false, true, &local_indent);
  if (status != PARSER_OK) {
    return status;
  }

  size_t arg_count = 0U;

  /// If not an empty parameter list
  if (lexer->token.type != TOKEN_RIGHT_PAREN) {

    while (true) {
      /// Argument name must be a valid identifier
      if (lexer->token.type != TOKEN_IDENTIFIER) {
        return PARSER_SYNTAX_ERROR;
      }

      /// Add the argument to the function's local scope
      Symbol *arg_symbol;
      switch (symbol_table_define(local_scope, lexer->token.value.string_value,
                                  TYPE_UNKNOWN, false, false, &arg_symbol)) {
      case SYMTABLE_OK:
        break;
      case SYMTABLE_DUPLICATE:
        return PARSER_SEMANTIC_ERROR;
      default:
        return PARSER_INTERNAL_ERROR;
      }

      /// Consume the argument name
      status = parser_advance(lexer);
      if (status != PARSER_OK) {
        return status;
      }

      /// Consume any number of newlines before the type annotation
      status = parser_consume_newlines(lexer, false, true, &local_indent);
      if (status != PARSER_OK) {
        return status;
      }

      /// Parse the mandatory type annotation for the argument
      status = p_type(lexer, &arg_symbol->type, true, true, &local_indent);
      if (status != PARSER_OK) {
        return status;
      }

      arg_count += 1U;

      /// Consume any number of newlines before the next comma or closing
      /// parenthesis
      status = parser_consume_newlines(lexer, false, true, &local_indent);
      if (status != PARSER_OK) {
        return status;
      }

      /// Expect a comma or closing parenthesis
      if (lexer->token.type == TOKEN_RIGHT_PAREN) {
        break;
      }
      status = parser_expect_and_consume(lexer, TOKEN_COMMA);
      if (status != PARSER_OK) {
        return status;
      }

      /// Consume any number of newlines before the next argument
      bool has_newline = lexer->token.type == TOKEN_NEWLINE;
      status = parser_consume_newlines(lexer, false, true, &local_indent);
      if (status != PARSER_OK) {
        return status;
      }

      /// A trailing comma is allowed only when followed by a newline and the
      /// closing parenthesis.
      if (has_newline && lexer->token.type == TOKEN_RIGHT_PAREN) {
        break;
      }
    }
  }

  /// Reset the indentation level to what it was before the opening
  /// parenthesis
  status = parser_status_from_lexer(
      lexer_change_current_indent(lexer, (-1) * local_indent));
  if (status != PARSER_OK) {
    return status;
  }
  function->argument_count = arg_count;

  /// Consume the closing parenthesis (re-enables indentation)
  return parser_expect_and_consume(lexer, TOKEN_RIGHT_PAREN);
}

/**
 * @brief Consumes one contiguous lexer-emitted INDENT delta.
 * @details Entry: the first possible INDENT. Exit: the first non-INDENT
 * token.
 * @param lexer Initialized lexer producing layout tokens.
 * @param indent_delta Output number of consumed TOKEN_INDENT tokens.
 * @return PARSER_OK, PARSER_INVALID_ARGUMENT, PARSER_INTERNAL_ERROR, or a
 * translated lexer failure.
 */
static ParserStatus parser_consume_indent_delta(Lexer *lexer,
                                                size_t *indent_delta) {
  ParserStatus status;

  if (lexer == NULL || indent_delta == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }
  *indent_delta = 0U;
  while (lexer->token.type == TOKEN_INDENT) {
    if (*indent_delta == SIZE_MAX) {
      return PARSER_INTERNAL_ERROR;
    }
    *indent_delta += 1U;
    status = parser_expect_and_consume(lexer, TOKEN_INDENT);
    if (status != PARSER_OK) {
      return status;
    }
  }
  return PARSER_OK;
}

/**
 * @brief Consumes exactly one matching DEDENT delta after a nested construct.
 * @details Entry: the first expected DEDENT. Exit: the token after the last
 * consumed DEDENT.
 * @param lexer Initialized lexer producing layout tokens.
 * @param indent_delta Number of TOKEN_DEDENT tokens required.
 * @return PARSER_OK when all requested DEDENTs occur in order, a syntax error
 * for another token, or a translated lexer failure.
 */
static ParserStatus parser_consume_dedents(Lexer *lexer, size_t indent_delta) {
  ParserStatus status;

  if (lexer == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }
  while (indent_delta > 0U) {
    if (lexer->token.type == TOKEN_EOF) {
      return PARSER_OK;
    }
    status = parser_expect_and_consume(lexer, TOKEN_DEDENT);
    if (status != PARSER_OK) {
      return status;
    }
    indent_delta -= 1U;
  }
  return PARSER_OK;
}

/**
 * @brief Parses a function definition
 * `def id ( params ) [: Type] = (expr | NEWLINE INDENT block DEDENT)`.
 * @details Entry: @c lexer->token is @c def. Exit: the token after the
 * function definition.
 * @param lexer Initialized lexer.
 * @param program Program receiving the function.
 * @param function Output pointer to the parsed function.
 * @return PARSER_OK or the first syntax, semantic, internal or lexer error.
 */
static ParserStatus p_function(Lexer *lexer, Program *program,
                               FunctionSymbol **return_function) {
  ParserStatus status;

  if (lexer == NULL || program == NULL || return_function == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  /// Consume `def`
  status = parser_expect_and_consume(lexer, TOKEN_KW_DEF);
  if (status != PARSER_OK) {
    return status;
  }

  /// Get the function name and add it to the program's function table.
  if (lexer->token.type != TOKEN_IDENTIFIER) {
    return PARSER_SYNTAX_ERROR;
  }

  FunctionSymbol *function;
  if (function_table_get_or_create(&program->functions,
                                   lexer->token.value.string_value,
                                   &function) != FUNCTION_TABLE_OK) {
    return PARSER_INTERNAL_ERROR;
  }

  /// Check if the function is not already defined
  if (function->is_defined) {
    return PARSER_SEMANTIC_ERROR;
  }
  function->is_defined = true;
  *return_function = function;

  /// Consume the function name
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  /// Parse the function's argument list
  status = p_def_args(lexer, program, function);
  if (status != PARSER_OK) {
    return status;
  }

  /// Parse the optional return type
  status = p_type(lexer, &function->return_type, false, false, NULL);
  if (status != PARSER_OK) {
    return status;
  }

  /// Consume `=` to indicate the start of the function body
  status = parser_expect_and_consume(lexer, TOKEN_ASSIGN);
  if (status != PARSER_OK) {
    return status;
  }

  /// Check for an implicit return on the same line
  if (lexer->token.type != TOKEN_NEWLINE) {
    ASTNode *implicit_return_node = NULL;
    // TODO: uncomment once p_expression is implemented.
    // status = p_expression(lexer, program, function, function->local_scope,
    //                       &implicit_return_node);
    // if (status != PARSER_OK) {
    //   return status;
    // }

    if (function_symbol_set_ast_root(function, implicit_return_node) !=
        FUNCTION_TABLE_OK) {
      ast_node_destroy(implicit_return_node);
      return PARSER_INTERNAL_ERROR;
    }
    return PARSER_OK;
  }

  /// Consume the NEWLINE
  status = parser_consume_newlines(lexer, true, false, NULL);
  if (status != PARSER_OK) {
    return status;
  }

  ASTNode *block = NULL;

  /// Parse the function body as a code block and transfer its AST to the
  /// function.
  status =
      p_code_block(lexer, program, function, function->local_scope, &block);
  if (status != PARSER_OK) {
    return status;
  }

  if (function_symbol_set_ast_root(function, block) != FUNCTION_TABLE_OK) {
    ast_node_destroy(block);
    return PARSER_INTERNAL_ERROR;
  }

  if (block->inferred_type.type != TYPE_UNKNOWN) {
    status = parser_status_from_type(
        type_entity_resolve(&function->return_type, block->inferred_type.type));
  } else {
    status = parser_status_from_type(
        type_entity_depend(&block->inferred_type, &function->return_type));
  }
  if (status != PARSER_OK) {
    return status;
  }

  /// TODO: Last statement in the block must be a return ASTNode if the last
  /// statement doesnt have value it is implicitly returning unit.

  return PARSER_OK;
}

/**
 * @brief Parses an @c main function.
 * @details Grammar: `@main def id () [: Unit] = ...`.
 * Entry: @c lexer->token is @c @main. Exit: the token after the main
 * function.
 * @param lexer Initialized lexer.
 * @param program Program receiving the function.
 * @return PARSER_OK or the first syntax, semantic, internal or lexer error.
 */
static ParserStatus p_main_function(Lexer *lexer, Program *program) {
  ParserStatus status;

  if (lexer == NULL || program == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  /// Consume `@main`
  status = parser_expect_and_consume(lexer, TOKEN_MAIN_ANNOTATION);
  if (status != PARSER_OK) {
    return status;
  }

  /// Consume newlines after `@main`
  status = parser_consume_newlines(lexer, false, true, NULL);
  if (status != PARSER_OK) {
    return status;
  }

  /// Parse the function definition (p_function consumes `def` itself)
  /// and get the FunctionSymbol
  FunctionSymbol *function = NULL;
  status = p_function(lexer, program, &function);
  if (status != PARSER_OK) {
    return status;
  }

  /// Check that arg count is zero
  if (function->argument_count != 0U) {
    return PARSER_SEMANTIC_ERROR; /// Code 5
  }

  /// Check that the return type is UNIT
  status = parser_status_from_type(
      type_entity_resolve(&function->return_type, TYPE_UNIT));
  if (status != PARSER_OK) {
    return status; /// Code 6 on a type conflict
  }

  /// Mark the function as the main function
  function->is_main = true;

  /// p_function already left the next unprocessed token in lexer->token.
  return PARSER_OK;
}

/**
 * @brief Consumes a program directly from its lexer.
 * @details Loads the first lookahead token and dispatches top-level
 * productions until @ref TOKEN_EOF. Every production leaves the next
 * unprocessed token in @c lexer->token.
 * @param lexer Freshly initialized lexer.
 * @param program Initialized output owner for the parsed program.
 * @return PARSER_OK at end of input or the first lexer/parser failure.
 */
ParserStatus p_program(Lexer *lexer, Program *program) {
  bool main_function_found = false;
  ParserStatus status;

  if (lexer == NULL || program == NULL || !program->initialized) {
    return PARSER_INVALID_ARGUMENT;
  }

  /// Load the first lookahead token.
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  while (lexer->token.type != TOKEN_EOF) {
    /// Dispatch only the currently sketched top-level constructs.
    if (lexer->token.type == TOKEN_KW_DEF) {
      /// Parse the function definition (p_function consumes `def`)
      FunctionSymbol *function = NULL;
      status = p_function(lexer, program, &function);
    } else if (lexer->token.type == TOKEN_MAIN_ANNOTATION) {
      if (main_function_found) {
        status = PARSER_SEMANTIC_ERROR; /// Code 4: redefinition
      } else {
        /// Parse the main function (p_main_function consumes `@main`)
        main_function_found = true;
        status = p_main_function(lexer, program);
      }
    } else {
      status = PARSER_SYNTAX_ERROR;
    }

    if (status != PARSER_OK) {
      return status;
    }
  }
  return PARSER_OK;
}
