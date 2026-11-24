#include <string>

#include "gtest/gtest.h"

extern "C" {
#include "../src/symtable.h"
}

/*
 * AI-generated test block — symbol table gaps.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover symtable edge cases the other suites skip: invalid arguments,
 *   children growth, duplicates, odd names, deep resolve, every DataType.
 */
class SymbolTableGapTest : public ::testing::Test {
protected:
  void SetUp() override { ASSERT_NE(root, nullptr); }
  void TearDown() override { symbol_table_destroy(root); }
  SymbolTable *root = symbol_table_create_root();
};

TEST_F(SymbolTableGapTest, RejectsNullAndInvalidArguments) {
  SymbolTable broken = {nullptr, nullptr, 0U, 0U, nullptr};
  Symbol placeholder = {};
  Symbol *out = &placeholder;
  EXPECT_EQ(symbol_table_define(&broken, "x", type_scalar(TYPE_INT), true,
                                false, &out),
            SYMTABLE_INVALID_ARGUMENT);
  EXPECT_EQ(out, nullptr);
  EXPECT_EQ(symbol_table_define(nullptr, "x", type_scalar(TYPE_INT), true,
                                false, nullptr),
            SYMTABLE_INVALID_ARGUMENT);
  EXPECT_EQ(symbol_table_define(root, nullptr, type_scalar(TYPE_INT), true,
                                false, nullptr),
            SYMTABLE_INVALID_ARGUMENT);
  EXPECT_EQ(symbol_table_resolve(root, nullptr), nullptr);
  symbol_table_destroy(nullptr);
}

TEST_F(SymbolTableGapTest, ChildrenArrayGrowsForManyChildren) {
  SymbolTable *blocks[50] = {nullptr};
  for (size_t i = 0U; i < 50U; i += 1U) {
    ASSERT_NE(blocks[i] = symbol_table_create_child(root), nullptr) << i;
  }
  ASSERT_EQ(root->child_count, 50U);
  for (size_t i = 0U; i < 50U; i += 1U) {
    EXPECT_EQ(root->children[i], blocks[i]) << i;
    EXPECT_EQ(blocks[i]->parent, root) << i;
  }
}

TEST_F(SymbolTableGapTest, DuplicateIsRejectedWithoutSideEffects) {
  Symbol *first = nullptr;
  ASSERT_EQ(symbol_table_define(root, "x", type_scalar(TYPE_INT), false, true,
                                &first),
            SYMTABLE_OK);
  Symbol *second = first;
  EXPECT_EQ(symbol_table_define(root, "x", type_scalar(TYPE_REAL), true, false,
                                &second),
            SYMTABLE_DUPLICATE);
  EXPECT_EQ(second, nullptr);
  EXPECT_TRUE(type_equal(first->type, type_scalar(TYPE_INT)));
  EXPECT_TRUE(first->constant && !first->initialized);
  SymbolTable *block = symbol_table_create_child(root);
  ASSERT_NE(block, nullptr);
  Symbol *inner = nullptr;
  ASSERT_EQ(symbol_table_define(block, "x", type_scalar(TYPE_BOOL), true, true,
                                &inner),
            SYMTABLE_OK);
  EXPECT_EQ(symbol_table_define(block, "x", type_scalar(TYPE_STRING), true,
                                true, nullptr),
            SYMTABLE_DUPLICATE);
  EXPECT_EQ(symbol_table_resolve(block, "x"), inner);
  EXPECT_EQ(symbol_table_resolve(root, "x"), first);
}

TEST_F(SymbolTableGapTest, NamesComparedExactly) {
  const char *names[] = {"", "a", "A", "item", "item_1"};
  Symbol *symbols[5] = {nullptr};
  for (size_t i = 0U; i < 5U; i += 1U) {
    ASSERT_EQ(symbol_table_define(root, names[i], type_scalar(TYPE_INT), true,
                                  false, &symbols[i]),
              SYMTABLE_OK);
  }
  for (size_t i = 0U; i < 5U; i += 1U) {
    EXPECT_EQ(symbol_table_lookup_local(root, names[i]), symbols[i]) << i;
  }
  EXPECT_EQ(symbol_table_lookup_local(root, "item_"), nullptr);
}

TEST_F(SymbolTableGapTest, ResolveThroughDeepChain) {
  SymbolTable *scopes[21] = {root};
  ASSERT_EQ(symbol_table_define(root, "top", type_scalar(TYPE_INT), true, false,
                                nullptr),
            SYMTABLE_OK);
  for (size_t i = 1U; i <= 20U; i += 1U) {
    ASSERT_NE(scopes[i] = symbol_table_create_child(scopes[i - 1U]), nullptr);
  }
  ASSERT_EQ(symbol_table_define(scopes[20], "deep", type_scalar(TYPE_INT), true,
                                false, nullptr),
            SYMTABLE_OK);
  EXPECT_EQ(symbol_table_resolve(scopes[20], "missing"), nullptr);
  EXPECT_NE(symbol_table_resolve(scopes[20], "top"), nullptr);
  EXPECT_EQ(symbol_table_resolve(root, "deep"), nullptr);
}

TEST_F(SymbolTableGapTest, StoresEveryDataTypeValue) {
  const BaseType types[] = {TYPE_UNKNOWN, TYPE_INT,  TYPE_REAL, TYPE_STRING,
                            TYPE_UNIT,    TYPE_BOOL, TYPE_ANY};
  for (size_t i = 0U; i < sizeof(types) / sizeof(types[0]); i += 1U) {
    const char name[] = {static_cast<char>('a' + i), '\0'};
    ASSERT_EQ(symbol_table_define(root, name, type_scalar(types[i]), true,
                                  false, nullptr),
              SYMTABLE_OK);
    Symbol *found = symbol_table_lookup_local(root, name);
    ASSERT_NE(found, nullptr) << i;
    EXPECT_TRUE(type_equal(found->type, type_scalar(types[i]))) << i;
  }
}

/*
 * AI-generated test block — symbol pointer identity.
 * Model: Claude (Claude Code).
 * Prompt:
 *   A Symbol pointer returned by define stays the binding's identity after
 *   1000 later defines (BVS rebalancing); edits through it stay visible.
 */
TEST_F(SymbolTableGapTest, SymbolPointerStaysValidAcrossLaterDefinitions) {
  constexpr size_t kCount = 1000U;
  Symbol *stored[kCount] = {nullptr};
  for (size_t i = 0U; i < kCount; i += 1U) {
    // 7919 is coprime to 1000, so the stride visits every id exactly once.
    const size_t id = (i * 7919U) % kCount;
    const std::string name = "s" + std::to_string(id);
    ASSERT_EQ(symbol_table_define(root, name.c_str(), type_scalar(TYPE_INT),
                                  true, false, &stored[id]),
              SYMTABLE_OK)
        << name;
    ASSERT_NE(stored[id], nullptr) << name;
    if (i == 0U) {
      stored[0]->type = type_scalar(TYPE_STRING);
    }
  }
  for (size_t id = 0U; id < kCount; id += 1U) {
    const std::string name = "s" + std::to_string(id);
    EXPECT_EQ(symbol_table_lookup_local(root, name.c_str()), stored[id])
        << name;
    EXPECT_STREQ(stored[id]->name, name.c_str());
  }
  EXPECT_TRUE(type_equal(symbol_table_lookup_local(root, "s0")->type,
                         type_scalar(TYPE_STRING)));
  SymbolTable *child = symbol_table_create_child(root);
  ASSERT_NE(child, nullptr);
  EXPECT_EQ(symbol_table_resolve(child, "s0"), stored[0]);
}
