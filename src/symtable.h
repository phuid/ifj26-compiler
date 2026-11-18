/**
 * @file symtable.h
 * @brief Lexical symbol table declarations.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was mostly generated using an LLM agent based on
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
  bool constant;
  SymbolTable *owner;
} Symbol;

/** Array of pointers to symbols owned by a lexical symbol table. */
typedef struct {
  Symbol **data;
  size_t capacity;
} SymbolArray;

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
  BST *bindings; /// BVS of Symbol bindings in this scope; owned by the table
};

/**
 * @brief Creates an empty root scope.
 * @details The returned table owns its BVS and any child scopes later attached
 * to it. The caller must release the complete scope tree with
 * @ref symbol_table_destroy.
 * @return An owned root scope with no parent, or null on allocation failure.
 */
SymbolTable *symbol_table_create_root(void);

/**
 * @brief Creates and attaches a child scope.
 * @details The new scope has an empty BVS and is owned by @p parent. The
 * returned pointer is borrowed and must not be destroyed separately.
 * @param parent Non-null parent scope.
 * @return A borrowed pointer to the attached child, or null for an invalid
 * argument or allocation failure.
 */
SymbolTable *symbol_table_create_child(SymbolTable *parent);

/**
 * @brief Recursively destroys a scope tree.
 * @details Releases all descendants, their BVS instances, and all BVS-owned
 * symbols. Every pointer to a scope or symbol in the tree becomes invalid.
 * Passing null has no effect.
 * @param table Owned root of the scope tree to destroy, or null.
 */
void symbol_table_destroy(SymbolTable *table);

/**
 * @brief Looks up a name only in one exact scope.
 * @details Parent and child scopes are not searched. The returned symbol
 * remains owned by the scope's BVS and must not be freed by the caller.
 * @param table Non-null scope to search.
 * @param name Non-null null-terminated identifier to find.
 * @return A borrowed pointer to the local binding, or null if @p name is absent
 * or an argument is invalid. The pointer is valid until the owning scope tree
 * is destroyed.
 */
Symbol *symbol_table_lookup_local(const SymbolTable *table, const char *name);

/**
 * @brief Resolves a name in a scope and its ancestors.
 * @details The nearest binding is returned, so local definitions shadow
 * bindings with the same name in parent scopes. The returned symbol remains
 * owned by the defining scope's BVS and must not be freed by the caller.
 * @param start Non-null scope at which to begin the search.
 * @param name Non-null null-terminated identifier to resolve.
 * @return A borrowed pointer to the nearest binding, or null if @p name is not
 * found or an argument is invalid. The pointer is valid until the owning scope
 * tree is destroyed.
 */
Symbol *symbol_table_resolve(const SymbolTable *start, const char *name);

/**
 * @brief Defines a symbol in one exact scope.
 * @details Duplicate names in the same scope are rejected; child scopes may
 * shadow parent names. The table stores a BVS-owned symbol with its own copy of
 * @p name. The caller retains ownership of @p name and may free it after the
 * call. A binding's identity is its @c Symbol pointer, not a numeric ID.
 * @param table Non-null exact scope in which to define the binding.
 * @param name Non-null null-terminated identifier to copy.
 * @param type Declared data type of the symbol.
 * @param initialized Whether the symbol starts initialized.
 * @param constant Whether the symbol is immutable.
 * @param symbol Optional output parameter. If non-null, it receives a borrowed
 * pointer to the BVS-owned binding on success and is set to null on failure.
 * The caller must not free the returned binding.
 * @return @ref SYMTABLE_OK on success, @ref SYMTABLE_DUPLICATE if @p name
 * already exists in @p table, @ref SYMTABLE_INVALID_ARGUMENT for invalid
 * arguments, or @ref SYMTABLE_OUT_OF_MEMORY on allocation failure.
 */
SymbolTableStatus symbol_table_define(SymbolTable *table, const char *name,
                                      DataType type, bool initialized,
                                      bool constant, Symbol **symbol);

/**
 * @brief Collects all symbols in a scope and its descendants.
 * @details Traversal is recursive. On success the caller owns only the
 * allocated pointer array in @c return_symbols->data and must release it with
 * @c free(). Every pointed-to symbol remains owned by its defining scope's BVS;
 * the caller must not free those symbols, and their pointers are valid only
 * while the owning scope tree exists.
 * @param table Non-null root of the scope subtree to traverse.
 * @param return_symbols Output parameter. Must not own an existing allocation
 * on entry. On success it receives the allocated array and its element count in
 * @c capacity; on failure it is unchanged.
 * @return @ref SYMTABLE_OK on success, @ref SYMTABLE_INVALID_ARGUMENT for
 * invalid arguments, or @ref SYMTABLE_OUT_OF_MEMORY on allocation failure.
 */
SymbolTableStatus
symbol_table_get_scope_symbol_list(SymbolTable *table,
                                   SymbolArray *return_symbols);

#endif // SRC_SYMTABLE_H_
