#include "type.h"

#include <stdint.h>
#include <stdlib.h>

/** Appends a non-owning dependent edge to an unresolved type entity. */
static TypeResolveStatus type_entity_append_dependent(TypeEntity *source,
                                                      TypeEntity *dependent) {
  TypeEntity **resized;
  size_t new_capacity;

  if (source->dependent_count == source->dependent_capacity) {
    new_capacity =
        source->dependent_capacity == 0U ? 4U : source->dependent_capacity * 2U;
    if (new_capacity < source->dependent_capacity ||
        new_capacity > SIZE_MAX / sizeof(*source->dependents)) {
      return TYPE_RESOLVE_OUT_OF_MEMORY;
    }
    resized =
        realloc(source->dependents, new_capacity * sizeof(*source->dependents));
    if (resized == NULL) {
      return TYPE_RESOLVE_OUT_OF_MEMORY;
    }
    source->dependents = resized;
    source->dependent_capacity = new_capacity;
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
