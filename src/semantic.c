/**
 * @file semantic.c
 * @brief Semantic analysis: call graph, recursion rules and type checking.
 * @author Vojtech Ondracek
 * @author Simon Hrouda
 * @note Code in this file was partially generated using an LLM agent based on
 *       specifikace.txt.
 *
 * Why only functions are graph nodes: variables cannot be referenced before
 * their definition (error 3), parameters are always annotated and there are no
 * global variables. A top-down walk of a function body therefore knows the
 * type of every variable at its definition, provided the return types of the
 * called functions are known. Only calls may refer forward (and recursively).
 *
 * A function with an explicit return type is a leaf for type inference (its
 * type is in the header). A function without it must not lie on any cycle of
 * the call graph (spec 2.3), so the dependencies between such functions form
 * a DAG and a memoized DFS (check callee before caller) resolves them in a
 * topological order without building that order explicitly.
 */

#include "semantic.h"

#include "ast.h"
#include "function_table.h"
#include "symtable.h"
#include "type.h"

#include <stdbool.h>
#include <stddef.h>

/** State shared by the whole semantic pass. */
typedef struct {
  Program *program;
  unsigned mark; ///< Last used FunctionSymbol::visit_mark value.
} SemanticContext;

/** Kind of the value a block/statement produces as its result. */
typedef enum {
  RESULT_VALUE,   ///< Ends with a value of @c BodyResult::type.
  RESULT_RETURNS, ///< Every path ends with `return`.
  RESULT_BAD_IF   ///< Ends with an `if` violating rule 3 of spec 2.3.
} ResultKind;

/** Result of a block, i.e. of its last statement (spec 2.3). */
typedef struct {
  ResultKind kind;
  DataType type;
} BodyResult;

/* ------------------------------------------------------------------------- */
/* Builtin functions                                                         */
/* ------------------------------------------------------------------------- */

typedef struct {
  const char *name;
  DataType return_type;
  size_t parameter_count;
  DataType parameters[3];
} BuiltinSignature;

static const BuiltinSignature BUILTINS[] = {
    {"print", TYPE_UNIT, 1U, {TYPE_ANY}},
    {"readLine", TYPE_STRING, 0U, {TYPE_UNKNOWN}},
    {"readInt", TYPE_INT, 0U, {TYPE_UNKNOWN}},
    {"readDouble", TYPE_REAL, 0U, {TYPE_UNKNOWN}},
    {"toStr", TYPE_STRING, 1U, {TYPE_ANY}},
    {"toInt", TYPE_INT, 1U, {TYPE_REAL}},
    {"strLength", TYPE_INT, 1U, {TYPE_STRING}},
    {"subStr", TYPE_STRING, 3U, {TYPE_STRING, TYPE_INT, TYPE_INT}},
    {"ord", TYPE_INT, 2U, {TYPE_STRING, TYPE_INT}},
    {"chr", TYPE_STRING, 1U, {TYPE_INT}},
};

/**
 * @brief Gives signatures to builtins referenced (but not defined) by the
 * program. The parser only created forward references for them.
 */
static SemanticStatus register_builtins(Program *program) {
  size_t index;
  size_t parameter;

  for (index = 0U; index < sizeof(BUILTINS) / sizeof(BUILTINS[0]);
       index += 1U) {
    FunctionSymbol *function =
        function_table_lookup(&program->functions, BUILTINS[index].name);

    /// Reference only when the function is used in the program; otherwise, it
    /// is not an error to leave it undefined.
    if (function == NULL) {
      continue;
    }
    /// A user definition of a builtin name (e.g. `def print(...)`) is a
    /// second definition of that function; overloading is not supported.
    if (function->is_defined) {
      return SEMANTIC_REDEFINITION;
    }
    function->is_defined = true;
    function->is_builtin = true;
    function->has_explicit_return_type = true;
    function->return_type.type = BUILTINS[index].return_type;
    function->argument_count = 0U;
    for (parameter = 0U; parameter < BUILTINS[index].parameter_count;
         parameter += 1U) {
      if (function_symbol_add_parameter(
              function, BUILTINS[index].parameters[parameter]) !=
          FUNCTION_TABLE_OK) {
        return SEMANTIC_INTERNAL_ERROR;
      }
    }
    function->check_state = FUNCTION_CHECKED;
  }
  return SEMANTIC_OK;
}

/* ------------------------------------------------------------------------- */
/* Call graph                                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Collects call-graph edges and `return` occurrences of one body.
 * @details Walks the whole AST, unreachable code included (spec 2.3).
 */
static SemanticStatus collect_graph(FunctionSymbol *function,
                                    const ASTNode *node) {
  size_t index;
  SemanticStatus status = SEMANTIC_OK;

  if (node == NULL) {
    return SEMANTIC_OK;
  }

  switch (node->kind) {
  case AST_FUNCTION_CALL:
    if (function_symbol_add_callee(function, node->data.call.function) !=
        FUNCTION_TABLE_OK) {
      return SEMANTIC_INTERNAL_ERROR;
    }
    for (index = 0U;
         status == SEMANTIC_OK && index < node->data.call.argument_count;
         index += 1U) {
      status = collect_graph(function, node->data.call.arguments[index]);
    }
    return status;
  case AST_BLOCK:
    for (index = 0U;
         status == SEMANTIC_OK && index < node->data.block.statement_count;
         index += 1U) {
      status = collect_graph(function, node->data.block.statements[index]);
    }
    return status;
  case AST_RETURN:
    function->contains_return = true;
    return collect_graph(function, node->data.return_statement.value);
  case AST_ASSIGN:
    return collect_graph(function, node->data.assignment.value);
  case AST_IF:
    status = collect_graph(function, node->data.if_statement.condition);
    if (status == SEMANTIC_OK) {
      status = collect_graph(function, node->data.if_statement.then_branch);
    }
    if (status == SEMANTIC_OK) {
      status = collect_graph(function, node->data.if_statement.else_branch);
    }
    return status;
  case AST_ADD:
  case AST_SUBTRACT:
  case AST_MULTIPLY:
  case AST_DIVIDE:
  case AST_LESS:
  case AST_LESS_EQUAL:
  case AST_GREATER:
  case AST_GREATER_EQUAL:
  case AST_EQUAL:
  case AST_NOT_EQUAL:
    status = collect_graph(function, node->data.binary_op.left);
    if (status == SEMANTIC_OK) {
      status = collect_graph(function, node->data.binary_op.right);
    }
    return status;
  case AST_IMPLICIT_INT_TO_REAL:
    return collect_graph(function, node->data.conversion.operand);
  case AST_CONSTANT_INT:
  case AST_CONSTANT_REAL:
  case AST_CONSTANT_STRING:
  case AST_CONSTANT_UNIT:
  case AST_VARIABLE_REF:
    return SEMANTIC_OK;
  }
  return SEMANTIC_INTERNAL_ERROR;
}

/**
 * @brief Tells whether @p target is reachable from @p from by at least one
 * call edge (i.e. whether @p target lies on a cycle when from == target).
 * @details Plain DFS with visit marks; O(V + E) per query.
 */
static bool reaches(FunctionSymbol *from, const FunctionSymbol *target,
                    unsigned mark) {
  size_t index;

  for (index = 0U; index < from->callee_count; index += 1U) {
    FunctionSymbol *callee = from->callees[index];
    if (callee == target) {
      return true;
    }
    if (callee->visit_mark != mark) {
      callee->visit_mark = mark;
      if (reaches(callee, target, mark)) {
        return true;
      }
    }
  }
  return false;
}

/* ------------------------------------------------------------------------- */
/* Types                                                                     */
/* ------------------------------------------------------------------------- */

static bool is_numeric(DataType type) {
  return type == TYPE_INT || type == TYPE_REAL;
}

/**
 * @brief Makes the expression in @p slot usable where @p to is expected.
 * @details Same type is accepted as is; Int is widened to Double by wrapping
 * the expression in an AST_IMPLICIT_INT_TO_REAL node (the only implicit
 * conversion of IFJ26). Anything else yields @p mismatch.
 */
static SemanticStatus coerce(ASTNode **slot, DataType from, DataType to,
                             SemanticStatus mismatch) {
  ASTNode *conversion;

  if (from == to) {
    return SEMANTIC_OK;
  }
  if (from == TYPE_INT && to == TYPE_REAL) {
    conversion = ast_node_create_implicit_int_to_real(*slot);
    if (conversion == NULL) {
      return SEMANTIC_INTERNAL_ERROR;
    }
    *slot = conversion;
    return SEMANTIC_OK;
  }
  return mismatch;
}

static SemanticStatus check_function(SemanticContext *ctx,
                                     FunctionSymbol *function);

static SemanticStatus check_expression(SemanticContext *ctx, ASTNode **slot,
                                       DataType *out_type);

/** Type-checks a call and yields the callee's return type. */
static SemanticStatus check_call(SemanticContext *ctx, ASTNode *node,
                                 DataType *out_type) {
  FunctionSymbol *callee = node->data.call.function;
  SemanticStatus status;
  size_t index;

  if (callee == NULL || !callee->is_defined) {
    return SEMANTIC_UNDEFINED;
  }
  if (node->data.call.argument_count != callee->argument_count) {
    return SEMANTIC_BAD_CALL;
  }

  /// Arguments only need the callee's signature (known after parsing).
  for (index = 0U; index < node->data.call.argument_count; index += 1U) {
    DataType argument_type;
    DataType parameter_type = callee->parameter_types[index];

    status = check_expression(ctx, &node->data.call.arguments[index],
                              &argument_type);
    if (status != SEMANTIC_OK) {
      return status;
    }
    if (argument_type == TYPE_BOOL) {
      /// Boolean values cannot be passed without BOOLTHEN.
      return SEMANTIC_BAD_CALL;
    }
    if (parameter_type != TYPE_ANY) {
      status = coerce(&node->data.call.arguments[index], argument_type,
                      parameter_type, SEMANTIC_BAD_CALL);
      if (status != SEMANTIC_OK) {
        return status;
      }
    }
  }

  /// The result type is the only real dependency between functions: if the
  /// callee's type has to be inferred, check the callee first (DFS edge).
  if (!callee->has_explicit_return_type) {
    status = check_function(ctx, callee);
    if (status != SEMANTIC_OK) {
      return status;
    }
  }
  *out_type = callee->return_type.type;
  return SEMANTIC_OK;
}

/** Type-checks a binary arithmetic or relational expression. */
static SemanticStatus check_binary(SemanticContext *ctx, ASTNode *node,
                                   DataType *out_type) {
  DataType left;
  DataType right;
  SemanticStatus status;
  bool relational = node->kind != AST_ADD && node->kind != AST_SUBTRACT &&
                    node->kind != AST_MULTIPLY && node->kind != AST_DIVIDE;
  bool equality = node->kind == AST_EQUAL || node->kind == AST_NOT_EQUAL;

  status = check_expression(ctx, &node->data.binary_op.left, &left);
  if (status == SEMANTIC_OK) {
    status = check_expression(ctx, &node->data.binary_op.right, &right);
  }
  if (status != SEMANTIC_OK) {
    return status;
  }

  /// Booleans are not operands in the base language (no chaining of
  /// relational operators, no BOOLTHEN).
  if (left == TYPE_BOOL || right == TYPE_BOOL) {
    return SEMANTIC_TYPE_ERROR;
  }

  if (is_numeric(left) && is_numeric(right)) {
    DataType common =
        (left == TYPE_REAL || right == TYPE_REAL) ? TYPE_REAL : TYPE_INT;
    status =
        coerce(&node->data.binary_op.left, left, common, SEMANTIC_TYPE_ERROR);
    if (status == SEMANTIC_OK) {
      status = coerce(&node->data.binary_op.right, right, common,
                      SEMANTIC_TYPE_ERROR);
    }
    if (status != SEMANTIC_OK) {
      return status;
    }
    *out_type = relational ? TYPE_BOOL : common;
    return SEMANTIC_OK;
  }

  if (left == TYPE_STRING && right == TYPE_STRING) {
    if (node->kind == AST_ADD) {
      *out_type = TYPE_STRING;
      return SEMANTIC_OK;
    }
    if (relational) {
      *out_type = TYPE_BOOL;
      return SEMANTIC_OK;
    }
    return SEMANTIC_TYPE_ERROR;
  }

  /// `==` / `!=` also compare two values of the same (e.g. Unit) type.
  if (equality && left == right) {
    *out_type = TYPE_BOOL;
    return SEMANTIC_OK;
  }
  return SEMANTIC_TYPE_ERROR;
}

/**
 * @brief Computes the type of the expression in @p slot (bottom-up) and
 * stores it in the node. @p slot may be rewritten by an inserted conversion.
 */
static SemanticStatus check_expression(SemanticContext *ctx, ASTNode **slot,
                                       DataType *out_type) {
  ASTNode *node = *slot;
  SemanticStatus status = SEMANTIC_OK;
  DataType type = TYPE_UNKNOWN;

  if (node == NULL) {
    return SEMANTIC_INTERNAL_ERROR;
  }

  switch (node->kind) {
  case AST_CONSTANT_INT:
    type = TYPE_INT;
    break;
  case AST_CONSTANT_REAL:
    type = TYPE_REAL;
    break;
  case AST_CONSTANT_STRING:
    type = TYPE_STRING;
    break;
  case AST_CONSTANT_UNIT:
    type = TYPE_UNIT;
    break;
  case AST_VARIABLE_REF:
    type = node->data.variable.symbol->type.type;
    if (type == TYPE_UNKNOWN) {
      /// e.g. `val x = x`: used inside its own initialization.
      return SEMANTIC_UNDEFINED;
    }
    break;
  case AST_FUNCTION_CALL:
    status = check_call(ctx, node, &type);
    break;
  case AST_ADD:
  case AST_SUBTRACT:
  case AST_MULTIPLY:
  case AST_DIVIDE:
  case AST_LESS:
  case AST_LESS_EQUAL:
  case AST_GREATER:
  case AST_GREATER_EQUAL:
  case AST_EQUAL:
  case AST_NOT_EQUAL:
    status = check_binary(ctx, node, &type);
    break;
  case AST_IMPLICIT_INT_TO_REAL:
    status = check_expression(ctx, &node->data.conversion.operand, &type);
    if (status == SEMANTIC_OK && type != TYPE_INT) {
      return SEMANTIC_INTERNAL_ERROR;
    }
    type = TYPE_REAL;
    break;
  case AST_BLOCK:
  case AST_ASSIGN:
  case AST_RETURN:
  case AST_IF:
    /// Statements are not expressions in the base language.
    return SEMANTIC_INTERNAL_ERROR;
  }
  if (status != SEMANTIC_OK) {
    return status;
  }

  node->inferred_type.type = type;
  *out_type = type;
  return SEMANTIC_OK;
}

/** Type-checks one statement (recursively for nested blocks). */
static SemanticStatus check_statement(SemanticContext *ctx,
                                      FunctionSymbol *function,
                                      ASTNode **slot) {
  ASTNode *node = *slot;
  SemanticStatus status = SEMANTIC_OK;
  DataType type;
  size_t index;

  switch (node->kind) {
  case AST_BLOCK:
    for (index = 0U;
         status == SEMANTIC_OK && index < node->data.block.statement_count;
         index += 1U) {
      status =
          check_statement(ctx, function, &node->data.block.statements[index]);
    }
    if (status == SEMANTIC_OK) {
      node->inferred_type.type = TYPE_UNIT;
    }
    return status;

  case AST_ASSIGN: {
    Symbol *target = node->data.assignment.target;

    if (!node->data.assignment.is_definition && target->constant) {
      return SEMANTIC_OTHER; /// Assignment to `val` or a parameter.
    }
    status = check_expression(ctx, &node->data.assignment.value, &type);
    if (status != SEMANTIC_OK) {
      return status;
    }
    if (type == TYPE_BOOL) {
      return SEMANTIC_TYPE_ERROR; /// Booleans cannot be stored.
    }
    if (node->data.assignment.is_definition &&
        target->type.type == TYPE_UNKNOWN) {
      /// `val x = expr`: the variable takes the initializer's type.
      target->type.type = type;
    } else {
      status = coerce(&node->data.assignment.value, type, target->type.type,
                      SEMANTIC_TYPE_ERROR);
      if (status != SEMANTIC_OK) {
        return status;
      }
    }
    node->inferred_type.type = TYPE_UNIT;
    return SEMANTIC_OK;
  }

  case AST_RETURN:
    if (!function->has_explicit_return_type) {
      return SEMANTIC_TYPE_ERROR; /// Already excluded, kept for safety.
    }
    if (node->data.return_statement.value == NULL) {
      /// A bare `return` is allowed only with an explicit Unit return type.
      return function->return_type.type == TYPE_UNIT ? SEMANTIC_OK
                                                     : SEMANTIC_TYPE_ERROR;
    }
    status = check_expression(ctx, &node->data.return_statement.value, &type);
    if (status != SEMANTIC_OK) {
      return status;
    }
    node->inferred_type.type = type;
    if (function->return_type.type == TYPE_UNIT) {
      return SEMANTIC_OK; /// The value is evaluated and discarded (rule 4).
    }
    return coerce(&node->data.return_statement.value, type,
                  function->return_type.type, SEMANTIC_TYPE_ERROR);

  case AST_IF:
    status = check_expression(ctx, &node->data.if_statement.condition, &type);
    if (status != SEMANTIC_OK) {
      return status;
    }
    if (type != TYPE_BOOL) {
      return SEMANTIC_TYPE_ERROR;
    }
    status =
        check_statement(ctx, function, &node->data.if_statement.then_branch);
    if (status == SEMANTIC_OK && node->data.if_statement.else_branch != NULL) {
      status =
          check_statement(ctx, function, &node->data.if_statement.else_branch);
    }
    node->inferred_type.type = TYPE_UNIT;
    return status;

  default:
    /// Expression statement (incl. a call whose result is dropped).
    return check_expression(ctx, slot, &type);
  }
}

/**
 * @brief Determines the result of a statement as the last one of a block
 * (spec 2.3, rules 1-3). Types of all nodes must already be computed.
 */
static BodyResult statement_result(const ASTNode *node) {
  BodyResult result = {RESULT_VALUE, TYPE_UNIT};

  switch (node->kind) {
  case AST_RETURN:
    result.kind = RESULT_RETURNS;
    return result;
  case AST_ASSIGN:
    return result; /// Definition/assignment: ()
  case AST_BLOCK:
    if (node->data.block.statement_count == 0U) {
      return result;
    }
    return statement_result(
        node->data.block.statements[node->data.block.statement_count - 1U]);
  case AST_IF: {
    BodyResult then_result =
        statement_result(node->data.if_statement.then_branch);
    BodyResult else_result = result; /// Missing else behaves as `()`.

    if (node->data.if_statement.else_branch != NULL) {
      else_result = statement_result(node->data.if_statement.else_branch);
    }
    if (then_result.kind == RESULT_RETURNS &&
        else_result.kind == RESULT_RETURNS) {
      result.kind = RESULT_RETURNS;
    } else if (then_result.kind == RESULT_VALUE &&
               then_result.type == TYPE_UNIT &&
               else_result.kind == RESULT_VALUE &&
               else_result.type == TYPE_UNIT) {
      result.kind = RESULT_VALUE;
    } else {
      result.kind = RESULT_BAD_IF;
    }
    return result;
  }
  default:
    result.type = node->inferred_type.type;
    return result;
  }
}

/** Returns the slot holding the value-producing last statement of a body. */
static ASTNode **last_value_slot(ASTNode **slot) {
  while ((*slot)->kind == AST_BLOCK &&
         (*slot)->data.block.statement_count > 0U) {
    slot =
        &(*slot)
             ->data.block.statements[(*slot)->data.block.statement_count - 1U];
  }
  return slot;
}

/**
 * @brief Checks one function body and fixes its return type (memoized DFS).
 * @details Functions reached through calls of not-annotated functions are
 * checked first, so this resolves the return types in topological order.
 */
static SemanticStatus check_function(SemanticContext *ctx,
                                     FunctionSymbol *function) {
  SemanticStatus status;
  BodyResult result;

  if (function->check_state == FUNCTION_CHECKED) {
    return SEMANTIC_OK;
  }
  if (function->check_state == FUNCTION_CHECKING) {
    /// Unreachable: cycles through not-annotated functions were rejected.
    return SEMANTIC_INTERNAL_ERROR;
  }
  if (function->ast_root == NULL) {
    return SEMANTIC_INTERNAL_ERROR;
  }
  function->check_state = FUNCTION_CHECKING;

  status = check_statement(ctx, function, &function->ast_root);
  if (status != SEMANTIC_OK) {
    return status;
  }

  result = statement_result(function->ast_root);
  if (function->has_explicit_return_type) {
    DataType declared = function->return_type.type;

    if (declared != TYPE_UNIT && result.kind != RESULT_RETURNS) {
      if (result.kind == RESULT_BAD_IF) {
        return SEMANTIC_TYPE_ERROR;
      }
      status = coerce(last_value_slot(&function->ast_root), result.type,
                      declared, SEMANTIC_TYPE_ERROR);
      if (status != SEMANTIC_OK) {
        return status;
      }
    }
  } else {
    /// No `return` here (checked), so the result is the value of the body.
    if (result.kind != RESULT_VALUE || result.type == TYPE_BOOL ||
        result.type == TYPE_UNKNOWN) {
      return SEMANTIC_TYPE_ERROR;
    }
    function->return_type.type = result.type;
  }

  function->check_state = FUNCTION_CHECKED;
  return SEMANTIC_OK;
}

/* ------------------------------------------------------------------------- */
/* Entry point                                                               */
/* ------------------------------------------------------------------------- */

SemanticStatus semantic_check(Program *program) {
  SemanticContext ctx;
  FunctionTable *table;
  FunctionSymbol *main_function = NULL;
  SemanticStatus status;
  size_t index;

  if (program == NULL || !program->initialized) {
    return SEMANTIC_INTERNAL_ERROR;
  }
  ctx.program = program;
  ctx.mark = 0U;
  table = &program->functions;

  status = register_builtins(program);
  if (status != SEMANTIC_OK) {
    return status;
  }

  /// 1. Every referenced function exists; exactly one @main exists.
  for (index = 0U; index < table->function_count; index += 1U) {
    FunctionSymbol *function = table->functions[index];
    if (!function->is_defined) {
      return SEMANTIC_UNDEFINED;
    }
    if (function->is_main) {
      if (main_function != NULL) {
        return SEMANTIC_REDEFINITION;
      }
      main_function = function;
    }
  }
  if (main_function == NULL) {
    return SEMANTIC_UNDEFINED;
  }

  /// 2. Build the call graph and find `return` statements.
  for (index = 0U; index < table->function_count; index += 1U) {
    FunctionSymbol *function = table->functions[index];
    if (function->is_builtin) {
      continue;
    }
    status = collect_graph(function, function->ast_root);
    if (status != SEMANTIC_OK) {
      return status;
    }
  }

  /// 3. A function needs an explicit return type if it contains `return` or
  /// is directly/indirectly recursive (lies on a cycle of the call graph).
  for (index = 0U; index < table->function_count; index += 1U) {
    FunctionSymbol *function = table->functions[index];
    if (function->is_builtin || function->has_explicit_return_type) {
      continue;
    }
    if (function->contains_return) {
      return SEMANTIC_TYPE_ERROR;
    }
    ctx.mark += 1U;
    if (reaches(function, function, ctx.mark)) {
      return SEMANTIC_TYPE_ERROR;
    }
  }

  /// 4. Type-check all functions; callees whose type must be inferred are
  /// checked on demand first (implicit topological order).
  for (index = 0U; index < table->function_count; index += 1U) {
    status = check_function(&ctx, table->functions[index]);
    if (status != SEMANTIC_OK) {
      return status;
    }
  }

  /// 5. The entry function has no parameters and returns Unit.
  if (main_function->argument_count != 0U) {
    return SEMANTIC_BAD_CALL;
  }
  if (main_function->return_type.type != TYPE_UNIT) {
    return SEMANTIC_TYPE_ERROR;
  }
  return SEMANTIC_OK;
}
