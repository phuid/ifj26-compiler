#include "gtest/gtest.h"

extern "C" {
#include "../src/type.h"
}

TEST(DataTypeTest, RepresentsNestedListsAndExtractsElementTypes) {
  DataType scalar = type_scalar(TYPE_INT);
  DataType list = type_list(scalar);
  DataType matrix = type_list(list);
  DataType element = type_unknown();

  EXPECT_EQ(matrix.base, TYPE_INT);
  EXPECT_EQ(matrix.list_depth, 2U);
  EXPECT_TRUE(type_is_list(matrix));
  EXPECT_FALSE(type_is_list(scalar));
  EXPECT_TRUE(type_is_scalar(scalar, TYPE_INT));
  EXPECT_FALSE(type_is_scalar(list, TYPE_INT));

  ASSERT_TRUE(type_element_type(matrix, &element));
  EXPECT_TRUE(type_equal(element, list));
  ASSERT_TRUE(type_element_type(element, &element));
  EXPECT_TRUE(type_equal(element, scalar));
  EXPECT_FALSE(type_element_type(scalar, &element));
  EXPECT_TRUE(type_is_unknown(type_list(type_unknown())));
}
