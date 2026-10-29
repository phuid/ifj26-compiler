#include "ast.h"

#include "dynamic_array.h"

#include <stdlib.h>

/** Allocates an AST node with a still-unknown semantic type. */
static ASTNode *ast_node_create(ASTNodeKind kind, size_t line, size_t column) {
  ASTNode *node = calloc(1U, sizeof(*node));

  if (node != NULL) {
    node->kind = kind;
    node->line = line;
    node->column = column;
    node->inferred_type = TYPE_UNKNOWN;
  }
  return node;
}

ASTNode *ast_node_create_int(long long value, size_t line, size_t column) {
  ASTNode *node = ast_node_create(AST_CONSTANT_INT, line, column);

  if (node != NULL) {
    node->data.int_value = value;
    node->inferred_type = TYPE_INT;
  }
  return node;
}

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
    ast_node_destroy(node->data.binary_op.left);
    ast_node_destroy(node->data.binary_op.right);
    break;
  case AST_BLOCK:
    for (index = 0U; index < node->data.block.statement_count; index += 1U) {
      ast_node_destroy(node->data.block.statements[index]);
    }
    free(node->data.block.statements);
    break;
  case AST_FUNCTION:
    ast_node_destroy(node->data.function.body);
    break;
  case AST_IMPLICIT_INT_TO_REAL:
    ast_node_destroy(node->data.conversion.operand);
    break;
  case AST_CONSTANT_INT:
  case AST_CONSTANT_REAL:
  case AST_VARIABLE_REF:
    break;
  }
  free(node);
}
