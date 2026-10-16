#include "source_preprocess.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char whitespace_chars[] = {' ', '\t', '\n', '\r', '\v', '\f'};

/* Check whether c is one of the source language whitespace characters. */
bool is_whitespace(char c) {
  size_t index;

  /* Compare c with each accepted whitespace character. */
  for (index = 0;
       index < sizeof(whitespace_chars) / sizeof(whitespace_chars[0]);
       index++) {
    /* Return true when c matches the current whitespace character. */
    if (c == whitespace_chars[index]) {
      return true;
    }
  }
  return false;
}

/* AI-generated code
 * prompt:
 """
in this file, create a function with the signature int source_preprocess(char
*source_buffer)

that takes a null-terminated string and changes it in ways described later.
Saving the modified string in the same buffer. If the string that should be
saved exceeds the length of the original string, print an error statement to
stderr and return 99 before modifying the input buffer.
It returns 0 on success, 1 on malformed input, 2 on missing prolog and 99 on
other errors.

Include brief comments in the code, describing at least each function, if
statement, loop. Add helper functions if needed.

function body:
1) find all block comments, starting with /\* and ending with *\/, assert that
any nested block comments inside start and end inside (there must be the same
amount of /\* as *\/ inside a block comment) check if it contains \n, if it does
assert, that there were only whitespace chars on the same line as the end of the
comment and replace the whole block comment with \n if the block comment didn't
include \n, replace it with 1 space and dont check following chars.

2) find all line comments, starting with // and ending with \n or end of string.
Replace a newline-terminated comment with \n and remove an EOF-terminated
comment without adding a character.

3) in the string that remains, find the first occurence of prolog "import
ifj26.*", there can be one or more whitespace characters between its words.
Assert, that there are only whitespace characters before it from the start of
the file. Assert that there is a newline after the prolog.
Assert that there are only whitespace characters between the prolog and the
newline. Remove the prolog and all whitespace characters before the newline.
Leave the newline and all characters after it in place. if the prolog doesnt
exist return 2

comment delimiters must be recognized only outside string literals. Ordinary
strings start and end with a double quote; a quote preceded by a backslash is
part of the string. Multiline strings start and end with three double quotes
and may contain newlines. An unterminated string is malformed input.

use the predefined function is_whitespace to check for whitespace characters.
"""
agent: gpt-5.6-luna
*/

/* Copy a quoted string and return false when it is unterminated. */
static bool copy_string(const char *source, size_t source_length, size_t *index,
                        char *output, size_t *output_length) {
  size_t start = *index;
  bool multiline = start + 2 < source_length && source[start + 1] == '"' &&
                   source[start + 2] == '"';
  size_t delimiter_length = multiline ? 3 : 1;
  size_t position = start + delimiter_length;

  /* Scan the string until its closing delimiter is found. */
  while (position < source_length) {
    /* A triple quote closes a multiline string. */
    if (multiline && position + 2 < source_length && source[position] == '"' &&
        source[position + 1] == '"' && source[position + 2] == '"') {
      position += 3;
      memcpy(output + *output_length, source + start, position - start);
      *output_length += position - start;
      *index = position;
      return true;
    }
    /* A non-escaped quote closes an ordinary string. */
    if (!multiline && source[position] == '"') {
      size_t backslashes = 0;
      size_t before = position;

      /* Count adjacent backslashes to determine whether the quote is escaped.
       */
      while (before > start && source[before - 1] == '\\') {
        backslashes++;
        before--;
      }
      /* Close the string only when the quote is not escaped. */
      if (backslashes % 2 == 0) {
        position++;
        memcpy(output + *output_length, source + start, position - start);
        *output_length += position - start;
        *index = position;
        return true;
      }
    }
    position++;
  }
  return false;
}

/* Copy a block comment replacement while validating nested delimiters. */
static bool copy_block_comment(const char *source, size_t source_length,
                               size_t *index, char *output,
                               size_t *output_length) {
  size_t position = *index + 2;
  size_t depth = 1;
  bool has_newline = false;

  /* Scan the complete nested block comment. */
  while (position < source_length && depth > 0) {
    /* Count a nested block-comment opener. */
    if (position + 1 < source_length && source[position] == '/' &&
        source[position + 1] == '*') {
      depth++;
      position += 2;
    } else if (position + 1 < source_length && source[position] == '*' &&
               source[position + 1] == '/') {
      /* Match one block-comment closing delimiter. */
      depth--;
      position += 2;
    } else {
      /* Remember whether the comment contains a newline. */
      if (source[position] == '\n') {
        has_newline = true;
      }
      position++;
    }
  }
  /* Reject an unclosed or incorrectly nested block comment. */
  if (depth != 0) {
    return false;
  }
  /* A multiline comment must end its logical line with whitespace. */
  if (has_newline) {
    size_t trailing = position;

    /* Inspect characters after the comment up to its newline. */
    while (source[trailing] != '\0' && source[trailing] != '\n') {
      /* Reject any non-whitespace character after the comment. */
      if (!is_whitespace(source[trailing])) {
        return false;
      }
      trailing++;
    }
  }
  /* Preserve a newline for multiline comments and a space otherwise. */
  output[*output_length] = has_newline ? '\n' : ' ';
  (*output_length)++;
  *index = position;
  return true;
}

/* Strip comments while copying strings unchanged and validating literals. */
static bool strip_comments(const char *source, size_t source_length,
                           char *output, size_t *output_length) {
  size_t position = 0;

  /* Process every source character in order. */
  while (position < source_length) {
    /* Copy strings without interpreting comment markers inside them. */
    if (source[position] == '"') {
      if (!copy_string(source, source_length, &position, output,
                       output_length)) {
        return false;
      }
    } else if (position + 1 < source_length && source[position] == '/' &&
               source[position + 1] == '*') {
      /* Replace a block comment with its required separator. */
      if (!copy_block_comment(source, source_length, &position, output,
                              output_length)) {
        return false;
      }
    } else if (position + 1 < source_length && source[position] == '/' &&
               source[position + 1] == '/') {
      /* Skip a line comment until its newline or the end of the source. */
      while (position < source_length && source[position] != '\n') {
        position++;
      }
      /* Preserve a newline-terminated line comment's newline. */
      if (source[position] == '\n') {
        output[(*output_length)++] = '\n';
        position++;
      }
    } else {
      /* Copy an ordinary source character unchanged. */
      output[(*output_length)++] = source[position++];
    }
  }
  output[*output_length] = '\0';
  return true;
}

/* Remove the leading import prolog and validate its logical line. */
static int remove_prolog(char *source, size_t *length) {
  static const char first_word[] = "import";
  static const char second_word[] = "ifj26.*";
  size_t position = 0;
  size_t prolog_start;
  size_t word_index;

  /* Skip whitespace before the prolog. */
  while (is_whitespace(source[position])) {
    position++;
  }
  prolog_start = position;
  /* Reject a source whose first token is not the prolog. */
  if (strncmp(source + position, first_word, sizeof(first_word) - 1) != 0) {
    return 2;
  }
  position += sizeof(first_word) - 1;
  /* Require at least one whitespace character between prolog words. */
  if (!is_whitespace(source[position])) {
    return 2;
  }
  /* Skip whitespace between the prolog words. */
  while (is_whitespace(source[position])) {
    position++;
  }
  /* Match the case-sensitive second prolog word. */
  for (word_index = 0; second_word[word_index] != '\0'; word_index++) {
    /* Reject a mismatch in the second prolog word. */
    if (position + word_index >= *length ||
        source[position + word_index] != second_word[word_index]) {
      return 2;
    }
  }
  position += word_index;
  /* Skip whitespace after the prolog, but do not skip its newline. */
  while (is_whitespace(source[position]) && source[position] != '\n') {
    position++;
  }
  /* Reject non-whitespace characters before the required line ending. */
  if (source[position] != '\0' && source[position] != '\n') {
    return 1;
  }
  /* Remove the prolog and leading whitespace while retaining the newline. */
  /* Keep same-line indentation but discard whitespace from prior lines. */
  if (memchr(source, '\n', prolog_start) != NULL) {
    prolog_start = 0;
  }
  memmove(source + prolog_start, source + position, *length - position + 1);
  *length = prolog_start + *length - position;
  return 0;
}

/* Preprocess source_buffer in place without partial changes on errors. */
int source_preprocess(char *source_buffer) {
  size_t original_length;
  size_t output_length = 0;
  char *processed;
  int result;

  /* Reject a null input pointer. */
  if (source_buffer == NULL) {
    fprintf(stderr, "source_preprocess: source buffer is null\n");
    return 99;
  }
  original_length = strlen(source_buffer);
  processed = malloc(original_length + 1);
  /* Report allocation failure without modifying the input buffer. */
  if (processed == NULL) {
    fprintf(stderr, "source_preprocess: allocation failed\n");
    return 99;
  }
  /* Validate and strip comments into temporary storage. */
  if (!strip_comments(source_buffer, original_length, processed,
                      &output_length)) {
    free(processed);
    return 1;
  }
  /* Validate and remove the prolog from the temporary result. */
  result = remove_prolog(processed, &output_length);
  if (result != 0) {
    free(processed);
    return result;
  }
  /* Guard the final copy against an unexpected transformation expansion. */
  if (output_length > original_length) {
    fprintf(stderr, "source_preprocess: processed string is too long\n");
    free(processed);
    return 99;
  }
  /* Commit the result only after every validation step succeeds. */
  memcpy(source_buffer, processed, output_length + 1);
  free(processed);
  return 0;
}
