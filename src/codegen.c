#include "codegen.h"
#include "ast.h"
#include "builtins.h"
#include "instructions.h"
#include "parser.h"
#include <string.h>

static const char *global_var_names[] = {"trash"};

static const size_t global_var_count =
    sizeof(global_var_names) / sizeof(global_var_names[0]);

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

void pair_real_var(InstructionOperand operand, Symbol *symbol) {
  if (operand.type != REAL_VARIABLE_TYPE) {
    fprintf(stderr, "pair_real_var - invalid operand type\n");
    exit(1);
  }

  if (symbol->assembler_suffix == -1) {
    symbol->assembler_suffix = variable_suffix_counter;
    variable_suffix_counter++;

    print_DEFVAR(operand);
  }
}

int reserve_label_id(void) { return label_suffix_counter++; }

void trash_return_value(void) {
  InstructionOperand operand = {
      .type = COMPILER_VAR_TYPE,
      .value = {.compiler_variable = {.frame = GF, .name = "trash"}}};
  print_POPS(operand);
}

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

void handle_if(const ASTNode *node, bool discard_return_value) {
  const ASTNode *condition = node->data.if_statement.condition;
  const ASTNode *then_branch = node->data.if_statement.then_branch;
  const ASTNode *else_branch = node->data.if_statement.else_branch;

  const bool is_expression = node->data.if_statement.used_as_an_expression;

  int else_label_id = reserve_label_id();
  int end_label_id = reserve_label_id();

  const ASTNodeHandler handlerCondition = ast_handlers[condition->kind];
  handlerCondition(condition, false);
  print_JUMPIFEQS_bool("IF_ELSE", else_label_id, false);

  if (then_branch != NULL) {
    const ASTNodeHandler handlerThen = ast_handlers[then_branch->kind];
    handlerThen(then_branch, !is_expression);
  }
  print_JUMP("IF_END", end_label_id);

  print_LABEL("IF_ELSE", else_label_id);
  if (else_branch != NULL) {
    const ASTNodeHandler handlerElse = ast_handlers[else_branch->kind];
    handlerElse(else_branch, !is_expression);
  }

  print_LABEL("IF_END", end_label_id);

  if (!is_expression && !discard_return_value) {
    InstructionOperand nil = {
        .type = NIL_TYPE,
    };
    print_PUSHS(nil);
  }
}

void convert_AST_to_assembler(Program *program) {
  print_prolog();

  for (size_t i = 0; i < global_var_count; i++) {
    const InstructionOperand operand = {
        .type = COMPILER_VAR_TYPE,
        .value = {
            .compiler_variable = {.frame = GF, .name = global_var_names[i]}}};

    print_DEFVAR(operand);
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