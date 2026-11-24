/**
 * @file type.c
 * @brief Semantic data type representation and helpers.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was mostly generated using an LLM agent based on
 *       specifikace.txt.
 */

#include "type.h"

#include <stdint.h>

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
