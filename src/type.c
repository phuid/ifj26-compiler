/**
 * @file type.c
 * @brief Semantic type names and dependency resolution.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was mostly generated using an LLM agent based on
 *       specifikace.txt.
 */

#include "type.h"

#include "dynamic_array.h"

#include <stdlib.h>

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
