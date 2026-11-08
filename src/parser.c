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
#include "symtable.h"
#include "type.h"

#include <stdbool.h>
#include <stdint.h>

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

/*
 * Lookahead rule shared by every p_* production (see specifikace.txt):
 *   entry: lexer->token is the first token of the production,
 *   exit (PARSER_OK): lexer->token is the first token the production did not
 *   process. A production advances with parser_advance only after it has
 * processed the current token.
 */

/**
 * @brief Parses an optional type annotation `: Type`.
 * @details Entry: @c lexer->token is the possible @c :. Without @c : nothing
 * is consumed. Exit: the token after the type identifier.
 * @param lexer Initialized lexer.
 * @param type_entity Type entity resolved to the annotated type.
 * @param required If true, a type annotation is required.
 * @return PARSER_OK, PARSER_SYNTAX_ERROR, or a translated lexer failure.
 */
static ParserStatus p_type(Lexer *lexer, TypeEntity *type_entity,
                           bool required) {
  ParserStatus status;

  if (lexer == NULL || type_entity == NULL) {
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
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
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

// Parses an implicit return expression on the function header line.
// Entry: the first token of the expression. Exit: the token after the
// expression.
static ParserStatus p_expression(Lexer *lexer, Program *program,
                                 FunctionSymbol *function,
                                 ASTNode **return_node) {
  if (lexer == NULL || program == NULL || return_node == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  /// TODO
  (void)function;
  return PARSER_OK;
}

static ParserStatus p_variable_init_statement(Lexer *lexer, Program *program,
                                              bool constant,
                                              FunctionSymbol *function,
                                              ASTNode **return_node) {
  ParserStatus status;

  /// Consume the 'val' or 'var' keyword
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  /// Get the function's local scope (always created by p_def_args)
  SymbolTable *local_scope = function->local_scope;

  /// Add the variable to the function's local scope
  Symbol *var_symbol;
  switch (symbol_table_define(local_scope, lexer->token.value.string_value,
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
  status = p_type(lexer, &var_symbol->type, false);
  if (status != PARSER_OK) {
    return status;
  }

  /// Expect an assignment operator
  if (lexer->token.type != TOKEN_ASSIGN) {
    return PARSER_SYNTAX_ERROR;
  }

  /// Consume the assignment operator
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  /// Parse the expression for the variable's initial value
  ASTNode *expression_node = NULL;
  status = p_expression(lexer, program, function, &expression_node);
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
                                       ASTNode **return_node) {
  ParserStatus status;

  /// Consume the 'return' keyword
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  /// Parse the optional return expression
  ASTNode *expression_node = NULL;
  if (lexer->token.type != TOKEN_NEWLINE && lexer->token.type != TOKEN_DEDENT &&
      lexer->token.type != TOKEN_EOF) {
    status = p_expression(lexer, program, function, &expression_node);
    if (status != PARSER_OK) {
      ast_node_destroy(expression_node);
      return status;
    }

    /// If the function's return type is unit, discard the expression and set it
    /// to null
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
    /// If there is no expression, resolve the return statement's inferred type
    /// to unit
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
                                FunctionSymbol *function,
                                ASTNode **return_node) {
  if (lexer == NULL || program == NULL || return_node == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  switch (lexer->token.type) {
    //  TODO: Implement these statement productions, then uncomment their cases
    //  here. case TOKEN_KW_IF:
    //    return p_if_statement(lexer, program, function, return_node);
    //    break;
    //  case TOKEN_KW_WHILE:
    //    return p_while_statement(lexer, program, function, return_node);
    //    break;
    //  case TOKEN_KW_FOR:
    //    return p_for_statement(lexer, program, function, return_node);
    //    break;
    //  case TOKEN_KW_LIST:
    //    return p_list_statement(lexer, program, function, return_node);
    //    break;
  case TOKEN_KW_VAL:
    return p_variable_init_statement(lexer, program, true, function,
                                     return_node);
  case TOKEN_KW_VAR:
    return p_variable_init_statement(lexer, program, false, function,
                                     return_node);
  case TOKEN_KW_RETURN:
    return p_return_statement(lexer, program, function, return_node);

  /// Expression
  case TOKEN_IDENTIFIER:
  case TOKEN_LEFT_PAREN:
  case TOKEN_INT:
  case TOKEN_DOUBLE:
  case TOKEN_STRING:
  case TOKEN_NOT:
  case TOKEN_MINUS:
    return p_expression(lexer, program, function, return_node);
    break;
  default:
    return PARSER_SYNTAX_ERROR;
  }
}

// TODO: Implement p_code_block and p_expression, then uncomment these
// declarations and their calls in p_function.
//
// Parses an indented code block.
// Entry: the first token of the block (after INDENT). Exit: the DEDENT closing
// the block.
static ParserStatus p_code_block(Lexer *lexer, Program *program,
                                 FunctionSymbol *function,
                                 ASTNode **return_node) {
  ParserStatus status;

  ASTNode *block_node =
      ast_node_create_block(lexer->token.line, lexer->token.column);
  if (block_node == NULL) {
    return PARSER_INTERNAL_ERROR;
  }

  /// Loop over statements until a DEDENT or EOF is reached.
  while (lexer->token.type != TOKEN_DEDENT && lexer->token.type != TOKEN_EOF) {

    if (lexer->token.type == TOKEN_INVALID ||
        lexer->token.type == TOKEN_KW_DEF) {
      return PARSER_SYNTAX_ERROR;
    }

    /// Skip any semicolons or newlines before the next statement.
    while (lexer->token.type == TOKEN_SEMICOLON ||
           lexer->token.type == TOKEN_NEWLINE) {
      status = parser_advance(lexer);
      if (status != PARSER_OK) {
        return status;
      }
    }

    /// The separators may have been the last tokens of the block.
    if (lexer->token.type == TOKEN_DEDENT || lexer->token.type == TOKEN_EOF) {
      break;
    }

    /// Parse the next statement and append it to the block.
    ASTNode *statement = NULL;
    status = p_statement(lexer, program, function, &statement);
    if (status != PARSER_OK) {
      return status;
    }

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
        return status;
      }
    }

    ASTStatus ast_status;
    ast_status = ast_node_block_append_statement(block_node, statement);
    if (ast_status != AST_OK) {
      return PARSER_INTERNAL_ERROR;
    }
  }
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
  if (lexer->token.type != TOKEN_LEFT_PAREN) {
    return PARSER_SYNTAX_ERROR;
  }
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  /// Create the function's local scope for its parameters
  SymbolTable *local_scope = function_symbol_create_local_scope(function);
  if (local_scope == NULL) {
    return PARSER_INTERNAL_ERROR;
  }

  /// Empty parameter list
  if (lexer->token.type != TOKEN_RIGHT_PAREN) {

    size_t arg_count = 0U;
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

      /// Parse the mandatory type annotation for the argument
      status = p_type(lexer, &arg_symbol->type, true);
      if (status != PARSER_OK) {
        return status;
      }

      arg_count += 1U;

      /// Expect a comma or closing parenthesis
      if (lexer->token.type == TOKEN_RIGHT_PAREN) {
        break;
      }
      if (lexer->token.type != TOKEN_COMMA) {
        return PARSER_SYNTAX_ERROR;
      }
      status = parser_advance(lexer);
      if (status != PARSER_OK) {
        return status;
      }
    }

    function->argument_count = arg_count;
  }

  /// Consume the closing parenthesis by getting the next token.
  return parser_advance(lexer);
}

/**
 * @brief Consumes one contiguous lexer-emitted INDENT delta.
 * @details Entry: the first possible INDENT. Exit: the first non-INDENT token.
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
    status = parser_advance(lexer);
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
    if (lexer->token.type != TOKEN_DEDENT) {
      return PARSER_SYNTAX_ERROR;
    }
    indent_delta -= 1U;
    status = parser_advance(lexer);
    if (status != PARSER_OK) {
      return status;
    }
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

  if (lexer == NULL || program == NULL) {
    return PARSER_INVALID_ARGUMENT;
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
  status = p_type(lexer, &function->return_type, false);
  if (status != PARSER_OK) {
    return status;
  }

  /// Consume `=` to indicate the start of the function body
  if (lexer->token.type != TOKEN_ASSIGN) {
    return PARSER_SYNTAX_ERROR;
  }
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  /// Check for an implicit return on the same line
  if (lexer->token.type != TOKEN_NEWLINE) {
    ASTNode *implicit_return_node = NULL;
    // TODO: uncomment once p_expression is implemented.
    // status = p_expression(lexer, program, &implicit_return_node);
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

  /// Consume the NEWLINE, then get the indent and parse the function's body
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  size_t indent_delta;
  ASTNode *block = NULL;

  status = parser_consume_indent_delta(lexer, &indent_delta);
  if (status != PARSER_OK) {
    return status;
  }
  if (indent_delta == 0U) {
    return PARSER_SYNTAX_ERROR;
  }

  /// Parse the function body as a code block and transfer its AST to the
  /// function.
  status = p_code_block(lexer, program, function, &block);
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

  /// Return the parsed function to the caller
  *return_function = function;

  /// Consume the matching DEDENT tokens after the function body
  return parser_consume_dedents(lexer, indent_delta);
}

/**
 * @brief Parses an @c main function.
 * @details Entry: @c lexer->token is @c @main. The concrete grammar remains
 * deliberately undefined for now; only the annotation is consumed.
 * Exit: the token after the main function.
 * @param lexer Initialized lexer.
 * @param program Program receiving the function.
 * @return PARSER_OK until the main-function grammar is specified.
 */
static ParserStatus p_main_function(Lexer *lexer, Program *program) {
  ParserStatus status;

  if (lexer == NULL || program == NULL) {
    return PARSER_INVALID_ARGUMENT;
  }

  if (lexer->token.type != TOKEN_MAIN_ANNOTATION) {
    return PARSER_SYNTAX_ERROR;
  }

  /// Check if next token is a function definition
  if (lexer->token.type != TOKEN_KW_DEF) {
    return PARSER_SYNTAX_ERROR;
  }

  /// Consume `def`
  status = parser_advance(lexer);
  if (status != PARSER_OK) {
    return status;
  }

  /// Parse the function definition and get the FunctionSymbol
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

  return parser_advance(lexer);
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
      /// Consume `def` and parse the function definition
      status = parser_advance(lexer);
      if (status != PARSER_OK) {
        return status;
      }

      FunctionSymbol *function = NULL;
      status = p_function(lexer, program, &function);
    } else if (lexer->token.type == TOKEN_MAIN_ANNOTATION) {
      if (main_function_found) {
        status = PARSER_SEMANTIC_ERROR; /// Code 4: redefinition
      } else {
        /// Consume `@main` and parse the main function
        status = parser_advance(lexer);
        if (status != PARSER_OK) {
          return status;
        }

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
