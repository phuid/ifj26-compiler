/**
 * @file dynamic_array.h
 * Project: IFJ26 compiler
 * @brief Generic dynamic-array capacity management declarations.
 * @author Vojtěch Ondráček (xondrav00)
 * @author Šimon Hrouda (xhrouds00)
 * @note Code in this file was mostly generated using an LLM agent based on
 *       specifikace.txt.
 */

#ifndef SRC_DYNAMIC_ARRAY_H_
#define SRC_DYNAMIC_ARRAY_H_

#include <stddef.h>

/** Reports the outcome of generic dynamically allocated array growth. */
typedef enum {
  DYNAMIC_ARRAY_OK,
  DYNAMIC_ARRAY_INVALID_ARGUMENT,
  DYNAMIC_ARRAY_OUT_OF_MEMORY
} DynamicArrayStatus;

/**
 * @brief Ensures capacity for at least @p required elements.
 * @details The caller retains ownership of the allocation and its elements.
 * Existing elements are preserved. Capacity grows geometrically from four
 * elements and arithmetic overflow is reported instead of wrapping.
 * @param data Address of the caller-owned typed array, cast to @c void **.
 * @param capacity Address of the allocation capacity in elements.
 * @param required Minimum element count required by the caller.
 * @param element_size Size of one element in bytes.
 */
DynamicArrayStatus dynamic_array_reserve(void **data, size_t *capacity,
                                         size_t required, size_t element_size);

#endif // SRC_DYNAMIC_ARRAY_H_
