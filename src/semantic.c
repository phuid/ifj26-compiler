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
  Program *program; ///< Program being checked (borrowed).
  unsigned mark;    ///< Last used FunctionSymbol::visit_mark value.
} SemanticContext;

/** Kind of the value a block/statement produces as its result. */
typedef enum {
  RESULT_VALUE,   ///< Ends with a value of @c BodyResult::type.
  RESULT_RETURNS, ///< Every path ends with `return`.
  RESULT_BAD_IF   ///< Ends with an `if` violating rule 3 of spec 2.3.
} ResultKind;

/** Result of a block, i.e. of its last statement (spec 2.3). */
typedef struct {
  ResultKind kind; ///< How the block ends.
  DataType type;   ///< Type of the value; meaningful only for RESULT_VALUE.
} BodyResult;

/* ------------------------------------------------------------------------- */
/* Builtin functions                                                         */
/* ------------------------------------------------------------------------- */

/** Fixed signature of one builtin function of IFJ26. */
typedef struct {
  const char *name;       ///< Name as written in the source code.
  BaseType return_type;   ///< Type of the value the builtin returns.
  size_t parameter_count; ///< Number of used items in @c parameters.
  /// Parameter types in source order; TYPE_ANY accepts any value.
  BaseType parameters[3];
} BuiltinSignature;

/** Signatures of all builtin functions, matched to the program by name. */
static const BuiltinSignature BUILTINS[] = {
    {"print", TYPE_UNIT, 1U, {TYPE_ANY}},
    {"readLine", TYPE_STRING, 0U, {TYPE_UNKNOWN}},
    {"readInt", TYPE_INT, 0U, {TYPE_UNKNOWN}},
    {"readDouble", TYPE_REAL, 0U, {TYPE_UNKNOWN}},
    {"readBoolean", TYPE_BOOL, 0U, {TYPE_UNKNOWN}},
    {"toStr", TYPE_STRING, 1U, {TYPE_ANY}},
    {"toInt", TYPE_INT, 1U, {TYPE_REAL}},
    {"strLength", TYPE_INT, 1U, {TYPE_STRING}},
    {"subStr", TYPE_STRING, 3U, {TYPE_STRING, TYPE_INT, TYPE_INT}},
    {"ord", TYPE_INT, 2U, {TYPE_STRING, TYPE_INT}},
    {"chr", TYPE_STRING, 1U, {TYPE_INT}},
};

/**
 * @brief Gives signatures to builtins referenced (but not defined) by the
 * program.
 * @details The parser only created forward references for them. Builtins that
 * the program never calls are not in the function table and are skipped.
 * @param program Parsed program whose function table is updated.
 * @return SEMANTIC_OK, SEMANTIC_REDEFINITION when the program defines a
 * function with a builtin name, or SEMANTIC_INTERNAL_ERROR on OOM.
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

    /// Fill in the signature; a builtin has no body, so it is already checked.
    function->is_defined = true;
    function->is_builtin = true;
    function->has_explicit_return_type = true;
    function->return_type.type = type_scalar(BUILTINS[index].return_type);
    function->argument_count = 0U;
    for (parameter = 0U; parameter < BUILTINS[index].parameter_count;
         parameter += 1U) {
      if (function_symbol_add_parameter(
              function, type_scalar(BUILTINS[index].parameters[parameter])) !=
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
 * @details Walks the whole AST, unreachable code included (spec 2.3). Every
 * called function is added once to @c FunctionSymbol::callees and
 * @c FunctionSymbol::contains_return is set when a `return` is found.
 * @param function Function whose body is walked; receives the edges.
 * @param node Current subtree; may be null (e.g. a missing else branch).
 * @return SEMANTIC_OK, or SEMANTIC_INTERNAL_ERROR on OOM or an unknown node
 * kind.
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
    /// Edge to the callee first, then calls nested in the arguments.
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
  case AST_LIST_CONSTRUCTOR:
    /// `List(...)` has no callee, only calls nested in its elements.
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
  case AST_AND:
  case AST_OR:
  case AST_LIST_CONCAT:
  case AST_LIST_INDEXING:
  case AST_TO:
  case AST_UNTIL:
  case AST_BY:
    status = collect_graph(function, node->data.binary_op.left);
    if (status == SEMANTIC_OK) {
      status = collect_graph(function, node->data.binary_op.right);
    }
    return status;
  case AST_NOT:
    return collect_graph(function, node->data.unary_op.operand);
  case AST_IMPLICIT_INT_TO_REAL:
    return collect_graph(function, node->data.conversion.operand);
  case AST_CONSTANT_INT:
  case AST_CONSTANT_REAL:
  case AST_CONSTANT_STRING:
  case AST_CONSTANT_BOOL:
  case AST_CONSTANT_UNIT:
  case AST_VARIABLE_REF:
    /// Leaves cannot contain calls or `return`.
    return SEMANTIC_OK;
  }

  /// Unknown node kind; every valid kind is handled above.
  return SEMANTIC_INTERNAL_ERROR;
}

/**
 * @brief Tells whether @p target is reachable from @p from by at least one
 * call edge (i.e. whether @p target lies on a cycle when from == target).
 * @details Plain DFS with visit marks; O(V + E) per query.
 * @param from Function whose callees are searched.
 * @param target Function being looked for.
 * @param mark Fresh mark of this query; visited functions are stamped with it.
 * @return True if a non-empty call path leads from @p from to @p target.
 */
static bool reaches(FunctionSymbol *from, const FunctionSymbol *target,
                    unsigned mark) {
  size_t index;

  for (index = 0U; index < from->callee_count; index += 1U) {
    FunctionSymbol *callee = from->callees[index];
    if (callee == target) {
      return true;
    }

    /// Descend into each function at most once per query.
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

/**
 * @brief Tells whether @p type is numeric (Int or Double).
 * @param type Type to test.
 * @return True for the scalar types Int and Double.
 */
static bool is_numeric(DataType type) {
  return type_is_scalar(type, TYPE_INT) || type_is_scalar(type, TYPE_REAL);
}

/**
 * @brief Tells whether @p node is an empty list literal `List()`.
 * @details Its element type cannot be inferred from the elements; it takes
 * the type expected by the context (spec 5.8).
 */
static bool is_empty_list(const ASTNode *node) {
  return node->kind == AST_LIST_CONSTRUCTOR &&
         node->data.call.argument_count == 0U;
}

/**
 * @brief Makes the expression in @p slot usable where @p to is expected.
 * @details Same type is accepted as is; Int is widened to Double by wrapping
 * the expression in an AST_IMPLICIT_INT_TO_REAL node (the only implicit
 * conversion of IFJ26). An empty `List()` takes any expected list type.
 * Anything else yields @p mismatch.
 * @param slot Slot holding the expression; rewritten when a conversion is
 * inserted.
 * @param from Type of the expression in @p slot.
 * @param to Type expected by the context.
 * @param mismatch Status for incompatible types; it depends on the context
 * (call argument, operand, assignment, ...).
 * @return SEMANTIC_OK, @p mismatch, or SEMANTIC_INTERNAL_ERROR on OOM.
 */
static SemanticStatus coerce(ASTNode **slot, DataType from, DataType to,
                             SemanticStatus mismatch) {
  ASTNode *conversion;

  if (!type_is_unknown(from) && type_equal(from, to)) {
    return SEMANTIC_OK;
  }
  if (type_is_scalar(from, TYPE_INT) && type_is_scalar(to, TYPE_REAL)) {
    /// The conversion node takes ownership of the original expression.
    conversion = ast_node_create_implicit_int_to_real(*slot);
    if (conversion == NULL) {
      return SEMANTIC_INTERNAL_ERROR;
    }
    *slot = conversion;
    return SEMANTIC_OK;
  }
  if (is_empty_list(*slot) && type_is_list(to)) {
    (*slot)->inferred_type.type = to;
    return SEMANTIC_OK;
  }
  return mismatch;
}

/**
 * @brief Finds the common type of two values that meet in one place (if
 * branches, list elements).
 * @param left Type of the first value.
 * @param right Type of the second value.
 * @param out_type Receives the common type: the shared type, or Double for a
 * mix of Int and Double.
 * @return True when the types are compatible.
 */
static bool common_type(DataType left, DataType right, DataType *out_type) {
  if (type_is_unknown(left) || type_is_unknown(right)) {
    return false;
  }
  if (type_equal(left, right)) {
    *out_type = left;
    return true;
  }
  if (is_numeric(left) && is_numeric(right)) {
    *out_type = type_scalar(TYPE_REAL);
    return true;
  }
  return false;
}

static SemanticStatus check_function(SemanticContext *ctx,
                                     FunctionSymbol *function);

static SemanticStatus check_expression(SemanticContext *ctx,
                                       FunctionSymbol *function, ASTNode **slot,
                                       DataType *out_type);

static SemanticStatus check_statement(SemanticContext *ctx,
                                      FunctionSymbol *function, ASTNode **slot);

static SemanticStatus statement_result(ASTNode *node, BodyResult *out_result);

/**
 * @brief Returns the slot holding the value-producing last statement of a
 * body.
 * @details Descends through nested blocks so that a conversion can be inserted
 * directly around the final expression.
 * @param slot Slot holding the function body.
 * @return Slot of the last non-block statement, or of an empty block.
 */
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
 * @brief Type-checks a call and yields the callee's return type.
 * @details Arguments are checked against the parameter types of the callee and
 * Int arguments are widened to Double where needed. A callee without an
 * explicit return type is checked first so that its type is inferred.
 * @param ctx State of the semantic pass.
 * @param function Function containing the call.
 * @param node AST_FUNCTION_CALL node; its argument slots may be rewritten.
 * @param out_type Receives the return type of the callee on success.
 * @return SEMANTIC_OK, SEMANTIC_UNDEFINED for an undefined callee,
 * SEMANTIC_BAD_CALL for a wrong number or type of arguments, or an error from
 * checking the arguments or the callee.
 */
static SemanticStatus check_call(SemanticContext *ctx, FunctionSymbol *function,
                                 ASTNode *node, DataType *out_type) {
  FunctionSymbol *callee = node->data.call.function;
  SemanticStatus status;
  size_t index;

  if (callee == NULL || !callee->is_defined) {
    return SEMANTIC_UNDEFINED;
  }

  /// The number of arguments must match the signature exactly.
  if (node->data.call.argument_count != callee->argument_count) {
    return SEMANTIC_BAD_CALL;
  }

  /// Arguments only need the callee's signature (known after parsing).
  for (index = 0U; index < node->data.call.argument_count; index += 1U) {
    DataType argument_type;
    DataType parameter_type = callee->parameter_types[index];

    status = check_expression(ctx, function, &node->data.call.arguments[index],
                              &argument_type);
    if (status != SEMANTIC_OK) {
      return status;
    }

    /// A TYPE_ANY parameter takes any (inferable) value without conversion.
    if (type_is_scalar(parameter_type, TYPE_ANY)) {
      if (type_is_unknown(argument_type)) {
        return SEMANTIC_TYPE_ERROR; /// e.g. print(List())
      }
      continue;
    }
    status = coerce(&node->data.call.arguments[index], argument_type,
                    parameter_type, SEMANTIC_BAD_CALL);
    if (status != SEMANTIC_OK) {
      return status;
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

/**
 * @brief Type-checks a list literal `List(e1, e2, ...)` (LISTS).
 * @details All elements must have a common type (Int elements are widened if
 * any element is Double). An empty list gets its type from the context, see
 * coerce(); until then its type is unknown.
 */
static SemanticStatus check_list_constructor(SemanticContext *ctx,
                                             FunctionSymbol *function,
                                             ASTNode *node,
                                             DataType *out_type) {
  DataType element = type_unknown();
  SemanticStatus status;
  size_t index;

  if (node->data.call.argument_count == 0U) {
    *out_type = type_unknown();
    return SEMANTIC_OK;
  }

  /// First pass: types of all elements and their common type.
  for (index = 0U; index < node->data.call.argument_count; index += 1U) {
    DataType type;

    status = check_expression(ctx, function, &node->data.call.arguments[index],
                              &type);
    if (status != SEMANTIC_OK) {
      return status;
    }
    if (index == 0U) {
      element = type;
    } else if (!common_type(element, type, &element)) {
      return SEMANTIC_TYPE_ERROR;
    }
  }
  if (type_is_unknown(element)) {
    return SEMANTIC_TYPE_ERROR; /// e.g. List(List())
  }

  /// Second pass: widen the elements to the common type.
  for (index = 0U; index < node->data.call.argument_count; index += 1U) {
    status = coerce(&node->data.call.arguments[index],
                    node->data.call.arguments[index]->inferred_type.type,
                    element, SEMANTIC_TYPE_ERROR);
    if (status != SEMANTIC_OK) {
      return status;
    }
  }
  *out_type = type_list(element);
  return SEMANTIC_OK;
}

/**
 * @brief Type-checks a binary operator.
 * @details Arithmetic: numeric operands are converted to a common type
 * (Double if either of them is Double), Strings support `+`. Relational
 * `<`... compare numbers or Strings, `==`/`!=` also any two values of the same
 * type; their result is Boolean, so they cannot be chained. `&&`/`||` take
 * Booleans (BOOLTHEN). `++`, indexing, `to`, `until` and `by` work on lists
 * and Int ranges (LISTS, CONTROL).
 * @param ctx State of the semantic pass.
 * @param function Function containing the expression.
 * @param node Binary operator node; its operand slots may be rewritten.
 * @param out_type Receives the result type.
 * @return SEMANTIC_OK, SEMANTIC_TYPE_ERROR for incompatible operands, or an
 * error from checking an operand.
 */
static SemanticStatus check_binary(SemanticContext *ctx,
                                   FunctionSymbol *function, ASTNode *node,
                                   DataType *out_type) {
  DataType left;
  DataType right;
  DataType common;
  SemanticStatus status;

  status = check_expression(ctx, function, &node->data.binary_op.left, &left);
  if (status == SEMANTIC_OK) {
    status =
        check_expression(ctx, function, &node->data.binary_op.right, &right);
  }
  if (status != SEMANTIC_OK) {
    return status;
  }

  switch (node->kind) {
  case AST_AND:
  case AST_OR:
    if (!type_is_scalar(left, TYPE_BOOL) || !type_is_scalar(right, TYPE_BOOL)) {
      return SEMANTIC_TYPE_ERROR;
    }
    *out_type = type_scalar(TYPE_BOOL);
    return SEMANTIC_OK;

  case AST_LIST_CONCAT:
    /// `++` joins two lists of the same type; `List()` takes the other's type.
    if (type_is_list(left) && is_empty_list(node->data.binary_op.right)) {
      right = left;
      node->data.binary_op.right->inferred_type.type = left;
    } else if (type_is_list(right) &&
               is_empty_list(node->data.binary_op.left)) {
      left = right;
      node->data.binary_op.left->inferred_type.type = right;
    }
    if (!type_is_list(left) || !type_equal(left, right)) {
      return SEMANTIC_TYPE_ERROR;
    }
    *out_type = left;
    return SEMANTIC_OK;

  case AST_LIST_INDEXING:
    if (!type_is_list(left) || !type_is_scalar(right, TYPE_INT)) {
      return SEMANTIC_TYPE_ERROR;
    }
    type_element_type(left, out_type);
    return SEMANTIC_OK;

  case AST_TO:
  case AST_UNTIL:
    /// Int range; represented as List[Int] (it supports `toList`, `by`).
    if (!type_is_scalar(left, TYPE_INT) || !type_is_scalar(right, TYPE_INT)) {
      return SEMANTIC_TYPE_ERROR;
    }
    *out_type = type_list(type_scalar(TYPE_INT));
    return SEMANTIC_OK;

  case AST_BY:
    if (!type_equal(left, type_list(type_scalar(TYPE_INT))) ||
        !type_is_scalar(right, TYPE_INT)) {
      return SEMANTIC_TYPE_ERROR;
    }
    *out_type = left;
    return SEMANTIC_OK;

  case AST_ADD:
  case AST_SUBTRACT:
  case AST_MULTIPLY:
  case AST_DIVIDE:
    if (is_numeric(left) && is_numeric(right)) {
      break; /// Numeric arithmetic, see below.
    }
    if (node->kind == AST_ADD && type_is_scalar(left, TYPE_STRING) &&
        type_is_scalar(right, TYPE_STRING)) {
      *out_type = left; /// String concatenation.
      return SEMANTIC_OK;
    }
    return SEMANTIC_TYPE_ERROR;

  case AST_LESS:
  case AST_LESS_EQUAL:
  case AST_GREATER:
  case AST_GREATER_EQUAL:
    if (is_numeric(left) && is_numeric(right)) {
      break; /// Numeric comparison, see below.
    }
    if (type_is_scalar(left, TYPE_STRING) &&
        type_is_scalar(right, TYPE_STRING)) {
      *out_type = type_scalar(TYPE_BOOL);
      return SEMANTIC_OK;
    }
    return SEMANTIC_TYPE_ERROR;

  case AST_EQUAL:
  case AST_NOT_EQUAL:
    if (is_numeric(left) && is_numeric(right)) {
      break; /// Numeric comparison, see below.
    }
    /// Any two values of the same type (incl. Boolean, Unit, lists).
    if (type_is_unknown(left) || !type_equal(left, right)) {
      return SEMANTIC_TYPE_ERROR;
    }
    *out_type = type_scalar(TYPE_BOOL);
    return SEMANTIC_OK;

  default:
    return SEMANTIC_INTERNAL_ERROR;
  }

  /// Mixed Int and Double operands: the Int side is widened to Double.
  common_type(left, right, &common);
  status =
      coerce(&node->data.binary_op.left, left, common, SEMANTIC_TYPE_ERROR);
  if (status == SEMANTIC_OK) {
    status =
        coerce(&node->data.binary_op.right, right, common, SEMANTIC_TYPE_ERROR);
  }
  if (status != SEMANTIC_OK) {
    return status;
  }
  switch (node->kind) {
  case AST_ADD:
  case AST_SUBTRACT:
  case AST_MULTIPLY:
  case AST_DIVIDE:
    *out_type = common;
    break;
  default:
    *out_type = type_scalar(TYPE_BOOL);
    break;
  }
  return SEMANTIC_OK;
}

/**
 * @brief Type-checks the condition and both branches of an `if`.
 * @details Each branch is a block or a one-line expression (CONTROL); its
 * statements are checked, its value is evaluated later by if_result().
 */
static SemanticStatus check_if_parts(SemanticContext *ctx,
                                     FunctionSymbol *function, ASTNode *node) {
  SemanticStatus status;
  DataType type;

  /// The condition is any Boolean expression (BOOLTHEN).
  status = check_expression(ctx, function, &node->data.if_statement.condition,
                            &type);
  if (status != SEMANTIC_OK) {
    return status;
  }
  if (!type_is_scalar(type, TYPE_BOOL)) {
    return SEMANTIC_TYPE_ERROR;
  }
  status = check_statement(ctx, function, &node->data.if_statement.then_branch);
  if (status == SEMANTIC_OK && node->data.if_statement.else_branch != NULL) {
    status =
        check_statement(ctx, function, &node->data.if_statement.else_branch);
  }
  return status;
}

/**
 * @brief Determines the value of an already checked `if` (spec 2.3, CONTROL).
 * @details A missing else behaves as `()`. A branch ending with `return`
 * yields no value and adapts to the other branch. Otherwise both branch
 * values must have a common type; Int branches are widened if the other one
 * is Double.
 * @param node Checked AST_IF node; its branch slots may be rewritten.
 * @param out_result Receives how the `if` ends and the type of its value.
 * @return SEMANTIC_OK or SEMANTIC_INTERNAL_ERROR on OOM.
 */
static SemanticStatus if_result(ASTNode *node, BodyResult *out_result) {
  BodyResult then_result;
  BodyResult else_result = {RESULT_VALUE, {TYPE_UNIT, 0U}};
  SemanticStatus status;
  DataType common;

  status = statement_result(node->data.if_statement.then_branch, &then_result);
  if (status == SEMANTIC_OK && node->data.if_statement.else_branch != NULL) {
    status =
        statement_result(node->data.if_statement.else_branch, &else_result);
  }
  if (status != SEMANTIC_OK) {
    return status;
  }

  if (then_result.kind == RESULT_BAD_IF || else_result.kind == RESULT_BAD_IF) {
    out_result->kind = RESULT_BAD_IF;
    return SEMANTIC_OK;
  }
  if (then_result.kind == RESULT_RETURNS) {
    *out_result = else_result; /// Both return, or only the else value.
    return SEMANTIC_OK;
  }
  if (else_result.kind == RESULT_RETURNS) {
    *out_result = then_result;
    return SEMANTIC_OK;
  }
  if (!common_type(then_result.type, else_result.type, &common)) {
    out_result->kind = RESULT_BAD_IF;
    return SEMANTIC_OK;
  }

  /// Widen the Int branch value(s) to Double where the types differ.
  status = coerce(last_value_slot(&node->data.if_statement.then_branch),
                  then_result.type, common, SEMANTIC_INTERNAL_ERROR);
  if (status == SEMANTIC_OK && node->data.if_statement.else_branch != NULL) {
    status = coerce(last_value_slot(&node->data.if_statement.else_branch),
                    else_result.type, common, SEMANTIC_INTERNAL_ERROR);
  }
  out_result->kind = RESULT_VALUE;
  out_result->type = common;
  return status;
}

/**
 * @brief Computes the type of the expression in @p slot (bottom-up) and
 * stores it in the node.
 * @details Slots of nested operands and arguments may be rewritten by an
 * inserted Int-to-Double conversion.
 * @param ctx State of the semantic pass.
 * @param function Function containing the expression.
 * @param slot Slot holding the expression to check.
 * @param out_type Receives the computed type on success; unknown only for an
 * empty `List()`.
 * @return SEMANTIC_OK, SEMANTIC_UNDEFINED for a variable used in its own
 * initialization, SEMANTIC_INTERNAL_ERROR for a statement node, or an error
 * from a nested check.
 */
static SemanticStatus check_expression(SemanticContext *ctx,
                                       FunctionSymbol *function, ASTNode **slot,
                                       DataType *out_type) {
  ASTNode *node = *slot;
  SemanticStatus status = SEMANTIC_OK;
  DataType type = type_unknown();

  if (node == NULL) {
    return SEMANTIC_INTERNAL_ERROR;
  }

  switch (node->kind) {
  case AST_CONSTANT_INT:
    type = type_scalar(TYPE_INT);
    break;
  case AST_CONSTANT_REAL:
    type = type_scalar(TYPE_REAL);
    break;
  case AST_CONSTANT_STRING:
    type = type_scalar(TYPE_STRING);
    break;
  case AST_CONSTANT_BOOL:
    type = type_scalar(TYPE_BOOL);
    break;
  case AST_CONSTANT_UNIT:
    type = type_scalar(TYPE_UNIT);
    break;
  case AST_VARIABLE_REF:
    type = node->data.variable.symbol->type.type;
    if (type_is_unknown(type)) {
      /// e.g. `val x = x`: used inside its own initialization.
      return SEMANTIC_UNDEFINED;
    }
    break;
  case AST_FUNCTION_CALL:
    status = check_call(ctx, function, node, &type);
    break;
  case AST_LIST_CONSTRUCTOR:
    status = check_list_constructor(ctx, function, node, &type);
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
  case AST_AND:
  case AST_OR:
  case AST_LIST_CONCAT:
  case AST_LIST_INDEXING:
  case AST_TO:
  case AST_UNTIL:
  case AST_BY:
    status = check_binary(ctx, function, node, &type);
    break;
  case AST_NOT:
    status =
        check_expression(ctx, function, &node->data.unary_op.operand, &type);
    if (status == SEMANTIC_OK && !type_is_scalar(type, TYPE_BOOL)) {
      return SEMANTIC_TYPE_ERROR;
    }
    break;
  case AST_IMPLICIT_INT_TO_REAL:
    /// The operand of a conversion must be Int; the result is Double.
    status =
        check_expression(ctx, function, &node->data.conversion.operand, &type);
    if (status == SEMANTIC_OK && !type_is_scalar(type, TYPE_INT)) {
      return SEMANTIC_INTERNAL_ERROR;
    }
    type = type_scalar(TYPE_REAL);
    break;
  case AST_IF: {
    /// `if` used as an expression (CONTROL): else is mandatory and the
    /// branches must have a common type.
    BodyResult result;

    if (node->data.if_statement.else_branch == NULL) {
      return SEMANTIC_TYPE_ERROR;
    }
    status = check_if_parts(ctx, function, node);
    if (status == SEMANTIC_OK) {
      status = if_result(node, &result);
    }
    if (status != SEMANTIC_OK) {
      return status;
    }
    if (result.kind != RESULT_VALUE) {
      return SEMANTIC_TYPE_ERROR;
    }
    type = result.type;
    break;
  }
  case AST_BLOCK:
  case AST_ASSIGN:
  case AST_RETURN:
    /// Statements are not expressions.
    return SEMANTIC_INTERNAL_ERROR;
  }
  if (status != SEMANTIC_OK) {
    return status;
  }

  /// Record the computed type in the AST node.
  node->inferred_type.type = type;
  *out_type = type;
  return SEMANTIC_OK;
}

/**
 * @brief Type-checks one statement (recursively for nested blocks).
 * @param ctx State of the semantic pass.
 * @param function Function containing the statement; used by `return`.
 * @param slot Slot holding the statement.
 * @return SEMANTIC_OK, SEMANTIC_OTHER for an assignment to an immutable
 * variable, SEMANTIC_TYPE_ERROR for incompatible types, or an error from a
 * nested check.
 */
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

    /// The block node itself is Unit; its result is given by statement_result.
    if (status == SEMANTIC_OK) {
      node->inferred_type.type = type_scalar(TYPE_UNIT);
    }
    return status;

  case AST_ASSIGN: {
    Symbol *target = node->data.assignment.target;

    if (!node->data.assignment.is_definition && target->constant) {
      /// Assignment to `val` or a parameter.
      return SEMANTIC_OTHER;
    }
    status =
        check_expression(ctx, function, &node->data.assignment.value, &type);
    if (status != SEMANTIC_OK) {
      return status;
    }
    if (node->data.assignment.is_definition &&
        type_is_unknown(target->type.type)) {
      /// `val x = expr`: the variable takes the initializer's type; it must be
      /// inferable (not e.g. `val x = List()`).
      if (type_is_unknown(type)) {
        return SEMANTIC_TYPE_ERROR;
      }
      target->type.type = type;
    } else {
      /// The variable already has a type: the value must match or be widened.
      status = coerce(&node->data.assignment.value, type, target->type.type,
                      SEMANTIC_TYPE_ERROR);
      if (status != SEMANTIC_OK) {
        return status;
      }
    }
    node->inferred_type.type = type_scalar(TYPE_UNIT);
    return SEMANTIC_OK;
  }

  case AST_RETURN:
    if (!function->has_explicit_return_type) {
      /// Already excluded, kept for safety.
      return SEMANTIC_TYPE_ERROR;
    }
    if (node->data.return_statement.value == NULL) {
      /// A bare `return` is allowed only with an explicit Unit return type.
      return type_is_scalar(function->return_type.type, TYPE_UNIT)
                 ? SEMANTIC_OK
                 : SEMANTIC_TYPE_ERROR;
    }
    status = check_expression(ctx, function, &node->data.return_statement.value,
                              &type);
    if (status != SEMANTIC_OK) {
      return status;
    }
    node->inferred_type.type = type;
    if (type_is_scalar(function->return_type.type, TYPE_UNIT)) {
      /// The value is evaluated and discarded (rule 4).
      return SEMANTIC_OK;
    }
    return coerce(&node->data.return_statement.value, type,
                  function->return_type.type, SEMANTIC_TYPE_ERROR);

  case AST_IF:
    if (node->data.if_statement.used_as_an_expression) {
      return check_expression(ctx, function, slot, &type);
    }
    /// An `if` statement; its value (if needed) is given by statement_result.
    status = check_if_parts(ctx, function, node);
    if (status == SEMANTIC_OK) {
      node->inferred_type.type = type_scalar(TYPE_UNIT);
    }
    return status;

  default:
    /// Expression statement (incl. a call whose result is dropped).
    return check_expression(ctx, function, slot, &type);
  }
}

/**
 * @brief Determines the result of a statement as the last one of a block
 * (spec 2.3, rules 1-3).
 * @details Types of all nodes must already be computed. A final `if` yields
 * the common value of its branches (it is an expression with CONTROL).
 * @param node Statement to evaluate; branch slots of an `if` may be rewritten
 * by inserted conversions.
 * @param out_result Receives how the statement ends and, for RESULT_VALUE, the
 * type of its value.
 * @return SEMANTIC_OK or SEMANTIC_INTERNAL_ERROR on OOM.
 */
static SemanticStatus statement_result(ASTNode *node, BodyResult *out_result) {
  out_result->kind = RESULT_VALUE;
  out_result->type = type_scalar(TYPE_UNIT);

  switch (node->kind) {
  case AST_RETURN:
    out_result->kind = RESULT_RETURNS;
    return SEMANTIC_OK;
  case AST_ASSIGN:
    /// Definition/assignment: ()
    return SEMANTIC_OK;
  case AST_BLOCK:
    if (node->data.block.statement_count == 0U) {
      return SEMANTIC_OK;
    }
    return statement_result(
        node->data.block.statements[node->data.block.statement_count - 1U],
        out_result);
  case AST_IF:
    if (node->data.if_statement.used_as_an_expression) {
      out_result->type = node->inferred_type.type; /// Already unified.
      return SEMANTIC_OK;
    }
    return if_result(node, out_result);
  default:
    out_result->type = node->inferred_type.type;
    return SEMANTIC_OK;
  }
}

/**
 * @brief Checks one function body and fixes its return type (memoized DFS).
 * @details Functions reached through calls of not-annotated functions are
 * checked first, so this resolves the return types in topological order.
 * @param ctx State of the semantic pass.
 * @param function Function to check; an already checked one is skipped.
 * @return SEMANTIC_OK, SEMANTIC_TYPE_ERROR when the body does not match the
 * return type, SEMANTIC_INTERNAL_ERROR on a broken invariant, or an error from
 * checking the body.
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

  /// Compare the value of the body with the (declared or inferred) return type.
  status = statement_result(function->ast_root, &result);
  if (status != SEMANTIC_OK) {
    return status;
  }
  if (function->has_explicit_return_type) {
    DataType declared = function->return_type.type;

    /// With a declared Unit type the final value is discarded.
    if (!type_is_scalar(declared, TYPE_UNIT) && result.kind != RESULT_RETURNS) {
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
    if (result.kind != RESULT_VALUE || type_is_unknown(result.type)) {
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

  /// Builtins first, so that calls to them count as defined in step 1.
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
  if (!type_is_scalar(main_function->return_type.type, TYPE_UNIT)) {
    return SEMANTIC_TYPE_ERROR;
  }
  return SEMANTIC_OK;
}
