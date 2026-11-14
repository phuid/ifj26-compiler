/**
 * @file ast.c
 * @brief Abstract syntax tree structures and operations.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was mostly generated using an LLM agent based on
 *       specifikace.txt.
 */

#include "ast.h"

#include "dynamic_array.h"

#include <stdlib.h>
#include <string.h>

/**
 * @brief Allocates a zero-initialized AST node with an unresolved type.
 * @param kind Node kind selecting the active union payload.
 * @param line One-based source line stored in the node.
 * @param column One-based source column stored in the node.
 * @return An owned node, or null if allocation fails.
 */
static ASTNode *ast_node_create(ASTNodeKind kind, size_t line, size_t column) {
  ASTNode *node = calloc(1U, sizeof(*node));

  if (node != NULL) {
    node->kind = kind;
    node->line = line;
    node->column = column;
    type_entity_init(&node->inferred_type);
  }
  return node;
}

/**
 * @brief Constructs an owned integer literal with a concrete integer type.
 * @param value Literal value.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return A new integer node, or null on allocation failure.
 */
ASTNode *ast_node_create_int(long long value, size_t line, size_t column) {
  ASTNode *node = ast_node_create(AST_CONSTANT_INT, line, column);

  if (node != NULL) {
    node->data.int_value = value;
    type_entity_resolve(&node->inferred_type, type_scalar(TYPE_INT));
  }
  return node;
}

/**
 * @brief Constructs an owned real literal with a concrete real type.
 * @param value Literal value.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return A new real node, or null on allocation failure.
 */
ASTNode *ast_node_create_real(double value, size_t line, size_t column) {
  ASTNode *node = ast_node_create(AST_CONSTANT_REAL, line, column);

  if (node != NULL) {
    node->data.real_value = value;
    type_entity_resolve(&node->inferred_type, type_scalar(TYPE_REAL));
  }
  return node;
}

/**
 * @brief Constructs an owned string literal holding a copy of @p value.
 * @details The caller keeps ownership of @p value; the node owns its copy.
 * @param value Non-null null-terminated string to copy.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return A new string node, or null for invalid input or OOM.
 */
ASTNode *ast_node_create_string(const char *value, size_t line, size_t column) {
  ASTNode *node;
  char *copy;
  size_t size;

  if (value == NULL) {
    return NULL;
  }
  size = strlen(value) + 1U;
  copy = malloc(size);
  if (copy == NULL) {
    return NULL;
  }
  memcpy(copy, value, size);
  node = ast_node_create(AST_CONSTANT_STRING, line, column);
  if (node == NULL) {
    free(copy);
    return NULL;
  }
  node->data.string_value = copy;
  type_entity_resolve(&node->inferred_type, type_scalar(TYPE_STRING));
  return node;
}

/**
 * @brief Constructs an owned boolean literal.
 * @param value Literal value.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return A new boolean node, or null on allocation failure.
 */
ASTNode *ast_node_create_bool(bool value, size_t line, size_t column) {
  ASTNode *node = ast_node_create(AST_CONSTANT_BOOL, line, column);

  if (node != NULL) {
    node->data.bool_value = value;
    type_entity_resolve(&node->inferred_type, type_scalar(TYPE_BOOL));
  }
  return node;
}

/**
 * @brief Constructs an owned unit literal.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return A new unit node, or null on allocation failure.
 */
ASTNode *ast_node_create_unit(size_t line, size_t column) {
  ASTNode *node = ast_node_create(AST_CONSTANT_UNIT, line, column);

  if (node != NULL) {
    type_entity_resolve(&node->inferred_type, type_scalar(TYPE_UNIT));
  }
  return node;
}

/**
 * @brief Constructs an owned reference to a borrowed resolved symbol.
 * @param symbol Non-null semantic binding borrowed by the new node.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return A variable-reference node, or null for invalid input or OOM.
 */
ASTNode *ast_node_create_variable_ref(Symbol *symbol, size_t line,
                                      size_t column) {
  ASTNode *node;

  if (symbol == NULL) {
    return NULL;
  }
  node = ast_node_create(AST_VARIABLE_REF, line, column);
  if (node != NULL) {
    node->data.variable.symbol = symbol;
  }
  return node;
}

/**
 * @brief Constructs an owned call node for a borrowed resolved function.
 * @param function Non-null function record borrowed by the new node.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return A function-call node, or null for invalid input or OOM.
 */
ASTNode *ast_node_create_function_call(FunctionSymbol *function, size_t line,
                                       size_t column) {
  ASTNode *node;

  if (function == NULL) {
    return NULL;
  }
  node = ast_node_create(AST_FUNCTION_CALL, line, column);
  if (node != NULL) {
    node->data.call.function = function;
  }
  return node;
}

ASTNode *ast_node_create_list_constructor(size_t line, size_t column) {
  ASTNode *node = ast_node_create(AST_LIST_CONSTRUCTOR, line, column);
  return node;
}

/**
 * @brief Transfers an argument node into a function-call node.
 * @details On failure, @p argument remains owned by the caller.
 * @param call Non-null AST_FUNCTION_CALL node receiving the argument.
 * @param argument Non-null owned child to transfer on success.
 * @return Construction status.
 */
ASTStatus ast_node_call_append_argument(ASTNode *call, ASTNode *argument) {
  if (call == NULL || argument == NULL || call->kind != AST_FUNCTION_CALL) {
    return AST_INVALID_ARGUMENT;
  }
  if (dynamic_array_reserve((void **)&call->data.call.arguments,
                            &call->data.call.argument_capacity,
                            call->data.call.argument_count + 1U,
                            sizeof(*call->data.call.arguments)) !=
      DYNAMIC_ARRAY_OK) {
    return AST_OUT_OF_MEMORY;
  }
  call->data.call.arguments[call->data.call.argument_count] = argument;
  call->data.call.argument_count += 1U;
  return AST_OK;
}

/**
 * @brief Constructs an empty owned statement block.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return A new block node, or null on allocation failure.
 */
ASTNode *ast_node_create_block(size_t line, size_t column) {
  return ast_node_create(AST_BLOCK, line, column);
}

/**
 * @brief Transfers a statement node into a block node.
 * @details On failure, @p statement remains owned by the caller.
 * @param block Non-null AST_BLOCK node receiving the statement.
 * @param statement Non-null owned child to transfer on success.
 * @return Construction status.
 */
ASTStatus ast_node_block_append_statement(ASTNode *block, ASTNode *statement) {
  if (block == NULL || statement == NULL || block->kind != AST_BLOCK) {
    return AST_INVALID_ARGUMENT;
  }
  if (dynamic_array_reserve((void **)&block->data.block.statements,
                            &block->data.block.statement_capacity,
                            block->data.block.statement_count + 1U,
                            sizeof(*block->data.block.statements)) !=
      DYNAMIC_ARRAY_OK) {
    return AST_OUT_OF_MEMORY;
  }
  block->data.block.statements[block->data.block.statement_count] = statement;
  block->data.block.statement_count += 1U;
  return AST_OK;
}

/**
 * @brief Constructs an owned assignment to a borrowed resolved symbol.
 * @details On failure, @p value remains owned by the caller.
 * @param target Non-null symbol borrowed by the new node.
 * @param value Non-null owned expression to transfer on success.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return An assignment node, or null for invalid input or OOM.
 */
ASTNode *ast_node_create_assign(Symbol *target, ASTNode *value, size_t line,
                                size_t column) {
  ASTNode *node;

  if (target == NULL || value == NULL) {
    return NULL;
  }
  node = ast_node_create(AST_ASSIGN, line, column);
  if (node != NULL) {
    node->data.assignment.target = target;
    node->data.assignment.value = value;
  }
  return node;
}

/**
 * @brief Constructs an owned return statement.
 * @details On failure, @p value remains owned by the caller.
 * @param value Owned expression to transfer on success, or null.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return A return node, or null on allocation failure.
 */
ASTNode *ast_node_create_return(ASTNode *value, size_t line, size_t column) {
  ASTNode *node = ast_node_create(AST_RETURN, line, column);

  if (node != NULL) {
    node->data.return_statement.value = value;
  }
  return node;
}

/**
 * @brief Constructs an owned conditional node.
 * @details On failure, all children remain owned by the caller.
 * @param condition Non-null owned condition.
 * @param then_branch Non-null owned branch.
 * @param else_branch Owned alternative branch, or null.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return A conditional node, or null for invalid input or OOM.
 */
ASTNode *ast_node_create_if(ASTNode *condition, ASTNode *then_branch,
                            ASTNode *else_branch, size_t line, size_t column) {
  ASTNode *node;

  if (condition == NULL || then_branch == NULL) {
    return NULL;
  }
  node = ast_node_create(AST_IF, line, column);
  if (node != NULL) {
    node->data.if_statement.condition = condition;
    node->data.if_statement.then_branch = then_branch;
    node->data.if_statement.else_branch = else_branch;
  }
  return node;
}

/**
 * @brief Constructs an owned binary arithmetic node.
 * @details Ownership of both operands transfers on success only.
 * @param kind Binary op node kind, one of: AST_ADD, AST_SUBTRACT, AST_MULTIPLY,
 * AST_DIVIDE, AST_LIST_CONCAT, AST_EQUAL, AST_NOT_EQUAL, AST_LESS,
 * AST_LESS_EQUAL, AST_GREATER, AST_GREATER_EQUAL, AST_AND, AST_OR.
 * @param left Non-null owned left operand.
 * @param right Non-null owned right operand.
 * @param line One-based source line.
 * @param column One-based source column.
 * @return An owned node of the requested @p kind, or null for an invalid
 * argument or allocation failure.
 */
ASTNode *ast_node_create_binary_op(ASTNodeKind kind, ASTNode *left,
                                   ASTNode *right, size_t line, size_t column) {
  ASTNode *node;

  if (left == NULL || right == NULL) {
    return NULL;
  }

  /// guard against invalid kinds that are not binary operations
  switch (kind) {
  case AST_ADD:
  case AST_SUBTRACT:
  case AST_MULTIPLY:
  case AST_DIVIDE:
  case AST_LIST_CONCAT:
  case AST_EQUAL:
  case AST_NOT_EQUAL:
  case AST_LESS:
  case AST_LESS_EQUAL:
  case AST_GREATER:
  case AST_GREATER_EQUAL:
  case AST_AND:
  case AST_OR:
    break;
  default:
    return NULL;
  }

  node = ast_node_create(kind, line, column);
  if (node != NULL) {
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;
  }
  // TODO: should there be type dependency here?
  return node;
}

/**
 * @brief Creates a logical NOT node.
 * @details Ownership of the operand transfers on success only.
 * @param operand Non-null operand whose ownership transfers on success only.
 * @param line One-based source line of the operator.
 * @param column One-based source column of the operator.
 * @return An owned node with @c kind AST_NOT, or null for an invalid argument
 * or allocation failure.
 */
ASTNode *ast_node_create_not(ASTNode *operand, size_t line, size_t column) {
  ASTNode *node;

  if (operand == NULL) {
    return NULL;
  }
  node = ast_node_create(AST_NOT, line, column);
  if (node != NULL) {
    node->data.unary_op.operand = operand;
    type_entity_resolve(&node->inferred_type, type_scalar(TYPE_BOOL));
  }
  return node;
}

/**
 * @brief Wraps an owned expression in an implicit Int-to-Double conversion.
 * @details On failure, @p operand remains owned by the caller.
 * @param operand Non-null owned expression to transfer on success.
 * @return A conversion node, or null for invalid input or OOM.
 */
ASTNode *ast_node_create_implicit_int_to_real(ASTNode *operand) {
  ASTNode *node;

  if (operand == NULL) {
    return NULL;
  }
  node =
      ast_node_create(AST_IMPLICIT_INT_TO_REAL, operand->line, operand->column);
  if (node != NULL) {
    node->data.conversion.operand = operand;
    type_entity_resolve(&node->inferred_type, type_scalar(TYPE_REAL));
  }
  return node;
}

/**
 * @brief Recursively destroys one AST-owned subtree.
 * @details Semantic records referenced by nodes are borrowed and never freed.
 * @param node Owned subtree root; null is a no-op.
 */
void ast_node_destroy(ASTNode *node) {
  size_t index;

  if (node == NULL) {
    return;
  }
  switch (node->kind) {
  case AST_CONSTANT_STRING:
    free(node->data.string_value);
    break;
  case AST_FUNCTION_CALL:
    for (index = 0U; index < node->data.call.argument_count; index += 1U) {
      ast_node_destroy(node->data.call.arguments[index]);
    }
    free(node->data.call.arguments);
    break;
  case AST_ASSIGN:
    ast_node_destroy(node->data.assignment.value);
    break;
  case AST_RETURN:
    ast_node_destroy(node->data.return_statement.value);
    break;
  case AST_IF:
    ast_node_destroy(node->data.if_statement.condition);
    ast_node_destroy(node->data.if_statement.then_branch);
    ast_node_destroy(node->data.if_statement.else_branch);
    break;
  case AST_ADD:
  case AST_SUBTRACT:
  case AST_MULTIPLY:
  case AST_DIVIDE:
  case AST_LIST_CONCAT:
  case AST_EQUAL:
  case AST_NOT_EQUAL:
  case AST_LESS:
  case AST_LESS_EQUAL:
  case AST_GREATER:
  case AST_GREATER_EQUAL:
  case AST_AND:
  case AST_OR:
    ast_node_destroy(node->data.binary_op.left);
    ast_node_destroy(node->data.binary_op.right);
    break;
  case AST_BLOCK:
    for (index = 0U; index < node->data.block.statement_count; index += 1U) {
      ast_node_destroy(node->data.block.statements[index]);
    }
    free(node->data.block.statements);
    break;
  case AST_IMPLICIT_INT_TO_REAL:
    ast_node_destroy(node->data.conversion.operand);
    break;
  case AST_NOT:
    ast_node_destroy(node->data.unary_op.operand);
    break;
  case AST_CONSTANT_INT:
  case AST_CONSTANT_REAL:
  case AST_CONSTANT_BOOL:
  case AST_CONSTANT_UNIT:
  case AST_VARIABLE_REF:
    break;
  }
  free(node);
}
