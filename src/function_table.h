/**
 * @file function_table.h
 * @brief Top-level function table declarations.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was partially generated using an LLM agent based on
 *       specifikace.txt.
 */

#ifndef SRC_FUNCTION_TABLE_H_
#define SRC_FUNCTION_TABLE_H_

#include <stdbool.h>
#include <stddef.h>

#include "symtable.h"

typedef struct ASTNode ASTNode;

/** Represents one top-level function and owns the root of its BVS-backed local
 * scope tree. */
typedef struct FunctionSymbol {
  char *name;
  TypeEntity return_type;
  size_t argument_count;
  SymbolTable *local_scope;
  ASTNode *ast_root;
  bool is_defined;
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

/** Releases all function records, their local scopes, and dependency arrays. */
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

#endif // SRC_FUNCTION_TABLE_H_
