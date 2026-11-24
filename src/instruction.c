/**
 * @file instruction.c
 * @brief Prints target-code instructions and operands.
 * @author Milan Novák
 */

#include "codegen.h"
#include "error_trace.h"
#include "instructions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Prints one instruction operand in IFJCode26 syntax.
 * @param operand Operand to format.
 */
static void _print_operand(const InstructionOperand operand) {
  switch (operand.type) {
  case INT_TYPE:
    printf("int@%lld", operand.value.integer);
    break;
  case REAL_TYPE:
    printf("float@%a", operand.value.real);
    break;
  case STRING_TYPE:
    printf("string@");
    const char *str = operand.value.string;
    for (size_t i = 0; i < strlen(str); i++) {
      unsigned char c = (unsigned char)str[i];
      if (c <= 32 || c == 35 || c == 92) {
        printf("\\%03u", c);
      } else {
        putchar(c);
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
  case REAL_VARIABLE_TYPE: {
    const VariableFrame frame = operand.value.real_variable.frame;
    const char *frameStr = frame == TF ? "TF" : (frame == LF ? "LF" : "GF");
    const char *name = operand.value.real_variable.symbol->name;
    const int suffix = operand.value.real_variable.symbol->assembler_suffix;
    printf("%s@%s$%i", frameStr, name, suffix);
  } break;
  case COMPILER_VAR_TYPE: {
    const VariableFrame frame = operand.value.compiler_variable.frame;
    const char *frameStr = frame == TF ? "TF" : (frame == LF ? "LF" : "GF");
    const char *name = operand.value.compiler_variable.name;
    const int suffix = operand.value.compiler_variable.suffix;
    printf("%s@%s$$%i", frameStr, name, suffix);
  } break;
  default:
    ERROR_INTERNAL("invalid instruction operand type %d", (int)operand.type);
    ERROR_TRACE("while generating code");
    exit(99);
  }
}

/**
 * @brief Prints a space-separated operand list followed by a newline.
 * @param operands Array of operands.
 * @param count Number of operands in the array.
 */
static void _print_operands(const InstructionOperand *operands, size_t count) {
  for (size_t i = 0; i < count; i++) {
    printf(" ");
    _print_operand(operands[i]);
  }
  printf("\n");
}

// Macro that formats the arguments into an array, calculates the number of
// arguments
#define print_operands(...)                                                    \
  do {                                                                         \
    const InstructionOperand _ops[] = {__VA_ARGS__};                           \
    _print_operands(_ops, sizeof(_ops) / sizeof(_ops[0]));                     \
  } while (0)

void print_prolog() { printf(".IFJcode26\n"); }

void print_LABEL(const char *name, const int suffix) {
  printf("LABEL %s$%i\n", name, suffix);
}

void print_DEFVAR(const InstructionOperand operand) {
  printf("DEFVAR");
  print_operands(operand);
}

void print_PUSHS(const InstructionOperand operand) {
  printf("PUSHS");
  print_operands(operand);
}

void print_POPS(const InstructionOperand operand) {
  printf("POPS");
  print_operands(operand);
}

void print_ADDS(void) { printf("ADDS\n"); }

void print_SUBS(void) { printf("SUBS\n"); }

void print_MULS(void) { printf("MULS\n"); }

void print_DIVS(void) { printf("DIVS\n"); }

void print_IDIVS(void) { printf("IDIVS\n"); }

void print_LTS(void) { printf("LTS\n"); }

void print_GTS(void) { printf("GTS\n"); }

void print_EQS(void) { printf("EQS\n"); }

void print_ANDS(void) { printf("ANDS\n"); }

void print_ORS(void) { printf("ORS\n"); }

void print_NOTS(void) { printf("NOTS\n"); }

void print_INT2FLOATS(void) { printf("INT2FLOATS\n"); }

void print_CALL(const char *label) { printf("CALL %s$0\n", label); }

void print_CREATEFRAME(void) { printf("CREATEFRAME\n"); }

void print_PUSHFRAME(void) { printf("PUSHFRAME\n"); }

void print_POPFRAME(void) { printf("POPFRAME\n"); }

void print_RETURN(void) { printf("RETURN\n"); }

void print_EXIT(const int code) { printf("EXIT int@%i\n", code); }

void print_JUMP(const char *label, const int suffix) {
  printf("JUMP %s$%i\n", label, suffix);
}

void print_JUMPIFEQS(const char *label, const int suffix) {
  printf("JUMPIFEQS %s$%i\n", label, suffix);
}

void print_JUMPIFNEQS(const char *label, const int suffix) {
  printf("JUMPIFNEQS %s$%i\n", label, suffix);
}

void print_JUMPIFEQS_bool(const char *label, const int suffix,
                          const bool value) {
  InstructionOperand valueOp = {
      .type = BOOL_TYPE,
      .value.boolean = value,
  };
  print_PUSHS(valueOp);
  print_JUMPIFEQS(label, suffix);
}

void print_NEWARRAY(const InstructionOperand array,
                    const InstructionOperand size) {
  printf("NEWARRAY");

  print_operands(array, size);
}

void print_ARRAYGET(const InstructionOperand target_var,
                    const InstructionOperand array,
                    const InstructionOperand index) {
  printf("ARRAYGET");

  print_operands(target_var, array, index);
}

void print_ARRAYSET(const InstructionOperand array,
                    const InstructionOperand index,
                    const InstructionOperand value) {
  printf("ARRAYSET");

  print_operands(array, index, value);
}

void print_ADD(const InstructionOperand target, const InstructionOperand left,
               const InstructionOperand right) {
  printf("ADD");

  print_operands(target, left, right);
}

void print_SUB(const InstructionOperand target, const InstructionOperand left,
               const InstructionOperand right) {
  printf("SUB");

  print_operands(target, left, right);
}

void print_MOVE(const InstructionOperand target,
                const InstructionOperand source) {
  printf("MOVE");

  print_operands(target, source);
}

void print_JUMPIFEQ(const char *label, const int suffix,
                    const InstructionOperand left,
                    const InstructionOperand right) {
  printf("JUMPIFEQ %s$%i ", label, suffix);

  print_operands(left, right);
}
void print_JUMPIFNEQ(const char *label, const int suffix,
                     const InstructionOperand left,
                     const InstructionOperand right) {
  printf("JUMPIFNEQ %s$%i ", label, suffix);

  print_operands(left, right);
}

void print_ISINTS(void) { printf("ISINTS\n"); }

void print_LT(const InstructionOperand target, const InstructionOperand left,
              const InstructionOperand right) {
  printf("LT");

  print_operands(target, left, right);
}

void print_GT(const InstructionOperand target, const InstructionOperand left,
              const InstructionOperand right) {
  printf("GT");

  print_operands(target, left, right);
}

void print_EQ(const InstructionOperand target, const InstructionOperand left,
              const InstructionOperand right) {
  printf("EQ");

  print_operands(target, left, right);
}

void print_IDIV(const InstructionOperand target, const InstructionOperand left,
                const InstructionOperand right) {
  printf("IDIV");

  print_operands(target, left, right);
}

void print_CONCAT(const InstructionOperand target,
                  const InstructionOperand left,
                  const InstructionOperand right) {
  printf("CONCAT");

  print_operands(target, left, right);
}