/**
 * @file dynamic_array.c
 * Project: IFJ26 compiler
 * @brief Generic dynamic-array capacity management.
 * @author Vojtěch Ondráček (xondrav00)
 * @author Šimon Hrouda (xhrouds00)
 * @note Code in this file was mostly generated using an LLM agent based on
 *       specifikace.txt.
 */

#include "dynamic_array.h"

#include <stdint.h>
#include <stdlib.h>

DynamicArrayStatus dynamic_array_reserve(void **data, size_t *capacity,
                                         size_t required, size_t element_size) {
  size_t new_capacity;
  void *resized;

  if (data == NULL || capacity == NULL || element_size == 0U ||
      (*capacity != 0U && *data == NULL)) {
    return DYNAMIC_ARRAY_INVALID_ARGUMENT;
  }
  if (required <= *capacity) {
    return DYNAMIC_ARRAY_OK;
  }

  new_capacity = *capacity == 0U ? 4U : *capacity;
  while (new_capacity < required) {
    if (new_capacity > SIZE_MAX / 2U) {
      new_capacity = required;
      break;
    }
    new_capacity *= 2U;
  }
  if (new_capacity > SIZE_MAX / element_size) {
    return DYNAMIC_ARRAY_OUT_OF_MEMORY;
  }

  resized = realloc(*data, new_capacity * element_size);
  if (resized == NULL) {
    return DYNAMIC_ARRAY_OUT_OF_MEMORY;
  }
  *data = resized;
  *capacity = new_capacity;
  return DYNAMIC_ARRAY_OK;
}
