#include <stdbool.h>

#ifndef SOURCE_PREPROCESS_H
#define SOURCE_PREPROCESS_H

extern const char whitespace_chars[];
bool is_whitespace(char c);
int source_preprocess(char *source_buffer);

#endif
