#include "gtest/gtest.h"

extern "C" {
#include "../src/type.h"
}

/*
 * AI-generated test block — propagation from an already resolved source.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Implement only the test below. Model the IFJ26 chain
 *   `val x = 1.5; val y = x; val z = y` where x is resolved before any
 *   dependency is recorded. type_entity_depend must propagate the type at
 *   once and must not store any edge in dependents.
 */
TEST(TypeEntityExtraTest, DependOnResolvedSourcePropagatesImmediately) {
  TypeEntity x;
  TypeEntity y;
  TypeEntity z;
  type_entity_init(&x);
  type_entity_init(&y);
  type_entity_init(&z);

  ASSERT_EQ(type_entity_resolve(&x, type_scalar(TYPE_REAL)), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_depend(&x, &y), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_depend(&y, &z), TYPE_RESOLVE_OK);

  EXPECT_TRUE(type_equal(y.type, type_scalar(TYPE_REAL)));
  EXPECT_TRUE(type_equal(z.type, type_scalar(TYPE_REAL)));
  EXPECT_EQ(x.dependent_count, 0U);
  EXPECT_EQ(y.dependent_count, 0U);

  type_entity_destroy(&z);
  type_entity_destroy(&y);
  type_entity_destroy(&x);
}

/*
 * AI-generated test block — one source with many direct dependents.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Implement only the test below. Register ten unresolved dependents on one
 *   unresolved source in a loop so the dependents array has to grow, then
 *   resolve the source and check that every dependent received its type.
 */
TEST(TypeEntityExtraTest, ResolveReachesEveryDirectDependent) {
  const size_t count = 10U;
  TypeEntity source;
  TypeEntity dependents[10];
  type_entity_init(&source);
  for (size_t i = 0U; i < count; i++) {
    type_entity_init(&dependents[i]);
  }

  for (size_t i = 0U; i < count; i++) {
    ASSERT_EQ(type_entity_depend(&source, &dependents[i]), TYPE_RESOLVE_OK);
  }
  ASSERT_EQ(source.dependent_count, count);
  EXPECT_GE(source.dependent_capacity, count);

  ASSERT_EQ(type_entity_resolve(&source, type_scalar(TYPE_STRING)),
            TYPE_RESOLVE_OK);

  for (size_t i = 0U; i < count; i++) {
    EXPECT_TRUE(type_equal(dependents[i].type, type_scalar(TYPE_STRING)))
        << "dependent " << i;
  }

  for (size_t i = 0U; i < count; i++) {
    type_entity_destroy(&dependents[i]);
  }
  type_entity_destroy(&source);
}

/*
 * AI-generated test block — invalid arguments.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Implement only the two tests below. One checks that type_entity_resolve
 *   rejects a null entity and TYPE_UNKNOWN without changing the entity, the
 *   other that type_entity_depend rejects a null source or dependent without
 *   recording an edge. Also call init/destroy with nullptr, which must not
 *   crash.
 */
TEST(TypeEntityExtraTest, ResolveRejectsInvalidArguments) {
  TypeEntity entity;
  type_entity_init(&entity);

  EXPECT_EQ(type_entity_resolve(nullptr, type_scalar(TYPE_INT)),
            TYPE_RESOLVE_INVALID_ARGUMENT);
  EXPECT_EQ(type_entity_resolve(&entity, type_scalar(TYPE_UNKNOWN)),
            TYPE_RESOLVE_INVALID_ARGUMENT);
  EXPECT_TRUE(type_equal(entity.type, type_scalar(TYPE_UNKNOWN)));

  type_entity_init(nullptr);
  type_entity_destroy(nullptr);
  type_entity_destroy(&entity);
}

TEST(TypeEntityExtraTest, DependRejectsNullEndpoints) {
  TypeEntity entity;
  type_entity_init(&entity);

  EXPECT_EQ(type_entity_depend(nullptr, &entity),
            TYPE_RESOLVE_INVALID_ARGUMENT);
  EXPECT_EQ(type_entity_depend(&entity, nullptr),
            TYPE_RESOLVE_INVALID_ARGUMENT);
  EXPECT_EQ(entity.dependent_count, 0U);
  EXPECT_TRUE(type_equal(entity.type, type_scalar(TYPE_UNKNOWN)));

  type_entity_destroy(&entity);
}

/*
 * AI-generated test block — resolving the same type twice.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Implement only the test below. Model a function with two `return`
 *   statements of the same type: the return entity is resolved to Bool twice.
 *   Both calls succeed and a dependent registered before them keeps Bool.
 */
TEST(TypeEntityExtraTest, ResolvingSameTypeTwiceIsAllowed) {
  TypeEntity function_return;
  TypeEntity call_result;
  type_entity_init(&function_return);
  type_entity_init(&call_result);

  ASSERT_EQ(type_entity_depend(&function_return, &call_result),
            TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(&function_return, type_scalar(TYPE_BOOL)),
            TYPE_RESOLVE_OK);
  EXPECT_EQ(type_entity_resolve(&function_return, type_scalar(TYPE_BOOL)),
            TYPE_RESOLVE_OK);

  EXPECT_TRUE(type_equal(function_return.type, type_scalar(TYPE_BOOL)));
  EXPECT_TRUE(type_equal(call_result.type, type_scalar(TYPE_BOOL)));

  type_entity_destroy(&call_result);
  type_entity_destroy(&function_return);
}

/*
 * AI-generated test block — conflict against an already resolved source.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Implement only the test below. Model `val s: String = f()` where f is
 *   already known to return Double: the dependency is recorded after both
 *   sides have a concrete type, so type_entity_depend reports a conflict and
 *   the declared String type stays unchanged.
 */
TEST(TypeEntityExtraTest, DependOnResolvedSourceDetectsConflict) {
  TypeEntity f_return;
  TypeEntity s;
  type_entity_init(&f_return);
  type_entity_init(&s);

  ASSERT_EQ(type_entity_resolve(&f_return, type_scalar(TYPE_REAL)),
            TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(&s, type_scalar(TYPE_STRING)), TYPE_RESOLVE_OK);

  EXPECT_EQ(type_entity_depend(&f_return, &s), TYPE_RESOLVE_CONFLICT);
  EXPECT_TRUE(type_equal(s.type, type_scalar(TYPE_STRING)));
  EXPECT_TRUE(type_equal(f_return.type, type_scalar(TYPE_REAL)));

  type_entity_destroy(&s);
  type_entity_destroy(&f_return);
}
