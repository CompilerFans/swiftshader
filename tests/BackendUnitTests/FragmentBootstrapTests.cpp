#include "Backend/FakeRuntimeAPI.hpp"
#include "Backend/FragmentBootstrap.hpp"
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
#	include "Backend/CudaRuntimeAPI.hpp"
#endif

#include <gtest/gtest.h>

TEST(FragmentBootstrap, EmitsFragmentStageWrapperAndShaderBody)
{
	std::string source = backend::fragmentBootstrapCudaSource();

	EXPECT_NE(source.find("struct FragmentInvocation"), std::string::npos);
	EXPECT_NE(source.find("struct FsParams"), std::string::npos);
	EXPECT_NE(source.find("static __device__ void fs_main"), std::string::npos);
	EXPECT_NE(source.find("extern \"C\" __global__ void fs_entry"), std::string::npos);
	EXPECT_NE(source.find("params.colorBuffer[offset + 0]"), std::string::npos);
}

TEST(FragmentBootstrap, LaunchUsesSingleFsParamsArgument)
{
	backend::FakeRuntimeAPI runtime;
	backend::launchFragmentBootstrap(runtime);

	EXPECT_NE(runtime.lastModuleSource().find("struct FsParams"), std::string::npos);
	EXPECT_EQ(runtime.lastLaunch().argumentCount, 1u);
	EXPECT_EQ(runtime.lastLaunch().groupCountX, 1u);
}

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
TEST(FragmentBootstrap, CudaRuntimeWritesConstantColor)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<backend::FragmentBootstrapInvocation> invocations = {
		{ 1u, 1u, 1u, 0u },
		{ 2u, 1u, 1u, 0u },
	};
	std::vector<uint8_t> colorBuffer;

	ASSERT_TRUE(backend::runFragmentBootstrap(runtime, 4u, 4u, invocations, backend::FragmentBootstrapConfig{}, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 4u * 4u * 4u);

	size_t first = ((1u * 4u) + 1u) * 4u;
	EXPECT_EQ(colorBuffer[first + 0], 255u);
	EXPECT_EQ(colorBuffer[first + 1], 0u);
	EXPECT_EQ(colorBuffer[first + 2], 0u);
	EXPECT_EQ(colorBuffer[first + 3], 255u);
}

TEST(FragmentBootstrap, CudaRuntimeSkipsHelperAndExportMaskedInvocations)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<backend::FragmentBootstrapInvocation> invocations = {
		{ 1u, 1u, 0u, 0u },
		{ 2u, 1u, 1u, 1u },
		{ 3u, 1u, 1u, 0u },
	};
	std::vector<uint8_t> colorBuffer;

	ASSERT_TRUE(backend::runFragmentBootstrap(runtime, 4u, 4u, invocations, backend::FragmentBootstrapConfig{}, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 4u * 4u * 4u);

	size_t skippedExport = ((1u * 4u) + 1u) * 4u;
	EXPECT_EQ(colorBuffer[skippedExport + 0], 0u);
	EXPECT_EQ(colorBuffer[skippedExport + 3], 0u);

	size_t skippedHelper = ((1u * 4u) + 2u) * 4u;
	EXPECT_EQ(colorBuffer[skippedHelper + 0], 0u);
	EXPECT_EQ(colorBuffer[skippedHelper + 3], 0u);

	size_t written = ((1u * 4u) + 3u) * 4u;
	EXPECT_EQ(colorBuffer[written + 0], 255u);
	EXPECT_EQ(colorBuffer[written + 3], 255u);
}
#endif
