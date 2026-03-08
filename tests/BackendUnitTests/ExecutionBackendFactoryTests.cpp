#include "Backend/GraphicsBackend.hpp"

#include <gtest/gtest.h>

TEST(ExecutionBackendFactory, ReportsExpectedBootstrapMode)
{
	backend::resetExecutionBackendCapture();
#if SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND
	EXPECT_EQ(backend::defaultGraphicsBootstrapMode(), backend::GraphicsBootstrapMode::CustomWithCpuGraphicsFallback);
#else
	EXPECT_EQ(backend::defaultGraphicsBootstrapMode(), backend::GraphicsBootstrapMode::CpuOnly);
#endif
	EXPECT_FALSE(backend::lastExecutionBackendCapture().usedCpuFactory);
	EXPECT_FALSE(backend::lastExecutionBackendCapture().usedCustomFactory);
}
