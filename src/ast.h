#ifndef SRC_AST_H_
#define SRC_AST_H_

#include <stddef.h>

#include "symtable.h"

typedef struct FunctionSymbol FunctionSymbol;
typedef struct ASTNode ASTNode;

/** Identifies the syntax/semantic construct represented by an AST node. */
typedef enum {
  AST_FUNCTION,
  AST_BLOCK,
  AST_CONSTANT_INT,
  AST_CONSTANT_REAL,
  AST_CONSTANT_STRING,
  AST_VARIABLE_REF,
  AST_FUNCTION_CALL,
  AST_ASSIGN,
  AST_RETURN,
  AST_IF,
  AST_ADD,
  AST_SUBTRACT,
  AST_MULTIPLY,
  AST_DIVIDE,
  AST_IMPLICIT_INT_TO_REAL
} ASTNodeKind;

/** Reports an AST construction operation. */
typedef enum { AST_OK, AST_INVALID_ARGUMENT, AST_OUT_OF_MEMORY } ASTStatus;

/** Owns a syntax tree node and all recursively owned child nodes. */
struct ASTNode {
  ASTNodeKind kind;
  size_t line;
  size_t column;
  DataType inferred_type;
  union {
    long long int_value;
    double real_value;
    char *string_value;
    struct {
      Symbol *symbol;
    } variable;
    struct {
      FunctionSymbol *function;
      ASTNode **arguments;
      size_t argument_count;
      size_t argument_capacity;
    } call;
    struct {
      ASTNode *left;
      ASTNode *right;
    } binary_op;
    struct {
      Symbol *target;
      ASTNode *value;
    } assignment;
    struct {
      ASTNode *value;
    } return_statement;
    struct {
      ASTNode *condition;
      ASTNode *then_branch;
      ASTNode *else_branch;
    } if_statement;
    struct {
      ASTNode **statements;
      size_t statement_count;
      size_t statement_capacity;
    } block;
    struct {
      FunctionSymbol *function;
      ASTNode *body;
    } function;
    struct {
      ASTNode *operand;
    } conversion;
  } data;
};

/** Creates an integer literal node. */
ASTNode *ast_node_create_int(long long value, size_t line, size_t column);

/** Creates a variable reference that borrows one resolved symbol. */
ASTNode *ast_node_create_variable_ref(Symbol *symbol, size_t line,
                                      size_t column);

/** Creates a function call that borrows one resolved function. */
ASTNode *ast_node_create_function_call(FunctionSymbol *function, size_t line,
                                       size_t column);

/** Appends an owned argument node to a function-call node. */
ASTStatus ast_node_call_append_argument(ASTNode *call, ASTNode *argument);

/** Recursively releases node-owned children and payloads. */
void ast_node_destroy(ASTNode *node);

#endif // SRC_AST_H_
