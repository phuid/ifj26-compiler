#include <cstddef>
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
 * AI-generated test block — parser edge-case helpers.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover the edge cases of the IFJ26 parser (program structure and
 *   statements, not expressions) that tests/parser_*.cc do not cover yet.
 *   Helpers run the lexer and p_program over an in-memory source and own
 *   the Program for one test. Sources keep the existing convention of the
 *   parser tests and carry no `import ifj26.*` prolog, which the parser does
 *   not implement yet (reported by ParserBugTest).
 */
namespace {

/** Returns a null-terminated copy of @p text for the lexer to borrow. */
std::vector<char> buffer_from(const char *text) {
  return std::vector<char>(text, text + strlen(text) + 1);
}

/** Owns the Program produced by parsing one source. */
struct Parsed {
  Program program;
  ParserStatus status = PARSER_INTERNAL_ERROR;

  explicit Parsed(const std::string &source) {
    if (program_init(&program) != PARSER_OK) {
      return;
    }
    std::vector<char> buffer = buffer_from(source.c_str());
    Lexer lexer;
    if (lexer_init(&lexer, buffer.data()) == LEXER_OK) {
      status = p_program(&lexer, &program);
      lexer_destroy(&lexer);
    }
  }
  ~Parsed() { program_destroy(&program); }
  Parsed(const Parsed &) = delete;
  Parsed &operator=(const Parsed &) = delete;

  size_t count() const { return program.functions.function_count; }
  FunctionSymbol *at(size_t i) const { return program.functions.functions[i]; }
  FunctionSymbol *function(const char *name) const {
    return function_table_lookup(&program.functions, name);
  }
};

ParserStatus parse(const std::string &source) { return Parsed(source).status; }

/** Returns the body block of function `f`, or null. */
const ASTNode *f_body(const Parsed &p) {
  const FunctionSymbol *f = p.status == PARSER_OK ? p.function("f") : nullptr;
  if (f == nullptr) {
    return nullptr;
  }
  const ASTNode *root = f->ast_root;
  return root != nullptr && root->kind == AST_BLOCK ? root : nullptr;
}

/**
 * Entry function for sources that must parse: the parser itself reports a
 * missing @main (code 3, spec 2). Placed after a source with with_entry(), or
 * before it (kEntryLines lines) where the source must end the input.
 */
constexpr const char *kEntry = "@main\ndef entry(): Unit =\n  return\n";
constexpr size_t kEntryLines = 3U;

std::string with_entry(const std::string &source) { return source + kEntry; }

/** Valid two-parameter function used as the base of the token tables. */
constexpr const char *kHeader = "def f(a: Int, b: String): Unit =";
constexpr const char *kFunction = "def f(a: Int, b: String): Unit =\n"
                                  "  return\n";

} // namespace

/*
 * AI-generated test block — end of input inside every construct.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cut a function header, a @main header and the statements of a block at
 *   every token boundary; end of input at such a point is syntax error 2.
 *   Skip the prefixes pinned elsewhere: `... =` + EOF (known gap), the
 *   empty-block prefixes (ParserBugTest) and `val x: Int =` (stmt cov).
 */
class ParserEdgeEofTest : public ::testing::TestWithParam<const char *> {};

TEST_P(ParserEdgeEofTest, EndOfInputIsSyntaxError) {
  Parsed p(GetParam());
  EXPECT_EQ(p.status, PARSER_SYNTAX_ERROR);
}

INSTANTIATE_TEST_SUITE_P(
    Headers, ParserEdgeEofTest,
    ::testing::Values("def", "def f", "def f(", "def f(a", "def f(a:",
                      "def f(a: Int", "def f(a: Int,", "def f(a: Int, b",
                      "def f(a: Int, b:", "def f(a: Int, b: String",
                      "def f(a: Int, b: String)", "def f(a: Int, b: String):",
                      "def f(a: Int, b: String): Unit", "@main def",
                      "@main def m", "@main def m(", "@main def m()",
                      "@main def m():", "@main def m(): Unit",
                      "def f(\n  a: Int,\n", "def f(\n  a: Int,\n  b"));

INSTANTIATE_TEST_SUITE_P(
    Statements, ParserEdgeEofTest,
    ::testing::Values("def f() =\n  val", "def f() =\n  var x",
                      "def f() =\n  val x:", "def f() =\n  var x: Int",
                      "def f() =\n  if", "def f() =\n  return\n  val"));

/*
 * AI-generated test block — one missing or doubled token of a header.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Drop or double each token of `def f(a: Int, b: String): Unit =` with a
 *   `return` body (missing name, parenthesis, colon, type, comma, `=`,
 *   NEWLINE, INDENT or body); every variant is syntax error 2.
 */
class ParserEdgeTokenTest : public ::testing::TestWithParam<const char *> {};

TEST_P(ParserEdgeTokenTest, MalformedHeaderIsSyntaxError) {
  const std::string source = std::string(GetParam()) + "\n  return\n";
  Parsed p(source);
  EXPECT_EQ(p.status, PARSER_SYNTAX_ERROR);
}

INSTANTIATE_TEST_SUITE_P(
    Missing, ParserEdgeTokenTest,
    ::testing::Values(
        "f(a: Int, b: String): Unit =", "def (a: Int, b: String): Unit =",
        "def f a: Int, b: String): Unit =", "def f(: Int, b: String): Unit =",
        "def f(a Int, b: String): Unit =", "def f(a:, b: String): Unit =",
        "def f(a: Int b: String): Unit =", "def f(a: Int, : String): Unit =",
        "def f(a: Int, b String): Unit =", "def f(a: Int, b:): Unit =",
        "def f(a: Int, b: String: Unit =", "def f(a: Int, b: String) Unit =",
        "def f(a: Int, b: String): Unit",
        "def f(a: Int, b: String): Unit = return"));

INSTANTIATE_TEST_SUITE_P(
    Doubled, ParserEdgeTokenTest,
    ::testing::Values("def def f(a: Int, b: String): Unit =",
                      "def f f(a: Int, b: String): Unit =",
                      "def f((a: Int, b: String): Unit =",
                      "def f(a a: Int, b: String): Unit =",
                      "def f(a:: Int, b: String): Unit =",
                      "def f(a: Int Int, b: String): Unit =",
                      "def f(a: Int,, b: String): Unit =",
                      "def f(a: Int, b: String,): Unit =",
                      "def f(a: Int, b: String)): Unit =",
                      "def f(a: Int, b: String):: Unit =",
                      "def f(a: Int, b: String): Unit Unit =",
                      "def f(a: Int, b: String): Unit ==",
                      "@main @main def m(): Unit ="));

TEST(ParserEdgeTest, MissingBlockPartsAreSyntaxErrors) {
  const char *const sources[] = {
      "def f(a: Int, b: String): Unit =\nreturn\n", // INDENT dropped
      "def f(a: Int, b: String): Unit =\n  \n",     // body dropped
      "def f(a: Int, b: String): Unit =\ndef g() =\n  return\n",
      "def f()\n: Int =\n  return\n", // `:` on the next logical line
      "def f()\n  =\n  return\n",     // `=` on the next logical line
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(source), PARSER_SYNTAX_ERROR);
  }
}

/*
 * AI-generated test block — lexical error at every token boundary.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Put an invalid character `$` after every token of a two-parameter
 *   function and of a @main header; the parser must return the lexer's
 *   code 1 (PARSER_LEXER_ERROR), never a syntax or semantic error.
 */
TEST(ParserEdgeTest, LexicalErrorAfterEveryTokenIsLexerError) {
  const std::string body = std::string(kHeader) + "\n  return";
  const size_t cuts[] = {3,  5,  6,  7,  8,  12, 13, 15,
                         16, 23, 24, 25, 30, 32, 33, 42};
  for (size_t cut : cuts) {
    const std::string source = body.substr(0, cut) + " $";
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(source), PARSER_LEXER_ERROR);
  }
  const char *const tails[] = {
      "\n  $",
      "\n  return\n  $",
      "\n  return;$",
      "\n  return; $\n",
      "\n  return\n$",
      "\n  return\n@main $",
      "\n  return\n@main def $",
  };
  for (const char *tail : tails) {
    const std::string source = std::string(kHeader) + tail;
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(source), PARSER_LEXER_ERROR);
  }
}

TEST(ParserEdgeTest, LexicalErrorInBlockLayoutIsLexerError) {
  const char *const sources[] = {
      "def f() =\n\treturn\n",              // tab as indentation
      "def f() =\n  return\n\treturn\n",    // tab on the second line
      "def f() =\n  return \"unterminated", // string not closed
      "def f(a: Int,\n  b: Int\n  $) =\n  return\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(source), PARSER_LEXER_ERROR);
  }
}

/*
 * AI-generated test block — keywords where an identifier is expected.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Spec 1.1: keywords (also the extension ones below the line) cannot be
 *   identifiers; `_` alone is not an identifier. Using one as a function,
 *   parameter or variable name is syntax error 2 and no function named by
 *   it is stored. Names that only contain a keyword are accepted.
 */
class ParserEdgeKeywordTest : public ::testing::TestWithParam<const char *> {};

TEST_P(ParserEdgeKeywordTest, KeywordAsFunctionNameIsSyntaxError) {
  const std::string name = GetParam();
  Parsed p("def " + name + "() =\n  return\n");
  EXPECT_EQ(p.status, PARSER_SYNTAX_ERROR);
  EXPECT_EQ(p.count(), 0U);
}

TEST_P(ParserEdgeKeywordTest, KeywordAsParameterNameIsSyntaxError) {
  const std::string name = GetParam();
  Parsed p("def f(a: Int, " + name + ": Int) =\n  return\n");
  EXPECT_EQ(p.status, PARSER_SYNTAX_ERROR);
  ASSERT_EQ(p.count(), 1U);
  EXPECT_EQ(p.at(0)->argument_count, 1U);
}

TEST_P(ParserEdgeKeywordTest, KeywordAsVariableNameIsSyntaxError) {
  const std::string name = GetParam();
  EXPECT_EQ(parse("def f() =\n  var " + name + ": Int = 1\n"),
            PARSER_SYNTAX_ERROR);
}

INSTANTIATE_TEST_SUITE_P(Keywords, ParserEdgeKeywordTest,
                         ::testing::Values("def", "do", "Double", "else", "if",
                                           "import", "Int", "return", "String",
                                           "then", "Unit", "val", "var",
                                           "while", "Boolean", "case", "class",
                                           "extension", "for", "List", "match",
                                           "type", "_"));

TEST(ParserEdgeTest, NamesContainingKeywordsAreIdentifiers) {
  const char *const names[] = {"main", "Main", "iff",     "_if",
                               "Int2", "defx", "return_", "__"};
  for (const char *name : names) {
    SCOPED_TRACE(name);
    const std::string n = name;
    Parsed p(with_entry("def " + n + "(" + n + ": Int) =\n  return\n"));
    ASSERT_EQ(p.status, PARSER_OK);
    FunctionSymbol *f = p.function(name);
    ASSERT_NE(f, nullptr);
    ASSERT_NE(f->local_scope, nullptr);
    EXPECT_NE(symbol_table_lookup_local(f->local_scope, name), nullptr);
  }
}

/*
 * AI-generated test block — accepted layouts and block positions.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Spec 1.2: any indentation width opens a block, whitespace-only and
 *   blank lines are ignored, newlines inside parentheses are whitespace and
 *   a trailing comma needs a newline before `)`. Each accepted source gives
 *   one body block; the AST_BLOCK carries the position of its first token.
 */
TEST(ParserEdgeLayoutTest, AcceptedLayoutsKeepStatementsAndBlockPosition) {
  struct Case {
    const char *source;
    size_t statements, line, column;
  };
  const Case cases[] = {
      {"def f() =\n return\n", 1, 2, 2},
      {"def f() =\n        return\n        return\n", 2, 2, 9},
      {"def f() =\n\n\n    return\n", 1, 4, 5},
      {"def f() =\n  return\n    \n  return\n", 2, 2, 3},
      {"def f() =\n  return\n \n  return\n", 2, 2, 3},
      {"def f() =\n  return   ", 1, 2, 3}, // end of input, no NEWLINE
      {"def f() =\n  return\n\n\n\n", 1, 2, 3},
      {"def f (a: Int) =\n  return\n", 1, 2, 3},
      {"def f(a: Int\n  , b: Int) =\n  return\n", 1, 3, 3},
      {"def f(\n\n\n  a: Int\n\n  ) =\n  return\n", 1, 7, 3},
      {"def f(\n  a: Int,\n\n  ) =\n  return\n", 1, 5, 3},
      {"def f(a: Int,\n      b: Int) =\n  return\n", 1, 3, 3},
      {"def f(\n  a: Int,\n  b: Int\n  ): Int =\n    return\n", 1, 5, 5},
  };
  for (const Case &c : cases) {
    SCOPED_TRACE(c.source);
    Parsed p(kEntry + std::string(c.source));
    const ASTNode *body = f_body(p);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->data.block.statement_count, c.statements);
    EXPECT_EQ(body->line, c.line + kEntryLines);
    EXPECT_EQ(body->column, c.column);
  }
}

TEST(ParserEdgeLayoutTest, RejectedLayoutsAreSyntaxErrors) {
  const char *const sources[] = {
      "def f(\n  a: Int,,\n  ) =\n  return\n",
      "def f(a: Int,\n  ,\n  ) =\n  return\n",
      "def f(\n    a: Int,\n  b: Int,\n) =\n  return\n",
      ";\ndef f() =\n  return\n",
      "def f() =\n  return\n;\n",
      "def f() =\n  @main\n",
      "def f() =\n  ;return return\n",
      // `;` only separates statements (spec 1.2), so it cannot lead a line.
      "def f() =\n  ;return\n",
      "def f() =\n  ; ; ;return\n",
      "def f() =\n  return\n  def g() =\n    return\n",
      "def f() =\n    return\n  def g() =\n    return\n",
      "@main\n@main\ndef m(): Unit =\n  return\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(source), PARSER_SYNTAX_ERROR);
  }
}

/*
 * AI-generated test block — large inputs.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Many parameters keep their types in source order, many functions keep
 *   their order, a block with hundreds of statements (also separated by
 *   blank and whitespace-only lines) keeps all of them and a 1000-space
 * indentation opens a block; every case parses quickly with PARSER_OK.
 */
TEST(ParserEdgeLimitTest, ManyParametersKeepSourceOrder) {
  constexpr size_t kParameters = 70U;
  const char *const types[] = {"Int", "Double", "String", "Unit"};
  const BaseType expected[] = {TYPE_INT, TYPE_REAL, TYPE_STRING, TYPE_UNIT};
  std::string source = "def f(";
  for (size_t i = 0; i < kParameters; ++i) {
    source +=
        (i == 0 ? "p" : ",\n  p") + std::to_string(i) + ": " + types[i % 4U];
  }
  Parsed p(with_entry(source + ") =\n  return\n"));
  ASSERT_EQ(p.status, PARSER_OK);
  ASSERT_EQ(p.count(), 2U);
  ASSERT_EQ(p.at(0)->argument_count, kParameters);
  for (size_t i = 0; i < kParameters; ++i) {
    EXPECT_TRUE(type_is_scalar(p.at(0)->parameters[i]->type, expected[i % 4U]))
        << i;
  }
}

TEST(ParserEdgeLimitTest, ManyFunctionsKeepSourceOrder) {
  constexpr size_t kFunctions = 300U;
  std::string source;
  for (size_t i = 0; i < kFunctions; ++i) {
    source += (i == 150U ? "@main " : "") + std::string("def f") +
              std::to_string(i) + "(): Unit =\n  return\n";
  }
  Parsed p(source);
  ASSERT_EQ(p.status, PARSER_OK);
  ASSERT_EQ(p.count(), kFunctions);
  for (size_t i = 0; i < kFunctions; ++i) {
    EXPECT_EQ(p.at(i)->name, "f" + std::to_string(i));
    EXPECT_EQ(p.at(i)->is_main, i == 150U);
  }
}

TEST(ParserEdgeLimitTest, LongBlocksAndDeepIndentationAreAccepted) {
  struct Case {
    std::string source;
    size_t statements, column;
  };
  std::string lines = "def f() =\n", spaced = "def f() =\n";
  for (int i = 0; i < 500; ++i) {
    lines += "  return\n";
    spaced += "    return\n\n        \n";
  }
  const Case cases[] = {
      {lines, 500U, 3U},
      {spaced, 500U, 5U},
      {"def f() =\n" + std::string(1000, ' ') + "return", 1U, 1001U},
  };
  for (const Case &c : cases) {
    Parsed p(kEntry + c.source);
    const ASTNode *body = f_body(p);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->data.block.statement_count, c.statements);
    EXPECT_EQ(body->column, c.column);
  }
}

/*
 * AI-generated test block — malformed prolog.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Spec 2: the prolog `import ifj26.*` must appear exactly once, first, in
 *   exactly this form and on its own logical line; any other form is syntax
 *   error 2. Every source is followed by a valid @main function, so the
 *   verdict depends on the prolog alone once the prolog is implemented.
 */
class ParserEdgePrologTest : public ::testing::TestWithParam<const char *> {};

TEST_P(ParserEdgePrologTest, MalformedPrologIsSyntaxError) {
  const std::string source =
      std::string(GetParam()) + "\n@main def m(): Unit =\n  return\n";
  EXPECT_EQ(parse(source), PARSER_SYNTAX_ERROR);
}

INSTANTIATE_TEST_SUITE_P(
    Malformed, ParserEdgePrologTest,
    ::testing::Values("import", "import ifj26", "import ifj26.",
                      "import ifj26*", "import ifj26.x", "import ifj27.*",
                      "import IFJ26.*", "import .*", "ifj26.*",
                      "import ifj26.*.*", "import ifj26.* import ifj26.*",
                      "import ifj26.*\nimport ifj26.*",
                      "import ifj26.* @main def g(): Unit =\n  return",
                      "def g(): Unit =\n  return\nimport ifj26.*"));

/*
 * AI-generated test block — parser output in the semantic pass.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Spec 2: the entry function is the one annotated with @main, whatever
 *   its name; functions without @main (also one named `main`) leave the
 *   program without an entry function, error 3.
 */
TEST(ParserEdgePipelineTest, EntryFunctionIsChosenByAnnotationOnly) {
  // The parser itself reports the missing entry function (code 3).
  const char *const without_entry[] = {
      "def main(): Unit =\n  return\n",
      "def f(): Unit =\n  return\ndef g(): Unit =\n  return\n",
  };
  for (const char *source : without_entry) {
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(source), PARSER_UNDEFINED_ERROR);
  }
  Parsed p("def main(): Unit =\n  return\n@main\ndef runFoo(): Unit =\n"
           "  return\n");
  ASSERT_EQ(p.status, PARSER_OK);
  EXPECT_EQ(semantic_check(&p.program), SEMANTIC_OK);
}

/*
 * AI-generated test block — known gaps (expected behaviour, disabled).
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Record the specified result of structures the parser cannot handle
 *   yet: anything with an expression (stub p_expression, no while
 *   production) and the prolog. Rejection tests carry a valid control, so
 *   they do not pass for the wrong reason; enable each once src/ is fixed.
 */
namespace {

/** Returns the only statement of @p block, or null. */
const ASTNode *single_statement(const ASTNode *block) {
  if (block == nullptr || block->kind != AST_BLOCK ||
      block->data.block.statement_count != 1U) {
    return nullptr;
  }
  return block->data.block.statements[0];
}

constexpr const char *kIfElse = "def f(a: Int): Unit =\n"
                                "  if a < 1 then\n"
                                "    return\n"
                                "  else\n"
                                "    return\n";

} // namespace

// Same cause as ParserGapKnownGapTest.DISABLED_MissingFunctionBodyIsSyntaxError
// (parser.c:1163): the one-line body is never parsed, so a second definition
// on the header line is taken as the next top-level construct (today OK).
TEST(ParserEdgeKnownGapTest, DISABLED_DefinitionAsOneLineBodyIsSyntaxError) {
  const char *const sources[] = {
      "def f() = def g() =\n  return\n",
      "def f(): Unit = @main def g(): Unit =\n  return\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(source), PARSER_SYNTAX_ERROR);
  }
}

// Needs p_expression (the `if` condition is never parsed). A DEDENT of two
// levels closes the innermost branch and one of three levels the function.
TEST(ParserEdgeKnownGapTest, DISABLED_NestedIfClosesSeveralLevelsAtOnce) {
  Parsed p(with_entry("def g(a: Int): Unit =\n"
                      "  if a < 1 then\n    return\n"
                      "  else\n"
                      "    if a < 2 then\n"
                      "      if a < 3 then\n        return\n"
                      "      else\n        return\n"
                      "    else\n      return\n"
                      "def h(): Unit =\n  return\n"));
  ASSERT_EQ(p.status, PARSER_OK);
  ASSERT_EQ(p.count(), 3U);
  const ASTNode *outer = single_statement(p.at(0)->ast_root);
  ASSERT_NE(outer, nullptr);
  ASSERT_EQ(outer->kind, AST_IF);
  const ASTNode *middle =
      single_statement(outer->data.if_statement.else_branch);
  ASSERT_NE(middle, nullptr);
  ASSERT_EQ(middle->kind, AST_IF);
  const ASTNode *inner =
      single_statement(middle->data.if_statement.then_branch);
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(inner->kind, AST_IF);
  EXPECT_NE(single_statement(p.at(1)->ast_root), nullptr);
}

// Needs p_expression. Spec 2.4: `else` lies on its own logical line with the
// indentation of `if`; spec 1.2: blocks cannot be empty. `else return` on one
// line is left out: CONTROL (spec 5.3) allows one-line bodies of all branches.
TEST(ParserEdgeKnownGapTest, DISABLED_MisplacedElseIsSyntaxError) {
  ASSERT_EQ(parse(with_entry(kIfElse)), PARSER_OK);
  const char *const sources[] = {
      "def f(a: Int): Unit =\n  if a < 1 then\n    return\n"
      "    else\n    return\n",
      "def f(a: Int): Unit =\n  if a < 1 then\n    return\n"
      " else\n    return\n",
      "def f(a: Int): Unit =\n  if a < 1 then\n    return else\n    return\n",
      "def f(a: Int): Unit =\n  if a < 1 then\n  else\n    return\n",
      "def f(a: Int): Unit =\n  if a < 1 then\n    return\n  else\n  return\n",
      "def f(a: Int): Unit =\n  if a < 1 then\n    return\n  else\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(with_entry(source)), PARSER_SYNTAX_ERROR);
  }
}

// Needs p_expression. parser.c:642 keeps the `if` position for the node.
TEST(ParserEdgeKnownGapTest, DISABLED_IfNodeHasPositionOfIfKeyword) {
  Parsed p(with_entry(kIfElse));
  const ASTNode *node = single_statement(f_body(p));
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->kind, AST_IF);
  EXPECT_EQ(node->line, 2U);
  EXPECT_EQ(node->column, 3U);
}

// Needs a while production (p_statement, parser.c:758) and p_expression.
TEST(ParserEdgeKnownGapTest, DISABLED_MalformedWhileIsSyntaxError) {
  ASSERT_EQ(parse(with_entry("def f(a: Int): Unit =\n"
                             "  while a < 1 do\n    return\n  return\n")),
            PARSER_OK);
  const char *const sources[] = {
      "def f(a: Int): Unit =\n  while a < 1\n    return\n",
      "def f(a: Int): Unit =\n  while a < 1 do return\n",
      "def f(a: Int): Unit =\n  while a < 1 do\n  return\n",
      "def f(a: Int): Unit =\n  while do\n    return\n",
      "def f(a: Int): Unit =\n  while a < 1 do\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(with_entry(source)), PARSER_SYNTAX_ERROR);
  }
}

// Needs p_expression; today p_statement returns OK without a node and
// p_code_block dereferences it (parser.c:872, as in
// ParserGapKnownGapTest.DISABLED_ExpressionStatementIsAccepted), so each
// source runs in a child process.
TEST(ParserEdgeKnownGapTest, DISABLED_StatementsStartingWithTermsAreAccepted) {
  const char *const sources[] = {
      "def f(): Unit =\n  var x = 1\n  x = 2\n",             // assignment
      "def f(): Unit =\n  g()\ndef g(): Unit =\n  return\n", // call first
      "def f(): Unit =\n  ()\n", // Unit literal body
      "def f() =\n  -1\n",
      "def f() =\n  1.5\n",
      "def f() =\n  \"s\"\n",
      "def f(a: Int) =\n  (a)\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EXIT(std::exit(parse(with_entry(source)) == PARSER_OK ? 0 : 1),
                ::testing::ExitedWithCode(0), "");
  }
}

// Needs p_expression (and `;` after a statement, see
// ParserGapKnownGapTest.DISABLED_SemicolonAfterStatementIsAccepted): the
// statement before `$` is complete, so the first error is lexical (code 1).
TEST(ParserEdgeKnownGapTest,
     DISABLED_LexicalErrorAfterCompleteStatementIsLexerError) {
  const char *const sources[] = {
      "def f() = 1 $",
      "def f(): Unit =\n  return; $\n",
      "def f() =\n  val x = 1 $\n",
  };
  for (const char *source : sources) {
    SCOPED_TRACE(source);
    EXPECT_EQ(parse(source), PARSER_LEXER_ERROR);
  }
}

// Needs p_expression. Spec 2.2/2.4: each branch is its own scope; a name may
// be defined in both and may shadow a parameter, but is gone after the if.
TEST(ParserEdgeKnownGapTest, DISABLED_BranchesHaveSeparateScopes) {
  EXPECT_EQ(parse(with_entry("def f(a: Int): Unit =\n  if a < 1 then\n"
                             "    val x = 1\n  else\n    val x = 2\n")),
            PARSER_OK);
  EXPECT_EQ(parse(with_entry("def f(a: Int): Unit =\n  if a < 1 then\n"
                             "    val a = 1\n  else\n    ()\n")),
            PARSER_OK);
  EXPECT_EQ(parse(with_entry("def f(a: Int): Int =\n  if a < 1 then\n"
                             "    val x = 1\n  else\n    val y = 1\n"
                             "  return x\n")),
            PARSER_UNDEFINED_ERROR); // spec 6: undefined variable -> 3
}

// Prolog not parsed (reported by ParserBugTest.ProgramWithPrologIsAccepted);
// spec 2 allows any whitespace between its tokens and blank lines after it.
TEST(ParserEdgeKnownGapTest, DISABLED_PrologWithWhitespaceIsAccepted) {
  const char *const prologs[] = {
      "import ifj26.*\n",
      "import   ifj26 . *  \n",
      "import ifj26.*\n\n\n",
  };
  for (const char *prolog : prologs) {
    SCOPED_TRACE(prolog);
    Parsed p(std::string(prolog) + kFunction +
             "@main def m(): Unit =\n"
             "  return\n");
    EXPECT_EQ(p.status, PARSER_OK);
    EXPECT_EQ(p.count(), 2U);
  }
}
