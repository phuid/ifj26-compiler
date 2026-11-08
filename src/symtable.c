/**
 * @file symtable.c
 * @brief Lexical symbol table operations.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was mostly generated using an LLM agent based on
 *       specifikace.txt.
 */

#include "symtable.h"

#include "dynamic_array.h"

#include <stdlib.h>
#include <string.h>

/** Duplicates one identifier spelling into binding-owned storage. */
static char *copy_name(const char *name) {
  size_t length = strlen(name);
  char *copy = malloc(length + 1U);

  if (copy != NULL) {
    memcpy(copy, name, length + 1U);
  }
  return copy;
}

/** Compares two symbol bindings by their identifier spelling. */
static int symbol_compare(const void *left, const void *right) {
  const Symbol *left_symbol = left;
  const Symbol *right_symbol = right;

  return strcmp(left_symbol->name, right_symbol->name);
}

/** Creates the independent BVS-owned copy of one newly defined binding. */
static void *symbol_copy(const void *value) {
  const Symbol *source = value;
  Symbol *copy;

  if (source == NULL || source->name == NULL) {
    return NULL;
  }

  /// Allocate a new symbol and copy its name and type.
  copy = calloc(1U, sizeof(*copy));
  if (copy == NULL) {
    return NULL;
  }
  copy->name = copy_name(source->name);
  if (copy->name == NULL) {
    free(copy);
    return NULL;
  }

  type_entity_init(&copy->type);
  copy->type.type = source->type.type;
  copy->initialized = source->initialized;
  copy->owner = source->owner;
  return copy;
}

/** Releases one BVS-owned symbol binding. */
static void symbol_destroy(void *value) {
  Symbol *symbol = value;

  if (symbol == NULL) {
    return;
  }
  type_entity_destroy(&symbol->type);
  free(symbol->name);
  free(symbol);
}

/** Ensures room for one additional owned child scope. */
static bool reserve_children(SymbolTable *table) {
  return dynamic_array_reserve((void **)&table->children,
                               &table->child_capacity, table->child_count + 1U,
                               sizeof(*table->children)) == DYNAMIC_ARRAY_OK;
}

SymbolTable *symbol_table_create_root(void) {
  SymbolTable *table = calloc(1U, sizeof(*table));

  if (table == NULL) {
    return NULL;
  }
  table->bindings = bstCreate(symbol_compare, symbol_copy, symbol_destroy);
  if (table->bindings == NULL) {
    free(table);
    return NULL;
  }
  return table;
}

SymbolTable *symbol_table_create_child(SymbolTable *parent) {
  SymbolTable *child;

  if (parent == NULL || !reserve_children(parent)) {
    return NULL;
  }
  child = symbol_table_create_root();
  if (child == NULL) {
    return NULL;
  }
  child->parent = parent;
  parent->children[parent->child_count] = child;
  parent->child_count += 1U;
  return child;
}

void symbol_table_destroy(SymbolTable *table) {
  size_t index;

  if (table == NULL) {
    return;
  }
  for (index = 0U; index < table->child_count; index += 1U) {
    symbol_table_destroy(table->children[index]);
  }
  free(table->children);
  bstDestroy(&table->bindings);
  free(table);
}

Symbol *symbol_table_lookup_local(const SymbolTable *table, const char *name) {
  Symbol key;

  if (table == NULL || table->bindings == NULL || name == NULL) {
    return NULL;
  }
  key.name = (char *)name;
  return bstSearch(table->bindings, &key);
}

Symbol *symbol_table_resolve(const SymbolTable *start, const char *name) {
  const SymbolTable *table = start;
  Symbol *symbol;

  while (table != NULL) {
    symbol = symbol_table_lookup_local(table, name);
    if (symbol != NULL) {
      return symbol;
    }
    table = table->parent;
  }
  return NULL;
}

SymbolTableStatus symbol_table_define(SymbolTable *table, const char *name,
                                      DataType type, bool initialized,
                                      bool constant, Symbol **symbol) {
  Symbol candidate;
  Symbol *stored;

  if (symbol != NULL) {
    /// Initialize the output pointer to null in case of failure.
    *symbol = NULL;
  }
  if (table == NULL || table->bindings == NULL || name == NULL) {
    return SYMTABLE_INVALID_ARGUMENT;
  }
  if (symbol_table_lookup_local(table, name) != NULL) {
    /// Reject duplicate names in the same scope.
    return SYMTABLE_DUPLICATE;
  }

  /// Create a temporary candidate symbol to insert into the BVS.
  candidate.name = (char *)name;
  type_entity_init(&candidate.type);
  candidate.type.type = type;
  candidate.initialized = initialized;
  candidate.constant = constant;
  candidate.owner = table;

  /// Insert the candidate symbol into the symbol table.
  bstInsert(table->bindings, &candidate);
  stored = symbol_table_lookup_local(table, name);
  if (stored == NULL) {
    return SYMTABLE_OUT_OF_MEMORY;
  }

  if (symbol != NULL) {
    *symbol = stored;
  }
  return SYMTABLE_OK;
}
