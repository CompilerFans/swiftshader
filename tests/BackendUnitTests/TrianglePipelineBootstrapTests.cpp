#include "Backend/FakeRuntimeAPI.hpp"
#include "Backend/TrianglePipelineBootstrap.hpp"
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
#	include "Backend/CudaRuntimeAPI.hpp"
#endif

#include <gtest/gtest.h>

TEST(TrianglePipelineBootstrap, FakeRuntimeLaunchesThreeStages)
{
	backend::FakeRuntimeAPI runtime;
	backend::FakeRuntimeAPI::resetGlobalCapture();

	backend::launchTrianglePipelineBootstrap(runtime);

	EXPECT_EQ(backend::FakeRuntimeAPI::globalLaunchCount(), 3u);
	EXPECT_NE(backend::FakeRuntimeAPI::globalLastModuleSource().find("struct FsParams"), std::string::npos);
}

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
TEST(TrianglePipelineBootstrap, CudaRuntimeProducesGreenTriangleColorBuffer)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<uint8_t> colorBuffer;
	backend::CudaRuntimeAPI::resetGlobalCapture();
	ASSERT_TRUE(backend::runTrianglePipelineBootstrap(runtime, 64u, 64u, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 64u * 64u * 4u);
	EXPECT_EQ(backend::CudaRuntimeAPI::globalLaunchCount(), 3u);

	size_t inside = ((32u * 64u) + 32u) * 4u;
	EXPECT_EQ(colorBuffer[inside + 0], 0u);
	EXPECT_EQ(colorBuffer[inside + 1], 255u);
	EXPECT_EQ(colorBuffer[inside + 2], 0u);
	EXPECT_EQ(colorBuffer[inside + 3], 255u);

	size_t outside = ((2u * 64u) + 2u) * 4u;
	EXPECT_EQ(colorBuffer[outside + 0], 0u);
	EXPECT_EQ(colorBuffer[outside + 1], 0u);
	EXPECT_EQ(colorBuffer[outside + 2], 0u);
	EXPECT_EQ(colorBuffer[outside + 3], 0u);
}

TEST(TrianglePipelineBootstrap, CudaRuntimeAppliesRequestedFragmentColor)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	backend::TrianglePipelineBootstrapConfig config = {};
	config.width = 64u;
	config.height = 64u;
	config.colorR = 1.0f;
	config.colorG = 0.0f;
	config.colorB = 0.0f;
	config.colorA = 1.0f;

	std::vector<uint8_t> colorBuffer;
	ASSERT_TRUE(backend::runTrianglePipelineBootstrap(runtime, config, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 64u * 64u * 4u);

	size_t inside = ((32u * 64u) + 32u) * 4u;
	EXPECT_EQ(colorBuffer[inside + 0], 255u);
	EXPECT_EQ(colorBuffer[inside + 1], 0u);
	EXPECT_EQ(colorBuffer[inside + 2], 0u);
	EXPECT_EQ(colorBuffer[inside + 3], 255u);
}
#endif
