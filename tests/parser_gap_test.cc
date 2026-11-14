#include <cstdio>

#include "gtest/gtest.h"

extern "C" {
#include "../src/parser.h"
}

/*
 * AI-generated test block — parser gap-coverage helpers.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Run the lexer and p_program over in-memory IFJ26 text; own the Program
 *   for one test and offer parse(source) -> status for table-driven cases.
 */
namespace {

FILE *stream_from(const char *text) {
  FILE *stream = tmpfile();
  if (stream != nullptr) {
    fputs(text, stream);
    rewind(stream);
  }
  return stream;
}

ParserStatus parse_into(const char *source, Program *program) {
  FILE *stream = stream_from(source);
  if (stream == nullptr) {
    return PARSER_INTERNAL_ERROR;
  }
  Lexer lexer;
  ParserStatus status = PARSER_INTERNAL_ERROR;
  if (lexer_init(&lexer, stream) == LEXER_OK) {
    status = p_program(&lexer, program);
    lexer_destroy(&lexer);
  }
  fclose(stream);
  return status;
}

struct ParsedProgram {
  Program program;
  ParserStatus status;
  explicit ParsedProgram(const char *source) {
    status = program_init(&program) == PARSER_OK ? parse_into(source, &program)
                                                  : PARSER_INTERNAL_ERROR;
  }
  ~ParsedProgram() { program_destroy(&program); }
  ParsedProgram(const ParsedProgram &) = delete;
  ParsedProgram &operator=(const ParsedProgram &) = delete;

  size_t count() const { return program.functions.function_count; }
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
  FILE *stream = stream_from("def f() =\n  return\n");
  ASSERT_NE(stream, nullptr);
  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  EXPECT_EQ(p_program(&lexer, nullptr), PARSER_INVALID_ARGUMENT);
  lexer_destroy(&lexer);
  fclose(stream);
}

/*
 * AI-generated test block — sources the parser must accept.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Valid layouts (blank lines, multiline parameters, `;` separators, end of
 *   input instead of DEDENT) parse OK with the expected body statement count.
 */
TEST(ParserGapStatusTest, ValidSourcesAreAccepted) {
  struct Case { const char *source; size_t statements; };
  const Case cases[] = {
      {"def f() =\n  return\n", 1}, {"def f() =\n  return", 1},
      {"def f() =\n\n  return\n", 1}, {"def f() = \n  return\n", 1},
      {"def f() =\n  return\n  return\n  return\n", 3},
      {"def f() =\n  return\n\n\n  return\n", 2},
      {"def f() =\n  return\n  return", 2}, {"def f() =\n  ;\n", 0},
      {"def f() =\n  ;return\n", 1}, {"def f() =\n  ;;return\n", 1},
      {"def f() =\n  ;\n  return\n", 1},
      {"def f(\n  a: Int,\n  b: String,\n  ) =\n  return\n", 1},
      {"def f(\n  a: Int,\n  b: String\n  ) =\n  return\n", 1},
      {"def f(a:\n  Int) =\n  return\n", 1},
      {"def f(a\n  : Int) =\n  return\n", 1},
      {"@main\n\ndef main() =\n  return\n", 1},
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
      "5\n", "val x = 1\n", "return\n", "if\n",                // top level
      "def () =\n  return\n", "def while() =\n  return\n", "def\n",
      "def f()\n  return\n", "def f(): Int\n  return\n", "def f() return\n",
      "def f", "def f() = return\n", "def f() =\nreturn\n", "def f() =\n",
      "@main x\n", "@main\n", "@main", "@main @main\n",
      "def f(\n  a: Int,\n  b: String,\n) =\n  return\n",     // `)` at header
      "def f(a) =\n  return\n", "def f(a Int) =\n  return\n",
      "def f(a: Int b: Int) =\n  return\n", "def f(a: Int =\n  return\n",
      "def f( =\n  return\n", "def f(a: Int,) =\n  return\n",
      "def f(,) =\n  return\n", "def f(1) =\n  return\n",
      "def f(): =\n  return\n", "def f(): Foo =\n  return\n",
      "def f(a: Foo) =\n  return\n", "def f(a: val) =\n  return\n",
      "def f(a: ) =\n  return\n", "def f():\n Int =\n  return\n",
      "def f() =\n  val = 1\n", "def f() =\n  var = 1\n",
      "def f() =\n  val if = 1\n", "def f() =\n  val x 1\n",
      "def f() =\n  var x\n", "def f() =\n  val x: = 1\n",
      "def f() =\n  val x: Foo = 1\n",
      "def f(a: Boolean) =\n  if a\n    return\n  else\n    return\n",
      "def f(a: Boolean) =\n  if a then\n    return\n",
      "def f(a: Boolean) =\n  if a then return\n  else\n    return\n",
      "def f() =\n  if then\n    return\n  else\n    return\n",
      "def f() =\n  def g() =\n    return\n",
      "def f() =\n  return\n    return\n",
      "def f() =\n    return\n  return\n", "def f() =\n  return\n  else\n",
      "def f() =\n  then\n", "def f() =\n  import\n",
      "def f() =\n  return return\n",
  };
  expect_status(sources, PARSER_SYNTAX_ERROR);
}

TEST(ParserGapStatusTest, SemanticAndLexerErrorsArePropagated) {
  const char *const semantic[] = {
      "@main def main() =\n  return\n@main def other() =\n  return\n",
      "def f() =\n  return\ndef f() =\n  return\n",
      "@main def main() =\n  return\ndef main() =\n  return\n",
      "@main def main(a: Int) =\n  return\n",
      "def f(a: Int, a: String) =\n  return\n",
      "def f(a: Int) =\n  val a = 1\n", "def f(a: Int) =\n  var a = 1\n",
  };
  expect_status(semantic, PARSER_SEMANTIC_ERROR);
  const char *const lexical[] = {
      "  def f() =\n  return\n", "def f() =\n  return\n  $\n",
      "def f() =\n  return\ndef g() =\n  return\n\"abc",
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
  EXPECT_EQ(p.function("main")->return_type.type, TYPE_UNIT);
  ASSERT_NE(p.at(0)->ast_root, nullptr);
  ASTNode *bare_return = p.at(0)->ast_root->data.block.statements[0];
  EXPECT_EQ(bare_return->kind, AST_RETURN);
  EXPECT_EQ(bare_return->data.return_statement.value, nullptr);
}

TEST(ParserGapStructureTest, ReturnTypeAnnotationIsResolved) {
  struct Case { const char *source; DataType type; };
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
    EXPECT_EQ(p.function("f")->return_type.type, c.type);
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
  const DataType expected[] = {TYPE_INT, TYPE_REAL, TYPE_STRING, TYPE_BOOL,
                               TYPE_UNIT};
  for (size_t i = 0; i < 5U; ++i) {
    EXPECT_EQ(f->parameter_types[i], expected[i]) << "parameter " << i;
  }
  Symbol *a = symbol_table_lookup_local(f->local_scope, "a");
  ASSERT_NE(a, nullptr);
  EXPECT_TRUE(a->constant);
  EXPECT_TRUE(a->initialized);
  EXPECT_EQ(a->type.type, TYPE_INT);
}

/*
 * AI-generated test block — known gaps (expected behaviour, disabled).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Record correct results for features waiting on p_expression or a
 *   parser fix; enable each test once its cause is resolved.
 */
// Needs p_expression (initializer is never parsed).
TEST(ParserGapKnownGapTest, DISABLED_ValWithInitializerIsAccepted) {
  EXPECT_EQ(parse("def f() =\n  val x = 1\n"), PARSER_OK);
}

// Needs p_expression (if condition is never parsed).
TEST(ParserGapKnownGapTest, DISABLED_IfElseIsAccepted) {
  EXPECT_EQ(parse("def f(a: Boolean) =\n  if a then\n    return\n"
                  "  else\n    return\n"), PARSER_OK);
}

// Needs p_expression (p_call_args / p_operand are never reached).
TEST(ParserGapKnownGapTest, DISABLED_CallArgumentsAreAccepted) {
  EXPECT_EQ(parse("def g(a: Int) =\n  h(1, 2.5, \"s\", a, h())\n"), PARSER_OK);
}

// Bug: null statement dereferenced at parser.c:870 (crashes if enabled).
TEST(ParserGapKnownGapTest, DISABLED_ExpressionStatementIsAccepted) {
  EXPECT_EQ(parse("def f() =\n  5\n"), PARSER_OK);
}

// Bug: statement must be followed by NEWLINE, `;` rejected (parser.c:884).
TEST(ParserGapKnownGapTest, DISABLED_SemicolonAfterStatementIsAccepted) {
  EXPECT_EQ(parse("def f() =\n  return; return\n"), PARSER_OK);
}

// Bug: top-level NEWLINE tokens are not skipped (parser.c:1276).
TEST(ParserGapKnownGapTest, DISABLED_LeadingBlankLineIsAccepted) {
  EXPECT_EQ(parse("\ndef f() =\n  return\n"), PARSER_OK);
}

// Bug: symbol defined before its initializer is parsed (parser.c:530).
TEST(ParserGapKnownGapTest, DISABLED_InitializerReferencingItselfIsError) {
  EXPECT_EQ(parse("def f() =\n  val x = x\n"), PARSER_SEMANTIC_ERROR);
}

// Needs a while production in p_statement and p_expression.
TEST(ParserGapKnownGapTest, DISABLED_WhileLoopIsAccepted) {
  EXPECT_EQ(parse("def f(a: Boolean) =\n  while a do\n    return\n"),
            PARSER_OK);
}
