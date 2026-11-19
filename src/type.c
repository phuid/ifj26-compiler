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

#include <stdint.h>
#include <stdlib.h>

DataType type_unknown(void) {
  DataType type = {TYPE_UNKNOWN, 0U};
  return type;
}

DataType type_scalar(BaseType base) {
  DataType type = {base, 0U};
  return type;
}

DataType type_list(DataType element_type) {
  if (element_type.base == TYPE_UNKNOWN ||
      element_type.list_depth == SIZE_MAX) {
    return type_unknown();
  }

  element_type.list_depth += 1U;
  return element_type;
}

bool type_is_unknown(DataType type) {
  return type.base == TYPE_UNKNOWN && type.list_depth == 0U;
}

bool type_is_list(DataType type) {
  return type.base != TYPE_UNKNOWN && type.list_depth > 0U;
}

bool type_is_scalar(DataType type, BaseType base) {
  return type.base != TYPE_UNKNOWN && type.base == base &&
         type.list_depth == 0U;
}

bool type_equal(DataType left, DataType right) {
  return left.base == right.base && left.list_depth == right.list_depth;
}

bool type_element_type(DataType list, DataType *element_type) {
  if (element_type == NULL || !type_is_list(list)) {
    return false;
  }

  list.list_depth -= 1U;
  *element_type = list;
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

  if (!type_is_unknown(entity->type) && !type_equal(entity->type, type)) {
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

  entity->type = type_unknown();
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
  entity->type = type_unknown();
  entity->resolving = false;
}

TypeResolveStatus type_entity_depend(TypeEntity *source,
                                     TypeEntity *dependent) {
  TypeResolveStatus status;

  if (source == NULL || dependent == NULL) {
    return TYPE_RESOLVE_INVALID_ARGUMENT;
  }
  if (!type_is_unknown(source->type)) {
    return type_entity_resolve(dependent, source->type);
  }

  status = type_entity_append_dependent(source, dependent);
  return status;
}

TypeResolveStatus type_entity_resolve(TypeEntity *entity, DataType type) {
  if (entity == NULL || type.base == TYPE_UNKNOWN) {
    return TYPE_RESOLVE_INVALID_ARGUMENT;
  }

  return type_entity_resolve_recursive(entity, type);
}
