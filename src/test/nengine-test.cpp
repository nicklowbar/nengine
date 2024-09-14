#include "src/core/include/nengine.h"

#include <gtest/gtest.h>
#include <iostream>
#include <memory>

TEST(nengine_test, nengine_default_initialization)
{
    auto nengine_instance = std::make_unique<nengine>();
    ASSERT_TRUE(true);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
