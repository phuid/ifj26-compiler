#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

void print_prolog(void);
void print_LABEL(const char *name, const int suffix);
void print_DEFVAR(const InstructionOperand operand);
void print_PUSHS(const InstructionOperand operand);
void print_POPS(const InstructionOperand operand);

void print_ADDS(void);
void print_SUBS(void);
void print_MULS(void);
void print_DIVS(void);
void print_IDIVS(void);

void print_LTS(void);
void print_GTS(void);
void print_EQS(void);

void print_ANDS(void);
void print_ORS(void);
void print_NOTS(void);

void print_INT2FLOATS(void);

void print_CALL(const char *label);

void print_CREATEFRAME(void);
void print_PUSHFRAME(void);
void print_POPFRAME(void);

void print_RETURN(void);
void print_EXIT(const int code);

void print_JUMP(const char *label, const int suffix);
void print_JUMPIFEQS(const char *label, const int suffix);
void print_JUMPIFNEQS(const char *label, const int suffix);
void print_JUMPIFEQS_bool(const char *label, const int suffix, bool value);

void print_NEWARRAY(const InstructionOperand array,
                    const InstructionOperand size);
void print_ARRAYGET(const InstructionOperand target_var,
                    const InstructionOperand array,
                    const InstructionOperand index);
void print_ARRAYSET(const InstructionOperand array,
                    const InstructionOperand index,
                    const InstructionOperand value);

void print_ADD(const InstructionOperand target, const InstructionOperand left,
               const InstructionOperand right);
void print_SUB(const InstructionOperand target, const InstructionOperand left,
               const InstructionOperand right);
void print_IDIV(const InstructionOperand target, const InstructionOperand left,
                const InstructionOperand right);
void print_MOVE(const InstructionOperand target,
                const InstructionOperand source);
void print_JUMPIFEQ(const char *label, const int suffix,
                    const InstructionOperand left,
                    const InstructionOperand right);
void print_JUMPIFNEQ(const char *label, const int suffix,
                     const InstructionOperand left,
                     const InstructionOperand right);
void print_ISINTS(void);

void print_LT(const InstructionOperand target, const InstructionOperand left,
              const InstructionOperand right);
void print_GT(const InstructionOperand target, const InstructionOperand left,
              const InstructionOperand right);
void print_EQ(const InstructionOperand target, const InstructionOperand left,
              const InstructionOperand right);

void print_CONCAT(const InstructionOperand target,
                  const InstructionOperand left,
                  const InstructionOperand right);

#endif // INSTRUCTIONS_H