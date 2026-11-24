/**
 * @file instructions.h
 * @brief Defines target-code operands and instruction-printing functions.
 * @author Milan Novák
 */

#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H
#include "symtable.h"
#include <stdbool.h>

typedef enum { TF, LF, GF } VariableFrame;

typedef enum {
  STRING_TYPE,
  INT_TYPE,
  REAL_TYPE,
  BOOL_TYPE,
  NIL_TYPE,
  REAL_VARIABLE_TYPE,
  COMPILER_VAR_TYPE,
} OperandType;

typedef struct {
  VariableFrame frame;
  Symbol *symbol;
} RealVariable;

typedef struct {
  VariableFrame frame;
  const char *name;
  const int suffix;
} CompilerVariable;

typedef struct {
  OperandType type;
  union {
    long long int integer;
    double real;
    bool boolean;
    char *string;
    CompilerVariable compiler_variable;
    RealVariable real_variable;
  } value;
} InstructionOperand;

/** @brief Prints the IFJCode26 prolog. */
void print_prolog(void);
/** @brief Prints a label with a numeric suffix. @param name Label name. @param
 * suffix Label suffix. */
void print_LABEL(const char *name, const int suffix);
/** @brief Prints an operand variable declaration. @param operand Variable
 * operand. */
void print_DEFVAR(const InstructionOperand operand);
/** @brief Prints a push onto the operand stack. @param operand Operand to push.
 */
void print_PUSHS(const InstructionOperand operand);
/** @brief Prints a pop from the operand stack. @param operand Destination
 * operand. */
void print_POPS(const InstructionOperand operand);

/** @brief Prints the stack arithmetic ADDS instruction. */
void print_ADDS(void);
/** @brief Prints the stack arithmetic SUBS instruction. */
void print_SUBS(void);
/** @brief Prints the stack arithmetic MULS instruction. */
void print_MULS(void);
/** @brief Prints the stack arithmetic DIVS instruction. */
void print_DIVS(void);
/** @brief Prints the stack arithmetic IDIVS instruction. */
void print_IDIVS(void);

/** @brief Prints the stack comparison LTS instruction. */
void print_LTS(void);
/** @brief Prints the stack comparison GTS instruction. */
void print_GTS(void);
/** @brief Prints the stack comparison EQS instruction. */
void print_EQS(void);

/** @brief Prints the stack logical ANDS instruction. */
void print_ANDS(void);
/** @brief Prints the stack logical ORS instruction. */
void print_ORS(void);
/** @brief Prints the stack logical NOTS instruction. */
void print_NOTS(void);

/** @brief Prints the stack INT2FLOATS conversion instruction. */
void print_INT2FLOATS(void);

/** @brief Prints a call to a generated function. @param label Function name. */
void print_CALL(const char *label);

/** @brief Prints the CREATEFRAME instruction. */
void print_CREATEFRAME(void);
/** @brief Prints the PUSHFRAME instruction. */
void print_PUSHFRAME(void);
/** @brief Prints the POPFRAME instruction. */
void print_POPFRAME(void);

/** @brief Prints the RETURN instruction. */
void print_RETURN(void);
/** @brief Prints an EXIT instruction. @param code Exit status. */
void print_EXIT(const int code);

/** @brief Prints a jump to a suffixed label. @param label Label name. @param
 * suffix Label suffix. */
void print_JUMP(const char *label, const int suffix);
/** @brief Prints JUMPIFEQS to a suffixed label. @param label Label name. @param
 * suffix Label suffix. */
void print_JUMPIFEQS(const char *label, const int suffix);
/** @brief Prints JUMPIFNEQS to a suffixed label. @param label Label name.
 * @param suffix Label suffix. */
void print_JUMPIFNEQS(const char *label, const int suffix);
/** @brief Pushes a Boolean and jumps conditionally. @param label Label name.
 * @param suffix Label suffix. @param value Boolean value to push. */
void print_JUMPIFEQS_bool(const char *label, const int suffix, bool value);

/** @brief Prints a NEWARRAY instruction. @param array Array variable. @param
 * size Array size. */
void print_NEWARRAY(const InstructionOperand array,
                    const InstructionOperand size);
/** @brief Prints an ARRAYGET instruction. @param target_var Destination
 * variable. @param array Array operand. @param index Element index. */
void print_ARRAYGET(const InstructionOperand target_var,
                    const InstructionOperand array,
                    const InstructionOperand index);
/** @brief Prints an ARRAYSET instruction. @param array Array operand. @param
 * index Element index. @param value Element value. */
void print_ARRAYSET(const InstructionOperand array,
                    const InstructionOperand index,
                    const InstructionOperand value);

/** @brief Prints an ADD instruction. @param target Destination operand. @param
 * left Left operand. @param right Right operand. */
void print_ADD(const InstructionOperand target, const InstructionOperand left,
               const InstructionOperand right);
/** @brief Prints a SUB instruction. @param target Destination operand. @param
 * left Left operand. @param right Right operand. */
void print_SUB(const InstructionOperand target, const InstructionOperand left,
               const InstructionOperand right);
/** @brief Prints an IDIV instruction. @param target Destination operand. @param
 * left Left operand. @param right Right operand. */
void print_IDIV(const InstructionOperand target, const InstructionOperand left,
                const InstructionOperand right);
/** @brief Prints a MOVE instruction. @param target Destination operand. @param
 * source Source operand. */
void print_MOVE(const InstructionOperand target,
                const InstructionOperand source);
/** @brief Prints JUMPIFEQ for two operands. @param label Label name. @param
 * suffix Label suffix. @param left Left operand. @param right Right operand. */
void print_JUMPIFEQ(const char *label, const int suffix,
                    const InstructionOperand left,
                    const InstructionOperand right);
/** @brief Prints JUMPIFNEQ for two operands. @param label Label name. @param
 * suffix Label suffix. @param left Left operand. @param right Right operand. */
void print_JUMPIFNEQ(const char *label, const int suffix,
                     const InstructionOperand left,
                     const InstructionOperand right);
/** @brief Prints the ISINTS instruction. */
void print_ISINTS(void);

/** @brief Prints an LT instruction. @param target Destination operand. @param
 * left Left operand. @param right Right operand. */
void print_LT(const InstructionOperand target, const InstructionOperand left,
              const InstructionOperand right);
/** @brief Prints a GT instruction. @param target Destination operand. @param
 * left Left operand. @param right Right operand. */
void print_GT(const InstructionOperand target, const InstructionOperand left,
              const InstructionOperand right);
/** @brief Prints an EQ instruction. @param target Destination operand. @param
 * left Left operand. @param right Right operand. */
void print_EQ(const InstructionOperand target, const InstructionOperand left,
              const InstructionOperand right);

/** @brief Prints a CONCAT instruction. @param target Destination operand.
 * @param left Left operand. @param right Right operand. */
void print_CONCAT(const InstructionOperand target,
                  const InstructionOperand left,
                  const InstructionOperand right);

#endif // INSTRUCTIONS_H