#include "gtest/gtest.h"

extern "C" {
#include "../src/type.h"
}

TEST(TypeNameTest, MapsCanonicalTypeIdentifiers) {
  EXPECT_EQ(type_from_identifier("Int"), TYPE_INT);
  EXPECT_EQ(type_from_identifier("Double"), TYPE_REAL);
  EXPECT_EQ(type_from_identifier("String"), TYPE_STRING);
  EXPECT_EQ(type_from_identifier("Unit"), TYPE_UNIT);
  EXPECT_EQ(type_from_identifier("Boolean"), TYPE_BOOL);
  EXPECT_EQ(type_from_identifier("List"), TYPE_UNKNOWN);
  EXPECT_EQ(type_from_identifier("int"), TYPE_UNKNOWN);
  EXPECT_EQ(type_from_identifier(nullptr), TYPE_UNKNOWN);
}

TEST(IdentifierTest, RejectsEveryReservedWord) {
  static const char *reserved_words[] = {
      "def", "do",     "Double",  "else", "if",    "import",
      "Int", "return", "String",  "then", "Unit",  "val",
      "var", "while",  "Boolean", "case", "class", "extension",
      "for", "List",   "match",   "type",
  };

  for (const char *word : reserved_words) {
    EXPECT_FALSE(identifier_is_valid(word)) << word;
  }
}

TEST(IdentifierTest, AcceptsUnreservedIdentifier) {
  EXPECT_TRUE(identifier_is_valid("result_2"));
  EXPECT_TRUE(identifier_is_valid("int"));
  EXPECT_FALSE(identifier_is_valid(nullptr));
}
