#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H
#include "codegen.h"
#include <stdio.h>
#include <string.h>

// TODO split this into .c and .h

static void print_operand(InstructionOperand operand) {
  switch (operand.type) {
  case INT_TYPE:
    // WRITE technically wants to use %d
    printf("int@%lld", operand.value.integer);
    break;
  case REAL_TYPE:
    printf("float@%a", operand.value.real);
    break;
  case STRING_TYPE:
    const char *str = operand.value.string;
    for (const unsigned char *p = (const unsigned char *)str; *p != '\0'; p++) {
      if (*p <= 32 || *p == 35 || *p == 92) {
        printf("\\%03u", *p);
      } else {
        putchar(*p);
      }
    }
    break;
  case BOOL_TYPE:
    const char *boolStr = operand.value.boolean ? "true" : "false";
    printf("bool@%s", boolStr);
    break;
  case NIL_TYPE:
    printf("nil@nil");
    break;
  case VARIABLE_TYPE:
    const VariableFrame frame = operand.value.variable.frame;
    const char *frameStr = frame == TF ? "TF" : (frame == LF ? "LF" : "GF");
    const char *name = operand.value.variable.symbol->name;
    const int suffix = operand.value.variable.symbol->assembler_suffix;
    printf("%s@%s$%i", frameStr, name, suffix);
    break;
  default:
    fprintf(stderr, "Invalid InstructionOperand type in print_operand - stupid "
                    "programer\n");
    exit(1);
    break;
  }
}

static void print_prolog() { printf(".IFJcode26\n"); }

static void print_LABEL(const char *name) { printf("LABEL %s\n", name); }

static void print_DEFVAR(AssemblerVariable variable) {
  printf("DEFVAR ");
  InstructionOperand operand = {.type = VARIABLE_TYPE,
                                .value = {.variable = variable}};
  print_operand(operand);
  printf("\n");
}

static void print_PUSHS(InstructionOperand operand) {
  printf("PUSHS ");
  print_operand(operand);
  printf("\n");
}

static void print_POPS(InstructionOperand operand) {
  printf("POPS ");
  print_operand(operand);
  printf("\n");
}

static void print_ADDS() { printf("ADDS\n"); }

static void print_SUBS() { printf("SUBS\n"); }

static void print_MULS() { printf("MULS\n"); }

static void print_DIVS() { printf("DIVS\n"); }

static void print_IDIVS() { printf("IDIVS\n"); }

static void print_LTS() { printf("LTS\n"); }

static void print_GTS() { printf("GTS\n"); }

static void print_EQS() { printf("EQS\n"); }

static void print_ANDS() { printf("ANDS\n"); }

static void print_ORS() { printf("ORS\n"); }

static void print_NOTS() { printf("NOTS\n"); }

static void print_INT2FLOATS() { printf("INT2FLOATS\n"); }

static void print_CALL(const char *label) { printf("CALL %s\n", label); }

static void print_CREATEFRAME() { printf("CREATEFRAME\n"); }

static void print_PUSHFRAME() { printf("PUSHFRAME\n"); }

static void print_POPFRAME() { printf("POPFRAME\n"); }

static void print_RETURN() { printf("RETURN\n"); }

static void print_EXIT(int code) { printf("EXIT int@%i\n", code); }

#endif /* INSTRUCTIONS_H */