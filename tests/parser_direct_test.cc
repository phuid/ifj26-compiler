#include <cstdio>

#include "gtest/gtest.h"

extern "C" {
#include "../src/parser.h"
}

namespace {

FILE *stream_from(const char *text) {
  FILE *stream = tmpfile();
  if (stream == nullptr) {
    return nullptr;
  }
  fputs(text, stream);
  rewind(stream);
  return stream;
}

} // namespace

TEST(ParserDirectLexerTest, EmptyInputFinishesWithoutSyntaxRules) {
  FILE *stream = stream_from("");
  ASSERT_NE(stream, nullptr);

  Lexer lexer;
  ASSERT_EQ(lexer_init(&lexer, stream), LEXER_OK);
  Program program;
  ASSERT_EQ(program_init(&program), PARSER_OK);
  EXPECT_EQ(p_program(&lexer, &program), PARSER_OK);
  EXPECT_EQ(program.functions.function_count, 0U);

  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
}
