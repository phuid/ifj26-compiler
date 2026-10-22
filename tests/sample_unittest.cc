#include "../src/sample_lib.h"
#include "gtest/gtest.h"

TEST(SampleTest, AssertionTrue) { ASSERT_TRUE(false); }
TEST(SampleTest, aa) { ASSERT_TRUE(false); }
TEST(SampleTest, AsseraaationTrue) { ASSERT_TRUE(true); }
TEST(LibTest, AssertionTrue) { ASSERT_TRUE(next(2) == 3); }
