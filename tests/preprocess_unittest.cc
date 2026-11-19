/**
 * @file preprocess_unittest.cc
 * @brief Unit tests for the IFJ26 source preprocessor.
 * @author Simon Hrouda
 */

#include "../src/source_preprocess.h"
#include <gtest/gtest.h>

TEST(PreprocessTest, MissingProlog) {
  char source_buffer[] = "int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 2); // Expecting return value 2 for missing prolog
}

TEST(PreprocessTest, PrologWithWhitespace) {
  char source_buffer[] = "   import ifj26.*   \nint main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0); // Expecting return value 0 for success
  ASSERT_STREQ(source_buffer,
               "   \nint main() { return 0 }"); // Expecting prolog and leading
                                                // whitespace removed
}

TEST(PreprocessTest, PrologWithLeadingWhitespace) {
  char source_buffer[] =
      " \n\t \t \r  import ifj26.*\n int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0); // Expecting return value 0 for success
  ASSERT_STREQ(source_buffer,
               "\n int main() { return 0 }"); // Expecting prolog and leading
                                              // whitespace removed
}

TEST(PreprocessTest, PrologWithLeadingNonWhitespace) {
  char source_buffer[] = "abc import ifj26.* int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 2); // Expecting return value 2 for missing prolog
}

TEST(PreprocessTest, PrologWithLeadingNonWhitespaceAndWhitespace) {
  char source_buffer[] =
      "abc \n\t \v \t \r  import ifj26.* int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 2); // Expecting return value 2 for missing prolog
}

TEST(PreprocessTest, PrologWithOnlyWhitespaceBefore) {
  char source_buffer[] =
      "\n\t \t \r  import\t\n ifj26.* int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result,
            1); // Expecting return value 1 for malformed input due to prolog
                // followed by non-whitespace characters on the same line
}

TEST(PreprocessTest, PrologWithCharsOnLine) {
  char source_buffer[] = "import ifj26.* abc \nint main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 1); // Expecting return value 1 for malformed input
}

TEST(PreprocessTest, BlockCommentBasic) {
  char source_buffer[] =
      "import ifj26.* /* This is a comment */\n int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0); // Expecting return value 0 for success
  ASSERT_STREQ(source_buffer,
               "\n int main() { return 0 }"); // Expecting block comment
                                              // replaced with space
}

TEST(PreprocessTest, BlockCommentBasicWithoutWhiteSpace) {
  char source_buffer[] =
      "import ifj26.*/* This is a comment */\n int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0); // Expecting return value 0 for success
  ASSERT_STREQ(source_buffer,
               "\n int main() { return 0 }"); // Expecting block comment
                                              // replaced with space
}

TEST(PreprocessTest, MultilineBlockComment) {
  char source_buffer[] = "import ifj26.* /* This is a\nmultiline comment */\n"
                         "int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0); // Expecting return value 0 for success
  ASSERT_STREQ(source_buffer,
               "\n\nint main() { return 0 }"); // Expecting block comment
                                               // replaced with newline
}

TEST(PreprocessTest, MultilineBlockCommentPreservesNewlineCount) {
  char source_buffer[] =
      "import ifj26.* /* first line\nsecond line\nthird line */\n"
      "int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0);
  ASSERT_STREQ(source_buffer, "\n\n\nint main() { return 0 }");
}

TEST(PreprocessTest, LineCommentBasic) {
  char source_buffer[] =
      "import ifj26.* // This is a line comment\n int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0); // Expecting return value 0 for success
  ASSERT_STREQ(source_buffer,
               "\n int main() { return 0 }"); // Expecting line comment
                                              // replaced with newline
}

TEST(PreprocessTest, NestedBlockComments) {
  char source_buffer[] =
      "import ifj26.* /* Outer comment /* Inner comment */ End of outer */\n"
      "       int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0); // Expecting return value 0 for success
  ASSERT_STREQ(source_buffer,
               "\n       int main() { return 0 }"); // Expecting nested block
                                                    // comments replaced with
                                                    // newline
}

TEST(PreprocessTest, NestedBlockCommentsWithNewline) {
  char source_buffer[] =
      "import ifj26.* /* Outer comment /* Inner comment */\nEnd of outer */\n"
      "   int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0); // Expecting return value 0 for success
  ASSERT_STREQ(source_buffer,
               "\n\n   int main() { return 0 }"); // Expecting nested block
                                                  // comments replaced with
                                                  // newline
}

TEST(PreprocessTest, MalformedNestedBlockComment) {
  char source_buffer[] = "import ifj26.* /* Outer comment /* Inner comment "
                         "*/\nint main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 1); // Expecting return value 1 for malformed input
}

TEST(PreprocessTest, MultilineCommentFollowedByLineComment) {
  char source_buffer[] =
      "import ifj26.* /* This is a\nmultiline comment */ \t  // Line comment\n"
      "int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 1); // Expecting return value 1 cause line comment cant be
                        // on the same line as multiline comment end
}

TEST(PreprocessTest, MultilineCommentFollowedByLineCommentWithoutWhitespace) {
  char source_buffer[] =
      "import ifj26.* /* This is a\nmultiline comment */// Line comment\n"
      "int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 1); // Expecting return value 1 for malformed input
}

TEST(PreprocessTest, MultilineCommentFollowedByLineCommentWithNL) {
  char source_buffer[] =
      "import ifj26.* /* This is a\nmultiline comment */\n// Line comment\n"
      "int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0); // Expecting return value 0 for success
  ASSERT_STREQ(source_buffer, "\n\n\nint main() { return 0 }");
}

TEST(PreprocessTest, MultilineCommentFollowedByLineCommentWithWhitespace) {
  char source_buffer[] =
      "import ifj26.* /* This is a\nmultiline comment */   // Line comment\n"
      "int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 1); // Expecting return value 1 for malformed input
}

TEST(PreprocessTest, MultilineCommentFollowedByChars) {
  char source_buffer[] =
      "import ifj26.* /* This is a\nmultiline comment */  afd\n"
      "int main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 1); // Expecting return value 1 for malformed input
}

TEST(PreprocessTest, NullSourceBuffer) {
  char *source_buffer = NULL;
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 99); // Expecting return value 99 for null source buffer
}

TEST(PreprocessTest, EmptySourceBuffer) {
  char source_buffer[] = "";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 2); // Expecting return value 2 for missing prolog
}

TEST(PreprocessTest, OnlyWhitespaceSourceBuffer) {
  char source_buffer[] = "   \n\t \v \t \r  ";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 2); // Expecting return value 2 for missing prolog
}

TEST(PreprocessTest, EmptySourceBufferWithProlog) {
  char source_buffer[] = "import ifj26.*";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0);            // Expecting return value 0 for success
  ASSERT_STREQ(source_buffer, ""); // Expecting prolog removed
}

TEST(PreprocessTest, EmptySourceBufferWithComment) {
  char source_buffer[] = "/* This is a comment */";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 2); // Expecting return value 2 for missing prolog
}

TEST(PreprocessTest, CommentBeforeProlog) {
  char source_buffer[] =
      "/* Comment before prolog */ import ifj26.*\nint main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0);
}

TEST(PreprocessTest, MultilineCommentFollowedBySpacesThenNewline) {
  char source_buffer[] =
      "import ifj26.* /* Multiline\ncomment */  \t  \nint main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0);
}

TEST(PreprocessTest, SinglelineBlockCommentFollowedByLineComment) {
  char source_buffer[] = "import ifj26.* /* Singleline */ // This is fine\nint "
                         "main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0);
}

TEST(PreprocessTest, PrologCaseSensitivity) {
  char source_buffer[] = "Import ifj26.*\nint main() { return 0 }";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 2);
}

TEST(PreprocessTest, UnclosedBlockComment) {
  char source_buffer[] = "import ifj26.*\n/* Unclosed comment";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 1);
}

TEST(PreprocessTest, EndOfFileComment) {
  char source_buffer[] = "import ifj26.*\n /* This is a comment */";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0);                // Expecting return value 0 for success
  ASSERT_STREQ(source_buffer, "\n  "); // Expecting comment replaced with space
}

TEST(PreprocessTest, EndOfFileLineComment) {
  char source_buffer[] = "import ifj26.* \n// This is a line comment";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0);              // Expecting return value 0 for success
  ASSERT_STREQ(source_buffer, "\n"); // Expecting line comment replaced with
                                     // newline or EOF
}

TEST(PreprocessTest, LineCommentIsIgnoredAndCountsAsNewline) {
  char source_buffer[] = "import ifj26.*\na // comment\nb";

  int result = source_preprocess(source_buffer);

  ASSERT_EQ(result, 0);
  ASSERT_STREQ(source_buffer, "\na \nb");
}

TEST(PreprocessTest, NestedBlockCommentIsIgnored) {
  char source_buffer[] = "import ifj26.*\na /* x /* y */ z */ b";

  int result = source_preprocess(source_buffer);

  ASSERT_EQ(result, 0);
  ASSERT_STREQ(source_buffer, "\na   b");
}

TEST(PreprocessTest, UnterminatedBlockCommentIsMalformed) {
  char source_buffer[] = "import ifj26.*\na /* x /* y */";

  int result = source_preprocess(source_buffer);

  ASSERT_EQ(result, 1);
}

TEST(PreprocessTest, EndOfFileUnclosedBlockComment) {
  char source_buffer[] = "import ifj26.*\n"
                         "int main() { return 0 } /* abc";

  int result = source_preprocess(source_buffer);

  ASSERT_EQ(result, 1); // Expecting return value 1 for malformed input due to
                        // unclosed block comment
}

TEST(PreprocessTest, EndOfFileUnclosedNestedBlockComment) {
  char source_buffer[] = "import ifj26.*\n"
                         "/* Outer comment /* Inner comment */ abc";

  int result = source_preprocess(source_buffer);

  ASSERT_EQ(result, 1); // Expecting return value 1 for malformed input due to
                        // unclosed nested block comment
}

TEST(PreprocessTest, EndOfFileMultilineCommentWithWhitespace) {
  char source_buffer[] = "import ifj26.* /* This is a\nmultiline comment */   ";

  int result = source_preprocess(source_buffer);

  ASSERT_EQ(result, 0); // Expecting return value 0 for success
}

TEST(PreprocessTest, CommentInsideStringLiteral) {
  char source_buffer[] =
      "import ifj26.*\nval s = \"/* Not a comment */ // Neither is this\"";
  int result = source_preprocess(source_buffer);
  ASSERT_EQ(result, 0);
  ASSERT_STREQ(source_buffer,
               "\nval s = \"/* Not a comment */ // Neither is this\"");
}

TEST(PreprocessTest, CommentInsideMultilineStringLiteral) {
  char source_buffer[] =
      "import ifj26.*\n"
      "val s = \"\"\"/*not a comment*/\n// still not a comment\"\"\"";

  int result = source_preprocess(source_buffer);

  ASSERT_EQ(result, 0); // Expecting return value 0 for success
  ASSERT_STREQ(
      source_buffer,
      "\nval s = \"\"\"/*not a comment*/\n// still not a comment\"\"\"");
}

TEST(PreprocessTest, CommentMarkersInsideStringLiteral) {
  char source_buffer[] =
      "import ifj26.*\nval s = \"like/* th*/is // not a comment\"";

  int result = source_preprocess(source_buffer);

  ASSERT_EQ(result, 0);
  ASSERT_STREQ(source_buffer, "\nval s = \"like/* th*/is // not a comment\"");
}

TEST(PreprocessTest, EscapedQuoteDoesNotEndStringLiteral) {
  char source_buffer[] =
      "import ifj26.*\nval s = \"like \\\" this // is not a comment\"";

  int result = source_preprocess(source_buffer);

  ASSERT_EQ(result, 0);
  ASSERT_STREQ(source_buffer,
               "\nval s = \"like \\\" this // is not a comment\"");
}

TEST(PreprocessTest, UnterminatedStringLiteral) {
  char source_buffer[] = "import ifj26.*\nval s = \"unterminated";

  int result = source_preprocess(source_buffer);

  ASSERT_EQ(result, 1);
}
