#include "codegen.h"
#include "ast.h"
#include "builtins.h"
#include "codegen_globals.h"
#include "error_trace.h"
#include "instructions.h"
#include "parser.h"
#include "symtable.h"
#include <stdlib.h>
#include <string.h>

// This is such a disqusting solution, i dont like this
static int variable_suffix_counter = 0;

static int label_suffix_counter = 0;

static const ASTNodeHandler ast_handlers[] = {
    [AST_BLOCK] = handle_block,
    [AST_CONSTANT_INT] = handle_int,
    [AST_CONSTANT_REAL] = handle_real,
    [AST_CONSTANT_STRING] = handle_string,
    [AST_CONSTANT_BOOL] = handle_bool,
    [AST_CONSTANT_UNIT] = handle_unit,
    [AST_VARIABLE_REF] = handle_variable_ref,
    [AST_FUNCTION_CALL] = handle_call,
    [AST_LIST_CONSTRUCTOR] = handle_list_constructor,
    [AST_LIST_INDEXING] = handle_list_indexing,
    [AST_LIST_CONCAT] = handle_list_concat,
    [AST_TO] = handle_to,
    [AST_UNTIL] = handle_until,
    [AST_BY] = handle_by,
    [AST_ASSIGN] = handle_asign,
    [AST_RETURN] = handle_return,
    [AST_IF] = handle_if,
    [AST_WHILE] = handle_while,
    [AST_FOR] = handle_for,
    [AST_ADD] = handle_add,
    [AST_SUBTRACT] = handle_subtract,
    [AST_MULTIPLY] = handle_multiply,
    [AST_DIVIDE] = handle_divide,
    [AST_EQUAL] = handle_equal,
    [AST_NOT_EQUAL] = handle_not_equal,
    [AST_LESS] = handle_less,
    [AST_LESS_EQUAL] = handle_less_equal,
    [AST_GREATER] = handle_greater,
    [AST_GREATER_EQUAL] = handle_greater_equal,
    [AST_AND] = handle_and,
    [AST_OR] = handle_or,
    [AST_NOT] = handle_not,
    [AST_IMPLICIT_INT_TO_REAL] = handle_implicit_int_to_real,
};

static const InstructionOperand INT_ZERO_OPERAND = {.type = INT_TYPE,
                                                    .value = {.integer = 0}};

static const InstructionOperand INT_ONE_OPERAND = {.type = INT_TYPE,
                                                   .value = {.integer = 1}};

static const InstructionOperand BOOL_TRUE_OPERAND = {
    .type = BOOL_TYPE, .value = {.boolean = true}};

/** Function whose code is being generated; null outside of a function. */
static const FunctionSymbol *current_function = NULL;

/**
 * @brief Adds the current function as a context frame, prints the error trace
 * and exits with the internal error code.
 */
static _Noreturn void codegen_abort(void) {
  if (current_function != NULL) {
    ERROR_TRACE("while generating code for function '%s'",
                current_function->name);
  }
  ERROR_TRACE("while generating code");
  exit(99);
}

/**
 * @brief Returns the code generation handler of @p node, or reports an
 * internal error and exits if the node kind has none.
 */
static ASTNodeHandler ast_handler_for(const ASTNode *node) {
  if (node == NULL) {
    ERROR_INTERNAL("missing AST node");
    codegen_abort();
  }

  /// If the node kind is out of bounds or has no handler, report an internal
  /// error.
  if ((size_t)node->kind >= sizeof(ast_handlers) / sizeof(ast_handlers[0]) ||
      ast_handlers[node->kind] == NULL) {
    ERROR_ORIGIN_AT(node->line, node->column, NULL,
                    "internal error: no code generator for AST node kind %d",
                    (int)node->kind);
    codegen_abort();
  }
  return ast_handlers[node->kind];
}

void pair_real_var(Symbol *symbol) {
  if (symbol->assembler_suffix != -1) {
    ERROR_INTERNAL("symbol '%s' already has a real variable", symbol->name);
    codegen_abort();
  }

  symbol->assembler_suffix = variable_suffix_counter;
  variable_suffix_counter++;

  InstructionOperand operand = {
      .type = REAL_VARIABLE_TYPE,
      .value = {.real_variable = {.frame = LF, .symbol = symbol}}};
  print_DEFVAR(operand);
}

InstructionOperand create_local_variable(const char *name) {
  InstructionOperand operand = {.type = COMPILER_VAR_TYPE,
                                .value = {.compiler_variable = {
                                              .frame = LF,
                                              .name = name,
                                              .suffix = variable_suffix_counter,
                                          }}};
  variable_suffix_counter++;
  return operand;
}

int reserve_label_id(void) { return label_suffix_counter++; }

void trash_return_value(void) { print_POPS(TRASH_GLOBAL); }

void handle_block(const ASTNode *node, bool discard_return_value) {
  for (size_t i = 0; i < node->data.block.statement_count - 1; i++) {
    ASTNode *statement = node->data.block.statements[i];
    ASTNodeHandler handler = ast_handler_for(statement);
    handler(statement, true);
  }

  // keep the last statemen return value on the data stack
  ASTNode *statement =
      node->data.block.statements[node->data.block.statement_count - 1];
  ASTNodeHandler handler = ast_handler_for(statement);
  handler(statement, discard_return_value);
}

void handle_asign(const ASTNode *node, bool discard_return_value) {
  Symbol *target = node->data.assignment.target;
  ASTNode *value = node->data.assignment.value;

  InstructionOperand variable = {
      .type = REAL_VARIABLE_TYPE,
      .value = {.real_variable = {.frame = LF, .symbol = target}}};

  ASTNodeHandler handler = ast_handler_for(value);
  handler(value, false);

  print_POPS(variable);

  if (!discard_return_value) {
    InstructionOperand nil = {
        .type = NIL_TYPE,
    };
    print_PUSHS(nil);
  }
}

void handle_variable_ref(const ASTNode *node, bool discard_return_value) {
  Symbol *symbol = node->data.variable.symbol;

  InstructionOperand operand = {
      .type = REAL_VARIABLE_TYPE,
      .value = {.real_variable = {.frame = LF, .symbol = symbol}}};

  if (!discard_return_value) {
    print_PUSHS(operand);
  }
}

void handle_int(const ASTNode *node, bool discard_return_value) {
  InstructionOperand operand = {.type = INT_TYPE,
                                .value = {.integer = node->data.int_value}};

  if (!discard_return_value) {
    print_PUSHS(operand);
  }
}

void handle_real(const ASTNode *node, bool discard_return_value) {
  InstructionOperand operand = {.type = REAL_TYPE,
                                .value = {.real = node->data.real_value}};

  if (!discard_return_value) {
    print_PUSHS(operand);
  }
}

void handle_string(const ASTNode *node, bool discard_return_value) {
  InstructionOperand operand = {.type = STRING_TYPE,
                                .value = {.string = node->data.string_value}};

  if (!discard_return_value) {
    print_PUSHS(operand);
  }
}

void handle_bool(const ASTNode *node, bool discard_return_value) {
  InstructionOperand operand = {.type = BOOL_TYPE,
                                .value = {.boolean = node->data.bool_value}};

  if (!discard_return_value) {
    print_PUSHS(operand);
  }
}

void handle_unit(const ASTNode *node, bool discard_return_value) {
  (void)node;
  InstructionOperand operand = {
      .type = NIL_TYPE,
  };

  if (!discard_return_value) {
    print_PUSHS(operand);
  }
}

void resolve_binary_operands(const ASTNode *node) {
  ASTNode *left = node->data.binary_op.left;
  ASTNode *right = node->data.binary_op.right;
  ASTNodeHandler handlerLeft = ast_handler_for(left);
  ASTNodeHandler handlerRight = ast_handler_for(right);

  handlerLeft(left, false);
  handlerRight(right, false);
}

void handle_add(const ASTNode *node, bool discard_return_value) {
  resolve_binary_operands(node);

  if (node->inferred_type.type.base == TYPE_STRING) {
    print_POPS(TRASH_GLOBAL);
    print_POPS(TMP_GLOBAL);
    print_CONCAT(TMP_GLOBAL, TMP_GLOBAL, TRASH_GLOBAL);
  } else {
    print_ADDS();
  }

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_subtract(const ASTNode *node, bool discard_return_value) {
  resolve_binary_operands(node);
  print_SUBS();

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_multiply(const ASTNode *node, bool discard_return_value) {
  resolve_binary_operands(node);
  print_MULS();

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_divide(const ASTNode *node, bool discard_return_value) {
  resolve_binary_operands(node);

  switch (node->inferred_type.type.base) {
  case TYPE_REAL:
    print_DIVS();
    break;
  case TYPE_INT:
    print_IDIVS();
    break;
  default:
    ERROR_ORIGIN_AT(node->line, node->column, NULL,
                    "internal error: division of operands that are neither Int "
                    "nor Double");
    codegen_abort();
  }

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_equal(const ASTNode *node, bool discard_return_value) {
  resolve_binary_operands(node);
  print_EQS();

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_not_equal(const ASTNode *node, bool discard_return_value) {
  resolve_binary_operands(node);
  print_EQS();
  print_NOTS();

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_less(const ASTNode *node, bool discard_return_value) {
  resolve_binary_operands(node);
  print_LTS();

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_less_equal(const ASTNode *node, bool discard_return_value) {
  resolve_binary_operands(node);
  print_GTS();
  print_NOTS();

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_greater(const ASTNode *node, bool discard_return_value) {
  resolve_binary_operands(node);
  print_GTS();

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_greater_equal(const ASTNode *node, bool discard_return_value) {
  resolve_binary_operands(node);
  print_LTS();
  print_NOTS();

  if (discard_return_value) {
    trash_return_value();
  }
}

void short_circuit(const ASTNode *node, bool short_value,
                   const char *short_name, const char *end_name) {
  unsigned int id = reserve_label_id();

  InstructionOperand shortOp = {.type = BOOL_TYPE,
                                .value = {.boolean = short_value}};

  const ASTNode *left = node->data.binary_op.left;
  const ASTNodeHandler handlerLeft = ast_handler_for(left);
  handlerLeft(left, false);

  print_JUMPIFEQS_bool(short_name, id, short_value);

  const ASTNode *right = node->data.binary_op.right;
  const ASTNodeHandler handlerRight = ast_handler_for(right);
  handlerRight(right, false);

  print_JUMP(end_name, id);

  print_LABEL(short_name, id);
  print_PUSHS(shortOp);
  print_LABEL(end_name, id);
}

void handle_and(const ASTNode *node, bool discard_return_value) {
  short_circuit(node, false, "AND_SHORT", "AND_END");

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_or(const ASTNode *node, bool discard_return_value) {
  short_circuit(node, true, "OR_SHORT", "OR_END");

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_not(const ASTNode *node, bool discard_return_value) {
  resolve_binary_operands(node);
  print_NOTS();

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_implicit_int_to_real(const ASTNode *node,
                                 bool discard_return_value) {
  ASTNode *operand = node->data.conversion.operand;

  ASTNodeHandler handler = ast_handler_for(operand);
  handler(operand, false);

  print_INT2FLOATS();

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_call(const ASTNode *node, bool discard_return_value) {
  for (size_t i = 0; i < node->data.call.argument_count; i++) {
    const ASTNode *argument = node->data.call.arguments[i];
    ASTNodeHandler handler = ast_handler_for(argument);
    handler(argument, false);
  }

  print_CALL(node->data.call.function->name);

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_return(const ASTNode *node, bool discard_return_value) {
  (void)discard_return_value;
  const ASTNode *return_expression = node->data.return_statement.value;
  ASTNodeHandler handler = ast_handler_for(return_expression);
  handler(return_expression, false);
  print_POPFRAME();
  print_RETURN();
}

void handle_function_definition(const FunctionSymbol *function) {
  current_function = function;
  printf("\n");
  print_LABEL(function->name, 0);

  print_CREATEFRAME();
  print_PUSHFRAME();

  // Hoist local variables definitions to the start of the function to avoid
  // issues with redeclaration
  SymbolArray hoisted_symbols;
  if (symbol_table_get_scope_symbol_list(function->local_scope,
                                         &hoisted_symbols) != SYMTABLE_OK) {
    ERROR_INTERNAL("failed to list the local variables of function '%s'",
                   function->name);
    codegen_abort();
  }
  for (size_t i = 0; i < hoisted_symbols.capacity; i++) {
    pair_real_var(hoisted_symbols.data[i]);
  }
  free(hoisted_symbols.data);

  for (size_t i = 0; i < function->argument_count; i++) {
    Symbol *parameter = function->parameters[function->argument_count - 1 - i];
    InstructionOperand operand = {
        .type = REAL_VARIABLE_TYPE,
        .value = {.real_variable = {.frame = LF, .symbol = parameter}}};

    print_POPS(operand);
  }

  ASTNodeHandler handler = ast_handler_for(function->ast_root);
  handler(function->ast_root, false);
  current_function = NULL;
}

void handle_list_constructor(const ASTNode *node, bool discard_return_value) {
  const size_t data_size = node->data.call.argument_count;
  const size_t real_size = data_size + 1;

  const InstructionOperand real_size_op = {.type = INT_TYPE,
                                           .value = {
                                               .integer = real_size,
                                           }};

  print_NEWARRAY(LIST_GLOBAL_A, real_size_op);

  InstructionOperand data_size_op = {.type = INT_TYPE,
                                     .value = {
                                         .integer = data_size,
                                     }};

  // Save the array size as its first element
  print_ARRAYSET(LIST_GLOBAL_A, INT_ZERO_OPERAND, data_size_op);

  for (size_t i = 0; i < data_size; i++) {
    InstructionOperand index = {.type = INT_TYPE,
                                .value = {
                                    .integer = i + 1,
                                }};
    const ASTNode *element = node->data.call.arguments[i];
    const ASTNodeHandler element_handler = ast_handler_for(element);

    // Save the list, the nested evaluation may overwrite the global
    print_PUSHS(LIST_GLOBAL_A);
    element_handler(element, false);

    // The element is on top, take it first, then restore the list
    print_POPS(LIST_ELEMENT_GLOBAL_A);
    print_POPS(LIST_GLOBAL_A);
    print_ARRAYSET(LIST_GLOBAL_A, index, LIST_ELEMENT_GLOBAL_A);
  }

  if (!discard_return_value) {
    print_PUSHS(LIST_GLOBAL_A);
  }
}

void handle_list_indexing(const ASTNode *node, bool discard_return_value) {
  int id = reserve_label_id();
  resolve_binary_operands(node);

  print_POPS(LIST_INDEX_GLOBAL_A);
  print_POPS(LIST_GLOBAL_A);
  print_ARRAYGET(LIST_SIZE_GLOBAL_A, LIST_GLOBAL_A, INT_ZERO_OPERAND);

  // Shift index by one, the zeroth element is the size of the array
  print_ADD(LIST_INDEX_GLOBAL_A, LIST_INDEX_GLOBAL_A, INT_ONE_OPERAND);

  // Check if index is above lower bound. The upper bound is checked by the
  // interpret.
  print_GT(COND_GLOBAL, LIST_INDEX_GLOBAL_A, INT_ZERO_OPERAND);
  print_JUMPIFEQ("INDEX_OK", id, COND_GLOBAL, BOOL_TRUE_OPERAND);
  print_EXIT(58);

  print_LABEL("INDEX_OK", id);

  print_ARRAYGET(LIST_ELEMENT_GLOBAL_A, LIST_GLOBAL_A, LIST_INDEX_GLOBAL_A);
  if (!discard_return_value) {
    print_PUSHS(LIST_ELEMENT_GLOBAL_A);
  }
}

void handle_list_concat(const ASTNode *node, bool discard_return_value) {
  resolve_binary_operands(node);
  // Load the arrays
  print_POPS(LIST_GLOBAL_B);
  print_POPS(LIST_GLOBAL_A);

  // Load the sizes
  print_ARRAYGET(LIST_SIZE_GLOBAL_A, LIST_GLOBAL_A, INT_ZERO_OPERAND);
  print_ARRAYGET(LIST_SIZE_GLOBAL_B, LIST_GLOBAL_B, INT_ZERO_OPERAND);

  // Setup the result array
  print_ADD(LIST_SIZE_GLOBAL_RES, LIST_SIZE_GLOBAL_A, LIST_SIZE_GLOBAL_B);
  print_ADD(LIST_SIZE_GLOBAL_RES, LIST_SIZE_GLOBAL_RES, INT_ONE_OPERAND);
  print_NEWARRAY(LIST_GLOBAL_RES, LIST_SIZE_GLOBAL_RES);
  // Set array data size
  print_SUB(LIST_SIZE_GLOBAL_RES, LIST_SIZE_GLOBAL_RES, INT_ONE_OPERAND);
  print_ARRAYSET(LIST_GLOBAL_RES, INT_ZERO_OPERAND, LIST_SIZE_GLOBAL_RES);

  // setup the indicies
  print_MOVE(LIST_INDEX_GLOBAL_RES, INT_ONE_OPERAND);
  print_MOVE(LIST_INDEX_GLOBAL_A, INT_ONE_OPERAND);
  print_MOVE(LIST_INDEX_GLOBAL_B, INT_ONE_OPERAND);

  // Increment source sizes, otherwise the last element is skipped.
  print_ADD(LIST_SIZE_GLOBAL_A, LIST_SIZE_GLOBAL_A, INT_ONE_OPERAND);
  print_ADD(LIST_SIZE_GLOBAL_B, LIST_SIZE_GLOBAL_B, INT_ONE_OPERAND);

  // loop throuth array A and move its elements to the result array
  int id = reserve_label_id();
  print_LABEL("LIST_CONCAT_A_START", id);
  // End loop
  print_JUMPIFEQ("LIST_CONCAT_A_END", id, LIST_INDEX_GLOBAL_A,
                 LIST_SIZE_GLOBAL_A);

  // Move the element from A to res
  print_ARRAYGET(LIST_ELEMENT_GLOBAL_RES, LIST_GLOBAL_A, LIST_INDEX_GLOBAL_A);
  print_ARRAYSET(LIST_GLOBAL_RES, LIST_INDEX_GLOBAL_RES,
                 LIST_ELEMENT_GLOBAL_RES);

  // Increment indicies
  print_ADD(LIST_INDEX_GLOBAL_A, LIST_INDEX_GLOBAL_A, INT_ONE_OPERAND);
  print_ADD(LIST_INDEX_GLOBAL_RES, LIST_INDEX_GLOBAL_RES, INT_ONE_OPERAND);
  print_JUMP("LIST_CONCAT_A_START", id);
  print_LABEL("LIST_CONCAT_A_END", id);

  // loop throuth array B and move its elements to the result array
  print_LABEL("LIST_CONCAT_B_START", id);
  // End loop
  print_JUMPIFEQ("LIST_CONCAT_B_END", id, LIST_INDEX_GLOBAL_B,
                 LIST_SIZE_GLOBAL_B);

  // Move the element from B to res
  print_ARRAYGET(LIST_ELEMENT_GLOBAL_RES, LIST_GLOBAL_B, LIST_INDEX_GLOBAL_B);
  print_ARRAYSET(LIST_GLOBAL_RES, LIST_INDEX_GLOBAL_RES,
                 LIST_ELEMENT_GLOBAL_RES);

  // Increment indicies
  print_ADD(LIST_INDEX_GLOBAL_B, LIST_INDEX_GLOBAL_B, INT_ONE_OPERAND);
  print_ADD(LIST_INDEX_GLOBAL_RES, LIST_INDEX_GLOBAL_RES, INT_ONE_OPERAND);
  print_JUMP("LIST_CONCAT_B_START", id);
  print_LABEL("LIST_CONCAT_B_END", id);

  if (!discard_return_value) {
    print_PUSHS(LIST_GLOBAL_RES);
  }
}

static void check_zero_step(void) {
  const int id = reserve_label_id();
  print_JUMPIFNEQ("STEP_OK", id, BY_GLOBAL, INT_ZERO_OPERAND);
  print_EXIT(40);
  print_LABEL("STEP_OK", id);
}

static void create_array_from_range(bool inclusive) {
  const int id = reserve_label_id();

  // Check if the step is positive or negative
  print_LT(COND_GLOBAL, BY_GLOBAL, INT_ZERO_OPERAND);
  print_JUMPIFEQ("RANGE_NEG", id, COND_GLOBAL, BOOL_TRUE_OPERAND);

  // POSITIVE STEP
  // Handle until having the upper bound exclusive
  if (!inclusive) {
    // Skip the last iteration
    print_SUB(TO_GLOBAL, TO_GLOBAL, INT_ONE_OPERAND);
  }

  // Calculate DISTANCE = TO - FROM
  print_SUB(TO_GLOBAL, TO_GLOBAL, FROM_GLOBAL);
  // Save the step in temp variable
  print_MOVE(TMP_GLOBAL, BY_GLOBAL);
  print_JUMP("RANGE_DIST", id);

  // NEGATIVE STEP
  // Handle until having the upper bound exclusive
  print_LABEL("RANGE_NEG", id);
  if (!inclusive) {
    // Skip the last iteration
    print_ADD(TO_GLOBAL, TO_GLOBAL, INT_ONE_OPERAND);
  }
  // Calculate DISTANCE = FROM - TO
  print_SUB(TO_GLOBAL, FROM_GLOBAL, TO_GLOBAL);
  // Invert the step to positive, save in temp var
  print_SUB(TMP_GLOBAL, INT_ZERO_OPERAND, BY_GLOBAL);

  print_LABEL("RANGE_DIST", id);
  // TO holds the distance. If it is negative, the list is empty.
  print_LT(COND_GLOBAL, TO_GLOBAL, INT_ZERO_OPERAND);
  // Set the size to zero, in case the list is empty.
  print_MOVE(LIST_SIZE_GLOBAL_RES, INT_ZERO_OPERAND);
  print_JUMPIFEQ("RANGE_ALLOC", id, COND_GLOBAL, BOOL_TRUE_OPERAND);

  // TO holds distance, TMP holds the absolute value of step
  // Calculate the number of elements in the range
  print_IDIV(LIST_SIZE_GLOBAL_RES, TO_GLOBAL, TMP_GLOBAL);
  // Add one to the size, because the range is inclusive
  print_ADD(LIST_SIZE_GLOBAL_RES, LIST_SIZE_GLOBAL_RES, INT_ONE_OPERAND);

  print_LABEL("RANGE_ALLOC", id);
  // Add one for zeroth element to hold array size
  // TMP holds the real size
  print_ADD(TMP_GLOBAL, LIST_SIZE_GLOBAL_RES, INT_ONE_OPERAND);
  // Create new array
  print_NEWARRAY(LIST_GLOBAL_RES, TMP_GLOBAL);
  // Save the data size
  print_ARRAYSET(LIST_GLOBAL_RES, INT_ZERO_OPERAND, LIST_SIZE_GLOBAL_RES);

  // Start iteration at index 1
  print_MOVE(LIST_INDEX_GLOBAL_RES, INT_ONE_OPERAND);
  // Load start value
  print_MOVE(LIST_ELEMENT_GLOBAL_RES, FROM_GLOBAL);

  print_LABEL("RANGE_FILL", id);
  // End if beyond bounds
  print_JUMPIFEQ("RANGE_END", id, LIST_INDEX_GLOBAL_RES, TMP_GLOBAL);
  // Set the value
  print_ARRAYSET(LIST_GLOBAL_RES, LIST_INDEX_GLOBAL_RES,
                 LIST_ELEMENT_GLOBAL_RES);

  // Calculate next value
  print_ADD(LIST_ELEMENT_GLOBAL_RES, LIST_ELEMENT_GLOBAL_RES, BY_GLOBAL);
  // Increment counter
  print_ADD(LIST_INDEX_GLOBAL_RES, LIST_INDEX_GLOBAL_RES, INT_ONE_OPERAND);
  print_JUMP("RANGE_FILL", id);
  print_LABEL("RANGE_END", id);
}

// this processes the range and pushes the data into the globals
static void load_range_data(const ASTNode *range, const ASTNode *step) {
  resolve_binary_operands(range);

  if (step != NULL) {
    ast_handler_for(step)(step, false);
    print_POPS(BY_GLOBAL);
  } else {
    print_MOVE(BY_GLOBAL, INT_ONE_OPERAND);
  }
  print_POPS(TO_GLOBAL);
  print_POPS(FROM_GLOBAL);
}

void handle_to(const ASTNode *node, bool discard_return_value) {
  load_range_data(node, NULL);
  create_array_from_range(true);

  if (!discard_return_value) {
    print_PUSHS(LIST_GLOBAL_RES);
  }
}

void handle_until(const ASTNode *node, bool discard_return_value) {
  load_range_data(node, NULL);
  create_array_from_range(false);

  if (!discard_return_value) {
    print_PUSHS(LIST_GLOBAL_RES);
  }
}

void handle_by(const ASTNode *node, bool discard_return_value) {
  const ASTNode *left = node->data.binary_op.left;
  const ASTNode *right = node->data.binary_op.right;

  if (left->kind == AST_TO || left->kind == AST_UNTIL) {
    // Left child is TO or UNTIL, right child is the step
    load_range_data(left, right);
    check_zero_step();
    const bool inclusive = left->kind == AST_TO;
    create_array_from_range(inclusive);
  } else {
    // Left child is already a list. Right child is the step
    // The output here is a new list, containg every n-th element
    const int id = reserve_label_id();

    // Load operands, check for zero step
    resolve_binary_operands(node);
    print_POPS(BY_GLOBAL);
    print_POPS(LIST_GLOBAL_A);
    check_zero_step();

    // Load the size of the list
    print_ARRAYGET(LIST_SIZE_GLOBAL_A, LIST_GLOBAL_A, INT_ZERO_OPERAND);

    // Load step into TMP
    print_MOVE(TMP_GLOBAL, BY_GLOBAL);
    // Set the index to the first element
    print_MOVE(LIST_INDEX_GLOBAL_A, INT_ONE_OPERAND);

    // If the step is negative, do:
    print_LT(COND_GLOBAL, TMP_GLOBAL, INT_ZERO_OPERAND);
    print_JUMPIFNEQ("BY_POS", id, COND_GLOBAL, BOOL_TRUE_OPERAND);
    // Invert the step to positive
    print_SUB(TMP_GLOBAL, INT_ZERO_OPERAND, TMP_GLOBAL);
    // Switch the starting index to the last element
    print_MOVE(LIST_INDEX_GLOBAL_A, LIST_SIZE_GLOBAL_A);

    print_LABEL("BY_POS", id);
    // Load 0 the new list size
    print_MOVE(LIST_SIZE_GLOBAL_RES, INT_ZERO_OPERAND);

    // If the original list is empty, the result is an empty list, so skip
    // calculating new size.
    print_JUMPIFEQ("BY_ALLOC", id, LIST_SIZE_GLOBAL_A, INT_ZERO_OPERAND);

    // Calculate the size of the new array. It does this
    // new_size=(size_A-1)/step + 1
    print_SUB(LIST_SIZE_GLOBAL_RES, LIST_SIZE_GLOBAL_A, INT_ONE_OPERAND);
    print_IDIV(LIST_SIZE_GLOBAL_RES, LIST_SIZE_GLOBAL_RES, TMP_GLOBAL);
    print_ADD(LIST_SIZE_GLOBAL_RES, LIST_SIZE_GLOBAL_RES, INT_ONE_OPERAND);
    print_LABEL("BY_ALLOC", id);

    // Create the new array and set its data_size
    print_ADD(TMP_GLOBAL, LIST_SIZE_GLOBAL_RES, INT_ONE_OPERAND);
    print_NEWARRAY(LIST_GLOBAL_RES, TMP_GLOBAL);
    print_ARRAYSET(LIST_GLOBAL_RES, INT_ZERO_OPERAND, LIST_SIZE_GLOBAL_RES);

    // Copy every nth element from the old array to the new array
    // Start indexing at 1
    print_MOVE(LIST_INDEX_GLOBAL_RES, INT_ONE_OPERAND);
    print_LABEL("BY_FILL", id);
    // End if out of bounds
    print_JUMPIFEQ("BY_END", id, LIST_INDEX_GLOBAL_RES, TMP_GLOBAL);

    // Copy the element
    print_ARRAYGET(LIST_ELEMENT_GLOBAL_RES, LIST_GLOBAL_A, LIST_INDEX_GLOBAL_A);
    print_ARRAYSET(LIST_GLOBAL_RES, LIST_INDEX_GLOBAL_RES,
                   LIST_ELEMENT_GLOBAL_RES);

    // Increment indicies
    print_ADD(LIST_INDEX_GLOBAL_A, LIST_INDEX_GLOBAL_A, BY_GLOBAL);
    print_ADD(LIST_INDEX_GLOBAL_RES, LIST_INDEX_GLOBAL_RES, INT_ONE_OPERAND);
    print_JUMP("BY_FILL", id);
    print_LABEL("BY_END", id);
  }

  if (!discard_return_value) {
    print_PUSHS(LIST_GLOBAL_RES);
  }
}

void handle_if(const ASTNode *node, bool discard_return_value) {
  const ASTNode *condition = node->data.if_statement.condition;
  const ASTNode *then_branch = node->data.if_statement.then_branch;
  const ASTNode *else_branch = node->data.if_statement.else_branch;

  const bool is_expression = node->data.if_statement.used_as_an_expression;

  int id = reserve_label_id();

  const ASTNodeHandler handlerCondition = ast_handler_for(condition);
  handlerCondition(condition, false);
  print_JUMPIFEQS_bool("IF_ELSE", id, false);

  if (then_branch != NULL) {
    const ASTNodeHandler handlerThen = ast_handler_for(then_branch);
    handlerThen(then_branch, !is_expression);
  }
  print_JUMP("IF_END", id);

  print_LABEL("IF_ELSE", id);
  if (else_branch != NULL) {
    const ASTNodeHandler handlerElse = ast_handler_for(else_branch);
    handlerElse(else_branch, !is_expression);
  }

  print_LABEL("IF_END", id);

  if (!is_expression && !discard_return_value) {
    InstructionOperand nil = {
        .type = NIL_TYPE,
    };
    print_PUSHS(nil);
  }
}

void handle_while(const ASTNode *node, bool discard_return_value) {
  (void)discard_return_value;
  int id = reserve_label_id();

  const ASTNode *condition = node->data.while_loop.condition;
  const ASTNodeHandler condition_handler = ast_handler_for(condition);

  print_LABEL("WHILE_START", id);
  condition_handler(condition, false);

  print_JUMPIFEQS_bool("WHILE_END", id, false);

  const ASTNode *body = node->data.while_loop.body;
  const ASTNodeHandler body_handler = ast_handler_for(body);
  body_handler(body, true);

  print_JUMP("WHILE_START", id);
  print_LABEL("WHILE_END", id);
}

void handle_for(const ASTNode *node, bool discard_return_value) {
  const int id = reserve_label_id();

  Symbol *symbol = node->data.for_loop.variable;
  const InstructionOperand variable = {
      .type = REAL_VARIABLE_TYPE,
      .value = {.real_variable = {.frame = LF, .symbol = symbol}}};

  // Load the list
  const ASTNode *list = node->data.for_loop.iterable;
  ast_handler_for(list)(list, false);
  // Push starting index
  print_PUSHS(INT_ONE_OPERAND);

  print_LABEL("FOR_START", id);
  // Restore loop state
  print_POPS(LIST_INDEX_GLOBAL_RES);
  print_POPS(LIST_GLOBAL_RES);

  // Check if out of bounds
  print_ARRAYGET(LIST_SIZE_GLOBAL_RES, LIST_GLOBAL_RES, INT_ZERO_OPERAND);
  print_GT(COND_GLOBAL, LIST_INDEX_GLOBAL_RES, LIST_SIZE_GLOBAL_RES);
  print_JUMPIFEQ("FOR_END", id, COND_GLOBAL, BOOL_TRUE_OPERAND);

  // Load the current list element to the control variable
  print_ARRAYGET(variable, LIST_GLOBAL_RES, LIST_INDEX_GLOBAL_RES);

  // Increment index
  print_ADD(LIST_INDEX_GLOBAL_RES, LIST_INDEX_GLOBAL_RES, INT_ONE_OPERAND);
  // Save loop state
  print_PUSHS(LIST_GLOBAL_RES);
  print_PUSHS(LIST_INDEX_GLOBAL_RES);

  // If a guard exists, evaluate it
  const ASTNode *guard = node->data.for_loop.guard;
  if (guard != NULL) {
    ast_handler_for(guard)(guard, false);
    print_JUMPIFEQS_bool("FOR_START", id, false);
  }

  // Evaluate the body
  const ASTNode *body = node->data.for_loop.body;
  ast_handler_for(body)(body, true);

  print_JUMP("FOR_START", id);
  print_LABEL("FOR_END", id);

  if (!discard_return_value) {
    InstructionOperand nil = {.type = NIL_TYPE};
    print_PUSHS(nil);
  }
}

void convert_AST_to_assembler(Program *program) {
  print_prolog();

  for (size_t i = 0; i < GLOBAL_VAR_COUNT; i++) {
    print_DEFVAR(GLOBAL_VARS[i]);
  }

  FunctionSymbol *main = NULL;
  for (size_t i = 0; i < program->functions.function_count; i++) {
    if (program->functions.functions[i]->is_main) {
      main = program->functions.functions[i];
      break;
    }
  }
  if (main == NULL) {
    ERROR_INTERNAL("no main function present");
    codegen_abort();
  }

  print_CALL(main->name);
  print_EXIT(0);

  for (size_t i = 0; i < BUILTIN_COUNT; i++) {
    printf("%s", BUILTINS[i].assembler_code);
  }

  for (size_t i = 0; i < program->functions.function_count; i++) {
    const FunctionSymbol *function = program->functions.functions[i];
    // built in functions are handled separately
    if (!function->is_builtin) {
      handle_function_definition(function);
    }
  }
}