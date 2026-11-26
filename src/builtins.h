/**
 * @file builtins.h
 * Project: IFJ26 compiler
 * @brief Public interface exposing the builtin function definitions.
 * @author Milan Novák (xnovakm00)
 */

#ifndef BUILTINS_H
#define BUILTINS_H
#include "type.h"

typedef struct {
  const char *name;       ///< Name as written in the source code.
  BaseType return_type;   ///< Type of the value the builtin returns.
  size_t parameter_count; ///< Number of used items in @c parameters.
  /// Parameter types in source order; TYPE_ANY accepts any value.
  BaseType parameters[3];
  const char *assembler_code;
} BuiltinSignature;

extern const BuiltinSignature BUILTINS[];
extern const size_t BUILTIN_COUNT;
#endif /* BUILTINS_H */