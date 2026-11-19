/**
 * @file loop_test.cc
 * @brief Parser and semantic tests of while and for loops.
 */

#include <string>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "../src/parser.h"
#include "../src/semantic.h"
}

namespace {

/** Valid main function added to parsed test programs. */
const char *const kMainFunction = "@main def added_main(): Unit =\n"
                                  "  return\n";

/**
 * @brief Owns one parsed program (a valid main function is prepended).
 * @details Keeps the lexer, the program and the source buffer alive, so the
 * AST can be inspected after parsing.
 */
class LoopTest : public ::testing::Test {
protected:
  std::vector<char> source;
  Lexer lexer{};
  Program program{};
  bool lexer_ready = false;

  void TearDown() override {
    program_destroy(&program);
    if (lexer_ready) {
      lexer_destroy(&lexer);
    }
  }

  /** Parses @p text preceded by a valid main function. */
  ParserStatus parse(const char *text) {
    std::string source_text = std::string(kMainFunction) + text;
    source.clear();
    for (char c : source_text) {
      source.push_back(c);
    }
    source.push_back('\0');
    if (lexer_init(&lexer, source.data()) != LEXER_OK) {
      return PARSER_INTERNAL_ERROR;
    }
    lexer_ready = true;
    ParserStatus status = program_init(&program);
    if (status != PARSER_OK) {
      return status;
    }
    return p_program(&lexer, &program);
  }

  /** Parses @p text and runs the semantic pass; returns the exit code. */
  int compile(const char *text) {
    ParserStatus status = parse(text);
    if (status != PARSER_OK) {
      return parser_status_exit_code(status);
    }
    return (int)semantic_check(&program);
  }

  /** Returns the root block of function @p name. */
  ASTNode *body_of(const char *name) {
    FunctionSymbol *function = function_table_lookup(&program.functions, name);
    if (function == nullptr) {
      return nullptr;
    }
    return function->ast_root;
  }
};

} // namespace

/* ------------------------------------------------------------------------- */
/* while                                                                     */
/* ------------------------------------------------------------------------- */

TEST_F(LoopTest, WhileWithBlockBodyIsFollowedByStatement) {
  ASSERT_EQ(parse("def foo(n: Int) =\n"
                  "  while n > 1 do\n"
                  "    print(n)\n"
                  "    val a = 1\n"
                  "  val b = 2\n"),
            PARSER_OK);

  ASTNode *body = body_of("foo");
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->kind, AST_BLOCK);
  ASSERT_EQ(body->data.block.statement_count, 2U);
  ASTNode *loop = body->data.block.statements[0];
  ASSERT_EQ(loop->kind, AST_WHILE);
  EXPECT_EQ(loop->data.while_loop.condition->kind, AST_GREATER);
  ASSERT_EQ(loop->data.while_loop.body->kind, AST_BLOCK);
  EXPECT_EQ(loop->data.while_loop.body->data.block.statement_count, 2U);
  EXPECT_EQ(body->data.block.statements[1]->kind, AST_ASSIGN);
}

TEST_F(LoopTest, WhileWithOneLineBody) {
  ASSERT_EQ(parse("def foo(n: Int) =\n"
                  "  while n > 1 do print(n)\n"
                  "  val b = 2\n"),
            PARSER_OK);

  ASTNode *body = body_of("foo");
  ASSERT_EQ(body->data.block.statement_count, 2U);
  ASTNode *loop = body->data.block.statements[0];
  ASSERT_EQ(loop->kind, AST_WHILE);
  EXPECT_EQ(loop->data.while_loop.body->kind, AST_FUNCTION_CALL);
}

TEST_F(LoopTest, WhileAtEndOfFileWithoutNewline) {
  EXPECT_EQ(parse("def foo(n: Int) =\n"
                  "  while n > 1 do\n"
                  "    print(n)"),
            PARSER_OK);
}

TEST_F(LoopTest, WhileAsLastStatementOfThenBlockBeforeElse) {
  ASSERT_EQ(parse("def foo(n: Int) =\n"
                  "  if n > 0 then\n"
                  "    while n > 1 do\n"
                  "      print(n)\n"
                  "  else\n"
                  "    print(0)\n"
                  "  print(1)\n"),
            PARSER_OK);

  ASTNode *body = body_of("foo");
  ASSERT_EQ(body->data.block.statement_count, 2U);
  ASTNode *conditional = body->data.block.statements[0];
  ASSERT_EQ(conditional->kind, AST_IF);
  EXPECT_NE(conditional->data.if_statement.else_branch, nullptr);
}

TEST_F(LoopTest, WhileBodyEndingWithOneLineIf) {
  ASSERT_EQ(parse("def foo(n: Int) =\n"
                  "  while n > 1 do if n > 2 then print(n) else print(0)\n"
                  "  print(1)\n"),
            PARSER_OK);
  ASSERT_EQ(body_of("foo")->data.block.statement_count, 2U);
}

TEST_F(LoopTest, NestedLoops) {
  ASSERT_EQ(parse("def foo(n: Int) =\n"
                  "  while n > 1 do\n"
                  "    for i <- 1 to n do\n"
                  "      while i < 0 do\n"
                  "        print(i)\n"
                  "  print(1)\n"),
            PARSER_OK);

  ASTNode *body = body_of("foo");
  ASSERT_EQ(body->data.block.statement_count, 2U);
  ASTNode *outer = body->data.block.statements[0];
  ASSERT_EQ(outer->kind, AST_WHILE);
  ASTNode *inner = outer->data.while_loop.body->data.block.statements[0];
  ASSERT_EQ(inner->kind, AST_FOR);
  ASSERT_EQ(inner->data.for_loop.body->kind, AST_BLOCK);
  EXPECT_EQ(inner->data.for_loop.body->data.block.statements[0]->kind,
            AST_WHILE);
}

TEST_F(LoopTest, WhileSyntaxErrors) {
  EXPECT_EQ(parse("def foo(n: Int) =\n"
                  "  while n > 1\n"
                  "    print(n)\n"),
            PARSER_SYNTAX_ERROR);
}

TEST_F(LoopTest, WhileMissingDo) {
  EXPECT_EQ(parse("def foo(n: Int) =\n"
                  "  while n > 1 print(n)\n"),
            PARSER_SYNTAX_ERROR);
}

TEST_F(LoopTest, WhileEmptyBody) {
  EXPECT_EQ(parse("def foo(n: Int) =\n"
                  "  while n > 1 do\n"
                  "  print(n)\n"),
            PARSER_SYNTAX_ERROR);
}

TEST_F(LoopTest, WhileBodyIsSeparateScope) {
  EXPECT_EQ(parse("def foo(n: Int) =\n"
                  "  while n > 1 do\n"
                  "    val a = 1\n"
                  "  print(a)\n"),
            PARSER_UNDEFINED_ERROR);
}

TEST_F(LoopTest, WhileBodyMayShadow) {
  EXPECT_EQ(compile("def foo(n: Int) =\n"
                    "  val a = 1\n"
                    "  while n > 1 do\n"
                    "    val a = \"x\"\n"
                    "    print(a)\n"),
            0);
}

/* ------------------------------------------------------------------------- */
/* for                                                                       */
/* ------------------------------------------------------------------------- */

TEST_F(LoopTest, ForOverRangeWithStep) {
  ASSERT_EQ(parse("def foo() =\n"
                  "  for i <- 1 to 10 by 2 do\n"
                  "    print(i)\n"
                  "  print(0)\n"),
            PARSER_OK);

  ASTNode *body = body_of("foo");
  ASSERT_EQ(body->data.block.statement_count, 2U);
  ASTNode *loop = body->data.block.statements[0];
  ASSERT_EQ(loop->kind, AST_FOR);
  ASSERT_NE(loop->data.for_loop.variable, nullptr);
  EXPECT_STREQ(loop->data.for_loop.variable->name, "i");
  EXPECT_TRUE(loop->data.for_loop.variable->constant);
  EXPECT_EQ(loop->data.for_loop.guard, nullptr);
  ASTNode *source = loop->data.for_loop.iterable;
  ASSERT_EQ(source->kind, AST_BY);
  EXPECT_EQ(source->data.binary_op.left->kind, AST_TO);
  EXPECT_EQ(source->data.binary_op.right->kind, AST_CONSTANT_INT);

  /// The body refers to the control variable.
  ASTNode *call = loop->data.for_loop.body->data.block.statements[0];
  ASSERT_EQ(call->kind, AST_FUNCTION_CALL);
  ASSERT_EQ(call->data.call.arguments[0]->kind, AST_VARIABLE_REF);
  EXPECT_EQ(call->data.call.arguments[0]->data.variable.symbol,
            loop->data.for_loop.variable);
}

TEST_F(LoopTest, ForOverUntilOneLine) {
  ASSERT_EQ(parse("def foo(n: Int) =\n"
                  "  for i <- 0 until n do print(i)\n"
                  "  print(0)\n"),
            PARSER_OK);

  ASTNode *loop = body_of("foo")->data.block.statements[0];
  ASSERT_EQ(loop->kind, AST_FOR);
  EXPECT_EQ(loop->data.for_loop.iterable->kind, AST_UNTIL);
  EXPECT_EQ(loop->data.for_loop.body->kind, AST_FUNCTION_CALL);
}

TEST_F(LoopTest, ForOverListVariable) {
  ASSERT_EQ(compile("def foo() =\n"
                    "  val xs = List(\"a\", \"b\")\n"
                    "  for s <- xs do\n"
                    "    val t: String = s\n"
                    "    print(t)\n"),
            0);

  ASTNode *loop = body_of("foo")->data.block.statements[1];
  ASSERT_EQ(loop->kind, AST_FOR);
  EXPECT_EQ(loop->data.for_loop.iterable->kind, AST_VARIABLE_REF);
  /// The control variable takes the element type.
  EXPECT_TRUE(
      type_is_scalar(loop->data.for_loop.variable->type.type, TYPE_STRING));
}

TEST_F(LoopTest, ForSingleLineGuards) {
  ASSERT_EQ(parse("def foo() =\n"
                  "  for i <- 1 to 10 if i != 3 if i < 8 do print(i)\n"),
            PARSER_OK);

  ASTNode *loop = body_of("foo")->data.block.statements[0];
  ASSERT_EQ(loop->kind, AST_FOR);
  ASTNode *guard = loop->data.for_loop.guard;
  ASSERT_NE(guard, nullptr);
  ASSERT_EQ(guard->kind, AST_AND);
  EXPECT_EQ(guard->data.binary_op.left->kind, AST_NOT_EQUAL);
  EXPECT_EQ(guard->data.binary_op.right->kind, AST_LESS);
}

TEST_F(LoopTest, ForMultilineHeaderWithGuards) {
  ASSERT_EQ(compile("def foo() =\n"
                    "  val limit = 10\n"
                    "  for\n"
                    "    i <- 0 until limit\n"
                    "    if i != 3\n"
                    "    if i < 8\n"
                    "  do\n"
                    "    print(i)\n"
                    "  print(0)\n"),
            0);

  ASTNode *body = body_of("foo");
  ASSERT_EQ(body->data.block.statement_count, 4U);
  ASTNode *loop = body->data.block.statements[1];
  ASSERT_EQ(loop->kind, AST_FOR);
  EXPECT_EQ(loop->data.for_loop.iterable->kind, AST_UNTIL);
  ASSERT_NE(loop->data.for_loop.guard, nullptr);
  EXPECT_EQ(loop->data.for_loop.guard->kind, AST_AND);
  EXPECT_EQ(body->data.block.statements[2]->kind, AST_FUNCTION_CALL);
}

TEST_F(LoopTest, ForMultilineHeaderWithOneLineBody) {
  EXPECT_EQ(parse("def foo() =\n"
                  "  for\n"
                  "    i <- 0 until 5\n"
                  "  do print(i)\n"
                  "  print(0)\n"),
            PARSER_OK);
}

TEST_F(LoopTest, ForMultilineHeaderErrors) {
  /// `do` must be aligned with `for`.
  EXPECT_EQ(parse("def foo() =\n"
                  "  for\n"
                  "    i <- 0 until 5\n"
                  "    do print(i)\n"),
            PARSER_SYNTAX_ERROR);
}

TEST_F(LoopTest, ForMultipleGeneratorsAreRejected) {
  EXPECT_EQ(parse("def foo() =\n"
                  "  for\n"
                  "    i <- 0 until 5\n"
                  "    j <- 0 until 5\n"
                  "  do print(i)\n"),
            PARSER_SYNTAX_ERROR);
}

TEST_F(LoopTest, ForGeneratorOnNextLineOfOneLineFormIsRejected) {
  EXPECT_EQ(parse("def foo() =\n"
                  "  for i <- 0 until 5\n"
                  "    if i > 1\n"
                  "  do print(i)\n"),
            PARSER_SYNTAX_ERROR);
}

TEST_F(LoopTest, ForSyntaxErrors) {
  EXPECT_EQ(parse("def foo() =\n"
                  "  for i = 0 until 5 do print(i)\n"),
            PARSER_SYNTAX_ERROR);
}

TEST_F(LoopTest, ForMissingDo) {
  EXPECT_EQ(parse("def foo() =\n"
                  "  for i <- 0 until 5 print(i)\n"),
            PARSER_SYNTAX_ERROR);
}

TEST_F(LoopTest, ForVariableIsNotVisibleAfterLoop) {
  EXPECT_EQ(parse("def foo() =\n"
                  "  for i <- 0 until 5 do\n"
                  "    print(i)\n"
                  "  print(i)\n"),
            PARSER_UNDEFINED_ERROR);
}

TEST_F(LoopTest, ForSourceDoesNotSeeControlVariable) {
  EXPECT_EQ(parse("def foo() =\n"
                  "  for i <- 0 until i do print(i)\n"),
            PARSER_UNDEFINED_ERROR);
  /// An outer `i` is used in the source, the inner one in the body.
}

TEST_F(LoopTest, ForSourceUsesOuterVariableOfSameName) {
  EXPECT_EQ(compile("def foo() =\n"
                    "  val i = \"x\"\n"
                    "  for i <- List(1, 2) do\n"
                    "    val k: Int = i\n"
                    "    print(k)\n"
                    "  print(i)\n"),
            0);
}

TEST_F(LoopTest, ForBodyMayShadowControlVariable) {
  EXPECT_EQ(compile("def foo() =\n"
                    "  for i <- 0 until 3 do\n"
                    "    val i = \"x\"\n"
                    "    print(i)\n"),
            0);
}

/* ------------------------------------------------------------------------- */
/* Semantics                                                                 */
/* ------------------------------------------------------------------------- */

TEST_F(LoopTest, SpecFactorialStyleProgram) {
  EXPECT_EQ(compile("def foo(n: Int): Unit =\n"
                    "  var result = 1\n"
                    "  val i: Int = n\n"
                    "  while i > 1 do\n"
                    "    print(result)\n"),
            0);
}

TEST_F(LoopTest, WhileConditionMustBeBoolean) {
  EXPECT_EQ(compile("def foo(n: Int) =\n"
                    "  while n do print(n)\n"),
            6);
}

TEST_F(LoopTest, ForGuardMustBeBoolean) {
  EXPECT_EQ(compile("def foo() =\n"
                    "  for i <- 1 to 3 if i do print(i)\n"),
            6);
}

TEST_F(LoopTest, ForSourceMustBeList) {
  EXPECT_EQ(compile("def foo() =\n"
                    "  for i <- 5 do print(i)\n"),
            6);
  /// Range bounds must be Int.
}

TEST_F(LoopTest, ForRangeBoundsMustBeInt) {
  EXPECT_EQ(compile("def foo() =\n"
                    "  for i <- 1 to 2.5 do print(i)\n"),
            6);
}

TEST_F(LoopTest, ForControlVariableIsImmutableInt) {
  /// Assignment is not parsed yet; use the variable as an Int instead.
  EXPECT_EQ(compile("def foo() =\n"
                    "  for i <- 1 to 3 do\n"
                    "    val s: String = i\n"),
            6);
}

TEST_F(LoopTest, LoopBodyEndingWithIfWithoutElse) {
  /// The value of a loop body is discarded, so its final if needs no else.
  EXPECT_EQ(compile("def foo(n: Int) =\n"
                    "  while n > 1 do\n"
                    "    if n > 2 then\n"
                    "      print(n)\n"
                    "  for i <- 1 to 3 do\n"
                    "    if i > 2 then print(i)\n"),
            0);
}

TEST_F(LoopTest, FunctionEndingWithLoopInfersUnit) {
  ASSERT_EQ(compile("def foo(n: Int) =\n"
                    "  while n > 1 do\n"
                    "    val a = 1\n"),
            0);
  FunctionSymbol *function = function_table_lookup(&program.functions, "foo");
  EXPECT_TRUE(type_is_scalar(function->return_type.type, TYPE_UNIT));
}

TEST_F(LoopTest, FunctionEndingWithLoopCannotReturnInt) {
  EXPECT_EQ(compile("def foo(n: Int): Int =\n"
                    "  while n > 1 do\n"
                    "    val a = 1\n"),
            6);
}

TEST_F(LoopTest, ReturnInsideLoopRequiresExplicitType) {
  EXPECT_EQ(compile("def foo(n: Int) =\n"
                    "  while n > 1 do\n"
                    "    return 1\n"
                    "  2\n"),
            6);
  /// With an explicit type it is fine.
}

TEST_F(LoopTest, ReturnInsideLoopWithExplicitType) {
  EXPECT_EQ(compile("def foo(n: Int): Int =\n"
                    "  for i <- 0 until n do\n"
                    "    return i\n"
                    "  2\n"),
            0);
}

TEST_F(LoopTest, CallInsideLoopIsInCallGraph) {
  /// Recursion through a loop body requires an explicit return type.
  EXPECT_EQ(compile("def foo(n: Int) =\n"
                    "  while n > 1 do\n"
                    "    foo(n)\n"),
            6);
}

TEST_F(LoopTest, ForOverEmptyListWithoutTypeIsError) {
  EXPECT_EQ(compile("def foo() =\n"
                    "  for i <- List() do print(i)\n"),
            6);
}

/* ------------------------------------------------------------------------- */
/* `if` ends an expression only as a for-loop guard                          */
/* ------------------------------------------------------------------------- */

TEST_F(LoopTest, IfAfterOneLineThenBranchIsRejected) {
  EXPECT_EQ(parse("def foo() =\n"
                  "  if true then print(1) if true then print(2)\n"),
            PARSER_SYNTAX_ERROR);
}

TEST_F(LoopTest, IfAfterOneLineElseBranchIsRejected) {
  EXPECT_EQ(parse("def foo() =\n"
                  "  val v = if true then 1 else 2 if true then print(2)\n"),
            PARSER_SYNTAX_ERROR);
}

TEST_F(LoopTest, IfAfterOneLineLoopBodyInBranchIsRejected) {
  EXPECT_EQ(parse("def foo() =\n"
                  "  if true then while true do print(1) if true then "
                  "print(2)\n"),
            PARSER_SYNTAX_ERROR);
}

TEST_F(LoopTest, IfAfterOneLineLoopBodyIsRejected) {
  EXPECT_EQ(parse("def foo() =\n"
                  "  while true do print(1) if true then print(2)\n"),
            PARSER_SYNTAX_ERROR);
}

/* ------------------------------------------------------------------------- */
/* Final `if` of a block: the semantic pass decides whether its value is used */
/* ------------------------------------------------------------------------- */

TEST_F(LoopTest, UnitFunctionMayEndWithIfWithoutElse) {
  EXPECT_EQ(compile("def foo(n: Int): Unit =\n"
                    "  if n > 0 then\n"
                    "    print(n)\n"),
            0);
}

TEST_F(LoopTest, UnitFunctionMayEndWithIfWithDifferentBranchTypes) {
  EXPECT_EQ(compile("def foo(n: Int): Unit =\n"
                    "  if n > 0 then\n"
                    "    1\n"
                    "  else\n"
                    "    \"a\"\n"),
            0);
}

TEST_F(LoopTest, FunctionEndingWithIfWithoutElseInfersUnit) {
  ASSERT_EQ(compile("def foo(n: Int) =\n"
                    "  if n > 0 then\n"
                    "    print(n)\n"),
            0);
  FunctionSymbol *function = function_table_lookup(&program.functions, "foo");
  ASSERT_NE(function, nullptr);
  EXPECT_TRUE(type_is_scalar(function->return_type.type, TYPE_UNIT));
}

TEST_F(LoopTest, IntFunctionEndingWithIfWithoutElseIsError6) {
  EXPECT_EQ(compile("def foo(n: Int): Int =\n"
                    "  if n > 0 then\n"
                    "    1\n"),
            6);
}

TEST_F(LoopTest, IntFunctionEndingWithElseIfWithoutFinalElseIsError6) {
  EXPECT_EQ(compile("def foo(n: Int): Int =\n"
                    "  if n > 0 then\n"
                    "    1\n"
                    "  else if n < 0 then\n"
                    "    2\n"),
            6);
}

TEST_F(LoopTest, InferredFunctionEndingWithIncompatibleBranchesIsError6) {
  EXPECT_EQ(compile("def foo(n: Int) =\n"
                    "  if n > 0 then\n"
                    "    1\n"
                    "  else\n"
                    "    \"a\"\n"),
            6);
}

TEST_F(LoopTest, DoubleFunctionEndingWithIfWidensIntBranch) {
  EXPECT_EQ(compile("def foo(n: Int): Double =\n"
                    "  if n > 0 then\n"
                    "    1\n"
                    "  else\n"
                    "    2.5\n"),
            0);
}

TEST_F(LoopTest, NestedIfWithoutElseInIfExpressionBranchIsError6) {
  EXPECT_EQ(compile("def foo(x: Boolean) =\n"
                    "  val a = if x then\n"
                    "    if x then\n"
                    "      1\n"
                    "  else\n"
                    "    2\n"
                    "  a\n"),
            6);
}

TEST_F(LoopTest, NestedIfWithElseInIfExpressionBranchIsAccepted) {
  EXPECT_EQ(compile("def foo(x: Boolean) =\n"
                    "  val a = if x then\n"
                    "    if x then\n"
                    "      1\n"
                    "    else\n"
                    "      3\n"
                    "  else\n"
                    "    2\n"
                    "  a\n"),
            0);
}
