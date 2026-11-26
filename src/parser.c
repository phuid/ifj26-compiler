/**
 * @file parser.c
 * Project: IFJ26 compiler
 * @brief Syntactic analysis of IFJ26 source code.
 * @author Vojtěch Ondráček (xondrav00)
 * @author Šimon Hrouda (xhrouds00)
 * @note Code in this file was partially generated using an LLM agent based on
 *       specifikace.txt.
 */

#include "parser.h"

#include "error_trace.h"

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
  /// A failed allocation is not caused by the input program (exit code 99).
  if (status == LEXER_OUT_OF_MEMORY) {
    return PARSER_INTERNAL_ERROR;
  }
  return PARSER_LEXER_ERROR;
}

/* Maps every status to its exit code; API misuse is an internal error. */
int parser_status_exit_code(ParserStatus status) {
  if (status == PARSER_INVALID_ARGUMENT) {
    return PARSER_INTERNAL_ERROR;
  }
  return (int)status;
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

/* Initializes the owned FunctionTable that anchors every parsed function. */
ParserStatus program_init(Program *program) {
  if (program == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }
  if (function_table_init(&program->functions) != FUNCTION_TABLE_OK) {
    ERROR_INTERNAL("failed to initialize the function table");
    return PARSER_INTERNAL_ERROR;
  }
  program->initialized = true;
  return PARSER_OK;
}

/* Releases the complete semantic output without touching the borrowed lexer.
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
  if (status != LEXER_OK) {
    ERROR_TRACE("while reading the next token");
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
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  ParserStatus status;

  if (lexer->token.type != expected) {
    ERROR_ORIGIN(&lexer->token, "syntax error: expected '%s'",
                 error_trace_token_type_name(expected));
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
  bool newline_consumed = false;

  if (lexer == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  /// Consume newlines until the first non-newline token is reached.
  if (lexer->token.type == TOKEN_NEWLINE) {
    newline_consumed = true;
    status = parser_advance(lexer);
    /// If the newline is required but not present, report a syntax error.
  } else if (required) {
    /// If the token is EOF, it counts as newline if it is required
    if (lexer->token.type != TOKEN_EOF) {
      ERROR_ORIGIN(&lexer->token, "syntax error: expected the end of the line");
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
            ERROR_INTERNAL("indentation level overflow");
            return PARSER_INTERNAL_ERROR;
          }
          (*indent_delta)++;
        } else {
          if (*indent_delta == PTRDIFF_MIN) {
            ERROR_INTERNAL("indentation level underflow");
            return PARSER_INTERNAL_ERROR;
          }
          (*indent_delta)--;
        }
        if (*indent_delta <= 0) {
          ERROR_ORIGIN(&lexer->token,
                       "syntax error: a continuation line inside parentheses "
                       "must be indented more than the opening line");
          return PARSER_SYNTAX_ERROR;
        }
      }
      status = parser_advance(lexer);
    }
  }

  /// A line following a newline inside parentheses must be indented more than
  /// the line with the opening parenthesis. A line with the same indentation
  /// emits no layout token, so it has to be checked here.
  if (status == PARSER_OK && newline_consumed && consume_indent_dedent &&
      indent_delta != NULL && *indent_delta <= 0) {
    ERROR_ORIGIN(&lexer->token,
                 "syntax error: a continuation line inside parentheses must "
                 "be indented more than the opening line");
    return PARSER_SYNTAX_ERROR;
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
 * @brief Parses a scalar or recursively nested list type name.
 * @details Entry: the first token of the type name. Exit: the token after the
 * complete type name.
 */
static ParserStatus p_type_name(Lexer *lexer, bool consume_newlines_indents,
                                ptrdiff_t *indentation_level, DataType *type) {
  ParserStatus status;
  BaseType base;

  if (lexer == NULL || type == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  base = keyword_to_type(lexer->token.type);
  if (base != TYPE_UNKNOWN) {
    *type = type_scalar(base);
    return parser_advance(lexer);
  }

  if (lexer->token.type != TOKEN_KW_LIST) {
    ERROR_ORIGIN(&lexer->token, "syntax error: expected a type name");
    return PARSER_SYNTAX_ERROR;
  }

  /// Consume 'List' and the opening bracket.
  status = parser_expect_and_consume(lexer, TOKEN_KW_LIST);
  if (status != PARSER_OK) {
    return status;
  }
  if (consume_newlines_indents) {
    status = parser_consume_newlines(lexer, false, true, indentation_level);
    if (status != PARSER_OK) {
      ERROR_TRACE("after 'List' in a type name");
      return status;
    }
  }
  status = parser_expect_and_consume(lexer, TOKEN_LEFT_BRACKET);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing a list type name");
    return status;
  }

  ptrdiff_t local_indent = 0U;
  status = parser_consume_newlines(lexer, false, true, &local_indent);
  if (status != PARSER_OK) {
    ERROR_TRACE("after '[' of a list type name");
    return status;
  }

  /// Parse the element type, which may itself be a list.
  DataType element_type;
  status = p_type_name(lexer, consume_newlines_indents, &local_indent,
                       &element_type);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the element type of a list type name");
    return status;
  }

  if (consume_newlines_indents) {
    status = parser_consume_newlines(lexer, false, true, &local_indent);
    if (status != PARSER_OK) {
      ERROR_TRACE("after the element type of a list type name");
      return status;
    }
  }
  status = parser_expect_and_consume(lexer, TOKEN_RIGHT_BRACKET);
  if (status != PARSER_OK) {
    ERROR_TRACE("while closing a list type name");
    return status;
  }

  /// Reset the indentation level to what it was before the opening
  /// bracket
  status = parser_status_from_lexer(
      lexer_change_current_indent(lexer, (-1) * local_indent));
  if (status != PARSER_OK) {
    ERROR_TRACE("while restoring the indentation after a list type name");
    return status;
  }

  *type = type_list(element_type);
  if (type_is_unknown(*type)) {
    ERROR_INTERNAL("failed to build a list type");
  }
  return type_is_unknown(*type) ? PARSER_INTERNAL_ERROR : PARSER_OK;
}

/**
 * @brief Parses an optional type annotation `: Type`.
 * @details Entry: @c lexer->token is the possible @c :. Without @c : nothing
 * is consumed. Exit: the token after the complete type name.
 * @param lexer Initialized lexer.
 * @param out_type Set to the annotated type; left unchanged without @c :.
 * @param required If true, a type annotation is required.
 * @param consume_newlines_indents If true, any number of newlines and layout
 * tokens is consumed.
 * @param indentation_level If @p consume_newlines_indents is true, the current
 * indentation level is updated. Pass NULL if @p consume_newlines_indents is
 * false.
 * @return PARSER_OK, PARSER_SYNTAX_ERROR, or a translated lexer failure.
 */
static ParserStatus p_type(Lexer *lexer, DataType *out_type, bool required,
                           bool consume_newlines_indents,
                           ptrdiff_t *indentation_level) {
  ParserStatus status;

  if (lexer == NULL || out_type == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }
  if (consume_newlines_indents && indentation_level == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }
  if (!consume_newlines_indents && indentation_level != NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  /// Check for `:`
  if (lexer->token.type != TOKEN_COLON) {
    if (required) {
      ERROR_ORIGIN(&lexer->token, "syntax error: expected ':' and a type");
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
      ERROR_TRACE("after ':' of a type annotation");
      return status;
    }
  }

  status =
      p_type_name(lexer, consume_newlines_indents, indentation_level, out_type);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the type name of a type annotation");
  }
  return status;
}

static ParserStatus p_call_args(Lexer *lexer, Program *program,
                                FunctionSymbol *function, SymbolTable *scope,
                                ASTNode *call_node) {
  if (lexer == NULL || program == NULL || function == NULL || scope == NULL ||
      call_node == NULL) {
    ERROR_INTERNAL("invalid argument");
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
    ERROR_TRACE("after '(' of a call");
    return status;
  }

  /// Parse the function call arguments
  if (lexer->token.type != TOKEN_RIGHT_PAREN) {
    while (true) {
      if (lexer->token.type == TOKEN_EOF) {
        ERROR_ORIGIN(
            &lexer->token,
            "syntax error: expected ')' to close the argument list of a call");
        return PARSER_SYNTAX_ERROR;
      }

      ASTNode *arg_node = NULL;
      status = p_expression(lexer, program, function, scope, &arg_node, true,
                            &local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("while parsing argument %zu of a call",
                    call_node->data.call.argument_count + 1U);
        return status;
      }

      /// Add the argument node to the call node's argument list
      if (ast_node_call_append_argument(call_node, arg_node) != AST_OK) {
        ast_node_destroy(arg_node);
        ERROR_INTERNAL("failed to append a call argument");
        return PARSER_INTERNAL_ERROR;
      }

      /// Consume any number of newlines before the next comma or closing
      /// parenthesis
      status = parser_consume_newlines(lexer, false, true, &local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("after argument %zu of a call",
                    call_node->data.call.argument_count);
        return status;
      }

      /// Expect a comma or closing parenthesis
      if (lexer->token.type == TOKEN_RIGHT_PAREN) {
        break;
      }
      status = parser_expect_and_consume(lexer, TOKEN_COMMA);
      if (status != PARSER_OK) {
        ERROR_TRACE("after argument %zu of a call",
                    call_node->data.call.argument_count);
        return status;
      }

      /// Consume any number of newlines before the next argument
      bool has_newline = lexer->token.type == TOKEN_NEWLINE;
      status = parser_consume_newlines(lexer, false, true, &local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("after ',' in the arguments of a call");
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
    ERROR_TRACE(
        "while restoring the indentation after the arguments of a call");
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
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }
  if (consume_newlines_indents && local_indent == NULL) {
    ERROR_INTERNAL("invalid argument");
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
      ERROR_TRACE("after 'if'");
      return status;
    }
  }

  /// Parse the boolean expression
  ASTNode *condition_node = NULL;
  status = p_expression(lexer, program, function, scope, &condition_node,
                        consume_newlines_indents, local_indent);
  if (status != PARSER_OK) {
    ast_node_destroy(condition_node);
    ERROR_TRACE("while parsing the condition of an if");
    return status;
  }

  /// Inside parentheses: Consume any number of layout tokens after the boolean
  /// expression
  if (consume_newlines_indents) {
    status = parser_consume_newlines(lexer, false, true, local_indent);
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ERROR_TRACE("after the condition of an if");
      return status;
    }
  }

  /// Parse the 'then' keyword
  status = parser_expect_and_consume(lexer, TOKEN_KW_THEN);
  if (status != PARSER_OK) {
    ast_node_destroy(condition_node);
    ERROR_TRACE("after the condition of an if");
    return status;
  }

  /// Inside parentheses: Consume any number of layout tokens after the 'then'
  /// keyword
  if (consume_newlines_indents) {
    status = parser_consume_newlines(lexer, false, true, local_indent);
    if (status != PARSER_OK) {
      ERROR_TRACE("after 'then'");
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
      ERROR_INTERNAL("failed to allocate the scope of a then branch");
      return PARSER_INTERNAL_ERROR;
    }

    status =
        p_statement(lexer, program, function, then_scope, &then_branch_node);
    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing the one-line then branch of an if");
    }
    /// `if` needs to end an expression only to delimit a for-loop guard; on the
    /// same line after a one-line branch it cannot start another statement.
    /// e.g. `4 + 4 if x then 5 else 6` is invalid
    if (status == PARSER_OK && lexer->token.type == TOKEN_KW_IF) {
      ERROR_ORIGIN(&lexer->token, "syntax error: 'if' cannot follow a one-line "
                                  "then branch on the same line");
      status = PARSER_SYNTAX_ERROR;
    }
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
      ERROR_TRACE("after the one-line then branch of an if");
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
      ERROR_TRACE("after 'then'");
      ast_node_destroy(condition_node);
      return status;
    }

    /// create a new child scope for the 'then' block
    SymbolTable *then_scope = symbol_table_create_child(scope);
    if (then_scope == NULL) {
      ast_node_destroy(condition_node);
      ERROR_INTERNAL("failed to allocate the scope of a then branch");
      return PARSER_INTERNAL_ERROR;
    }

    /// Parse the 'then' branch
    status =
        p_code_block(lexer, program, function, then_scope, &then_branch_node);
    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing the then block of an if");
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      return status;
    }

    /// Consume any number of newlines after the 'then' branch
    status = parser_consume_newlines(lexer, false, false, NULL);
    if (status != PARSER_OK) {
      ERROR_TRACE("after the then block of an if");
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
      ERROR_INTERNAL("failed to allocate an if node");
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

  /// Inside parentheses: Consume any number of layout tokens after the 'else'
  /// keyword
  if (consume_newlines_indents) {
    status = parser_consume_newlines(lexer, false, true, local_indent);
    if (status != PARSER_OK) {
      ERROR_TRACE("after 'else'");
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
      ERROR_INTERNAL("failed to allocate the scope of an else branch");
      return PARSER_INTERNAL_ERROR;
    }

    if (lexer->token.type == TOKEN_KW_IF) {
      status = p_if(lexer, program, function, else_scope, &else_branch_node,
                    consume_newlines_indents, local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("while parsing the else-if branch of an if");
      }
    } else {
      status =
          p_statement(lexer, program, function, else_scope, &else_branch_node);
      if (status != PARSER_OK) {
        ERROR_TRACE("while parsing the one-line else branch of an if");
      }
      /// See the one-line then branch above.
      if (status == PARSER_OK && lexer->token.type == TOKEN_KW_IF) {
        ERROR_ORIGIN(&lexer->token, "syntax error: 'if' cannot follow a "
                                    "one-line else branch on the same line");
        status = PARSER_SYNTAX_ERROR;
      }
    }
    if (status != PARSER_OK) {
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      ast_node_destroy(else_branch_node);
      return status;
    }

    /// Consume any number of newlines after the 'else' statement
    /// also consume any indents/dedents if inside parentheses
    if (consume_newlines_indents) {
      status = parser_consume_newlines(lexer, false, true, local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("after the one-line else branch of an if");
        ast_node_destroy(condition_node);
        ast_node_destroy(then_branch_node);
        ast_node_destroy(else_branch_node);
        return status;
      }
    }

  } else {
    /// If there are newlines after the 'else' keyword, parse the 'else' branch
    /// as a code block

    /// Consume any number of newlines
    status = parser_consume_newlines(lexer, false, false, NULL);
    if (status != PARSER_OK) {
      ERROR_TRACE("after 'else'");
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      return status;
    }

    /// Create a new child scope for the 'else' block
    SymbolTable *else_scope = symbol_table_create_child(scope);
    if (else_scope == NULL) {
      ERROR_INTERNAL("failed to allocate the scope of an else branch");
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      return PARSER_INTERNAL_ERROR;
    }

    /// Parse the 'else' branch
    status =
        p_code_block(lexer, program, function, else_scope, &else_branch_node);
    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing the else block of an if");
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      ast_node_destroy(else_branch_node);
      return status;
    }

    /// Consume any number of newlines after the 'else' branch
    status = parser_consume_newlines(lexer, false, false, NULL);
    if (status != PARSER_OK) {
      ERROR_TRACE("after the else block of an if");
      ast_node_destroy(condition_node);
      ast_node_destroy(then_branch_node);
      ast_node_destroy(else_branch_node);
      return status;
    }
  }

  ASTNode *if_node = ast_node_create_if(condition_node, then_branch_node,
                                        else_branch_node, if_line, if_column);
  if (if_node == NULL) {
    ERROR_INTERNAL("failed to allocate an if node");
    ast_node_destroy(condition_node);
    ast_node_destroy(then_branch_node);
    ast_node_destroy(else_branch_node);
    return PARSER_INTERNAL_ERROR;
  }

  *return_node = if_node;

  return PARSER_OK;
}

/// Marks an if node and all nested if branches as expressions.
static void parser_mark_if_chain_as_expression(ASTNode *if_node) {
  if (if_node == NULL || if_node->kind != AST_IF) {
    return;
  }

  if_node->data.if_statement.used_as_an_expression = true;
  parser_mark_if_chain_as_expression(if_node->data.if_statement.then_branch);
  parser_mark_if_chain_as_expression(if_node->data.if_statement.else_branch);
}

static ParserStatus p_list_constructor_args(Lexer *lexer, Program *program,
                                            FunctionSymbol *function,
                                            SymbolTable *scope,
                                            ASTNode *constructor_node) {
  if (lexer == NULL || program == NULL || scope == NULL ||
      constructor_node == NULL) {
    ERROR_INTERNAL("invalid argument");
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
    ERROR_TRACE("after '(' of a list constructor");
    return status;
  }

  /// Parse the list constructor arguments
  if (lexer->token.type != TOKEN_RIGHT_PAREN) {
    while (true) {
      if (lexer->token.type == TOKEN_EOF) {
        ERROR_ORIGIN(&lexer->token,
                     "syntax error: expected ')' to close a list constructor");
        return PARSER_SYNTAX_ERROR;
      }

      ASTNode *arg_node = NULL;
      status = p_expression(lexer, program, function, scope, &arg_node, true,
                            &local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("while parsing element %zu of a list constructor",
                    constructor_node->data.call.argument_count + 1U);
        return status;
      }

      /// Add the argument node to the list constructor node's argument list
      if (ast_node_call_append_argument(constructor_node, arg_node) != AST_OK) {
        ast_node_destroy(arg_node);
        ERROR_INTERNAL("failed to append a list element");
        return PARSER_INTERNAL_ERROR;
      }

      /// Consume any number of newlines before the next comma or closing
      /// parenthesis
      status = parser_consume_newlines(lexer, false, true, &local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("after element %zu of a list constructor",
                    constructor_node->data.call.argument_count);
        return status;
      }

      /// Expect a comma or closing parenthesis
      if (lexer->token.type == TOKEN_RIGHT_PAREN) {
        break;
      }
      status = parser_expect_and_consume(lexer, TOKEN_COMMA);
      if (status != PARSER_OK) {
        ERROR_TRACE("after element %zu of a list constructor",
                    constructor_node->data.call.argument_count);
        return status;
      }

      /// Consume any number of newlines before the next argument
      bool has_newline = lexer->token.type == TOKEN_NEWLINE;
      status = parser_consume_newlines(lexer, false, true, &local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("after ',' in a list constructor");
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
    ERROR_TRACE("while restoring the indentation after a list constructor");
    return status;
  }

  return parser_expect_and_consume(lexer, TOKEN_RIGHT_PAREN);
}

static ParserStatus p_int_literal(Lexer *lexer, ASTNode **return_node) {
  *return_node = ast_node_create_int(lexer->token.value.int_value,
                                     lexer->token.line, lexer->token.column);
  if (*return_node == NULL) {
    ERROR_INTERNAL("failed to allocate an Int literal node");
    return PARSER_INTERNAL_ERROR;
  }
  return parser_advance(lexer);
}

static ParserStatus p_double_literal(Lexer *lexer, ASTNode **return_node) {
  *return_node = ast_node_create_real(lexer->token.value.double_value,
                                      lexer->token.line, lexer->token.column);
  if (*return_node == NULL) {
    ERROR_INTERNAL("failed to allocate a Double literal node");
    return PARSER_INTERNAL_ERROR;
  }
  return parser_advance(lexer);
}

static ParserStatus p_string_literal(Lexer *lexer, ASTNode **return_node) {
  *return_node = ast_node_create_string(lexer->token.value.string_value,
                                        lexer->token.line, lexer->token.column);
  if (*return_node == NULL) {
    ERROR_INTERNAL("failed to allocate a String literal node");
    return PARSER_INTERNAL_ERROR;
  }
  return parser_advance(lexer);
}

static ParserStatus p_true_literal(Lexer *lexer, ASTNode **return_node) {
  *return_node =
      ast_node_create_bool(true, lexer->token.line, lexer->token.column);
  if (*return_node == NULL) {
    ERROR_INTERNAL("failed to allocate a Bool literal node");
    return PARSER_INTERNAL_ERROR;
  }
  return parser_advance(lexer);
}

static ParserStatus p_false_literal(Lexer *lexer, ASTNode **return_node) {
  *return_node =
      ast_node_create_bool(false, lexer->token.line, lexer->token.column);
  if (*return_node == NULL) {
    ERROR_INTERNAL("failed to allocate a Bool literal node");
    return PARSER_INTERNAL_ERROR;
  }
  return parser_advance(lexer);
}

static ParserStatus p_list_primary(Lexer *lexer, Program *program,
                                   FunctionSymbol *function, SymbolTable *scope,
                                   ASTNode **return_node,
                                   bool consume_newlines_indents,
                                   ptrdiff_t *indentation_level) {
  ParserStatus status;
  status = parser_expect_and_consume(lexer, TOKEN_KW_LIST);
  if (status != PARSER_OK) {
    return status;
  }

  /// Inside parentheses: Consume any number of layout tokens after the 'List'
  if (consume_newlines_indents) {
    status = parser_consume_newlines(lexer, false, true, indentation_level);
    if (status != PARSER_OK) {
      ERROR_TRACE("after 'List'");
      return status;
    }
  }

  if (lexer->token.type == TOKEN_DOT) {
    /// List.range(1,10,2)
    status = parser_expect_and_consume(lexer, TOKEN_DOT);
    if (status != PARSER_OK) {
      return status;
    }

    /// indentifier
    if (lexer->token.type != TOKEN_IDENTIFIER) {
      ERROR_ORIGIN(&lexer->token,
                   "syntax error: expected a method name after 'List.'");
      return PARSER_SYNTAX_ERROR;
    }
    const char *method_name = lexer->token.value.string_value;
    if (strcmp(method_name, "range") != 0) {
      ERROR_ORIGIN(&lexer->token, "unknown method 'List.%s'", method_name);
      return PARSER_SYNTAX_ERROR;
    }

    status = parser_expect_and_consume(lexer, TOKEN_IDENTIFIER);
    if (status != PARSER_OK) {
      return status;
    }

    /// Inside parentheses: Consume any number of layout tokens after the '.'
    if (consume_newlines_indents) {
      status = parser_consume_newlines(lexer, false, true, indentation_level);
      if (status != PARSER_OK) {
        ERROR_TRACE("after 'List.range'");
        return status;
      }
    }

    /// lpar
    status = parser_expect_and_consume(lexer, TOKEN_LEFT_PAREN);
    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing a call to 'List.range'");
      return status;
    }

    ptrdiff_t local_indent = 0;
    status = parser_consume_newlines(lexer, false, true, &local_indent);
    if (status != PARSER_OK) {
      ERROR_TRACE("after '(' of 'List.range'");
      return status;
    }

    /// args, convert range(x,y,z) to operators ((x until y) by z)
    ASTNode *from_expr = NULL;
    status = p_expression(lexer, program, function, scope, &from_expr, true,
                          &local_indent);
    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing argument 1 of 'List.range'");
      return status;
    }

    status = parser_consume_newlines(lexer, false, true, &local_indent);
    if (status != PARSER_OK) {
      ERROR_TRACE("after argument 1 of 'List.range'");
      ast_node_destroy(from_expr);
      return status;
    }

    status = parser_expect_and_consume(lexer, TOKEN_COMMA);
    if (status != PARSER_OK) {
      ERROR_TRACE("after argument 1 of 'List.range'");
      ast_node_destroy(from_expr);
      return status;
    }

    status = parser_consume_newlines(lexer, false, true, &local_indent);
    if (status != PARSER_OK) {
      ERROR_TRACE("after ',' in 'List.range'");
      ast_node_destroy(from_expr);
      return status;
    }

    /// second argument (until)
    ASTNode *until_expr = NULL;
    status = p_expression(lexer, program, function, scope, &until_expr, true,
                          &local_indent);
    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing argument 2 of 'List.range'");
      ast_node_destroy(from_expr);
      return status;
    }

    status = parser_consume_newlines(lexer, false, true, &local_indent);
    if (status != PARSER_OK) {
      ERROR_TRACE("after argument 2 of 'List.range'");
      ast_node_destroy(from_expr);
      ast_node_destroy(until_expr);
      return status;
    }

    /// third argument is optional
    ASTNode *by_expr = NULL;
    if (lexer->token.type == TOKEN_COMMA) {
      status = parser_expect_and_consume(lexer, TOKEN_COMMA);
      if (status != PARSER_OK) {
        ast_node_destroy(from_expr);
        ast_node_destroy(until_expr);
        return status;
      }

      status = parser_consume_newlines(lexer, false, true, &local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("after ',' in 'List.range'");
        ast_node_destroy(from_expr);
        ast_node_destroy(until_expr);
        return status;
      }

      /// third argument (by)
      status = p_expression(lexer, program, function, scope, &by_expr, true,
                            &local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("while parsing argument 3 of 'List.range'");
        ast_node_destroy(from_expr);
        ast_node_destroy(until_expr);
        return status;
      }

      status = parser_consume_newlines(lexer, false, true, &local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("after argument 3 of 'List.range'");
        ast_node_destroy(from_expr);
        ast_node_destroy(until_expr);
        ast_node_destroy(by_expr);
        return status;
      }
    }

    /// reset indentation level to what it was before the opening parenthesis
    status = parser_status_from_lexer(
        lexer_change_current_indent(lexer, (-1) * local_indent));
    if (status != PARSER_OK) {
      ERROR_TRACE("while restoring the indentation after 'List.range'");
      ast_node_destroy(from_expr);
      ast_node_destroy(until_expr);
      ast_node_destroy(by_expr);
      return status;
    }

    /// build the expression ((x until y) by z)
    ASTNode *to_node =
        ast_node_create_binary_op(AST_UNTIL, from_expr, until_expr,
                                  lexer->token.line, lexer->token.column);
    if (to_node == NULL) {
      ERROR_INTERNAL("failed to allocate a range node");
      ast_node_destroy(from_expr);
      ast_node_destroy(until_expr);
      ast_node_destroy(by_expr);
      return PARSER_INTERNAL_ERROR;
    }

    if (by_expr != NULL) {
      /// wrap in by operator
      ASTNode *by_node = ast_node_create_binary_op(
          AST_BY, to_node, by_expr, lexer->token.line, lexer->token.column);
      if (by_node == NULL) {
        ERROR_INTERNAL("failed to allocate a range step node");
        ast_node_destroy(to_node);
        ast_node_destroy(by_expr);
        return PARSER_INTERNAL_ERROR;
      }
      *return_node = by_node;
    } else {
      *return_node = to_node;
    }

    /// rpar
    status = parser_expect_and_consume(lexer, TOKEN_RIGHT_PAREN);
    if (status != PARSER_OK) {
      ERROR_TRACE("while closing the call to 'List.range'");
      ast_node_destroy(*return_node);
      *return_node = NULL;
      return status;
    }

    return PARSER_OK;
  }

  ASTNode *list_node =
      ast_node_create_list_constructor(lexer->token.line, lexer->token.column);

  if (list_node == NULL) {
    ERROR_INTERNAL("failed to allocate a list constructor node");
    return PARSER_INTERNAL_ERROR;
  }

  status = p_list_constructor_args(lexer, program, function, scope, list_node);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the arguments of a list constructor");
    ast_node_destroy(list_node);
    return status;
  }

  *return_node = list_node;
  return PARSER_OK;
}

/**
@note This function was altered by an LLM agent to handle nested parentheses
and unit parsing in accordance with psa_spec.md. Model used: Kimi-k3.1-pro.
*/
static ParserStatus p_parenthesized_primary(Lexer *lexer, Program *program,
                                            FunctionSymbol *function,
                                            SymbolTable *scope,
                                            ASTNode **return_node) {
  ParserStatus status;
  size_t paren_line = lexer->token.line;
  size_t paren_column = lexer->token.column;

  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  ptrdiff_t nested_indent = 0;
  status = parser_consume_newlines(lexer, false, true, &nested_indent);
  if (status != PARSER_OK) {
    ERROR_TRACE("after '('");
    return status;
  }

  if (lexer->token.type == TOKEN_RIGHT_PAREN) {
    *return_node = ast_node_create_unit(paren_line, paren_column);
    if (*return_node == NULL) {
      ERROR_INTERNAL("failed to allocate a Unit node");
      return PARSER_INTERNAL_ERROR;
    }

    status = parser_status_from_lexer(
        lexer_change_current_indent(lexer, (-1) * nested_indent));
    if (status != PARSER_OK) {
      ERROR_TRACE("while closing '()'");
      ast_node_destroy(*return_node);
      *return_node = NULL;
      return status;
    }

    return parser_advance(lexer);
  }

  status = p_expression(lexer, program, function, scope, return_node, true,
                        &nested_indent);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing a parenthesized expression");
    return status;
  }
  if (*return_node == NULL) {
    ERROR_ORIGIN(&lexer->token,
                 "syntax error: expected an expression inside parentheses");
    return PARSER_SYNTAX_ERROR;
  }

  status = parser_consume_newlines(lexer, false, true, &nested_indent);
  if (status != PARSER_OK) {
    ERROR_TRACE("after a parenthesized expression");
    ast_node_destroy(*return_node);
    *return_node = NULL;
    return status;
  }

  status = parser_status_from_lexer(
      lexer_change_current_indent(lexer, (-1) * nested_indent));
  if (status != PARSER_OK) {
    ERROR_TRACE("while closing a parenthesized expression");
    ast_node_destroy(*return_node);
    *return_node = NULL;
    return status;
  }

  status = parser_expect_and_consume(lexer, TOKEN_RIGHT_PAREN);
  if (status != PARSER_OK) {
    ERROR_TRACE("while closing a parenthesized expression opened at %zu:%zu",
                paren_line, paren_column);
    ast_node_destroy(*return_node);
    *return_node = NULL;
    return status;
  }

  return PARSER_OK;
}

static ParserStatus p_identifier_primary(Lexer *lexer, Program *program,
                                         SymbolTable *scope,
                                         ASTNode **return_node,
                                         bool consume_newlines_indents,
                                         ptrdiff_t *indentation_level) {
  ParserStatus status;
  const char *token_name = lexer->token.value.string_value;
  size_t line = lexer->token.line;
  size_t column = lexer->token.column;
  size_t name_length = strlen(token_name) + 1U;
  char *name = malloc(name_length);
  if (name == NULL) {
    ERROR_INTERNAL("failed to allocate an identifier name");
    return PARSER_INTERNAL_ERROR;
  }
  memcpy(name, token_name, name_length);

  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    free(name);
    return status;
  }

  /// newline and indent/dedent consumption after the identifier is allowed
  /// when in parentheses
  if (consume_newlines_indents) {
    status = parser_consume_newlines(lexer, false, true, indentation_level);
    if (status != PARSER_OK) {
      ERROR_TRACE("after identifier '%s'", name);
      free(name);
      return status;
    }
  }

  Symbol *symbol = symbol_table_resolve(scope, name);
  if (lexer->token.type == TOKEN_LEFT_PAREN && symbol == NULL) {
    /// Without a local binding, an identifier followed by `(` is a call.
    FunctionSymbol *function = NULL;
    if (function_table_get_or_create(&program->functions, name, &function) !=
        FUNCTION_TABLE_OK) {
      ERROR_INTERNAL("failed to register function '%s'", name);
      free(name);
      return PARSER_INTERNAL_ERROR;
    }
    free(name);

    ASTNode *call_node = ast_node_create_function_call(function, line, column);
    if (call_node == NULL) {
      ERROR_INTERNAL("failed to allocate a call node");
      return PARSER_INTERNAL_ERROR;
    }

    /// Parse the function call arguments
    status = p_call_args(lexer, program, function, scope, call_node);
    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing the arguments of the call to '%s'",
                  function->name);
      ast_node_destroy(call_node);
      return status;
    }
    *return_node = call_node;
    return PARSER_OK;
  }

  /// A resolved identifier stays a primary variable reference. Any following
  /// parenthesis is left for the postfix-indexing parser.
  /// An undefined variable, or one used inside its own initializer
  /// (`val x = x`), is a use before definition (code 3).
  if (symbol == NULL || !symbol->initialized) {
    ERROR_ORIGIN_AT(line, column, name,
                    symbol == NULL
                        ? "use of undefined variable '%s'"
                        : "use of variable '%s' inside its own initializer",
                    name);
    free(name);
    return PARSER_UNDEFINED_ERROR;
  }
  free(name);
  ASTNode *variable_node = ast_node_create_variable_ref(symbol, line, column);
  if (variable_node == NULL) {
    ERROR_INTERNAL("failed to allocate a variable reference node");
    return PARSER_INTERNAL_ERROR;
  }

  *return_node = variable_node;
  return PARSER_OK;
}

static ParserStatus p_if_primary(Lexer *lexer, Program *program,
                                 FunctionSymbol *function, SymbolTable *scope,
                                 ASTNode **return_node,
                                 bool consume_newlines_indents,
                                 ptrdiff_t *indentation_level) {
  ParserStatus status;
  status = p_if(lexer, program, function, scope, return_node,
                consume_newlines_indents, indentation_level);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing an if expression");
    return status;
  }
  parser_mark_if_chain_as_expression(*return_node);
  return PARSER_OK;
}

static ParserStatus p_primary(Lexer *lexer, Program *program,
                              FunctionSymbol *function, SymbolTable *scope,
                              ASTNode **return_node,
                              bool consume_newlines_indents,
                              ptrdiff_t *indentation_level) {
  if (lexer == NULL || program == NULL || scope == NULL ||
      return_node == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  switch (lexer->token.type) {
  case TOKEN_INT:
    return p_int_literal(lexer, return_node);

  case TOKEN_DOUBLE:
    return p_double_literal(lexer, return_node);

  case TOKEN_STRING:
    return p_string_literal(lexer, return_node);

  case TOKEN_KW_TRUE:
    return p_true_literal(lexer, return_node);

  case TOKEN_KW_FALSE:
    return p_false_literal(lexer, return_node);

  case TOKEN_KW_LIST:
    return p_list_primary(lexer, program, function, scope, return_node,
                          consume_newlines_indents, indentation_level);

  case TOKEN_LEFT_PAREN:
    return p_parenthesized_primary(lexer, program, function, scope,
                                   return_node);

  case TOKEN_IDENTIFIER:
    return p_identifier_primary(lexer, program, scope, return_node,
                                consume_newlines_indents, indentation_level);

  case TOKEN_KW_IF:
    return p_if_primary(lexer, program, function, scope, return_node,
                        consume_newlines_indents, indentation_level);

  default:
    ERROR_ORIGIN(&lexer->token, "syntax error: expected an operand");
    return PARSER_SYNTAX_ERROR;
  }
}

/**
 * @brief Parses one `(expression)` list-indexing suffix.
 * @details Entry: @c lexer->token is the opening parenthesis. Exit: the token
 * after the closing parenthesis. This function takes ownership of @p list_node,
 * including on failure.
 */
static ParserStatus p_index_suffix(Lexer *lexer, Program *program,
                                   FunctionSymbol *function, SymbolTable *scope,
                                   ASTNode *list_node, ASTNode **return_node) {
  if (lexer == NULL || program == NULL || scope == NULL || list_node == NULL ||
      return_node == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  size_t indexing_line = lexer->token.line;
  size_t indexing_column = lexer->token.column;

  /// Consume the opening parenthesis.
  ParserStatus status = parser_expect_and_consume(lexer, TOKEN_LEFT_PAREN);
  if (status != PARSER_OK) {
    ast_node_destroy(list_node);
    return status;
  }

  /// Every suffix tracks layout independently from its containing expression.
  ptrdiff_t local_indent = 0;
  status = parser_consume_newlines(lexer, false, true, &local_indent);
  if (status != PARSER_OK) {
    ERROR_TRACE("after '(' of a list index");
    ast_node_destroy(list_node);
    return status;
  }

  ASTNode *index_node = NULL;
  status = p_expression(lexer, program, function, scope, &index_node, true,
                        &local_indent);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing a list index");
    ast_node_destroy(list_node);
    ast_node_destroy(index_node);
    return status;
  }

  if (index_node == NULL) {
    ERROR_INTERNAL("list index expression produced no node");
    ast_node_destroy(list_node);
    return PARSER_INTERNAL_ERROR;
  }

  status = parser_consume_newlines(lexer, false, true, &local_indent);
  if (status != PARSER_OK) {
    ERROR_TRACE("after a list index");
    ast_node_destroy(list_node);
    ast_node_destroy(index_node);
    return status;
  }

  /// Reset the indentation level to what it was before the opening parenthesis
  status = parser_status_from_lexer(
      lexer_change_current_indent(lexer, (-1) * local_indent));
  if (status != PARSER_OK) {
    ERROR_TRACE("while closing a list index");
    ast_node_destroy(list_node);
    ast_node_destroy(index_node);
    return status;
  }

  status = parser_expect_and_consume(lexer, TOKEN_RIGHT_PAREN);
  if (status != PARSER_OK) {
    ERROR_TRACE("while closing a list index opened at %zu:%zu", indexing_line,
                indexing_column);
    ast_node_destroy(list_node);
    ast_node_destroy(index_node);
    return status;
  }

  ASTNode *indexing_node = ast_node_create_list_indexing(
      list_node, index_node, indexing_line, indexing_column);
  if (indexing_node == NULL) {
    ERROR_INTERNAL("failed to allocate a list indexing node");
    ast_node_destroy(list_node);
    ast_node_destroy(index_node);
    return PARSER_INTERNAL_ERROR;
  }

  *return_node = indexing_node;
  return PARSER_OK;
}

static ParserStatus
p_member_suffix(Lexer *lexer, Program *program, FunctionSymbol *function,
                SymbolTable *scope, ASTNode *list_node, ASTNode **return_node,
                bool consume_newlines_indents, ptrdiff_t *indentation_level) {
  if (lexer == NULL || program == NULL || scope == NULL || list_node == NULL ||
      return_node == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  ParserStatus status = parser_expect_and_consume(lexer, TOKEN_DOT);
  if (status != PARSER_OK) {
    ast_node_destroy(list_node);
    return status;
  }

  if (consume_newlines_indents) {
    status = parser_consume_newlines(lexer, false, true, indentation_level);
    if (status != PARSER_OK) {
      ERROR_TRACE("after '.'");
      ast_node_destroy(list_node);
      return status;
    }
  }

  size_t member_line = lexer->token.line;
  size_t member_column = lexer->token.column;

  if (lexer->token.type != TOKEN_IDENTIFIER) {
    ERROR_ORIGIN(&lexer->token, "syntax error: expected a member name after "
                                "'.'");
    ast_node_destroy(list_node);
    return PARSER_SYNTAX_ERROR;
  }

  const char *token_name = lexer->token.value.string_value;
  size_t name_length = strlen(token_name) + 1U;
  char *name = malloc(name_length + 5); // Extra space for "list$" prefix
  if (name == NULL) {
    ERROR_INTERNAL("failed to allocate a method name");
    ast_node_destroy(list_node);
    return PARSER_INTERNAL_ERROR;
  }
  memcpy(name, "list$", 6); // Copy the prefix "list$"
  memcpy(name + 5, token_name, name_length);

  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    free(name);
    ast_node_destroy(list_node);
    return status;
  }

  FunctionSymbol *called_function = NULL;
  if (function_table_get_or_create(&program->functions, name,
                                   &called_function) != FUNCTION_TABLE_OK) {
    ERROR_INTERNAL("failed to register method '%s'", name);
    free(name);
    ast_node_destroy(list_node);
    return PARSER_INTERNAL_ERROR;
  }
  free(name);

  ASTNode *member_node = ast_node_create_function_call(
      called_function, member_line, member_column);
  if (member_node == NULL) {
    ERROR_INTERNAL("failed to allocate a method call node");
    ast_node_destroy(list_node);
    return PARSER_INTERNAL_ERROR;
  }

  ASTStatus ast_status = ast_node_call_append_argument(member_node, list_node);
  if (ast_status != AST_OK) {
    ERROR_INTERNAL("failed to append the receiver of method '%s'",
                   called_function->name);
    ast_node_destroy(member_node);
    ast_node_destroy(list_node);
    return PARSER_INTERNAL_ERROR;
  }

  if (consume_newlines_indents) {
    status = parser_consume_newlines(lexer, false, true, indentation_level);
    if (status != PARSER_OK) {
      ERROR_TRACE("after method '%s'", called_function->name);
      ast_node_destroy(member_node);
      return status;
    }
  }

  if (lexer->token.type == TOKEN_LEFT_PAREN) {
    status = p_call_args(lexer, program, function, scope, member_node);
    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing the arguments of method '%s'",
                  called_function->name);
      ast_node_destroy(member_node);
      return status;
    }
  }

  *return_node = member_node;
  return PARSER_OK;
}

/**
 * @brief Parses a primary operand with suffixes and prefix operators.
 * @details Prefix operators recurse so repeated unary operators associate from
 * the right. Indexing and member suffixes are completed before the operand
 * enters binary precedence parsing; repeated suffixes wrap the preceding
 * result from left to right.
 */
static ParserStatus p_operand(Lexer *lexer, Program *program,
                              FunctionSymbol *function, SymbolTable *scope,
                              ASTNode **return_node,
                              bool consume_newlines_indents,
                              ptrdiff_t *indentation_level) {
  if (lexer == NULL || program == NULL || scope == NULL ||
      return_node == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  ASTNode *operand_node = NULL;
  ParserStatus status = PARSER_OK;

  /// Parse prefix operators recursively so chains associate from the right.
  if (lexer->token.type == TOKEN_MINUS || lexer->token.type == TOKEN_PLUS ||
      lexer->token.type == TOKEN_NOT) {
    TokenType unary_op = lexer->token.type;
    size_t unary_line = lexer->token.line;
    size_t unary_column = lexer->token.column;

    status = parser_advance(lexer);
    if (status != PARSER_OK) {
      return status;
    }
    if (consume_newlines_indents) {
      status = parser_consume_newlines(lexer, false, true, indentation_level);
      if (status != PARSER_OK) {
        ERROR_TRACE("after a unary operator");
        return status;
      }
    }

    status = p_operand(lexer, program, function, scope, &operand_node,
                       consume_newlines_indents, indentation_level);
    if (status != PARSER_OK) {
      ast_node_destroy(operand_node);
      return status;
    }

    /// Unary `+` needs no wrapper because it does not change the operand.
    if (unary_op == TOKEN_NOT) {
      ASTNode *not_node =
          ast_node_create_not(operand_node, unary_line, unary_column);
      if (not_node == NULL) {
        ERROR_INTERNAL("failed to allocate a '!' node");
        ast_node_destroy(operand_node);
        return PARSER_INTERNAL_ERROR;
      }
      operand_node = not_node;
    } else if (unary_op == TOKEN_MINUS) {
      /// The AST has no unary-minus kind, so represent negation as `0 - x`.
      ASTNode *zero_node = ast_node_create_int(0, unary_line, unary_column);
      if (zero_node == NULL) {
        ERROR_INTERNAL("failed to allocate a negation node");
        ast_node_destroy(operand_node);
        return PARSER_INTERNAL_ERROR;
      }
      ASTNode *neg_node = ast_node_create_binary_op(
          AST_SUBTRACT, zero_node, operand_node, unary_line, unary_column);
      if (neg_node == NULL) {
        ERROR_INTERNAL("failed to allocate a negation node");
        ast_node_destroy(zero_node);
        ast_node_destroy(operand_node);
        return PARSER_INTERNAL_ERROR;
      }
      operand_node = neg_node;
    }

    *return_node = operand_node;
    return PARSER_OK;
  }

  status = p_primary(lexer, program, function, scope, &operand_node,
                     consume_newlines_indents, indentation_level);
  if (status != PARSER_OK) {
    ast_node_destroy(operand_node);
    return status;
  }

  while (true) {
    if (consume_newlines_indents) {
      status = parser_consume_newlines(lexer, false, true, indentation_level);
      if (status != PARSER_OK) {
        ERROR_TRACE("after an operand");
        ast_node_destroy(operand_node);
        return status;
      }
    }

    if (lexer->token.type == TOKEN_LEFT_PAREN) {
      ASTNode *indexing_node = NULL;
      status = p_index_suffix(lexer, program, function, scope, operand_node,
                              &indexing_node);
      if (status != PARSER_OK) {
        ERROR_TRACE("while parsing an index suffix");
        return status;
      }
      operand_node = indexing_node;
    } else if (lexer->token.type == TOKEN_DOT) {
      ASTNode *member_node = NULL;
      status = p_member_suffix(lexer, program, function, scope, operand_node,
                               &member_node, consume_newlines_indents,
                               indentation_level);
      if (status != PARSER_OK) {
        ERROR_TRACE("while parsing a member access");
        return status;
      }
      operand_node = member_node;
    } else {
      break;
    }
  }

  *return_node = operand_node;
  return PARSER_OK;
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
 * @param function Function whose body contains the expression.
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
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }
  if (consume_newlines_indents && local_indent == NULL) {
    ERROR_INTERNAL("invalid argument");
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
  /// Set once an if operand has been parsed outside parentheses.
  bool ended_by_if = false;

  while (true) {
    if (expecting_operand) {
      /// Before an operand, consume newlines and layout only in constructs
      /// that permit multiline expressions, such as parentheses and call
      /// arguments.
      if (consume_newlines_indents) {
        status = parser_consume_newlines(lexer, false, true, local_indent);
        if (status != PARSER_OK) {
          ERROR_TRACE("before an operand");
          terminal_stack_cleanup_nodes(&stack);
          return status;
        }
      }

      /// Delegate one primary operand to p_operand. It consumes that operand
      /// and any nested syntax it owns, leaving the next operator or
      /// delimiter as the current lookahead token.
      ASTNode *operand_node = NULL;
      if (!consume_newlines_indents && lexer->token.type == TOKEN_KW_IF) {
        ended_by_if = true;
      }
      status = p_operand(lexer, program, function, scope, &operand_node,
                         consume_newlines_indents, local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("while parsing an operand of an expression");
        ast_node_destroy(operand_node);
        terminal_stack_cleanup_nodes(&stack);
        return status;
      }
      if (operand_node == NULL) {
        ERROR_INTERNAL("operand produced no node");
        terminal_stack_cleanup_nodes(&stack);
        return PARSER_INTERNAL_ERROR;
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
          ERROR_TRACE("after an operand");
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
      TerminalType input_terminal = token_to_terminal(lexer->token);

      /// Outside parentheses an if operand (`val x = if c then 1 else 2`)
      /// swallows the rest of its line and may also consume the NEWLINE (or
      /// the DEDENT of its block) ending it. The current token then already
      /// belongs to the next statement, so the expression ends here.
      if (ended_by_if) {
        input_terminal = TERMINAL_DOLLAR;
      }

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
        ERROR_ORIGIN(&lexer->token,
                     "syntax error: incomplete expression before this token");
        terminal_stack_cleanup_nodes(&stack);
        return PARSER_SYNTAX_ERROR;
      }

      /// Reject tokens that are neither binary operators nor valid expression
      /// delimiters, and stack-only symbols that cannot index the table. The
      /// offending lookahead is left for the caller's diagnostic context.
      if (input_terminal == TERMINAL_INVALID ||
          top_terminal.type >= TERMINAL_COUNT) {
        ERROR_ORIGIN(&lexer->token, "syntax error: expected an operator or "
                                    "the end of the expression");
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
          ERROR_TRACE("while reducing an expression before %zu:%zu",
                      lexer->token.line, lexer->token.column);
          terminal_stack_cleanup_nodes(&stack);
          return status;
        }
        break;

      case PREC_INVALID:
      default:
        ERROR_ORIGIN(&lexer->token,
                     "syntax error: operator cannot follow this expression");
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
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  /// Remember the position of the 'val'/'var' keyword, then consume it
  size_t definition_line = lexer->token.line;
  size_t definition_column = lexer->token.column;
  status =
      parser_expect_and_consume(lexer, constant ? TOKEN_KW_VAL : TOKEN_KW_VAR);
  if (status != PARSER_OK) {
    return status;
  }

  /// Expect an identifier for the variable name
  if (lexer->token.type != TOKEN_IDENTIFIER) {
    ERROR_ORIGIN(&lexer->token,
                 "syntax error: expected a variable name after '%s'",
                 constant ? "val" : "var");
    return PARSER_SYNTAX_ERROR;
  }

  /// Keep the name and annotation until the initializer has been parsed. Only
  /// then is the variable added to its scope, so the initializer cannot refer
  /// to the variable being defined.
  size_t name_length = strlen(lexer->token.value.string_value) + 1U;
  char *variable_name = malloc(name_length);
  if (variable_name == NULL) {
    ERROR_INTERNAL("failed to allocate a variable name");
    return PARSER_INTERNAL_ERROR;
  }
  memcpy(variable_name, lexer->token.value.string_value, name_length);

  /// Consume the variable name
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    free(variable_name);
    return status;
  }

  /// Parse the optional type annotation for the variable
  DataType variable_type = type_unknown();
  status = p_type(lexer, &variable_type, false, false, NULL);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the type annotation of variable '%s'",
                variable_name);
    free(variable_name);
    return status;
  }

  /// Consume the assignment operator
  status = parser_expect_and_consume(lexer, TOKEN_ASSIGN);
  if (status != PARSER_OK) {
    ERROR_TRACE("in the definition of variable '%s'", variable_name);
    free(variable_name);
    return status;
  }

  /// Parse the expression for the variable's initial value
  ASTNode *expression_node = NULL;
  status = p_expression(lexer, program, function, scope, &expression_node,
                        false, NULL);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the initializer of variable '%s'",
                variable_name);
    free(variable_name);
    return status;
  }

  /// The variable is visible from the end of its definition on.
  Symbol *var_symbol;
  switch (symbol_table_define(scope, variable_name, variable_type, true,
                              constant, &var_symbol)) {
  case SYMTABLE_OK:
    break;
  case SYMTABLE_DUPLICATE:
    ERROR_ORIGIN_AT(definition_line, definition_column, variable_name,
                    "redefinition of variable '%s'", variable_name);
    ast_node_destroy(expression_node);
    free(variable_name);
    return PARSER_REDEFINITION_ERROR;
  default:
    ERROR_INTERNAL("failed to define variable '%s'", variable_name);
    ast_node_destroy(expression_node);
    free(variable_name);
    return PARSER_INTERNAL_ERROR;
  }

  /// The variable's type (if not annotated) is inferred later by the semantic
  /// pass (semantic.c), once the types of all called functions are known.

  /// Create an assignment node for the variable initialization
  ASTNode *assignment_node = ast_node_create_assign(
      var_symbol, expression_node, definition_line, definition_column);
  if (assignment_node == NULL) {
    ERROR_INTERNAL("failed to allocate an assignment node");
    ast_node_destroy(expression_node);
    free(variable_name);
    return PARSER_INTERNAL_ERROR;
  }
  assignment_node->data.assignment.is_definition = true;

  /// Return the assignment node to the caller
  *return_node = assignment_node;
  free(variable_name);

  return PARSER_OK;
}

static ParserStatus p_return_statement(Lexer *lexer, Program *program,
                                       FunctionSymbol *function,
                                       SymbolTable *scope,
                                       ASTNode **return_node) {
  ParserStatus status;

  if (lexer == NULL || program == NULL || function == NULL || scope == NULL ||
      return_node == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  /// Remember the position of the 'return' keyword, then consume it
  size_t return_line = lexer->token.line;
  size_t return_column = lexer->token.column;
  status = parser_expect_and_consume(lexer, TOKEN_KW_RETURN);
  if (status != PARSER_OK) {
    return status;
  }

  /// Parse the optional return expression
  ASTNode *expression_node = NULL;
  /// A bare `return` is followed by the end of the statement: a newline, a
  /// statement separator, the end of the block/file, or (CONTROL one-line
  /// branches) the `else` keyword.
  if (lexer->token.type != TOKEN_NEWLINE && lexer->token.type != TOKEN_DEDENT &&
      lexer->token.type != TOKEN_EOF && lexer->token.type != TOKEN_SEMICOLON &&
      lexer->token.type != TOKEN_KW_ELSE) {
    status = p_expression(lexer, program, function, scope, &expression_node,
                          false, NULL);
    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing the value of a return statement");
      ast_node_destroy(expression_node);
      return status;
    }
    /// The expression is kept even in a Unit function: it is still evaluated
    /// (and type-checked, and may call functions), only its value is dropped.
  }

  /// Create a return statement node; a bare `return` gets an implicit
  /// AST_CONSTANT_UNIT value from ast_node_create_return.
  ASTNode *return_node_local =
      ast_node_create_return(expression_node, return_line, return_column);
  if (return_node_local == NULL) {
    ERROR_INTERNAL("failed to allocate a return node");
    ast_node_destroy(expression_node);
    return PARSER_INTERNAL_ERROR;
  }

  /// Compatibility with the function's return type is checked by the
  /// semantic pass (semantic.c).

  /// Return the return statement node to the caller
  *return_node = return_node_local;

  return PARSER_OK;
}

/**
 * @brief Parses a loop (while or for) body after `do`.
 * @details Entry: the token after `do`. A NEWLINE opens an indented block,
 * anything else is a one-line statement (like an if branch). Exit: the token
 * after the block's DEDENT(s), or after the one-line statement (its line end
 * is left for the caller). The body gets its own child scope of @p scope.
 */
static ParserStatus p_loop_body(Lexer *lexer, Program *program,
                                FunctionSymbol *function, SymbolTable *scope,
                                ASTNode **return_node) {
  ParserStatus status;

  if (lexer == NULL || program == NULL || scope == NULL ||
      return_node == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  /// The body forms a separate scope.
  SymbolTable *body_scope = symbol_table_create_child(scope);
  if (body_scope == NULL) {
    ERROR_INTERNAL("failed to allocate the scope of a loop body");
    return PARSER_INTERNAL_ERROR;
  }

  if (lexer->token.type != TOKEN_NEWLINE) {
    /// One-line body: `while c do stmt`.
    return p_statement(lexer, program, function, body_scope, return_node);
  }

  /// Block body: NEWLINE INDENT statements DEDENT.
  status = parser_consume_newlines(lexer, true, false, NULL);
  if (status != PARSER_OK) {
    return status;
  }
  return p_code_block(lexer, program, function, body_scope, return_node);
}

/**
 * @brief Parses a while loop `while condition do body`.
 * @details Entry: @c lexer->token is @c while. Exit: the token after the body
 * (see @ref p_loop_body).
 */
static ParserStatus p_while(Lexer *lexer, Program *program,
                            FunctionSymbol *function, SymbolTable *scope,
                            ASTNode **return_node) {
  if (lexer == NULL || program == NULL || scope == NULL ||
      return_node == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  size_t while_line = lexer->token.line;
  size_t while_column = lexer->token.column;

  /// Consume the 'while' keyword
  ParserStatus status = parser_expect_and_consume(lexer, TOKEN_KW_WHILE);
  if (status != PARSER_OK) {
    return status;
  }

  /// Parse the condition; `do` ends the expression.
  ASTNode *condition_node = NULL;
  status = p_expression(lexer, program, function, scope, &condition_node, false,
                        NULL);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the condition of a while loop");
    return status;
  }

  /// Consume the 'do' keyword
  status = parser_expect_and_consume(lexer, TOKEN_KW_DO);
  if (status != PARSER_OK) {
    ERROR_TRACE("after the condition of a while loop");
    ast_node_destroy(condition_node);
    return status;
  }

  /// Parse the body, which may be a one-line statement or a block.
  ASTNode *body_node = NULL;
  status = p_loop_body(lexer, program, function, scope, &body_node);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the body of a while loop");
    ast_node_destroy(condition_node);
    ast_node_destroy(body_node);
    return status;
  }

  /// Create the while loop AST node
  ASTNode *while_node = ast_node_create_while(condition_node, body_node,
                                              while_line, while_column);
  if (while_node == NULL) {
    ERROR_INTERNAL("failed to allocate a while node");
    ast_node_destroy(condition_node);
    ast_node_destroy(body_node);
    return PARSER_INTERNAL_ERROR;
  }

  *return_node = while_node;
  return PARSER_OK;
}

/**
 * @brief Parses one for-loop guard `if expression` and joins it to @p guard.
 * @details Entry: @c lexer->token is @c if. Exit: the token after the guard
 * expression. Multiple guards are joined by `&&`, which evaluates them in
 * order with the same short-circuit semantics. On failure @p guard is
 * destroyed and set to null.
 */
static ParserStatus p_for_guard(Lexer *lexer, Program *program,
                                FunctionSymbol *function, SymbolTable *scope,
                                ASTNode **guard) {
  size_t if_line = lexer->token.line;
  size_t if_column = lexer->token.column;

  /// Consume the 'if' keyword
  ParserStatus status = parser_expect_and_consume(lexer, TOKEN_KW_IF);
  if (status == PARSER_OK) {

    /// Parse the guard expression; `do` ends the expression.
    ASTNode *condition_node = NULL;
    status = p_expression(lexer, program, function, scope, &condition_node,
                          false, NULL);
    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing a guard of a for loop");
    }
    if (status == PARSER_OK) {

      if (*guard == NULL) {
        /// If this is the first guard, it becomes the current guard.
        *guard = condition_node;
        return PARSER_OK;
      }

      /// Join the new guard with any previous guards using a logical AND.
      ASTNode *and_node = ast_node_create_binary_op(
          AST_AND, *guard, condition_node, if_line, if_column);
      if (and_node != NULL) {
        /// The new AND node becomes the current guard, and the previous guard
        /// is now owned by it.
        *guard = and_node;
        return PARSER_OK;
      }
      ERROR_INTERNAL("failed to allocate a for loop guard node");
      ast_node_destroy(condition_node);
      status = PARSER_INTERNAL_ERROR;
    }
  }

  ast_node_destroy(*guard);
  *guard = NULL;
  return status;
}

/**
 * @brief Parses a for loop with one generator and any number of guards.
 * @details Grammar:
 *   one-line:  `for id <- source {if guard} do body`
 *   multiline: `for NEWLINE INDENT id <- source {if guard}
 *              {NEWLINE if guard {if guard}} NEWLINE DEDENT do body`
 * In the multiline form the generator and guard lines are aligned and
 * indented more than `for`; `do` is aligned with `for`. The source is a list
 * (ranges `to`/`until`/`by` are list-valued operators). It is parsed in the
 * enclosing scope, so `for i <- 0 until i` refers to an outer `i`. The control
 * variable is a new immutable variable of a loop scope, which also holds the
 * guards; the body gets a child scope of it, so it may shadow the variable.
 * Entry: @c lexer->token is @c for. Exit: the token after the body.
 */
static ParserStatus p_for(Lexer *lexer, Program *program,
                          FunctionSymbol *function, SymbolTable *scope,
                          ASTNode **return_node) {
  if (lexer == NULL || program == NULL || scope == NULL ||
      return_node == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  size_t for_line = lexer->token.line;
  size_t for_column = lexer->token.column;

  /// Consume the 'for' keyword
  ParserStatus status = parser_expect_and_consume(lexer, TOKEN_KW_FOR);
  if (status != PARSER_OK) {
    return status;
  }

  /// Multiline header: the generator starts on the next, indented line.
  bool multiline = lexer->token.type == TOKEN_NEWLINE;
  size_t indent_delta = 0U;
  if (multiline) {
    status = parser_consume_newlines(lexer, true, false, NULL);
    if (status != PARSER_OK) {
      ERROR_TRACE("after 'for'");
      return status;
    }
    status = parser_consume_indent_delta(lexer, &indent_delta);
    if (status != PARSER_OK) {
      ERROR_TRACE("while reading the indentation of a multiline for header");
      return status;
    }
    if (indent_delta == 0U) {
      ERROR_ORIGIN(&lexer->token,
                   "syntax error: expected an indented generator after 'for'");
      return PARSER_SYNTAX_ERROR;
    }
  }

  /// Generator: `id <- source`. Keep a copy of the name, the token is
  /// replaced while the source is parsed.
  if (lexer->token.type != TOKEN_IDENTIFIER) {
    ERROR_ORIGIN(&lexer->token,
                 "syntax error: expected a loop variable name after 'for'");
    return PARSER_SYNTAX_ERROR;
  }
  size_t name_length = strlen(lexer->token.value.string_value) + 1U;
  char *name = malloc(name_length);
  if (name == NULL) {
    ERROR_INTERNAL("failed to allocate a loop variable name");
    return PARSER_INTERNAL_ERROR;
  }
  memcpy(name, lexer->token.value.string_value, name_length);

  status = parser_advance(lexer);
  if (status == PARSER_OK) {
    status = parser_expect_and_consume(lexer, TOKEN_FOR_GENERATOR);
  }
  if (status != PARSER_OK) {
    ERROR_TRACE("after the loop variable '%s'", name);
    free(name);
    return status;
  }

  /// The source does not see the control variable yet.
  ASTNode *iterable_node = NULL;
  status = p_expression(lexer, program, function, scope, &iterable_node, false,
                        NULL);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the source of the for loop over '%s'", name);
    free(name);
    return status;
  }

  /// Define the control variable in a new loop scope. It is initialized and
  /// immutable; its type (element type of the source) is set by the semantic
  /// pass.
  SymbolTable *loop_scope = symbol_table_create_child(scope);
  Symbol *variable = NULL;
  if (loop_scope == NULL ||
      symbol_table_define(loop_scope, name, type_unknown(), true, true,
                          &variable) != SYMTABLE_OK) {
    ERROR_INTERNAL("failed to define the loop variable '%s'", name);
    free(name);
    ast_node_destroy(iterable_node);
    return PARSER_INTERNAL_ERROR;
  }
  free(name);

  /// Guards: any number of `if guard`, on the generator line and, in the
  /// multiline form, on further aligned lines.
  ASTNode *guard_node = NULL;
  while (status == PARSER_OK) {
    if (lexer->token.type == TOKEN_KW_IF) {
      status = p_for_guard(lexer, program, function, loop_scope, &guard_node);
      if (status != PARSER_OK) {
        ERROR_TRACE("while parsing a guard of the for loop over '%s'",
                    variable->name);
      }
    } else if (multiline && lexer->token.type == TOKEN_NEWLINE) {
      /// A line with the same indentation emits no layout token, so a guard
      /// line directly follows the NEWLINE; a DEDENT ends the header.
      status = parser_consume_newlines(lexer, true, false, NULL);
      if (status != PARSER_OK) {
        ERROR_TRACE("in the header of the for loop over '%s'", variable->name);
      }
      if (status == PARSER_OK && lexer->token.type != TOKEN_KW_IF) {
        break;
      }
    } else {
      break;
    }
  }

  /// Multiline header: return to the indentation of `for`.
  if (status == PARSER_OK && multiline) {
    if (lexer->token.type == TOKEN_EOF) {
      ERROR_ORIGIN(&lexer->token,
                   "syntax error: expected 'do' after the for loop header");
      status = PARSER_SYNTAX_ERROR;
    } else {
      status = parser_consume_dedents(lexer, indent_delta);
      if (status != PARSER_OK) {
        ERROR_TRACE("while closing the header of the for loop over '%s'",
                    variable->name);
      }
    }
  }

  /// Consume the 'do' keyword
  if (status == PARSER_OK) {
    status = parser_expect_and_consume(lexer, TOKEN_KW_DO);
    if (status != PARSER_OK) {
      ERROR_TRACE("after the header of the for loop over '%s'", variable->name);
    }
  }

  ASTNode *body_node = NULL;
  if (status == PARSER_OK) {
    status = p_loop_body(lexer, program, function, loop_scope, &body_node);
    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing the body of the for loop over '%s'",
                  variable->name);
    }
  }
  if (status != PARSER_OK) {
    ast_node_destroy(iterable_node);
    ast_node_destroy(guard_node);
    ast_node_destroy(body_node);
    return status;
  }

  ASTNode *for_node = ast_node_create_for(variable, iterable_node, guard_node,
                                          body_node, for_line, for_column);
  if (for_node == NULL) {
    ERROR_INTERNAL("failed to allocate a for node");
    ast_node_destroy(iterable_node);
    ast_node_destroy(guard_node);
    ast_node_destroy(body_node);
    return PARSER_INTERNAL_ERROR;
  }

  *return_node = for_node;
  return PARSER_OK;
}

/**
 * @brief Parses a statement starting with an identifier: either an expression
 * statement or an assignment `id = výraz-nebo-volání`.
 * @details The lexer has only one token of lookahead, so the statement is first
 * parsed as an expression. If `=` follows, that expression must be a bare
 * variable reference, which becomes the assignment target. Whether the
 * variable is modifiable and the value's type are checked by the semantic pass.
 * Entry: @c lexer->token is the identifier. Exit: the token after the
 * statement.
 */
static ParserStatus p_identifier_statement(Lexer *lexer, Program *program,
                                           FunctionSymbol *function,
                                           SymbolTable *scope,
                                           ASTNode **return_node) {
  size_t line = lexer->token.line;
  size_t column = lexer->token.column;

  ASTNode *target_node = NULL;
  ParserStatus status =
      p_expression(lexer, program, function, scope, &target_node, false, NULL);
  /// Parse expression until token assign or until the end of the expression.
  /// If assign is next then the expression must be a variable reference
  /// and the statement is an assignment. Otherwise, the statement is a plain
  /// expression.
  if (status != PARSER_OK || lexer->token.type != TOKEN_ASSIGN) {
    /// A plain expression statement (or an error).
    *return_node = target_node;
    return status;
  }

  /// Only a variable can be assigned to (not `a + 1 = 2` or `list(0) = 2`).
  if (target_node->kind != AST_VARIABLE_REF) {
    ERROR_ORIGIN_AT(target_node->line, target_node->column, NULL,
                    "syntax error: only a variable can be assigned to");
    ast_node_destroy(target_node);
    return PARSER_SYNTAX_ERROR;
  }
  Symbol *target = target_node->data.variable.symbol;
  ast_node_destroy(target_node);

  /// Consume the assignment operator
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  /// Parse the assigned value
  ASTNode *value_node = NULL;
  status =
      p_expression(lexer, program, function, scope, &value_node, false, NULL);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the value assigned to '%s'", target->name);
    return status;
  }

  ASTNode *assignment_node =
      ast_node_create_assign(target, value_node, line, column);
  if (assignment_node == NULL) {
    ERROR_INTERNAL("failed to allocate an assignment node");
    ast_node_destroy(value_node);
    return PARSER_INTERNAL_ERROR;
  }
  assignment_node->data.assignment.is_definition = false;

  *return_node = assignment_node;
  return PARSER_OK;
}

static ParserStatus p_statement(Lexer *lexer, Program *program,
                                FunctionSymbol *function, SymbolTable *scope,
                                ASTNode **return_node) {
  if (lexer == NULL || program == NULL || scope == NULL ||
      return_node == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  ParserStatus status;
  const char *context;

  switch (lexer->token.type) {
  case TOKEN_KW_IF:
    context = "if statement";
    status = p_if(lexer, program, function, scope, return_node, false, NULL);
    break;
  case TOKEN_KW_WHILE:
    context = "while loop";
    status = p_while(lexer, program, function, scope, return_node);
    break;
  case TOKEN_KW_FOR:
    context = "for loop";
    status = p_for(lexer, program, function, scope, return_node);
    break;
  case TOKEN_KW_VAL:
    context = "val definition";
    status = p_variable_init_statement(lexer, program, true, function, scope,
                                       return_node);
    break;
  case TOKEN_KW_VAR:
    context = "var definition";
    status = p_variable_init_statement(lexer, program, false, function, scope,
                                       return_node);
    break;
  case TOKEN_KW_RETURN:
    context = "return statement";
    status = p_return_statement(lexer, program, function, scope, return_node);
    break;

  /// Assignment or expression
  case TOKEN_IDENTIFIER:
    context = "assignment or expression statement";
    status =
        p_identifier_statement(lexer, program, function, scope, return_node);
    break;

  /// Expression
  case TOKEN_LEFT_PAREN:
  case TOKEN_INT:
  case TOKEN_DOUBLE:
  case TOKEN_STRING:
  case TOKEN_NOT:
  case TOKEN_KW_TRUE:
  case TOKEN_KW_FALSE:
  case TOKEN_MINUS:
  case TOKEN_KW_LIST:
    context = "expression statement";
    status =
        p_expression(lexer, program, function, scope, return_node, false, NULL);
    break;
  default:
    ERROR_ORIGIN(&lexer->token, "syntax error: a statement cannot start here");
    return PARSER_SYNTAX_ERROR;
  }

  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing %s", context);
  }
  return status;
}

/**
 * @brief Finds the compound statement a statement ends with, i.e. its rightmost
 * part if that is an if (statement or expression) or a loop whose body is an
 * indented block.
 * @details Such a construct may already have consumed the NEWLINE ending its
 * line:
 * - an if takes the rest of its line and consumes the NEWLINE while looking
 *   for a following `else`. A parenthesized if is reported as well; it is
 *   followed by `)`, so the caller still sees that line's NEWLINE and only
 *   stops requiring it;
 * - a loop with a block body has consumed the NEWLINE and DEDENT(s) of its
 *   block. A loop with a one-line body is not reported itself; the search
 *   continues in that body.
 * @return The trailing if or loop node, or NULL if the statement does not end
 * with one.
 */
static const ASTNode *parser_trailing_compound(const ASTNode *node) {
  while (node != NULL) {
    switch (node->kind) {
    case AST_IF:
      return node;
    case AST_WHILE:
    case AST_FOR: {
      const ASTNode *body = node->kind == AST_WHILE ? node->data.while_loop.body
                                                    : node->data.for_loop.body;
      if (body != NULL && body->kind == AST_BLOCK) {
        return node;
      }
      node = body;
      break;
    }
    case AST_ASSIGN:
      node = node->data.assignment.value;
      break;
    case AST_RETURN:
      node = node->data.return_statement.value;
      break;
    case AST_NOT:
      node = node->data.unary_op.operand;
      break;
    case AST_ADD:
    case AST_SUBTRACT:
    case AST_MULTIPLY:
    case AST_DIVIDE:
    case AST_LIST_CONCAT:
    case AST_EQUAL:
    case AST_NOT_EQUAL:
    case AST_LESS:
    case AST_LESS_EQUAL:
    case AST_GREATER:
    case AST_GREATER_EQUAL:
    case AST_AND:
    case AST_OR:
    case AST_TO:
    case AST_UNTIL:
    case AST_BY:
      node = node->data.binary_op.right;
      break;
    default:
      return NULL;
    }
  }
  return NULL;
}

/**
 * @brief Tells whether a statement physically ends with an indented block: the
 * last branch of an if or the body of a loop, possibly reached through a chain
 * of one-line branches/bodies.
 * @details The DEDENT closing that block has already been consumed, so the
 * statement's line has certainly ended, even if the current token is `;` (that
 * `;` then starts a new line, which is an error).
 *
 * Examples (the statement is everything shown):
 * @code
 * if c then          // true: the else branch is a block
 *   print(1)
 * else
 *   print(2)
 *
 * if c then 1 else   // true: the one-line then branch does not matter,
 *   2                //       the last (else) branch is a block
 *
 * while c do         // true: the loop body is a block
 *   print(1)
 *
 * if c then while d do   // true: the then branch is a one-line loop
 *   print(1)             //       whose body is a block
 *
 * if c then print(1) else print(2)   // false: both branches are one-line
 * while c do print(1)                // false: the body is one-line
 * val x = 1 + 2                      // false: no if or loop at the end
 * @endcode
 */
static bool parser_ends_with_block(const ASTNode *node) {
  const ASTNode *trailing = parser_trailing_compound(node);

  /// Descends into the deepest construct, because parser_trailing_compound
  /// returns the shallowest rightmost one. A trailing if may contain other
  /// constructs in its branches, so the last branch, which is the physically
  /// rightmost one, has to be checked.
  while (trailing != NULL) {
    /// A trailing loop is only reported with a block body.
    if (trailing->kind == AST_WHILE || trailing->kind == AST_FOR) {
      return true;
    }
    /// The last branch is the else branch if it exists, otherwise the then
    /// branch.
    const ASTNode *last_branch = trailing->data.if_statement.else_branch != NULL
                                     ? trailing->data.if_statement.else_branch
                                     : trailing->data.if_statement.then_branch;
    if (last_branch == NULL) {
      return false;
    }
    if (last_branch->kind == AST_BLOCK) {
      return true;
    }
    trailing = parser_trailing_compound(last_branch);
  }
  return false;
}

/**
 * @brief Parses an indented code block.
 * @details Entry: the INDENT opening the block. Exit: the first token after
 * the DEDENT(s) closing the block.
 * A final `if` is not marked as an expression here: whether the value of the
 * block is used depends on its context (declared return type, loop body), so
 * the semantic pass decides it.
 */
static ParserStatus p_code_block(Lexer *lexer, Program *program,
                                 FunctionSymbol *function, SymbolTable *scope,
                                 ASTNode **return_node) {
  ParserStatus status;

  if (lexer == NULL || program == NULL || function == NULL || scope == NULL ||
      return_node == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  /// Expect an INDENT token to start the code block
  if (lexer->token.type != TOKEN_INDENT) {
    ERROR_ORIGIN(&lexer->token,
                 "syntax error: expected an indented block in function '%s'",
                 function->name);
    return PARSER_SYNTAX_ERROR;
  }

  size_t indent_delta;

  status = parser_consume_indent_delta(lexer, &indent_delta);
  if (status != PARSER_OK) {
    ERROR_TRACE("while reading the indentation of a block in function '%s'",
                function->name);
    return status;
  }
  if (indent_delta == 0U) {
    ERROR_ORIGIN(&lexer->token,
                 "syntax error: block in function '%s' is not indented",
                 function->name);
    return PARSER_SYNTAX_ERROR;
  }

  /// Create a new block node to hold the statements in the code block
  ASTNode *block_node =
      ast_node_create_block(lexer->token.line, lexer->token.column);
  if (block_node == NULL) {
    ERROR_INTERNAL("failed to allocate a block node");
    return PARSER_INTERNAL_ERROR;
  }

  /// Keep track of whether the last statement has ended with a semicolon, then
  /// unary operands are accepted
  bool ended_with_semicolon = false;

  /// Loop over statements until a DEDENT or EOF is reached.
  while (lexer->token.type != TOKEN_DEDENT && lexer->token.type != TOKEN_EOF) {

    /// IFJ26 has no empty statement: a semicolon is only a separator after a
    /// statement, so it can never start one (`;;` and a leading `;` are
    /// errors). Deeper indentation and nested definitions are not allowed.
    if (lexer->token.type == TOKEN_INVALID ||
        lexer->token.type == TOKEN_KW_DEF ||
        lexer->token.type == TOKEN_INDENT ||
        lexer->token.type == TOKEN_SEMICOLON ||
        (!ended_with_semicolon &&
         (lexer->token.type == TOKEN_MINUS || lexer->token.type == TOKEN_PLUS ||
          lexer->token.type == TOKEN_NOT))) {
      ERROR_ORIGIN(&lexer->token,
                   lexer->token.type == TOKEN_KW_DEF
                       ? "syntax error: nested function definitions are not "
                         "allowed (in function '%s')"
                       : "syntax error: unexpected token at the start of a "
                         "statement in function '%s'",
                   function->name);
      ast_node_destroy(block_node);
      return PARSER_SYNTAX_ERROR;
    }

    /// Parse the next statement and append it to the block.
    ASTNode *statement = NULL;
    status = p_statement(lexer, program, function, scope, &statement);
    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing a statement in a block of function '%s'",
                  function->name);
      ast_node_destroy(statement);
      ast_node_destroy(block_node);
      return status;
    }

    /// True if the line of the statement may have already ended inside the
    /// statement. Then a NEWLINE after it is not required, and a `;` after it
    /// is not a separator (it would start a new line, which is rejected at the
    /// start of the next iteration).
    ///
    /// This happens when the statement ends with an if or a loop (also inside
    /// an expression, such as `val x = if c then 1 else 2`):
    /// - an if has consumed the NEWLINE ending its line while looking for a
    ///   following `else` (or the DEDENT closing its block branch):
    ///       val x = if c then 1 else 2⏎   <- NEWLINE already consumed
    ///       print(x)                      <- current token: `print`
    /// - a loop with a block body has consumed the DEDENT closing that body:
    ///       while c do⏎
    ///         print(1)⏎
    ///       print(2)                      <- current token: `print`
    /// - special case: a parenthesized if is followed by `)`, so the NEWLINE
    ///   of its line is still here; it is only no longer required. Nothing but
    ///   a NEWLINE can follow the `)` anyway:
    ///       val x = (if c then 1 else 2)⏎ <- current token: NEWLINE
    ///
    /// A one-line if or loop followed by `;` has not consumed the NEWLINE, so
    /// the `;` is an ordinary separator:
    ///       if c then print(1); print(2)  <- false, `;` separates statements
    /// After a block, though, the line has ended for sure:
    ///       if c then⏎
    ///         print(1)⏎
    ///       ; print(2)                    <- true, the `;` starts a line
    bool line_may_have_ended = parser_trailing_compound(statement) != NULL &&
                               (lexer->token.type != TOKEN_SEMICOLON ||
                                parser_ends_with_block(statement));

    /// The block's result type is determined by the semantic pass.

    ASTStatus ast_status;
    ast_status = ast_node_block_append_statement(block_node, statement);
    if (ast_status != AST_OK) {
      ERROR_INTERNAL("failed to append a statement to a block");
      ast_node_destroy(statement);
      ast_node_destroy(block_node);
      return PARSER_INTERNAL_ERROR;
    }

    ended_with_semicolon = false;
    /// A semicolon is only a separator: it must be followed by the next
    /// statement on the same logical line, so it cannot end a line, a block
    /// or the file.
    if (!line_may_have_ended && lexer->token.type == TOKEN_SEMICOLON) {
      status = parser_expect_and_consume(lexer, TOKEN_SEMICOLON);
      if (status != PARSER_OK) {
        ERROR_TRACE("after a statement in function '%s'", function->name);
        ast_node_destroy(block_node);
        return status;
      }
      if (lexer->token.type == TOKEN_NEWLINE ||
          lexer->token.type == TOKEN_DEDENT || lexer->token.type == TOKEN_EOF) {
        ERROR_ORIGIN(&lexer->token,
                     "syntax error: ';' must be followed by another statement "
                     "on the same line (in function '%s')",
                     function->name);
        ast_node_destroy(block_node);
        return PARSER_SYNTAX_ERROR;
      }
      /// The next statement is on the same line: parse it right away.
      ended_with_semicolon = true;
      continue;
    }

    /// Consume the newlines after the statement. At least one is required
    /// unless the line may have already ended inside the statement (see
    /// line_may_have_ended above).
    status = parser_consume_newlines(lexer, !line_may_have_ended, false, NULL);
    if (status != PARSER_OK) {
      ERROR_TRACE("after a statement in function '%s'", function->name);
      ast_node_destroy(block_node);
      return status;
    }
  }

  if (block_node->data.block.statement_count == 0U) {
    ERROR_ORIGIN(&lexer->token,
                 "syntax error: an indented block in function '%s' is empty",
                 function->name);
    ast_node_destroy(block_node);
    return PARSER_SYNTAX_ERROR;
  }

  /// Consume same number of DEDENT tokens as the INDENT tokens that started
  /// the block.
  status = parser_consume_dedents(lexer, indent_delta);
  if (status != PARSER_OK) {
    ERROR_TRACE("while closing a block of function '%s'", function->name);
    ast_node_destroy(block_node);
    return status;
  }

  /// A trailing `if` stays a statement (it may lack else); the
  /// value a block ends with is determined by the semantic pass. Only an `if`
  /// inside an expression is marked as one.

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
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  /// Consume the opening parenthesis
  status = parser_expect_and_consume(lexer, TOKEN_LEFT_PAREN);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing a parameter list");
    return status;
  }

  /// Create the function's local scope for its parameters
  SymbolTable *local_scope = function_symbol_create_local_scope(function);
  if (local_scope == NULL) {
    ERROR_INTERNAL("failed to allocate the local scope of function '%s'",
                   function->name);
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
    ERROR_TRACE("after '(' of the parameter list");
    return status;
  }

  /// If not an empty parameter list
  if (lexer->token.type != TOKEN_RIGHT_PAREN) {

    while (true) {
      /// Argument name must be a valid identifier
      if (lexer->token.type != TOKEN_IDENTIFIER) {
        ERROR_ORIGIN(&lexer->token,
                     "syntax error: expected a parameter name of function "
                     "'%s'",
                     function->name);
        return PARSER_SYNTAX_ERROR;
      }

      /// Add the argument to the function's local scope
      Symbol *arg_symbol;
      /// Parameters behave as immutable, initialized local variables.
      switch (symbol_table_define(local_scope, lexer->token.value.string_value,
                                  type_unknown(), true, true, &arg_symbol)) {
      case SYMTABLE_OK:
        break;
      case SYMTABLE_DUPLICATE:
        ERROR_ORIGIN(&lexer->token,
                     "redefinition of parameter '%s' of function '%s'",
                     lexer->token.value.string_value, function->name);
        return PARSER_REDEFINITION_ERROR;
      default:
        ERROR_INTERNAL("failed to define parameter '%s'",
                       lexer->token.value.string_value);
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
        ERROR_TRACE("after parameter '%s'", arg_symbol->name);
        return status;
      }

      /// Parse the mandatory type annotation for the argument
      status = p_type(lexer, &arg_symbol->type, true, true, &local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("while parsing the type of parameter '%s'",
                    arg_symbol->name);
        return status;
      }

      /// Record the parameter type in the signature (in source order).
      if (function_symbol_add_parameter(function, arg_symbol) !=
          FUNCTION_TABLE_OK) {
        ERROR_INTERNAL("failed to record parameter '%s'", arg_symbol->name);
        return PARSER_INTERNAL_ERROR;
      }

      /// Consume any number of newlines before the next comma or closing
      /// parenthesis
      status = parser_consume_newlines(lexer, false, true, &local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("after the type of parameter '%s'", arg_symbol->name);
        return status;
      }

      /// Expect a comma or closing parenthesis
      if (lexer->token.type == TOKEN_RIGHT_PAREN) {
        break;
      }
      status = parser_expect_and_consume(lexer, TOKEN_COMMA);
      if (status != PARSER_OK) {
        ERROR_TRACE("after parameter '%s'", arg_symbol->name);
        return status;
      }

      /// Consume any number of newlines before the next argument
      bool has_newline = lexer->token.type == TOKEN_NEWLINE;
      status = parser_consume_newlines(lexer, false, true, &local_indent);
      if (status != PARSER_OK) {
        ERROR_TRACE("after ',' in the parameter list");
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
    ERROR_TRACE("while restoring the indentation after the parameter list");
    return status;
  }

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
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }
  *indent_delta = 0U;
  while (lexer->token.type == TOKEN_INDENT) {
    if (*indent_delta == SIZE_MAX) {
      ERROR_INTERNAL("indentation level overflow");
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
    ERROR_INTERNAL("invalid argument");
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
 * @param return_function Output pointer to the parsed function.
 * @return PARSER_OK or the first syntax, semantic, internal or lexer error.
 */
static ParserStatus p_function(Lexer *lexer, Program *program,
                               FunctionSymbol **return_function) {
  ParserStatus status;

  if (lexer == NULL || program == NULL || return_function == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  /// Consume `def`
  status = parser_expect_and_consume(lexer, TOKEN_KW_DEF);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing a function definition");
    return status;
  }

  /// Get the function name and add it to the program's function table.
  if (lexer->token.type != TOKEN_IDENTIFIER) {
    ERROR_ORIGIN(&lexer->token, "syntax error: expected a function name");
    return PARSER_SYNTAX_ERROR;
  }

  FunctionSymbol *function;
  if (function_table_get_or_create(&program->functions,
                                   lexer->token.value.string_value,
                                   &function) != FUNCTION_TABLE_OK) {
    ERROR_INTERNAL("failed to register function '%s'",
                   lexer->token.value.string_value);
    return PARSER_INTERNAL_ERROR;
  }

  /// Check if the function is not already defined
  if (function->is_defined) {
    ERROR_ORIGIN(&lexer->token, "redefinition of function '%s'",
                 function->name);
    return PARSER_REDEFINITION_ERROR;
  }
  function->is_defined = true;
  *return_function = function;

  /// Consume the function name
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    ERROR_TRACE("after the name of function '%s'", function->name);
    return status;
  }

  /// Parse the function's argument list
  status = p_def_args(lexer, program, function);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the parameters of function '%s'",
                function->name);
    return status;
  }

  /// Parse the optional return type
  status = p_type(lexer, &function->return_type, false, false, NULL);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the return type of function '%s'",
                function->name);
    return status;
  }
  function->has_explicit_return_type = !type_is_unknown(function->return_type);

  /// Consume `=` to indicate the start of the function body
  status = parser_expect_and_consume(lexer, TOKEN_ASSIGN);
  if (status != PARSER_OK) {
    ERROR_TRACE("before the body of function '%s'", function->name);
    return status;
  }

  /// Check for an implicit return on the same line
  if (lexer->token.type != TOKEN_NEWLINE) {
    ASTNode *implicit_return_node = NULL;
    status = p_expression(lexer, program, function, function->local_scope,
                          &implicit_return_node, false, NULL);
    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing the one-line body of function '%s'",
                  function->name);
      ast_node_destroy(implicit_return_node);
      return status;
    }

    if (function_symbol_set_ast_root(function, implicit_return_node) !=
        FUNCTION_TABLE_OK) {
      ERROR_INTERNAL("failed to set the body of function '%s'", function->name);
      ast_node_destroy(implicit_return_node);
      return PARSER_INTERNAL_ERROR;
    }

    /// The one-line body must end the logical line (or the file). A body
    /// ending with an if has already consumed that NEWLINE while looking for
    /// `else` (or the DEDENT closing its final block branch), so the newline
    /// is only optional then, as in p_code_block.
    bool newline_required =
        parser_trailing_compound(implicit_return_node) == NULL;
    status = parser_consume_newlines(lexer, newline_required, false, NULL);
    if (status != PARSER_OK) {
      ERROR_TRACE("after the one-line body of function '%s'", function->name);
    }
    return status;
  }

  /// Consume the NEWLINE
  status = parser_consume_newlines(lexer, true, false, NULL);
  if (status != PARSER_OK) {
    ERROR_TRACE("after '=' of function '%s'", function->name);
    return status;
  }

  ASTNode *block = NULL;

  /// Parse the function body as a code block and transfer its AST to the
  /// function.
  status =
      p_code_block(lexer, program, function, function->local_scope, &block);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the body of function '%s'", function->name);
    return status;
  }

  if (function_symbol_set_ast_root(function, block) != FUNCTION_TABLE_OK) {
    ERROR_INTERNAL("failed to set the body of function '%s'", function->name);
    ast_node_destroy(block);
    return PARSER_INTERNAL_ERROR;
  }

  /// The return type (if not annotated) is inferred by the semantic pass.

  return PARSER_OK;
}

/**
 * @brief Parses the `@main` function.
 * @details Grammar: `@main def id () [: Unit] = ...`.
 * Entry: @c lexer->token is `@main`. Exit: the token after the main
 * function.
 * @param lexer Initialized lexer.
 * @param program Program receiving the function.
 * @return PARSER_OK or the first syntax, semantic, internal or lexer error.
 */
static ParserStatus p_main_function(Lexer *lexer, Program *program) {
  ParserStatus status;

  if (lexer == NULL || program == NULL) {
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  /// Consume `@main`
  status = parser_expect_and_consume(lexer, TOKEN_MAIN_ANNOTATION);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the main function");
    return status;
  }

  /// Consume newlines after `@main`
  status = parser_consume_newlines(lexer, false, false, NULL);
  if (status != PARSER_OK) {
    ERROR_TRACE("after '@main'");
    return status;
  }

  /// Parse the function definition (p_function consumes `def` itself)
  /// and get the FunctionSymbol
  FunctionSymbol *function = NULL;
  status = p_function(lexer, program, &function);
  if (status != PARSER_OK) {
    ERROR_TRACE("while parsing the function annotated with '@main'");
    return status;
  }

  /// Check that arg count is zero
  if (function->argument_count != 0U) {
    ERROR_ORIGIN_AT(
        function->ast_root != NULL ? function->ast_root->line : 0U,
        function->ast_root != NULL ? function->ast_root->column : 0U, NULL,
        "main function '%s' must not have parameters (has %zu)", function->name,
        function->argument_count);
    return PARSER_BAD_CALL_ERROR; /// Code 5
  }

  /// The Unit result type is checked by the semantic pass (code 6).

  /// Mark the function as the main function
  function->is_main = true;

  /// p_function already left the next unprocessed token in lexer->token.
  return PARSER_OK;
}

/*
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
    ERROR_INTERNAL("invalid argument");
    return PARSER_INVALID_ARGUMENT;
  }

  /// Load the first lookahead token.
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  while (true) {
    /// Skip newlines between (and before) the top-level definitions.
    status = parser_consume_newlines(lexer, false, false, NULL);
    if (status != PARSER_OK) {
      return status;
    }
    if (lexer->token.type == TOKEN_EOF) {
      break;
    }

    /// Dispatch only the currently sketched top-level constructs.
    if (lexer->token.type == TOKEN_KW_DEF) {
      /// Parse the function definition (p_function consumes `def`)
      FunctionSymbol *function = NULL;
      status = p_function(lexer, program, &function);
    } else if (lexer->token.type == TOKEN_MAIN_ANNOTATION) {
      if (main_function_found) {
        ERROR_ORIGIN(&lexer->token, "redefinition of the main function");
        status = PARSER_REDEFINITION_ERROR; /// Code 4: redefinition
      } else {
        /// Parse the main function (p_main_function consumes `@main`)
        main_function_found = true;
        status = p_main_function(lexer, program);
      }
    } else {
      ERROR_ORIGIN(&lexer->token, "syntax error: expected 'def' or '@main' at "
                                  "the top level");
      status = PARSER_SYNTAX_ERROR;
    }

    if (status != PARSER_OK) {
      ERROR_TRACE("while parsing the program");
      return status;
    }
  }

  if (!main_function_found) {
    ERROR_ORIGIN(&lexer->token, "missing main function ('@main')");
    return PARSER_UNDEFINED_ERROR; /// Code 3: missing main function
  }

  return PARSER_OK;
}
