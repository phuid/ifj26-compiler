/**
 * @file source_preprocess.h
 * @brief Public interface for IFJ26 source preprocessing.
 * @author Simon Hrouda
 * @note Doxygen comments in this file were generated using an LLM agent:
 * gpt-5.6-luna
 */

#include <stdbool.h>

#ifndef SOURCE_PREPROCESS_H
#define SOURCE_PREPROCESS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PREPROCESS_OK = 0,        ///< Preprocessing succeeded.
  PREPROCESS_MALFORMED = 1, ///< Malformed input (e.g., unclosed comment).
  PREPROCESS_MISSING_PROLOG =
      2, ///< Required import prolog is missing or invalid.
  PREPROCESS_INTERNAL_ERROR = 99 ///< Internal error (e.g., null buffer).
} PreprocessStatus;

/**
 * @brief Removes comments and the required import prolog in place.
 *
 * Comment delimiters inside string literals are preserved. Block comments
 * are replaced with a space or with their original number of newlines so
 * that subsequent source locations remain meaningful. The input buffer is
 * modified only after preprocessing succeeds.
 *
 * @param source_buffer Null-terminated, writable source buffer.
 * @return @c 0 on success, @c 1 for malformed input, @c 2 when the required
 * prolog is missing or invalid, or @c 99 for a null buffer or another
 * internal error.
 */
PreprocessStatus source_preprocess(char *source_buffer);

#ifdef __cplusplus
}
#endif

#endif
