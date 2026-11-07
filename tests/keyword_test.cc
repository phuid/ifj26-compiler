#include "gtest/gtest.h"

extern "C" {
#include "../src/keyword.h"
}

TEST(KeywordTest, FindsEveryKeyword) {
  static const struct {
    const char *spelling;
    TokenType type;
    TokenType paren_type;
  } expected[] = {
      {"def", TOKEN_KW_DEF, TOKEN_INVALID},
      {"do", TOKEN_KW_DO, TOKEN_INVALID},
      {"Double", TOKEN_KW_DOUBLE, TOKEN_INVALID},
      {"else", TOKEN_KW_ELSE, TOKEN_INVALID},
      {"if", TOKEN_KW_IF, TOKEN_KW_IF_PAREN},
      {"import", TOKEN_KW_IMPORT, TOKEN_INVALID},
      {"Int", TOKEN_KW_INT, TOKEN_INVALID},
      {"return", TOKEN_KW_RETURN, TOKEN_KW_RETURN_PAREN},
      {"String", TOKEN_KW_STRING, TOKEN_INVALID},
      {"then", TOKEN_KW_THEN, TOKEN_INVALID},
      {"Unit", TOKEN_KW_UNIT, TOKEN_INVALID},
      {"val", TOKEN_KW_VAL, TOKEN_INVALID},
      {"var", TOKEN_KW_VAR, TOKEN_INVALID},
      {"while", TOKEN_KW_WHILE, TOKEN_KW_WHILE_PAREN},
      {"Boolean", TOKEN_KW_BOOLEAN, TOKEN_INVALID},
      {"case", TOKEN_KW_CASE, TOKEN_KW_CASE_PAREN},
      {"class", TOKEN_KW_CLASS, TOKEN_INVALID},
      {"extension", TOKEN_KW_EXTENSION, TOKEN_INVALID},
      {"for", TOKEN_KW_FOR, TOKEN_KW_FOR_PAREN},
      {"List", TOKEN_KW_LIST, TOKEN_KW_LIST_PAREN},
      {"match", TOKEN_KW_MATCH, TOKEN_INVALID},
      {"type", TOKEN_KW_TYPE, TOKEN_INVALID},
  };

  for (const auto &row : expected) {
    const Keyword *keyword = keyword_lookup(row.spelling);
    ASSERT_NE(keyword, nullptr) << row.spelling;
    EXPECT_EQ(keyword->type, row.type) << row.spelling;
    EXPECT_EQ(keyword->paren_type, row.paren_type) << row.spelling;
  }
}

TEST(KeywordTest, IgnoresNonKeywords) {
  EXPECT_EQ(keyword_lookup("result_2"), nullptr);
  EXPECT_EQ(keyword_lookup("int"), nullptr);
  EXPECT_EQ(keyword_lookup("Def"), nullptr);
  EXPECT_EQ(keyword_lookup("define"), nullptr);
  EXPECT_EQ(keyword_lookup("de"), nullptr);
  EXPECT_EQ(keyword_lookup(""), nullptr);
  EXPECT_EQ(keyword_lookup(nullptr), nullptr);
}

TEST(KeywordTest, MapsTypeKeywordsToDataTypes) {
  EXPECT_EQ(keyword_to_type(TOKEN_KW_INT), TYPE_INT);
  EXPECT_EQ(keyword_to_type(TOKEN_KW_DOUBLE), TYPE_REAL);
  EXPECT_EQ(keyword_to_type(TOKEN_KW_STRING), TYPE_STRING);
  EXPECT_EQ(keyword_to_type(TOKEN_KW_UNIT), TYPE_UNIT);
  EXPECT_EQ(keyword_to_type(TOKEN_KW_BOOLEAN), TYPE_BOOL);
  EXPECT_EQ(keyword_to_type(TOKEN_KW_LIST), TYPE_UNKNOWN);
  EXPECT_EQ(keyword_to_type(TOKEN_IDENTIFIER), TYPE_UNKNOWN);
}
