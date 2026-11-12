#include "../src/sample_lib.h"
#include "gtest/gtest.h"

TEST(SampleTest, AssertionTrue) { ASSERT_TRUE(true); }
TEST(SampleTest, AnotherAssertion) { ASSERT_TRUE(true); }
TEST(LibTest, AssertionTrue) { ASSERT_TRUE(next(2) == 3); }
