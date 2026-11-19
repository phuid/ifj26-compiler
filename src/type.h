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

/** Represents the scalar type at the innermost list level. */
typedef enum {
  TYPE_UNKNOWN,
  TYPE_INT,
  TYPE_REAL,
  TYPE_STRING,
  TYPE_UNIT,
  TYPE_BOOL,
  /** Placeholder parameter type of builtins accepting any value (print, toStr).
   */
  TYPE_ANY,
  TYPE_ANY_LIST
} BaseType;

/**
 * @brief Represents a scalar or an arbitrarily nested list type.
 * @details A depth of zero denotes a scalar, one denotes a list of the scalar
 * base type, and each additional level adds another list wrapper. Unknown types
 * always have zero depth.
 */
typedef struct {
  BaseType base;
  size_t list_depth;
} DataType;

/** Constructs the unresolved type. */
DataType type_unknown(void);

/** Constructs a scalar type, or the unresolved type for TYPE_UNKNOWN. */
DataType type_scalar(BaseType base);

/** Adds one list wrapper, or returns unknown for an unknown type or overflow.
 */
DataType type_list(DataType element_type);

/** Returns whether @p type is the canonical unresolved type. */
bool type_is_unknown(DataType type);

/** Returns whether @p type has at least one concrete list wrapper. */
bool type_is_list(DataType type);

/** Returns whether @p type is the requested scalar type without list wrappers.
 */
bool type_is_scalar(DataType type, BaseType base);

/** Compares both the scalar base and list nesting depth. */
bool type_equal(DataType left, DataType right);

/**
 * @brief Removes one list wrapper.
 * @return True when @p list is a list and @p element_type was written.
 */
bool type_element_type(DataType list, DataType *element_type);

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
