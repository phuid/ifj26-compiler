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
 * the call graph, so the dependencies between such functions form
 * a DAG and a memoized DFS (check callee before caller) resolves them in a
 * topological order without building that order explicitly.
 */

#include "semantic.h"

#include "ast.h"
#include "error_trace.h"
#include "function_table.h"
#include "symtable.h"
#include "type.h"

#include <stdbool.h>
#include <stddef.h>

/** Records an error origin at the start of the body of @p function. */
#define ERROR_ORIGIN_FUNCTION(function, ...)                                   \
  ERROR_ORIGIN_AT(                                                             \
      (function)->ast_root != NULL ? (function)->ast_root->line : 0U,          \
      (function)->ast_root != NULL ? (function)->ast_root->column : 0U, NULL,  \
      __VA_ARGS__)

#include "builtins.h"

/** State shared by the whole semantic pass. */
typedef struct {
  Program *program; ///< Program being checked (borrowed).
  unsigned mark;    ///< Last used FunctionSymbol::visit_mark value.
} SemanticContext;

/** Kind of the value a block/statement produces as its result. */
typedef enum {
  RESULT_VALUE,   ///< Ends with a value of @c BodyResult::type.
  RESULT_RETURNS, ///< Every path ends with `return`.
  RESULT_BAD_IF   ///< Ends with an `if` which violates the rules (e.g., missing
                  ///< `else`).
} ResultKind;

/** Result of a block, i.e. of its last statement. */
typedef struct {
  ResultKind kind; ///< How the block ends.
  DataType type;   ///< Type of the value; meaningful only for RESULT_VALUE.
} BodyResult;

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
  SymbolTable *local_scope;

  for (index = 0U; index < BUILTIN_COUNT; index += 1U) {
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
      ERROR_ORIGIN_FUNCTION(function, "redefinition of builtin function '%s'",
                            function->name);
      return SEMANTIC_REDEFINITION;
    }

    /// Fill in the signature; a builtin has no body, so it is already checked.
    function->is_defined = true;
    function->is_builtin = true;
    function->has_explicit_return_type = true;
    function->return_type.type = type_scalar(BUILTINS[index].return_type);
    function->argument_count = 0U;
    /// Parameters live in the builtin's own local scope like those of user
    /// functions; their names are synthetic and never visible to the program.
    local_scope = function_symbol_create_local_scope(function);
    if (local_scope == NULL) {
      ERROR_INTERNAL("cannot create the local scope of builtin '%s'",
                     function->name);
      return SEMANTIC_INTERNAL_ERROR;
    }
    for (parameter = 0U; parameter < BUILTINS[index].parameter_count;
         parameter += 1U) {
      static const char *const NAMES[] = {"p0", "p1", "p2"};
      Symbol *symbol;

      if (symbol_table_define(
              local_scope, NAMES[parameter],
              type_scalar(BUILTINS[index].parameters[parameter]), true, true,
              &symbol) != SYMTABLE_OK ||
          function_symbol_add_parameter(function, symbol) !=
              FUNCTION_TABLE_OK) {
        ERROR_INTERNAL("cannot add parameter %zu of builtin '%s'", parameter,
                       function->name);
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
 * @details Walks the whole AST, unreachable code included. Every
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
      ERROR_INTERNAL("cannot add a call-graph edge from '%s'", function->name);
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
  case AST_WHILE:
    status = collect_graph(function, node->data.while_loop.condition);
    if (status == SEMANTIC_OK) {
      status = collect_graph(function, node->data.while_loop.body);
    }
    return status;
  case AST_FOR:
    status = collect_graph(function, node->data.for_loop.iterable);
    if (status == SEMANTIC_OK) {
      status = collect_graph(function, node->data.for_loop.guard);
    }
    if (status == SEMANTIC_OK) {
      status = collect_graph(function, node->data.for_loop.body);
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
  ERROR_INTERNAL("unknown AST node kind %d", (int)node->kind);
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

/** Returns the name of a base type for error messages, e.g. "Int". */
static const char *base_type_name(BaseType base) {
  switch (base) {
  case TYPE_INT:
    return "Int";
  case TYPE_REAL:
    return "Double";
  case TYPE_STRING:
    return "String";
  case TYPE_UNIT:
    return "Unit";
  case TYPE_BOOL:
    return "Boolean";
  case TYPE_ANY:
    return "Any";
  case TYPE_ANY_LIST:
    return "List";
  default:
    return "unknown";
  }
}

/**
 * printf format and arguments that print a type as its base type and list
 * depth, e.g. "Int (list depth 2)" for List[List[Int]].
 */
#define TYPE_FMT "%s (list depth %zu)"
#define TYPE_ARGS(type) base_type_name((type).base), (type).list_depth

/**
 * @brief Returns the source spelling of a binary operator for error messages.
 * @param kind Kind of a binary operator node.
 * @return Operator text, e.g. "+" or "until".
 */
static const char *operator_name(ASTNodeKind kind) {
  switch (kind) {
  case AST_ADD:
    return "+";
  case AST_SUBTRACT:
    return "-";
  case AST_MULTIPLY:
    return "*";
  case AST_DIVIDE:
    return "/";
  case AST_LESS:
    return "<";
  case AST_LESS_EQUAL:
    return "<=";
  case AST_GREATER:
    return ">";
  case AST_GREATER_EQUAL:
    return ">=";
  case AST_EQUAL:
    return "==";
  case AST_NOT_EQUAL:
    return "!=";
  case AST_AND:
    return "&&";
  case AST_OR:
    return "||";
  case AST_LIST_CONCAT:
    return "++";
  case AST_LIST_INDEXING:
    return "[]";
  case AST_TO:
    return "to";
  case AST_UNTIL:
    return "until";
  case AST_BY:
    return "by";
  default:
    return "?";
  }
}

/** Records an origin for a binary operator applied to wrong operand types. */
#define ERROR_ORIGIN_OPERANDS(node, left, right)                               \
  ERROR_ORIGIN_AT(                                                             \
      (node)->line, (node)->column, NULL,                                      \
      "operator '%s' cannot be applied to " TYPE_FMT " and " TYPE_FMT,         \
      operator_name((node)->kind), TYPE_ARGS(left), TYPE_ARGS(right))

/**
 * @brief Tells whether @p node is a list expression whose element type is not
 * known at compile time.
 * @details This is a static property, not a runtime one: `List(1).drop(5)` is
 * empty at runtime but has the known type List[Int]. Only a literal `List()`
 * lacks an element type; the list-preserving builtins (`drop`, `tail`, `slice`
 * and `take`) retain this property of their receiver. The element type is
 * supplied later by the context where the expression is used.
 */
static bool has_unknown_element_type(const ASTNode *node) {

  /// Null node is not a list
  if (node == NULL) {
    return false;
  }

  /// If the node is a list constructor with no arguments, it has an unknown
  /// element type.
  if (node->kind == AST_LIST_CONSTRUCTOR) {
    return node->data.call.argument_count == 0U;
  }

  /// Apart from `List()`, only a call can carry an unknown element type: a
  /// list-preserving builtin (drop, take, slice, tail) applied to such a
  /// receiver. Method calls are parsed as calls with the receiver passed as
  /// arguments[0]. Any other node kind has a known type (or is an error
  /// reported elsewhere).
  /// The argument_count check guards the arguments[0] access below; calls
  /// without arguments (e.g. readLine()) get here before the callee is checked.
  if (node->kind != AST_FUNCTION_CALL || node->data.call.argument_count == 0U) {
    return false;
  }

  const FunctionSymbol *callee = node->data.call.function;
  bool callee_returns_list =
      callee != NULL && callee->is_builtin &&
      type_is_scalar(callee->return_type.type, TYPE_ANY_LIST);
  return callee_returns_list &&
         has_unknown_element_type(node->data.call.arguments[0]);
}

/**
 * @brief Makes the expression in @p slot usable where @p to is expected.
 * @details Same type is accepted as is; Int is widened to Double by wrapping
 * the expression in an AST_IMPLICIT_INT_TO_REAL node (the only implicit
 * conversion of IFJ26). An empty-list expression takes any expected list type.
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
      ERROR_INTERNAL("cannot allocate an Int-to-Double conversion node");
      return SEMANTIC_INTERNAL_ERROR;
    }
    *slot = conversion;
    return SEMANTIC_OK;
  }
  if (has_unknown_element_type(*slot) && type_is_list(to)) {
    (*slot)->inferred_type.type = to;
    return SEMANTIC_OK;
  }
  if (mismatch == SEMANTIC_INTERNAL_ERROR) {
    ERROR_INTERNAL("cannot convert " TYPE_FMT " to " TYPE_FMT, TYPE_ARGS(from),
                   TYPE_ARGS(to));
  } else {
    ERROR_ORIGIN_AT((*slot)->line, (*slot)->column, NULL,
                    "type mismatch: expected " TYPE_FMT ", got " TYPE_FMT,
                    TYPE_ARGS(to), TYPE_ARGS(from));
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
 * @brief Converts the value(s) a body ends with to @p to.
 * @details Like @ref coerce on @ref last_value_slot, but a final `if`
 * statement (not an expression) is not a value itself: its branches are
 * processed recursively instead, so a conversion is inserted around each
 * branch value rather than around the whole `if`. Branches ending with
 * `return` are skipped; a missing else counts as ().
 * @param slot Slot holding the body; nested slots may be rewritten.
 * @param to Required type of the value.
 * @param mismatch Status returned for incompatible types.
 * @return SEMANTIC_OK, @p mismatch, or SEMANTIC_INTERNAL_ERROR on OOM.
 */
static SemanticStatus coerce_tail(ASTNode **slot, DataType to,
                                  SemanticStatus mismatch) {
  ASTNode *node;
  SemanticStatus status;

  /// The last statement of a body may be a block, so descend to the last
  /// non-block statement (or an empty block).
  slot = last_value_slot(slot);
  node = *slot;
  if (node->kind == AST_RETURN) {
    /// A return statement ends the body with no value
    return SEMANTIC_OK;
  }

  /// A final `if` statement is not a value itself; its branches are processed
  /// recursively instead, so a conversion is inserted around each branch value
  /// rather than around the whole `if`. Branches ending with `return` are
  /// skipped; a missing else counts as ().
  if (node->kind == AST_IF && !node->data.if_statement.used_as_an_expression) {
    status = coerce_tail(&node->data.if_statement.then_branch, to, mismatch);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while matching the then branch of an if");
      return status;
    }
    if (node->data.if_statement.else_branch != NULL) {
      status = coerce_tail(&node->data.if_statement.else_branch, to, mismatch);
      if (status != SEMANTIC_OK) {
        ERROR_TRACE("while matching the else branch of an if");
      }
      return status;
    }
    if (!type_is_scalar(to, TYPE_UNIT)) {
      if (mismatch == SEMANTIC_INTERNAL_ERROR) {
        ERROR_INTERNAL("an if without else has no value of type " TYPE_FMT,
                       TYPE_ARGS(to));
      } else {
        ERROR_ORIGIN_AT(node->line, node->column, NULL,
                        "an if without else has no value, expected " TYPE_FMT,
                        TYPE_ARGS(to));
      }
    }
    return type_is_scalar(to, TYPE_UNIT) ? SEMANTIC_OK : mismatch;
  }

  if (node->kind == AST_ASSIGN || node->kind == AST_BLOCK) {
    /// A definition/assignment or an empty block has the value ().
    return coerce(slot, type_scalar(TYPE_UNIT), to, mismatch);
  }

  /// A final expression is coerced like any other expression.
  return coerce(slot, node->inferred_type.type, to, mismatch);
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
    ERROR_ORIGIN_AT(node->line, node->column, NULL,
                    "call of undefined function '%s'",
                    callee != NULL ? callee->name : "?");
    return SEMANTIC_UNDEFINED;
  }

  /// The number of arguments must match the signature exactly.
  if (node->data.call.argument_count != callee->argument_count) {
    ERROR_ORIGIN_AT(node->line, node->column, NULL,
                    "wrong number of arguments in call to '%s': expected %zu, "
                    "got %zu",
                    callee->name, callee->argument_count,
                    node->data.call.argument_count);
    return SEMANTIC_BAD_CALL;
  }

  /// Arguments only need the callee's signature (known after parsing).
  for (index = 0U; index < node->data.call.argument_count; index += 1U) {
    DataType argument_type;
    DataType parameter_type = callee->parameters[index]->type.type;

    status = check_expression(ctx, function, &node->data.call.arguments[index],
                              &argument_type);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while checking argument %zu of call to '%s'", index + 1U,
                  callee->name);
      return status;
    }

    //// A TYPE_ANY_LIST parameter takes any list (inferable) value without
    /// conversion.
    if (type_is_scalar(parameter_type, TYPE_ANY_LIST)) {
      if (!type_is_list(argument_type) &&
          !has_unknown_element_type(node->data.call.arguments[index])) {
        ERROR_ORIGIN_AT(node->data.call.arguments[index]->line,
                        node->data.call.arguments[index]->column, NULL,
                        "argument %zu of call to '%s' has type " TYPE_FMT
                        ", expected a list",
                        index + 1U, callee->name, TYPE_ARGS(argument_type));
        return SEMANTIC_TYPE_ERROR; /// e.g. head(42)
      }
      continue;
    }

    /// A TYPE_ANY parameter takes any (inferable) value without conversion.
    if (type_is_scalar(parameter_type, TYPE_ANY)) {
      if (type_is_unknown(argument_type) &&
          !has_unknown_element_type(node->data.call.arguments[index])) {
        ERROR_ORIGIN_AT(node->data.call.arguments[index]->line,
                        node->data.call.arguments[index]->column, NULL,
                        "cannot infer the type of argument %zu of call to "
                        "'%s'",
                        index + 1U, callee->name);
        return SEMANTIC_TYPE_ERROR; /// e.g. print(List())
      }
      continue;
    }
    status = coerce(&node->data.call.arguments[index], argument_type,
                    parameter_type, SEMANTIC_BAD_CALL);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while passing argument %zu to '%s'", index + 1U,
                  callee->name);
      return status;
    }
  }

  /// The result type is the only real dependency between functions: if the
  /// callee's type has to be inferred, check the callee first (DFS edge).
  if (!callee->has_explicit_return_type) {
    status = check_function(ctx, callee);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while checking callee '%s' (inferring its return type)",
                  callee->name);
      return status;
    }
  }

  if (type_is_scalar(callee->return_type.type, TYPE_ANY_LIST)) {
    if (!callee->is_builtin) {
      ERROR_INTERNAL("non-builtin '%s' returns an arbitrary list",
                     callee->name);
      return SEMANTIC_INTERNAL_ERROR; /// Only builtins can have TYPE_ANY_LIST.
    }

    /// The result is a list of the same type as the first argument passed
    /// to the builtin. Empty-list expressions remain unknown here;
    /// has_unknown_element_type() follows these calls so their context can
    /// supply the element type.
    *out_type = node->data.call.arguments[0]->inferred_type.type;
  } else if (type_is_scalar(callee->return_type.type, TYPE_ANY)) {
    /// if the functions return type is TYPE_ANY and its first argument is a
    /// list, the return type is the element type of that list

    if (!callee->is_builtin) {
      ERROR_INTERNAL("non-builtin '%s' returns an arbitrary type",
                     callee->name);
      return SEMANTIC_INTERNAL_ERROR; /// Only builtins can return TYPE_ANY.
    }

    /// The result is the element type of the first argument passed to the
    /// builtin.
    DataType element_type;
    if (!type_element_type(node->data.call.arguments[0]->inferred_type.type,
                           &element_type)) {
      /// Return SEMANTIC_ERROR if the first argument is not a list or an empty
      /// untyped list
      ERROR_ORIGIN_AT(node->data.call.arguments[0]->line,
                      node->data.call.arguments[0]->column, NULL,
                      "cannot determine the element type of the list passed "
                      "to '%s'",
                      callee->name);
      return SEMANTIC_TYPE_ERROR;
    }
    *out_type = element_type;
  } else {
    *out_type = callee->return_type.type;
  }
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
      ERROR_TRACE("while checking element %zu of a list literal", index + 1U);
      return status;
    }
    if (index == 0U) {
      element = type;
    } else if (!common_type(element, type, &element)) {
      ERROR_ORIGIN_AT(node->data.call.arguments[index]->line,
                      node->data.call.arguments[index]->column, NULL,
                      "element %zu of a list literal has type " TYPE_FMT
                      ", incompatible with " TYPE_FMT,
                      index + 1U, TYPE_ARGS(type), TYPE_ARGS(element));
      return SEMANTIC_TYPE_ERROR;
    }
  }
  if (type_is_unknown(element)) {
    ERROR_ORIGIN_AT(node->line, node->column, NULL,
                    "cannot infer the element type of a list literal");
    return SEMANTIC_TYPE_ERROR; /// e.g. List(List())
  }

  /// Second pass: widen the elements to the common type.
  for (index = 0U; index < node->data.call.argument_count; index += 1U) {
    status = coerce(&node->data.call.arguments[index],
                    node->data.call.arguments[index]->inferred_type.type,
                    element, SEMANTIC_TYPE_ERROR);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while converting element %zu of a list literal", index + 1U);
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
  if (status != SEMANTIC_OK) {
    ERROR_TRACE("while checking the left operand of '%s'",
                operator_name(node->kind));
    return status;
  }
  status = check_expression(ctx, function, &node->data.binary_op.right, &right);
  if (status != SEMANTIC_OK) {
    ERROR_TRACE("while checking the right operand of '%s'",
                operator_name(node->kind));
    return status;
  }

  switch (node->kind) {
  case AST_AND:
  case AST_OR:
    if (!type_is_scalar(left, TYPE_BOOL) || !type_is_scalar(right, TYPE_BOOL)) {
      ERROR_ORIGIN_OPERANDS(node, left, right);
      return SEMANTIC_TYPE_ERROR;
    }
    *out_type = type_scalar(TYPE_BOOL);
    return SEMANTIC_OK;

  case AST_LIST_CONCAT:
    /// `++` joins two lists of the same type; `List()` takes the other's type.
    if (type_is_list(left) &&
        has_unknown_element_type(node->data.binary_op.right)) {
      right = left;
      node->data.binary_op.right->inferred_type.type = left;
    } else if (type_is_list(right) &&
               has_unknown_element_type(node->data.binary_op.left)) {
      left = right;
      node->data.binary_op.left->inferred_type.type = right;
    }
    if (!type_is_list(left) || !type_equal(left, right)) {
      ERROR_ORIGIN_OPERANDS(node, left, right);
      return SEMANTIC_TYPE_ERROR;
    }
    *out_type = left;
    return SEMANTIC_OK;

  case AST_LIST_INDEXING:
    if (!type_is_list(left) || !type_is_scalar(right, TYPE_INT)) {
      ERROR_ORIGIN_OPERANDS(node, left, right);
      return SEMANTIC_TYPE_ERROR;
    }
    type_element_type(left, out_type);
    return SEMANTIC_OK;

  case AST_TO:
  case AST_UNTIL:
    /// Int range; represented as List[Int] (it supports `toList`, `by`).
    if (!type_is_scalar(left, TYPE_INT) || !type_is_scalar(right, TYPE_INT)) {
      ERROR_ORIGIN_OPERANDS(node, left, right);
      return SEMANTIC_TYPE_ERROR;
    }
    *out_type = type_list(type_scalar(TYPE_INT));
    return SEMANTIC_OK;

  case AST_BY:
    if (!type_equal(left, type_list(type_scalar(TYPE_INT))) ||
        !type_is_scalar(right, TYPE_INT)) {
      ERROR_ORIGIN_OPERANDS(node, left, right);
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
    ERROR_ORIGIN_OPERANDS(node, left, right);
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
    ERROR_ORIGIN_OPERANDS(node, left, right);
    return SEMANTIC_TYPE_ERROR;

  case AST_EQUAL:
  case AST_NOT_EQUAL:
    if (is_numeric(left) && is_numeric(right)) {
      break; /// Numeric comparison, see below.
    }
    /// Any two values of the same type (incl. Boolean, Unit, lists).
    if (type_is_unknown(left) || !type_equal(left, right)) {
      ERROR_ORIGIN_OPERANDS(node, left, right);
      return SEMANTIC_TYPE_ERROR;
    }
    *out_type = type_scalar(TYPE_BOOL);
    return SEMANTIC_OK;

  default:
    ERROR_INTERNAL("unexpected binary operator kind %d", (int)node->kind);
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
    ERROR_TRACE("while widening the operands of '%s'",
                operator_name(node->kind));
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
    ERROR_TRACE("while checking the condition of an if");
    return status;
  }
  if (!type_is_scalar(type, TYPE_BOOL)) {
    ERROR_ORIGIN_AT(node->data.if_statement.condition->line,
                    node->data.if_statement.condition->column, NULL,
                    "if condition must be Boolean, got " TYPE_FMT,
                    TYPE_ARGS(type));
    return SEMANTIC_TYPE_ERROR;
  }
  status = check_statement(ctx, function, &node->data.if_statement.then_branch);
  if (status != SEMANTIC_OK) {
    ERROR_TRACE("while checking the then branch of an if");
  }
  if (status == SEMANTIC_OK && node->data.if_statement.else_branch != NULL) {
    status =
        check_statement(ctx, function, &node->data.if_statement.else_branch);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while checking the else branch of an if");
    }
  }
  return status;
}

/**
 * @brief Determines the value of an already checked `if`.
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
    ERROR_TRACE("while determining the value of an if branch");
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
  status = coerce_tail(&node->data.if_statement.then_branch, common,
                       SEMANTIC_INTERNAL_ERROR);
  if (status == SEMANTIC_OK && node->data.if_statement.else_branch != NULL) {
    status = coerce_tail(&node->data.if_statement.else_branch, common,
                         SEMANTIC_INTERNAL_ERROR);
  }
  if (status != SEMANTIC_OK) {
    ERROR_TRACE("while widening the branch values of an if");
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
 * empty-list expression whose element type has not been supplied by context.
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
    ERROR_INTERNAL("missing expression node");
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
      ERROR_ORIGIN_AT(node->line, node->column, NULL,
                      "variable '%s' used in its own initialization",
                      node->data.variable.symbol->name);
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
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while checking the operand of '!'");
    }
    if (status == SEMANTIC_OK && !type_is_scalar(type, TYPE_BOOL)) {
      ERROR_ORIGIN_AT(node->data.unary_op.operand->line,
                      node->data.unary_op.operand->column, NULL,
                      "operator '!' cannot be applied to " TYPE_FMT,
                      TYPE_ARGS(type));
      return SEMANTIC_TYPE_ERROR;
    }
    break;
  case AST_IMPLICIT_INT_TO_REAL:
    /// The operand of a conversion must be Int; the result is Double.
    status =
        check_expression(ctx, function, &node->data.conversion.operand, &type);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while checking the operand of an Int-to-Double conversion");
    }
    if (status == SEMANTIC_OK && !type_is_scalar(type, TYPE_INT)) {
      ERROR_INTERNAL("Int-to-Double conversion applied to " TYPE_FMT,
                     TYPE_ARGS(type));
      return SEMANTIC_INTERNAL_ERROR;
    }
    type = type_scalar(TYPE_REAL);
    break;
  case AST_IF: {
    /// `if` used as an expression (CONTROL): else is mandatory and the
    /// branches must have a common type.
    BodyResult result;

    if (node->data.if_statement.else_branch == NULL) {
      ERROR_ORIGIN_AT(node->line, node->column, NULL,
                      "an if used as an expression must have an else branch");
      return SEMANTIC_TYPE_ERROR;
    }
    status = check_if_parts(ctx, function, node);
    if (status == SEMANTIC_OK) {
      status = if_result(node, &result);
    }
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while checking an if expression");
      return status;
    }
    if (result.kind != RESULT_VALUE) {
      ERROR_ORIGIN_AT(node->line, node->column, NULL,
                      "the branches of an if expression have no common value");
      return SEMANTIC_TYPE_ERROR;
    }
    type = result.type;
    break;
  }
  case AST_BLOCK:
  case AST_ASSIGN:
  case AST_RETURN:
  case AST_WHILE:
  case AST_FOR:
    /// Statements are not expressions.
    ERROR_INTERNAL("statement node used as an expression");
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
      if (status != SEMANTIC_OK) {
        ERROR_TRACE("while checking statement %zu of a block", index + 1U);
      }
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
      ERROR_ORIGIN_AT(node->line, node->column, NULL,
                      "assignment to immutable variable '%s'", target->name);
      return SEMANTIC_OTHER;
    }
    status =
        check_expression(ctx, function, &node->data.assignment.value, &type);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE(node->data.assignment.is_definition
                      ? "while checking the initializer of '%s'"
                      : "while checking the value assigned to '%s'",
                  target->name);
      return status;
    }
    if (node->data.assignment.is_definition &&
        type_is_unknown(target->type.type)) {
      /// `val x = expr`: the variable takes the initializer's type; it must be
      /// inferable (not e.g. `val x = List()`).
      if (type_is_unknown(type)) {
        ERROR_ORIGIN_AT(node->data.assignment.value->line,
                        node->data.assignment.value->column, NULL,
                        "cannot infer the type of variable '%s' from its "
                        "initializer",
                        target->name);
        return SEMANTIC_TYPE_ERROR;
      }
      target->type.type = type;
    } else {
      /// The variable already has a type: the value must match or be widened.
      status = coerce(&node->data.assignment.value, type, target->type.type,
                      SEMANTIC_TYPE_ERROR);
      if (status != SEMANTIC_OK) {
        ERROR_TRACE("while assigning to variable '%s' of type " TYPE_FMT,
                    target->name, TYPE_ARGS(target->type.type));
        return status;
      }
    }
    node->inferred_type.type = type_scalar(TYPE_UNIT);
    return SEMANTIC_OK;
  }

  case AST_RETURN:
    if (!function->has_explicit_return_type) {
      /// Already excluded, kept for safety.
      ERROR_ORIGIN_AT(node->line, node->column, NULL,
                      "'return' in function '%s' without a declared return "
                      "type",
                      function->name);
      return SEMANTIC_TYPE_ERROR;
    }
    if (node->data.return_statement.value == NULL) {
      /// A bare `return` is allowed only with an explicit Unit return type.
      if (!type_is_scalar(function->return_type.type, TYPE_UNIT)) {
        ERROR_ORIGIN_AT(
            node->line, node->column, NULL,
            "'return' without a value in function '%s' returning " TYPE_FMT,
            function->name, TYPE_ARGS(function->return_type.type));
        return SEMANTIC_TYPE_ERROR;
      }
      /// Attach an implicit Unit constant so that every return has a value.
      ASTNode *unit = ast_node_create_unit(node->line, node->column);
      if (unit == NULL) {
        ERROR_INTERNAL("cannot allocate the value of a bare 'return'");
        return SEMANTIC_INTERNAL_ERROR;
      }
      node->data.return_statement.value = unit;
      node->inferred_type.type = type_scalar(TYPE_UNIT);
      return SEMANTIC_OK;
    }
    status = check_expression(ctx, function, &node->data.return_statement.value,
                              &type);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while checking the returned value");
      return status;
    }
    node->inferred_type.type = type;
    if (type_is_scalar(function->return_type.type, TYPE_UNIT)) {
      /// The value is evaluated and discarded (rule 4).
      return SEMANTIC_OK;
    }
    status = coerce(&node->data.return_statement.value, type,
                    function->return_type.type, SEMANTIC_TYPE_ERROR);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while matching the returned value with the return type of "
                  "'%s'",
                  function->name);
    }
    return status;

  case AST_IF:
    if (node->data.if_statement.used_as_an_expression) {
      return check_expression(ctx, function, slot, &type);
    }
    /// An `if` statement; its value (if needed) is given by statement_result.
    status = check_if_parts(ctx, function, node);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while checking an if statement");
    }
    if (status == SEMANTIC_OK) {
      node->inferred_type.type = type_scalar(TYPE_UNIT);
    }
    return status;

  case AST_WHILE:
    /// The condition is any Boolean expression (BOOLTHEN).
    status = check_expression(ctx, function, &node->data.while_loop.condition,
                              &type);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while checking the condition of a while loop");
      return status;
    }
    if (!type_is_scalar(type, TYPE_BOOL)) {
      ERROR_ORIGIN_AT(node->data.while_loop.condition->line,
                      node->data.while_loop.condition->column, NULL,
                      "while condition must be Boolean, got " TYPE_FMT,
                      TYPE_ARGS(type));
      return SEMANTIC_TYPE_ERROR;
    }
    status = check_statement(ctx, function, &node->data.while_loop.body);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while checking the body of a while loop");
    }
    if (status == SEMANTIC_OK) {
      /// A loop yields ().
      node->inferred_type.type = type_scalar(TYPE_UNIT);
    }
    return status;

  case AST_FOR: {
    /// The source is a list (ranges are List[Int]); the control variable
    /// takes its element type before the guard and body are checked.
    Symbol *variable = node->data.for_loop.variable;
    DataType element;

    status =
        check_expression(ctx, function, &node->data.for_loop.iterable, &type);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while checking the iterable of a for loop over '%s'",
                  variable->name);
      return status;
    }
    if (!type_element_type(type, &element)) {
      /// Not a list, or an empty `List()` without an element type.
      ERROR_ORIGIN_AT(
          node->data.for_loop.iterable->line,
          node->data.for_loop.iterable->column, NULL,
          "for loop needs a list with a known element type, got " TYPE_FMT,
          TYPE_ARGS(type));
      return SEMANTIC_TYPE_ERROR;
    }
    variable->type.type = element;

    if (node->data.for_loop.guard != NULL) {
      status =
          check_expression(ctx, function, &node->data.for_loop.guard, &type);
      if (status != SEMANTIC_OK) {
        ERROR_TRACE("while checking the guard of a for loop");
        return status;
      }
      if (!type_is_scalar(type, TYPE_BOOL)) {
        ERROR_ORIGIN_AT(node->data.for_loop.guard->line,
                        node->data.for_loop.guard->column, NULL,
                        "for loop guard must be Boolean, got " TYPE_FMT,
                        TYPE_ARGS(type));
        return SEMANTIC_TYPE_ERROR;
      }
    }
    status = check_statement(ctx, function, &node->data.for_loop.body);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while checking the body of a for loop");
    }
    if (status == SEMANTIC_OK) {
      node->inferred_type.type = type_scalar(TYPE_UNIT);
    }
    return status;
  }

  default:
    /// Expression statement (incl. a call whose result is dropped).
    return check_expression(ctx, function, slot, &type);
  }
}

/**
 * @brief Determines the result of a statement as the last one of a block.
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
  case AST_WHILE:
  case AST_FOR:
    /// Definition/assignment/loop: ()
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
 * @brief Creates a typed `return ()` at the position of @p at.
 * @param at Node whose source position the new nodes take.
 * @return An owned return node, or null on allocation failure.
 */
static ASTNode *create_unit_return(const ASTNode *at) {
  ASTNode *unit = ast_node_create_unit(at->line, at->column);
  ASTNode *ret;

  if (unit == NULL) {
    return NULL;
  }
  ret = ast_node_create_return(unit, at->line, at->column);
  if (ret == NULL) {
    ast_node_destroy(unit);
    return NULL;
  }
  ret->inferred_type.type = type_scalar(TYPE_UNIT);
  return ret;
}

/**
 * @brief Places a `return ()` right after the statement in @p slot.
 * @details Two cases are distinguished by @p parent_block:
 *
 * @code
 *   BLOCK                 BLOCK
 *     ASSIGN       ->       ASSIGN
 *                           RETURN
 *                             UNIT
 *
 *   IF                    IF
 *     cond                  cond
 *     ASSIGN       ->       BLOCK        <- new block in the branch slot
 *     ...                     ASSIGN     <- the original statement
 *                             RETURN
 *                               UNIT
 *                           ...
 * @endcode
 * @param slot Slot holding the last statement of a body.
 * @param parent_block Block whose last statement is in @p slot, or null.
 * @return SEMANTIC_OK or SEMANTIC_INTERNAL_ERROR on OOM.
 */
static SemanticStatus append_unit_return(ASTNode **slot,
                                         ASTNode *parent_block) {
  ASTNode *statement = *slot;
  ASTNode *ret = create_unit_return(statement);
  ASTNode *block;

  if (ret == NULL) {
    ERROR_INTERNAL("cannot allocate an implicit 'return ()'");
    return SEMANTIC_INTERNAL_ERROR;
  }

  /// The return is appended to the parent block when the statement is that
  /// block's last one; otherwise the statement is wrapped in a new block.
  if (parent_block != NULL) {
    if (ast_node_block_append_statement(parent_block, ret) != AST_OK) {
      ast_node_destroy(ret);
      ERROR_INTERNAL("cannot append an implicit 'return ()' to a block");
      return SEMANTIC_INTERNAL_ERROR;
    }
    return SEMANTIC_OK;
  }

  /// Case 2: the statement is a bare `if` branch; wrap it in a new block
  /// `{ statement; return () }` that replaces it in the branch slot.
  block = ast_node_create_block(statement->line, statement->column);
  if (block == NULL) {
    ast_node_destroy(ret);
    ERROR_INTERNAL("cannot allocate a block for an implicit 'return ()'");
    return SEMANTIC_INTERNAL_ERROR;
  }
  block->inferred_type.type = type_scalar(TYPE_UNIT);
  if (ast_node_block_append_statement(block, statement) != AST_OK) {
    ast_node_destroy(block);
    ast_node_destroy(ret);
    ERROR_INTERNAL("cannot move a statement into a new block");
    return SEMANTIC_INTERNAL_ERROR;
  }
  /// The block owns the statement from now on.
  *slot = block;
  if (ast_node_block_append_statement(block, ret) != AST_OK) {
    ast_node_destroy(ret);
    ERROR_INTERNAL("cannot append an implicit 'return ()' to a new block");
    return SEMANTIC_INTERNAL_ERROR;
  }
  return SEMANTIC_OK;
}

/**
 * @brief Makes every path through an already checked body end with an
 * explicit `return` that has a value.
 * @details In a Unit function the final statement is kept (evaluated and
 * dropped) and a `return ()` is placed after it. In other functions the final
 * value expression is wrapped in a return. A final `if` statement is handled
 * per branch; in a Unit function a missing else gets a `return ()` after the
 * `if`. Existing returns are left untouched.
 * @param slot Slot holding the body (or the tail statement being processed).
 * @param parent_block Block whose last statement is in @p slot, or null.
 * @param unit_function True when the function returns Unit.
 * @return SEMANTIC_OK or SEMANTIC_INTERNAL_ERROR on OOM.
 */
static SemanticStatus add_implicit_return(ASTNode **slot, ASTNode *parent_block,
                                          bool unit_function) {
  ASTNode *node = *slot;
  ASTNode *ret;
  SemanticStatus status;

  switch (node->kind) {
  case AST_RETURN:
    /// Already a return, nothing to do.
    return SEMANTIC_OK;

  case AST_BLOCK:
    /// The block itself is Unit, but its last statement may be a value
    /// expression.
    if (node->data.block.statement_count == 0U) {
      /// An empty block has the value (), valid only in a Unit function.
      ret = create_unit_return(node);
      if (ret == NULL) {
        ERROR_INTERNAL("cannot allocate an implicit 'return ()'");
        return SEMANTIC_INTERNAL_ERROR;
      }
      if (ast_node_block_append_statement(node, ret) != AST_OK) {
        ast_node_destroy(ret);
        ERROR_INTERNAL("cannot append an implicit 'return ()' to a block");
        return SEMANTIC_INTERNAL_ERROR;
      }
      return SEMANTIC_OK;
    }
    return add_implicit_return(
        &node->data.block.statements[node->data.block.statement_count - 1U],
        node, unit_function);

  case AST_IF:
    /// The branches are checked separately; a missing else is handled below.
    if (node->data.if_statement.used_as_an_expression) {
      break;
    }
    status = add_implicit_return(&node->data.if_statement.then_branch, NULL,
                                 unit_function);
    if (status != SEMANTIC_OK) {
      ERROR_TRACE("while adding an implicit return to the then branch of an "
                  "if");
      return status;
    }
    if (node->data.if_statement.else_branch != NULL) {
      return add_implicit_return(&node->data.if_statement.else_branch, NULL,
                                 unit_function);
    }
    /// A missing else behaves as () (only valid in a Unit function).
    return unit_function ? append_unit_return(slot, parent_block) : SEMANTIC_OK;

  default:
    break;
  }

  if (unit_function) {
    /// The final statement is evaluated and its value is dropped; a `return ()`
    /// is placed after it.
    return append_unit_return(slot, parent_block);
  }

  /// The final value becomes the returned value.
  ret = ast_node_create_return(node, node->line, node->column);
  if (ret == NULL) {
    ERROR_INTERNAL("cannot allocate an implicit 'return'");
    return SEMANTIC_INTERNAL_ERROR;
  }
  ret->inferred_type.type = node->inferred_type.type;
  *slot = ret;
  return SEMANTIC_OK;
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
    ERROR_INTERNAL("function '%s' is already being checked (unexpected cycle)",
                   function->name);
    return SEMANTIC_INTERNAL_ERROR;
  }
  if (function->ast_root == NULL) {
    ERROR_INTERNAL("function '%s' has no body", function->name);
    return SEMANTIC_INTERNAL_ERROR;
  }
  function->check_state = FUNCTION_CHECKING;

  status = check_statement(ctx, function, &function->ast_root);
  if (status != SEMANTIC_OK) {
    ERROR_TRACE("while checking the body of function '%s'", function->name);
    return status;
  }

  /// Compare the value of the body with the (declared or inferred) return type.
  status = statement_result(function->ast_root, &result);
  if (status != SEMANTIC_OK) {
    ERROR_TRACE("while determining the value of the body of function '%s'",
                function->name);
    return status;
  }
  if (function->has_explicit_return_type) {
    DataType declared = function->return_type.type;

    /// With a declared Unit type the final value is discarded.
    if (!type_is_scalar(declared, TYPE_UNIT) && result.kind != RESULT_RETURNS) {
      if (result.kind == RESULT_BAD_IF) {
        ERROR_ORIGIN_FUNCTION(function,
                              "function '%s' must return a value, but its "
                              "body ends with an if without a value",
                              function->name);
        return SEMANTIC_TYPE_ERROR;
      }
      status = coerce_tail(&function->ast_root, declared, SEMANTIC_TYPE_ERROR);
      if (status != SEMANTIC_OK) {
        ERROR_TRACE("while matching the body of function '%s' with its "
                    "declared return type",
                    function->name);
        return status;
      }
    }
  } else {
    /// No `return` here (checked), so the result is the value of the body.
    if (result.kind != RESULT_VALUE || type_is_unknown(result.type)) {
      ERROR_ORIGIN_FUNCTION(function,
                            "cannot infer the return type of function '%s'",
                            function->name);
      return SEMANTIC_TYPE_ERROR;
    }
    function->return_type.type = result.type;
  }

  /// Make the implicit return at the end of the body explicit.
  status = add_implicit_return(
      &function->ast_root, NULL,
      type_is_scalar(function->return_type.type, TYPE_UNIT));
  if (status != SEMANTIC_OK) {
    ERROR_TRACE("while adding the implicit return of function '%s'",
                function->name);
    return status;
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
    ERROR_INTERNAL("semantic analysis of an uninitialized program");
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

  /// 1. Every referenced function exists; exactly one `@main` exists.
  for (index = 0U; index < table->function_count; index += 1U) {
    FunctionSymbol *function = table->functions[index];
    if (!function->is_defined) {
      ERROR_ORIGIN_AT(0U, 0U, NULL, "call of undefined function '%s'",
                      function->name);
      return SEMANTIC_UNDEFINED;
    }
    if (function->is_main) {
      if (main_function != NULL) {
        ERROR_ORIGIN_FUNCTION(function, "redefinition of the main function");
        return SEMANTIC_REDEFINITION;
      }
      main_function = function;
    }
  }
  if (main_function == NULL) {
    ERROR_ORIGIN_AT(0U, 0U, NULL, "missing main function ('@main')");
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
      ERROR_TRACE("while building the call graph of function '%s'",
                  function->name);
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
      ERROR_ORIGIN_FUNCTION(function,
                            "function '%s' contains 'return' but has no "
                            "declared return type",
                            function->name);
      return SEMANTIC_TYPE_ERROR;
    }
    ctx.mark += 1U;
    if (reaches(function, function, ctx.mark)) {
      ERROR_ORIGIN_FUNCTION(function,
                            "recursive function '%s' has no declared return "
                            "type",
                            function->name);
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
    ERROR_ORIGIN_FUNCTION(main_function,
                          "main function '%s' must not have parameters",
                          main_function->name);
    return SEMANTIC_BAD_CALL;
  }
  if (!type_is_scalar(main_function->return_type.type, TYPE_UNIT)) {
    ERROR_ORIGIN_FUNCTION(main_function, "main function '%s' must return Unit",
                          main_function->name);
    return SEMANTIC_TYPE_ERROR;
  }
  return SEMANTIC_OK;
}
