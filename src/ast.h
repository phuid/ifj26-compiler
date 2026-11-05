#ifndef SRC_AST_H_
#define SRC_AST_H_

#include <stddef.h>

#include "symtable.h"

typedef struct FunctionSymbol FunctionSymbol;
typedef struct ASTNode ASTNode;

/** Identifies the syntax/semantic construct represented by an AST node. */
typedef enum {
  /** Function declaration; activates @c data.function. */
  AST_FUNCTION,
  /** Statement block; activates @c data.block. */
  AST_BLOCK,
  /** Integer literal; activates @c data.int_value. */
  AST_CONSTANT_INT,
  /** Real literal; activates @c data.real_value. */
  AST_CONSTANT_REAL,
  /** Owned string literal; activates @c data.string_value. */
  AST_CONSTANT_STRING,
  /** Borrowed resolved variable binding; activates @c data.variable. */
  AST_VARIABLE_REF,
  /** Function call with owned argument nodes; activates @c data.call. */
  AST_FUNCTION_CALL,
  /** Assignment; activates @c data.assignment. */
  AST_ASSIGN,
  /** Return statement; activates @c data.return_statement. */
  AST_RETURN,
  /** Conditional expression or statement; activates @c data.if_statement. */
  AST_IF,
  /** Addition; activates @c data.binary_op. */
  AST_ADD,
  /** Subtraction; activates @c data.binary_op. */
  AST_SUBTRACT,
  /** Multiplication; activates @c data.binary_op. */
  AST_MULTIPLY,
  /** Division; activates @c data.binary_op. */
  AST_DIVIDE,
  /** Compiler-inserted conversion; activates @c data.conversion. */
  AST_IMPLICIT_INT_TO_REAL
} ASTNodeKind;

/** Reports an AST construction operation. */
typedef enum {
  /** Operation completed successfully. */
  AST_OK,
  /** Null, incompatible, or otherwise invalid argument was supplied. */
  AST_INVALID_ARGUMENT,
  /** Allocation for an AST-owned object failed. */
  AST_OUT_OF_MEMORY
} ASTStatus;

/**
 * @brief Owns one syntax tree node and all recursively owned child nodes.
 * @details The active @c data union member is selected exclusively by @c kind.
 * Pointers to @ref Symbol and @ref FunctionSymbol borrow canonical semantic
 * records; pointers to @ref ASTNode and their dynamic pointer arrays are owned
 * recursively by this node. Source locations are 1-based.
 */
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

/**
 * @brief Creates an integer-literal node.
 * @param value Literal value represented by the node.
 * @param line One-based source line of the literal.
 * @param column One-based source column of the literal.
 * @return An owned node with @c kind AST_CONSTANT_INT, or null on allocation
 * failure.
 */
ASTNode *ast_node_create_int(long long value, size_t line, size_t column);

/**
 * @brief Creates a variable-reference node for an already resolved binding.
 * @param symbol Non-null borrowed symbol record; it must outlive the AST.
 * @param line One-based source line of the reference.
 * @param column One-based source column of the reference.
 * @return An owned node with @c kind AST_VARIABLE_REF, or null for an invalid
 * argument or allocation failure.
 */
ASTNode *ast_node_create_variable_ref(Symbol *symbol, size_t line,
                                      size_t column);

/**
 * @brief Creates a function-call node for an already resolved function.
 * @param function Non-null borrowed function record; it must outlive the AST.
 * @param line One-based source line of the call.
 * @param column One-based source column of the call.
 * @return An owned node with @c kind AST_FUNCTION_CALL, or null for an invalid
 * argument or allocation failure.
 */
ASTNode *ast_node_create_function_call(FunctionSymbol *function, size_t line,
                                       size_t column);

/**
 * @brief Appends one owned argument node to a function call.
 * @param call Non-null call node with @c kind AST_FUNCTION_CALL.
 * @param argument Non-null node whose ownership transfers on success only.
 * @return @ref AST_OK, @ref AST_INVALID_ARGUMENT, or @ref AST_OUT_OF_MEMORY.
 */
ASTStatus ast_node_call_append_argument(ASTNode *call, ASTNode *argument);

/**
 * @brief Recursively releases a node and every AST-owned payload.
 * @details Borrowed @ref Symbol and @ref FunctionSymbol records are never
 * released. A null node is accepted as a no-op.
 * @param node Root of the owned subtree to destroy, or null.
 */
void ast_node_destroy(ASTNode *node);

#endif // SRC_AST_H_
