/**
 * @file type.h
 * @brief Semantic type declarations and dependency-resolution interface.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was mostly generated using an LLM agent based on
 *       specifikace.txt.
 */

#ifndef SRC_TYPE_H_
#define SRC_TYPE_H_

#include <stdbool.h>
#include <stddef.h>

/** Represents a value type known to semantic analysis. */
typedef enum {
  TYPE_UNKNOWN,
  TYPE_INT,
  TYPE_REAL,
  TYPE_STRING,
  TYPE_UNIT,
  TYPE_BOOL
} DataType;

/** Reports the result of a type dependency or resolution operation. */
typedef enum {
  TYPE_RESOLVE_OK,
  TYPE_RESOLVE_CONFLICT,
  TYPE_RESOLVE_INVALID_ARGUMENT,
  TYPE_RESOLVE_OUT_OF_MEMORY
} TypeResolveStatus;

/**
 * @brief Embeddable source of a type and its waiting dependents.
 *
 * @details This structure owns only its dynamic @c dependents array. It never
 * owns the pointed-to entities, which lets symbols and functions form one
 * dependency graph without shared ownership.
 */
typedef struct TypeEntity {
  DataType type;
  struct TypeEntity **dependents;
  size_t dependent_count;
  size_t dependent_capacity;
  bool resolving;
} TypeEntity;

/** Initializes an unresolved type entity. */
void type_entity_init(TypeEntity *entity);

/** Releases the dependent pointer array without destroying dependent entities.
 */
void type_entity_destroy(TypeEntity *entity);

/**
 * @brief Records that @p dependent needs the type of @p source.
 * @details If source already has a concrete type, it is propagated immediately.
 */
TypeResolveStatus type_entity_depend(TypeEntity *source, TypeEntity *dependent);

/**
 * @brief Assigns a concrete type and propagates it to all transitive
 * dependents.
 * @return TYPE_RESOLVE_CONFLICT if a reachable entity has another concrete
 * type.
 */
TypeResolveStatus type_entity_resolve(TypeEntity *entity, DataType type);

#endif // SRC_TYPE_H_
