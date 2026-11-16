/**
 * @file main.c
 * @brief Program entry point.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 */

#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include <stdio.h>

int main() {
  // FILE *file = fopen("prog.code", "r");
  // if (file == NULL) {
  //   fprintf(stderr, "file open error");
  //   return 1;
  // }

  Lexer lexer;
  if (lexer_init(&lexer, stdin) != LEXER_OK) {
    fprintf(stderr, "lexer init error\n");
    return 1;
  }

  Program program;
  ParserStatus err = program_init(&program);
  if (err != PARSER_OK) {
    fprintf(stderr, "program init error %i\n", err);
    return 1;
  }

  err = p_program(&lexer, &program);
  if (err != PARSER_OK) {
    fprintf(stderr, "program parse error %i\n", err);
    return 1;
  }

  SemanticStatus semantic_status = semantic_check(&program);

  if (semantic_status != SEMANTIC_OK) {
    fprintf(stderr, "semantic error %i\n", semantic_status);
    return 1;
  }

  convert_AST_to_assembler(&program);
  lexer_destroy(&lexer);
  program_destroy(&program);
  return 0;
}
