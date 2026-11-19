/**
 * @file main.c
 * @brief Program entry point.
 * @author Simon Hrouda
 * @author Milan Novak
 */

#include "codegen.h"
#include "dynamic_array.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "source_preprocess.h"

#include <stdio.h>
#include <stdlib.h>

typedef enum { EXIT_OK = 0, EXIT_INTERNAL_ERROR = 99 } ExitCode;

char *read_source_code(void) {
  char *source_code = NULL;
  size_t source_code_capacity = 0;
  size_t source_code_size = 0;

  int input_char;
  while ((input_char = getchar()) != EOF) {
    DynamicArrayStatus status =
        dynamic_array_reserve((void **)&source_code, &source_code_capacity,
                              ++source_code_size, sizeof(char));
    if (status != DYNAMIC_ARRAY_OK) {
      fprintf(stderr, "dynamic array reserve error %i\n", status);
      free(source_code);
      return NULL;
    }
    source_code[source_code_size - 1] = (char)input_char;
  }

  DynamicArrayStatus status =
      dynamic_array_reserve((void **)&source_code, &source_code_capacity,
                            source_code_size + 1, sizeof(char));
  if (status != DYNAMIC_ARRAY_OK) {
    fprintf(stderr, "dynamic array reserve error %i\n", status);
    free(source_code);
    return NULL;
  }
  source_code[source_code_size] = '\0';

  return source_code;
}

int main(void) {
  char *source_code = read_source_code();
  if (source_code == NULL) {
    fprintf(stderr, "error reading source code\n");
    return EXIT_INTERNAL_ERROR;
  }

  if (source_preprocess(source_code) != PREPROCESS_OK) {
    free(source_code);
    fprintf(stderr, "error preprocessing source code\n");
    return EXIT_INTERNAL_ERROR;
  }

  Lexer lexer;
  if (lexer_init(&lexer, source_code) != LEXER_OK) {
    free(source_code);
    fprintf(stderr, "lexer init error\n");
    return EXIT_INTERNAL_ERROR;
  }

  Program program;
  ParserStatus parser_status = program_init(&program);
  if (parser_status != PARSER_OK) {
    free(source_code);
    lexer_destroy(&lexer);
    fprintf(stderr, "program init error %i\n", parser_status);
    return parser_status_exit_code(parser_status);
  }

  parser_status = p_program(&lexer, &program);
  if (parser_status != PARSER_OK) {
    free(source_code);
    lexer_destroy(&lexer);
    program_destroy(&program);
    fprintf(stderr, "program parse error %i\n", parser_status);
    return parser_status_exit_code(parser_status);
  }

  SemanticStatus semantic_status = semantic_check(&program);

  if (semantic_status != SEMANTIC_OK) {
    free(source_code);
    lexer_destroy(&lexer);
    program_destroy(&program);
    fprintf(stderr, "semantic error %i\n", semantic_status);
    return semantic_status;
  }

  free(source_code);
  convert_AST_to_assembler(&program);
  lexer_destroy(&lexer);
  program_destroy(&program);
  return EXIT_OK;
}
