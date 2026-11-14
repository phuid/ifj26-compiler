#include "gtest/gtest.h"
#include <cstdint>
#include <cstdlib>

extern "C" {
#include "../src/dynamic_array.h"
}

/*
 * AI-generated test block — dynamic_array_reserve contract.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Tables for invalid args and exact growth, overflow, data preservation.
 */
TEST(DynamicArrayGapTest, InvalidArgumentsAreRejected) {
  void *data = nullptr;
  size_t zero = 0U, eight = 8U;
  struct Case { void **data; size_t *capacity; size_t required, size; };
  const Case cases[] = {{nullptr, &zero, 1, 4}, {&data, nullptr, 1, 4},
                        {&data, &zero, 1, 0}, {&data, &eight, 16, 4}};
  for (const Case &c : cases) {
    EXPECT_EQ(dynamic_array_reserve(c.data, c.capacity, c.required, c.size),
              DYNAMIC_ARRAY_INVALID_ARGUMENT);
  }
  EXPECT_TRUE(data == nullptr && zero == 0U && eight == 8U) << "modified";
}

TEST(DynamicArrayGapTest, CapacityGrowsExactly) {
  const size_t cases[][3] = {{0, 0, 0},     {0, 1, 4},      {0, 4, 4},
                             {4, 3, 4},     {4, 5, 8},      {8, 9, 16},
                             {4, 100, 128}, {0, 1000, 1024}};
  for (const auto &c : cases) {
    void *data = nullptr;
    size_t cap = 0U;
    ASSERT_EQ(dynamic_array_reserve(&data, &cap, c[0], 4U), DYNAMIC_ARRAY_OK);
    ASSERT_EQ(cap, c[0]);
    EXPECT_EQ(dynamic_array_reserve(&data, &cap, c[1], 4U), DYNAMIC_ARRAY_OK);
    EXPECT_EQ(cap, c[2]) << "start " << c[0] << ", required " << c[1];
    free(data);
  }
}

TEST(DynamicArrayGapTest, OverflowKeepsExistingStorage) {
  void *data = nullptr;
  size_t cap = 0U;
  ASSERT_EQ(dynamic_array_reserve(&data, &cap, 4U, 2U), DYNAMIC_ARRAY_OK);
  void *const original = data;
  for (size_t required : {SIZE_MAX / 2U + 2U, SIZE_MAX}) {
    EXPECT_EQ(dynamic_array_reserve(&data, &cap, required, 2U),
              DYNAMIC_ARRAY_OUT_OF_MEMORY);
    EXPECT_EQ(data, original);
    EXPECT_EQ(cap, 4U);
  }
  free(data);
}

TEST(DynamicArrayGapTest, DataSurvivesGrowth) {
  const size_t kCount = 256U;
  void *data = nullptr;
  size_t cap = 0U;
  for (size_t used = 0U; used < kCount; ++used) {
    ASSERT_EQ(dynamic_array_reserve(&data, &cap, used + 1U, sizeof(int)),
              DYNAMIC_ARRAY_OK);
    static_cast<int *>(data)[used] = static_cast<int>(used);
  }
  for (size_t i = 0U; i < kCount; ++i) {
    ASSERT_EQ(static_cast<int *>(data)[i], static_cast<int>(i));
  }
  free(data);
}
