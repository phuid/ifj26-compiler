#ifndef SRC_PARSER_H_
#define SRC_PARSER_H_

#include <stdbool.h>

#include "function_table.h"
#include "lexer.h"

/** Reports parser and lexer outcomes observed during syntactic analysis. */
typedef enum {
  PARSER_OK,
  PARSER_SYNTAX_ERROR,
  PARSER_SEMANTIC_ERROR,
  PARSER_LEXER_ERROR,
  PARSER_INVALID_ARGUMENT,
  PARSER_INTERNAL_ERROR
} ParserStatus;

/**
 * @brief Owns all semantic structures produced while parsing one source file.
 * @details Each FunctionSymbol in @c functions owns its local SymbolTable root.
 * Every lexical scope node owns one BVS for bindings; parent/child links form
 * the scope tree. Each function also owns its AST root; there is no duplicate
 * global AST because the language model has one AST per function.
 */
typedef struct {
  FunctionTable functions;
  bool initialized;
} Program;

/** Initializes an empty, owned program result. */
ParserStatus program_init(Program *program);

/** Releases every function, scope tree, AST, and type dependency in a program.
 */
void program_destroy(Program *program);

/**
 * @brief Parses one whole program into caller-owned output storage.
 * @details The function directly consumes tokens with @ref lexer_pls. Grammar
 * productions are intentionally not implemented yet; their future recursive
 * functions will receive the same lexer and fill @p program structures.
 * @param lexer Initialized lexer borrowed for the duration of the parse.
 * @param program Initialized output owner that receives function/scope/AST
 * data.
 * @return A parser status or a translated lexer failure.
 */
ParserStatus p_program(Lexer *lexer, Program *program);

#endif // SRC_PARSER_H_
