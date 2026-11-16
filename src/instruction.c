#include "codegen.h"
#include "instructions.h"
#include <stdio.h>

void print_operand(InstructionOperand operand) {
  switch (operand.type) {
  case INT_TYPE:
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
  case REAL_VARIABLE_TYPE:
    const VariableFrame frame = operand.value.real_variable.frame;
    const char *frameStr = frame == TF ? "TF" : (frame == LF ? "LF" : "GF");
    const char *name = operand.value.real_variable.symbol->name;
    const int suffix = operand.value.real_variable.symbol->assembler_suffix;
    printf("%s@%s$%i", frameStr, name, suffix);
    break;
  case COMPILER_VAR_TYPE:
    const VariableFrame frame_c = operand.value.compiler_variable.frame;
    const char *frameStr_c =
        frame_c == TF ? "TF" : (frame_c == LF ? "LF" : "GF");
    const char *name_c = operand.value.compiler_variable.name;
    printf("%s@%s", frameStr_c, name_c);
    break;
  default:
    fprintf(stderr, "Invalid InstructionOperand type in print_operand - stupid "
                    "programer\n");
    exit(1);
    break;
  }
}

void print_prolog() { printf(".IFJcode26\n"); }

void print_LABEL(const char *name) { printf("LABEL %s\n", name); }

void print_DEFVAR(InstructionOperand operand) {
  printf("DEFVAR ");
  print_operand(operand);
  printf("\n");
}

void print_PUSHS(InstructionOperand operand) {
  printf("PUSHS ");
  print_operand(operand);
  printf("\n");
}

void print_POPS(InstructionOperand operand) {
  printf("POPS ");
  print_operand(operand);
  printf("\n");
}

void print_ADDS() { printf("ADDS\n"); }

void print_SUBS() { printf("SUBS\n"); }

void print_MULS() { printf("MULS\n"); }

void print_DIVS() { printf("DIVS\n"); }

void print_IDIVS() { printf("IDIVS\n"); }

void print_LTS() { printf("LTS\n"); }

void print_GTS() { printf("GTS\n"); }

void print_EQS() { printf("EQS\n"); }

void print_ANDS() { printf("ANDS\n"); }

void print_ORS() { printf("ORS\n"); }

void print_NOTS() { printf("NOTS\n"); }

void print_INT2FLOATS() { printf("INT2FLOATS\n"); }

void print_CALL(const char *label) { printf("CALL %s\n", label); }

void print_CREATEFRAME() { printf("CREATEFRAME\n"); }

void print_PUSHFRAME() { printf("PUSHFRAME\n"); }

void print_POPFRAME() { printf("POPFRAME\n"); }

void print_RETURN() { printf("RETURN\n"); }

void print_EXIT(int code) { printf("EXIT int@%i\n", code); }
