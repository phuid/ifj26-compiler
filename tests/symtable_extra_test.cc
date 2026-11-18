#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gtest/gtest.h"

extern "C" {
#include "../src/symtable.h"
}

TEST(SymbolTableExtraTest, LookupLocalIgnoresParentScope) {
  SymbolTable *function_scope = symbol_table_create_root();
  ASSERT_NE(function_scope, nullptr);
  SymbolTable *loop_body = symbol_table_create_child(function_scope);
  ASSERT_NE(loop_body, nullptr);

  Symbol *count = nullptr;
  ASSERT_EQ(symbol_table_define(function_scope, "count", type_scalar(TYPE_INT),
                                true, true, &count),
            SYMTABLE_OK);
  ASSERT_NE(count, nullptr);

  EXPECT_EQ(symbol_table_lookup_local(loop_body, "count"), nullptr);
  EXPECT_EQ(symbol_table_lookup_local(function_scope, "count"), count);
  EXPECT_EQ(symbol_table_resolve(loop_body, "count"), count);

  symbol_table_destroy(function_scope);
}

TEST(SymbolTableExtraTest, SiblingScopesDoNotSeeEachOther) {
  SymbolTable *function_scope = symbol_table_create_root();
  ASSERT_NE(function_scope, nullptr);
  SymbolTable *then_branch = symbol_table_create_child(function_scope);
  ASSERT_NE(then_branch, nullptr);
  SymbolTable *else_branch = symbol_table_create_child(function_scope);
  ASSERT_NE(else_branch, nullptr);

  ASSERT_EQ(function_scope->child_count, 2U);
  EXPECT_EQ(function_scope->children[0], then_branch);
  EXPECT_EQ(function_scope->children[1], else_branch);
  EXPECT_EQ(then_branch->parent, function_scope);
  EXPECT_EQ(else_branch->parent, function_scope);

  Symbol *message = nullptr;
  ASSERT_EQ(symbol_table_define(then_branch, "message",
                                type_scalar(TYPE_STRING), true, true, &message),
            SYMTABLE_OK);
  ASSERT_NE(message, nullptr);

  EXPECT_EQ(symbol_table_resolve(else_branch, "message"), nullptr);
  EXPECT_EQ(symbol_table_resolve(function_scope, "message"), nullptr);
  EXPECT_EQ(symbol_table_resolve(then_branch, "message"), message);

  Symbol *else_message = nullptr;
  ASSERT_EQ(symbol_table_define(else_branch, "message", type_scalar(TYPE_INT),
                                true, false, &else_message),
            SYMTABLE_OK);
  ASSERT_NE(else_message, nullptr);
  EXPECT_NE(else_message, message);
  EXPECT_EQ(symbol_table_resolve(else_branch, "message"), else_message);
  EXPECT_EQ(symbol_table_resolve(then_branch, "message"), message);

  symbol_table_destroy(function_scope);
}

/*
 * AI-generated test block — deep scope chain with shadowing in the middle.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Implement only the test below. Model a function root that defines
 *   `limit: Int` and ten nested blocks (if inside while inside if ...),
 *   where the fifth block shadows `limit` with its own `val`. Every scope
 *   below the shadowing block must resolve to the shadowing Symbol, every
 *   scope above it to the root one, and the owner field must point to the
 *   scope that defined the resolved Symbol.
 */
TEST(SymbolTableExtraTest, DeepScopeResolvesRootBinding) {
  const size_t depth = 10U;
  const size_t shadow_level = 5U;
  SymbolTable *scopes[11] = {nullptr};
  scopes[0] = symbol_table_create_root();
  ASSERT_NE(scopes[0], nullptr);
  for (size_t index = 1U; index <= depth; index += 1U) {
    scopes[index] = symbol_table_create_child(scopes[index - 1U]);
    ASSERT_NE(scopes[index], nullptr) << index;
  }

  Symbol *root_limit = nullptr;
  Symbol *inner_limit = nullptr;
  ASSERT_EQ(symbol_table_define(scopes[0], "limit", type_scalar(TYPE_INT), true,
                                false, &root_limit),
            SYMTABLE_OK);
  ASSERT_EQ(symbol_table_define(scopes[shadow_level], "limit",
                                type_scalar(TYPE_REAL), true, true,
                                &inner_limit),
            SYMTABLE_OK);
  ASSERT_NE(root_limit, nullptr);
  ASSERT_NE(inner_limit, nullptr);

  for (size_t index = 0U; index <= depth; index += 1U) {
    Symbol *expected = index < shadow_level ? root_limit : inner_limit;
    SymbolTable *owner =
        index < shadow_level ? scopes[0] : scopes[shadow_level];
    Symbol *found = symbol_table_resolve(scopes[index], "limit");
    ASSERT_EQ(found, expected) << index;
    EXPECT_EQ(found->owner, owner) << index;
  }
  EXPECT_TRUE(type_equal(inner_limit->type.type, type_scalar(TYPE_REAL)));
  EXPECT_TRUE(inner_limit->constant);
  EXPECT_TRUE(type_equal(root_limit->type.type, type_scalar(TYPE_INT)));
  EXPECT_FALSE(root_limit->constant);

  symbol_table_destroy(scopes[0]);
}

TEST(SymbolTableExtraTest, ManyBindingsInOneScopeAreAllFound) {
  const size_t count = 200U;
  SymbolTable *function_scope = symbol_table_create_root();
  ASSERT_NE(function_scope, nullptr);

  for (size_t index = 0U; index < count; index += 1U) {
    char name[32];
    size_t number = (index * 37U) % count;
    snprintf(name, sizeof(name), "item_%zu", number);
    ASSERT_EQ(symbol_table_define(function_scope, name, type_scalar(TYPE_INT),
                                  true, number % 2U == 0U, nullptr),
              SYMTABLE_OK)
        << index;
  }

  for (size_t index = 0U; index < count; index += 1U) {
    char name[32];
    snprintf(name, sizeof(name), "item_%zu", index);
    Symbol *found = symbol_table_lookup_local(function_scope, name);
    ASSERT_NE(found, nullptr) << index;
    EXPECT_STREQ(found->name, name) << index;
    EXPECT_EQ(found->constant, index % 2U == 0U) << index;
  }
  EXPECT_EQ(symbol_table_lookup_local(function_scope, "item_200"), nullptr);
  EXPECT_EQ(symbol_table_lookup_local(function_scope, "item"), nullptr);

  symbol_table_destroy(function_scope);
}

TEST(SymbolTableExtraTest, DefineCopiesNameAndRecordsOwner) {
  SymbolTable *function_scope = symbol_table_create_root();
  ASSERT_NE(function_scope, nullptr);
  char *token_text = static_cast<char *>(malloc(sizeof("total")));
  ASSERT_NE(token_text, nullptr);
  memcpy(token_text, "total", sizeof("total"));

  Symbol *total = nullptr;
  ASSERT_EQ(symbol_table_define(function_scope, token_text,
                                type_scalar(TYPE_REAL), false, true, &total),
            SYMTABLE_OK);
  ASSERT_NE(total, nullptr);
  EXPECT_NE(total->name, token_text);
  free(token_text);

  EXPECT_STREQ(total->name, "total");
  EXPECT_EQ(total->owner, function_scope);
  EXPECT_TRUE(type_equal(total->type.type, type_scalar(TYPE_REAL)));
  EXPECT_TRUE(total->constant);
  EXPECT_FALSE(total->initialized);
  EXPECT_EQ(symbol_table_lookup_local(function_scope, "total"), total);

  symbol_table_destroy(function_scope);
}

TEST(SymbolTableExtraTest, RejectsNullArguments) {
  SymbolTable *program_scope = symbol_table_create_root();
  ASSERT_NE(program_scope, nullptr);
  Symbol *flag = nullptr;
  ASSERT_EQ(symbol_table_define(program_scope, "flag", type_scalar(TYPE_BOOL),
                                true, false, &flag),
            SYMTABLE_OK);
  ASSERT_NE(flag, nullptr);

  Symbol *result = flag;
  EXPECT_EQ(symbol_table_define(nullptr, "flag", type_scalar(TYPE_BOOL), true,
                                false, &result),
            SYMTABLE_INVALID_ARGUMENT);
  EXPECT_EQ(result, nullptr);
  result = flag;
  EXPECT_EQ(symbol_table_define(program_scope, nullptr, type_scalar(TYPE_BOOL),
                                true, false, &result),
            SYMTABLE_INVALID_ARGUMENT);
  EXPECT_EQ(result, nullptr);

  EXPECT_EQ(symbol_table_create_child(nullptr), nullptr);
  EXPECT_EQ(symbol_table_resolve(nullptr, "flag"), nullptr);
  EXPECT_EQ(symbol_table_lookup_local(nullptr, "flag"), nullptr);
  EXPECT_EQ(symbol_table_lookup_local(program_scope, nullptr), nullptr);
  EXPECT_EQ(symbol_table_resolve(program_scope, nullptr), nullptr);
  EXPECT_EQ(program_scope->child_count, 0U);
  EXPECT_EQ(symbol_table_lookup_local(program_scope, "flag"), flag);

  symbol_table_destroy(nullptr);
  symbol_table_destroy(program_scope);
}
