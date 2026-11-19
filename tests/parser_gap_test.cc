#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "../src/parser.h"
#include "../src/semantic.h"
}

/*
 * AI-generated test block — parser gap-coverage helpers.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Run the lexer and p_program over in-memory IFJ26 text; own the Program
 *   for one test and offer parse(source) -> status for table-driven cases.
 *   A source without `@main` gets a valid main function appended, so that the
 *   missing-main error (code 3) stays out of tests aimed at other constructs;
 *   every error in the source itself is found before the appended main.
 */
namespace {

/** Returns a null-terminated copy of @p text for the lexer to borrow. */
std::vector<char> buffer_from(const char *text) {
  return std::vector<char>(text, text + strlen(text) + 1);
}

ParserStatus parse_into(const char *source, Program *program) {
  std::vector<char> buffer = buffer_from(source);
  Lexer lexer;
  ParserStatus status = PARSER_INTERNAL_ERROR;
  if (lexer_init(&lexer, buffer.data()) == LEXER_OK) {
    status = p_program(&lexer, program);
    lexer_destroy(&lexer);
  }
  return status;
}

/** Valid main function appended to test sources that have none. */
const char *const kMainFunction = "@main def added_main(): Unit =\n"
                                  "  return\n";

struct ParsedProgram {
  Program program;
  ParserStatus status;
  /// True when kMainFunction was appended to the source.
  bool added_main;
  explicit ParsedProgram(const char *source, bool add_main = true)
      : added_main(add_main && std::strstr(source, "@main") == nullptr) {
    const std::string text =
        added_main ? std::string(source) + "\n" + kMainFunction : source;
    status = program_init(&program) == PARSER_OK
                 ? parse_into(text.c_str(), &program)
                 : PARSER_INTERNAL_ERROR;
  }
  ~ParsedProgram() { program_destroy(&program); }
  ParsedProgram(const ParsedProgram &) = delete;
  ParsedProgram &operator=(const ParsedProgram &) = delete;

  /// Number of functions from the source (without the appended main).
  size_t count() const {
    return program.functions.function_count - (added_main ? 1U : 0U);
  }
  FunctionSymbol *at(size_t i) const { return program.functions.functions[i]; }
  FunctionSymbol *function(const char *name) const {
    return function_table_lookup(&program.functions, name);
  }
};

ParserStatus parse(const char *source) { return ParsedProgram(source).status; }

template <size_t N>
void expect_status(const char *const (&sources)[N], ParserStatus expected) {
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(source), expected);
  }
}

} // namespace

/*
 * AI-generated test block — Program API and p_program argument checks.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   program_init rejects NULL, program_destroy accepts NULL and a second
 *   call, p_program rejects a NULL lexer or program.
 */
TEST(ParserGapApiTest, NullArgumentsAndDoubleDestroy) {
  EXPECT_EQ(program_init(nullptr), PARSER_INVALID_ARGUMENT);
  program_destroy(nullptr);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(nullptr, &program), PARSER_INVALID_ARGUMENT);
  program_destroy(&program);
  EXPECT_FALSE(program.initialized);
  program_destroy(&program);
  std::vector<char> buffer = buffer_from("def f() =\n  return\n");
  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, buffer.data()), LEXER_OK);
  EXPECT_EQ(p_program(&lexer, nullptr), PARSER_INVALID_ARGUMENT);
  lexer_destroy(&lexer);
}

/*
 * AI-generated test block — sources the parser must accept.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Valid layouts (blank lines, multiline parameters, end of input instead
 *   of DEDENT) parse OK with the expected body statement count.
 */
TEST(ParserGapStatusTest, ValidSourcesAreAccepted) {
  struct Case {
    const char *source;
    size_t statements;
  };
  const Case cases[] = {
      {"def f() =\n  return\n", 1},
      {"def f() =\n  return", 1},
      {"def f() =\n\n  return\n", 1},
      {"def f() = \n  return\n", 1},
      {"def f() =\n  return\n  return\n  return\n", 3},
      {"def f() =\n  return\n\n\n  return\n", 2},
      {"def f() =\n  return\n  return", 2},
      {"def f(\n  a: Int,\n  b: String,\n  ) =\n  return\n", 1},
      {"def f(\n  a: Int,\n  b: String\n  ) =\n  return\n", 1},
      {"def f(a:\n  Int) =\n  return\n", 1},
      {"def f(a\n  : Int) =\n  return\n", 1},
      {"@main\n\ndef main() =\n  return\n", 1},
      {"def f(\n    a: Int,\n  b: Int) =\n  return\n", 1},
      // BOOLTHEN: else is optional; a one-line then branch may precede it.
      {"def f(a: Boolean) =\n  if a then\n    return\n", 1},
      {"def f(a: Boolean) =\n  if a then return\n  else\n    return\n", 1},
  };
  for (const Case &c : cases) {
    SCOPED_TRACE(c.source);
    ParsedProgram p(c.source);
    ASSERT_EQ(p.status, PARSER_OK);
    ASSERT_EQ(p.count(), 1U);
    ASSERT_NE(p.at(0)->ast_root, nullptr);
    ASSERT_EQ(p.at(0)->ast_root->kind, AST_BLOCK);
    EXPECT_EQ(p.at(0)->ast_root->data.block.statement_count, c.statements);
  }
}

/*
 * AI-generated test block — syntax, semantic and lexer errors.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Malformed constructs are syntax errors; redefinitions and @main with
 *   parameters are semantic errors; lexer errors propagate unchanged.
 */
TEST(ParserGapStatusTest, MalformedSourcesAreSyntaxErrors) {
  const char *const sources[] = {
      "5\n",
      "val x = 1\n",
      "return\n",
      "if\n", // top level
      "def () =\n  return\n",
      "def while() =\n  return\n",
      "def\n",
      "def f()\n  return\n",
      "def f(): Int\n  return\n",
      "def f() return\n",
      "def f",
      "def f() = return\n",
      "def f() =\nreturn\n",
      "def f() =\n",
      "@main x\n",
      "@main\n",
      "@main",
      "@main @main\n",
      "def f(\n  a: Int,\n  b: String,\n) =\n  return\n", // `)` at header
      "def f(a) =\n  return\n",
      "def f(a Int) =\n  return\n",
      "def f(a: Int b: Int) =\n  return\n",
      "def f(a: Int =\n  return\n",
      "def f( =\n  return\n",
      "def f(a: Int,) =\n  return\n",
      "def f(,) =\n  return\n",
      "def f(1) =\n  return\n",
      "def f(): =\n  return\n",
      "def f(): Foo =\n  return\n",
      "def f(a: Foo) =\n  return\n",
      "def f(a: val) =\n  return\n",
      "def f(a: ) =\n  return\n",
      "def f():\n Int =\n  return\n",
      "def f() =\n  val = 1\n",
      "def f() =\n  var = 1\n",
      "def f() =\n  val if = 1\n",
      "def f() =\n  val x 1\n",
      "def f() =\n  var x\n",
      "def f() =\n  val x: = 1\n",
      "def f() =\n  val x: Foo = 1\n",
      "def f(a: Boolean) =\n  if a\n    return\n  else\n    return\n",
      // Blocks cannot be empty and `;` only separates two statements.
      "def f() =\n  ;\n",
      "def f() =\n  ;return\n",
      "def f() =\n  ;;return\n",
      "def f() =\n  ;\n  return\n",
      "def f() =\n  if then\n    return\n  else\n    return\n",
      "def f() =\n  def g() =\n    return\n",
      "def f() =\n  return\n    return\n",
      "def f() =\n    return\n  return\n",
      "def f() =\n  return\n  else\n",
      "def f() =\n  then\n",
      "def f() =\n  import\n",
      "def f() =\n  return return\n",
      "def f(): List =\n  return\n",              // not a type
      "def f(\n  a: Int,\nb: Int) =\n  return\n", // `(` line indent
  };
  expect_status(sources, PARSER_SYNTAX_ERROR);
}

TEST(ParserGapStatusTest, SemanticAndLexerErrorsArePropagated) {
  const char *const redefinition[] = {
      "@main def main() =\n  return\n@main def other() =\n  return\n",
      "def f() =\n  return\ndef f() =\n  return\n",
      "@main def main() =\n  return\ndef main() =\n  return\n",
      "def f(a: Int, a: String) =\n  return\n",
      "def f(a: Int) =\n  val a = 1\n",
      "def f(a: Int) =\n  var a = 1\n",
  };
  expect_status(redefinition, PARSER_REDEFINITION_ERROR);
  const char *const bad_main[] = {"@main def main(a: Int) =\n  return\n"};
  expect_status(bad_main, PARSER_BAD_CALL_ERROR);
  const char *const lexical[] = {
      "  def f() =\n  return\n",
      "def f() =\n  return\n  $\n",
      "def f() =\n  return\ndef g() =\n  return\n\"abc",
      "def $",
      "def f$",
      "def f($",
      "def f(a$",
      "def f(a:$",
      "def f(a:\n  $",
      "def f(a: Int$",
      "def f(a: Int,\n$",
      "def f()$",
      "def f():$",
      "def f(): Int$",
      "def f() =$",
      "def f() =\n  $",
      "def f() =\n  return$",
      "def f() =\n  val x$",
      "def f() =\n  val x:$",
      "def f() =\n  val x: Int$",
      "def f() =\n  if$",
      "def f() =\n  return;$",
      "@main$",
      "@main\n$",
      "@main def$",
      "def f() =\n  return\n$",
  };
  expect_status(lexical, PARSER_LEXER_ERROR);
}

/*
 * AI-generated test block — program, function and main structure.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Functions keep source order and only @main gets is_main; return types,
 *   parameters as constant locals and bare return are recorded correctly.
 */
TEST(ParserGapStructureTest, FunctionsKeepOrderAndOnlyMainIsMarked) {
  ParsedProgram p("def f() =\n  return\n\n@main def main(): Unit =\n  return\n"
                  "def g() =\n  return\n");
  ASSERT_EQ(p.status, PARSER_OK);
  ASSERT_EQ(p.count(), 3U);
  const char *names[] = {"f", "main", "g"};
  for (size_t i = 0; i < 3U; ++i) {
    SCOPED_TRACE(names[i]);
    EXPECT_STREQ(p.at(i)->name, names[i]);
    EXPECT_TRUE(p.at(i)->is_defined);
    EXPECT_EQ(p.at(i)->is_main, i == 1U);
  }
  EXPECT_EQ(p.function("main")->argument_count, 0U);
  EXPECT_TRUE(
      type_equal(p.function("main")->return_type.type, type_scalar(TYPE_UNIT)));
  ASSERT_NE(p.at(0)->ast_root, nullptr);
  ASTNode *bare_return = p.at(0)->ast_root->data.block.statements[0];
  EXPECT_EQ(bare_return->kind, AST_RETURN);
  EXPECT_EQ(bare_return->data.return_statement.value, nullptr);
}

TEST(ParserGapStructureTest, ReturnTypeAnnotationIsResolved) {
  struct Case {
    const char *source;
    BaseType type;
  };
  const Case cases[] = {
      {"def f() =\n  return\n", TYPE_UNKNOWN},
      {"def f(): Int =\n  return\n", TYPE_INT},
      {"def f(): Double =\n  return\n", TYPE_REAL},
      {"def f(): String =\n  return\n", TYPE_STRING},
      {"def f(): Unit =\n  return\n", TYPE_UNIT},
      {"def f(): Boolean =\n  return\n", TYPE_BOOL},
  };
  for (const Case &c : cases) {
    SCOPED_TRACE(c.source);
    ParsedProgram p(c.source);
    ASSERT_EQ(p.status, PARSER_OK);
    EXPECT_TRUE(
        type_equal(p.function("f")->return_type.type, type_scalar(c.type)));
    EXPECT_EQ(p.function("f")->has_explicit_return_type,
              c.type != TYPE_UNKNOWN);
  }
}

TEST(ParserGapStructureTest, ParametersAreTypedConstantLocals) {
  ParsedProgram p("def f(a: Int, b: Double, c: String, d: Boolean, e: Unit) =\n"
                  "  return\n");
  ASSERT_EQ(p.status, PARSER_OK);
  FunctionSymbol *f = p.function("f");
  ASSERT_EQ(f->argument_count, 5U);
  const BaseType expected[] = {TYPE_INT, TYPE_REAL, TYPE_STRING, TYPE_BOOL,
                               TYPE_UNIT};
  for (size_t i = 0; i < 5U; ++i) {
    EXPECT_TRUE(type_is_scalar(f->parameters[i]->type.type, expected[i]))
        << "parameter " << i;
  }
  Symbol *a = symbol_table_lookup_local(f->local_scope, "a");
  ASSERT_NE(a, nullptr);
  EXPECT_TRUE(a->constant);
  EXPECT_TRUE(a->initialized);
  EXPECT_TRUE(type_equal(a->type.type, type_scalar(TYPE_INT)));
}

/*
 * AI-generated test block — known gaps (expected behaviour, disabled).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Record correct results for features waiting on p_expression or a
 *   parser fix; enable each test once its cause is resolved.
 */
TEST(ParserGapKnownGapTest, ValWithInitializerIsAccepted) {
  EXPECT_EQ(parse("def f() =\n  val x = 1\n"), PARSER_OK);
}

TEST(ParserGapKnownGapTest, IfElseIsAccepted) {
  EXPECT_EQ(parse("def f(a: Boolean) =\n  if a then\n    return\n"
                  "  else\n    return\n"),
            PARSER_OK);
}

TEST(ParserGapKnownGapTest, CallArgumentsAreAccepted) {
  EXPECT_EXIT(
      {
        std::exit(parse("def g(a: Int) =\n  h(1, 2.5, \"s\", a, h())\n") ==
                          PARSER_OK
                      ? 0
                      : 1);
      },
      ::testing::ExitedWithCode(0), "");
}

TEST(ParserGapKnownGapTest, ExpressionStatementIsAccepted) {
  EXPECT_EXIT(
      { std::exit(parse("def f() =\n  5\n") == PARSER_OK ? 0 : 1); },
      ::testing::ExitedWithCode(0), "");
}

TEST(ParserGapKnownGapTest, SemicolonAfterStatementIsAccepted) {
  EXPECT_EQ(parse("def f() =\n  return; return\n"), PARSER_OK);
}

TEST(ParserGapKnownGapTest, LeadingBlankLineIsAccepted) {
  EXPECT_EQ(parse("\ndef f() =\n  return\n"), PARSER_OK);
}

TEST(ParserGapKnownGapTest, InitializerReferencingItselfIsError) {
  EXPECT_EQ(parse("def f() =\n  val x = x\n"), PARSER_UNDEFINED_ERROR);
}

TEST(ParserGapKnownGapTest, WhileLoopIsAccepted) {
  EXPECT_EQ(parse("def f(a: Boolean) =\n  while a do\n    return\n"),
            PARSER_OK);
}

/*
 * AI-generated test block — function header followed by end of input.
 * Model: Claude (Claude Code).
 * Prompt:
 *   A function whose `=` is followed by end of input has no body; the
 *   unexpected TOKEN_EOF must be a syntax error, not PARSER_OK.
 */
TEST(ParserGapKnownGapTest, MissingFunctionBodyIsSyntaxError) {
  const char *const sources[] = {
      "@main\ndef main(): Unit =",
      "@main\ndef main(): Unit = ",
      "def f() =",
  };
  expect_status(sources, PARSER_SYNTAX_ERROR);
}

/*
 * AI-generated test block — parameter lines at the opening indentation.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Lines inside parentheses must be indented more than the line with `(`
 *   (parser.c design rule); a line at the same indentation is a syntax error.
 */
TEST(ParserGapKnownGapTest, ParameterLineAtOpeningIndentIsSyntaxError) {
  const char *const sources[] = {
      "def f(\nx: Int) =\n  return\n",
      "def f(a:\nInt) =\n  return\n",
  };
  expect_status(sources, PARSER_SYNTAX_ERROR);
}

/*
 * AI-generated test block — position of a return statement node.
 * Model: Claude (Claude Code).
 * Prompt:
 *   The AST_RETURN node carries the line and column of the `return`
 *   keyword, as ast.h documents for ast_node_create_return.
 */
TEST(ParserGapKnownGapTest, ReturnNodeHasPositionOfReturnKeyword) {
  ParsedProgram p("def f() =\n  return\n");
  ASSERT_EQ(p.status, PARSER_OK);
  ASSERT_NE(p.at(0)->ast_root, nullptr);
  ASTNode *r = p.at(0)->ast_root->data.block.statements[0];
  EXPECT_EQ(r->kind, AST_RETURN);
  EXPECT_EQ(r->line, 2U);
  EXPECT_EQ(r->column, 3U);
}

/*
 * AI-generated test block — parser to semantic pipeline.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Feed real parser output into semantic_check and assert the documented
 *   exit codes for programs the current parser accepts.
 */
TEST(ParserGapPipelineTest, ParsedProgramsGetDocumentedSemanticCodes) {
  struct Case {
    const char *source;
    SemanticStatus expected;
  };
  const Case cases[] = {
      {"@main\ndef main(): Unit =\n  return\n", SEMANTIC_OK},
      {"def f(a: Int, b: Double): Unit =\n  return\n"
       "@main\ndef main(): Unit =\n  return\n",
       SEMANTIC_OK},
      {"@main\ndef main() =\n  return\n",
       SEMANTIC_TYPE_ERROR}, // `return` needs annotation
      {"def print(x: Int): Unit =\n  return\n"
       "@main\ndef main(): Unit =\n  return\n",
       SEMANTIC_REDEFINITION}, // builtin name redefined
      {"@main\ndef main(): Int =\n  return\n",
       SEMANTIC_TYPE_ERROR}, // bare return in non-Unit; also @main != Unit
      {"def f(): Int =\n  return\n@main\ndef main(): Unit =\n  return\n",
       SEMANTIC_TYPE_ERROR}, // bare return in non-Unit
  };
  for (const Case &c : cases) {
    SCOPED_TRACE(c.source);
    ParsedProgram p(c.source);
    ASSERT_EQ(p.status, PARSER_OK);
    EXPECT_EQ(semantic_check(&p.program), c.expected);
  }
  // A missing @main is already reported by the parser (code 3).
  EXPECT_EQ(ParsedProgram("", false).status, PARSER_UNDEFINED_ERROR);
  EXPECT_EQ(ParsedProgram("def f() =\n  return\n", false).status,
            PARSER_UNDEFINED_ERROR);
}
