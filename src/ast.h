/**
 * @file ast.h
 * @brief Public abstract syntax tree declarations.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was mostly generated using an LLM agent based on
 *       specifikace.txt.
 */

#ifndef SRC_AST_H_
#define SRC_AST_H_

#include <stddef.h>

#include "symtable.h"
#include "type.h"

typedef struct FunctionSymbol FunctionSymbol;
typedef struct ASTNode ASTNode;

/// Identifies the syntax/semantic construct represented by an AST node.
typedef enum {
  /** Statement block; activates @c data.block. */
  AST_BLOCK,
  /** Integer literal; activates @c data.int_value. */
  AST_CONSTANT_INT,
  /** Real literal; activates @c data.real_value. */
  AST_CONSTANT_REAL,
  /** Owned string literal; activates @c data.string_value. */
  AST_CONSTANT_STRING,
  /** Boolean literal; activates @c data.bool_value. */
  AST_CONSTANT_BOOL,
  /** Unit literal; does not activate any data member. */
  AST_CONSTANT_UNIT,
  /** Borrowed resolved variable binding; activates @c data.variable. */
  AST_VARIABLE_REF,
  /** Function call with owned argument nodes; activates @c data.call. */
  AST_FUNCTION_CALL,
  /** List call with owned argument nodes; activates @c data.call. @c
     FunctionSymbol is NULL */
  AST_LIST_CONSTRUCTOR,
  /** List indexing; activates @c data.binary_op. Left operand is the list,
     right operand is the index. */
  AST_LIST_INDEXING,
  /** Integer "to" operand, converts to list starting with left, ended by right;
     activates @c data.binary_op.  */
  AST_TO,
  /** Integer "until" operand, converts to list starting with left, ended by
     (right - 1); activates @c data.binary_op. */
  AST_UNTIL,
  /** List x integer "by" operand takes a list and an integer, and returns a
     list with elements from the original list, spaced by the integer; activates
     @c data.binary_op. Left operand is the list, right operand is the integer.
   */
  AST_BY,
  /** Assignment; activates @c data.assignment. */
  AST_ASSIGN,
  /** Return statement; activates @c data.return_statement. */
  AST_RETURN,
  /** Conditional expression or statement; activates @c data.if_statement. */
  AST_IF,
  /** While loop statement; activates @c data.while_loop. */
  AST_WHILE,
  /** For loop statement iterating over a list (ranges built by `to`, `until`
     and `by` are lists too); activates @c data.for_loop. */
  AST_FOR,
  /** Addition; activates @c data.binary_op. */
  AST_ADD,
  /** Subtraction; activates @c data.binary_op. */
  AST_SUBTRACT,
  /** Multiplication; activates @c data.binary_op. */
  AST_MULTIPLY,
  /** Division; activates @c data.binary_op. */
  AST_DIVIDE,
  /** Concatenation; activates @c data.binary_op. */
  AST_LIST_CONCAT,
  /** Equality; activates @c data.binary_op. */
  AST_EQUAL, // ==
  /** Inequality; activates @c data.binary_op. */
  AST_NOT_EQUAL, // !=
  /** Less than; activates @c data.binary_op. */
  AST_LESS, // <
  /** Less than or equal; activates @c data.binary_op. */
  AST_LESS_EQUAL, // <=
  /** Greater than; activates @c data.binary_op. */
  AST_GREATER, // >
  /** Greater than or equal; activates @c data.binary_op. */
  AST_GREATER_EQUAL, // >=
  /** Logical AND; activates @c data.binary_op. */
  AST_AND, // &&
  /** Logical OR; activates @c data.binary_op. */
  AST_OR, // ||
  /** Logical NOT; activates @c data.unary_op. */
  AST_NOT,

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
  TypeEntity inferred_type;
  union {
    long long int_value;
    double real_value;
    char *string_value;
    bool bool_value;
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
      ASTNode *operand;
    } unary_op;
    struct {
      Symbol *target;
      ASTNode *value;
      /** True for the initialization in `val`/`var`, false for `id = ...`. */
      bool is_definition;
    } assignment;
    struct {
      ASTNode *value;
    } return_statement;
    struct {
      ASTNode *condition;
      ASTNode *then_branch;
      ASTNode *else_branch;
      bool used_as_an_expression;
    } if_statement;
    struct {
      /** Boolean condition evaluated before every iteration. */
      ASTNode *condition;
      /** Loop body; a block or a one-line statement. */
      ASTNode *body;
    } while_loop;
    struct {
      /** Borrowed immutable control variable defined in the loop scope. */
      Symbol *variable;
      /** List the generator iterates over; evaluated once. */
      ASTNode *iterable;
      /** All guards joined by AST_AND (left to right), or null without
         guards. Evaluated for every element; the body runs only when true. */
      ASTNode *guard;
      /** Loop body; a block or a one-line statement. */
      ASTNode *body;
    } for_loop;
    struct {
      ASTNode **statements;
      size_t statement_count;
      size_t statement_capacity;
    } block;
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
 * @brief Creates a real-literal node.
 * @param value Literal value represented by the node.
 * @param line One-based source line of the literal.
 * @param column One-based source column of the literal.
 * @return An owned node with @c kind AST_CONSTANT_REAL, or null on allocation
 * failure.
 */
ASTNode *ast_node_create_real(double value, size_t line, size_t column);

/**
 * @brief Creates a string-literal node.
 * @details The node stores its own heap copy of @p value, which is released
 * by @ref ast_node_destroy. The caller keeps ownership of @p value and may
 * free it right after the call.
 * @param value Non-null null-terminated string to copy.
 * @param line One-based source line of the literal.
 * @param column One-based source column of the literal.
 * @return An owned node with @c kind AST_CONSTANT_STRING, or null for an
 * invalid argument or allocation failure.
 */
ASTNode *ast_node_create_string(const char *value, size_t line, size_t column);

/**
 * @brief Creates a boolean-literal node.
 * @param value Literal value represented by the node.
 * @param line One-based source line of the literal.
 * @param column One-based source column of the literal.
 * @return An owned node with @c kind AST_CONSTANT_BOOL, or null on allocation
 * failure.
 */
ASTNode *ast_node_create_bool(bool value, size_t line, size_t column);

/**
 * @brief Constructs an owned unit literal.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return A new unit node, or null on allocation failure.
 */
ASTNode *ast_node_create_unit(size_t line, size_t column);

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
 * @brief Creates a list-constructor node.
 * @param line One-based source line of the call.
 * @param column One-based source column of the call.
 * @return An owned node with @c kind AST_LIST_CONSTRUCTOR, or null for an
 * invalid argument or allocation failure.
 */
ASTNode *ast_node_create_list_constructor(size_t line, size_t column);

/**
 * @brief Creates a list-indexing node.
 * @param list Non-null node representing the list.
 * @param index Non-null node representing the index.
 * @param line One-based source line of the indexing.
 * @param column One-based source column of the indexing.
 * @return An owned node with @c kind AST_LIST_INDEXING, or null for an invalid
 * argument or allocation failure.
 */
ASTNode *ast_node_create_list_indexing(ASTNode *list, ASTNode *index,
                                       size_t line, size_t column);

/**
 * @brief Appends one owned argument node to a function call.
 * @param call Non-null call node with @c kind AST_FUNCTION_CALL.
 * @param argument Non-null node whose ownership transfers on success only.
 * @return @ref AST_OK, @ref AST_INVALID_ARGUMENT, or @ref
 * AST_OUT_OF_MEMORY.
 */
ASTStatus ast_node_call_append_argument(ASTNode *call, ASTNode *argument);

/**
 * @brief Creates an empty statement block.
 * @param line One-based source line of the block.
 * @param column One-based source column of the block.
 * @return An owned node with @c kind AST_BLOCK, or null on allocation failure.
 */
ASTNode *ast_node_create_block(size_t line, size_t column);

/**
 * @brief Appends one owned statement node to a block.
 * @param block Non-null block node with @c kind AST_BLOCK.
 * @param statement Non-null node whose ownership transfers on success only.
 * @return @ref AST_OK, @ref AST_INVALID_ARGUMENT, or @ref AST_OUT_OF_MEMORY.
 */
ASTStatus ast_node_block_append_statement(ASTNode *block, ASTNode *statement);

/**
 * @brief Creates an assignment node for an already resolved target.
 * @param target Non-null borrowed symbol record; it must outlive the AST.
 * @param value Non-null value node whose ownership transfers on success only.
 * @param line One-based source line of the assignment.
 * @param column One-based source column of the assignment.
 * @return An owned node with @c kind AST_ASSIGN, or null for an invalid
 * argument or allocation failure.
 */
ASTNode *ast_node_create_assign(Symbol *target, ASTNode *value, size_t line,
                                size_t column);

/**
 * @brief Creates a return-statement node.
 * @param value Returned expression whose ownership transfers on success only,
 * or null for a return without a value.
 * @param line One-based source line of the return.
 * @param column One-based source column of the return.
 * @return An owned node with @c kind AST_RETURN, or null on allocation
 * failure.
 */
ASTNode *ast_node_create_return(ASTNode *value, size_t line, size_t column);

/**
 * @brief Creates a conditional node.
 * @details Ownership of all non-null children transfers on success only.
 * @param condition Non-null condition expression.
 * @param then_branch Non-null branch evaluated when the condition holds.
 * @param else_branch Alternative branch, or null when absent.
 * @param line One-based source line of the conditional.
 * @param column One-based source column of the conditional.
 * @return An owned node with @c kind AST_IF, or null for an invalid argument
 * or allocation failure.
 */
ASTNode *ast_node_create_if(ASTNode *condition, ASTNode *then_branch,
                            ASTNode *else_branch, size_t line, size_t column);

/**
 * @brief Creates a while-loop node.
 * @details Ownership of both children transfers on success only.
 * @param condition Non-null condition expression.
 * @param body Non-null loop body (block or one-line statement).
 * @param line One-based source line of the `while` keyword.
 * @param column One-based source column of the `while` keyword.
 * @return An owned node with @c kind AST_WHILE, or null for an invalid
 * argument or allocation failure.
 */
ASTNode *ast_node_create_while(ASTNode *condition, ASTNode *body, size_t line,
                               size_t column);

/**
 * @brief Creates a for-loop node `for variable <- iterable [if guard] do body`.
 * @details Ownership of all non-null children transfers on success only.
 * @param variable Non-null borrowed control variable; it must outlive the AST.
 * @param iterable Non-null expression producing the iterated list.
 * @param guard Combined guard expression, or null when there is no guard.
 * @param body Non-null loop body (block or one-line statement).
 * @param line One-based source line of the `for` keyword.
 * @param column One-based source column of the `for` keyword.
 * @return An owned node with @c kind AST_FOR, or null for an invalid argument
 * or allocation failure.
 */
ASTNode *ast_node_create_for(Symbol *variable, ASTNode *iterable,
                             ASTNode *guard, ASTNode *body, size_t line,
                             size_t column);

/**
 * @brief Constructs an owned binary arithmetic node.
 * @details Ownership of both operands transfers on success only.
 * @param kind Binary op node kind, one of: AST_ADD, AST_SUBTRACT, AST_MULTIPLY,
 * AST_DIVIDE, AST_LIST_CONCAT, AST_EQUAL, AST_NOT_EQUAL, AST_LESS,
 * AST_LESS_EQUAL, AST_GREATER, AST_GREATER_EQUAL, AST_AND, AST_OR, AST_TO,
 * AST_UNTIL, AST_BY.
 * @param left Non-null owned left operand.
 * @param right Non-null owned right operand.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return An owned node of the requested @p kind, or null for an invalid
 * argument or allocation failure.
 */
ASTNode *ast_node_create_binary_op(ASTNodeKind kind, ASTNode *left,
                                   ASTNode *right, size_t line, size_t column);

/**
 * @brief Creates a logical NOT node.
 * @details Ownership of the operand transfers on success only.
 * @param operand Non-null operand whose ownership transfers on success only.
 * @param line One-based source line of the operator.
 * @param column One-based source column of the operator.
 * @return An owned node with @c kind AST_NOT, or null for an invalid argument
 * or allocation failure.
 */
ASTNode *ast_node_create_not(ASTNode *operand, size_t line, size_t column);

/**
 * @brief Wraps an expression in a compiler-inserted Int-to-Double conversion.
 * @details The new node inherits the source location of @p operand and has
 * inferred type TYPE_REAL.
 * @param operand Non-null node whose ownership transfers on success only.
 * @return An owned node with @c kind AST_IMPLICIT_INT_TO_REAL, or null for an
 * invalid argument or allocation failure.
 */
ASTNode *ast_node_create_implicit_int_to_real(ASTNode *operand);

/**
 * @brief Recursively releases a node and every AST-owned payload.
 * @details Borrowed @ref Symbol and @ref FunctionSymbol records are never
 * released. A null node is accepted as a no-op.
 * @param node Root of the owned subtree to destroy, or null.
 */
void ast_node_destroy(ASTNode *node);

#endif // SRC_AST_H_
