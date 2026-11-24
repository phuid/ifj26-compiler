/**
 * @file semantic.h
 * @brief Semantic analysis (type checking) run over a fully parsed program.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was partially generated using an LLM agent based on
 *       specifikace.txt.
 */

#ifndef SRC_SEMANTIC_H_
#define SRC_SEMANTIC_H_

#include "parser.h"

/** Result of the semantic pass; values equal the compiler exit codes. */
typedef enum {
  SEMANTIC_OK = 0,
  /** Undefined function/variable, use before definition, missing `@main`. */
  SEMANTIC_UNDEFINED = 3,
  /** Redefinition of a function or variable. */
  SEMANTIC_REDEFINITION = 4,
  /** Wrong number or type of call arguments, or a parameter in `@main`. */
  SEMANTIC_BAD_CALL = 5,
  /** Type incompatibility, missing return type annotation, etc. */
  SEMANTIC_TYPE_ERROR = 6,
  /** Other semantic error (assignment to an immutable variable). */
  SEMANTIC_OTHER = 7,
  SEMANTIC_INTERNAL_ERROR = 99
} SemanticStatus;

/**
 * @brief Type-checks a parsed program and fills in all inferred types.
 * @details Runs after parsing, when every function signature is known:
 *  1. builtin functions that are called get their signatures; a user
 *     definition of a builtin name is a redefinition (error 4),
 *  2. every called function must be defined and `@main` must exist,
 *  3. the call graph (function -> called functions) is built from the ASTs,
 *  4. a function without an explicit return type must not contain `return`
 *     and must not lie on a call-graph cycle,
 *  5. every function is checked by a memoized DFS: a call of a function
 *     without an explicit return type first checks the callee, so return types
 *     are inferred in topological order of the (now acyclic) dependencies.
 * Types are written into the @c DataType fields of symbols, functions and AST
 * nodes; Int-to-Double conversions are inserted as AST nodes.
 * @param program Successfully parsed program.
 * @return SEMANTIC_OK or the error code of the first detected error.
 */
SemanticStatus semantic_check(Program *program);

#endif // SRC_SEMANTIC_H_
