/**
 * @file codegen.h
 * Project: IFJ26 compiler
 * @brief Public interface for generating target code from a program.
 * @author Milan Novák (xnovakm00)
 */

#ifndef CODEGEN_H
#define CODEGEN_H

#include "parser.h"

void convert_AST_to_assembler(Program *program);

#endif /* CODEGEN_H */