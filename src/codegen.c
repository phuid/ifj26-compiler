#include "codegen.h"
#include "ast.h"
#include "builtins.h"
#include "codegen_globals.h"
#include "instructions.h"
#include "parser.h"
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
    [AST_LIST_CONSTRUCTOR] = NULL,
    [AST_LIST_INDEXING] = NULL,
    [AST_LIST_CONCAT] = NULL,
    [AST_TO] = NULL,
    [AST_UNTIL] = NULL,
    [AST_BY] = NULL,
    [AST_ASSIGN] = handle_asign,
    [AST_RETURN] = handle_return,
    [AST_IF] = handle_if,
    [AST_WHILE] = handle_while,
    [AST_FOR] = NULL,
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

void pair_real_var(InstructionOperand operand, Symbol *symbol) {
  if (operand.type != REAL_VARIABLE_TYPE) {
    fprintf(stderr, "pair_real_var - invalid operand type\n");
    exit(1);
  }

  if (symbol->assembler_suffix == -1) {
    symbol->assembler_suffix = variable_suffix_counter;
    variable_suffix_counter++;

    // TODO this will likely change
    print_DEFVAR(operand);
  }
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
    ASTNodeHandler handler = ast_handlers[statement->kind];
    handler(statement, true);
  }

  // keep the last statemen return value on the data stack
  ASTNode *statement =
      node->data.block.statements[node->data.block.statement_count - 1];
  ASTNodeHandler handler = ast_handlers[statement->kind];
  handler(statement, discard_return_value);
}

void handle_asign(const ASTNode *node, bool discard_return_value) {
  Symbol *target = node->data.assignment.target;
  ASTNode *value = node->data.assignment.value;

  InstructionOperand variable = {
      .type = REAL_VARIABLE_TYPE,
      .value = {.real_variable = {.frame = LF, .symbol = target}}};

  pair_real_var(variable, target);

  ASTNodeHandler handler = ast_handlers[value->kind];
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
  ASTNodeHandler handlerLeft = ast_handlers[left->kind];
  ASTNodeHandler handlerRight = ast_handlers[right->kind];

  handlerLeft(left, false);
  handlerRight(right, false);
}

void handle_add(const ASTNode *node, bool discard_return_value) {
  resolve_binary_operands(node);
  print_ADDS();

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
    fprintf(stderr, "handle_divide - division of invalid types - parser error, "
                    "shouldn happen\n");
    exit(1);
    break;
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
  const ASTNodeHandler handlerLeft = ast_handlers[left->kind];
  handlerLeft(left, false);

  print_JUMPIFEQS_bool(short_name, id, short_value);

  const ASTNode *right = node->data.binary_op.right;
  const ASTNodeHandler handlerRight = ast_handlers[right->kind];
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
  // node->data.conversion.operand
  ASTNode *operand = node->data.conversion.operand;

  ASTNodeHandler handler = ast_handlers[operand->kind];
  handler(operand, false);

  print_INT2FLOATS();

  if (discard_return_value) {
    trash_return_value();
  }
}

void handle_call(const ASTNode *node, bool discard_return_value) {
  for (size_t i = 0; i < node->data.call.argument_count; i++) {
    const ASTNode *argument = node->data.call.arguments[i];
    ASTNodeHandler handler = ast_handlers[argument->kind];
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
  ASTNodeHandler handler = ast_handlers[return_expression->kind];
  handler(return_expression, false);
  print_POPFRAME();
  print_RETURN();
}

void handle_function_definition(const FunctionSymbol *function) {
  printf("\n");
  print_LABEL(function->name, 0);

  print_CREATEFRAME();
  print_PUSHFRAME();

  // TODO traverse the entire symtable tree and defvar all variables at the
  // start

  // TODO dont create the variables, only asign into them
  for (size_t i = 0; i < function->argument_count; i++) {
    Symbol *parameter = function->parameters[function->argument_count - 1 - i];
    InstructionOperand operand = {
        .type = REAL_VARIABLE_TYPE,
        .value = {.real_variable = {.frame = LF, .symbol = parameter}}};

    pair_real_var(operand, parameter);
    print_POPS(operand);
  }

  handle_block(function->ast_root, false);
}

// [AST_LIST_CONSTRUCTOR] = NULL,
// [AST_LIST_INDEXING] = NULL,
// [AST_LIST_CONCAT] = NULL,
// [AST_TO] = NULL,
// [AST_UNTIL] = NULL,
// [AST_BY] = NULL,

void handle_list_constructor(const ASTNode *node, bool discard_return_value) {
  // TODO this will propably need to be unified with the other ways to make a
  // list; the zeroth element will be array will be its size
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
    const ASTNodeHandler element_handler = ast_handlers[element->kind];

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
  resolve_binary_operands(node);

  // Shift index by one, the zeroth element is the size of the array
  print_POPS(LIST_INDEX_GLOBAL_A);
  // TODO add bounds checking - the upper bound is checked by interpret, but
  // index -1 would pass because of the zeroth element
  print_ADD(LIST_INDEX_GLOBAL_A, LIST_INDEX_GLOBAL_A, INT_ONE_OPERAND);

  print_POPS(LIST_GLOBAL_A);

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

void handle_to(const ASTNode *node, bool discard_return_value) {
  // resolve the bounds
  resolve_binary_operands(node);
  // add implicit step of 1
  print_PUSHS(INT_ONE_OPERAND);

  if (discard_return_value) {
    trash_return_value();
    trash_return_value();
    trash_return_value();
  }
}

void handle_until(const ASTNode *node, bool discard_return_value) {
  // resolve bounds
  resolve_binary_operands(node);
  // subtract 1 from upper bound to make it exclusive
  print_PUSHS(INT_ONE_OPERAND);
  print_SUBS();
  // add implicit step of
  print_PUSHS(INT_ONE_OPERAND);

  if (discard_return_value) {
    trash_return_value();
    trash_return_value();
    trash_return_value();
  }
}

void handle_by(const ASTNode *node, bool discard_return_value) {
  // resolve bounds
  const ASTNode *range = node->data.binary_op.left;
  const ASTNode *by = node->data.binary_op.right;

  const ASTNodeHandler range_handler = ast_handlers[range->kind];
  range_handler(range, false);

  // discard the implicit step of 1
  trash_return_value();

  // resolve the new step
  const ASTNodeHandler by_handler = ast_handlers[by->kind];
  by_handler(by, false);
  // TODO step of 0 should produce runtime error

  if (discard_return_value) {
    trash_return_value();
    trash_return_value();
    trash_return_value();
  }
}

void handle_if(const ASTNode *node, bool discard_return_value) {
  const ASTNode *condition = node->data.if_statement.condition;
  const ASTNode *then_branch = node->data.if_statement.then_branch;
  const ASTNode *else_branch = node->data.if_statement.else_branch;

  const bool is_expression = node->data.if_statement.used_as_an_expression;

  int id = reserve_label_id();

  const ASTNodeHandler handlerCondition = ast_handlers[condition->kind];
  handlerCondition(condition, false);
  print_JUMPIFEQS_bool("IF_ELSE", id, false);

  if (then_branch != NULL) {
    const ASTNodeHandler handlerThen = ast_handlers[then_branch->kind];
    handlerThen(then_branch, !is_expression);
  }
  print_JUMP("IF_END", id);

  print_LABEL("IF_ELSE", id);
  if (else_branch != NULL) {
    const ASTNodeHandler handlerElse = ast_handlers[else_branch->kind];
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
  (void)discard_return_value; // TODO is this correct?
  int id = reserve_label_id();

  const ASTNode *condition = node->data.while_loop.condition;
  const ASTNodeHandler condition_handler = ast_handlers[condition->kind];

  print_LABEL("WHILE_START", id);
  condition_handler(condition, false);

  print_JUMPIFEQS_bool("WHILE_END", id, false);

  const ASTNode *body = node->data.while_loop.body;
  const ASTNodeHandler body_handler = ast_handlers[body->kind];
  body_handler(body, true);

  print_JUMP("WHILE_START", id);
  print_LABEL("WHILE_END", id);
}

void handle_for(const ASTNode *node, bool discard_return_value) {
  const int id = reserve_label_id();

  // resolve iterable
  const ASTNode *iterable = node->data.for_loop.iterable;
  const ASTNodeHandler handler = ast_handlers[iterable->kind];
  handler(iterable, false);

  bool is_range = false;
  // this is the only way it could be a range
  if (iterable->kind == AST_BY || iterable->kind == AST_UNTIL ||
      iterable->kind == AST_TO) {
    is_range = true;
  }

  // if its a list, push the index
  if (!is_range) {
    print_PUTS(INT_ONE_OPERAND);
  }

  print_LABEL("FOR_START", id);
  // Load the list iterable
  if (is_range) {
    print_POPS(BY_GLOBAL);
    print_POPS(TO_GLOBAL);
    print_POPS(FROM_GLOBAL);
  } else {
    print_POPS(LIST_INDEX_GLOBAL_RES);
    print_POPS(LIST_GLOBAL_RES);
  }

  // Check bounds
  if (is_range) {
    // TODO CONTINUE HERE
  }

  // TODO check if in array bounds

  // TODO load the element from arrary at index to the variable,
  const ASTNode *variable = node->data.for_loop.variable;

  // TODO evaluate guards

  const ASTNode *guard = node->data.for_loop.guard;

  // TODO the iterated list and index needs to be pushed to the data stack befor
  // the body runs and retrieved after it runs.

  // TODO handle the body
  const ASTNode *body = node->data.for_loop.body;

  // TODO pop the array and index
  // TODO jump to start
  // TODO end label
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
    fprintf(stderr, "no main function presesnt\n");
    exit(1);
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