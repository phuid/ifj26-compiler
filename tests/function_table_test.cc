#include "gtest/gtest.h"

extern "C" {
#include "../src/function_table.h"
}

TEST(FunctionTableTest, ForwardReferenceReturnsSameUnresolvedFunction) {
  FunctionTable table;
  ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);

  FunctionSymbol *first = nullptr;
  FunctionSymbol *second = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "bar", &first),
            FUNCTION_TABLE_OK);
  ASSERT_EQ(function_table_get_or_create(&table, "bar", &second),
            FUNCTION_TABLE_OK);

  EXPECT_EQ(first, second);
  EXPECT_TRUE(type_is_unknown(first->return_type));
  EXPECT_STREQ(first->name, "bar");

  function_table_destroy(&table);
}
