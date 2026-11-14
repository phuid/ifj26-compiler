#include "gtest/gtest.h"

extern "C" {
#include "../src/type.h"
}

TEST(TypeEntityTest, ResolvesTypeThroughTransitiveDependents) {
  TypeEntity function_return;
  TypeEntity local_b;
  TypeEntity local_a;

  type_entity_init(&function_return);
  type_entity_init(&local_b);
  type_entity_init(&local_a);

  ASSERT_EQ(type_entity_depend(&function_return, &local_b), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_depend(&local_b, &local_a), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(&function_return, type_scalar(TYPE_INT)),
            TYPE_RESOLVE_OK);

  EXPECT_TRUE(type_equal(function_return.type, type_scalar(TYPE_INT)));
  EXPECT_TRUE(type_equal(local_b.type, type_scalar(TYPE_INT)));
  EXPECT_TRUE(type_equal(local_a.type, type_scalar(TYPE_INT)));

  type_entity_destroy(&local_a);
  type_entity_destroy(&local_b);
  type_entity_destroy(&function_return);
}

TEST(TypeEntityTest, RejectsConflictingConcreteType) {
  TypeEntity source;
  TypeEntity dependent;

  type_entity_init(&source);
  type_entity_init(&dependent);

  ASSERT_EQ(type_entity_depend(&source, &dependent), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(&dependent, type_scalar(TYPE_STRING)),
            TYPE_RESOLVE_OK);
  EXPECT_EQ(type_entity_resolve(&source, type_scalar(TYPE_INT)),
            TYPE_RESOLVE_CONFLICT);
  EXPECT_TRUE(type_equal(dependent.type, type_scalar(TYPE_STRING)));

  type_entity_destroy(&dependent);
  type_entity_destroy(&source);
}

TEST(TypeEntityTest, HandlesDependencyCycleWithoutInfiniteRecursion) {
  TypeEntity left;
  TypeEntity right;

  type_entity_init(&left);
  type_entity_init(&right);

  ASSERT_EQ(type_entity_depend(&left, &right), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_depend(&right, &left), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(&left, type_scalar(TYPE_REAL)),
            TYPE_RESOLVE_OK);

  EXPECT_TRUE(type_equal(left.type, type_scalar(TYPE_REAL)));
  EXPECT_TRUE(type_equal(right.type, type_scalar(TYPE_REAL)));

  type_entity_destroy(&right);
  type_entity_destroy(&left);
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

TEST(TypeEntityTest, PropagatesNestedListsAndRejectsDifferentDepths) {
  TypeEntity source;
  TypeEntity dependent;
  DataType matrix = type_list(type_list(type_scalar(TYPE_INT)));

  type_entity_init(&source);
  type_entity_init(&dependent);

  ASSERT_EQ(type_entity_depend(&source, &dependent), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(&source, matrix), TYPE_RESOLVE_OK);
  EXPECT_TRUE(type_equal(source.type, matrix));
  EXPECT_TRUE(type_equal(dependent.type, matrix));

  EXPECT_EQ(type_entity_resolve(&dependent, type_list(type_scalar(TYPE_INT))),
            TYPE_RESOLVE_CONFLICT);

  type_entity_destroy(&dependent);
  type_entity_destroy(&source);
}
