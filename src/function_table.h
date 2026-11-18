/**
 * @file function_table.h
 * @brief Top-level function table declarations.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was mostly generated using an LLM agent based on
 *       specifikace.txt.
 */

#ifndef SRC_FUNCTION_TABLE_H_
#define SRC_FUNCTION_TABLE_H_

#include <stdbool.h>
#include <stddef.h>

#include "ast.h"
#include "symtable.h"
#include "type.h"

/** Progress of the semantic pass over one function (memoized DFS). */
typedef enum {
  FUNCTION_UNCHECKED,
  FUNCTION_CHECKING,
  FUNCTION_CHECKED
} FunctionCheckState;

/** Represents one top-level function and owns the root of its BVS-backed local
 * scope tree.
 * */
typedef struct FunctionSymbol {
  char *name;
  DataType return_type;
  /// True when the header contains `: type`. Only then may the function take
  /// part in (mutual) recursion or contain `return`.
  bool has_explicit_return_type;
  size_t argument_count;
  /// Parameter bindings in source order; @c argument_count items. Borrowed
  /// from @c local_scope (owned by its BVS); the declared type of a parameter
  /// is its symbol's type.
  Symbol **parameters;
  size_t parameter_capacity;
  SymbolTable *local_scope;
  ASTNode *ast_root;
  bool is_defined;
  bool is_main;
  bool is_builtin;

  /// Semantic-analysis data, filled by semantic_check().
  /// Call-graph edges: distinct functions called anywhere in the body
  /// (borrowed).
  FunctionSymbol **callees;
  size_t callee_count;
  size_t callee_capacity;
  bool contains_return;
  FunctionCheckState check_state;
  unsigned visit_mark; ///< Scratch mark for graph traversals.
} FunctionSymbol;

/** Owns the compiler's specialised top-level function namespace. */
typedef struct {
  FunctionSymbol **functions;
  size_t function_count;
  size_t function_capacity;
} FunctionTable;

/** Reports a function-table operation. */
typedef enum {
  FUNCTION_TABLE_OK,
  FUNCTION_TABLE_INVALID_ARGUMENT,
  FUNCTION_TABLE_OUT_OF_MEMORY
} FunctionTableStatus;

/** Initializes an empty function table. */
FunctionTableStatus function_table_init(FunctionTable *table);

/** Releases all function records, their local scopes, and ASTs. */
void function_table_destroy(FunctionTable *table);

/** Finds a function by its exact source name, or returns null. */
FunctionSymbol *function_table_lookup(const FunctionTable *table,
                                      const char *name);

/** Gets an existing function or creates an unresolved forward reference. */
FunctionTableStatus function_table_get_or_create(FunctionTable *table,
                                                 const char *name,
                                                 FunctionSymbol **function);

/** Creates the function's owned local root scope exactly once. */
SymbolTable *function_symbol_create_local_scope(FunctionSymbol *function);

/** Replaces the function-owned AST root and destroys a previous root. */
FunctionTableStatus function_symbol_set_ast_root(FunctionSymbol *function,
                                                 ASTNode *ast_root);

/**
 * Appends one declared parameter to the function signature.
 * @param function Function whose signature receives the parameter.
 * @param symbol Borrowed binding of the parameter in the function's local
 * scope; its type is the parameter's declared type.
 */
FunctionTableStatus function_symbol_add_parameter(FunctionSymbol *function,
                                                  Symbol *symbol);

/** Records a call-graph edge @p function -> @p callee (duplicates ignored). */
FunctionTableStatus function_symbol_add_callee(FunctionSymbol *function,
                                               FunctionSymbol *callee);

#endif // SRC_FUNCTION_TABLE_H_
