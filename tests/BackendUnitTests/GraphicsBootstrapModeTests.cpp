#include "Backend/GraphicsBackend.hpp"

#include <gtest/gtest.h>

TEST(GraphicsBootstrapMode, SelectsExpectedBootstrapMode)
{
#if SWIFTSHADER_ENABLE_GPU_BACKEND
    EXPECT_EQ(backend::GraphicsBootstrapMode::GpuWithCpuGraphicsFallback,
              backend::defaultGraphicsBootstrapMode());
#else
    EXPECT_EQ(backend::GraphicsBootstrapMode::CpuOnly,
              backend::defaultGraphicsBootstrapMode());
#endif
}
