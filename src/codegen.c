#include "codegen.h"
#include "ast.h"
#include "instructions.h"
#include "parser.h"

char *global_var_names[] = {};

// This is such a disqusting solution, i dont like this
static int variable_suffix_counter = 0;

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
    [AST_IF] = NULL,
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

void handle_block(const ASTNode *node) {
  for (size_t i = 0; i < node->data.block.statement_count; i++) {
    ASTNode *statement = node->data.block.statements[i];
    ASTNodeHandler handler = ast_handlers[statement->kind];
    handler(statement);
  }
}

void handle_asign(const ASTNode *node) {
  Symbol *target = node->data.assignment.target;
  ASTNode *value = node->data.assignment.value;

  AssemblerVariable variable = {.frame = LF, .symbol = target};

  if (target->assembler_suffix == -1) {
    target->assembler_suffix = variable_suffix_counter;
    variable_suffix_counter++;

    print_DEFVAR(variable);
  }

  ASTNodeHandler handler = ast_handlers[value->kind];
  handler(value);

  InstructionOperand operand = {.type = VARIABLE_TYPE,
                                .value = {.variable = variable}};

  print_POPS(operand);
}

void handle_variable_ref(const ASTNode *node) {
  Symbol *symbol = node->data.variable.symbol;

  InstructionOperand operand = {
      .type = VARIABLE_TYPE,
      .value = {.variable = {.frame = LF, .symbol = symbol}}};

  print_PUSHS(operand);
}

void handle_int(const ASTNode *node) {
  InstructionOperand operand = {.type = INT_TYPE,
                                .value = {.integer = node->data.int_value}};

  print_PUSHS(operand);
}

void handle_real(const ASTNode *node) {
  InstructionOperand operand = {.type = REAL_TYPE,
                                .value = {.real = node->data.real_value}};

  print_PUSHS(operand);
}

void handle_string(const ASTNode *node) {
  InstructionOperand operand = {.type = STRING_TYPE,
                                .value = {.string = node->data.string_value}};

  print_PUSHS(operand);
}

void handle_bool(const ASTNode *node) {
  InstructionOperand operand = {.type = BOOL_TYPE,
                                .value = {.boolean = node->data.bool_value}};

  print_PUSHS(operand);
}

void handle_unit(const ASTNode *node) {
  (void)node;
  InstructionOperand operand = {
      .type = NIL_TYPE,
  };

  print_PUSHS(operand);
}

void resolve_binary_operands(const ASTNode *node) {
  ASTNode *left = node->data.binary_op.left;
  ASTNode *right = node->data.binary_op.right;
  ASTNodeHandler handlerLeft = ast_handlers[left->kind];
  ASTNodeHandler handlerRight = ast_handlers[right->kind];

  handlerLeft(left);
  handlerRight(right);
}

void handle_add(const ASTNode *node) {
  resolve_binary_operands(node);
  print_ADDS();
}

void handle_subtract(const ASTNode *node) {
  resolve_binary_operands(node);
  print_SUBS();
}

void handle_multiply(const ASTNode *node) {
  resolve_binary_operands(node);
  print_MULS();
}

void handle_divide(const ASTNode *node) {
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
}

void handle_equal(const ASTNode *node) {
  resolve_binary_operands(node);
  print_EQS();
}

void handle_not_equal(const ASTNode *node) {
  resolve_binary_operands(node);
  print_EQS();
  print_NOTS();
}

void handle_less(const ASTNode *node) {
  resolve_binary_operands(node);
  print_LTS();
}

void handle_less_equal(const ASTNode *node) {
  resolve_binary_operands(node);
  print_GTS();
  print_NOTS();
}

void handle_greater(const ASTNode *node) {
  resolve_binary_operands(node);
  print_GTS();
}

void handle_greater_equal(const ASTNode *node) {
  resolve_binary_operands(node);
  print_LTS();
  print_NOTS();
}

void handle_and(const ASTNode *node) {
  resolve_binary_operands(node);
  print_ANDS();
}

void handle_or(const ASTNode *node) {
  resolve_binary_operands(node);
  print_ORS();
}

void handle_not(const ASTNode *node) {
  resolve_binary_operands(node);
  print_NOTS();
}

void handle_implicit_int_to_real(const ASTNode *node) {
  // node->data.conversion.operand
  ASTNode *operand = node->data.conversion.operand;

  ASTNodeHandler handler = ast_handlers[operand->kind];
  handler(operand);

  print_INT2FLOATS();
}

void handle_call(const ASTNode *node) {
  for (size_t i = 0; i < node->data.call.argument_count; i++) {
    const ASTNode *argument = node->data.call.arguments[i];
    ASTNodeHandler handler = ast_handlers[argument->kind];
    handler(argument);
  }

  print_CALL(node->data.call.function->name);
}

void handle_return(const ASTNode *node) {
  const ASTNode *return_expression = node->data.return_statement.value;
  ASTNodeHandler handler = ast_handlers[return_expression->kind];
  handler(return_expression);
  print_POPFRAME();
  print_RETURN();
}

void handle_function_definition(const FunctionSymbol *function) {
  print_LABEL(function->name);

  print_CREATEFRAME();
  print_PUSHFRAME();

  // TODO implement argument loading - waiting for update to FunctionSymbol
  // definition to add argument list
  for (size_t i = 0; i < function->argument_count; i++) {
    // InstructionOperand operand = {
    //   .type = VARIABLE_TYPE,
    //   .value = {
    //     .variable = {
    //       .frame = LF,
    //       .symbol = function.
    //     }
    //   }
    // }
  }

  handle_block(function->ast_root);
}

void convert_AST_to_assembler(Program *program) {
  print_prolog();

  // TODO global var definitions

  FunctionSymbol *main = NULL;
  for (size_t i = 0; i < program->functions.function_count; i++) {
    if (program->functions.functions[i]->is_main) {
      main = program->functions.functions[i];
      break;
    }
  }
  if (main == NULL) {
    // TODO error, but shouldnt happen
    return;
  }

  print_CALL(main->name);
  print_EXIT(0);

  for (size_t i = 0; i < program->functions.function_count; i++) {
    const FunctionSymbol *function = program->functions.functions[i];
    handle_function_definition(function);
  }
}