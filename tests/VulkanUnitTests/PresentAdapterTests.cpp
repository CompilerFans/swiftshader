#include "Backend/PresentAdapter.hpp"

#include <gtest/gtest.h>

TEST(PresentAdapter, CreateFallbackPresentPath)
{
    auto adapter = backend::createFallbackPresentAdapter();
    ASSERT_NE(adapter, nullptr);
    EXPECT_TRUE(adapter->isFallbackAdapter());
}
