#include "type.h"

#include "dynamic_array.h"

#include <stdlib.h>
#include <string.h>

/** Binds one canonical source spelling to one semantic data type. */
typedef struct {
  const char *identifier;
  DataType type;
} TypeName;

static const TypeName type_names[] = {
    {"Int", TYPE_INT},   {"Double", TYPE_REAL},  {"String", TYPE_STRING},
    {"Unit", TYPE_UNIT}, {"Boolean", TYPE_BOOL},
};

/** Lists reserved words that are not represented by a DataType. */
static const char *const reserved_words[] = {
    "def",       "do",  "else", "if",    "import", "return",
    "then",      "val", "var",  "while", "case",   "class",
    "extension", "for", "List", "match", "type",
};

/** Converts an identifier to its corresponding data type.
 * @param identifier The identifier to convert.
 * @return The data type corresponding to the identifier, or TYPE_UNKNOWN if
 * it's not a valid type.
 * @todo Implement user-defined types and dynamic type table
 */
DataType type_from_identifier(const char *identifier) {
  size_t index;

  if (identifier == NULL) {
    return TYPE_UNKNOWN;
  }
  for (index = 0U; index < sizeof(type_names) / sizeof(*type_names);
       index += 1U) {
    if (strcmp(identifier, type_names[index].identifier) == 0) {
      return type_names[index].type;
    }
  }
  return TYPE_UNKNOWN;
}

bool identifier_is_valid(const char *identifier) {
  size_t index;

  if (identifier == NULL || type_from_identifier(identifier) != TYPE_UNKNOWN) {
    return false;
  }
  for (index = 0U; index < sizeof(reserved_words) / sizeof(*reserved_words);
       index += 1U) {
    if (strcmp(identifier, reserved_words[index]) == 0) {
      return false;
    }
  }
  return true;
}

/** Appends a non-owning dependent edge to an unresolved type entity. */
static TypeResolveStatus type_entity_append_dependent(TypeEntity *source,
                                                      TypeEntity *dependent) {
  if (dynamic_array_reserve((void **)&source->dependents,
                            &source->dependent_capacity,
                            source->dependent_count + 1U,
                            sizeof(*source->dependents)) != DYNAMIC_ARRAY_OK) {
    return TYPE_RESOLVE_OUT_OF_MEMORY;
  }

  source->dependents[source->dependent_count] = dependent;
  source->dependent_count += 1U;
  return TYPE_RESOLVE_OK;
}

/** Recursively assigns one concrete type while breaking dependency cycles. */
static TypeResolveStatus type_entity_resolve_recursive(TypeEntity *entity,
                                                       DataType type) {
  size_t index;
  TypeResolveStatus status;

  if (entity->type != TYPE_UNKNOWN && entity->type != type) {
    return TYPE_RESOLVE_CONFLICT;
  }

  /// Avoid infinite recursion on circular dependencies by marking the entity as
  /// resolving.
  if (entity->resolving) {
    return TYPE_RESOLVE_OK;
  }

  entity->type = type;
  entity->resolving = true;

  /// Propagate the concrete type to all dependents.
  for (index = 0U; index < entity->dependent_count; index += 1U) {

    status = type_entity_resolve_recursive(entity->dependents[index], type);
    if (status != TYPE_RESOLVE_OK) {
      entity->resolving = false;
      return status;
    }
  }
  entity->resolving = false;
  return TYPE_RESOLVE_OK;
}

void type_entity_init(TypeEntity *entity) {
  if (entity == NULL) {
    return;
  }

  entity->type = TYPE_UNKNOWN;
  entity->dependents = NULL;
  entity->dependent_count = 0U;
  entity->dependent_capacity = 0U;
  entity->resolving = false;
}

void type_entity_destroy(TypeEntity *entity) {
  if (entity == NULL) {
    return;
  }

  free(entity->dependents);
  entity->dependents = NULL;
  entity->dependent_count = 0U;
  entity->dependent_capacity = 0U;
  entity->type = TYPE_UNKNOWN;
  entity->resolving = false;
}

TypeResolveStatus type_entity_depend(TypeEntity *source,
                                     TypeEntity *dependent) {
  TypeResolveStatus status;

  if (source == NULL || dependent == NULL) {
    return TYPE_RESOLVE_INVALID_ARGUMENT;
  }
  if (source->type != TYPE_UNKNOWN) {
    return type_entity_resolve(dependent, source->type);
  }

  status = type_entity_append_dependent(source, dependent);
  return status;
}

TypeResolveStatus type_entity_resolve(TypeEntity *entity, DataType type) {
  if (entity == NULL || type == TYPE_UNKNOWN) {
    return TYPE_RESOLVE_INVALID_ARGUMENT;
  }

  return type_entity_resolve_recursive(entity, type);
}
