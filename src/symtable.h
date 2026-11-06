/**
 * @file symtable.h
 * @brief Lexical symbol table declarations.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was partially generated using an LLM agent based on
 *       specifikace.txt.
 */

#ifndef SRC_SYMTABLE_H_
#define SRC_SYMTABLE_H_

#include <stdbool.h>
#include <stddef.h>

#include "bst.h"
#include "type.h"

typedef struct SymbolTable SymbolTable;

/** Represents one non-function identifier bound in exactly one lexical scope.
 */
typedef struct {
  char *name;
  TypeEntity type;
  bool initialized;
  SymbolTable *owner;
} Symbol;

/** Reports an operation on a lexical symbol table. */
typedef enum {
  SYMTABLE_OK,
  SYMTABLE_DUPLICATE,
  SYMTABLE_INVALID_ARGUMENT,
  SYMTABLE_OUT_OF_MEMORY
} SymbolTableStatus;

/**
 * Represents one lexical scope. Its bindings are owned by one BVS from main;
 * parent and children connect scopes into the lexical scope tree.
 */
struct SymbolTable {
  SymbolTable *parent;
  SymbolTable **children;
  size_t child_count;
  size_t child_capacity;
  BST *bindings;
};

/** Creates an owned root scope with an empty BVS for its bindings. */
SymbolTable *symbol_table_create_root(void);

/** Creates and attaches an owned child scope, or returns null on failure. */
SymbolTable *symbol_table_create_child(SymbolTable *parent);

/** Recursively destroys a root scope, its child scopes, and their BVS bindings.
 */
void symbol_table_destroy(SymbolTable *table);

/** Looks up a name only in the BVS of one exact scope. */
Symbol *symbol_table_lookup_local(const SymbolTable *table, const char *name);

/** Looks up a name in a scope and then every parent scope. */
Symbol *symbol_table_resolve(const SymbolTable *start, const char *name);

/**
 * @brief Defines a symbol in one exact scope.
 * @details Duplicate names in the same scope are rejected; child scopes may
 * shadow parent names. On success @p symbol receives the BVS-owned binding if
 * it is non-null. A binding's identity is its @c Symbol pointer, not a numeric
 * ID.
 */
SymbolTableStatus symbol_table_define(SymbolTable *table, const char *name,
                                      DataType type, bool initialized,
                                      Symbol **symbol);

#endif // SRC_SYMTABLE_H_
