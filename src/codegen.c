#include "codegen.h"
#include "ast.h"
#include "parser.h"
#include <stdarg.h>

char *global_var_names[] = {};

// This is such a disqusting solution, i dont like this
static int variable_suffix_counter = 0;

static const ASTNodeHandler ast_handlers[] = {
    [AST_BLOCK] = handle_block,
    [AST_CONSTANT_INT] = handle_int,
    [AST_CONSTANT_REAL] = handle_real,
    [AST_CONSTANT_STRING] = NULL,
    [AST_VARIABLE_REF] = handle_variable_ref,
    [AST_FUNCTION_CALL] = handle_call,
    [AST_ASSIGN] = handle_asign,
    [AST_RETURN] = NULL,
    [AST_IF] = NULL,
    [AST_ADD] = handle_add,
    [AST_SUBTRACT] = handle_sub,
    [AST_MULTIPLY] = handle_mul,
    [AST_DIVIDE] = NULL,
    [AST_IMPLICIT_INT_TO_REAL] = NULL,
};

void print_variable_name(VariableFrame frame, const char *name, int suffix) {
  char *frameStr = frame == TF ? "TF" : (frame == LF ? "LF" : "GF");
  printf("%s@%s$%i", frameStr, name, suffix);
}

void print_LABEL(const char *name) { printf("LABEL %s\n", name); }

void print_DEFVAR(const char *name, VariableFrame frame, int suffix) {
  printf("DEFVAR ");
  print_variable_name(frame, name, suffix);
  printf("\n");
}

void print_PUSHS(const char *name, VariableFrame frame, int suffix) {
  printf("PUSHS ");
  print_variable_name(frame, name, suffix);
  printf("\n");
}

void print_POPS(const char *name, VariableFrame frame, int suffix) {
  printf("POPS ");
  print_variable_name(frame, name, suffix);
  printf("\n");
}

void print_ADDS() { printf("ADDS\n"); }

void print_SUBS() { printf("SUBS\n"); }

void print_MULS() { printf("MULS\n"); }

// TODO division by zero handling?
void print_DIVS() { printf("DIVS\n"); }

// TODO division by zero handling?
void print_IDIVS() { printf("IDIVS\n"); }

void print_CALL(const char *label) { printf("CALL %s\n", label); }

void print_CREATEFRAME() { printf("CREATEFRAME\n"); }

void print_PUSHFRAME() { printf("PUSHFRAME\n"); }

void print_POPFRAME() { printf("POPFRAME\n"); }

void print_RETURN() { printf("RETURN\n"); }

void print_EXIT(int code) { printf("EXIT %i\n", code); }

void handle_block(const ASTNode *node) {
  for (size_t i = 0; i < node->data.block.statement_count; i++) {
    ASTNode *statement = node->data.block.statements[i];
    ASTNodeHandler handler = ast_handlers[statement->kind];
    handler(statement);
  }
}

// Should it also work with TF?
void handle_asign(const ASTNode *node) {
  Symbol *target = node->data.assignment.target;
  ASTNode *value = node->data.assignment.value;

  if (target->assemblerSuffix == -1) {
    print_DEFVAR(target->name, LF, variable_suffix_counter);
    target->assemblerSuffix = variable_suffix_counter;
    variable_suffix_counter++;
  }

  ASTNodeHandler handler = ast_handlers[value->kind];
  handler(value);

  print_POPS(target->name, LF, target->assemblerSuffix);
}

void handle_variable_ref(const ASTNode *node) {
  Symbol *symbol = node->data.variable.symbol;
  print_PUSHS(symbol->name, LF, symbol->assemblerSuffix);
}

void handle_int(const ASTNode *node) {
  printf("PUSHS int@%lld", node->data.int_value);
}

void handle_real(const ASTNode *node) {
  printf("PUSHS float@%a", node->data.real_value);
}

void handle_string(const ASTNode *node) {
  // TODO this is wrong, propably need to handle escaping characters
  exit(1);
  printf("PUSHS string@%s", node->data.string_value);
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

void handle_sub(const ASTNode *node) {
  resolve_binary_operands(node);
  print_SUBS();
}

void handle_mul(const ASTNode *node) {
  resolve_binary_operands(node);
  print_MULS();
}

void handle_div(const ASTNode *node) {
  resolve_binary_operands(node);
  // TODO handle DIVS/IDIVS
  exit(1);
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

  for (int i = function->argument_count - 1; i >= 0; i--) {
    // TODO
    // print_DEFVAR()
    // print_POPS(...)
  }

  for (size_t i = 0; i < function->ast_root->data.block.statement_count; i++) {
    ASTNode *statement = function->ast_root->data.block.statements[i];
    ASTNodeHandler handler = ast_handlers[statement->kind];
    handler(statement);
  }
}

void convert_AST_to_assembler(Program *program) {
  printf(".IFJcode2026");

  // TODO global var definitions

  // TODO what is the name of the main function?
  FunctionSymbol *main = NULL;
  for (size_t i = 0; i < program->functions.function_count; i++) {
    if (program->functions.functions[i]->is_main) {
      main = program->functions.functions[i];
      break;
    }
  }
  if (main == NULL) {
    // TODO error
    return;
  }

  print_CALL(main->name);
  print_EXIT(0);

  for (size_t i = 0; i < program->functions.function_count; i++) {
    const FunctionSymbol *function = program->functions.functions[i];
    handle_function_definition(function);
  }
}