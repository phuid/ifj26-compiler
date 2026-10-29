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
  ASSERT_EQ(type_entity_resolve(&function_return, TYPE_INT), TYPE_RESOLVE_OK);

  EXPECT_EQ(function_return.type, TYPE_INT);
  EXPECT_EQ(local_b.type, TYPE_INT);
  EXPECT_EQ(local_a.type, TYPE_INT);

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
  ASSERT_EQ(type_entity_resolve(&dependent, TYPE_STRING), TYPE_RESOLVE_OK);
  EXPECT_EQ(type_entity_resolve(&source, TYPE_INT), TYPE_RESOLVE_CONFLICT);
  EXPECT_EQ(dependent.type, TYPE_STRING);

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
  ASSERT_EQ(type_entity_resolve(&left, TYPE_REAL), TYPE_RESOLVE_OK);

  EXPECT_EQ(left.type, TYPE_REAL);
  EXPECT_EQ(right.type, TYPE_REAL);

  type_entity_destroy(&right);
  type_entity_destroy(&left);
}
