#include "function_table.h"

#include "ast.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** Copies one source-level name into table-owned storage. */
static char *copy_name(const char *name) {
  size_t length = strlen(name);
  char *copy = malloc(length + 1U);

  if (copy != NULL) {
    memcpy(copy, name, length + 1U);
  }
  return copy;
}

/** Ensures room for one additional owned function pointer. */
static bool reserve_functions(FunctionTable *table) {
  FunctionSymbol **resized;
  size_t capacity;

  if (table->function_count < table->function_capacity) {
    return true;
  }
  capacity =
      table->function_capacity == 0U ? 4U : table->function_capacity * 2U;
  if (capacity < table->function_capacity ||
      capacity > SIZE_MAX / sizeof(*table->functions)) {
    return false;
  }
  resized = realloc(table->functions, capacity * sizeof(*table->functions));
  if (resized == NULL) {
    return false;
  }
  table->functions = resized;
  table->function_capacity = capacity;
  return true;
}

FunctionTableStatus function_table_init(FunctionTable *table) {
  if (table == NULL) {
    return FUNCTION_TABLE_INVALID_ARGUMENT;
  }

  table->functions = NULL;
  table->function_count = 0U;
  table->function_capacity = 0U;
  return FUNCTION_TABLE_OK;
}

void function_table_destroy(FunctionTable *table) {
  size_t index;

  if (table == NULL) {
    return;
  }
  for (index = 0U; index < table->function_count; index += 1U) {
    ast_node_destroy(table->functions[index]->ast_root);
    type_entity_destroy(&table->functions[index]->return_type);
    symbol_table_destroy(table->functions[index]->local_scope);
    free(table->functions[index]->name);
    free(table->functions[index]);
  }
  free(table->functions);
  table->functions = NULL;
  table->function_count = 0U;
  table->function_capacity = 0U;
}

FunctionSymbol *function_table_lookup(const FunctionTable *table,
                                      const char *name) {
  size_t index;

  if (table == NULL || name == NULL) {
    return NULL;
  }
  for (index = 0U; index < table->function_count; index += 1U) {
    if (strcmp(table->functions[index]->name, name) == 0) {
      return table->functions[index];
    }
  }
  return NULL;
}

FunctionTableStatus function_table_get_or_create(FunctionTable *table,
                                                 const char *name,
                                                 FunctionSymbol **function) {
  FunctionSymbol *created;

  if (function != NULL) {
    *function = NULL;
  }
  if (table == NULL || name == NULL || function == NULL) {
    return FUNCTION_TABLE_INVALID_ARGUMENT;
  }

  /// Lookup first, then create if not found.
  created = function_table_lookup(table, name);
  if (created != NULL) {
    *function = created;
    return FUNCTION_TABLE_OK;
  }

  /// Check for capacity and allocate a new function symbol.
  if (!reserve_functions(table)) {
    return FUNCTION_TABLE_OUT_OF_MEMORY;
  }

  created = calloc(1U, sizeof(*created));
  if (created == NULL) {
    return FUNCTION_TABLE_OUT_OF_MEMORY;
  }

  /// Initialize the new function symbol and add it to the table.
  created->name = copy_name(name);
  if (created->name == NULL) {
    free(created);
    return FUNCTION_TABLE_OUT_OF_MEMORY;
  }
  type_entity_init(&created->return_type);
  table->functions[table->function_count] = created;
  table->function_count += 1U;
  *function = created;
  return FUNCTION_TABLE_OK;
}

SymbolTable *function_symbol_create_local_scope(FunctionSymbol *function) {
  if (function == NULL) {
    return NULL;
  }
  if (function->local_scope == NULL) {
    function->local_scope = symbol_table_create_root();
  }
  return function->local_scope;
}

FunctionTableStatus function_symbol_set_ast_root(FunctionSymbol *function,
                                                 ASTNode *ast_root) {
  if (function == NULL) {
    return FUNCTION_TABLE_INVALID_ARGUMENT;
  }
  if (function->ast_root == ast_root) {
    return FUNCTION_TABLE_OK;
  }

  ast_node_destroy(function->ast_root);
  function->ast_root = ast_root;
  return FUNCTION_TABLE_OK;
}
