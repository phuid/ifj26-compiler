#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "parser.h"
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

typedef void (*ASTNodeHandler)(const ASTNode *node, bool discard_return_value);

void handle_block(const ASTNode *node, bool discard_return_value);
void handle_asign(const ASTNode *node, bool discard_return_value);
void handle_variable_ref(const ASTNode *node, bool discard_return_value);

void handle_int(const ASTNode *node, bool discard_return_value);
void handle_real(const ASTNode *node, bool discard_return_value);
void handle_string(const ASTNode *node, bool discard_return_value);
void handle_bool(const ASTNode *node, bool discard_return_value);
void handle_unit(const ASTNode *node, bool discard_return_value);

void handle_add(const ASTNode *node, bool discard_return_value);
void handle_subtract(const ASTNode *node, bool discard_return_value);
void handle_multiply(const ASTNode *node, bool discard_return_value);
void handle_divide(const ASTNode *node, bool discard_return_value);
void handle_equal(const ASTNode *node, bool discard_return_value);
void handle_not_equal(const ASTNode *node, bool discard_return_value);
void handle_less(const ASTNode *node, bool discard_return_value);
void handle_less_equal(const ASTNode *node, bool discard_return_value);
void handle_greater(const ASTNode *node, bool discard_return_value);
void handle_greater_equal(const ASTNode *node, bool discard_return_value);
void handle_and(const ASTNode *node, bool discard_return_value);
void handle_or(const ASTNode *node, bool discard_return_value);
void handle_not(const ASTNode *node, bool discard_return_value);

void handle_implicit_int_to_real(const ASTNode *node,
                                 bool discard_return_value);

void handle_call(const ASTNode *node, bool discard_return_value);
void handle_return(const ASTNode *node, bool discard_return_value);
void handle_if(const ASTNode *node, bool discard_return_value);

void handle_while(const ASTNode *node, bool discard_return_value);
void handle_for(const ASTNode *node, bool discard_return_value);

void handle_list_constructor(const ASTNode *node, bool discard_return_value);
void handle_list_indexing(const ASTNode *node, bool discard_return_value);
void handle_list_concat(const ASTNode *node, bool discard_return_value);
void handle_to(const ASTNode *node, bool discard_return_value);
void handle_until(const ASTNode *node, bool discard_return_value);
void handle_by(const ASTNode *node, bool discard_return_value);

void handle_function_definition(const FunctionSymbol *function);

/* Main code generation entry point */

void convert_AST_to_assembler(Program *program);

#endif /* CODEGEN_H */