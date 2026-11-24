/**
 * @file codegen_globals.h
 * @brief Public interface exposing target code global variables.
 * @author Milan Novák
 */

#ifndef CODEGEN_GLOBALS_H
#define CODEGEN_GLOBALS_H
#include "instructions.h"

extern const InstructionOperand TRASH_GLOBAL;

extern const InstructionOperand LIST_GLOBAL_RES;
extern const InstructionOperand LIST_INDEX_GLOBAL_RES;
extern const InstructionOperand LIST_ELEMENT_GLOBAL_RES;
extern const InstructionOperand LIST_SIZE_GLOBAL_RES;

extern const InstructionOperand LIST_GLOBAL_A;
extern const InstructionOperand LIST_INDEX_GLOBAL_A;
extern const InstructionOperand LIST_ELEMENT_GLOBAL_A;
extern const InstructionOperand LIST_SIZE_GLOBAL_A;

extern const InstructionOperand LIST_GLOBAL_B;
extern const InstructionOperand LIST_INDEX_GLOBAL_B;
extern const InstructionOperand LIST_ELEMENT_GLOBAL_B;
extern const InstructionOperand LIST_SIZE_GLOBAL_B;

extern const InstructionOperand FROM_GLOBAL;
extern const InstructionOperand TO_GLOBAL;
extern const InstructionOperand BY_GLOBAL;

extern const InstructionOperand COND_GLOBAL;
extern const InstructionOperand TMP_GLOBAL;

extern const InstructionOperand GLOBAL_VARS[];
extern const size_t GLOBAL_VAR_COUNT;
#endif // CODEGEN_GLOBALS_H