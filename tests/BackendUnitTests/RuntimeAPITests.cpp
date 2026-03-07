#include "Backend/FakeRuntimeAPI.hpp"

#include <gtest/gtest.h>

TEST(RuntimeAPI, FakeRuntimeCreatesModuleHandle)
{
    backend::FakeRuntimeAPI api;
    auto module = api.createModule("kernel text");
    EXPECT_TRUE(module.valid());
}
