#include "gtest/gtest.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" {
#include "../src/dynamic_array.h"
}

TEST(DynamicArrayTest, ReserveGrowsAndPreservesExistingElements) {
  int *values = nullptr;
  size_t capacity = 0U;

  ASSERT_EQ(dynamic_array_reserve(reinterpret_cast<void **>(&values), &capacity,
                                  3U, sizeof(*values)),
            DYNAMIC_ARRAY_OK);
  ASSERT_GE(capacity, 3U);
  values[0] = 7;
  values[1] = 11;
  values[2] = 13;

  ASSERT_EQ(dynamic_array_reserve(reinterpret_cast<void **>(&values), &capacity,
                                  capacity + 1U, sizeof(*values)),
            DYNAMIC_ARRAY_OK);
  EXPECT_EQ(values[0], 7);
  EXPECT_EQ(values[1], 11);
  EXPECT_EQ(values[2], 13);

  free(values);
}

TEST(DynamicArrayTest, RejectsElementSizeOverflowWithoutChangingStorage) {
  void *values = nullptr;
  size_t capacity = 0U;

  EXPECT_EQ(dynamic_array_reserve(&values, &capacity, SIZE_MAX, 2U),
            DYNAMIC_ARRAY_OUT_OF_MEMORY);
  EXPECT_EQ(values, nullptr);
  EXPECT_EQ(capacity, 0U);
}
