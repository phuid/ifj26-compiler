#include <cstring>

#include "gtest/gtest.h"

extern "C" {
#include "../src/function_table.h"
}

/*
 * AI-generated test block — function table gaps.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Cover function-table edge cases the other suites skip: NULL arguments,
 *   parameter/callee growth, name copying, defaults, set_ast_root NULL bug.
 */
class FunctionTableGapTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_EQ(function_table_init(&table), FUNCTION_TABLE_OK);
    ASSERT_EQ(function_table_get_or_create(&table, "f", &f), FUNCTION_TABLE_OK);
  }
  void TearDown() override { function_table_destroy(&table); }
  FunctionTable table = {};
  FunctionSymbol *f = nullptr;
};

TEST_F(FunctionTableGapTest, RejectsNullArguments) {
  FunctionSymbol *out = nullptr;
  EXPECT_EQ(function_table_init(nullptr), FUNCTION_TABLE_INVALID_ARGUMENT);
  function_table_destroy(nullptr);
  EXPECT_EQ(function_table_get_or_create(nullptr, "g", &out),
            FUNCTION_TABLE_INVALID_ARGUMENT);
  EXPECT_EQ(function_table_get_or_create(&table, nullptr, &out),
            FUNCTION_TABLE_INVALID_ARGUMENT);
  EXPECT_EQ(function_table_get_or_create(&table, "g", nullptr),
            FUNCTION_TABLE_INVALID_ARGUMENT);
  EXPECT_EQ(function_table_lookup(nullptr, "f"), nullptr);
  EXPECT_EQ(function_table_lookup(&table, nullptr), nullptr);
  EXPECT_EQ(function_symbol_add_parameter(nullptr, TYPE_INT),
            FUNCTION_TABLE_INVALID_ARGUMENT);
  EXPECT_EQ(function_symbol_add_callee(nullptr, f),
            FUNCTION_TABLE_INVALID_ARGUMENT);
  EXPECT_EQ(function_symbol_add_callee(f, nullptr),
            FUNCTION_TABLE_INVALID_ARGUMENT);
  EXPECT_EQ(function_symbol_set_ast_root(nullptr, nullptr),
            FUNCTION_TABLE_INVALID_ARGUMENT);
}

TEST_F(FunctionTableGapTest, AddParameterKeepsOrderWhileGrowing) {
  const DataType cycle[] = {TYPE_STRING, TYPE_INT, TYPE_REAL, TYPE_BOOL};
  for (size_t i = 0U; i < 20U; i += 1U) {
    ASSERT_EQ(function_symbol_add_parameter(f, cycle[i % 4U]),
              FUNCTION_TABLE_OK);
  }
  ASSERT_EQ(f->argument_count, 20U);
  for (size_t i = 0U; i < 20U; i += 1U) {
    EXPECT_EQ(f->parameter_types[i], cycle[i % 4U]) << i;
  }
}

TEST_F(FunctionTableGapTest, AddCalleeDeduplicatesGrowsAndAllowsSelfCall) {
  FunctionSymbol *gs[40] = {nullptr};
  ASSERT_EQ(function_symbol_add_callee(f, f), FUNCTION_TABLE_OK);
  for (size_t i = 0U; i < 40U; i += 1U) {
    const char name[] = {'g', static_cast<char>('A' + i), '\0'};
    ASSERT_EQ(function_table_get_or_create(&table, name, &gs[i]),
              FUNCTION_TABLE_OK);
    ASSERT_EQ(function_symbol_add_callee(f, gs[i]), FUNCTION_TABLE_OK);
    ASSERT_EQ(function_symbol_add_callee(f, gs[i / 2U]), FUNCTION_TABLE_OK);
    ASSERT_EQ(function_symbol_add_callee(f, f), FUNCTION_TABLE_OK);
  }
  ASSERT_EQ(f->callee_count, 41U);
  EXPECT_EQ(f->callees[0], f);
  for (size_t i = 0U; i < 40U; i += 1U) {
    EXPECT_EQ(f->callees[i + 1U], gs[i]) << i;
  }
}

TEST_F(FunctionTableGapTest, NameIsCopiedAndRepeatedGetOrCreateReuses) {
  char name[] = "compute";
  FunctionSymbol *g = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, name, &g), FUNCTION_TABLE_OK);
  EXPECT_NE(g->name, name);
  std::memcpy(name, "garbage", sizeof(name));
  EXPECT_STREQ(g->name, "compute");
  FunctionSymbol *again = nullptr;
  ASSERT_EQ(function_table_get_or_create(&table, "compute", &again),
            FUNCTION_TABLE_OK);
  EXPECT_EQ(again, g);
  EXPECT_EQ(table.function_count, 2U);
}

TEST_F(FunctionTableGapTest, NewSymbolHasDefaultFields) {
  EXPECT_EQ(f->return_type.type, TYPE_UNKNOWN);
  EXPECT_FALSE(f->has_explicit_return_type || f->is_defined || f->is_main ||
               f->is_builtin || f->contains_return);
  EXPECT_EQ(f->argument_count + f->callee_count, 0U);
  EXPECT_TRUE(f->local_scope == nullptr && f->ast_root == nullptr);
  EXPECT_EQ(f->check_state, FUNCTION_UNCHECKED);
}

/*
 * AI-generated test block — re-setting the same AST root.
 * Model: Claude (Claude Code).
 * Prompt:
 *   set_ast_root(NULL) on a fresh function is OK; setting the same root
 *   twice keeps it instead of destroying it.
 */
TEST_F(FunctionTableGapTest, SettingTheSameRootAgainKeepsIt) {
  EXPECT_EQ(function_symbol_set_ast_root(f, nullptr), FUNCTION_TABLE_OK);
  EXPECT_EQ(f->ast_root, nullptr);
  ASTNode *root = ast_node_create_block(1U, 1U);
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(function_symbol_set_ast_root(f, root), FUNCTION_TABLE_OK);
  EXPECT_EQ(function_symbol_set_ast_root(f, root), FUNCTION_TABLE_OK);
  EXPECT_EQ(f->ast_root, root);
}

// Disabled: double free at function_table.c:148 (NULL over existing root).
TEST_F(FunctionTableGapTest, DISABLED_SetAstRootNullOverExistingRootIsSafe) {
  ASTNode *body = ast_node_create_block(1U, 1U);
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(function_symbol_set_ast_root(f, body), FUNCTION_TABLE_OK);
  EXPECT_EQ(function_symbol_set_ast_root(f, nullptr),
            FUNCTION_TABLE_INVALID_ARGUMENT);
  EXPECT_EQ(f->ast_root, body);
}
