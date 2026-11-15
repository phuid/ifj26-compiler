#include "gtest/gtest.h"

#include <cstring>
#include <initializer_list>
#include <utility>

extern "C" {
#include "../src/type.h"
}

/** Owns N initialised entities, links edges by index, destroys at exit. */
template <size_t N> struct Entities {
  TypeEntity e[N];
  Entities() {
    for (TypeEntity &entity : e)
      type_entity_init(&entity);
  }
  ~Entities() {
    for (TypeEntity &entity : e)
      type_entity_destroy(&entity);
  }
  TypeEntity *operator[](size_t index) { return &e[index]; }
  void Link(std::initializer_list<std::pair<size_t, size_t>> edges) {
    for (const auto &[from, to] : edges) {
      ASSERT_EQ(type_entity_depend(&e[from], &e[to]), TYPE_RESOLVE_OK);
    }
  }
};

/*
 * AI-generated test block — type tables.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Every concrete DataType propagates two levels; listed pairs conflict.
 */
TEST(TypeEntityGapTest, EveryConcreteTypePropagates) {
  // TYPE_ANY is accepted and propagated like a concrete type (design choice).
  for (DataType type :
       {TYPE_INT, TYPE_REAL, TYPE_STRING, TYPE_UNIT, TYPE_BOOL, TYPE_ANY}) {
    Entities<3> g;
    g.Link({{0, 1}, {1, 2}});
    EXPECT_EQ(type_entity_resolve(g[0], type), TYPE_RESOLVE_OK) << type;
    EXPECT_EQ(g[2]->type, type);
  }
}

TEST(TypeEntityGapTest, ConflictPairs) {
  // TYPE_ANY does not unify with Int (design choice).
  const std::pair<DataType, DataType> pairs[] = {
      {TYPE_INT, TYPE_REAL},    {TYPE_REAL, TYPE_INT}, {TYPE_UNIT, TYPE_INT},
      {TYPE_BOOL, TYPE_STRING}, {TYPE_ANY, TYPE_INT},  {TYPE_INT, TYPE_ANY}};
  for (const auto &[fixed, other] : pairs) {
    Entities<2> g;
    g.Link({{0, 1}});
    ASSERT_EQ(type_entity_resolve(g[1], fixed), TYPE_RESOLVE_OK);
    EXPECT_EQ(type_entity_resolve(g[0], other), TYPE_RESOLVE_CONFLICT)
        << fixed << " vs " << other;
    EXPECT_EQ(g[1]->type, fixed);
  }
}

/*
 * AI-generated test block — dependency graph shapes and lifecycle.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Transitive conflict, self edge, cycle, diamond, dup edge, 200 deps, reuse.
 */
TEST(TypeEntityGapTest, TransitiveConflictResetsResolving) {
  Entities<3> g;
  ASSERT_EQ(type_entity_resolve(g[2], TYPE_STRING), TYPE_RESOLVE_OK);
  g.Link({{0, 1}, {1, 2}});
  EXPECT_EQ(type_entity_resolve(g[0], TYPE_INT), TYPE_RESOLVE_CONFLICT);
  // No rollback on conflict: a and b keep Int (design choice).
  EXPECT_EQ(g[1]->type, TYPE_INT);
  for (size_t i = 0U; i < 3U; i++) {
    EXPECT_FALSE(g[i]->resolving) << i;
  }
  EXPECT_EQ(type_entity_resolve(g[0], TYPE_INT), TYPE_RESOLVE_CONFLICT);
}

TEST(TypeEntityGapTest, SelfDependenceTerminates) {
  Entities<1> g;
  g.Link({{0, 0}});
  EXPECT_EQ(g[0]->dependent_count, 1U);
  EXPECT_EQ(type_entity_resolve(g[0], TYPE_INT), TYPE_RESOLVE_OK);
}

TEST(TypeEntityGapTest, ThreeCycleResolvesEveryNode) {
  Entities<3> g;
  g.Link({{0, 1}, {1, 2}, {2, 0}});
  ASSERT_EQ(type_entity_resolve(g[1], TYPE_BOOL), TYPE_RESOLVE_OK);
  for (size_t i = 0U; i < 3U; i++) {
    EXPECT_EQ(g[i]->type, TYPE_BOOL) << i;
  }
}

TEST(TypeEntityGapTest, DiamondReachesSharedNodeTwice) {
  Entities<4> g;
  g.Link({{0, 1}, {0, 2}, {1, 3}, {2, 3}});
  EXPECT_EQ(type_entity_resolve(g[0], TYPE_REAL), TYPE_RESOLVE_OK);
  EXPECT_EQ(g[3]->type, TYPE_REAL);
}

TEST(TypeEntityGapTest, DuplicateEdgeIsStoredTwice) {
  Entities<2> g;
  g.Link({{0, 1}, {0, 1}});
  EXPECT_EQ(g[0]->dependent_count, 2U);
  EXPECT_EQ(type_entity_resolve(g[0], TYPE_STRING), TYPE_RESOLVE_OK);
  EXPECT_EQ(g[1]->type, TYPE_STRING);
}

TEST(TypeEntityGapTest, ManyDependentsSurviveReallocation) {
  constexpr size_t kCount = 200U;
  Entities<kCount + 1U> g;
  for (size_t i = 1U; i <= kCount; i++) {
    g.Link({{0, i}});
  }
  ASSERT_EQ(g[0]->dependent_count, kCount);
  ASSERT_EQ(type_entity_resolve(g[0], TYPE_REAL), TYPE_RESOLVE_OK);
  for (size_t i = 1U; i <= kCount; i++) {
    EXPECT_EQ(g[0]->dependents[i - 1U], g[i]) << i;
    EXPECT_EQ(g[i]->type, TYPE_REAL) << i;
  }
}

TEST(TypeEntityGapTest, InitDestroyStateAndReuse) {
  TypeEntity entity;
  std::memset(&entity, 0xAB, sizeof(entity));
  type_entity_init(&entity);
  EXPECT_EQ(entity.type, TYPE_UNKNOWN);
  EXPECT_EQ(entity.dependents, nullptr);
  EXPECT_EQ(entity.dependent_count, 0U);
  EXPECT_FALSE(entity.resolving);
  Entities<1> old_dependent;
  ASSERT_EQ(type_entity_depend(&entity, old_dependent[0]), TYPE_RESOLVE_OK);
  ASSERT_EQ(type_entity_resolve(&entity, TYPE_INT), TYPE_RESOLVE_OK);
  type_entity_destroy(&entity);
  EXPECT_EQ(entity.type, TYPE_UNKNOWN);
  EXPECT_EQ(entity.dependents, nullptr);
  type_entity_destroy(&entity);
  type_entity_init(&entity);
  EXPECT_EQ(type_entity_resolve(&entity, TYPE_STRING), TYPE_RESOLVE_OK);
  EXPECT_EQ(old_dependent[0]->type, TYPE_INT);
  type_entity_destroy(&entity);
}
