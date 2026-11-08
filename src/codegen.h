#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "parser.h"

#include <stddef.h>

typedef enum { TF, LF, GF } VariableFrame;

typedef void (*ASTNodeHandler)(const ASTNode *node);

void print_variable_name(VariableFrame frame, const char *name, int suffix);

void print_LABEL(const char *name);

void print_DEFVAR(const char *name, VariableFrame frame, int suffix);
void print_PUSHS(const char *name, VariableFrame frame, int suffix);
void print_POPS(const char *name, VariableFrame frame, int suffix);

void print_ADDS(void);
void print_SUBS(void);
void print_MULS(void);
void print_DIVS(void);
void print_IDIVS(void);

void print_CALL(const char *label);

void print_CREATEFRAME(void);
void print_PUSHFRAME(void);
void print_POPFRAME(void);
void print_RETURN(void);

/* AST handlers */

void handle_block(const ASTNode *node);
void handle_asign(const ASTNode *node);
void handle_variable_ref(const ASTNode *node);

void handle_int(const ASTNode *node);
void handle_real(const ASTNode *node);
void handle_string(const ASTNode *node);

void resolve_binary_operands(const ASTNode *node);

void handle_add(const ASTNode *node);
void handle_sub(const ASTNode *node);
void handle_mul(const ASTNode *node);
void handle_div(const ASTNode *node);

void handle_call(const ASTNode *node);
void handle_return(const ASTNode *node);
void handle_function_definition(const FunctionSymbol *function);

/* Main code generation entry point */

void convert_AST_to_assembler(Program *program);

#endif /* CODEGEN_H */