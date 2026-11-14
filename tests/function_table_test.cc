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
  EXPECT_TRUE(type_is_unknown(first->return_type.type));
  EXPECT_STREQ(first->name, "bar");

  function_table_destroy(&table);
}

TEST(FunctionTableTest, ResolvedReturnTypePropagatesToLocalSymbol) {
  FunctionTable table;
  ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);

  FunctionSymbol *function = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "fc", &function),
            FUNCTION_TABLE_OK);
  ASSERT_NE(function_symbol_create_local_scope(function), nullptr);

  Symbol *local = nullptr;
  ASSERT_EQ(symbol_table_define(function->local_scope, "b", type_unknown(),
                                true, false, &local),
            SYMTABLE_OK);
  ASSERT_EQ(type_entity_depend(&function->return_type, &local->type),
            TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(&function->return_type, type_scalar(TYPE_INT)),
            TYPE_RESOLVE_OK);

  EXPECT_TRUE(type_equal(local->type.type, type_scalar(TYPE_INT)));
  function_table_destroy(&table);
}
