/**
 * @file parser.h
 * Project: IFJ26 compiler
 * @brief Public parser interface and parsed-program representation.
 * @author Vojtěch Ondráček (xondrav00)
 * @author Šimon Hrouda (xhrouds00)
 */

#ifndef SRC_PARSER_H_
#define SRC_PARSER_H_

#include <stdbool.h>

#include "function_table.h"
#include "lexer.h"

/**
 * @brief Reports parser and lexer outcomes observed during syntactic analysis.
 * @details Every value that corresponds to an error of the compiled program
 * equals the compiler exit code from the specification (section 6). Use
 * @ref parser_status_exit_code to obtain the exit code of any status.
 */
typedef enum {
  PARSER_OK = 0,
  /** Lexical error (exit code 1). */
  PARSER_LEXER_ERROR = 1,
  /** Syntax error (exit code 2). */
  PARSER_SYNTAX_ERROR = 2,
  /** Use of an undefined or not yet defined variable (exit code 3). */
  PARSER_UNDEFINED_ERROR = 3,
  /** Redefinition of a function, variable or parameter (exit code 4). */
  PARSER_REDEFINITION_ERROR = 4,
  /** Disallowed parameters of the @c main function (exit code 5). */
  PARSER_BAD_CALL_ERROR = 5,
  /** Type compatibility or inference error (exit code 6). */
  PARSER_TYPE_ERROR = 6,
  /** Internal compiler error, e.g. failed allocation (exit code 99). */
  PARSER_INTERNAL_ERROR = 99,
  /** API misuse by the caller; reported as an internal error (exit code 99).
   */
  PARSER_INVALID_ARGUMENT = 100
} ParserStatus;

/**
 * @brief Converts a parser status to the compiler exit code.
 * @param status Status returned by a parser function.
 * @return The exit code defined by the specification.
 */
int parser_status_exit_code(ParserStatus status);

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

/** Releases every function, scope tree, and AST in a program.
 */
void program_destroy(Program *program);

/**
 * @brief Parses one whole program into caller-owned output storage.
 * @details Loads the first token into @c lexer->token and runs the recursive
 * p_* productions. Each production starts with its first token in
 * @c lexer->token and returns with the first unprocessed token there (see
 * specifikace.txt, "Lookahead token").
 * @param lexer Freshly initialized lexer borrowed for the duration of the
 * parse.
 * @param program Initialized output owner that receives function/scope/AST
 * data.
 * @return A parser status or a translated lexer failure.
 */
ParserStatus p_program(Lexer *lexer, Program *program);

#endif // SRC_PARSER_H_
