#include "gtest/gtest.h"

extern "C" {
#include "../src/keyword.h"
}

TEST(KeywordTest, FindsEveryKeyword) {
  static const struct {
    const char *spelling;
    TokenType type;
  } expected[] = {
      {"def", TOKEN_KW_DEF},         {"do", TOKEN_KW_DO},
      {"Double", TOKEN_KW_DOUBLE},   {"else", TOKEN_KW_ELSE},
      {"if", TOKEN_KW_IF},           {"import", TOKEN_KW_IMPORT},
      {"Int", TOKEN_KW_INT},         {"return", TOKEN_KW_RETURN},
      {"String", TOKEN_KW_STRING},   {"then", TOKEN_KW_THEN},
      {"Unit", TOKEN_KW_UNIT},       {"val", TOKEN_KW_VAL},
      {"var", TOKEN_KW_VAR},         {"while", TOKEN_KW_WHILE},
      {"Boolean", TOKEN_KW_BOOLEAN}, {"case", TOKEN_KW_CASE},
      {"class", TOKEN_KW_CLASS},     {"extension", TOKEN_KW_EXTENSION},
      {"for", TOKEN_KW_FOR},         {"List", TOKEN_KW_LIST},
      {"match", TOKEN_KW_MATCH},     {"type", TOKEN_KW_TYPE},
  };

  for (const auto &row : expected) {
    const Keyword *keyword = keyword_lookup(row.spelling);
    ASSERT_NE(keyword, nullptr) << row.spelling;
    EXPECT_EQ(keyword->type, row.type) << row.spelling;
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
